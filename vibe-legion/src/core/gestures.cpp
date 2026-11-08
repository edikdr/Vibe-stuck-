#include "core/gestures.hpp"

#include <algorithm>
#include <cmath>

namespace vl {

const std::vector<BuiltinGesture>& builtinGestures() {
    static const std::vector<BuiltinGesture> g = {
        {"open_palm", "Открытая ладонь", "\xE2\x9C\x8B", false},
        {"fist", "Кулак", "\xE2\x9C\x8A", false},
        {"point", "Указательный палец", "\xE2\x98\x9D", false},
        {"victory", "Два пальца (V)", "\xE2\x9C\x8C", false},
        {"three", "Три пальца", "3", false},
        {"thumbs_up", "Большой палец вверх", "\xF0\x9F\x91\x8D", false},
        {"thumbs_down", "Большой палец вниз", "\xF0\x9F\x91\x8E", false},
        {"ok", "Жест «OK»", "\xF0\x9F\x91\x8C", false},
        {"pinch", "Щипок", "\xF0\x9F\xA4\x8F", false},
        {"rock", "«Коза»", "\xF0\x9F\xA4\x98", false},
        {"call", "«Позвони»", "\xF0\x9F\xA4\x99", false},
        {"swipe_left", "Взмах влево", "\xE2\x86\x90", true},
        {"swipe_right", "Взмах вправо", "\xE2\x86\x92", true},
        {"swipe_up", "Взмах вверх", "\xE2\x86\x91", true},
        {"swipe_down", "Взмах вниз", "\xE2\x86\x93", true},
        {"wave", "Помахать рукой", "\xF0\x9F\x91\x8B", true},
        {"two_palms", "Две ладони", "\xF0\x9F\x99\x8C", false, true},
        {"two_fists", "Два кулака", "\xE2\x9C\x8A", false, true},
        {"two_spread", "Две руки: развести в стороны", "\xE2\x86\x94", true, true},
        {"two_pinch", "Две руки: свести вместе", "\xE2\x86\x92\xE2\x86\x90", true, true},
    };
    return g;
}

const BuiltinGesture* findBuiltin(const std::string& id) {
    for (const auto& g : builtinGestures())
        if (id == g.id) return &g;
    return nullptr;
}

namespace {

constexpr int kFingerJoints[5][4] = {
    {THUMB_CMC, THUMB_MCP, THUMB_IP, THUMB_TIP},
    {INDEX_MCP, INDEX_PIP, INDEX_DIP, INDEX_TIP},
    {MIDDLE_MCP, MIDDLE_PIP, MIDDLE_DIP, MIDDLE_TIP},
    {RING_MCP, RING_PIP, RING_DIP, RING_TIP},
    {PINKY_MCP, PINKY_PIP, PINKY_DIP, PINKY_TIP},
};

enum Want : uint8_t { A, E, C };  // any / extended / curled

struct PoseDef {
    const char* id;
    Want f[5];
};

}  // namespace

void analyzeHand(HandObservation& h) {
    const auto& w = h.lm.world;
    const auto& im = h.lm.image;
    FingerState fs;

    // Fingers 1..4: total bend along the chain, MCP counts half (it legitimately flexes).
    for (int f = 1; f < 5; ++f) {
        const auto& j = kFingerJoints[f];
        const float aMcp = jointAngle(w[WRIST], w[j[0]], w[j[1]]);
        const float aPip = jointAngle(w[j[0]], w[j[1]], w[j[2]]);
        const float aDip = jointAngle(w[j[1]], w[j[2]], w[j[3]]);
        const float bend = 0.5f * (kPi - aMcp) + (kPi - aPip) + 0.5f * (kPi - aDip);
        fs.curl[f] = std::clamp((bend - 0.25f) / 2.3f, 0.f, 1.f);
        const bool reaches = length(w[j[3]] - w[WRIST]) > length(w[j[1]] - w[WRIST]) * 1.05f;
        const bool folded = length(w[j[3]] - w[WRIST]) < length(w[j[1]] - w[WRIST]);
        // A grey zone between "straight" and "bent": a half-bent finger (a hand holding
        // a phone, resting on the keyboard) satisfies neither and so cannot form a pose.
        fs.extended[f] = fs.curl[f] < 0.33f && reaches;
        fs.curled[f] = fs.curl[f] > 0.50f || folded;
    }
    // Thumb: straight chain and the tip well away from the palm centre.
    {
        const float aMcp = jointAngle(w[THUMB_CMC], w[THUMB_MCP], w[THUMB_IP]);
        const float aIp = jointAngle(w[THUMB_MCP], w[THUMB_IP], w[THUMB_TIP]);
        const float bend = (kPi - aMcp) + (kPi - aIp);
        const float palmW = std::max(1e-4f, length(w[INDEX_MCP] - w[PINKY_MCP]));
        const float away = length(w[THUMB_TIP] - w[MIDDLE_MCP]) / palmW;
        const float awayIdx = length(w[THUMB_TIP] - w[INDEX_PIP]) / palmW;
        fs.curl[0] = std::clamp(bend / 1.6f, 0.f, 1.f) * 0.5f + std::clamp(1.2f - away, 0.f, 1.f) * 0.5f;
        fs.extended[0] = bend < 1.1f && away > 0.95f && awayIdx > 0.45f;
        fs.curled[0] = !(bend < 1.4f && away > 0.8f);
    }
    h.fingers = fs;

    // Palm orientation: frames are mirrored (selfie), right palm facing camera => cross > 0.
    const Vec2 a = im[INDEX_MCP].xy() - im[WRIST].xy();
    const Vec2 b = im[PINKY_MCP].xy() - im[WRIST].xy();
    const float cr = cross(a, b);
    h.palmFacing = h.isRight ? cr > 0 : cr < 0;
    h.palmPx = length(im[MIDDLE_MCP].xy() - im[WRIST].xy());

    // Pinch / OK: thumb and index tips touching, relative to palm length (metric).
    const float palmLen = std::max(1e-4f, length(w[MIDDLE_MCP] - w[WRIST]));
    const float tipGap = length(w[THUMB_TIP] - w[INDEX_TIP]) / palmLen;
    const bool touching = tipGap < 0.32f;

    // Thumb direction in the image (for thumbs up/down).
    const Vec2 tdir = im[THUMB_TIP].xy() - im[THUMB_MCP].xy();
    const bool thumbVertical = std::fabs(tdir.y) > std::fabs(tdir.x) * 1.2f && std::fabs(tdir.y) > 0.4f * h.palmPx;

    static const PoseDef defs[] = {
        {"call", {E, C, C, C, E}},
        {"rock", {A, E, C, C, E}},
        {"open_palm", {A, E, E, E, E}},
        {"three", {A, E, E, E, C}},
        {"victory", {A, E, E, C, C}},
        {"point", {A, E, C, C, C}},
        {"fist", {A, C, C, C, C}},
    };

    auto score = [&](const PoseDef& d, bool& ok) {
        float s = 0.f;
        int n = 0;
        ok = true;
        for (int f = 0; f < 5; ++f) {
            if (d.f[f] == A) continue;
            const bool want = d.f[f] == E;
            if (want ? !fs.extended[f] : !fs.curled[f]) ok = false;
            s += want ? 1.f - fs.curl[f] : fs.curl[f];
            ++n;
        }
        return n ? s / n : 0.f;
    };

    h.pose.clear();
    h.poseConfidence = 0.f;
    // Special poses first.
    if (touching && fs.extended[2] && fs.extended[3] && fs.extended[4]) {
        h.pose = "ok";
        h.poseConfidence = std::clamp(1.f - tipGap / 0.32f, 0.3f, 1.f);
        return;
    }
    if (fs.extended[0] && thumbVertical && fs.curled[1] && fs.curled[2] && fs.curled[3] && fs.curled[4]) {
        h.pose = tdir.y < 0 ? "thumbs_up" : "thumbs_down";
        h.poseConfidence = std::clamp(0.5f * (fs.curl[1] + fs.curl[2] + fs.curl[3] + fs.curl[4]) / 2.f + 0.5f, 0.f, 1.f);
        return;
    }
    if (touching && fs.curl[1] < 0.85f && !(fs.extended[2] && fs.extended[3])) {
        h.pose = "pinch";
        h.poseConfidence = std::clamp(1.f - tipGap / 0.32f, 0.3f, 1.f);
        return;
    }
    // Mean direction of the extended long fingers, in the image: poses shown to the camera
    // (V, three, open palm) have the fingers up; the same finger shape pointing sideways or
    // down is a hand busy with something else (holding a phone, typing).
    Vec2 fdir{};
    for (int f = 1; f < 5; ++f)
        if (fs.extended[f]) fdir += im[kFingerJoints[f][3]].xy() - im[kFingerJoints[f][0]].xy();
    const bool fingersUp = fdir.y < -0.34f * length(fdir);
    for (const auto& d : defs) {
        bool ok;
        const float s = score(d, ok);
        if (!ok) continue;
        const std::string id = d.id;
        if (id == "open_palm" && !h.palmFacing) continue;
        if ((id == "open_palm" || id == "victory" || id == "three") && !fingersUp) continue;
        if (s < 0.6f) continue;  // too close to the grey zone
        h.pose = d.id;
        h.poseConfidence = s;
        return;
    }
}

std::array<float, 42> handTemplate(const HandLandmarks& lm, bool isRight, bool rotationInvariant,
                                   bool mirrorInvariant) {
    std::array<float, 42> out{};
    const Vec2 o = lm.image[WRIST].xy();
    const Vec2 axis = lm.image[MIDDLE_MCP].xy() - o;
    const float len = std::max(1e-3f, length(axis));
    float cs = 1.f, sn = 0.f;
    if (rotationInvariant) {
        // Rotate so the wrist->middle MCP axis points to (0,-1).
        const float ang = std::atan2(axis.y, axis.x);  // current
        const float rot = -kPi * 0.5f - ang;
        cs = std::cos(rot);
        sn = std::sin(rot);
    }
    const bool flip = mirrorInvariant && !isRight;
    for (int i = 0; i < kHandLandmarks; ++i) {
        Vec2 p = (lm.image[i].xy() - o) / len;
        Vec2 r{cs * p.x - sn * p.y, sn * p.x + cs * p.y};
        if (flip) r.x = -r.x;
        out[2 * i] = r.x;
        out[2 * i + 1] = r.y;
    }
    return out;
}

float templateDistance(const std::array<float, 42>& a, const std::array<float, 42>& b) {
    float s = 0.f;
    for (int i = 0; i < 42; i += 2) {
        const float dx = a[i] - b[i], dy = a[i + 1] - b[i + 1];
        s += dx * dx + dy * dy;
    }
    return std::sqrt(s / 21.f);
}

void matchCustom(HandObservation& h, const std::vector<CustomGesture>& gestures) {
    h.customId.clear();
    h.customConfidence = 0.f;
    for (const auto& g : gestures) {
        if (g.samples.empty()) continue;
        const auto t = handTemplate(h.lm, h.isRight, g.rotationInvariant, g.mirrorInvariant);
        float near[3] = {1e9f, 1e9f, 1e9f};  // 3 smallest distances, ascending
        for (const auto& s : g.samples) {
            float d = templateDistance(t, s);
            for (float& n : near)
                if (d < n) std::swap(d, n);
        }
        const size_t k = std::min<size_t>(3, g.samples.size());
        float best = 0.f;
        for (size_t i = 0; i < k; ++i) best += near[i];
        best /= static_cast<float>(k);
        const float conf = 1.f - best / std::max(0.01f, g.tolerance);
        if (conf > 0.f && conf > h.customConfidence) {
            h.customConfidence = conf;
            h.customId = g.id;
        }
    }
}

namespace {

// Outlier removal, de-duplication and thinning, for any sample type.
template <class Row, class Dist>
std::vector<Row> condenseImpl(const std::vector<Row>& raw, size_t maxSamples, Dist dist, float dupEps) {
    const size_t n = raw.size();
    if (n == 0 || maxSamples == 0) return {};
    // 1. Outliers: a sample whose median distance to the others is far above the
    //    typical one belongs to a transition, not to the pose being recorded.
    std::vector<float> med(n, 0.f);
    if (n >= 5) {
        std::vector<float> d;
        const size_t stride = std::max<size_t>(1, n / 60);  // bounded cost for long recordings
        for (size_t i = 0; i < n; ++i) {
            d.clear();
            for (size_t j = 0; j < n; j += stride)
                if (j != i) d.push_back(dist(raw[i], raw[j]));
            std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
            med[i] = d[d.size() / 2];
        }
    }
    std::vector<float> sorted = med;
    std::nth_element(sorted.begin(), sorted.begin() + n / 2, sorted.end());
    const float typical = sorted[n / 2];
    const float cut = std::max(typical * 2.0f, typical + 0.05f);
    std::vector<size_t> keep;
    for (size_t i = 0; i < n; ++i)
        if (n < 5 || med[i] <= cut) keep.push_back(i);
    // 2. Near-duplicates (hand held perfectly still) add nothing.
    std::vector<Row> out;
    for (size_t i : keep) {
        bool dup = false;
        for (const auto& o : out)
            if (dist(raw[i], o) < dupEps) { dup = true; break; }
        if (!dup) out.push_back(raw[i]);
    }
    // 3. Spread the remainder evenly over the recording.
    if (out.size() > maxSamples) {
        std::vector<Row> thin;
        for (size_t i = 0; i < maxSamples; ++i) thin.push_back(out[i * out.size() / maxSamples]);
        out.swap(thin);
    }
    return out;
}

float rms(const float* a, const float* b, size_t n) {
    if (n == 0) return 0.f;
    float s = 0.f;
    for (size_t i = 0; i < n; ++i) {
        const float d = a[i] - b[i];
        s += d * d;
    }
    return std::sqrt(s / static_cast<float>(n));
}

Vec3 normalized(Vec3 v) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.f / l) : Vec3{};
}

constexpr size_t kTwoRow = 2 * kHandFeat + 2;  // two hands + their offset

// Distance between two sample rows of a gesture of the given kind.
float rowDistance(CustomKind kind, const std::vector<float>& a, const std::vector<float>& b) {
    if (kind != CustomKind::TwoHands) return featureDistance(a, b, kHandFeat);
    if (a.size() < kTwoRow || b.size() < kTwoRow) return 1e9f;
    const float shapes = 0.5f * (rms(a.data(), b.data(), kHandFeat) + rms(a.data() + kHandFeat, b.data() + kHandFeat, kHandFeat));
    // Where the hands are relative to each other matters: one palm length off adds 0.25.
    const float rel = std::hypot(a[2 * kHandFeat] - b[2 * kHandFeat], a[2 * kHandFeat + 1] - b[2 * kHandFeat + 1]);
    return shapes + 0.25f * rel;
}

// Mean of the 3 nearest rows: one stray sample cannot make an unrelated shape match.
float nearest3(CustomKind kind, const std::vector<float>& q, const std::vector<std::vector<float>>& rows) {
    float near[3] = {1e9f, 1e9f, 1e9f};
    for (const auto& r : rows) {
        float d = rowDistance(kind, q, r);
        for (float& n : near)
            if (d < n) std::swap(d, n);
    }
    const size_t k = std::min<size_t>(3, rows.size());
    if (k == 0) return 1e9f;
    float s = 0.f;
    for (size_t i = 0; i < k; ++i) s += near[i];
    return s / static_cast<float>(k);
}

float percentile(std::vector<float> v, float p) {
    if (v.empty()) return 0.f;
    const size_t i = std::min(v.size() - 1, static_cast<size_t>(p * static_cast<float>(v.size() - 1) + 0.5f));
    std::nth_element(v.begin(), v.begin() + static_cast<long>(i), v.end());
    return v[i];
}

// Direction reversals of a 1D signal with hysteresis `h`; also returns the times
// and positions of the turning points.
int reversals(const std::vector<float>& s, float h, std::vector<size_t>* turns = nullptr) {
    if (s.empty() || h <= 0.f) return 0;
    int dir = 0, rev = 0;
    float ext = s.front();
    size_t extI = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const float v = s[i];
        if (dir == 0) {
            if (v - ext > h) { dir = 1; ext = v; extI = i; }
            else if (ext - v > h) { dir = -1; ext = v; extI = i; }
        } else if (dir == 1) {
            if (v > ext) { ext = v; extI = i; }
            else if (ext - v > h) { if (turns) turns->push_back(extI); dir = -1; ++rev; ext = v; extI = i; }
        } else {
            if (v < ext) { ext = v; extI = i; }
            else if (v - ext > h) { if (turns) turns->push_back(extI); dir = 1; ++rev; ext = v; extI = i; }
        }
    }
    return rev;
}

float axisValue(MotionAxis axis, Vec2 c, float palmPx, float refPalm) {
    switch (axis) {
        case MotionAxis::X: return c.x / refPalm;
        case MotionAxis::Y: return c.y / refPalm;
        case MotionAxis::Depth: return std::log(std::max(1.f, palmPx)) * 4.f;  // 25 % closer ~ +0.9
    }
    return 0.f;
}

const wchar_t* axisNameRu(MotionAxis a) {
    switch (a) {
        case MotionAxis::X: return L"влево-вправо";
        case MotionAxis::Y: return L"вверх-вниз";
        case MotionAxis::Depth: return L"к камере и обратно";
    }
    return L"";
}

std::string utf8(const std::wstring& w) {  // tiny UTF-16 -> UTF-8 for BMP text
    std::string s;
    for (wchar_t c : w) {
        const unsigned u = static_cast<unsigned>(c);
        if (u < 0x80) s.push_back(static_cast<char>(u));
        else if (u < 0x800) { s.push_back(static_cast<char>(0xC0 | (u >> 6))); s.push_back(static_cast<char>(0x80 | (u & 0x3F))); }
        else { s.push_back(static_cast<char>(0xE0 | (u >> 12))); s.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F))); s.push_back(static_cast<char>(0x80 | (u & 0x3F))); }
    }
    return s;
}

}  // namespace

std::vector<std::array<float, 42>> condenseSamples(const std::vector<std::array<float, 42>>& raw, size_t maxSamples) {
    return condenseImpl(raw, maxSamples, [](const auto& a, const auto& b) { return templateDistance(a, b); }, 0.015f);
}

std::vector<std::vector<float>> condenseRows(const std::vector<std::vector<float>>& raw, size_t maxSamples) {
    return condenseImpl(raw, maxSamples, [](const auto& a, const auto& b) { return featureDistance(a, b); }, 0.015f);
}

std::vector<float> handFeatures(const HandLandmarks& lm, bool isRight, bool mirrorToRight) {
    const auto& w = lm.world;
    const Vec3 o = w[WRIST];
    const Vec3 up = w[MIDDLE_MCP] - o;
    const float L = std::max(1e-4f, length(up));
    const Vec3 ax = normalized(w[PINKY_MCP] - w[INDEX_MCP]);
    const Vec3 ay = normalized(up - ax * dot(up, ax));
    const Vec3 az = cross(ax, ay);
    // Mirroring a hand (x -> -x) keeps its local x and y coordinates and negates z.
    const bool flip = mirrorToRight && !isRight;
    std::vector<float> f;
    f.reserve(kHandFeat);
    for (int i = 0; i < kHandLandmarks; ++i) {
        const Vec3 p = (w[i] - o) * (1.f / L);
        f.push_back(dot(p, ax));
        f.push_back(dot(p, ay));
        f.push_back(flip ? -dot(p, az) : dot(p, az));
    }
    // Orientation towards the camera: finger direction and palm normal (of the mirrored
    // hand when flipping), weighted below the shape.
    constexpr float kW = 0.6f;
    const Vec3 d = flip ? Vec3{-ay.x, ay.y, ay.z} : ay;
    const Vec3 n = flip ? Vec3{az.x, -az.y, -az.z} : az;
    for (float v : {d.x, d.y, d.z, n.x, n.y, n.z}) f.push_back(v * kW);
    return f;
}

float featureDistance(const std::vector<float>& a, const std::vector<float>& b, size_t n) {
    const size_t m = std::min(a.size(), b.size());
    return rms(a.data(), b.data(), n == 0 ? m : std::min(n, m));
}

Vec2 palmCentre(const HandObservation& h) {
    const auto& im = h.lm.image;
    return (im[WRIST].xy() + im[INDEX_MCP].xy() + im[MIDDLE_MCP].xy() + im[PINKY_MCP].xy()) * 0.25f;
}

bool learnCustomGesture(const std::vector<RecordedFrame>& frames, CustomGesture& g, std::string* reportRu) {
    auto say = [&](const std::wstring& s) { if (reportRu) *reportRu = utf8(s); };
    const size_t budget = static_cast<size_t>(std::clamp(g.sampleBudget, 16, 128));
    size_t withHand = 0, withTwo = 0;
    for (const auto& fr : frames) {
        withHand += !fr.hands.empty();
        withTwo += fr.hands.size() >= 2;
    }
    if (withHand < 15) {
        say(L"рука почти не была видна — держите её в кадре и запишите ещё раз");
        return false;
    }
    int learnedRev = 0;
    std::wstring quality;
    // Which built-in pose the hand mostly showed (for the "looks like" warning).
    std::unordered_map<std::string, int> poses;
    for (const auto& fr : frames)
        for (const auto& h : fr.hands)
            if (!h.pose.empty()) ++poses[h.pose];

    if (!g.dynamic) {
        // ---------------------------------------------------------------- a sign
        // Only frames where the hand(s) stood still: the moments of raising, adjusting
        // or lowering the hand are not part of the sign.
        auto steady = [](const RecordedFrame& fr) {
            for (const auto& h : fr.hands)
                if (h.speed > 1.2f) return false;
            return !fr.hands.empty();
        };
        size_t steadyCount = 0;
        for (const auto& fr : frames) steadyCount += steady(fr);
        const bool onlySteady = steadyCount >= 15;
        if (!onlySteady)
            quality = L"рука почти всё время двигалась — для такого жеста подойдёт «Динамичный»";
        if (withTwo * 10 >= withHand * 6) {
            std::vector<std::vector<float>> raw;
            for (const auto& fr : frames) {
                if (fr.hands.size() < 2 || (onlySteady && !steady(fr))) continue;
                const auto& l = fr.hands[0];
                const auto& r = fr.hands[1];
                const float palm = std::max(1.f, 0.5f * (l.palmPx + r.palmPx));
                std::vector<float> row = l.feat;
                row.insert(row.end(), r.feat.begin(), r.feat.end());
                const Vec2 rel = (r.centre - l.centre) / palm;
                row.push_back(rel.x);
                row.push_back(rel.y);
                if (row.size() == kTwoRow) raw.push_back(std::move(row));
            }
            g.kind = CustomKind::TwoHands;
            g.rows = condenseImpl(raw, budget,
                                  [](const auto& a, const auto& b) { return rowDistance(CustomKind::TwoHands, a, b); }, 0.015f);
        } else {
            std::vector<std::vector<float>> raw;
            for (const auto& fr : frames) {
                if (fr.hands.empty() || (onlySteady && !steady(fr))) continue;
                const RecordedHand* h = &fr.hands[0];
                for (const auto& x : fr.hands)
                    if (x.palmPx > h->palmPx) h = &x;
                raw.push_back(g.mirrorInvariant ? h->featMirror : h->feat);
            }
            g.kind = CustomKind::OneHand;
            g.rows = condenseRows(raw, budget);
        }
        g.motionAmp = 0.f;
        g.motionPeriodMs = 0.f;
    } else {
        // ---------------------------------------------------------------- a movement
        std::vector<std::vector<float>> raw;
        std::vector<Vec2> centres;
        std::vector<float> palms;
        std::vector<int64_t> times;
        for (const auto& fr : frames) {
            if (fr.hands.empty()) continue;
            const RecordedHand* h = &fr.hands[0];
            for (const auto& x : fr.hands)
                if (x.palmPx > h->palmPx) h = &x;
            raw.push_back(g.mirrorInvariant ? h->featMirror : h->feat);
            centres.push_back(h->centre);
            palms.push_back(h->palmPx);
            times.push_back(fr.t);
        }
        const float refPalm = std::max(1.f, percentile(palms, 0.5f));
        // The axis that moved most.
        MotionAxis bestAxis = MotionAxis::X;
        float bestAmp = 0.f;
        std::vector<float> best, bestRaw;  // smoothed (turns, size) and raw (speeds)
        for (MotionAxis ax : {MotionAxis::X, MotionAxis::Y, MotionAxis::Depth}) {
            std::vector<float> s(centres.size());
            for (size_t i = 0; i < s.size(); ++i) s[i] = axisValue(ax, centres[i], palms[i], refPalm);
            std::vector<float> sm(s.size());  // 3-tap smoothing against landmark jitter
            for (size_t i = 0; i < s.size(); ++i)
                sm[i] = (s[i > 0 ? i - 1 : i] + s[i] + s[i + 1 < s.size() ? i + 1 : i]) / 3.f;
            const float amp = percentile(sm, 0.95f) - percentile(sm, 0.05f);
            if (amp > bestAmp) { bestAmp = amp; bestAxis = ax; best = sm; bestRaw = s; }
        }
        if (bestAmp < 0.15f) {
            say(L"движения почти не было — повторяйте его всю запись или выберите «Статичный»");
            return false;
        }
        // Velocity per frame along the axis (gaps where the hand left the frame are skipped).
        float fastPos = 0.f, fastNeg = 0.f;
        std::vector<float> speeds;
        for (size_t i = 1; i < best.size(); ++i) {
            const float dt = static_cast<float>(times[i] - times[i - 1]) / 1000.f;
            if (dt <= 0.f || dt > 0.3f) continue;
            speeds.push_back(std::fabs(best[i] - best[i - 1]) / dt);
        }
        const float fast = 0.5f * percentile(speeds, 0.9f);
        std::vector<float> strokeTimes, strokeTravel;
        int dirOfRun = 0;
        float runTravel = 0.f, runTime = 0.f;
        auto closeRun = [&] {
            if (dirOfRun != 0 && runTravel > 0.25f * bestAmp) {
                (dirOfRun > 0 ? fastPos : fastNeg) += runTravel;
                strokeTravel.push_back(runTravel);
                strokeTimes.push_back(runTime);
            }
            dirOfRun = 0;
            runTravel = runTime = 0.f;
        };
        for (size_t i = 1; i < best.size(); ++i) {
            const float dt = static_cast<float>(times[i] - times[i - 1]) / 1000.f;
            if (dt <= 0.f || dt > 0.3f) { closeRun(); continue; }
            const float v = (best[i] - best[i - 1]) / dt;
            const int d = std::fabs(v) >= fast ? (v > 0 ? 1 : -1) : 0;
            if (d != dirOfRun) closeRun();
            if (d != 0) { dirOfRun = d; runTravel += std::fabs(best[i] - best[i - 1]); runTime += dt; }
        }
        closeRun();
        std::vector<size_t> turns;
        const int rev = reversals(best, 0.3f * bestAmp, &turns);
        learnedRev = rev;
        // Mean speed of each half-stroke between turning points, per direction. Waving /
        // shaking goes both ways at a similar speed; a stroke is quick one way and slow (or
        // out of view) on the way back.
        std::vector<float> spd[2], amp[2], dur[2];  // [0] negative direction, [1] positive
        for (size_t k = 1; k < turns.size(); ++k) {
            const size_t ia = turns[k - 1], ib = turns[k];
            const float dt = static_cast<float>(times[ib] - times[ia]) / 1000.f;
            if (dt <= 0.f || dt > 3.f) continue;
            // Raw values: smoothing shifts the peaks and hides how quick the quick part was.
            float hiV = bestRaw[ia], loV = bestRaw[ia];
            for (size_t i = ia; i <= ib; ++i) { hiV = std::max(hiV, bestRaw[i]); loV = std::min(loV, bestRaw[i]); }
            const float dv = best[ib] > best[ia] ? hiV - loV : loV - hiV;
            const int d = dv > 0.f ? 1 : 0;
            spd[d].push_back(std::fabs(dv) / dt);
            amp[d].push_back(std::fabs(dv));
            dur[d].push_back(dt);
        }
        g.axis = bestAxis;
        g.motionAmp = bestAmp;
        float vNeg = spd[0].empty() ? 0.f : percentile(spd[0], 0.5f);
        float vPos = spd[1].empty() ? 0.f : percentile(spd[1], 0.5f);
        if (spd[0].empty() && spd[1].empty()) { vPos = fastPos; vNeg = fastNeg; }  // no clean turns: use fast runs
        const float lo = std::min(vPos, vNeg), hi = std::max(vPos, vNeg);
        if (rev >= 2 && lo > 0.f && hi < 1.6f * lo) {
            g.kind = CustomKind::Motion;
            g.motionPeriodMs = 2.f * static_cast<float>(times.back() - times.front()) / static_cast<float>(rev);
        } else {
            g.kind = CustomKind::Stroke;
            const int d = vPos >= vNeg ? 1 : 0;
            g.motionDir = d ? 1 : -1;
            if (!amp[d].empty()) {
                g.motionAmp = percentile(amp[d], 0.5f);
                g.motionPeriodMs = 1000.f * percentile(dur[d], 0.5f);
                learnedRev = static_cast<int>(amp[d].size());
            } else {
                std::vector<float> tr = strokeTravel, tm = strokeTimes;
                g.motionAmp = tr.empty() ? bestAmp : percentile(tr, 0.5f);
                g.motionPeriodMs = tm.empty() ? 400.f : 1000.f * percentile(tm, 0.5f);
                learnedRev = static_cast<int>(tr.size());
            }
            if (learnedRev < 2) quality = L"движение повторено мало раз — сделайте 3–5 повторов для надёжности";
        }        g.rows = condenseRows(raw, budget);
    }
    if (g.rows.empty()) {
        say(L"слишком мало чётких кадров — держите руку целиком в кадре и запишите ещё раз");
        return false;
    }
    // ---- threshold from the recording's own consistency: how far each sample is from
    // its 3 nearest neighbours (90th percentile), with a floor and a ceiling.
    std::vector<float> spread;  // a perfectly steady recording collapses to a few rows: floor applies
    for (size_t i = 0; g.rows.size() >= 4 && i < g.rows.size(); ++i) {
        std::vector<std::vector<float>> others;
        for (size_t j = 0; j < g.rows.size(); ++j)
            if (j != i) others.push_back(g.rows[j]);
        spread.push_back(nearest3(g.kind, g.rows[i], others));
    }
    const float sp = percentile(spread, 0.9f);
    g.threshold = std::clamp(2.5f * sp + 0.03f, 0.10f, 0.32f);
    if (sp > 0.11f && quality.empty())
        quality = L"поза заметно менялась во время записи — жест будет узнаваться мягче; для точности держите форму ровнее";
    g.samples.clear();  // a new recording replaces a legacy 2D one
    std::wstring rep;
    switch (g.kind) {
        case CustomKind::OneHand: rep = L"знак одной рукой"; break;
        case CustomKind::TwoHands: rep = L"знак двумя руками — сработает, только когда в кадре обе"; break;
        case CustomKind::Motion:
            rep = std::wstring(L"движение туда-обратно ") + axisNameRu(g.axis) + L" (~" + std::to_wstring(std::max(1, learnedRev / 2)) +
                  L" повторов)";
            break;
        case CustomKind::Stroke: {
            const wchar_t* dir = g.axis == MotionAxis::X ? (g.motionDir > 0 ? L"вправо" : L"влево")
                                 : g.axis == MotionAxis::Y ? (g.motionDir > 0 ? L"вниз" : L"вверх")
                                                           : (g.motionDir > 0 ? L"к камере" : L"от камеры");
            rep = std::wstring(L"рывок ") + dir + L" (" + std::to_wstring(learnedRev) + L" повторов)";
            break;
        }
    }
    rep += L", " + std::to_wstring(g.rows.size()) + L" образцов";
    // "Looks like a built-in pose": worth knowing if a rule also uses that pose.
    std::string topPose;
    int topCount = 0;
    for (const auto& [id, n] : poses)
        if (n > topCount) { topCount = n; topPose = id; }
    g.lookalike = topCount * 10 >= static_cast<int>(withHand) * 7 ? topPose : std::string();
    std::string out = utf8(rep);
    if (const auto* b = g.lookalike.empty() ? nullptr : findBuiltin(g.lookalike))
        out += std::string("; поза совпадает со встроенной «") + b->nameRu + "»";
    if (!quality.empty()) out += ". " + utf8(quality);
    if (reportRu) *reportRu = out;    return true;
}

float gestureConfusion(const CustomGesture& a, const CustomGesture& b) {
    if (a.rows.empty() || b.rows.empty()) return 1e9f;
    const bool twoA = a.kind == CustomKind::TwoHands, twoB = b.kind == CustomKind::TwoHands;
    if (twoA != twoB) return 1e9f;  // one hand vs two hands never collide
    float sum = 0.f;
    for (const auto& r : a.rows) sum += nearest3(twoA ? CustomKind::TwoHands : CustomKind::OneHand, r, b.rows);
    const float typical = sum / static_cast<float>(a.rows.size());
    return typical / std::max(0.02f, std::max(a.threshold * a.strictness, b.threshold * b.strictness));
}

std::string CustomMatcher::update(std::vector<HandObservation>& hands, const std::vector<CustomGesture>& gestures,
                                  int64_t nowMs) {
    for (auto& h : hands) {
        h.customId.clear();
        h.customConfidence = 0.f;
    }
    auto offer = [](HandObservation& h, const std::string& id, float conf) {
        if (conf > h.customConfidence) {
            h.customConfidence = conf;
            h.customId = id;
        }
    };
    HandObservation* left = nullptr;
    HandObservation* right = nullptr;
    if (hands.size() >= 2) {
        left = &hands[0];
        right = &hands[1];
        if (palmCentre(*left).x > palmCentre(*right).x) std::swap(left, right);
    }
    std::string fired;
    for (const auto& g : gestures) {
        if (!g.recorded()) continue;
        if (g.legacy()) {  // v1-v3 2D outline of one hand
            std::vector<CustomGesture> one{g};
            for (auto& h : hands) {
                if (h.busy) continue;
                HandObservation tmp = h;
                matchCustom(tmp, one);
                if (!tmp.customId.empty()) offer(h, g.id, tmp.customConfidence);
            }
            continue;
        }
        const float thr = std::max(0.02f, g.threshold * g.strictness);
        if (g.kind == CustomKind::OneHand) {
            for (auto& h : hands) {
                if (h.busy || h.feat.empty()) continue;
                const float d = nearest3(g.kind, g.mirrorInvariant ? h.featMirror : h.feat, g.rows);
                if (d <= thr) offer(h, g.id, 1.f - d / thr);
            }
        } else if (g.kind == CustomKind::TwoHands) {
            if (!left || left->feat.empty() || right->feat.empty()) continue;  // never with one hand
            std::vector<float> q = left->feat;
            q.insert(q.end(), right->feat.begin(), right->feat.end());
            const float palm = std::max(1.f, 0.5f * (left->palmPx + right->palmPx));
            const Vec2 rel = (palmCentre(*right) - palmCentre(*left)) / palm;
            q.push_back(rel.x);
            q.push_back(rel.y);
            const float d = nearest3(g.kind, q, g.rows);
            if (d <= thr) {
                offer(*left, g.id, 1.f - d / thr);
                offer(*right, g.id, 1.f - d / thr);
            }
        } else {  // Motion / Stroke: the shape AND the recorded movement
            MotionState& ms = motion_[g.id];
            const bool stroke = g.kind == CustomKind::Stroke;
            const HandObservation* best = nullptr;
            float bestD = 1e9f;
            for (const auto& h : hands) {
                if (h.feat.empty()) continue;
                const float d = nearest3(CustomKind::OneHand, g.mirrorInvariant ? h.featMirror : h.feat, g.rows);
                if (d < bestD) { bestD = d; best = &h; }
            }
            const int64_t window = stroke ? static_cast<int64_t>(std::clamp(2.5f * g.motionPeriodMs, 500.f, 1500.f))
                                          : static_cast<int64_t>(std::clamp(1.8f * g.motionPeriodMs, 1500.f, 4000.f));
            while (!ms.signal.empty() && nowMs - ms.signal.front().first > window) ms.signal.pop_front();
            while (!ms.pose.empty() && nowMs - ms.pose.front().first > window) ms.pose.pop_front();
            const int64_t cooldown = static_cast<int64_t>(stroke ? std::max(500.f, g.motionPeriodMs) : std::max(800.f, 0.8f * g.motionPeriodMs));
            int poseOk = 0;
            for (const auto& p : ms.pose) poseOk += p.second;
            // Stroke: travel in the learned direction from the furthest point back, recently.
            // Only the last ~1.6 stroke durations count, and the movement must be about as
            // quick as recorded: a slow drift of the hand is not a stroke.
            auto strokeTravel = [&]() {
                if (ms.signal.size() < 2) return 0.f;
                const auto [tLast, last] = ms.signal.back();
                const int64_t limit = static_cast<int64_t>(std::max(300.f, 1.6f * g.motionPeriodMs));
                float startV = last;
                int64_t startT = tLast;
                for (auto it = ms.signal.rbegin(); it != ms.signal.rend() && tLast - it->first <= limit; ++it)
                    if ((it->second - startV) * static_cast<float>(g.motionDir) < 0.f) { startV = it->second; startT = it->first; }
                const float travel = (last - startV) * static_cast<float>(g.motionDir);
                const float dt = std::max(0.05f, static_cast<float>(tLast - startT) / 1000.f);
                const float learnedSpeed = g.motionAmp / std::max(0.1f, g.motionPeriodMs / 1000.f);
                return travel / dt >= 0.4f * learnedSpeed ? travel : 0.f;
            };
            auto fire = [&] {
                ms.lastFire = nowMs;
                ms.signal.clear();
                ms.pose.clear();
                ms.progress = 0.f;
                if (fired.empty()) fired = "custom:" + g.id;
            };
            if (!best) {
                // The hand left the frame: a stroke that carried it out still counts.
                if (stroke && poseOk * 100 >= static_cast<int>(ms.pose.size()) * 35 && !ms.pose.empty() &&
                    strokeTravel() >= 0.45f * g.motionAmp && nowMs - ms.lastFire >= cooldown)
                    fire();
                ms.signal.clear();
                ms.pose.clear();
                ms.progress = 0.f;
                continue;
            }
            // X / Y in palm lengths of a slowly adapting reference size, so moving towards
            // the camera does not read as sideways movement.
            ms.refPalm = ms.refPalm > 0.f ? 0.9f * ms.refPalm + 0.1f * best->palmPx : std::max(1.f, best->palmPx);
            ms.signal.push_back({nowMs, axisValue(g.axis, palmCentre(*best), best->palmPx, ms.refPalm)});
            const bool shapeNow = bestD <= thr * 1.6f;  // motion blur distorts the hand: looser
            ms.pose.push_back({nowMs, shapeNow});
            poseOk += shapeNow;
            if (stroke) {
                const float travel = strokeTravel();
                const bool shapeOk = poseOk * 100 >= static_cast<int>(ms.pose.size()) * 35;  // shape in >= 35 % of frames
                ms.progress = shapeOk ? std::min(1.f, travel / std::max(0.05f, 0.6f * g.motionAmp)) : 0.f;
                if (shapeOk && travel >= 0.6f * g.motionAmp && nowMs - ms.lastFire >= cooldown) fire();
                continue;
            }
            std::vector<float> s;
            for (const auto& p : ms.signal) s.push_back(p.second);
            const float range = s.empty() ? 0.f : *std::max_element(s.begin(), s.end()) - *std::min_element(s.begin(), s.end());
            const int rev = reversals(s, 0.3f * g.motionAmp);
            const bool shapeOk = poseOk * 10 >= static_cast<int>(ms.pose.size()) * 4;  // >= 40 %: the movement is the main cue
            ms.progress = shapeOk ? std::min(1.f, rev / 2.f) : 0.f;
            if (shapeOk && rev >= 2 && range >= 0.5f * g.motionAmp && nowMs - ms.lastFire >= cooldown) fire();
        }
    }
    return fired;
}

float CustomMatcher::motionProgress(const std::string& id) const {
    auto it = motion_.find(id);
    return it == motion_.end() ? 0.f : it->second.progress;
}

std::string MotionDetector::stroke(int64_t nowMs, float minTravel, Vec2* axis, float* palmOut) const {
    (void)nowMs;
    if (trail_.size() < 3) return {};
    // Open hand for most of the stroke: blurred frames mid-swipe often read as a fist.
    int open = 0;
    float palm = 0.f;
    for (const auto& p : trail_) { open += p.open; palm += p.palm; }
    if (open * 3 < static_cast<int>(trail_.size())) return {};  // open in >= 1/3 of the frames
    palm /= static_cast<float>(trail_.size());
    const Vec2 d = (trail_.back().c - trail_.front().c) / palm;
    const bool horiz = std::fabs(d.x) >= std::fabs(d.y);
    const float along = horiz ? d.x : d.y, across = horiz ? d.y : d.x;
    if (std::fabs(along) < minTravel || std::fabs(along) < 1.5f * std::fabs(across)) return {};
    // Mostly one-way: steps against the stroke may add up to at most a quarter of it.
    float back = 0.f;
    for (size_t i = 1; i < trail_.size(); ++i) {
        const Vec2 s = (trail_[i].c - trail_[i - 1].c) / palm;
        const float m = horiz ? s.x : s.y;
        if (m * along < 0.f) back += std::fabs(m);
    }
    if (back > 0.25f * std::fabs(along)) return {};
    if (axis) *axis = horiz ? Vec2{along > 0 ? 1.f : -1.f, 0.f} : Vec2{0.f, along > 0 ? 1.f : -1.f};
    if (palmOut) *palmOut = palm;
    return horiz ? (along > 0 ? "swipe_right" : "swipe_left") : (along > 0 ? "swipe_down" : "swipe_up");
}

std::string MotionDetector::wave(int64_t nowMs) {
    if (nowMs - lastFire_ < 1000 || wave_.size() < 6) return {};
    float palm = 0.f, minY = 1e9f, maxY = -1e9f, minX = 1e9f, maxX = -1e9f;
    int open = 0;
    std::vector<float> xs;
    for (const auto& p : wave_) {
        palm += p.palm;
        open += p.open;
        minX = std::min(minX, p.c.x); maxX = std::max(maxX, p.c.x);
        minY = std::min(minY, p.c.y); maxY = std::max(maxY, p.c.y);
    }
    palm /= static_cast<float>(wave_.size());
    if (open * 10 < static_cast<int>(wave_.size()) * 7) return {};  // open hand >= 70 % of the time
    if (maxY - minY > maxX - minX) return {};                          // side to side, not up and down
    for (const auto& p : wave_) xs.push_back(p.c.x / palm);
    std::vector<size_t> turns;
    if (reversals(xs, 0.5f, &turns) < 3 || turns.size() < 3) return {};
    // Similar speed both ways. Repeated swipes are a fast stroke and a slow return.
    float fast[2] = {0.f, 0.f};
    int cnt[2] = {0, 0};
    for (size_t k = 1; k < turns.size(); ++k) {
        const auto& a = wave_[turns[k - 1]];
        const auto& b = wave_[turns[k]];
        const float dt = std::max(1.f, static_cast<float>(b.t - a.t));
        const float v = std::fabs(b.c.x - a.c.x) / palm / dt;
        const int dir = b.c.x > a.c.x ? 1 : 0;
        fast[dir] += v;
        ++cnt[dir];
    }
    if (cnt[0] && cnt[1]) {
        const float v0 = fast[0] / cnt[0], v1 = fast[1] / cnt[1];
        if (std::max(v0, v1) > 2.2f * std::max(1e-6f, std::min(v0, v1))) return {};
    }
    lastFire_ = nowMs;
    lastAxis_ = {};
    trail_.clear();
    wave_.clear();
    pending_.clear();
    return "wave";
}

std::string MotionDetector::fire(std::string g, Vec2 axis, int64_t nowMs) {
    lastFire_ = nowMs;
    lastAxis_ = axis;
    trail_.clear();
    pending_.clear();
    return g;
}

std::string MotionDetector::update(const HandObservation& h, int64_t nowMs) {
    const Vec2 c = palmCentre(h);
    const P p{c, std::max(1.f, h.palmPx), nowMs, h.fingers.count() >= 3};
    trail_.push_back(p);
    while (!trail_.empty() && nowMs - trail_.front().t > 500) trail_.pop_front();
    wave_.push_back(p);
    while (!wave_.empty() && nowMs - wave_.front().t > 1600) wave_.pop_front();

    if (auto w = wave(nowMs); !w.empty()) return w;
    if (!pending_.empty()) {
        const Vec2 moved = (c - pendingAt_) / pendingPalm_;
        const float along = moved.x * pendingAxis_.x + moved.y * pendingAxis_.y;
        if (along < -0.6f) {
            pending_.clear();  // came straight back: a wave / back-and-forth in progress
        } else if (nowMs - pendingT_ >= 180) {
            return fire(pending_, pendingAxis_, nowMs);
        } else if (along > 0.f) {
            pendingAt_ = c;  // still travelling: follow the hand
        }
        return {};
    }
    if (nowMs - lastFire_ < 350) return {};
    Vec2 axis{};
    float palm = 1.f;
    auto s = stroke(nowMs, 1.5f, &axis, &palm);
    if (s.empty()) return {};
    // The hand travelling back after a swipe to start the next one is not a swipe.
    if (nowMs - lastFire_ < 900 && axis.x * lastAxis_.x + axis.y * lastAxis_.y < -0.5f) {
        trail_.clear();
        trail_.push_back(p);
        return {};
    }
    if (!oscillationAware_) return fire(std::move(s), axis, nowMs);
    pending_ = std::move(s);
    pendingT_ = nowMs;
    pendingAt_ = c;
    pendingAxis_ = axis;
    pendingPalm_ = palm;
    return {};
}

std::string MotionDetector::lost(int64_t nowMs) {
    std::string g;
    Vec2 axis{};
    if (!pending_.empty()) {
        g.swap(pending_);
        axis = pendingAxis_;
    } else if (nowMs - lastFire_ >= 350) {
        g = stroke(nowMs, 1.2f, &axis, nullptr);  // the stroke carried the hand out of view
        if (!g.empty() && nowMs - lastFire_ < 900 && axis.x * lastAxis_.x + axis.y * lastAxis_.y < -0.5f) g.clear();
    }
    if (!g.empty()) {
        lastFire_ = nowMs;
        lastAxis_ = axis;
    }
    trail_.clear();
    wave_.clear();
    pending_.clear();
    return g;
}

std::string TwoHandDetector::staticPose(const HandObservation* a, const HandObservation* b) const {
    if (!a || !b) return {};
    if (a->pose == "open_palm" && b->pose == "open_palm") return "two_palms";
    if (a->pose == "fist" && b->pose == "fist") return "two_fists";
    return {};
}

std::string TwoHandDetector::update(const HandObservation* a, const HandObservation* b, int64_t nowMs) {
    if (!a || !b) {
        trail_.clear();
        return {};
    }
    const float palm = std::max(1.f, (a->palmPx + b->palmPx) * 0.5f);
    trail_.push_back({length(palmCentre(*a) - palmCentre(*b)) / palm, nowMs});
    while (!trail_.empty() && nowMs - trail_.front().t > 700) trail_.pop_front();
    if (nowMs - lastFire_ < 900 || trail_.size() < 3) return {};
    float mn = 1e9f, mx = -1e9f;
    for (const auto& p : trail_) { mn = std::min(mn, p.d); mx = std::max(mx, p.d); }
    const float d = trail_.back().d;
    std::string g;
    if (d - mn >= 1.5f && d >= mn * 1.35f) g = "two_spread";
    else if (mx - d >= 1.5f && mx >= d * 1.35f) g = "two_pinch";
    if (!g.empty()) {
        lastFire_ = nowMs;
        trail_.clear();
    }
    return g;
}
bool GestureEngine::handMatches(const GestureRule& r, const HandObservation& h, std::string* whyNot) {
    auto no = [&](const char* m) {
        if (whyNot) *whyNot = m;
        return false;
    };
    const bool custom = r.gesture.rfind("custom:", 0) == 0;
    const auto* b = custom ? nullptr : findBuiltin(r.gesture);
    if (custom) {
        if (h.customId != r.gesture.substr(7)) return no("другой жест");
    } else if (b) {
        if (b->twoHands) return no("нужны две руки");
        if (!b->dynamic && h.pose != r.gesture) return no("другой жест");
    }
    if (r.hand == HandSide::Left && h.isRight) return no("нужна левая рука");
    if (r.hand == HandSide::Right && !h.isRight) return no("нужна правая рука");
    // Static gestures are meant to be shown: a clearly visible hand, held still, not
    // busy next to the other hand (holding a phone, typing).
    if (custom || (b && !b->dynamic)) {
        if (!h.reliable()) return no("рука видна нечётко");
        if (!h.steady) return no("рука в движении");
        if (!custom && h.busy) return no("руки заняты");
    }
    return true;
}

std::vector<GestureEvent> GestureEngine::update(const std::vector<HandObservation>& hands,
                                                const std::string& dynamicGesture, const std::string& twoHandPose,
                                                const FaceContext& faces, const std::vector<GestureRule>& rules,
                                                int64_t nowMs) {
    std::vector<GestureEvent> events;
    constexpr int64_t kGraceMs = 160;  // tolerate one or two dropped / misclassified frames

    for (const auto& r : rules) {
        auto& st = state_[r.id];
        if (!r.enabled) {
            st = RuleState{};
            continue;
        }
        bool personOk = !r.requireFace || faces.anyFace;
        if (r.person == PersonFilter::AnyKnown) personOk = personOk && !faces.people.empty();
        if (r.person == PersonFilter::Specific)
            personOk = personOk && std::find(faces.people.begin(), faces.people.end(), r.personId) != faces.people.end();
        if (r.requireLooking) personOk = personOk && faces.looking;

        const auto* b = findBuiltin(r.gesture);
        const bool dynamic = b && b->dynamic;

        auto fire = [&] {
            GestureEvent e;
            e.ruleId = r.id;
            e.ruleName = r.name;
            e.action = r.action;
            e.gesture = r.gesture;
            events.push_back(std::move(e));
            st.lastFired = nowMs;
        };

        // A custom gesture with movement arrives like a built-in movement.
        if (r.gesture.rfind("custom:", 0) == 0 && dynamicGesture == r.gesture) {
            st.progress = 0.f;
            if (personOk && nowMs - st.lastFired >= r.cooldownMs) fire();
            continue;
        }

        const HandObservation* match = nullptr;
        if (b && b->twoHands) {
            // Two-hand gestures: both hands visible, and for the static ones the pair pose matches.
            if (hands.size() >= 2 && (dynamic || twoHandPose == r.gesture)) match = &hands[0];
        } else {
            for (const auto& h : hands)
                if (handMatches(r, h)) { match = &h; break; }
        }

        if (dynamic) {
            st.progress = 0.f;
            if (personOk && match && dynamicGesture == r.gesture && nowMs - st.lastFired >= r.cooldownMs) fire();
            continue;
        }

        if (personOk && match) {
            if (!st.holding) {
                st.holding = true;
                st.holdStart = nowMs;
            }
            st.lastSeen = nowMs;
            const int64_t held = nowMs - st.holdStart;
            st.progress = r.holdMs > 0 ? std::min(1.f, static_cast<float>(held) / r.holdMs) : 1.f;
            if (held >= r.holdMs) {
                if (!st.firedThisHold) {
                    if (nowMs - st.lastFired >= r.cooldownMs) {
                        fire();
                        st.firedThisHold = true;
                    }
                } else if (r.repeat && nowMs - st.lastFired >= r.repeatMs) {
                    fire();
                }
            }
        } else if (st.holding && nowMs - st.lastSeen > kGraceMs) {
            st.holding = false;
            st.firedThisHold = false;
            st.progress = 0.f;
        }
    }
    return events;
}

float GestureEngine::progress(const std::string& ruleId) const {
    auto it = state_.find(ruleId);
    return it == state_.end() ? 0.f : it->second.progress;
}

}  // namespace vl
