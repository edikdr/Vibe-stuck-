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
        float best = 1e9f;
        for (const auto& s : g.samples) best = std::min(best, templateDistance(t, s));
        const float conf = 1.f - best / std::max(0.01f, g.tolerance);
        if (conf > 0.f && conf > h.customConfidence) {
            h.customConfidence = conf;
            h.customId = g.id;
        }
    }
}

std::string SwipeDetector::update(const HandObservation& h, int64_t nowMs) {
    const auto& im = h.lm.image;
    const Vec2 c = (im[WRIST].xy() + im[INDEX_MCP].xy() + im[MIDDLE_MCP].xy() + im[PINKY_MCP].xy()) * 0.25f;
    trail_.push_back({c, std::max(1.f, h.palmPx), nowMs});
    while (!trail_.empty() && nowMs - trail_.front().t > 380) trail_.pop_front();
    if (nowMs - lastSwipe_ < 650 || trail_.size() < 4) return {};
    if (h.fingers.count() < 3) return {};  // swipe with an open-ish hand only
    const P& s = trail_.front();
    const float palm = (s.palm + h.palmPx) * 0.5f;
    const Vec2 d = (c - s.c) / palm;
    std::string dir;
    if (std::fabs(d.x) > 2.2f && std::fabs(d.x) > 2.f * std::fabs(d.y)) dir = d.x > 0 ? "swipe_right" : "swipe_left";
    else if (std::fabs(d.y) > 2.0f && std::fabs(d.y) > 2.f * std::fabs(d.x)) dir = d.y > 0 ? "swipe_down" : "swipe_up";
    if (!dir.empty()) {
        lastSwipe_ = nowMs;
        trail_.clear();
    }
    return dir;
}

bool GestureEngine::handMatches(const GestureRule& r, const HandObservation& h, std::string* whyNot) {
    auto no = [&](const char* m) {
        if (whyNot) *whyNot = m;
        return false;
    };
    if (r.gesture.rfind("custom:", 0) == 0) {
        if (h.customId != r.gesture.substr(7)) return no("другой жест");
    } else if (const auto* b = findBuiltin(r.gesture); b && !b->dynamic) {
        if (h.pose != r.gesture) return no("другой жест");
    }
    if (r.hand == HandSide::Left && h.isRight) return no("нужна левая рука");
    if (r.hand == HandSide::Right && !h.isRight) return no("нужна правая рука");
    if (r.useDistance) {
        if (h.distanceCm <= 0.f) return no("дистанция неизвестна");
        if (h.distanceCm < r.minCm) return no("слишком близко");
        if (h.distanceCm > r.maxCm) return no("слишком далеко");
    }
    return true;
}

std::vector<GestureEvent> GestureEngine::update(const std::vector<HandObservation>& hands,
                                                const std::string& dynamicGesture, const FaceContext& faces,
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
        for (const auto& h : hands)
            if (handMatches(r, h)) { match = &h; break; }

        auto fire = [&](const HandObservation* h) {
            GestureEvent e;
            e.ruleId = r.id;
            e.ruleName = r.name;
            e.action = r.action;
            e.gesture = r.gesture;
            e.distanceCm = h ? h->distanceCm : -1.f;
            events.push_back(std::move(e));
            st.lastFired = nowMs;
        };

        if (dynamic) {
            st.progress = 0.f;
            if (personOk && match && dynamicGesture == r.gesture && nowMs - st.lastFired >= r.cooldownMs) fire(match);
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
                        fire(match);
                        st.firedThisHold = true;
                    }
                } else if (r.repeat && nowMs - st.lastFired >= r.repeatMs) {
                    fire(match);
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
