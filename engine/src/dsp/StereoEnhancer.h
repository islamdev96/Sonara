// StereoEnhancer.h - Mid/Side stereo widening + light ambience. Drives the
// "Surround" and "Ambience" controls. Mono-safe (collapses cleanly).
#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

namespace wab::dsp {

class StereoEnhancer {
public:
    void prepare(double sampleRate) noexcept {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        const int n = std::max<int>(1, static_cast<int>(0.018 * sr_)); // up to 18 ms
        ambBufL_.assign(n, 0.0f);
        ambBufR_.assign(n, 0.0f);
        ambLen_ = n; ambPos_ = 0;
    }

    void setWidth(float surround) noexcept { width_ = 1.0f + std::clamp(surround,0.0f,1.0f) * 1.2f; }
    void setAmbience(float amb) noexcept { ambMix_ = std::clamp(amb,0.0f,1.0f) * 0.35f; }

    // Process one stereo frame in place.
    inline void processStereo(float& l, float& r) noexcept {
        // Mid/Side widening.
        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r) * width_;
        float outL = mid + side;
        float outR = mid - side;

        // Cross-fed delayed ambience (cheap Haas-style spaciousness).
        if (ambMix_ > 0.0001f) {
            const int readPos = ambPos_;
            const float dl = ambBufL_[readPos];
            const float dr = ambBufR_[readPos];
            ambBufL_[ambPos_] = outL;
            ambBufR_[ambPos_] = outR;
            if (++ambPos_ >= ambLen_) ambPos_ = 0;
            outL += ambMix_ * dr;
            outR += ambMix_ * dl;
        }
        l = outL; r = outR;
    }

    void reset() noexcept {
        std::fill(ambBufL_.begin(), ambBufL_.end(), 0.0f);
        std::fill(ambBufR_.begin(), ambBufR_.end(), 0.0f);
        ambPos_ = 0;
    }

private:
    double sr_ = 48000.0;
    float  width_ = 1.0f;
    float  ambMix_ = 0.0f;
    std::vector<float> ambBufL_, ambBufR_;
    int ambLen_ = 1, ambPos_ = 0;
};

} // namespace wab::dsp
