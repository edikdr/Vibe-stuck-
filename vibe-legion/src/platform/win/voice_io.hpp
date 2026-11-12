// Windows side of voice commands: microphone capture (WASAPI, shared mode, converted
// by Windows to 16 kHz mono), the Vosk speech recogniser loaded from its DLL at run
// time, and the microphone mute state.
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

// Vosk (https://alphacephei.com/vosk): offline speech recognition. The DLL is optional:
// without it the rest of the app works and the voice page says what is missing.
class Vosk {
public:
    Vosk() = default;
    ~Vosk() { unload(); }
    Vosk(const Vosk&) = delete;
    Vosk& operator=(const Vosk&) = delete;

    // dllDir: folder with libvosk.dll and its runtime DLLs; modelDir: an unpacked model.
    bool load(const std::filesystem::path& dllDir, const std::filesystem::path& modelDir, std::string* errorRu);
    void unload();
    bool ok() const { return model_ != nullptr; }

    // Is the word in the model's dictionary? (lower case; unknown words are never recognised)
    bool hasWord(const std::string& word) const;
    // Recognises one utterance against a closed list of phrases (JSON array, see
    // voice::grammarJson). Returns the text ("" or "[unk]" when nothing fits).
    std::string recognize(const std::vector<float>& pcm16k, const std::string& grammarJson) const;

private:
    HMODULE dll_ = nullptr;
    void* model_ = nullptr;
    struct Api;
    Api* api_ = nullptr;
};

}  // namespace vl::win
