// Low-light enhancement ("night vision") for the RGB webcam, and helpers for the
// Windows Hello IR sensor. Everything here runs on the ~640x360 processing frame,
// costs ~1 ms on one Zen 5 core and allocates nothing after the first frame.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/image.hpp"

namespace vl {

struct NightParams {
    float strength = 0.7f;      // 0..1 overall aggressiveness
    float targetMean = 0.42f;   // desired mean luminance after gamma
    bool localContrast = true;  // CLAHE on luminance
    bool denoise = true;        // motion-adaptive temporal filter
    float clipLimit = 2.5f;     // CLAHE clip limit (x average bin height)
};

struct NightStats {
    float meanIn = 0.f;
    float meanOut = 0.f;
    float gamma = 1.f;
};

class NightVision {
public:
    // In-place enhancement of an RGB image.
    void process(Image& rgb, const NightParams& p, NightStats* stats = nullptr);
    void reset() { prev_.clear(); }

private:
    void temporalDenoise(Image& rgb, float strength, float mean);
    void buildClahe(const std::vector<uint8_t>& luma, int w, int h, float clip);

    std::vector<uint8_t> prev_;
    std::vector<uint8_t> luma_;
    static constexpr int kTilesX = 8, kTilesY = 5;
    std::array<std::array<uint8_t, 256>, kTilesX * kTilesY> tileLut_{};
};

// Many Hello IR cameras alternate "emitter on" / "emitter off" frames. This picks
// the lit frames and, optionally, subtracts the unlit one to cancel sunlight / lamps.
class IrFrameSelector {
public:
    // Returns true if `frame` should be processed. May modify it (ambient subtraction).
    bool accept(Image& frame, bool subtractAmbient);
    bool alternating() const { return alternating_; }

private:
    std::array<float, 8> history_{};
    int count_ = 0;
    bool alternating_ = false;
    float lastMean_ = 0.f;
    Image dark_;
};

// Fixed lookup for display: maps an IR (gray) image to a pleasant "night vision"
// tint for the preview only. Inference always sees the neutral gray.
void tintNightPreview(Image& rgb);

}  // namespace vl
