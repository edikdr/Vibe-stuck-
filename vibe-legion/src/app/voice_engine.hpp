// Voice commands: one worker thread that, on request, opens the microphone, waits for a
// phrase, recognises it against the user's list of commands (Vosk, offline) and checks
// that the voice is the owner's (speaker embedding). Nothing listens between requests:
// the microphone is opened by the "listen" gesture and closed after the phrase.
// Models are loaded on first use and dropped after a few idle minutes.
#pragma once

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/settings.hpp"
#include "core/voice.hpp"

namespace vl {

struct VoiceOutcome {
    enum class Kind {
        Command,        // recognised, voice accepted: run commandId
        NotUnderstood,  // speech, but none of the phrases
        NoSpeech,       // nobody spoke in time
        NotOwner,       // a phrase was recognised, but the voice is not the owner's
        NoVoicePrint,   // "owner only" is on and no voice has been enrolled
        Error,          // microphone / models
        Cancelled
    };
    Kind kind = Kind::Error;
    std::string heard;       // recognised phrase
    std::string commandId;
    float score = -1.f;      // similarity to the owner (-1: not measured)
    std::string error;       // Russian
};

struct VoiceEnrollOutcome {
    bool ok = false;
    std::vector<std::vector<float>> samples;
    float speechSec = 0.f;
    float consistency = 0.f;  // how alike the pieces are (typical score of the owner's own voice)
    std::string error;
};

class VoiceEngine {
public:
    enum class State { Idle, Loading, Listening, Recognizing, Enrolling };
    struct Callbacks {
        std::function<void(VoiceOutcome)> onOutcome;          // worker thread
        std::function<void(VoiceEnrollOutcome)> onEnrolled;   // worker thread
        std::function<void()> onState;                        // worker thread, throttled
    };

    VoiceEngine(std::filesystem::path modelsDir, std::filesystem::path voskDir, Callbacks cb);
    ~VoiceEngine();

    void configure(const VoiceSettings& s, const voice::VoicePrint& print);
    // Are the library and both models on disk? Otherwise *missingRu says what to do.
    bool installed(std::string* missingRu) const;

    bool listen();          // false: busy
    bool enroll(int ms);    // records `ms` of the user reading aloud
    void cancel();
    void warmUp();          // load the models now (the voice page is open)

    State state() const { return state_; }
    float level() const { return level_; }        // 0..1 microphone level while capturing
    float progress() const { return progress_; }  // 0..1 of the listening window / enrollment

    // Words of the phrase the recogniser does not know (it could never hear them).
    // Returns false while the models are not loaded or busy: ask again later.
    bool unknownWords(const std::string& phrase, std::vector<std::string>& missing);

private:
    enum class Job { None, Listen, Enroll, Warm, Quit };
    struct Models;
    void run();
    bool ensureModels(std::string* error);
    void doListen();
    void doEnroll();
    void setState(State s);
    void notify();

    std::filesystem::path modelsDir_, voskDir_;
    Callbacks cb_;

    std::thread thread_;
    std::mutex jobMutex_;
    std::condition_variable jobCv_;
    Job job_ = Job::None;
    int enrollMs_ = 15000;
    std::atomic<bool> cancel_{false};

    std::mutex cfgMutex_;
    VoiceSettings settings_;
    voice::VoicePrint print_;

    std::mutex modelMutex_;  // guards models_ (load / unload / recognition vs. dictionary look-ups)
    Models* models_ = nullptr;

    std::atomic<bool> loaded_{false};      // models are in memory
    std::atomic<bool> warmFailed_{false};  // loading failed: do not retry on every repaint
    std::atomic<State> state_{State::Idle};
    std::atomic<float> level_{0.f}, progress_{0.f};
    uint64_t lastNotify_ = 0;
};

}  // namespace vl
