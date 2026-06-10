// Limiter.h - Lookahead brickwall peak limiter. This is what makes the boost
// safe: no matter how much gain the user dials in, the output never exceeds
// the ceiling, so you get loudness without harsh digital clipping.
#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

namespace wab::dsp {

class Limiter {
public:
    void prepare(double sampleRate, int maxChannels) noexcept {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        maxCh_ = std::max(1, maxChannels);
        lookaheadSamples_ = std::max<int>(1, static_cast<int>(0.0015 * sr_)); // 1.5 ms
        delay_.assign(static_cast<size_t>(lookaheadSamples_) * maxCh_, 0.0f);
        delayPos_ = 0;
        env_ = 1.0;
        rel_ = std::exp(-1.0 / (0.050 * sr_)); // 50 ms release
        setCeiling(-0.3f);
    }

    void setCeiling(float ceilingDb) noexcept {
        ceiling_ = std::pow(10.0, ceilingDb / 20.0);
    }

    // Process one interleaved frame in place. `ch` = channel count this block.
    inline void processFrame(float* frame, int ch) noexcept {
        // Detect the peak across channels for this frame.
        double peak = 0.0;
        for (int c = 0; c < ch; ++c) peak = std::max(peak, std::fabs((double)frame[c]));

        // Target gain needed to keep this peak under the ceiling.
        double target = (peak > ceiling_) ? (ceiling_ / (peak + 1e-12)) : 1.0;
        // Attack instantly (we have lookahead), release smoothly.
        if (target < env_) env_ = target;            // clamp down immediately
        else env_ = rel_ * env_ + (1.0 - rel_) * target;

        // Pull the delayed frame, apply the (smoothed) gain.
        const size_t base = static_cast<size_t>(delayPos_) * maxCh_;
        for (int c = 0; c < ch; ++c) {
            const float delayed = delay_[base + c];
            delay_[base + c] = frame[c];
            float out = static_cast<float>(delayed * env_);
            // final safety hard clip exactly at ceiling
            if (out > (float)ceiling_) out = (float)ceiling_;
            else if (out < -(float)ceiling_) out = -(float)ceiling_;
            frame[c] = out;
        }
        if (++delayPos_ >= lookaheadSamples_) delayPos_ = 0;
    }

    void reset() noexcept {
        std::fill(delay_.begin(), delay_.end(), 0.0f);
        delayPos_ = 0; env_ = 1.0;
    }

private:
    double sr_ = 48000.0;
    int    maxCh_ = 2;
    int    lookaheadSamples_ = 72;
    int    delayPos_ = 0;
    double env_ = 1.0;
    double rel_ = 0.0;
    double ceiling_ = 0.97;
    std::vector<float> delay_;
};

} // namespace wab::dsp
