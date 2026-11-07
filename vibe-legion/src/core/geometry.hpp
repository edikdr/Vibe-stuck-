// Small 2D/3D math helpers shared by detectors, ROI tracking and gestures.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace vl {

constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    float x = 0, y = 0;
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec2 xy() const { return {x, y}; }
};

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 a) { return std::sqrt(dot(a, a)); }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

// Angle at vertex b of triangle a-b-c, radians in [0, pi].
inline float jointAngle(Vec3 a, Vec3 b, Vec3 c) {
    Vec3 u = a - b, v = c - b;
    float lu = length(u), lv = length(v);
    if (lu < 1e-6f || lv < 1e-6f) return kPi;
    float cs = std::clamp(dot(u, v) / (lu * lv), -1.f, 1.f);
    return std::acos(cs);
}

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }
    float area() const { return std::max(0.f, w) * std::max(0.f, h); }
    bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
};

inline float iou(const RectF& a, const RectF& b) {
    float x1 = std::max(a.x, b.x), y1 = std::max(a.y, b.y);
    float x2 = std::min(a.right(), b.right()), y2 = std::min(a.bottom(), b.bottom());
    float inter = std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1);
    float uni = a.area() + b.area() - inter;
    return uni > 0 ? inter / uni : 0.f;
}

// 2x3 affine transform: [x'] = [a b c] [x y 1]^T, [y'] = [d e f] [x y 1]^T
struct Affine {
    float a = 1, b = 0, c = 0, d = 0, e = 1, f = 0;

    Vec2 apply(Vec2 p) const { return {a * p.x + b * p.y + c, d * p.x + e * p.y + f}; }
    Vec2 applyLinear(Vec2 p) const { return {a * p.x + b * p.y, d * p.x + e * p.y}; }

    Affine inverse() const {
        float det = a * e - b * d;
        if (std::fabs(det) < 1e-12f) return {};
        float id = 1.f / det;
        Affine r;
        r.a = e * id;
        r.b = -b * id;
        r.d = -d * id;
        r.e = a * id;
        r.c = -(r.a * c + r.b * f);
        r.f = -(r.d * c + r.e * f);
        return r;
    }

    // this ∘ o (apply o first, then this)
    Affine compose(const Affine& o) const {
        Affine r;
        r.a = a * o.a + b * o.d;
        r.b = a * o.b + b * o.e;
        r.c = a * o.c + b * o.f + c;
        r.d = d * o.a + e * o.d;
        r.e = d * o.b + e * o.e;
        r.f = d * o.c + e * o.f + f;
        return r;
    }
};

// Rotated square region of interest in image pixels.
struct RotatedRect {
    Vec2 center;
    float size = 0;      // side length, pixels
    float rotation = 0;  // radians, positive = clockwise in image coords (y down)
};

// Affine mapping ROI-space pixels (0..outSize) to image pixels.
inline Affine roiToImage(const RotatedRect& r, float outSize) {
    float s = r.size / outSize;
    float cs = std::cos(r.rotation) * s, sn = std::sin(r.rotation) * s;
    // p_img = R * S * (p_roi - outSize/2) + center
    Affine m;
    m.a = cs; m.b = -sn;
    m.d = sn; m.e = cs;
    float h = outSize * 0.5f;
    m.c = r.center.x - (cs * h - sn * h);
    m.f = r.center.y - (sn * h + cs * h);
    return m;
}

inline float normalizeAngle(float a) {
    return a - 2.f * kPi * std::floor((a + kPi) / (2.f * kPi));
}

// Least-squares similarity transform (rotation+uniform scale+translation, no reflection)
// mapping src[i] -> dst[i]. Closed form for 2D (equivalent to Umeyama without reflection).
template <size_t N>
Affine estimateSimilarity(const std::array<Vec2, N>& src, const std::array<Vec2, N>& dst) {
    Vec2 ms, md;
    for (size_t i = 0; i < N; ++i) { ms += src[i]; md += dst[i]; }
    ms = ms / static_cast<float>(N);
    md = md / static_cast<float>(N);
    double sxx = 0, sxy = 0, ss = 0;
    for (size_t i = 0; i < N; ++i) {
        Vec2 p = src[i] - ms, q = dst[i] - md;
        sxx += p.x * q.x + p.y * q.y;  // a component
        sxy += p.x * q.y - p.y * q.x;  // b component
        ss += p.x * p.x + p.y * p.y;
    }
    if (ss < 1e-9) return {};
    float A = static_cast<float>(sxx / ss), B = static_cast<float>(sxy / ss);
    Affine m;
    m.a = A; m.b = -B;
    m.d = B; m.e = A;
    m.c = md.x - (A * ms.x - B * ms.y);
    m.f = md.y - (B * ms.x + A * ms.y);
    return m;
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace vl
