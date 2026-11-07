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
        fs.extended[f] = fs.curl[f] < 0.40f && reaches;
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
            if (fs.extended[f] != want) ok = false;
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
    if (fs.extended[0] && thumbVertical && !fs.extended[1] && !fs.extended[2] && !fs.extended[3] &&
        !fs.extended[4]) {
        h.pose = tdir.y < 0 ? "thumbs_up" : "thumbs_down";
        h.poseConfidence = std::clamp(0.5f * (fs.curl[1] + fs.curl[2] + fs.curl[3] + fs.curl[4]) / 2.f + 0.5f, 0.f, 1.f);
        return;
    }
    if (touching && fs.curl[1] < 0.85f && !(fs.extended[2] && fs.extended[3])) {
        h.pose = "pinch";
        h.poseConfidence = std::clamp(1.f - tipGap / 0.32f, 0.3f, 1.f);
        return;
    }
    for (const auto& d : defs) {
        bool ok;
        const float s = score(d, ok);
        if (!ok) continue;
        if (std::string(d.id) == "open_palm" && !h.palmFacing) continue;
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

std::vector<std::array<float, 42>> condenseSamples(const std::vector<std::array<float, 42>>& raw, size_t maxSamples) {
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
                if (j != i) d.push_back(templateDistance(raw[i], raw[j]));
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
    std::vector<std::array<float, 42>> out;
    for (size_t i : keep) {
        bool dup = false;
        for (const auto& o : out)
            if (templateDistance(raw[i], o) < 0.015f) { dup = true; break; }
        if (!dup) out.push_back(raw[i]);
    }
    // 3. Spread the remainder evenly over the recording.
    if (out.size() > maxSamples) {
        std::vector<std::array<float, 42>> thin;
        for (size_t i = 0; i < maxSamples; ++i) thin.push_back(out[i * out.size() / maxSamples]);
        out.swap(thin);
    }
    return out;
}

namespace {
Vec2 palmCentre(const HandObservation& h) {
    const auto& im = h.lm.image;
    return (im[WRIST].xy() + im[INDEX_MCP].xy() + im[MIDDLE_MCP].xy() + im[PINKY_MCP].xy()) * 0.25f;
}
}  // namespace

std::string MotionDetector::stroke(int64_t nowMs, float minTravel, Vec2* axis, float* palmOut) const {
    if (nowMs - lastFire_ < 600 || trail_.size() < 3) return {};
    // Open hand for most of the stroke: blurred frames mid-swipe often read as a fist.
    int open = 0;
    float palm = 0.f;
    for (const auto& p : trail_) { open += p.open; palm += p.palm; }
    if (open * 2 < static_cast<int>(trail_.size())) return {};
    palm /= static_cast<float>(trail_.size());
    const Vec2 d = (trail_.back().c - trail_.front().c) / palm;
    const bool horiz = std::fabs(d.x) >= std::fabs(d.y);
    const float along = horiz ? d.x : d.y, across = horiz ? d.y : d.x;
    if (std::fabs(along) < minTravel || std::fabs(along) < 1.7f * std::fabs(across)) return {};
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
    for (const auto& p : wave_) {
        palm += p.palm;
        open += p.open;
        minX = std::min(minX, p.c.x); maxX = std::max(maxX, p.c.x);
        minY = std::min(minY, p.c.y); maxY = std::max(maxY, p.c.y);
    }
    palm /= static_cast<float>(wave_.size());
    if (open * 10 < static_cast<int>(wave_.size()) * 7) return {};  // open hand >= 70 % of the time
    if (maxY - minY > maxX - minX) return {};                          // side to side, not up and down
    // Count direction reversals along x with 0.5 palm-length hysteresis.
    int dir = 0, rev = 0;
    float ext = wave_.front().c.x;
    for (const auto& q : wave_) {
        const float dx = (q.c.x - ext) / palm;
        if (dir == 0) {
            if (dx > 0.5f) { dir = 1; ext = q.c.x; }
            else if (dx < -0.5f) { dir = -1; ext = q.c.x; }
        } else if (dir == 1) {
            if (q.c.x > ext) ext = q.c.x;
            else if (dx < -0.5f) { dir = -1; ++rev; ext = q.c.x; }
        } else {
            if (q.c.x < ext) ext = q.c.x;
            else if (dx > 0.5f) { dir = 1; ++rev; ext = q.c.x; }
        }
    }
    if (rev < 3) return {};
    lastFire_ = nowMs;
    trail_.clear();
    wave_.clear();
    pending_.clear();
    return "wave";
}

std::string MotionDetector::update(const HandObservation& h, int64_t nowMs) {
    const Vec2 c = palmCentre(h);
    const P p{c, std::max(1.f, h.palmPx), nowMs, h.fingers.count() >= 3};
    trail_.push_back(p);
    while (!trail_.empty() && nowMs - trail_.front().t > 500) trail_.pop_front();
    wave_.push_back(p);
    while (!wave_.empty() && nowMs - wave_.front().t > 1500) wave_.pop_front();

    if (auto w = wave(nowMs); !w.empty()) return w;
    if (!pending_.empty()) {
        const Vec2 moved = (c - pendingAt_) / pendingPalm_;
        const float along = moved.x * pendingAxis_.x + moved.y * pendingAxis_.y;
        if (along < -0.6f) {
            pending_.clear();  // came straight back: a wave in progress
        } else if (nowMs - pendingT_ >= 180) {
            std::string g;
            g.swap(pending_);
            lastFire_ = nowMs;
            trail_.clear();
            return g;
        } else {
            pendingAt_ = along > 0.f ? c : pendingAt_;  // still travelling: follow the hand
        }
        return {};
    }
    Vec2 axis{};
    float palm = 1.f;
    if (auto s = stroke(nowMs, 1.7f, &axis, &palm); !s.empty()) {
        pending_ = std::move(s);
        pendingT_ = nowMs;
        pendingAt_ = c;
        pendingAxis_ = axis;
        pendingPalm_ = palm;
    }
    return {};
}

std::string MotionDetector::lost(int64_t nowMs) {
    std::string g;
    if (!pending_.empty()) g.swap(pending_);
    else g = stroke(nowMs, 1.3f, nullptr, nullptr);  // the stroke carried the hand out of view
    if (!g.empty()) lastFire_ = nowMs;
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
    if (r.gesture.rfind("custom:", 0) == 0) {
        if (h.customId != r.gesture.substr(7)) return no("другой жест");
    } else if (const auto* b = findBuiltin(r.gesture)) {
        if (b->twoHands) return no("нужны две руки");
        if (!b->dynamic && h.pose != r.gesture) return no("другой жест");
    }
    if (r.hand == HandSide::Left && h.isRight) return no("нужна левая рука");
    if (r.hand == HandSide::Right && !h.isRight) return no("нужна правая рука");
    return true;
}

std::vector<GestureEvent> GestureEngine::update(const std::vector<HandObservation>& hands,
                                                const std::string& dynamicGesture, const std::string& twoHandPose,
                                                const FaceContext& faces,
                                                const std::vector<GestureRule>& rules, FaceGate gate,
                                                int64_t nowMs) {
    std::vector<GestureEvent> events;
    const bool gateOk = gate == FaceGate::Off || (gate == FaceGate::AnyFace && faces.anyFace) ||
                        (gate == FaceGate::KnownOnly && !faces.people.empty());
    constexpr int64_t kGraceMs = 160;  // tolerate one or two dropped / misclassified frames

    for (const auto& r : rules) {
        auto& st = state_[r.id];
        if (!r.enabled) {
            st = RuleState{};
            continue;
        }
        bool personOk = gateOk;
        if (r.person == PersonFilter::AnyKnown) personOk = personOk && !faces.people.empty();
        if (r.person == PersonFilter::Specific)
            personOk = personOk && std::find(faces.people.begin(), faces.people.end(), r.personId) != faces.people.end();
        if (r.requireLooking) personOk = personOk && faces.looking;

        const auto* b = findBuiltin(r.gesture);
        const bool dynamic = b && b->dynamic;

        const HandObservation* match = nullptr;
        if (b && b->twoHands) {
            // Two-hand gestures: both hands visible, and for the static ones the pair pose matches.
            if (hands.size() >= 2 && (dynamic || twoHandPose == r.gesture)) match = &hands[0];
        } else {
            for (const auto& h : hands)
                if (handMatches(r, h)) { match = &h; break; }
        }

        auto fire = [&] {
            GestureEvent e;
            e.ruleId = r.id;
            e.ruleName = r.name;
            e.action = r.action;
            e.gesture = r.gesture;
            events.push_back(std::move(e));
            st.lastFired = nowMs;
        };

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
