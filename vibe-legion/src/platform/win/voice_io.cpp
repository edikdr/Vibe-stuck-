#include "platform/win/voice_io.hpp"

#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

#include <algorithm>


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

}  // namespace vl::win
