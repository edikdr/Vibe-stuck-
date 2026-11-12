#include "core/voice.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>

#include "core/json.hpp"
#include "core/macro.hpp"

namespace vl::voice {

namespace {

constexpr int kWin = 400;    // 25 ms
constexpr int kShift = 160;  // 10 ms
constexpr int kFft = 512;
constexpr double kPi = 3.14159265358979323846;

void fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / static_cast<double>(len);
        const std::complex<float> wl(static_cast<float>(std::cos(ang)), static_cast<float>(std::sin(ang)));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

float mel(float hz) { return 1127.f * std::log(1.f + hz / 700.f); }

// Triangular filters, equally spaced on the mel scale between 20 Hz and Nyquist, over
// the first kFft/2 bins (Kaldi leaves the Nyquist bin out).
struct MelBank {
    std::vector<std::vector<std::pair<int, float>>> bins;  // per filter: (fft bin, weight)
    MelBank() : bins(kMelBins) {
        const float lo = mel(20.f), hi = mel(kRate / 2.f);
        const float delta = (hi - lo) / (kMelBins + 1);
        for (int j = 0; j < kMelBins; ++j) {
            const float left = lo + j * delta, centre = left + delta, right = centre + delta;
            for (int i = 0; i < kFft / 2; ++i) {
                const float m = mel(static_cast<float>(i) * kRate / kFft);
                if (m <= left || m >= right) continue;
                bins[j].push_back({i, m <= centre ? (m - left) / (centre - left) : (right - m) / (right - centre)});
            }
        }
    }
};

const MelBank& melBank() {
    static const MelBank b;
    return b;
}

const std::vector<float>& hamming() {
    static const std::vector<float> w = [] {
        std::vector<float> v(kWin);
        for (int i = 0; i < kWin; ++i) v[i] = static_cast<float>(0.54 - 0.46 * std::cos(2 * kPi * i / (kWin - 1)));
        return v;
    }();
    return w;
}

}  // namespace

std::vector<float> fbank(const std::vector<float>& pcm, int* frames) {
    const int n = pcm.size() < static_cast<size_t>(kWin) ? 0 : 1 + static_cast<int>((pcm.size() - kWin) / kShift);
    if (frames) *frames = n;
    std::vector<float> out(static_cast<size_t>(n) * kMelBins);
    if (n == 0) return out;
    const auto& win = hamming();
    const auto& bank = melBank();
    std::vector<float> fr(kWin);
    std::vector<std::complex<float>> spec(kFft);
    std::vector<float> power(kFft / 2);
    for (int t = 0; t < n; ++t) {
        const float* x = pcm.data() + static_cast<size_t>(t) * kShift;
        double mean = 0;
        for (int i = 0; i < kWin; ++i) mean += x[i];
        mean /= kWin;
        for (int i = 0; i < kWin; ++i) fr[i] = (x[i] - static_cast<float>(mean)) * 32768.f;  // int16 scale, no DC
        for (int i = kWin - 1; i > 0; --i) fr[i] -= 0.97f * fr[i - 1];                        // pre-emphasis
        fr[0] -= 0.97f * fr[0];
        for (int i = 0; i < kFft; ++i) spec[i] = i < kWin ? std::complex<float>(fr[i] * win[i], 0.f) : std::complex<float>();
        fft(spec);
        for (int i = 0; i < kFft / 2; ++i) power[i] = std::norm(spec[i]);
        float* row = out.data() + static_cast<size_t>(t) * kMelBins;
        for (int j = 0; j < kMelBins; ++j) {
            float e = 0.f;
            for (const auto& [bin, w] : bank.bins[j]) e += power[bin] * w;
            row[j] = std::log(std::max(e, FLT_EPSILON));
        }
    }
    // Mean normalisation over the utterance: removes the microphone / room colouring.
    for (int j = 0; j < kMelBins; ++j) {
        double m = 0;
        for (int t = 0; t < n; ++t) m += out[static_cast<size_t>(t) * kMelBins + j];
        m /= n;
        for (int t = 0; t < n; ++t) out[static_cast<size_t>(t) * kMelBins + j] -= static_cast<float>(m);
    }
    return out;
}

float levelDb(const float* pcm, size_t n) {
    if (!n) return -120.f;
    double s = 0;
    for (size_t i = 0; i < n; ++i) s += static_cast<double>(pcm[i]) * pcm[i];
    const double rms = std::sqrt(s / static_cast<double>(n));
    return rms < 1e-6 ? -120.f : static_cast<float>(20.0 * std::log10(rms));
}

Endpointer::State Endpointer::feed(const float* pcm, size_t n) {
    buf_.insert(buf_.end(), pcm, pcm + n);
    total_ += n;
    while (state_ != State::Done && done_ + kFrame <= buf_.size()) {
        const float db = levelDb(buf_.data() + done_, kFrame);
        lastDb_ = db;
        if (!noiseInit_) {
            noiseDb_ = std::clamp(db, -75.f, -35.f);
            noiseInit_ = true;
        }
        const float thr = std::max(noiseDb_ + 9.f, -52.f);
        const bool loud = db > thr;
        if (!loud || state_ == State::Waiting) {
            // The floor drops quickly and rises slowly, so speech itself does not lift it.
            const float target = std::clamp(db, -75.f, -30.f);
            noiseDb_ += (target < noiseDb_ ? 0.25f : loud ? 0.004f : 0.03f) * (target - noiseDb_);
        }
        if (state_ == State::Waiting) {
            loud_ = loud ? loud_ + 1 : 0;
            if (loud_ >= 3) {  // 60 ms of speech
                state_ = State::Speaking;
                const size_t pre = static_cast<size_t>(kRate) * 300 / 1000 + 3 * kFrame;
                start_ = done_ + kFrame > pre ? done_ + kFrame - pre : 0;
                quietMs_ = 0;
                speechMs_ = 60;
            }
        } else {
            speechMs_ += 20;
            quietMs_ = loud ? 0 : quietMs_ + 20;
            if (quietMs_ >= hangMs_ || speechMs_ >= maxSpeechMs_) {
                state_ = State::Done;
                // Keep 200 ms of the trailing quiet: enough for the recogniser to close the last word.
                const size_t tail = static_cast<size_t>(std::max(0, quietMs_ - 200)) * kRate / 1000;
                end_ = done_ + kFrame > tail ? done_ + kFrame - tail : done_ + kFrame;
            }
        }
        done_ += kFrame;
    }
    return state_;
}

std::vector<float> Endpointer::utterance() const {
    if (state_ == State::Waiting) return {};
    const size_t e = state_ == State::Done ? std::min(end_, buf_.size()) : buf_.size();
    if (e <= start_) return {};
    return std::vector<float>(buf_.begin() + static_cast<std::ptrdiff_t>(start_), buf_.begin() + static_cast<std::ptrdiff_t>(e));
}

std::vector<float> speechOnly(const std::vector<float>& pcm) {
    const size_t frame = kRate / 50;
    const size_t n = pcm.size() / frame;
    if (!n) return {};
    std::vector<float> db(n);
    for (size_t i = 0; i < n; ++i) db[i] = levelDb(pcm.data() + i * frame, frame);
    // Noise floor: the quiet 15 % of the recording; speech is well above it.
    std::vector<float> sorted = db;
    std::sort(sorted.begin(), sorted.end());
    const float floorDb = sorted[n * 15 / 100];
    const float thr = std::max(floorDb + 9.f, -52.f);
    std::vector<char> keep(n, 0);
    for (size_t i = 0; i < n; ++i)
        if (db[i] > thr) {  // 100 ms of margin around loud frames keeps soft consonants
            const size_t a = i >= 5 ? i - 5 : 0, b = std::min(n - 1, i + 5);
            for (size_t k = a; k <= b; ++k) keep[k] = 1;
        }
    std::vector<float> out;
    for (size_t i = 0; i < n; ++i)
        if (keep[i]) out.insert(out.end(), pcm.begin() + static_cast<std::ptrdiff_t>(i * frame), pcm.begin() + static_cast<std::ptrdiff_t>((i + 1) * frame));
    return out;
}

// ------------------------------------------------------------------ voice print

std::vector<uint8_t> VoicePrint::serialize() const {
    std::vector<uint8_t> out;
    auto put32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    out.insert(out.end(), {'V', 'H', 'V', '1'});
    const uint32_t dim = samples.empty() ? 0 : static_cast<uint32_t>(samples[0].size());
    put32(static_cast<uint32_t>(samples.size()));
    put32(dim);
    for (const auto& s : samples) {
        if (s.size() != dim) continue;
        const auto* p = reinterpret_cast<const uint8_t*>(s.data());
        out.insert(out.end(), p, p + dim * sizeof(float));
    }
    return out;
}

bool VoicePrint::deserialize(const std::vector<uint8_t>& blob) {
    samples.clear();
    if (blob.size() < 12 || std::memcmp(blob.data(), "VHV1", 4) != 0) return false;
    auto get32 = [&](size_t at) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(blob[at + i]) << (8 * i);
        return v;
    };
    const uint32_t count = get32(4), dim = get32(8);
    if (count > 4096 || dim == 0 || dim > 4096) return count == 0;
    if (blob.size() < 12 + static_cast<size_t>(count) * dim * sizeof(float)) return false;
    for (uint32_t i = 0; i < count; ++i) {
        std::vector<float> s(dim);
        std::memcpy(s.data(), blob.data() + 12 + static_cast<size_t>(i) * dim * sizeof(float), dim * sizeof(float));
        samples.push_back(std::move(s));
    }
    return true;
}

void l2normalize(std::vector<float>& v) {
    double s = 0;
    for (float f : v) s += static_cast<double>(f) * f;
    const float inv = s > 1e-12 ? static_cast<float>(1.0 / std::sqrt(s)) : 0.f;
    for (float& f : v) f *= inv;
}

float cosine(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.f;
    double dot = 0, na = 0, nb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        na += static_cast<double>(a[i]) * a[i];
        nb += static_cast<double>(b[i]) * b[i];
    }
    return na < 1e-12 || nb < 1e-12 ? 0.f : static_cast<float>(dot / std::sqrt(na * nb));
}

float ownerScore(const VoicePrint& p, const std::vector<float>& emb) {
    if (p.samples.empty() || emb.empty()) return -1.f;
    std::vector<float> sims;
    for (const auto& s : p.samples) sims.push_back(cosine(s, emb));
    std::sort(sims.begin(), sims.end(), [](float a, float b) { return a > b; });
    const size_t k = std::min<size_t>(3, sims.size());
    float sum = 0.f;
    for (size_t i = 0; i < k; ++i) sum += sims[i];
    return sum / static_cast<float>(k);
}

// ------------------------------------------------------------------ phrases

std::string normalizePhrase(const std::string& utf8) {
    const std::string low = lowerUtf8(utf8);
    std::string out;
    bool space = false;
    for (size_t i = 0; i < low.size();) {
        const unsigned char c = static_cast<unsigned char>(low[i]);
        if (c < 0x80) {
            const bool word = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            if (word) {
                if (space && !out.empty()) out.push_back(' ');
                space = false;
                out.push_back(static_cast<char>(c));
            } else {
                space = true;
            }
            ++i;
            continue;
        }
        // Two-byte sequences cover Cyrillic; anything else (dashes, quotes, emoji) is a separator.
        const size_t len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        if (len == 2 && i + 1 < low.size()) {
            const uint32_t cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(low[i + 1]) & 0x3Fu);
            if ((cp >= 0x430 && cp <= 0x44F) || cp == 0x451) {
                if (space && !out.empty()) out.push_back(' ');
                space = false;
                if (cp == 0x451) out += "\xD0\xB5";  // ё -> е: the recogniser's dictionary has no ё
                else out.append(low, i, 2);
                i += 2;
                continue;
            }
        }
        space = true;
        i += std::min(len, low.size() - i);
    }
    return out;
}

std::vector<std::string> splitWords(const std::string& normalized) {
    std::vector<std::string> w;
    size_t pos = 0;
    while (pos < normalized.size()) {
        size_t sp = normalized.find(' ', pos);
        if (sp == std::string::npos) sp = normalized.size();
        if (sp > pos) w.push_back(normalized.substr(pos, sp - pos));
        pos = sp + 1;
    }
    return w;
}

int matchCommand(const std::vector<VoiceCommand>& commands, const std::string& heard) {
    const std::string h = normalizePhrase(heard);
    if (h.empty()) return -1;
    for (size_t i = 0; i < commands.size(); ++i)
        if (commands[i].enabled && normalizePhrase(commands[i].phrase) == h) return static_cast<int>(i);
    return -1;
}

std::string grammarJson(const std::vector<VoiceCommand>& commands) {
    json::Array a;
    std::vector<std::string> seen;
    for (const auto& c : commands) {
        const std::string p = normalizePhrase(c.phrase);
        if (!c.enabled || p.empty() || std::find(seen.begin(), seen.end(), p) != seen.end()) continue;
        seen.push_back(p);
        a.push_back(p);
    }
    a.push_back(std::string("[unk]"));
    json::Value v(std::move(a));
    return v.dump(0);
}

}  // namespace vl::voice
