// Biquad.h - RBJ cookbook biquad filter (Transposed Direct Form II).
// Portable, real-time safe, no allocations in the audio path.
// Part of the Sonara self-contained DSP engine.
#pragma once
#include <cmath>

namespace wab::dsp {

#ifndef WAB_PI
#define WAB_PI 3.14159265358979323846
#endif

class Biquad {
public:
    // Coefficients (a0 normalized to 1).
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    // State (per-channel) - Transposed Direct Form II.
    double z1 = 0.0, z2 = 0.0;

    inline float process(float x) noexcept {
        const double in = static_cast<double>(x);
        const double y = b0 * in + z1;
        z1 = b1 * in - a1 * y + z2;
        z2 = b2 * in - a2 * y;
        return static_cast<float>(y);
    }

    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    // ----- Designers (set coefficients) -----
    // Peaking EQ. gainDb can be +/-. Q controls bandwidth.
    void setPeaking(double sampleRate, double freq, double Q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * WAB_PI * clampFreq(freq, sampleRate) / sampleRate;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / (2.0 * (Q <= 0.0001 ? 0.0001 : Q));
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * cw) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    void setLowShelf(double sampleRate, double freq, double Q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * WAB_PI * clampFreq(freq, sampleRate) / sampleRate;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / (2.0 * (Q <= 0.0001 ? 0.0001 : Q));
        const double twoSqrtAalpha = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha;
        b0 = (A * ((A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha)) / a0;
        b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cw)) / a0;
        b2 = (A * ((A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha)) / a0;
        a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cw)) / a0;
        a2 = ((A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha) / a0;
    }

    void setHighShelf(double sampleRate, double freq, double Q, double gainDb) noexcept {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * WAB_PI * clampFreq(freq, sampleRate) / sampleRate;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / (2.0 * (Q <= 0.0001 ? 0.0001 : Q));
        const double twoSqrtAalpha = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha;
        b0 = (A * ((A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha)) / a0;
        b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0;
        b2 = (A * ((A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha)) / a0;
        a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0;
        a2 = ((A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha) / a0;
    }

    void setLowPass(double sampleRate, double freq, double Q) noexcept {
        const double w0 = 2.0 * WAB_PI * clampFreq(freq, sampleRate) / sampleRate;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / (2.0 * (Q <= 0.0001 ? 0.0001 : Q));
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - cw) / 2.0) / a0;
        b1 = (1.0 - cw) / a0;
        b2 = ((1.0 - cw) / 2.0) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighPass(double sampleRate, double freq, double Q) noexcept {
        const double w0 = 2.0 * WAB_PI * clampFreq(freq, sampleRate) / sampleRate;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);
        const double alpha = sw / (2.0 * (Q <= 0.0001 ? 0.0001 : Q));
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) / 2.0) / a0;
        b1 = -(1.0 + cw) / a0;
        b2 = ((1.0 + cw) / 2.0) / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = (1.0 - alpha) / a0;
    }

private:
    static inline double clampFreq(double f, double sr) noexcept {
        const double nyq = sr * 0.49;
        if (f < 10.0) return 10.0;
        if (f > nyq) return nyq;
        return f;
    }
};

} // namespace wab::dsp
