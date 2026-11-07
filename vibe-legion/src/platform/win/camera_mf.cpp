#include "platform/win/camera_mf.hpp"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <strmif.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <chrono>
#include <cstring>

namespace vl::win {

namespace {

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::wstring getString(IMFActivate* a, const GUID& key) {
    WCHAR* s = nullptr;
    UINT32 len = 0;
    std::wstring out;
    if (SUCCEEDED(a->GetAllocatedString(key, &s, &len)) && s) {
        out.assign(s, len);
        CoTaskMemFree(s);
    }
    return out;
}

bool isIrSubtype(const GUID& g) { return g == MFVideoFormat_L8 || g == MFVideoFormat_L16; }

std::vector<CameraDevice> enumerateCategory(const GUID* category, bool sensor) {
    std::vector<CameraDevice> out;
    ComPtr<IMFAttributes> attr;
    if (FAILED(MFCreateAttributes(&attr, 2))) return out;
    attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (category) attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, *category);
    IMFActivate** devs = nullptr;
    UINT32 n = 0;
    if (FAILED(MFEnumDeviceSources(attr.Get(), &devs, &n))) return out;
    for (UINT32 i = 0; i < n; ++i) {
        CameraDevice d;
        d.name = getString(devs[i], MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
        d.symlink = getString(devs[i], MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        std::wstring lower = d.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        d.infrared = sensor || lower.find(L"ir ") != std::wstring::npos || lower.find(L"infrared") != std::wstring::npos ||
                     lower.find(L" ir") != std::wstring::npos;
        out.push_back(std::move(d));
        devs[i]->Release();
    }
    CoTaskMemFree(devs);
    return out;
}

}  // namespace

MfCamera::MfCamera(FrameMailbox& mailbox) : mailbox_(mailbox) {}

MfCamera::~MfCamera() { stop(); }

std::vector<CameraDevice> MfCamera::enumerate() {
    auto colour = enumerateCategory(nullptr, false);
    auto sensors = enumerateCategory(&kCategorySensorCamera, true);
    // Sensor category may also list the colour camera's group; keep unique links.
    for (auto& s : sensors) {
        bool dup = false;
        for (const auto& c : colour)
            if (_wcsicmp(c.symlink.c_str(), s.symlink.c_str()) == 0) dup = true;
        if (!dup) colour.push_back(std::move(s));
    }
    return colour;
}

void MfCamera::configure(const CameraSettings& s) {
    std::lock_guard<std::mutex> lk(m_);
    const bool changed = s.deviceId != cfg_.deviceId || s.irDeviceId != cfg_.irDeviceId || s.width != cfg_.width ||
                         s.height != cfg_.height || s.irTorch != cfg_.irTorch;
    cfg_ = s;
    if (changed) reconfigure_ = true;
}

void MfCamera::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread([this] { run(); });
}

void MfCamera::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void MfCamera::requestIr(bool on) {
    if (wantIr_.exchange(on) != on) reconfigure_ = true;
}

void MfCamera::setLowLightBoost(bool on) { lowLight_ = on; }

void MfCamera::setStreaming(bool on) {
    if (streaming_.exchange(on) != on) reconfigure_ = true;
}

std::string MfCamera::statusText() const {
    std::lock_guard<std::mutex> lk(m_);
    return status_;
}

void MfCamera::setStatus(std::string s) {
    std::lock_guard<std::mutex> lk(m_);
    status_ = std::move(s);
}

void MfCamera::close() {
    if (lowLightApplied_) applyLowLight(false);
    if (openIr_) applyIrTorch(false);
    reader_.Reset();
    if (source_) source_->Shutdown();
    source_.Reset();
    openIr_ = false;
    usingIr_ = false;
}

bool MfCamera::open(bool ir) {
    close();
    CameraSettings cfg;
    {
        std::lock_guard<std::mutex> lk(m_);
        cfg = cfg_;
    }
    const auto devices = enumerate();
    bool haveIr = false;
    for (const auto& d : devices) haveIr = haveIr || d.infrared;
    irAvailable_ = haveIr;

    const CameraDevice* pick = nullptr;
    const std::wstring want = widen(ir ? cfg.irDeviceId : cfg.deviceId);
    for (const auto& d : devices)
        if (d.infrared == ir && (want.empty() || _wcsicmp(d.symlink.c_str(), want.c_str()) == 0)) { pick = &d; break; }
    if (!pick)
        for (const auto& d : devices)
            if (d.infrared == ir) { pick = &d; break; }
    if (!pick) {
        setStatus(ir ? "ИК-камера не найдена" : "Камера не найдена");
        return false;
    }

    ComPtr<IMFAttributes> attr;
    MFCreateAttributes(&attr, 4);
    attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    attr->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, pick->symlink.c_str());
    if (ir) attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, kCategorySensorCamera);
    // Shared (read-only) access through the Windows Camera Frame Server: we only read
    // frames and never take control of the camera, so Discord, Teams, the Camera app
    // etc. can open it at the same time. Without this, a background reader holds the
    // camera exclusively and video calls get "camera in use". Falls back to the
    // classic (exclusive) mode where the Frame Server is not available.
    attr->SetUINT32(MF_DEVSOURCE_ATTRIBUTE_FRAMESERVER_SHARE_MODE, TRUE);
    HRESULT hr = MFCreateDeviceSource(attr.Get(), &source_);
    shared_ = SUCCEEDED(hr);
    if (FAILED(hr)) {
        attr->DeleteItem(MF_DEVSOURCE_ATTRIBUTE_FRAMESERVER_SHARE_MODE);
        hr = MFCreateDeviceSource(attr.Get(), &source_);
    }
    if (FAILED(hr)) {
        setStatus(hr == E_ACCESSDENIED ? "Доступ к камере запрещён (Параметры → Конфиденциальность → Камера)"
                                       : "Не удалось открыть камеру");
        return false;
    }

    ComPtr<IMFAttributes> rattr;
    MFCreateAttributes(&rattr, 3);
    rattr->SetUINT32(MF_LOW_LATENCY, TRUE);
    rattr->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);  // MJPG decode + colour convert
    rattr->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);
    hr = MFCreateSourceReaderFromMediaSource(source_.Get(), rattr.Get(), &reader_);
    if (FAILED(hr)) {
        setStatus("Не удалось создать поток камеры");
        close();
        return false;
    }
    const DWORD stream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

    // Pick the native mode closest to the requested size (>= 15 fps), preferring
    // formats that need no decoding (NV12 > YUY2 > IR gray > MJPG).
    ComPtr<IMFMediaType> best;
    double bestScore = -1e18;
    for (DWORD i = 0;; ++i) {
        ComPtr<IMFMediaType> t;
        if (FAILED(reader_->GetNativeMediaType(stream, i, &t))) break;
        GUID sub{};
        t->GetGUID(MF_MT_SUBTYPE, &sub);
        UINT32 w = 0, h = 0, num = 0, den = 1;
        MFGetAttributeSize(t.Get(), MF_MT_FRAME_SIZE, &w, &h);
        MFGetAttributeRatio(t.Get(), MF_MT_FRAME_RATE, &num, &den);
        const double fps = den ? static_cast<double>(num) / den : 0;
        if (w == 0 || h == 0 || fps < 14.5) continue;
        double score = 0;
        const double tw = ir ? 320 : cfg.width, th = ir ? 240 : cfg.height;
        const bool big = w >= tw && h >= th;
        score -= big ? (w * h - tw * th) / 1000.0 : (tw * th - w * h) / 100.0;  // under-size is worse
        score += std::min(fps, 30.0) * 50;  // want 30 fps
        if (sub == MFVideoFormat_NV12) score += 500;
        else if (sub == MFVideoFormat_YUY2) score += 300;
        else if (isIrSubtype(sub)) score += ir ? 800 : -5000;
        else if (sub == MFVideoFormat_MJPG) score += 0;
        else score -= 200;
        if (score > bestScore) { bestScore = score; best = t; }
    }
    if (!best) {
        setStatus("Камера не отдаёт подходящий формат");
        close();
        return false;
    }
    // In shared mode another app may already control the format; then we take
    // whatever the camera is streaming and let the reader convert / scale it.
    if (FAILED(reader_->SetCurrentMediaType(stream, nullptr, best.Get()))) {
        ComPtr<IMFMediaType> curNative;
        if (SUCCEEDED(reader_->GetNativeMediaType(stream, static_cast<DWORD>(MF_SOURCE_READER_CURRENT_TYPE_INDEX), &curNative)))
            best = curNative;
    }

    GUID nativeSub{};
    best->GetGUID(MF_MT_SUBTYPE, &nativeSub);
    UINT32 w = 0, h = 0;
    MFGetAttributeSize(best.Get(), MF_MT_FRAME_SIZE, &w, &h);
    const UINT32 nativeW = w, nativeH = h;
    // Recognition needs ~640 px wide; ask the reader to scale big shared streams down.
    if (!ir && w > static_cast<UINT32>(std::max(640, cfg.width)) * 3 / 2) {
        const UINT32 tw = static_cast<UINT32>(std::max(640, cfg.width));
        h = h * tw / w;
        h &= ~1u;
        w = tw;
    }

    // Ask for an uncompressed output we can read.
    const GUID wanted[] = {isIrSubtype(nativeSub) ? nativeSub : MFVideoFormat_NV12, MFVideoFormat_YUY2, MFVideoFormat_RGB32};
    bool set = false;
    for (int pass = 0; pass < 2 && !set; ++pass) {  // scaled size first, then the native one
        if (pass == 1) { if (w == nativeW && h == nativeH) break; w = nativeW; h = nativeH; }
        for (const GUID& g : wanted) {
            ComPtr<IMFMediaType> out;
            MFCreateMediaType(&out);
            out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            out->SetGUID(MF_MT_SUBTYPE, g);
            MFSetAttributeSize(out.Get(), MF_MT_FRAME_SIZE, w, h);
            if (SUCCEEDED(reader_->SetCurrentMediaType(stream, nullptr, out.Get()))) { set = true; break; }
        }
    }
    if (!set) {
        setStatus("Не удалось настроить формат камеры");
        close();
        return false;
    }
    ComPtr<IMFMediaType> cur;
    reader_->GetCurrentMediaType(stream, &cur);
    GUID sub{};
    cur->GetGUID(MF_MT_SUBTYPE, &sub);
    MFGetAttributeSize(cur.Get(), MF_MT_FRAME_SIZE, &w, &h);
    width_ = static_cast<int>(w);
    height_ = static_cast<int>(h);
    UINT32 strideU = 0;
    LONG strideL = 0;
    if (SUCCEEDED(cur->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideU))) strideL = static_cast<LONG>(strideU);
    if (sub == MFVideoFormat_NV12) { format_ = PixelFormat::NV12; if (!strideL) strideL = width_; }
    else if (sub == MFVideoFormat_YUY2) { format_ = PixelFormat::YUY2; if (!strideL) strideL = width_ * 2; }
    else if (sub == MFVideoFormat_L8) { format_ = PixelFormat::Gray8; if (!strideL) strideL = width_; }
    else if (sub == MFVideoFormat_L16) { format_ = PixelFormat::Gray16; if (!strideL) strideL = width_ * 2; }
    else { format_ = PixelFormat::BGRA32; if (!strideL) strideL = width_ * 4; }
    stride_ = std::abs(static_cast<int>(strideL));

    openIr_ = ir;
    usingIr_ = ir;
    if (ir && cfg.irTorch) applyIrTorch(true);
    std::string name = narrow(pick->name);
    setStatus(name + " · " + std::to_string(width_) + "×" + std::to_string(height_) + (ir ? " · ИК" : "") +
              (shared_ ? " · общий доступ" : ""));
    return true;
}

void MfCamera::applyIrTorch(bool on) {
#if defined(__IMFExtendedCameraController_INTERFACE_DEFINED__) && defined(KSCAMERA_EXTENDEDPROP_INFRAREDTORCH_ON)
    // Windows 10 1903+: ask the driver to drive the IR emitter (most Hello cameras
    // light it automatically while streaming; this makes it explicit).
    ComPtr<IMFGetService> gs;
    if (!source_ || FAILED(source_.As(&gs))) return;
    ComPtr<IMFExtendedCameraController> ctl;
    if (FAILED(gs->GetService(GUID_NULL, IID_PPV_ARGS(&ctl)))) return;
    ComPtr<IMFExtendedCameraControl> c;
    if (FAILED(ctl->GetExtendedCameraControl(0xFFFFFFFF, KSPROPERTY_CAMERACONTROL_EXTENDED_INFRAREDTORCH, &c))) return;
    const ULONGLONG caps = c->GetCapabilities();
    ULONGLONG flags = KSCAMERA_EXTENDEDPROP_INFRAREDTORCH_OFF;
    if (on) flags = (caps & KSCAMERA_EXTENDEDPROP_INFRAREDTORCH_ALTERNATINGFRAMEILLUMINATION)
                        ? KSCAMERA_EXTENDEDPROP_INFRAREDTORCH_ALTERNATINGFRAMEILLUMINATION
                        : KSCAMERA_EXTENDEDPROP_INFRAREDTORCH_ON;
    if (SUCCEEDED(c->SetFlags(flags))) c->CommitSettings();
#else
    (void)on;  // SDK without the extended camera controller: driver default applies
#endif
}

void MfCamera::applyLowLight(bool on) {
    ComPtr<IAMVideoProcAmp> amp;
    if (!source_ || FAILED(source_.As(&amp))) {
        lowLightApplied_ = on;
        return;
    }
    if (on) {
        if (!haveSaved_) {
            amp->Get(VideoProcAmp_Gain, &savedGain_, &savedGainFlags_);
            amp->Get(VideoProcAmp_BacklightCompensation, &savedBacklight_, &savedBacklightFlags_);
            haveSaved_ = true;
        }
        long mn, mx, step, def, caps;
        if (SUCCEEDED(amp->GetRange(VideoProcAmp_BacklightCompensation, &mn, &mx, &step, &def, &caps)))
            amp->Set(VideoProcAmp_BacklightCompensation, mx, VideoProcAmp_Flags_Manual);
        if (SUCCEEDED(amp->GetRange(VideoProcAmp_Gain, &mn, &mx, &step, &def, &caps)) && mx > def)
            amp->Set(VideoProcAmp_Gain, def + (mx - def) * 6 / 10, VideoProcAmp_Flags_Manual);
    } else if (haveSaved_) {
        amp->Set(VideoProcAmp_Gain, savedGain_, savedGainFlags_);
        amp->Set(VideoProcAmp_BacklightCompensation, savedBacklight_, savedBacklightFlags_);
        haveSaved_ = false;
    }
    lowLightApplied_ = on;
}

void MfCamera::run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // Capture is I/O-bound: let Windows run it on efficient cores at low clocks.
    THREAD_POWER_THROTTLING_STATE pt{};
    pt.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    pt.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    pt.StateMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &pt, sizeof pt);

    RawFrame frame;
    int64_t retryAt = 0;
    int failures = 0;
    while (running_) {
        if (reconfigure_.exchange(false)) {
            close();
            retryAt = 0;
        }
        if (!streaming_) {
            if (reader_) close();
            setStatus("Камера отключена (пауза)");
            Sleep(150);
            continue;
        }
        if (!reader_) {
            if (nowMs() < retryAt) { Sleep(100); continue; }
            const bool ir = wantIr_.load();
            if (!open(ir)) {
                if (ir) {
                    wantIr_ = false;  // fall back to colour
                    continue;
                }
                retryAt = nowMs() + std::min(10000, 1000 * (1 + failures++));
                continue;
            }
            failures = 0;
        }
        if (lowLight_ != lowLightApplied_ && !openIr_) applyLowLight(lowLight_);

        DWORD streamIndex = 0, flags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        HRESULT hr = reader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &streamIndex,
                                         &flags, &ts, &sample);
        if (FAILED(hr) || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM))) {
            // Device unplugged, privacy shutter switched off, or taken by another app.
            setStatus(hr == static_cast<HRESULT>(0xC00D3704) /* MF_E_HW_MFT_FAILED_START_STREAMING */
                          ? "Камера занята другим приложением"
                          : "Камера недоступна — переподключение…");
            close();
            retryAt = nowMs() + 2000;
            continue;
        }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            reconfigure_ = true;
            continue;
        }
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buf;
        if (FAILED(sample->ConvertToContiguousBuffer(&buf))) continue;
        BYTE* data = nullptr;
        LONG pitch = 0;
        ComPtr<IMF2DBuffer> buf2d;
        bool locked2d = false;
        DWORD len = 0;
        if (SUCCEEDED(buf.As(&buf2d)) && SUCCEEDED(buf2d->Lock2D(&data, &pitch))) {
            locked2d = true;
        } else if (SUCCEEDED(buf->Lock(&data, nullptr, &len))) {
            pitch = stride_;
        } else {
            continue;
        }
        const int absPitch = std::abs(static_cast<int>(pitch));
        size_t bytes = static_cast<size_t>(absPitch) * height_;
        if (format_ == PixelFormat::NV12) bytes += static_cast<size_t>(absPitch) * height_ / 2;
        frame.data.resize(bytes);
        if (pitch > 0) {
            std::memcpy(frame.data.data(), data, bytes);
        } else {
            // Bottom-up RGB32: flip while copying.
            for (int y = 0; y < height_; ++y)
                std::memcpy(frame.data.data() + static_cast<size_t>(y) * absPitch, data + static_cast<ptrdiff_t>(y) * pitch, absPitch);
        }
        if (locked2d) buf2d->Unlock2D();
        else buf->Unlock();

        frame.width = width_;
        frame.height = height_;
        frame.stride = absPitch;
        frame.uvOffset = 0;
        frame.format = format_;
        frame.infrared = openIr_;
        frame.timestampMs = nowMs();
        mailbox_.publish(frame);
    }
    close();
    CoUninitialize();
}

}  // namespace vl::win
