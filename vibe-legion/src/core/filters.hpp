// Signal filters: One Euro (landmark jitter) and a plain EMA.
#pragma once

#include <cmath>

namespace vl {

// Casiez et al., "1€ Filter" (CHI 2012). Low lag on fast motion, smooth when still.
class OneEuro {
public:
    OneEuro(float minCutoff = 1.5f, float beta = 0.02f, float dCutoff = 1.0f)
        : minCutoff_(minCutoff), beta_(beta), dCutoff_(dCutoff) {}

    float filter(float x, float dtSec) {
        if (!init_ || dtSec <= 0.f) {
            init_ = true;
            x_ = x;
            dx_ = 0.f;
            return x;
        }
        const float dx = (x - x_) / dtSec;
        dx_ += alpha(dCutoff_, dtSec) * (dx - dx_);
        const float cutoff = minCutoff_ + beta_ * std::fabs(dx_);
        x_ += alpha(cutoff, dtSec) * (x - x_);
        return x_;
    }
    void reset() { init_ = false; }
    float value() const { return x_; }

private:
    static float alpha(float cutoff, float dt) {
        const float tau = 1.f / (2.f * 3.14159265f * cutoff);
        return 1.f / (1.f + tau / dt);
    }
    float minCutoff_, beta_, dCutoff_;
    float x_ = 0.f, dx_ = 0.f;
    bool init_ = false;
};

class Ema {
public:
    explicit Ema(float alpha = 0.3f) : a_(alpha) {}
    float update(float x) {
        v_ = init_ ? v_ + a_ * (x - v_) : x;
        init_ = true;
        return v_;
    }
    void reset() { init_ = false; }
    bool valid() const { return init_; }
    float value() const { return v_; }
    void setAlpha(float a) { a_ = a; }

private:
    float a_, v_ = 0.f;
    bool init_ = false;
};

}  // namespace vl
