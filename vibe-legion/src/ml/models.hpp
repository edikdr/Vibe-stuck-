// The four networks. Each owns its session and a preallocated input buffer, so a
// steady-state frame does zero heap allocations on our side.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/detection.hpp"
#include "core/face_db.hpp"
#include "core/image.hpp"
#include "ml/ort_runtime.hpp"

namespace vl::ml {

class PalmDetector {
public:
    bool load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err);
    std::vector<PalmDetection> detect(const Image& rgb, float scoreThreshold = 0.5f, int maxHands = 2);
    bool ok() const { return m_.ok(); }
    double lastMs() const { return m_.lastMs; }
    const std::string& device() const { return m_.device; }

private:
    ModelSession m_;
    PalmDecoder decoder_;
    std::vector<float> input_;
    int boxesIdx_ = 0, scoresIdx_ = 1;
};

class HandLandmarker {
public:
    bool load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err);
    // Returns false if the model says there is no hand in the ROI.
    bool run(const Image& rgb, const RotatedRect& roi, HandLandmarks& out, float presenceThreshold = 0.6f);
    bool ok() const { return m_.ok(); }
    double lastMs() const { return m_.lastMs; }

private:
    ModelSession m_;
    std::vector<float> input_;
    int lmIdx_ = 0, presIdx_ = 1, handIdx_ = 2, worldIdx_ = 3;
    static constexpr int kInput = 224;
};

class FaceDetector {
public:
    // width/height: inference resolution (multiples of 32 recommended, >= 128).
    bool load(const std::filesystem::path& file, const InferenceConfig& cfg, int width, int height, std::string* err);
    std::vector<FaceDetection> detect(const Image& rgb, float scoreThreshold = 0.7f, int maxFaces = 4);
    bool ok() const { return m_.ok(); }
    double lastMs() const { return m_.lastMs; }

private:
    ModelSession m_;
    std::vector<float> input_;
    int w_ = 0, h_ = 0;
    int idx_[12] = {};
};

class FaceEmbedder {
public:
    bool load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err);
    void unload() { m_ = {}; std::vector<float>().swap(input_); }
    bool embed(const Image& rgb, const FaceDetection& face, Embedding& out);
    bool ok() const { return m_.ok(); }
    double lastMs() const { return m_.lastMs; }

private:
    ModelSession m_;
    std::vector<float> input_;
};

}  // namespace vl::ml
