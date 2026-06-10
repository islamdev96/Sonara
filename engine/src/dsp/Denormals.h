// Denormals.h - RAII guard that flushes subnormal (denormal) floats to zero on
// the audio thread. When a filter/envelope tail decays toward silence it can
// produce subnormal values; on x86 these are handled in microcode and can be
// 10-100x slower, causing audible CPU spikes / glitches. Setting the SSE
// FTZ (flush-to-zero) + DAZ (denormals-are-zero) flags makes that cost vanish
// with zero per-sample overhead. No-op on architectures without SSE.
#pragma once

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #include <xmmintrin.h>
  #include <pmmintrin.h>
  #define WAB_HAS_SSE_DENORMAL 1
#endif

namespace wab::dsp {

class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#ifdef WAB_HAS_SSE_DENORMAL
        prev_ = _mm_getcsr();
        // bit 15 = FTZ, bit 6 = DAZ
        _mm_setcsr(prev_ | 0x8040u);
#endif
    }
    ~ScopedNoDenormals() noexcept {
#ifdef WAB_HAS_SSE_DENORMAL
        _mm_setcsr(prev_);
#endif
    }

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

#ifdef WAB_HAS_SSE_DENORMAL
private:
    unsigned int prev_ = 0;
#endif
};

} // namespace wab::dsp
