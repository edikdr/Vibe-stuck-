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
    bool dynamic;       // movements: instantaneous, hold time ignored
    bool twoHands = false;
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
    float palmPx = 0.f;        // wrist -> middle MCP, pixels
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
// A gesture's score is the mean of its 3 nearest samples, so one stray sample
// recorded mid-transition cannot make an unrelated pose match.
void matchCustom(HandObservation& h, const std::vector<CustomGesture>& gestures);

// Turns a raw recording (every frame for several seconds) into a compact sample
// set: drops outliers (hand entering / leaving, tracker glitches), merges
// near-duplicates and keeps at most `maxSamples`, spread over the recording.
std::vector<std::array<float, 42>> condenseSamples(const std::vector<std::array<float, 42>>& raw, size_t maxSamples);

// Movements of one hand, in palm-length units (independent of distance to the camera):
//   * swipes left / right / up / down: a fast, mostly straight stroke of >= 1.7 palm
//     lengths within 0.5 s, with the hand open for most of it (motion blur makes
//     single frames look like a fist, so one bad frame no longer cancels a swipe);
//     a hand that leaves the frame mid-stroke is evaluated on the way out;
//   * wave: the open hand goes side to side, >= 3 reversals within 1.5 s.
// Works from ~8 fps (dim room) up.
class MotionDetector {
public:
    // Returns "swipe_left" / "swipe_right" / "swipe_up" / "swipe_down" / "wave" or "".
    std::string update(const HandObservation& h, int64_t nowMs);
    // The hand was lost this frame: a stroke that carried it out of view still counts.
    std::string lost(int64_t nowMs);
    void reset() { trail_.clear(); wave_.clear(); pending_.clear(); }

private:
    struct P { Vec2 c; float palm; int64_t t; bool open; };
    // Direction of a qualifying stroke in the swipe window ("" = none); axis = unit stroke direction.
    std::string stroke(int64_t nowMs, float minTravel, Vec2* axis, float* palm) const;
    std::string wave(int64_t nowMs);
    std::deque<P> trail_;   // last 0.5 s (swipes)
    std::deque<P> wave_;    // last 1.5 s (waving)
    int64_t lastFire_ = -100000;
    // A swipe is confirmed ~0.18 s after the stroke unless the hand comes straight
    // back: that is the first half of a wave, not a swipe.
    std::string pending_;
    int64_t pendingT_ = 0;
    Vec2 pendingAt_{}, pendingAxis_{};
    float pendingPalm_ = 1.f;
};
using SwipeDetector = MotionDetector;

// Gestures made with both hands.
//   static: "two_palms" (both open palms), "two_fists";
//   dynamic: "two_spread" (hands move apart), "two_pinch" (hands come together).
class TwoHandDetector {
public:
    // a, b: the two tracked hands this frame (nullptr when fewer than two are visible).
    std::string staticPose(const HandObservation* a, const HandObservation* b) const;
    std::string update(const HandObservation* a, const HandObservation* b, int64_t nowMs);
    void reset() { trail_.clear(); }

private:
    struct P { float d; int64_t t; };  // distance between palm centres / mean palm length
    std::deque<P> trail_;
    int64_t lastFire_ = -100000;
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
};

class GestureEngine {
public:
    // Feed one processed frame. `hands` may be empty. Returns fired events.
    // twoHandPose: static two-hand gesture this frame ("" = none). dynamicGesture may be a
    // one-hand movement or a two-hand one ("two_spread" / "two_pinch").
    std::vector<GestureEvent> update(const std::vector<HandObservation>& hands, const std::string& dynamicGesture,
                                     const std::string& twoHandPose, const FaceContext& faces,
                                     const std::vector<GestureRule>& rules, FaceGate gate, int64_t nowMs);

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
