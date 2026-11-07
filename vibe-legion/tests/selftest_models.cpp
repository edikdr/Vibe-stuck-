// Runs the real ONNX models on binary PPM (P6) images and prints what they find.
// Usage: vl_selftest <models dir> <image.ppm> [more.ppm ...]
// Also handy on Windows to check that the models/ folder is complete.
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>

#include "core/gestures.hpp"
#include "ml/models.hpp"
#include "vision/pipeline.hpp"

using namespace vl;

static bool readPpm(const char* path, Image& img) {
    std::ifstream f(path, std::ios::binary);
    std::string magic;
    int w, h, maxv;
    if (!(f >> magic >> w >> h >> maxv) || magic != "P6" || maxv != 255) return false;
    f.get();
    img.reset(w, h, 3);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(img.data.data()), static_cast<std::streamsize>(img.data.size())));
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <models dir> <image.ppm>...\n", argv[0]);
        return 2;
    }
    const std::filesystem::path dir = argv[1];
    ml::InferenceConfig cfg;
    std::string err;
    ml::PalmDetector palm;
    ml::HandLandmarker hand;
    ml::FaceDetector face;
    ml::FaceEmbedder emb;
    if (!palm.load(dir / "palm_detection_mediapipe_2023feb.onnx", cfg, &err)) { std::printf("palm: %s\n", err.c_str()); return 1; }
    if (!hand.load(dir / "handpose_estimation_mediapipe_2023feb.onnx", cfg, &err)) { std::printf("hand: %s\n", err.c_str()); return 1; }
    if (!face.load(dir / "face_detection_yunet_2023mar.onnx", cfg, 224, 128, &err)) { std::printf("face: %s\n", err.c_str()); return 1; }
    auto sf = dir / "face_recognition_sface_2021dec_int8.onnx";
    if (!std::filesystem::exists(sf)) sf = dir / "face_recognition_sface_2021dec.onnx";
    if (!emb.load(sf, cfg, &err)) { std::printf("sface: %s\n", err.c_str()); return 1; }
    std::printf("device: %s\n", palm.device().c_str());

    std::vector<Embedding> embeddings;
    for (int a = 2; a < argc; ++a) {
        Image img;
        if (!readPpm(argv[a], img)) { std::printf("%s: cannot read\n", argv[a]); continue; }
        std::printf("== %s (%dx%d)\n", argv[a], img.width, img.height);
        auto palms = palm.detect(img, 0.5f, 2);
        std::printf("  palms: %zu (%.1f ms)\n", palms.size(), palm.lastMs());
        for (const auto& p : palms) {
            std::printf("    score %.2f box %.0f,%.0f %.0fx%.0f\n", p.score, p.box.x, p.box.y, p.box.w, p.box.h);
            HandLandmarks lm;
            if (hand.run(img, handRoiFromPalm(p), lm)) {
                HandObservation o;
                o.lm = lm;
                o.isRight = lm.rightness > 0.5f;
                analyzeHand(o);
                // Second pass from the tracking ROI must find the hand again.
                HandLandmarks lm2;
                const bool tracked = hand.run(img, handRoiFromLandmarks(lm), lm2);
                std::printf("    hand: presence %.2f %s pose '%s' (%.2f) fingers %d%d%d%d%d facing %d tracked=%d (%.1f ms)\n",
                            lm.presence, o.isRight ? "right" : "left", o.pose.c_str(), o.poseConfidence,
                            o.fingers.extended[0], o.fingers.extended[1], o.fingers.extended[2], o.fingers.extended[3],
                            o.fingers.extended[4], o.palmFacing, tracked, hand.lastMs());
                std::printf("      wrist %.0f,%.0f  middle_tip %.0f,%.0f\n", lm.image[WRIST].x, lm.image[WRIST].y,
                            lm.image[MIDDLE_TIP].x, lm.image[MIDDLE_TIP].y);
            } else {
                std::printf("    hand: not confirmed\n");
            }
        }
        auto faces = face.detect(img, 0.7f, 4);
        std::printf("  faces: %zu (%.1f ms)\n", faces.size(), face.lastMs());
        for (const auto& f : faces) {
            Embedding e;
            const bool ok = emb.embed(img, f, e);
            const HeadPose hp = estimateHeadPose(f);
            std::printf("    score %.2f box %.0f,%.0f %.0fx%.0f yaw %.2f pitch %.2f embed=%d (%.1f ms)\n", f.score, f.box.x,
                        f.box.y, f.box.w, f.box.h, hp.yaw, hp.pitch, ok, emb.lastMs());
            if (ok) {
                for (size_t k = 0; k < embeddings.size(); ++k) std::printf("      cos vs #%zu: %.3f\n", k, cosine(e, embeddings[k]));
                embeddings.push_back(e);
            }
        }
    }
    return 0;
}
