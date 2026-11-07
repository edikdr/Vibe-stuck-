#include "core/detection.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace vl {

std::vector<int> nms(const std::vector<RectF>& boxes, const std::vector<float>& scores, float iouThreshold,
                     int maxResults) {
    std::vector<int> order(boxes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return scores[a] > scores[b]; });
    std::vector<int> keep;
    for (int i : order) {
        bool ok = true;
        for (int k : keep)
            if (iou(boxes[i], boxes[k]) > iouThreshold) { ok = false; break; }
        if (ok) {
            keep.push_back(i);
            if (static_cast<int>(keep.size()) >= maxResults) break;
        }
    }
    return keep;
}

// ------------------------------------------------------------------ palms

PalmDecoder::PalmDecoder() {
    // SSD anchors of the MediaPipe palm model: stride 8 -> 24x24 grid x2 anchors,
    // strides 16,16,16 merged -> 12x12 grid x6 anchors. All anchors are unit-size,
    // so only centres matter. Verified against the opencv_zoo anchor table.
    anchors_.reserve(kAnchors);
    const int grids[2] = {24, 12}, per[2] = {2, 6};
    for (int l = 0; l < 2; ++l)
        for (int y = 0; y < grids[l]; ++y)
            for (int x = 0; x < grids[l]; ++x)
                for (int k = 0; k < per[l]; ++k)
                    anchors_.push_back({(x + 0.5f) / grids[l], (y + 0.5f) / grids[l]});
}

std::vector<PalmDetection> PalmDecoder::decode(const float* boxes, const float* scores, const Affine& t,
                                               float scoreThreshold, float nmsThreshold, int maxResults) const {
    std::vector<PalmDetection> cand;
    std::vector<RectF> rects;
    std::vector<float> sc;
    const float logitThr = std::log(scoreThreshold / (1.f - scoreThreshold));
    for (int i = 0; i < kAnchors; ++i) {
        const float logit = std::clamp(scores[i], -100.f, 100.f);
        if (logit < logitThr) continue;
        const float* b = boxes + static_cast<size_t>(i) * 18;
        const Vec2 a = anchors_[i] * static_cast<float>(kInput);
        const Vec2 c{b[0] + a.x, b[1] + a.y};
        const Vec2 p1 = t.apply({c.x - b[2] * 0.5f, c.y - b[3] * 0.5f});
        const Vec2 p2 = t.apply({c.x + b[2] * 0.5f, c.y + b[3] * 0.5f});
        PalmDetection d;
        d.box = {std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::fabs(p2.x - p1.x), std::fabs(p2.y - p1.y)};
        for (int k = 0; k < 7; ++k) d.keypoints[k] = t.apply({b[4 + 2 * k] + a.x, b[5 + 2 * k] + a.y});
        d.score = sigmoid(logit);
        rects.push_back(d.box);
        sc.push_back(d.score);
        cand.push_back(d);
    }
    std::vector<PalmDetection> out;
    for (int k : nms(rects, sc, nmsThreshold, maxResults)) out.push_back(cand[k]);
    return out;
}

namespace {

inline Vec2 rotate(Vec2 p, float r) {
    const float c = std::cos(r), s = std::sin(r);
    return {c * p.x - s * p.y, s * p.x + c * p.y};
}

inline float uprightRotation(Vec2 wrist, Vec2 towardFingers) {
    return normalizeAngle(kPi * 0.5f - std::atan2(-(towardFingers.y - wrist.y), towardFingers.x - wrist.x));
}

// Common: rotate points into the upright frame around c0, take their bbox,
// shift along the hand axis and enlarge to a square.
template <typename It>
RotatedRect uprightBox(It begin, It end, float rotation, float shiftY, float enlarge) {
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (It it = begin; it != end; ++it) {
        minx = std::min(minx, it->x); maxx = std::max(maxx, it->x);
        miny = std::min(miny, it->y); maxy = std::max(maxy, it->y);
    }
    const Vec2 c0{(minx + maxx) * 0.5f, (miny + maxy) * 0.5f};
    minx = miny = 1e9f;
    maxx = maxy = -1e9f;
    for (It it = begin; it != end; ++it) {
        Vec2 q = rotate(Vec2{it->x, it->y} - c0, -rotation);
        minx = std::min(minx, q.x); maxx = std::max(maxx, q.x);
        miny = std::min(miny, q.y); maxy = std::max(maxy, q.y);
    }
    const float w = maxx - minx, h = maxy - miny;
    Vec2 cq{(minx + maxx) * 0.5f, (miny + maxy) * 0.5f + shiftY * h};
    RotatedRect r;
    r.center = c0 + rotate(cq, rotation);
    r.size = std::max(w, h) * enlarge;
    r.rotation = rotation;
    return r;
}

}  // namespace

RotatedRect handRoiFromPalm(const PalmDetection& palm) {
    const float rot = uprightRotation(palm.keypoints[0], palm.keypoints[2]);
    // opencv_zoo recipe for this model: bbox of the 7 rotated palm keypoints,
    // shifted 0.4 toward the fingers, enlarged 3x, squared.
    return uprightBox(palm.keypoints.begin(), palm.keypoints.end(), rot, -0.4f, 3.0f);
}

void decodeHandLandmarks(const float* lm, const float* world, float presence, float handedness,
                         const RotatedRect& roi, int inputSize, HandLandmarks& out) {
    const Affine m = roiToImage(roi, static_cast<float>(inputSize));
    const float zScale = roi.size / inputSize;
    for (int i = 0; i < kHandLandmarks; ++i) {
        const Vec2 p = m.apply({lm[3 * i], lm[3 * i + 1]});
        out.image[i] = {p.x, p.y, lm[3 * i + 2] * zScale};
        const Vec2 w = rotate({world[3 * i], world[3 * i + 1]}, roi.rotation);
        out.world[i] = {w.x, w.y, world[3 * i + 2]};
    }
    out.presence = presence;
    out.rightness = handedness;
}

RotatedRect handRoiFromLandmarks(const HandLandmarks& h) {
    static constexpr int subset[] = {0, 1, 2, 3, 5, 6, 9, 10, 13, 14, 17, 18};
    std::array<Vec2, 12> pts;
    for (size_t i = 0; i < pts.size(); ++i) pts[i] = h.image[subset[i]].xy();
    const Vec2 wrist = h.image[WRIST].xy();
    Vec2 mid = (h.image[INDEX_MCP].xy() + h.image[RING_MCP].xy()) * 0.5f;
    mid = (mid + h.image[MIDDLE_MCP].xy()) * 0.5f;
    const float rot = uprightRotation(wrist, mid);
    return uprightBox(pts.begin(), pts.end(), rot, -0.1f, 2.0f);
}

// ------------------------------------------------------------------ faces

std::vector<FaceDetection> decodeYuNet(const std::array<YuNetLevel, 3>& levels, int padW, int padH,
                                       const Affine& t, float scoreThreshold, float nmsThreshold, int maxResults) {
    std::vector<FaceDetection> cand;
    std::vector<RectF> rects;
    std::vector<float> sc;
    for (const auto& L : levels) {
        if (!L.cls || !L.obj || !L.bbox || !L.kps) continue;
        const int cols = padW / L.stride, rows = padH / L.stride;
        const float s = static_cast<float>(L.stride);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const size_t idx = static_cast<size_t>(r) * cols + c;
                const float cls = std::clamp(L.cls[idx], 0.f, 1.f), obj = std::clamp(L.obj[idx], 0.f, 1.f);
                const float score = std::sqrt(cls * obj);
                if (score < scoreThreshold) continue;
                const float* b = L.bbox + idx * 4;
                const float cx = (c + b[0]) * s, cy = (r + b[1]) * s;
                const float w = std::exp(b[2]) * s, h = std::exp(b[3]) * s;
                FaceDetection f;
                const Vec2 p1 = t.apply({cx - w * 0.5f, cy - h * 0.5f}), p2 = t.apply({cx + w * 0.5f, cy + h * 0.5f});
                f.box = {p1.x, p1.y, p2.x - p1.x, p2.y - p1.y};
                const float* k = L.kps + idx * 10;
                for (int n = 0; n < 5; ++n) f.landmarks[n] = t.apply({(k[2 * n] + c) * s, (k[2 * n + 1] + r) * s});
                f.score = score;
                rects.push_back(f.box);
                sc.push_back(score);
                cand.push_back(f);
            }
        }
    }
    std::vector<FaceDetection> out;
    for (int k : nms(rects, sc, nmsThreshold, maxResults)) out.push_back(cand[k]);
    return out;
}

Affine sfaceAlignment(const FaceDetection& f) {
    // ArcFace/SFace canonical 5-point template for 112x112 crops.
    static const std::array<Vec2, 5> dst = {{{38.2946f, 51.6963f},
                                             {73.5318f, 51.5014f},
                                             {56.0252f, 71.7366f},
                                             {41.5493f, 92.3655f},
                                             {70.7299f, 92.2041f}}};
    return estimateSimilarity(f.landmarks, dst);
}

HeadPose estimateHeadPose(const FaceDetection& f) {
    const Vec2 re = f.landmarks[0], le = f.landmarks[1], nose = f.landmarks[2];
    const Vec2 mouth = (f.landmarks[3] + f.landmarks[4]) * 0.5f;
    const Vec2 eyes = (re + le) * 0.5f;
    const Vec2 ax = le - re;
    const float d = length(ax);
    HeadPose hp;
    if (d < 1e-3f) return hp;
    const Vec2 u = ax / d, v{-u.y, u.x};  // u along eyes, v "down" the face
    const float offX = dot(nose - eyes, u) / d;  // ~0 frontal, ~+-0.5 profile
    const float eyeMouth = dot(mouth - eyes, v);
    const float t = eyeMouth > 1e-3f ? dot(nose - eyes, v) / eyeMouth : 0.55f;
    hp.yaw = std::clamp(offX * 2.2f, -1.f, 1.f);
    hp.pitch = std::clamp((t - 0.55f) * 3.f, -1.f, 1.f);
    return hp;
}

}  // namespace vl
