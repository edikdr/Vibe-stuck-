// Tiny image containers and the handful of resampling ops the pipeline needs.
// Deliberately no OpenCV: it would cost ~60 MB of DLLs and tens of MB of RAM for
// three functions (colour convert, letterbox, rotated crop).
#pragma once

#include <cstdint>
#include <vector>

#include "core/geometry.hpp"

namespace vl {

// Interleaved 8-bit image, 1 (gray) or 3 (RGB) channels.
struct Image {
    int width = 0, height = 0, channels = 3;
    std::vector<uint8_t> data;

    Image() = default;
    Image(int w, int h, int c) { reset(w, h, c); }
    void reset(int w, int h, int c) {
        width = w; height = h; channels = c;
        data.assign(static_cast<size_t>(w) * h * c, 0);
    }
    bool empty() const { return width <= 0 || height <= 0 || data.empty(); }
    int stride() const { return width * channels; }
    uint8_t* row(int y) { return data.data() + static_cast<size_t>(y) * stride(); }
    const uint8_t* row(int y) const { return data.data() + static_cast<size_t>(y) * stride(); }
};

enum class PixelFormat { NV12, YUY2, Gray8, Gray16, RGB24, BGRA32 };

// Converts a raw camera frame to RGB (or gray replicated to RGB) and optionally
// downsamples by an integer factor on the fly (cheap 2x2/4x4 box filter).
// `stride` is bytes per row of the first plane. For NV12 the UV plane follows
// immediately after `height` rows unless uvOffset is given.
void convertFrame(const uint8_t* src, int width, int height, int stride, PixelFormat fmt,
                  int downscale, bool mirror, Image& dstRgb, size_t uvOffset = 0);

// Tensor layouts the models want.
enum class TensorLayout { NHWC, NCHW };

struct TensorSpec {
    int width = 0, height = 0;
    TensorLayout layout = TensorLayout::NHWC;
    bool bgr = false;     // channel order
    float scale = 1.f;    // value = pixel * scale + bias
    float bias = 0.f;
};

// Letterbox `src` into a WxH tensor (aspect preserved, centred, zero padding).
// Returns the transform tensor-pixels -> source-pixels.
Affine letterboxToTensor(const Image& src, const TensorSpec& spec, float* out);

// Top-left aligned resize that preserves aspect (YuNet style: scale then pad right/bottom).
Affine fitTopLeftToTensor(const Image& src, const TensorSpec& spec, float* out);

// Samples `src` through an affine map (tensor pixel -> source pixel), bilinear,
// zero outside. Used for rotated hand crops and face alignment.
void warpToTensor(const Image& src, const Affine& tensorToSrc, const TensorSpec& spec, float* out);

// Same but into an 8-bit RGB image (for previews / thumbnails).
void warpToImage(const Image& src, const Affine& dstToSrc, Image& dst);

// Plain bilinear resize (thumbnail for UI preview).
void resizeBilinear(const Image& src, Image& dst, int w, int h);

// Fast mean luminance (0..1) on a sparse grid.
float meanLuma(const Image& img, int step = 8);

}  // namespace vl
