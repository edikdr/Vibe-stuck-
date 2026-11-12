#include "platform/win/voice_io.hpp"

#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <algorithm>

#include "core/json.hpp"

namespace vl::win {

namespace {

ComPtr<IMMDevice> defaultMicrophone() {
    ComPtr<IMMDeviceEnumerator> en;
    ComPtr<IMMDevice> dev;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return dev;
    // The microphone calls use; fall back to the general default.
    if (FAILED(en->GetDefaultAudioEndpoint(eCapture, eCommunications, &dev))) en->GetDefaultAudioEndpoint(eCapture, eConsole, &dev);
    return dev;
}

template <typename Fn>
bool forEachMicrophone(Fn&& fn) {
    ComPtr<IMMDeviceEnumerator> en;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return false;
    ComPtr<IMMDeviceCollection> all;
    if (FAILED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &all))) return false;
    UINT n = 0;
    all->GetCount(&n);
    bool any = false;
    for (UINT i = 0; i < n; ++i) {
        ComPtr<IMMDevice> d;
        ComPtr<IAudioEndpointVolume> vol;
        if (FAILED(all->Item(i, &d)) ||
            FAILED(d->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(vol.GetAddressOf()))))
            continue;
        any = fn(vol.Get()) || any;
    }
    return any;
}

// Vosk is built with MinGW and opens files with the ANSI code page. The 8.3 short path is
// plain ASCII even when the profile folder has Cyrillic (or any other) letters in it.
std::string ansiPath(const std::filesystem::path& p) {
    std::wstring w = p.wstring();
    wchar_t shortBuf[MAX_PATH * 2];
    const DWORD n = GetShortPathNameW(w.c_str(), shortBuf, static_cast<DWORD>(std::size(shortBuf)));
    if (n > 0 && n < std::size(shortBuf)) w.assign(shortBuf, n);
    const int len = WideCharToMultiByte(CP_ACP, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(std::max(0, len)), '\0');
    if (len > 0) WideCharToMultiByte(CP_ACP, 0, w.c_str(), static_cast<int>(w.size()), out.data(), len, nullptr, nullptr);
    return out;
}

}  // namespace

// ------------------------------------------------------------------ microphone

bool MicCapture::start(std::string* errorRu) {
    stop();
    auto fail = [&](const char* why) {
        if (errorRu) *errorRu = why;
        client_.Reset();
        capture_.Reset();
        return false;
    };
    ComPtr<IMMDevice> dev = defaultMicrophone();
    if (!dev) return fail("микрофон не найден");
    if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client_.GetAddressOf()))))
        return fail("микрофон недоступен (проверьте «Параметры → Конфиденциальность → Микрофон»)");
    WAVEFORMATEX fmt{};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = 16000;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = 32000;
    // Windows resamples and down-mixes for us. The buffer holds 5 s: audio keeps arriving
    // while the speech model is still loading after the gesture.
    const DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    if (FAILED(client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 50000000, 0, &fmt, nullptr)))
        return fail("микрофон не открылся (занят в монопольном режиме или запрещён)");
    if (FAILED(client_->GetService(IID_PPV_ARGS(&capture_)))) return fail("микрофон не отдаёт звук");
    if (FAILED(client_->Start())) return fail("микрофон не запустился");
    running_ = true;
    return true;
}

bool MicCapture::read(std::vector<float>& out) {
    if (!running_) return false;
    UINT32 packet = 0;
    while (SUCCEEDED(capture_->GetNextPacketSize(&packet)) && packet > 0) {
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        const HRESULT hr = capture_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr)) return hr != AUDCLNT_E_DEVICE_INVALIDATED;
        const auto* s = reinterpret_cast<const int16_t*>(data);
        const size_t at = out.size();
        out.resize(at + frames);
        for (UINT32 i = 0; i < frames; ++i) out[at + i] = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? 0.f : s[i] / 32768.f;
        capture_->ReleaseBuffer(frames);
    }
    return true;
}

void MicCapture::stop() {
    if (client_ && running_) client_->Stop();
    running_ = false;
    capture_.Reset();
    client_.Reset();
}

bool microphonesMuted() {
    ComPtr<IMMDevice> dev = defaultMicrophone();
    ComPtr<IAudioEndpointVolume> vol;
    BOOL muted = FALSE;
    if (!dev || FAILED(dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(vol.GetAddressOf()))))
        return false;
    vol->GetMute(&muted);
    return muted != FALSE;
}

bool setMicrophonesMuted(bool mute) {
    return forEachMicrophone([mute](IAudioEndpointVolume* v) { return SUCCEEDED(v->SetMute(mute ? TRUE : FALSE, nullptr)); });
}

// ------------------------------------------------------------------ Vosk

struct Vosk::Api {
    void* (*model_new)(const char*) = nullptr;
    void (*model_free)(void*) = nullptr;
    int (*model_find_word)(void*, const char*) = nullptr;
    void* (*recognizer_new_grm)(void*, float, const char*) = nullptr;
    int (*accept_waveform_s)(void*, const short*, int) = nullptr;
    const char* (*result)(void*) = nullptr;
    const char* (*final_result)(void*) = nullptr;
    void (*recognizer_free)(void*) = nullptr;
    void (*set_log_level)(int) = nullptr;
};

bool Vosk::load(const std::filesystem::path& dllDir, const std::filesystem::path& modelDir, std::string* errorRu) {
    unload();
    auto fail = [&](const char* why) {
        if (errorRu) *errorRu = why;
        unload();
        return false;
    };
    std::error_code ec;
    if (!std::filesystem::exists(dllDir / L"libvosk.dll", ec)) return fail("нет библиотеки распознавания речи (vosk\\libvosk.dll) — запустите scripts\\fetch-deps.ps1");
    if (!std::filesystem::exists(modelDir / L"am" / L"final.mdl", ec)) return fail("нет модели русской речи (models\\vosk-model-small-ru) — запустите scripts\\fetch-deps.ps1");
    // Its MinGW runtime DLLs sit next to it: search that folder for dependencies.
    dll_ = LoadLibraryExW((dllDir / L"libvosk.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll_) return fail("библиотека распознавания речи не загрузилась");
    api_ = new Api;
    auto sym = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(dll_, name)); };
    api_->model_new = reinterpret_cast<decltype(api_->model_new)>(sym("vosk_model_new"));
    api_->model_free = reinterpret_cast<decltype(api_->model_free)>(sym("vosk_model_free"));
    api_->model_find_word = reinterpret_cast<decltype(api_->model_find_word)>(sym("vosk_model_find_word"));
    api_->recognizer_new_grm = reinterpret_cast<decltype(api_->recognizer_new_grm)>(sym("vosk_recognizer_new_grm"));
    api_->accept_waveform_s = reinterpret_cast<decltype(api_->accept_waveform_s)>(sym("vosk_recognizer_accept_waveform_s"));
    api_->result = reinterpret_cast<decltype(api_->result)>(sym("vosk_recognizer_result"));
    api_->final_result = reinterpret_cast<decltype(api_->final_result)>(sym("vosk_recognizer_final_result"));
    api_->recognizer_free = reinterpret_cast<decltype(api_->recognizer_free)>(sym("vosk_recognizer_free"));
    api_->set_log_level = reinterpret_cast<decltype(api_->set_log_level)>(sym("vosk_set_log_level"));
    if (!api_->model_new || !api_->model_free || !api_->model_find_word || !api_->recognizer_new_grm ||
        !api_->accept_waveform_s || !api_->result || !api_->final_result || !api_->recognizer_free)
        return fail("библиотека распознавания речи другой версии");
    if (api_->set_log_level) api_->set_log_level(-1);  // it logs to stderr otherwise
    model_ = api_->model_new(ansiPath(modelDir).c_str());
    if (!model_) return fail("модель русской речи не открылась");
    return true;
}

void Vosk::unload() {
    if (model_ && api_ && api_->model_free) api_->model_free(model_);
    model_ = nullptr;
    delete api_;
    api_ = nullptr;
    if (dll_) FreeLibrary(dll_);
    dll_ = nullptr;
}

bool Vosk::hasWord(const std::string& word) const { return ok() && api_->model_find_word(model_, word.c_str()) >= 0; }

std::string Vosk::recognize(const std::vector<float>& pcm, const std::string& grammarJson) const {
    if (!ok() || pcm.empty()) return {};
    void* rec = api_->recognizer_new_grm(model_, 16000.f, grammarJson.c_str());
    if (!rec) return {};
    std::vector<short> s(pcm.size());
    for (size_t i = 0; i < pcm.size(); ++i) s[i] = static_cast<short>(std::clamp(pcm[i], -1.f, 1.f) * 32767.f);
    // Half a second of silence at the end lets the decoder finish the last word.
    s.insert(s.end(), 8000, 0);
    std::string text;
    auto take = [&](const char* res) {
        if (!res) return;
        const auto r = json::parse(res);
        const std::string t = r.ok ? r.value["text"].asString() : std::string();
        if (!t.empty()) text += (text.empty() ? "" : " ") + t;
    };
    for (size_t at = 0; at < s.size(); at += 4000) {
        // Non-zero: the decoder closed a phrase (a pause inside the command); collect it.
        if (api_->accept_waveform_s(rec, s.data() + at, static_cast<int>(std::min<size_t>(4000, s.size() - at))))
            take(api_->result(rec));
    }
    take(api_->final_result(rec));
    api_->recognizer_free(rec);
    return text;
}

}  // namespace vl::win
