// Voice commands: one worker thread that keeps the microphone open, cuts the stream into
// phrases and compares every phrase with the commands the user recorded (see
// core/voice.hpp): what was said and with which intonation, then whose voice it is.
// No gesture is needed. The microphone is open only while voice commands are switched
// on, at least one command is recorded and the app is not paused.
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

struct VoiceHeard {
    enum class Kind {
        Command,    // matched a recorded command and the voice fits: run commandId
        NotOwner,   // matched a command, but it is somebody else's voice
        Unknown     // speech that is not one of the commands (talk, music, TV)
    };
    Kind kind = Kind::Unknown;
    std::string commandId;   // Command / NotOwner: the command; Unknown: the nearest one
    float ratio = 99.f;      // distance to the nearest command / allowed distance (< 1 fits)
    float score = -1.f;      // voice similarity to the command's recordings (-1: not measured)
    float seconds = 0.f;
};

struct VoiceTakeResult {
    bool ok = false;
    std::string commandId;
    voice::VoiceTake take;
    float seconds = 0.f;
    std::string error;       // Russian
};

class VoiceEngine {
public:
    enum class State { Off, Loading, Listening, Recording, Error };
    struct Callbacks {  // all called on the worker thread
        std::function<void(VoiceHeard)> onHeard;
        std::function<void(VoiceTakeResult)> onTake;
        std::function<void()> onState;  // throttled; also while the level meter moves
    };

    VoiceEngine(std::filesystem::path modelsDir, Callbacks cb);
    ~VoiceEngine();

    void configure(const VoiceSettings& s, const voice::VoiceStore& store);
    void setActive(bool on);           // false while the app is paused / the PC is locked
    bool installed(std::string* missingRu) const;

    // Records the next phrase as a take of this command (up to 8 s to start speaking).
    void recordTake(const std::string& commandId);
    void cancelRecording();
    std::string recordingId() const;

    State state() const { return state_; }
    float level() const { return level_; }   // 0..1 microphone level
    bool speaking() const { return speaking_; }
    std::string errorText() const;

private:
    struct Models;
    void run();
    void handlePhrase(const std::vector<float>& pcm);
    void setState(State s);
    void notify(bool force = false);

    std::filesystem::path modelsDir_;
    Callbacks cb_;
    std::thread thread_;

    mutable std::mutex mutex_;          // settings_, store_, recordId_, error_
    std::condition_variable cv_;
    VoiceSettings settings_;
    voice::VoiceStore store_;
    std::string recordId_;
    uint64_t recordDeadline_ = 0;
    std::string error_;
    bool quit_ = false;

    Models* models_ = nullptr;          // worker thread only
    std::atomic<bool> active_{true};
    std::atomic<State> state_{State::Off};
    std::atomic<float> level_{0.f};
    std::atomic<bool> speaking_{false};
    uint64_t lastNotify_ = 0;
};

}  // namespace vl
