// Camera-to-hand / camera-to-face distance from monocular landmarks.
//
// The idea in the Gemini notes (Dist = K / |wrist - middle_mcp|) works only when the
// palm faces the camera squarely. We use the hand model's *metric world landmarks*
// (true 3D bone lengths) against their 2D pixel projections over several bones and
// keep the least foreshortened one, so tilting the hand no longer inflates the
// estimate. A per-user calibration factor absorbs hand size and lens differences.
#pragma once

#include "core/detection.hpp"
#include "core/filters.hpp"

namespace vl {

struct CameraIntrinsics {
    float hfovDeg = 74.f;  // typical 16:9 laptop webcam; refined by calibration
    float focalPx(int frameWidth) const;
};

class DistanceEstimator {
public:
    // Raw (unsmoothed) estimate, centimetres. frameWidth = width of the pixel space
    // the landmarks live in. Returns <= 0 if not computable.
    float handRawCm(const HandLandmarks& h, int frameWidth) const;
    float faceRawCm(const FaceDetection& f, int frameWidth) const;

    float smoothHand(float rawCm);
    void resetHand() { handEma_.reset(); }

    // Calibration: user holds the palm at a known distance; we take the median of
    // raw samples and store the multiplicative correction.
    void setCalibration(float scale) { scale_ = scale > 0.05f && scale < 20.f ? scale : 1.f; }
    float calibration() const { return scale_; }
    static float solveCalibration(float knownCm, float medianRawUncalibratedCm) {
        return medianRawUncalibratedCm > 1.f ? knownCm / medianRawUncalibratedCm : 1.f;
    }

    CameraIntrinsics intrinsics;
    float faceIpdCm = 6.3f;  // adult mean interpupillary distance

private:
    float scale_ = 1.f;
    Ema handEma_{0.35f};
};

}  // namespace vl
