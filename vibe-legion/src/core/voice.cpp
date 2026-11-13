#include "core/voice.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>


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
    if (state_ == State::Done) return state_;
    buf_.insert(buf_.end(), pcm, pcm + n);
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
                // Keep 150 ms of the trailing quiet: soft word endings stay in.
                const size_t tail = static_cast<size_t>(std::max(0, quietMs_ - 150)) * kRate / 1000;
                end_ = done_ + kFrame > tail ? done_ + kFrame - tail : done_ + kFrame;
            }
        }
        done_ += kFrame;
    }
    // Always listening: while nobody speaks only the last second is kept (the pre-roll).
    if (state_ == State::Waiting && buf_.size() > static_cast<size_t>(kRate) * 2) {
        const size_t drop = (buf_.size() - kRate) / kFrame * kFrame;
        const size_t cut = std::min(drop, done_);
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(cut));
        done_ -= cut;
    }
    return state_;
}

std::vector<float> Endpointer::utterance() const {
    if (state_ == State::Waiting) return {};
    const size_t e = state_ == State::Done ? std::min(end_, buf_.size()) : buf_.size();
    if (e <= start_) return {};
    return std::vector<float>(buf_.begin() + static_cast<std::ptrdiff_t>(start_), buf_.begin() + static_cast<std::ptrdiff_t>(e));
}

void Endpointer::restart() {
    state_ = State::Waiting;
    buf_.clear();
    done_ = start_ = end_ = 0;
    loud_ = quietMs_ = speechMs_ = 0;
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

// ------------------------------------------------------------------ phrase features

namespace {

// Pitch of one 40 ms window by normalised autocorrelation, 70..400 Hz. 0 = unvoiced.
float pitchHz(const float* x, int n) {
    const int minLag = kRate / 400, maxLag = kRate / 70;
    if (n <= maxLag + 1) return 0.f;
    double e0 = 0;
    for (int i = 0; i < n; ++i) e0 += static_cast<double>(x[i]) * x[i];
    if (e0 < 1e-7 * n) return 0.f;
    double best = 0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double s = 0, e1 = 0;
        for (int i = 0; i + lag < n; ++i) {
            s += static_cast<double>(x[i]) * x[i + lag];
            e1 += static_cast<double>(x[i + lag]) * x[i + lag];
        }
        double e2 = 0;
        for (int i = 0; i + lag < n; ++i) e2 += static_cast<double>(x[i]) * x[i];
        const double r = s / std::sqrt(std::max(1e-12, e1 * e2));
        // Prefer the shortest lag among near-equal peaks: avoids octave-down errors.
        if (r > best * 1.06) {
            best = r;
            bestLag = lag;
        }
    }
    return best > 0.5 && bestLag > 0 ? static_cast<float>(kRate) / static_cast<float>(bestLag) : 0.f;
}

}  // namespace

std::vector<float> phraseFeatures(const std::vector<float>& pcm, int* frames) {
    int n10 = 0;
    const std::vector<float> mel = fbank(pcm, &n10);  // 10 ms frames, mean-normalised
    const int n = n10 / 2;                            // 20 ms steps
    if (frames) *frames = n;
    std::vector<float> out(static_cast<size_t>(std::max(0, n)) * kTplDim, 0.f);
    if (n < 4) {
        if (frames) *frames = 0;
        return {};
    }
    // DCT of the log-mel spectrum: coefficients 1..12 describe its shape (the sound),
    // not its level or fine harmonic structure.
    static const std::vector<float> dct = [] {
        std::vector<float> d(static_cast<size_t>(kCep) * kMelBins);
        for (int k = 0; k < kCep; ++k)
            for (int j = 0; j < kMelBins; ++j)
                d[static_cast<size_t>(k) * kMelBins + j] = static_cast<float>(std::cos(kPi * (k + 1) * (j + 0.5) / kMelBins));
        return d;
    }();
    std::vector<float> loud(static_cast<size_t>(n)), pitch(static_cast<size_t>(n), 0.f);
    for (int t = 0; t < n; ++t) {
        const float* a = mel.data() + static_cast<size_t>(2 * t) * kMelBins;
        const float* b = a + kMelBins;
        float* o = out.data() + static_cast<size_t>(t) * kTplDim;
        for (int k = 0; k < kCep; ++k) {
            float s = 0.f;
            const float* d = dct.data() + static_cast<size_t>(k) * kMelBins;
            for (int j = 0; j < kMelBins; ++j) s += d[j] * (a[j] + b[j]) * 0.5f;
            o[k] = s;
        }
        const size_t at = static_cast<size_t>(2 * t) * 160;
        const size_t len = std::min<size_t>(640, pcm.size() - at);
        loud[static_cast<size_t>(t)] = levelDb(pcm.data() + at, std::min<size_t>(len, 400));
        pitch[static_cast<size_t>(t)] = pitchHz(pcm.data() + at, static_cast<int>(len));
    }
    // Per-phrase normalisation of every cepstral coefficient (zero mean, unit spread).
    for (int k = 0; k < kCep; ++k) {
        double m = 0, v = 0;
        for (int t = 0; t < n; ++t) m += out[static_cast<size_t>(t) * kTplDim + k];
        m /= n;
        for (int t = 0; t < n; ++t) {
            const double d = out[static_cast<size_t>(t) * kTplDim + k] - m;
            v += d * d;
        }
        const float sd = static_cast<float>(std::sqrt(v / n)) + 1e-3f;
        for (int t = 0; t < n; ++t) {
            float& x = out[static_cast<size_t>(t) * kTplDim + k];
            x = (x - static_cast<float>(m)) / sd;
        }
    }
    // Loudness contour: relative to the loudest part, 0 (peak) .. -1 (30 dB below or quieter).
    float peak = -120.f;
    for (float l : loud) peak = std::max(peak, l);
    // Pitch contour: octaves above / below the phrase's median pitch; unvoiced frames 0.
    std::vector<float> voiced;
    for (float f : pitch)
        if (f > 0) voiced.push_back(f);
    float median = 0.f;
    if (!voiced.empty()) {
        std::nth_element(voiced.begin(), voiced.begin() + static_cast<std::ptrdiff_t>(voiced.size() / 2), voiced.end());
        median = voiced[voiced.size() / 2];
    }
    for (int t = 0; t < n; ++t) {
        float* o = out.data() + static_cast<size_t>(t) * kTplDim;
        o[kCep] = std::clamp((loud[static_cast<size_t>(t)] - peak) / 30.f, -1.f, 0.f);
        const float f = pitch[static_cast<size_t>(t)];
        o[kCep + 1] = f > 0 && median > 0 ? std::clamp(std::log2(f / median), -1.f, 1.f) : 0.f;
        o[kCep + 2] = f > 0 ? 1.f : 0.f;
    }
    return out;
}

// ------------------------------------------------------------------ matching

namespace {

// Weights of the three groups next to the 12 cepstra (each has unit spread).
constexpr float kLoudWeight = 2.0f;

float frameDist(const float* a, const float* b, float pitchW) {
    float s = 0.f;
    for (int k = 0; k < kCep; ++k) {
        const float d = a[k] - b[k];
        s += d * d;
    }
    const float dl = (a[kCep] - b[kCep]) * kLoudWeight;
    s += dl * dl;
    // Pitch counts where both frames are voiced; a voiced frame against an unvoiced one
    // is a mismatch of its own (different sound).
    if (a[kCep + 2] > 0.5f && b[kCep + 2] > 0.5f) {
        const float dp = (a[kCep + 1] - b[kCep + 1]) * pitchW;
        s += dp * dp;
    } else if (a[kCep + 2] != b[kCep + 2]) {
        s += 1.0f;
    }
    return std::sqrt(s / static_cast<float>(kCep));
}

}  // namespace

float dtwDistance(const VoiceTake& a, const VoiceTake& b, float intonation) {
    const int n = a.frames, m = b.frames;
    if (n < 4 || m < 4) return 99.f;
    // Twice as fast or slow is another way of saying it, or another phrase.
    if (n > 2 * m || m > 2 * n) return 99.f;
    const float pitchW = 8.f * std::clamp(intonation, 0.f, 1.f);  // an octave apart ~ a wrong sound at full weight
    const int band = std::max(std::abs(n - m) + 2, static_cast<int>(0.35f * static_cast<float>(std::max(n, m))));
    const float inf = 1e30f;
    std::vector<float> prev(static_cast<size_t>(m) + 1, inf), cur(static_cast<size_t>(m) + 1, inf);
    std::vector<int> prevLen(static_cast<size_t>(m) + 1, 0), curLen(static_cast<size_t>(m) + 1, 0);
    prev[0] = 0.f;
    for (int i = 1; i <= n; ++i) {
        std::fill(cur.begin(), cur.end(), inf);
        const int centre = static_cast<int>(static_cast<long long>(i) * m / n);
        const int lo = std::max(1, centre - band), hi = std::min(m, centre + band);
        const float* fa = a.feat.data() + static_cast<size_t>(i - 1) * kTplDim;
        for (int j = lo; j <= hi; ++j) {
            const float d = frameDist(fa, b.feat.data() + static_cast<size_t>(j - 1) * kTplDim, pitchW);
            float best = prev[static_cast<size_t>(j - 1)];
            int len = prevLen[static_cast<size_t>(j - 1)];
            if (prev[static_cast<size_t>(j)] < best) { best = prev[static_cast<size_t>(j)]; len = prevLen[static_cast<size_t>(j)]; }
            if (cur[static_cast<size_t>(j - 1)] < best) { best = cur[static_cast<size_t>(j - 1)]; len = curLen[static_cast<size_t>(j - 1)]; }
            if (best >= inf) continue;
            cur[static_cast<size_t>(j)] = best + d;
            curLen[static_cast<size_t>(j)] = len + 1;
        }
        std::swap(prev, cur);
        std::swap(prevLen, curLen);
    }
    const float total = prev[static_cast<size_t>(m)];
    const int steps = prevLen[static_cast<size_t>(m)];
    return total >= inf || steps == 0 ? 99.f : total / static_cast<float>(steps);
}

namespace {

// Typical distance between takes of one phrase said by one person, and the bounds the
// learned threshold must stay within (measured on recordings; see tests/voicetest.cpp).
// Same phrase, same voice: 0.25..0.6. A phrase one word apart ("включи" / "выключи"): ~0.5.
// Different phrases: 0.9 and more.
constexpr float kTypicalSpread = 0.42f;
constexpr float kMinThreshold = 0.56f, kMaxThreshold = 0.80f;

// Mean distance from every take to its nearest other take.
float spread(const VoiceModel& m, float intonation) {
    if (m.takes.size() < 2) return kTypicalSpread;
    double sum = 0;
    for (size_t i = 0; i < m.takes.size(); ++i) {
        float best = 99.f;
        for (size_t j = 0; j < m.takes.size(); ++j)
            if (j != i) best = std::min(best, dtwDistance(m.takes[i], m.takes[j], intonation));
        sum += best;
    }
    return static_cast<float>(sum / static_cast<double>(m.takes.size()));
}

float modelDistance(const VoiceModel& m, const VoiceTake& t, float intonation) {
    std::vector<float> d;
    for (const auto& k : m.takes) d.push_back(dtwDistance(k, t, intonation));
    if (d.empty()) return 99.f;
    std::sort(d.begin(), d.end());
    return d.size() >= 2 ? (d[0] + d[1]) * 0.5f : d[0];
}

}  // namespace

void learnThreshold(VoiceModel& m, float intonation) {
    // A live phrase is a little further from the takes than the takes are from each other
    // (they were recorded in one sitting): allow half as much again as their own spread.
    m.threshold = std::clamp(spread(m, intonation) * 1.5f + 0.05f, kMinThreshold, kMaxThreshold);
}

float takeOddness(const VoiceModel& m, const VoiceTake& t, float intonation) {
    if (m.takes.empty()) return 0.f;
    float best = 99.f;
    for (const auto& k : m.takes) best = std::min(best, dtwDistance(k, t, intonation));
    return best / std::max(kTypicalSpread, spread(m, intonation));
}

VoiceModel* VoiceStore::find(const std::string& id) {
    for (auto& c : commands)
        if (c.id == id) return &c;
    return nullptr;
}
const VoiceModel* VoiceStore::find(const std::string& id) const {
    for (const auto& c : commands)
        if (c.id == id) return &c;
    return nullptr;
}
VoiceModel& VoiceStore::get(const std::string& id) {
    if (VoiceModel* m = find(id)) return *m;
    commands.push_back({});
    commands.back().id = id;
    return commands.back();
}
void VoiceStore::remove(const std::string& id) {
    commands.erase(std::remove_if(commands.begin(), commands.end(), [&](const VoiceModel& m) { return m.id == id; }), commands.end());
}

VoiceMatch matchPhrase(const VoiceStore& store, const std::vector<VoiceCommand>& commands, const VoiceTake& heard,
                       float intonation) {
    VoiceMatch r;
    float second = 99.f;
    for (const auto& c : commands) {
        const VoiceModel* m = store.find(c.id);
        if (!c.enabled || !m || !m->ready()) continue;
        const float thr = std::max(0.01f, m->threshold * std::clamp(c.strictness, 0.6f, 1.5f));
        const float ratio = modelDistance(*m, heard, intonation) / thr;
        if (ratio < r.ratio) {
            second = r.ratio;
            r.ratio = ratio;
            r.nearest = c.id;
        } else {
            second = std::min(second, ratio);
        }
    }
    r.margin = r.ratio > 0 ? second / r.ratio : 99.f;
    // Close enough, and not a toss-up between two commands that sound alike.
    if (!r.nearest.empty() && r.ratio <= 1.f && r.margin >= 1.12f) r.id = r.nearest;
    return r;
}

// ------------------------------------------------------------------ storage

namespace {
void put32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void putFloats(std::vector<uint8_t>& o, const std::vector<float>& f) {
    put32(o, static_cast<uint32_t>(f.size()));
    const auto* p = reinterpret_cast<const uint8_t*>(f.data());
    o.insert(o.end(), p, p + f.size() * sizeof(float));
}
struct Reader {
    const std::vector<uint8_t>& b;
    size_t at = 0;
    bool ok = true;
    uint32_t u32() {
        if (at + 4 > b.size()) { ok = false; return 0; }
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + i]) << (8 * i);
        at += 4;
        return v;
    }
    std::vector<float> floats() {
        const uint32_t n = u32();
        if (!ok || n > (1u << 22) || at + static_cast<size_t>(n) * sizeof(float) > b.size()) { ok = false; return {}; }
        std::vector<float> f(n);
        if (n) std::memcpy(f.data(), b.data() + at, static_cast<size_t>(n) * sizeof(float));
        at += static_cast<size_t>(n) * sizeof(float);
        return f;
    }
    std::string str() {
        const uint32_t n = u32();
        if (!ok || n > 4096 || at + n > b.size()) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(b.data() + at), n);
        at += n;
        return s;
    }
};
}  // namespace

std::vector<uint8_t> VoiceStore::serialize() const {
    std::vector<uint8_t> out = {'V', 'H', 'V', '2'};
    put32(out, static_cast<uint32_t>(commands.size()));
    for (const auto& c : commands) {
        put32(out, static_cast<uint32_t>(c.id.size()));
        out.insert(out.end(), c.id.begin(), c.id.end());
        putFloats(out, {c.threshold});
        put32(out, static_cast<uint32_t>(c.takes.size()));
        for (const auto& t : c.takes) {
            put32(out, static_cast<uint32_t>(t.frames));
            putFloats(out, t.feat);
            putFloats(out, t.emb);
        }
    }
    return out;
}

bool VoiceStore::deserialize(const std::vector<uint8_t>& blob) {
    commands.clear();
    if (blob.size() < 8 || std::memcmp(blob.data(), "VHV2", 4) != 0) return false;
    Reader r{blob, 4};
    const uint32_t n = r.u32();
    if (n > 1024) return false;
    for (uint32_t i = 0; i < n && r.ok; ++i) {
        VoiceModel m;
        m.id = r.str();
        const auto thr = r.floats();
        m.threshold = thr.empty() ? 0.f : thr[0];
        const uint32_t takes = r.u32();
        if (takes > 64) { r.ok = false; break; }
        for (uint32_t k = 0; k < takes && r.ok; ++k) {
            VoiceTake t;
            t.frames = static_cast<int>(r.u32());
            t.feat = r.floats();
            t.emb = r.floats();
            if (t.frames < 0 || t.feat.size() != static_cast<size_t>(t.frames) * kTplDim) r.ok = false;
            else m.takes.push_back(std::move(t));
        }
        if (r.ok) commands.push_back(std::move(m));
    }
    if (!r.ok) commands.clear();
    return r.ok;
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

float ownerScore(const VoiceModel& m, const std::vector<float>& emb) {
    std::vector<float> sims;
    for (const auto& t : m.takes)
        if (!t.emb.empty() && !emb.empty()) sims.push_back(cosine(t.emb, emb));
    if (sims.empty()) return -1.f;
    std::sort(sims.begin(), sims.end(), [](float a, float b) { return a > b; });
    return sims.size() >= 2 ? (sims[0] + sims[1]) * 0.5f : sims[0];
}

}  // namespace vl::voice
