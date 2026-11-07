#include "ml/models.hpp"

#include <algorithm>

namespace vl::ml {

// ------------------------------------------------------------------ palm

bool PalmDetector::load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err) {
    if (!m_.open(file, cfg, err)) return false;
    // Outputs: boxes [1,2016,18] and scores [1,2016,1]; identify by last dim.
    for (size_t i = 0; i < m_.outputShapes.size(); ++i) {
        const auto& s = m_.outputShapes[i];
        if (!s.empty() && s.back() == 18) boxesIdx_ = static_cast<int>(i);
        else if (!s.empty() && s.back() == 1) scoresIdx_ = static_cast<int>(i);
    }
    input_.assign(static_cast<size_t>(PalmDecoder::kInput) * PalmDecoder::kInput * 3, 0.f);
    return true;
}

std::vector<PalmDetection> PalmDetector::detect(const Image& rgb, float thr, int maxHands) {
    if (!ok() || rgb.empty()) return {};
    TensorSpec spec{PalmDecoder::kInput, PalmDecoder::kInput, TensorLayout::NHWC, false, 1.f / 255.f, 0.f};
    const Affine t = letterboxToTensor(rgb, spec, input_.data());
    auto out = m_.run(input_.data(), {1, PalmDecoder::kInput, PalmDecoder::kInput, 3});
    const float* boxes = out[boxesIdx_].GetTensorData<float>();
    const float* scores = out[scoresIdx_].GetTensorData<float>();
    return decoder_.decode(boxes, scores, t, thr, 0.3f, maxHands);
}

// ------------------------------------------------------------------ hand landmarks

bool HandLandmarker::load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err) {
    if (!m_.open(file, cfg, err)) return false;
    // TF-exported names: Identity (screen 63), Identity_1 (presence), Identity_2
    // (handedness), Identity_3 (world 63). Fall back to graph order if renamed.
    const int a = m_.outputIndex("Identity"), b = m_.outputIndex("Identity_1");
    const int c = m_.outputIndex("Identity_2"), d = m_.outputIndex("Identity_3");
    if (a >= 0 && b >= 0 && c >= 0 && d >= 0) { lmIdx_ = a; presIdx_ = b; handIdx_ = c; worldIdx_ = d; }
    if (m_.outputNames.size() < 4) {
        if (err) *err = "неожиданная модель руки (нужно 4 выхода)";
        m_ = {};
        return false;
    }
    input_.assign(static_cast<size_t>(kInput) * kInput * 3, 0.f);
    return true;
}

bool HandLandmarker::run(const Image& rgb, const RotatedRect& roi, HandLandmarks& out, float presenceThr) {
    if (!ok() || rgb.empty() || roi.size < 8.f) return false;
    TensorSpec spec{kInput, kInput, TensorLayout::NHWC, false, 1.f / 255.f, 0.f};
    warpToTensor(rgb, roiToImage(roi, static_cast<float>(kInput)), spec, input_.data());
    auto o = m_.run(input_.data(), {1, kInput, kInput, 3});
    const float presence = o[presIdx_].GetTensorData<float>()[0];
    if (presence < presenceThr) return false;
    decodeHandLandmarks(o[lmIdx_].GetTensorData<float>(), o[worldIdx_].GetTensorData<float>(), presence,
                        o[handIdx_].GetTensorData<float>()[0], roi, kInput, out);
    return true;
}

// ------------------------------------------------------------------ faces

bool FaceDetector::load(const std::filesystem::path& file, const InferenceConfig& cfg, int width, int height,
                        std::string* err) {
    std::vector<uint8_t> bytes;
    if (!OrtRuntime::readFile(file, bytes)) {
        if (err) *err = "не найден файл модели: " + file.filename().string();
        return false;
    }
    width = (width + 31) / 32 * 32;
    height = (height + 31) / 32 * 32;
    const bool patched = patchYuNetInputSize(bytes, width, height);
    if (!m_.open(file, cfg, err, &bytes)) return false;
    const auto& shape = m_.inputShapes[0];  // NCHW
    if (shape.size() == 4 && shape[2] > 0 && shape[3] > 0) {
        h_ = static_cast<int>(shape[2]);
        w_ = static_cast<int>(shape[3]);
    } else {
        w_ = width;
        h_ = height;
    }
    (void)patched;  // unpatched model simply runs at its native 640x640
    static const char* names[12] = {"cls_8", "cls_16", "cls_32", "obj_8", "obj_16", "obj_32",
                                    "bbox_8", "bbox_16", "bbox_32", "kps_8", "kps_16", "kps_32"};
    for (int i = 0; i < 12; ++i) {
        idx_[i] = m_.outputIndex(names[i]);
        if (idx_[i] < 0) {
            if (err) *err = std::string("в модели лица нет выхода ") + names[i];
            m_ = {};
            return false;
        }
    }
    input_.assign(static_cast<size_t>(w_) * h_ * 3, 0.f);
    return true;
}

std::vector<FaceDetection> FaceDetector::detect(const Image& rgb, float thr, int maxFaces) {
    if (!ok() || rgb.empty()) return {};
    // YuNet: BGR, 0..255, NCHW, image at top-left, zero padding right/bottom.
    TensorSpec spec{w_, h_, TensorLayout::NCHW, true, 1.f, 0.f};
    const Affine t = fitTopLeftToTensor(rgb, spec, input_.data());
    auto o = m_.run(input_.data(), {1, 3, h_, w_});
    std::array<YuNetLevel, 3> lv;
    const int strides[3] = {8, 16, 32};
    for (int i = 0; i < 3; ++i) {
        lv[i].cls = o[idx_[i]].GetTensorData<float>();
        lv[i].obj = o[idx_[3 + i]].GetTensorData<float>();
        lv[i].bbox = o[idx_[6 + i]].GetTensorData<float>();
        lv[i].kps = o[idx_[9 + i]].GetTensorData<float>();
        lv[i].stride = strides[i];
        // Guard against a model whose output size disagrees with our grid.
        const auto n = o[idx_[i]].GetTensorTypeAndShapeInfo().GetElementCount();
        if (n != static_cast<size_t>((w_ / strides[i]) * (h_ / strides[i]))) return {};
    }
    return decodeYuNet(lv, w_, h_, t, thr, 0.3f, maxFaces);
}

// ------------------------------------------------------------------ face embedding

bool FaceEmbedder::load(const std::filesystem::path& file, const InferenceConfig& cfg, std::string* err) {
    if (!m_.open(file, cfg, err)) return false;
    input_.assign(3 * 112 * 112, 0.f);
    return true;
}

bool FaceEmbedder::embed(const Image& rgb, const FaceDetection& face, Embedding& out) {
    if (!ok() || rgb.empty()) return false;
    // SFace: aligned 112x112, RGB, 0..255, NCHW.
    TensorSpec spec{112, 112, TensorLayout::NCHW, false, 1.f, 0.f};
    const Affine srcToCrop = sfaceAlignment(face);
    warpToTensor(rgb, srcToCrop.inverse(), spec, input_.data());
    auto o = m_.run(input_.data(), {1, 3, 112, 112});
    const float* f = o[0].GetTensorData<float>();
    if (o[0].GetTensorTypeAndShapeInfo().GetElementCount() < out.size()) return false;
    std::copy(f, f + out.size(), out.begin());
    l2normalize(out);
    return true;
}

}  // namespace vl::ml
