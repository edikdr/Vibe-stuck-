// Hand pose classification (built-in poses + user-recorded templates), swipe
// detection and the rule engine (hold time, cooldown, distance window, face gating).
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/detection.hpp"
#include "core/settings.hpp"

namespace vl {

struct BuiltinGesture {
    const char* id;
    const char* nameRu;
    const char* glyph;  // Segoe Fluent Icons / emoji fallback hint for the UI
    bool dynamic;       // swipes: instantaneous, hold time ignored
};
const std::vector<BuiltinGesture>& builtinGestures();
const BuiltinGesture* findBuiltin(const std::string& id);

struct FingerState {
    // thumb, index, middle, ring, pinky
    std::array<bool, 5> extended{};
    std::array<float, 5> curl{};  // 0 = straight .. 1 = fully curled
    int count() const { int n = 0; for (bool e : extended) n += e; return n; }
};

struct HandObservation {
    HandLandmarks lm;
    FingerState fingers;
    bool isRight = false;       // user's actual hand (frames are mirrored = selfie view)
    bool palmFacing = false;    // palm towards the camera
    float distanceCm = -1.f;    // smoothed
    float palmPx = 0.f;         // wrist -> middle MCP, pixels
    std::string pose;           // best built-in static pose id ("" = none)
    float poseConfidence = 0.f;
    std::string customId;       // matched custom template ("" = none)
    float customConfidence = 0.f;
};

// Static pose from one frame of landmarks.
void analyzeHand(HandObservation& h);

// 42-float normalised template of a hand (for recording and matching custom gestures).
std::array<float, 42> handTemplate(const HandLandmarks& lm, bool isRight, bool rotationInvariant,
                                   bool mirrorInvariant);
float templateDistance(const std::array<float, 42>& a, const std::array<float, 42>& b);
// Best matching custom gesture for the hand; sets customId/customConfidence.
void matchCustom(HandObservation& h, const std::vector<CustomGesture>& gestures);

// Swipe detector on the palm centre, in palm-length units per second.
class SwipeDetector {
public:
    // Returns "swipe_left" / "swipe_right" / "swipe_up" / "swipe_down" or "".
    std::string update(const HandObservation& h, int64_t nowMs);
    void reset() { trail_.clear(); }

private:
    struct P { Vec2 c; float palm; int64_t t; };
    std::deque<P> trail_;
    int64_t lastSwipe_ = 0;
};

// What the engine knows about faces this frame.
struct FaceContext {
    bool anyFace = false;
    bool looking = false;             // at least one face looking at the screen
    std::vector<std::string> people;  // recognised person ids present
};

struct GestureEvent {
    std::string ruleId;
    std::string ruleName;
    Action action;
    std::string gesture;
    float distanceCm = -1.f;
};

class GestureEngine {
public:
    // Feed one processed frame. `hands` may be empty. Returns fired events.
    std::vector<GestureEvent> update(const std::vector<HandObservation>& hands, const std::string& dynamicGesture,
                                     const FaceContext& faces, const std::vector<GestureRule>& rules,
                                     FaceGate gate, int64_t nowMs);

    // 0..1 hold progress per rule id (for the OSD ring / UI).
    float progress(const std::string& ruleId) const;
    void reset() { state_.clear(); }

    // Pure predicate, exposed for tests and the "why didn't it fire" UI hint.
    static bool handMatches(const GestureRule& r, const HandObservation& h, std::string* whyNot = nullptr);

private:
    struct RuleState {
        bool holding = false;
        int64_t holdStart = 0;
        int64_t lastSeen = 0;
        int64_t lastFired = -1000000;
        bool firedThisHold = false;
        float progress = 0.f;
    };
    std::unordered_map<std::string, RuleState> state_;
};

}  // namespace vl
