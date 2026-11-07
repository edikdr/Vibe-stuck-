// The vision worker: one thread that pulls the newest camera frame at an adaptive
// rate, runs night enhancement, face + hand models, the gesture engine, and
// publishes a snapshot for the UI. Platform-independent (tested on Linux).
#pragma once

#include <atomic>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/face_db.hpp"
#include "core/filters.hpp"
#include "core/gestures.hpp"
#include "core/night_vision.hpp"
#include "core/settings.hpp"
#include "ml/models.hpp"
#include "vision/frame.hpp"

namespace vl {

enum class PipelineMode { Starting, Paused, Sleep, Idle, Active, NoCamera, Covered, ModelError };
const char* modeTextRu(PipelineMode m);

struct FaceInfo {
    FaceDetection det;
    std::string personId;   // "" = unknown
    std::string name;
    float similarity = 0.f;
    bool looking = false;
};

struct PipelineSnapshot {
    PipelineMode mode = PipelineMode::Starting;
    std::string reason;          // why paused / error text
    float fps = 0.f;
    double msFrame = 0, msPalm = 0, msHand = 0, msFace = 0, msEmbed = 0;
    int frameW = 0, frameH = 0;

    bool hasHand = false;
    HandObservation hand;        // primary hand (the larger one)
    bool hasSecondHand = false;
    HandObservation hand2;
    std::string twoHandPose;     // "two_palms" / "two_fists" / ""
    std::string lastDynamic;     // last movement id (swipe, wave, two-hand), kept ~1 s for display
    std::string whyNot;          // why the first enabled rule does not match right now

    std::vector<FaceInfo> faces;

    float light = 0.f;           // mean luminance of the raw frame
    bool nightActive = false;
    bool infrared = false;
    bool irAlternating = false;
    std::string device;          // execution provider in use
    std::string camera;          // camera status line

    std::vector<std::pair<std::string, float>> ruleProgress;

    // Preview (only produced while the UI asks for it).
    std::vector<uint8_t> previewBgra;
    int previewW = 0, previewH = 0;
    uint64_t previewSeq = 0;

    // Long-running tasks.
    float enrollProgress = -1.f;  // <0 = idle
    float recordProgress = -1.f;
    std::string taskHint;
};

struct PipelineCallbacks {
    std::function<void(const GestureEvent&)> onGesture;
    std::function<void(const std::string& personId, std::vector<Embedding>, bool ir)> onEnrolled;
    // ok = false: unusable recording (report says why). Otherwise learned carries kind,
    // rows, threshold and movement; report is a one-line Russian summary.
    std::function<void(const std::string& gestureId, bool ok, CustomGesture learned, std::string report)> onRecorded;
    std::function<void()> onSnapshot;  // throttled (~15 Hz while previewing, 2 Hz otherwise)
};

class VisionPipeline {
public:
    VisionPipeline(std::filesystem::path modelsDir, FrameMailbox& mailbox, CameraControl* camera,
                   PipelineCallbacks cb);
    ~VisionPipeline();

    void start();
    void stop();

    void setSettings(std::shared_ptr<const Settings> s);
    void setFaceDb(std::shared_ptr<const FaceDatabase> db);
    void setPaused(bool paused, std::string reason);  // external pause (lock, game, user)
    void setEco(bool eco);                            // battery / energy saver
    void setPreviewWanted(bool on) { previewWanted_ = on; }

    // Tasks (results arrive through callbacks).
    // Collects face samples for durationMs (counted from when a face is first seen).
    void startEnrollment(const std::string& personId, int durationMs);
    void startRecording(const std::string& gestureId, bool rotationInvariant, bool mirrorInvariant,
                        int ms = CustomGesture::kRecordMs);
    void cancelTasks();

    PipelineSnapshot snapshot() const;

    // Testing hook: process one already-converted RGB frame synchronously.
    void processForTest(const Image& rgb, int64_t nowMs);

private:
    void run();
    void loadModels(const Settings& s);
    void processFrame(Image& rgb, bool ir, int64_t nowMs, const Settings& s);
    void updateNight(float light, bool ir, int64_t nowMs, const Settings& s);
    int targetFps(const Settings& s) const;

    std::filesystem::path modelsDir_;
    FrameMailbox& mailbox_;
    CameraControl* camera_;
    PipelineCallbacks cb_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> eco_{false};
    std::atomic<bool> previewWanted_{false};

    mutable std::mutex cfgMutex_;
    std::shared_ptr<const Settings> settings_;
    std::shared_ptr<const FaceDatabase> faceDb_;
    std::string pauseReason_;

    mutable std::mutex snapMutex_;
    PipelineSnapshot snap_;

    // Worker-thread state ------------------------------------------------
    ml::PalmDetector palm_;
    ml::HandLandmarker hand_;
    ml::FaceDetector face_;
    ml::FaceEmbedder embedder_;
    std::string loadedKey_;
    std::string modelError_;
    int64_t embedderLastUse_ = 0;
    std::string embedModelLoaded_;

    NightVision night_;
    IrFrameSelector irSel_;
    GestureEngine engine_;
    TwoHandDetector twoHands_;
    CustomMatcher custom_;

    // Up to two tracked hands. Each keeps its own landmark filter, movement detector
    // and a short pose history (majority vote against single-frame flicker).
    struct Track {
        bool active = false;
        HandLandmarks lastLm;
        std::array<OneEuro, kHandLandmarks * 2> filter;
        MotionDetector motion;
        std::deque<std::string> poses;
        std::deque<std::pair<int64_t, Vec2>> path;  // palm centre, last 0.3 s (speed)
    };
    std::array<Track, 2> tracks_;
    int64_t lastPalmScanMs_ = -100000;
    std::atomic<bool> twoTracked_{false};  // two hands -> frame rate capped (CPU budget)
    void resetTrack(Track& t);
    int64_t lastHandMs_ = -100000, lastFaceSeenMs_ = -100000, lastFaceRunMs_ = -100000;
    int64_t lastFrameMs_ = 0;
    std::vector<FaceInfo> faces_;
    std::vector<int64_t> faceRecognizedMs_;
    std::string lastDynamic_;
    int64_t lastDynamicMs_ = 0;

    float lightEma_ = 0.5f;
    bool nightOn_ = false;
    int64_t darkSinceMs_ = -1, brightSinceMs_ = -1, irSinceMs_ = -1, coveredSinceMs_ = -1;
    bool lowLightHw_ = false;

    std::vector<int64_t> frameTimes_;
    int64_t lastSnapshotNotify_ = 0;
    uint64_t previewSeq_ = 0;

    // Tasks.
    struct Enroll { std::string person; int durationMs = 10000; std::vector<Embedding> got; bool ir = false;
                    int64_t start = 0, last = 0; };
    struct Record { std::string id; bool rot = true, mir = true; int ms = CustomGesture::kRecordMs; int64_t start = 0;
                    std::vector<RecordedFrame> frames; };
    std::mutex taskMutex_;
    std::unique_ptr<Enroll> enroll_;
    std::unique_ptr<Record> record_;
};

}  // namespace vl
