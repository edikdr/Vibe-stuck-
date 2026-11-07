#include "core/distance.hpp"

#include <algorithm>
#include <cmath>

namespace vl {

float CameraIntrinsics::focalPx(int frameWidth) const {
    const float half = std::clamp(hfovDeg, 30.f, 140.f) * 0.5f * kPi / 180.f;
    return frameWidth * 0.5f / std::tan(half);
}

float DistanceEstimator::handRawCm(const HandLandmarks& h, int frameWidth) const {
    // Bones that are rigid regardless of finger pose.
    static constexpr int bones[][2] = {
        {WRIST, INDEX_MCP}, {WRIST, MIDDLE_MCP}, {WRIST, RING_MCP}, {WRIST, PINKY_MCP},
        {INDEX_MCP, PINKY_MCP}, {INDEX_MCP, MIDDLE_MCP}, {WRIST, THUMB_CMC}};
    float best = 0.f;  // pixels per metre, least foreshortened bone
    for (const auto& b : bones) {
        const float px = length(h.image[b[0]].xy() - h.image[b[1]].xy());
        const float m = length(h.world[b[0]] - h.world[b[1]]);
        if (m < 0.005f || px < 1.f) continue;
        best = std::max(best, px / m);
    }
    if (best <= 0.f) return -1.f;
    const float zMetres = intrinsics.focalPx(frameWidth) / best;
    return zMetres * 100.f * scale_;
}

float DistanceEstimator::faceRawCm(const FaceDetection& f, int frameWidth) const {
    const float ipdPx = length(f.landmarks[0] - f.landmarks[1]);
    if (ipdPx < 2.f) return -1.f;
    const HeadPose hp = estimateHeadPose(f);
    // Yaw shortens the projected eye distance roughly by cos(yaw).
    const float yawRad = hp.yaw * 0.9f;
    const float corr = std::max(0.5f, std::cos(yawRad));
    return intrinsics.focalPx(frameWidth) * (faceIpdCm / 100.f) / (ipdPx / corr) * 100.f;
}

float DistanceEstimator::smoothHand(float rawCm) {
    if (rawCm <= 0.f) return handEma_.valid() ? handEma_.value() : -1.f;
    // Outlier guard: a single frame jumping >40% is damped harder.
    if (handEma_.valid() && std::fabs(rawCm - handEma_.value()) > 0.4f * handEma_.value())
        handEma_.setAlpha(0.12f);
    else
        handEma_.setAlpha(0.35f);
    return handEma_.update(rawCm);
}

}  // namespace vl
