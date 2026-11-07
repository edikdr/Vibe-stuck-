// Model-agnostic pre/post-processing for:
//   * MediaPipe palm detector (opencv_zoo palm_detection_mediapipe_2023feb)
//   * MediaPipe hand landmarker (opencv_zoo handpose_estimation_mediapipe_2023feb)
//   * YuNet face detector (opencv_zoo face_detection_yunet_2023mar)
//   * SFace alignment (opencv_zoo face_recognition_sface_2021dec)
// Kept free of ONNX Runtime so it can be unit-tested anywhere.
#pragma once

#include <array>
#include <vector>

#include "core/geometry.hpp"

namespace vl {

// ------------------------------------------------------------------ palms

struct PalmDetection {
    RectF box;                    // source image pixels
    std::array<Vec2, 7> keypoints;  // 0 wrist, 1..4 finger bases, 5..6 thumb
    float score = 0.f;
};

class PalmDecoder {
public:
    static constexpr int kInput = 192;
    static constexpr int kAnchors = 2016;

    PalmDecoder();
    // boxes: [2016 x 18] raw regressors, scores: [2016] logits.
    // tensorToSrc maps tensor pixels (0..192) to source pixels.
    std::vector<PalmDetection> decode(const float* boxes, const float* scores, const Affine& tensorToSrc,
                                      float scoreThreshold, float nmsThreshold, int maxResults) const;

    const std::vector<Vec2>& anchors() const { return anchors_; }

private:
    std::vector<Vec2> anchors_;
};

// ROI for the landmark model computed from a palm detection (MediaPipe recipe:
// rotate so wrist->middle-finger-base points up, shift toward the fingers, enlarge).
RotatedRect handRoiFromPalm(const PalmDetection& palm);

// ------------------------------------------------------------------ hands

enum HandLm {
    WRIST = 0, THUMB_CMC, THUMB_MCP, THUMB_IP, THUMB_TIP,
    INDEX_MCP, INDEX_PIP, INDEX_DIP, INDEX_TIP,
    MIDDLE_MCP, MIDDLE_PIP, MIDDLE_DIP, MIDDLE_TIP,
    RING_MCP, RING_PIP, RING_DIP, RING_TIP,
    PINKY_MCP, PINKY_PIP, PINKY_DIP, PINKY_TIP,
    kHandLandmarks
};

struct HandLandmarks {
    std::array<Vec3, kHandLandmarks> image;  // x,y in source pixels; z relative depth (pixel scale)
    std::array<Vec3, kHandLandmarks> world;  // metric, metres, origin ~ hand centre
    float presence = 0.f;
    float rightness = 0.f;  // model output: probability the hand is a right hand (as seen in a selfie view)
};

// Map raw landmark-model output back into source image coordinates.
// lm: 63 floats in tensor pixels (0..224); world: 63 floats metres.
void decodeHandLandmarks(const float* lm, const float* world, float presence, float handedness,
                         const RotatedRect& roi, int inputSize, HandLandmarks& out);

// Tracking ROI from the previous frame's landmarks (MediaPipe HandLandmarksToRect).
RotatedRect handRoiFromLandmarks(const HandLandmarks& h);

// ------------------------------------------------------------------ faces

struct FaceDetection {
    RectF box;                      // source pixels
    std::array<Vec2, 5> landmarks;  // right eye, left eye, nose, right mouth, left mouth (subject's)
    float score = 0.f;
};

// YuNet outputs per stride (8,16,32): cls[N], obj[N], bbox[N*4], kps[N*10].
struct YuNetLevel {
    const float* cls = nullptr;
    const float* obj = nullptr;
    const float* bbox = nullptr;
    const float* kps = nullptr;
    int stride = 8;
};

std::vector<FaceDetection> decodeYuNet(const std::array<YuNetLevel, 3>& levels, int padW, int padH,
                                       const Affine& tensorToSrc, float scoreThreshold, float nmsThreshold,
                                       int maxResults);

// Similarity transform source pixels -> 112x112 aligned SFace crop.
Affine sfaceAlignment(const FaceDetection& f);

// Rough head yaw/pitch from the 5 YuNet points: 0 = looking straight at the camera,
// +-1 = profile. Good enough to tell "looking at the screen" from "turned away".
struct HeadPose {
    float yaw = 0.f, pitch = 0.f;
};
HeadPose estimateHeadPose(const FaceDetection& f);

// ------------------------------------------------------------------ common

// Greedy NMS on boxes sorted by score; returns kept indices.
std::vector<int> nms(const std::vector<RectF>& boxes, const std::vector<float>& scores, float iouThreshold,
                     int maxResults);

inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

}  // namespace vl
