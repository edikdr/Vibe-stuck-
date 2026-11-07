#include "core/image.hpp"

#include <algorithm>
#include <cmath>

namespace vl {

namespace {

inline uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

// BT.601 limited range YUV -> RGB, fixed point.
inline void yuv2rgb(int y, int u, int v, uint8_t* o) {
    int c = (y - 16) * 298;
    int d = u - 128, e = v - 128;
    o[0] = clamp8((c + 409 * e + 128) >> 8);
    o[1] = clamp8((c - 100 * d - 208 * e + 128) >> 8);
    o[2] = clamp8((c + 516 * d + 128) >> 8);
}

}  // namespace

void convertFrame(const uint8_t* src, int width, int height, int stride, PixelFormat fmt, int downscale,
                  bool mirror, Image& dst, size_t uvOffset) {
    const int k = std::max(1, downscale);
    const int ow = width / k, oh = height / k;
    if (dst.width != ow || dst.height != oh || dst.channels != 3) dst.reset(ow, oh, 3);
    const int kk = k * k;

    const uint8_t* uvPlane = src + (uvOffset ? uvOffset : static_cast<size_t>(stride) * height);

    for (int oy = 0; oy < oh; ++oy) {
        uint8_t* out = dst.row(oy);
        const int sy = oy * k;
        for (int ox = 0; ox < ow; ++ox) {
            const int sx = ox * k;
            uint8_t* px = out + 3 * (mirror ? (ow - 1 - ox) : ox);
            switch (fmt) {
                case PixelFormat::NV12: {
                    int ysum = 0;
                    for (int dy = 0; dy < k; ++dy) {
                        const uint8_t* yr = src + static_cast<size_t>(sy + dy) * stride + sx;
                        for (int dx = 0; dx < k; ++dx) ysum += yr[dx];
                    }
                    const int cy = (sy + k / 2) >> 1, cx = (sx + k / 2) & ~1;
                    const uint8_t* uv = uvPlane + static_cast<size_t>(cy) * stride + cx;
                    yuv2rgb(ysum / kk, uv[0], uv[1], px);
                    break;
                }
                case PixelFormat::YUY2: {
                    int ysum = 0;
                    for (int dy = 0; dy < k; ++dy) {
                        const uint8_t* r = src + static_cast<size_t>(sy + dy) * stride;
                        for (int dx = 0; dx < k; ++dx) ysum += r[(sx + dx) * 2];
                    }
                    const uint8_t* r = src + static_cast<size_t>(sy) * stride + (sx & ~1) * 2;
                    yuv2rgb(ysum / kk, r[1], r[3], px);
                    break;
                }
                case PixelFormat::Gray8: {
                    int sum = 0;
                    for (int dy = 0; dy < k; ++dy) {
                        const uint8_t* r = src + static_cast<size_t>(sy + dy) * stride + sx;
                        for (int dx = 0; dx < k; ++dx) sum += r[dx];
                    }
                    px[0] = px[1] = px[2] = static_cast<uint8_t>(sum / kk);
                    break;
                }
                case PixelFormat::Gray16: {
                    int sum = 0;
                    for (int dy = 0; dy < k; ++dy) {
                        const uint8_t* r = src + static_cast<size_t>(sy + dy) * stride + sx * 2;
                        for (int dx = 0; dx < k; ++dx) sum += r[dx * 2 + 1];  // high byte (LE)
                    }
                    px[0] = px[1] = px[2] = static_cast<uint8_t>(sum / kk);
                    break;
                }
                case PixelFormat::RGB24: {
                    const uint8_t* r = src + static_cast<size_t>(sy) * stride + sx * 3;
                    px[0] = r[0]; px[1] = r[1]; px[2] = r[2];
                    break;
                }
                case PixelFormat::BGRA32: {
                    const uint8_t* r = src + static_cast<size_t>(sy) * stride + sx * 4;
                    px[0] = r[2]; px[1] = r[1]; px[2] = r[0];
                    break;
                }
            }
        }
    }
}

namespace {

inline void bilinear(const Image& src, float x, float y, float out[3]) {
    const int c = src.channels;
    if (x < -1.f || y < -1.f || x > src.width || y > src.height) {
        out[0] = out[1] = out[2] = 0.f;
        return;
    }
    int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    float fx = x - x0, fy = y - y0;
    float acc[3] = {0, 0, 0};
    const float w[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    const int xs[4] = {x0, x0 + 1, x0, x0 + 1}, ys[4] = {y0, y0, y0 + 1, y0 + 1};
    for (int k = 0; k < 4; ++k) {
        if (xs[k] < 0 || ys[k] < 0 || xs[k] >= src.width || ys[k] >= src.height) continue;
        const uint8_t* p = src.row(ys[k]) + xs[k] * c;
        if (c == 1) {
            acc[0] += w[k] * p[0]; acc[1] += w[k] * p[0]; acc[2] += w[k] * p[0];
        } else {
            acc[0] += w[k] * p[0]; acc[1] += w[k] * p[1]; acc[2] += w[k] * p[2];
        }
    }
    out[0] = acc[0]; out[1] = acc[1]; out[2] = acc[2];
}

inline void store(const TensorSpec& spec, float* out, int x, int y, const float rgb[3]) {
    const int W = spec.width, H = spec.height;
    const float r = rgb[0] * spec.scale + spec.bias;
    const float g = rgb[1] * spec.scale + spec.bias;
    const float b = rgb[2] * spec.scale + spec.bias;
    const float c0 = spec.bgr ? b : r, c2 = spec.bgr ? r : b;
    if (spec.layout == TensorLayout::NHWC) {
        float* p = out + (static_cast<size_t>(y) * W + x) * 3;
        p[0] = c0; p[1] = g; p[2] = c2;
    } else {
        const size_t plane = static_cast<size_t>(W) * H, idx = static_cast<size_t>(y) * W + x;
        out[idx] = c0; out[plane + idx] = g; out[2 * plane + idx] = c2;
    }
}

}  // namespace

void warpToTensor(const Image& src, const Affine& m, const TensorSpec& spec, float* out) {
    float rgb[3], tmp[3];
    // When shrinking by more than ~2x, plain bilinear aliases badly (thin fingers vanish);
    // take a 2x2 supersample per output pixel instead (cheap stand-in for INTER_AREA).
    const float scale = std::sqrt(std::fabs(m.a * m.e - m.b * m.d));
    const bool ss = scale > 1.8f;
    for (int y = 0; y < spec.height; ++y) {
        for (int x = 0; x < spec.width; ++x) {
            if (!ss) {
                Vec2 s = m.apply({x + 0.5f, y + 0.5f});
                bilinear(src, s.x - 0.5f, s.y - 0.5f, rgb);
            } else {
                rgb[0] = rgb[1] = rgb[2] = 0.f;
                for (int k = 0; k < 4; ++k) {
                    Vec2 s = m.apply({x + 0.25f + 0.5f * (k & 1), y + 0.25f + 0.5f * (k >> 1)});
                    bilinear(src, s.x - 0.5f, s.y - 0.5f, tmp);
                    rgb[0] += tmp[0] * 0.25f; rgb[1] += tmp[1] * 0.25f; rgb[2] += tmp[2] * 0.25f;
                }
            }
            store(spec, out, x, y, rgb);
        }
    }
}

Affine letterboxToTensor(const Image& src, const TensorSpec& spec, float* out) {
    const float s = std::min(static_cast<float>(spec.width) / src.width, static_cast<float>(spec.height) / src.height);
    const float padX = (spec.width - src.width * s) * 0.5f, padY = (spec.height - src.height * s) * 0.5f;
    Affine t;  // tensor -> src
    t.a = 1.f / s; t.b = 0; t.c = -padX / s;
    t.d = 0; t.e = 1.f / s; t.f = -padY / s;
    warpToTensor(src, t, spec, out);
    return t;
}

Affine fitTopLeftToTensor(const Image& src, const TensorSpec& spec, float* out) {
    const float s = std::min(static_cast<float>(spec.width) / src.width, static_cast<float>(spec.height) / src.height);
    Affine t;
    t.a = 1.f / s; t.e = 1.f / s;
    warpToTensor(src, t, spec, out);
    return t;
}

void warpToImage(const Image& src, const Affine& m, Image& dst) {
    float rgb[3];
    for (int y = 0; y < dst.height; ++y) {
        uint8_t* row = dst.row(y);
        for (int x = 0; x < dst.width; ++x) {
            Vec2 s = m.apply({x + 0.5f, y + 0.5f});
            bilinear(src, s.x - 0.5f, s.y - 0.5f, rgb);
            uint8_t* p = row + x * dst.channels;
            for (int c = 0; c < dst.channels; ++c) p[c] = clamp8(static_cast<int>(rgb[c] + 0.5f));
        }
    }
}

void resizeBilinear(const Image& src, Image& dst, int w, int h) {
    if (dst.width != w || dst.height != h || dst.channels != src.channels) dst.reset(w, h, src.channels);
    Affine m;
    m.a = static_cast<float>(src.width) / w;
    m.e = static_cast<float>(src.height) / h;
    warpToImage(src, m, dst);
}

float meanLuma(const Image& img, int step) {
    if (img.empty()) return 0.f;
    step = std::max(1, step);
    uint64_t sum = 0, n = 0;
    for (int y = step / 2; y < img.height; y += step) {
        const uint8_t* r = img.row(y);
        for (int x = step / 2; x < img.width; x += step) {
            const uint8_t* p = r + x * img.channels;
            sum += img.channels >= 3 ? (77u * p[0] + 150u * p[1] + 29u * p[2]) >> 8 : p[0];
            ++n;
        }
    }
    return n ? static_cast<float>(sum) / (255.f * n) : 0.f;
}

}  // namespace vl
