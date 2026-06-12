/**
 * Fast Fourier Transform (Cooley-Tukey Radix-2).
 * re and im must be Float32Array of length N (power of 2).
 */
export function fft(re: Float32Array, im: Float32Array): void {
  const n = re.length;
  if (n <= 1) return;

  const reEven = new Float32Array(n / 2);
  const imEven = new Float32Array(n / 2);
  const reOdd = new Float32Array(n / 2);
  const imOdd = new Float32Array(n / 2);

  for (let i = 0; i < n / 2; i++) {
    reEven[i] = re[2 * i];
    imEven[i] = im[2 * i];
    reOdd[i] = re[2 * i + 1];
    imOdd[i] = im[2 * i + 1];
  }

  fft(reEven, imEven);
  fft(reOdd, imOdd);

  for (let i = 0; i < n / 2; i++) {
    const angle = (-2 * Math.PI * i) / n;
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);

    const tRe = reOdd[i] * cos - imOdd[i] * sin;
    const tIm = reOdd[i] * sin + imOdd[i] * cos;

    re[i] = reEven[i] + tRe;
    im[i] = imEven[i] + tIm;
    re[i + n / 2] = reEven[i] - tRe;
    im[i + n / 2] = imEven[i] - tIm;
  }
}

/**
 * Computes frequency spectrum magnitudes for N raw samples.
 * Returns a Float32Array of size N/2 containing normalized magnitudes (0..1).
 */
export function computeSpectrum(samples: number[] | Float32Array): Float32Array {
  const n = samples.length;
  const re = new Float32Array(n);
  const im = new Float32Array(n);
  
  // Apply a Hanning window to prevent spectral leakage
  for (let i = 0; i < n; i++) {
    const windowValue = 0.5 * (1.0 - Math.cos((2 * Math.PI * i) / (n - 1)));
    re[i] = samples[i] * windowValue;
  }

  fft(re, im);

  const outLen = n / 2;
  const magnitudes = new Float32Array(outLen);
  
  for (let i = 0; i < outLen; i++) {
    const mag = Math.sqrt(re[i] * re[i] + im[i] * im[i]);
    // Normalize and scale magnitude (gain multiplier for visual dynamics)
    magnitudes[i] = Math.min(1.0, mag * 4.0 / n);
  }

  return magnitudes;
}
