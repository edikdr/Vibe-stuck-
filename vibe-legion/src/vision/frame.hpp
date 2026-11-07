// Camera -> pipeline hand-off. The camera thread overwrites a single slot
// ("mailbox"), so the pipeline always gets the newest frame and nothing queues up:
// this is what removes the multi-frame lag of buffered capture on Windows.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "core/image.hpp"

namespace vl {

struct RawFrame {
    std::vector<uint8_t> data;
    int width = 0, height = 0, stride = 0;
    size_t uvOffset = 0;  // NV12 chroma plane offset (0 = right after luma)
    PixelFormat format = PixelFormat::NV12;
    bool infrared = false;
    int64_t timestampMs = 0;
    uint64_t seq = 0;
};

class FrameMailbox {
public:
    // Called by the camera thread. Swaps buffers to avoid reallocations.
    void publish(RawFrame& f) {
        {
            std::lock_guard<std::mutex> lk(m_);
            f.seq = ++seq_;
            std::swap(slot_, f);
            has_ = true;
        }
        cv_.notify_one();
    }
    // Waits up to timeoutMs for a frame newer than `lastSeq`. Swaps it into `out`.
    bool take(RawFrame& out, uint64_t lastSeq, int timeoutMs) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] { return (has_ && slot_.seq != lastSeq) || wake_; }))
            return false;
        wake_ = false;
        if (!has_ || slot_.seq == lastSeq) return false;
        std::swap(out, slot_);
        has_ = false;
        return true;
    }
    void wake() {
        { std::lock_guard<std::mutex> lk(m_); wake_ = true; }
        cv_.notify_all();
    }
    void clear() { std::lock_guard<std::mutex> lk(m_); has_ = false; }

private:
    std::mutex m_;
    std::condition_variable cv_;
    RawFrame slot_;
    bool has_ = false, wake_ = false;
    uint64_t seq_ = 0;
};

// What the pipeline may ask of the camera (implemented by the Windows MF camera).
class CameraControl {
public:
    virtual ~CameraControl() = default;
    virtual bool irAvailable() const = 0;
    virtual bool usingIr() const = 0;
    virtual void requestIr(bool on) = 0;            // async source switch
    virtual void setLowLightBoost(bool on) = 0;     // gain / backlight compensation
    virtual void setStreaming(bool on) = 0;         // stop = camera LED off
    virtual std::string statusText() const = 0;
};

}  // namespace vl
