// Media Foundation capture for the colour webcam and the Windows Hello IR sensor.
// One capture thread reads synchronously and publishes into a FrameMailbox; the
// source reader is set to low latency and only the newest frame is ever kept.
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/settings.hpp"
#include "platform/win/win_common.hpp"
#include "vision/frame.hpp"

struct IMFMediaSource;
struct IMFSourceReader;

namespace vl::win {

struct CameraDevice {
    std::wstring name;
    std::wstring symlink;
    bool infrared = false;
};

class MfCamera final : public CameraControl {
public:
    explicit MfCamera(FrameMailbox& mailbox);
    ~MfCamera() override;

    static std::vector<CameraDevice> enumerate();

    void configure(const CameraSettings& s);  // thread-safe, applied by the capture thread
    void start();
    void stop();

    // CameraControl
    bool irAvailable() const override { return irAvailable_; }
    bool usingIr() const override { return usingIr_; }
    void requestIr(bool on) override;
    void setLowLightBoost(bool on) override;
    void setStreaming(bool on) override;
    std::string statusText() const override;

private:
    void run();
    bool open(bool ir);
    void close();
    void applyLowLight(bool on);
    void applyIrTorch(bool on);
    void setStatus(std::string s);

    FrameMailbox& mailbox_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> streaming_{true};
    std::atomic<bool> wantIr_{false};
    std::atomic<bool> usingIr_{false};
    std::atomic<bool> irAvailable_{false};
    std::atomic<bool> lowLight_{false};
    std::atomic<bool> reconfigure_{true};

    mutable std::mutex m_;
    CameraSettings cfg_;
    std::string status_ = "Камера не запущена";

    // Capture-thread state.
    ComPtr<IMFMediaSource> source_;
    ComPtr<IMFSourceReader> reader_;
    int width_ = 0, height_ = 0, stride_ = 0;
    PixelFormat format_ = PixelFormat::NV12;
    bool openIr_ = false;
    bool lowLightApplied_ = false;
    long savedGain_ = 0, savedGainFlags_ = 0, savedBacklight_ = 0, savedBacklightFlags_ = 0;
    bool haveSaved_ = false;
};

}  // namespace vl::win
