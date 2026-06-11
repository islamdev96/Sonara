// dsp_test.cpp - Portable unit + integration tests for the self-contained
// BoostEngine. Compiles and runs on any platform (used in CI on Linux).
// Verifies: stability (no NaN/Inf), bypass transparency, that the limiter keeps
// output under the ceiling even with extreme boost, and that boost raises RMS.
#include "../src/dsp/BoostEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>

using namespace wab::dsp;

static int g_fail = 0;
static void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++g_fail;
}

static double rms(const std::vector<float>& v) {
    double s = 0; for (float x : v) s += (double)x * x; return std::sqrt(s / v.size());
}
static bool finiteAll(const std::vector<float>& v) {
    for (float x : v) {
        if (!std::isfinite(x)) return false;
    }
    return true;
}
static float peak(const std::vector<float>& v) {
    float p = 0; for (float x : v) p = std::max(p, std::fabs(x)); return p;
}

static std::vector<float> makeSine(int frames, int ch, double sr, double hz, float amp) {
    std::vector<float> buf((size_t)frames * ch);
    for (int n = 0; n < frames; ++n) {
        float s = amp * (float)std::sin(2.0 * WAB_PI * hz * n / sr);
        for (int c = 0; c < ch; ++c) buf[(size_t)n * ch + c] = s;
    }
    return buf;
}

int main() {
    const double sr = 48000.0; const int ch = 2; const int frames = 48000;
    std::printf("Sonara - DSP engine tests (sr=%.0f ch=%d)\n", sr, ch);

    // 1) Bypass must be bit-transparent.
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.enabled = 0; p.seq = 1; eng.updateParameters(p);
        auto in = makeSine(frames, ch, sr, 440.0, 0.5f);
        auto out = in; eng.process(out.data(), frames);
        bool same = true; for (size_t i = 0; i < in.size(); ++i) if (in[i] != out[i]) { same = false; break; }
        check(same, "bypass is transparent");
    }

    // 2) Stability with an aggressive everything-on preset + white noise.
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.seq = 2; p.preampDb = 12.0f; p.bass = 1.0f; p.clarity = 1.0f;
        p.dynamic = 1.0f; p.surround = 1.0f; p.ambience = 1.0f;
        for (int b = 0; b < kNumEqBands; ++b) p.eqGainsDb[b] = (b % 2 ? 6.0f : -4.0f);
        eng.updateParameters(p);
        std::mt19937 rng(1234); std::uniform_real_distribution<float> d(-0.7f, 0.7f);
        std::vector<float> buf((size_t)frames * ch); for (auto& x : buf) x = d(rng);
        eng.process(buf.data(), frames);
        check(finiteAll(buf), "no NaN/Inf under extreme settings");
    }

    // 3) Limiter keeps the ceiling even with +30 dB of insane boost.
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.seq = 3; p.preampDb = 30.0f; p.limiterOn = 1; p.limiterCeilingDb = -1.0f;
        eng.updateParameters(p);
        auto buf = makeSine(frames, ch, sr, 1000.0, 0.5f);
        eng.process(buf.data(), frames);
        const float ceil = std::pow(10.0f, -1.0f / 20.0f);
        check(finiteAll(buf), "limiter output finite");
        check(peak(buf) <= ceil + 1e-3f, "limiter holds output under ceiling");
    }

    // 4) Boost (with limiter off) actually increases loudness for quiet input.
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.seq = 4; p.preampDb = 12.0f; p.limiterOn = 0; eng.updateParameters(p);
        auto in = makeSine(frames, ch, sr, 440.0, 0.05f);
        auto warm = in; eng.process(warm.data(), frames); // let gain smoothing settle
        auto out = in; eng.process(out.data(), frames);
        double gainDb = 20.0 * std::log10(rms(out) / rms(in));
        check(std::fabs(gainDb - 12.0) < 0.5, "preamp gain accurate (~+12 dB)");
    }

    // 5) Mono (1ch) and 6-channel (5.1) must not crash and stay finite.
    {
        for (int c : {1, 6}) {
            BoostEngine eng; eng.prepare(sr, c);
            Parameters p; p.seq = 5; p.preampDb = 6.0f; p.bass = 0.5f; p.surround = 0.5f;
            eng.updateParameters(p);
            auto buf = makeSine(frames, c, sr, 440.0, 0.3f);
            eng.process(buf.data(), frames);
            char name[64]; std::snprintf(name, sizeof(name), "%d-channel processing finite", c);
            check(finiteAll(buf), name);
        }
    }

    // 6) With everything flat (0 dB preamp, flat EQ, no effects, limiter off)
    //    the enabled chain must still be bit-transparent (EQ-skip correctness).
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.seq = 6; p.preampDb = 0.0f; p.limiterOn = 0; eng.updateParameters(p);
        auto in = makeSine(frames, ch, sr, 440.0, 0.5f);
        auto out = in; eng.process(out.data(), frames);
        bool same = true; for (size_t i = 0; i < in.size(); ++i) if (in[i] != out[i]) { same = false; break; }
        check(same, "flat enabled chain is transparent (EQ skip)");
    }

    // 7) A loud burst followed by silence must stay finite and decay toward
    //    zero - i.e. no denormal/instability buildup in the tail.
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p; p.seq = 7; p.preampDb = 9.0f; p.bass = 1.0f; p.clarity = 1.0f;
        p.dynamic = 1.0f; p.surround = 1.0f; p.ambience = 1.0f; eng.updateParameters(p);
        std::vector<float> buf((size_t)frames * ch, 0.0f);
        std::mt19937 rng(99); std::uniform_real_distribution<float> d(-0.8f, 0.8f);
        const int loud = (int)(0.1 * sr); // first 100 ms loud, rest hard silence
        for (int n = 0; n < loud; ++n) for (int c = 0; c < ch; ++c) buf[(size_t)n * ch + c] = d(rng);
        eng.process(buf.data(), frames);
        check(finiteAll(buf), "tail stays finite after silence");
        const size_t tailLen = (size_t)(0.2 * sr) * ch; // examine last 200 ms
        std::vector<float> tail(buf.end() - tailLen, buf.end());
        check(peak(tail) < 1e-2f, "tail decays toward silence");
    }

    // 8) Parameter smoothing: a sudden preamp jump must ramp (no click/zipper).
    {
        BoostEngine eng; eng.prepare(sr, ch);
        Parameters p0; p0.seq = 80; p0.preampDb = 0.0f; p0.limiterOn = 0; eng.updateParameters(p0);
        Parameters p1 = p0; p1.seq = 81; p1.preampDb = 12.0f; eng.updateParameters(p1);
        std::vector<float> buf((size_t)frames * ch, 0.25f); // constant input -> output tracks gain
        eng.process(buf.data(), frames);
        const float target = 0.25f * std::pow(10.0f, 12.0f / 20.0f);
        bool gentleStart = std::fabs(buf[0] - 0.25f) < 0.05f;      // no instant jump
        bool converged   = std::fabs(buf[(size_t)(frames - 1) * ch] - target) < 0.02f;
        float maxStep = 0.0f;
        for (int n = 1; n < frames; ++n)
            maxStep = std::max(maxStep, std::fabs(buf[(size_t)n * ch] - buf[(size_t)(n - 1) * ch]));
        check(gentleStart && converged, "preamp change ramps smoothly (no zipper)");
        check(maxStep < 0.01f, "no abrupt per-sample gain step");
    }

    std::printf(g_fail == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
