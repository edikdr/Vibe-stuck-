// Windows side of voice commands: microphone capture (WASAPI, shared mode, converted
// by Windows to 16 kHz mono) and the microphone mute state.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "platform/win/win_common.hpp"

#include <audioclient.h>

namespace vl::win {

// Default microphone -> 16 kHz mono float. One instance per listening session; all calls
// from the same thread (COM is initialised by the caller).
class MicCapture {
public:
    MicCapture() = default;
    ~MicCapture() { stop(); }
    MicCapture(const MicCapture&) = delete;
    MicCapture& operator=(const MicCapture&) = delete;

    bool start(std::string* errorRu);
    // Appends whatever arrived since the last call. Returns false when the device is gone.
    bool read(std::vector<float>& out);
    void stop();

private:
    ComPtr<IAudioClient> client_;
    ComPtr<IAudioCaptureClient> capture_;
    bool running_ = false;
};

// Capture endpoints muted in Windows (what "Микрофон вкл/выкл" does)?
bool microphonesMuted();
// Mutes / unmutes every active microphone. Returns false if none could be changed.
bool setMicrophonesMuted(bool mute);

}  // namespace vl::win
