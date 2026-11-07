#include "core/night_vision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vl {

namespace {
inline int lumaOf(const uint8_t* p) { return (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8; }
}  // namespace

void NightVision::temporalDenoise(Image& rgb, float strength, float mean) {
    const size_t n = rgb.data.size();
    if (prev_.size() != n) {
        prev_ = rgb.data;
        return;
    }
    // Darker scene -> more sensor noise -> wider "static" threshold.
    const int thr = static_cast<int>(6 + 22 * strength * std::clamp(1.f - mean * 3.f, 0.f, 1.f));
    // Weight of the new frame for static pixels (Q8). Lower = stronger smoothing.
    const int wStatic = static_cast<int>(256 * (0.55f - 0.30f * strength));
    uint8_t* cur = rgb.data.data();
    uint8_t* prv = prev_.data();
    for (size_t i = 0; i + 2 < n; i += 3) {
        int d = std::abs(lumaOf(cur + i) - lumaOf(prv + i));
        // Smooth ramp between "static" and "moving" so edges don't flicker.
        int w = d <= thr ? wStatic : (d >= 2 * thr ? 256 : wStatic + (256 - wStatic) * (d - thr) / thr);
        for (int c = 0; c < 3; ++c) {
            int v = prv[i + c] + (((cur[i + c] - prv[i + c]) * w) >> 8);
            cur[i + c] = prv[i + c] = static_cast<uint8_t>(v);
        }
    }
}

void NightVision::buildClahe(const std::vector<uint8_t>& luma, int w, int h, float clip) {
    for (int ty = 0; ty < kTilesY; ++ty) {
        for (int tx = 0; tx < kTilesX; ++tx) {
            const int x0 = tx * w / kTilesX, x1 = (tx + 1) * w / kTilesX;
            const int y0 = ty * h / kTilesY, y1 = (ty + 1) * h / kTilesY;
            std::array<int, 256> hist{};
            int total = 0;
            // Subsample 2x for speed; histogram shape is what matters.
            for (int y = y0; y < y1; y += 2)
                for (int x = x0; x < x1; x += 2) { ++hist[luma[static_cast<size_t>(y) * w + x]]; ++total; }
            auto& lut = tileLut_[ty * kTilesX + tx];
            if (total == 0) {
                for (int i = 0; i < 256; ++i) lut[i] = static_cast<uint8_t>(i);
                continue;
            }
            const float limit = std::max(1.f, clip * total / 256.f);
            float excess = 0.f;
            std::array<float, 256> hf;
            for (int i = 0; i < 256; ++i) {
                hf[i] = static_cast<float>(hist[i]);
                if (hf[i] > limit) { excess += hf[i] - limit; hf[i] = limit; }
            }
            // Redistribute clipped mass uniformly (float: small tiles must not lose it).
            const float add = excess / 256.f;
            float cdf = 0.f;
            for (int i = 0; i < 256; ++i) {
                cdf += hf[i] + add;
                lut[i] = static_cast<uint8_t>(std::min(255.f, cdf * 255.f / total + 0.5f));
            }
        }
    }
}

void NightVision::process(Image& rgb, const NightParams& p, NightStats* stats) {
    if (rgb.empty() || rgb.channels != 3) return;
    const float strength = std::clamp(p.strength, 0.f, 1.f);
    const float meanIn = std::max(meanLuma(rgb, 6), 1.f / 255.f);

    if (p.denoise) temporalDenoise(rgb, strength, meanIn);

    // Global adaptive gamma: bring mean luminance toward the target.
    float g = std::log(std::clamp(p.targetMean, 0.1f, 0.8f)) / std::log(meanIn);
    g = std::clamp(g, 0.30f, 1.0f);
    g = 1.f - strength * (1.f - g);
    std::array<uint8_t, 256> gammaLut;
    for (int i = 0; i < 256; ++i)
        gammaLut[i] = static_cast<uint8_t>(std::lround(255.0 * std::pow(i / 255.0, static_cast<double>(g))));

    const int w = rgb.width, h = rgb.height;
    luma_.resize(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        const uint8_t* r = rgb.row(y);
        uint8_t* l = luma_.data() + static_cast<size_t>(y) * w;
        for (int x = 0; x < w; ++x) l[x] = gammaLut[lumaOf(r + 3 * x)];
    }

    const bool clahe = p.localContrast && strength > 0.05f;
    if (clahe) buildClahe(luma_, w, h, 1.f + (p.clipLimit - 1.f) * strength);
    const int claheMix = static_cast<int>(256 * std::min(1.f, 0.4f + 0.6f * strength));

    for (int y = 0; y < h; ++y) {
        uint8_t* r = rgb.row(y);
        const uint8_t* l = luma_.data() + static_cast<size_t>(y) * w;
        // Bilinear blend between the 4 nearest tile LUTs (standard CLAHE interpolation).
        float fy = (y + 0.5f) * kTilesY / h - 0.5f;
        int ty0 = std::clamp(static_cast<int>(std::floor(fy)), 0, kTilesY - 1);
        int ty1 = std::min(ty0 + 1, kTilesY - 1);
        float wy = std::clamp(fy - ty0, 0.f, 1.f);
        for (int x = 0; x < w; ++x) {
            const int y0 = lumaOf(r + 3 * x);
            int yn = l[x];
            if (clahe) {
                float fx = (x + 0.5f) * kTilesX / w - 0.5f;
                int tx0 = std::clamp(static_cast<int>(std::floor(fx)), 0, kTilesX - 1);
                int tx1 = std::min(tx0 + 1, kTilesX - 1);
                float wx = std::clamp(fx - tx0, 0.f, 1.f);
                float v = (1 - wy) * ((1 - wx) * tileLut_[ty0 * kTilesX + tx0][yn] + wx * tileLut_[ty0 * kTilesX + tx1][yn]) +
                          wy * ((1 - wx) * tileLut_[ty1 * kTilesX + tx0][yn] + wx * tileLut_[ty1 * kTilesX + tx1][yn]);
                yn = yn + ((static_cast<int>(v) - yn) * claheMix >> 8);
            }
            // Scale RGB by luminance gain: keeps hue, limits noise amplification.
            const int gain = std::min(8 * 256, (yn * 256 + 128) / std::max(y0, 4));
            uint8_t* px = r + 3 * x;
            for (int c = 0; c < 3; ++c) px[c] = static_cast<uint8_t>(std::min(255, (px[c] * gain) >> 8));
        }
    }

    if (stats) {
        stats->meanIn = meanIn;
        stats->meanOut = meanLuma(rgb, 6);
        stats->gamma = g;
    }
}

bool IrFrameSelector::accept(Image& frame, bool subtractAmbient) {
    const float m = meanLuma(frame, 8);
    history_[count_ % history_.size()] = m;
    ++count_;
    if (count_ >= static_cast<int>(history_.size())) {
        // Alternating if every consecutive pair differs strongly with flipping sign.
        int flips = 0;
        for (size_t i = 1; i < history_.size(); ++i) {
            const size_t a = (count_ + i - 1) % history_.size(), b = (count_ + i) % history_.size();
            const float hi = std::max(history_[a], history_[b]), lo = std::min(history_[a], history_[b]);
            if (hi > 0.02f && hi > lo * 1.35f) ++flips;
        }
        alternating_ = flips >= static_cast<int>(history_.size()) - 2;
    }
    const bool lit = !alternating_ || m >= lastMean_;
    if (alternating_ && !lit) {
        // Remember the dark (ambient-only) frame for subtraction.
        dark_ = frame;
    } else if (alternating_ && subtractAmbient && dark_.data.size() == frame.data.size()) {
        for (size_t i = 0; i < frame.data.size(); ++i) {
            const int v = static_cast<int>(frame.data[i]) - dark_.data[i];
            frame.data[i] = static_cast<uint8_t>(std::clamp(v * 2, 0, 255));  // x2 restores range
        }
    }
    lastMean_ = m;
    return lit;
}

void tintNightPreview(Image& rgb) {
    // Soft teal-green phosphor look; keeps it readable rather than "army green".
    for (size_t i = 0; i + 2 < rgb.data.size(); i += 3) {
        const int v = rgb.data[i + 1];
        rgb.data[i] = static_cast<uint8_t>(v * 55 / 100);
        rgb.data[i + 1] = static_cast<uint8_t>(std::min(255, v * 105 / 100 + 6));
        rgb.data[i + 2] = static_cast<uint8_t>(v * 85 / 100);
    }
}

}  // namespace vl
