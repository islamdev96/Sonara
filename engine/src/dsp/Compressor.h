// Compressor.h - Feed-forward dynamics compressor with peak detection and
// smoothed gain envelope. Used for the "Dynamic Boost" (loudness) feature so
// quiet content is brought up without the loud parts distorting.
#pragma once
#include <cmath>
#include <algorithm>

namespace wab::dsp {

class Compressor {
public:
    void prepare(double sampleRate) noexcept {
        sr_ = sampleRate > 0 ? sampleRate : 48000.0;
        setTimes(5.0, 120.0);
        env_ = 0.0;
    }

    // amount: 0..1 maps to a gentle->aggressive compression curve.
    void setAmount(float amount) noexcept {
        amount = std::clamp(amount, 0.0f, 1.0f);
        // Threshold drops and ratio rises as amount increases.
        thresholdDb_ = -6.0 - 18.0 * amount;   // -6 dB down to -24 dB
        ratio_       = 1.0 + 7.0 * amount;      // 1:1 up to 8:1
        makeupDb_    = 0.5 * (-thresholdDb_) * (1.0 - 1.0 / ratio_) * amount;
        thresholdLin_ = dbToLin(thresholdDb_);              // cache for fast compare
        makeupLin_   = static_cast<float>(dbToLin(makeupDb_)); // cache makeup gain
        active_      = amount > 0.0001f;
    }

    void setTimes(double attackMs, double releaseMs) noexcept {
        atk_ = std::exp(-1.0 / (0.001 * attackMs  * sr_));
        rel_ = std::exp(-1.0 / (0.001 * releaseMs * sr_));
    }

    // Returns linear gain to apply to the current frame (computed from the
    // detector signal `detect`, typically max(|L|,|R|)).
    inline float computeGain(float detect) noexcept {
        if (!active_) return 1.0f;
        const double x = std::fabs(static_cast<double>(detect));
        const double coeff = (x > env_) ? atk_ : rel_;
        env_ = coeff * env_ + (1.0 - coeff) * x;
        // Below threshold the gain reduction is zero, so skip the per-sample
        // log10/pow and just apply the cached makeup gain (the common case).
        if (env_ <= thresholdLin_) return makeupLin_;
        const double envDb = 20.0 * std::log10(env_ + 1e-9);
        const double overDb = envDb - thresholdDb_;
        const double grDb = overDb - overDb / ratio_; // gain reduction (positive)
        return static_cast<float>(dbToLin(makeupDb_ - grDb));
    }

    void reset() noexcept { env_ = 0.0; }

private:
    static inline double dbToLin(double db) noexcept { return std::pow(10.0, db / 20.0); }

    double sr_ = 48000.0;
    double atk_ = 0.0, rel_ = 0.0;
    double env_ = 0.0;
    double thresholdDb_ = -12.0;
    double ratio_ = 2.0;
    double makeupDb_ = 0.0;
    double thresholdLin_ = 0.5;   // linear form of thresholdDb_ (fast compare)
    float  makeupLin_ = 1.0f;     // cached linear makeup gain
    bool   active_ = false;
};

} // namespace wab::dsp
