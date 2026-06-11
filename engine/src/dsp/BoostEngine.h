// BoostEngine.h - The self-contained audio engine. This is what replaces the
// dependency on Equalizer APO. It owns the full DSP chain and processes
// interleaved 32-bit float audio for any channel count / sample rate.
//
// Signal chain:
//   input -> preamp(boost) -> 10-band EQ -> bass/clarity shelves
//         -> dynamic compressor -> stereo enhance/ambience
//         -> output trim -> brickwall limiter -> output
//
// Real-time safe: prepare() does all allocation; process() never allocates.
#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include "Parameters.h"
#include "Biquad.h"
#include "Compressor.h"
#include "Limiter.h"
#include "StereoEnhancer.h"
#include "Denormals.h"

namespace wab::dsp {

class BoostEngine {
public:
    // Call when the audio format is (re)negotiated.
    void prepare(double sampleRate, int channels) noexcept {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        ch_ = std::max(1, channels);
        eq_.assign(static_cast<size_t>(ch_) * kNumEqBands, Biquad{});
        bass_.assign(ch_, Biquad{});
        clarity_.assign(ch_, Biquad{});
        comp_.prepare(sr_);
        limiter_.prepare(sr_, ch_);
        widener_.prepare(sr_);
        // ~15 ms one-pole smoothing for the master gains: removes the audible
        // click / "zipper noise" when the user drags the boost or volume slider.
        gainSmoothCoef_ = 1.0f - std::exp(-1.0f / (0.015f * static_cast<float>(sr_)));
        lastSeq_ = 0xFFFFFFFFu;
        applyParams(current_, /*force=*/true);
    }

    // Copy in new parameters (typically read from shared memory). Cheap; only
    // recomputes filter coefficients when something actually changed.
    void updateParameters(const Parameters& p) noexcept {
        if (p.magic != kParamMagic) return;
        current_ = p;
        applyParams(current_, false);
    }

    // Process one block of interleaved float samples, in place.
    // frameCount = samples per channel. Buffer length = frameCount * ch_.
    inline void process(float* buffer, int frameCount) noexcept {
        if (!current_.enabled) return; // true passthrough when bypassed

        ScopedNoDenormals noDenormals; // flush subnormals -> no CPU spikes on quiet tails

        const int ch = ch_;
        const int* activeEq = activeEq_;
        const int nActiveEq = nActiveEq_;
        // Smoothed master gains: ramp per frame toward target to avoid zipper noise.
        const float smCoef  = gainSmoothCoef_;
        const float preTgt  = preampTarget_;
        const float outTgt  = outGainTarget_;
        float preamp  = preampLin_;
        float outGain = outGainLin_;
        const bool outGainActive = (outTgt != 1.0f) || (outGain != 1.0f);
        for (int n = 0; n < frameCount; ++n) {
            float* f = buffer + static_cast<size_t>(n) * ch;

            // Ramp master gains toward their targets (click-free slider moves).
            preamp += (preTgt - preamp) * smCoef;
            if (outGainActive) outGain += (outTgt - outGain) * smCoef;

            // 1) Preamp (the boost) + per-channel EQ + shelves.
            float detect = 0.0f;
            for (int c = 0; c < ch; ++c) {
                float s = f[c] * preamp;
                Biquad* bands = &eq_[static_cast<size_t>(c) * kNumEqBands];
                for (int i = 0; i < nActiveEq; ++i) s = bands[activeEq[i]].process(s);
                if (bassOn_)    s = bass_[c].process(s);
                if (clarityOn_) s = clarity_[c].process(s);
                // Soft pre-clip: tames extreme peaks from stacked bass/preamp
                // before they hit the compressor+limiter. Uses tanh saturation
                // above ~+6dB (2.0 linear) for a gentle knee instead of hard clip.
                if (std::fabs(s) > 2.0f) {
                    s = std::copysign(2.0f + std::tanh(std::fabs(s) - 2.0f), s);
                }
                f[c] = s;
                detect = std::max(detect, std::fabs(s));
            }

            // 2) Dynamic compressor (one gain for all channels keeps image stable).
            if (compOn_) {
                const float g = comp_.computeGain(detect);
                for (int c = 0; c < ch; ++c) f[c] *= g;
            }

            // 3) Stereo enhance / ambience (stereo only).
            if (ch >= 2 && stereoOn_) widener_.processStereo(f[0], f[1]);

            // 4) Output trim.
            if (outGainActive) for (int c = 0; c < ch; ++c) f[c] *= outGain;

            // 5) Brickwall limiter (safety - prevents any clipping).
            if (current_.limiterOn) limiter_.processFrame(f, ch);
        }
        // Persist smoothed gain state across process() calls.
        preampLin_  = preamp;
        outGainLin_ = outGain;
    }

    void reset() noexcept {
        for (auto& b : eq_) b.reset();
        for (auto& b : bass_) b.reset();
        for (auto& b : clarity_) b.reset();
        comp_.reset(); limiter_.reset(); widener_.reset();
    }

    const Parameters& parameters() const noexcept { return current_; }

private:
    static inline float dbToLin(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

    void applyParams(const Parameters& p, bool force) noexcept {
        if (!force && p.seq == lastSeq_) return;
        lastSeq_ = p.seq;

        preampTarget_  = dbToLin(p.preampDb);
        outGainTarget_ = dbToLin(p.outputGainDb);
        if (force) { preampLin_ = preampTarget_; outGainLin_ = outGainTarget_; }

        // 10-band parametric EQ (Q ~ 1.4 for musical overlap). A band left flat
        // (~0 dB) is an identity filter, so we skip it entirely on the audio
        // path instead of running a no-op biquad for every sample.
        nActiveEq_ = 0;
        for (int b = 0; b < kNumEqBands; ++b) {
            if (std::fabs(p.eqGainsDb[b]) > 0.01f) {
                activeEq_[nActiveEq_++] = b;
                for (int c = 0; c < ch_; ++c)
                    eq_[static_cast<size_t>(c) * kNumEqBands + b]
                        .setPeaking(sr_, kEqFrequencies[b], 1.4, p.eqGainsDb[b]);
            } else {
                for (int c = 0; c < ch_; ++c)
                    eq_[static_cast<size_t>(c) * kNumEqBands + b].reset();
            }
        }

        // Bass = low shelf @ 105 Hz up to +9 dB.
        bassOn_ = p.bass > 0.001f;
        if (bassOn_) for (int c = 0; c < ch_; ++c) bass_[c].setLowShelf(sr_, 105.0, 0.707, p.bass * 9.0f);

        // Clarity = high shelf @ 7.5 kHz up to +6 dB.
        clarityOn_ = p.clarity > 0.001f;
        if (clarityOn_) for (int c = 0; c < ch_; ++c) clarity_[c].setHighShelf(sr_, 7500.0, 0.707, p.clarity * 6.0f);

        compOn_ = p.dynamic > 0.001f;
        comp_.setAmount(p.dynamic);

        stereoOn_ = (p.surround > 0.001f) || (p.ambience > 0.001f);
        widener_.setWidth(p.surround);
        widener_.setAmbience(p.ambience);

        limiter_.setCeiling(p.limiterCeilingDb);
    }

    double sr_ = 48000.0;
    int    ch_ = 2;
    Parameters current_{};
    uint32_t lastSeq_ = 0xFFFFFFFFu;

    float preampLin_ = 1.0f, outGainLin_ = 1.0f;       // smoothed current gains
    float preampTarget_ = 1.0f, outGainTarget_ = 1.0f; // ramp targets
    float gainSmoothCoef_ = 1.0f;                      // per-frame ramp coefficient
    bool  bassOn_ = false, clarityOn_ = false, compOn_ = false, stereoOn_ = false;

    std::vector<Biquad> eq_;       // ch_ * kNumEqBands
    int   activeEq_[kNumEqBands] = {0}; // indices of non-flat EQ bands
    int   nActiveEq_ = 0;              // count of valid entries in activeEq_
    std::vector<Biquad> bass_;     // ch_
    std::vector<Biquad> clarity_;  // ch_
    Compressor    comp_;
    Limiter       limiter_;
    StereoEnhancer widener_;
};

} // namespace wab::dsp
