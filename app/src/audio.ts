// Pure helpers that map UI slider positions to the engine's parameter units.
// Kept separate so they can be unit-tested and reused without React.

// 10-band graphic EQ center frequencies (display labels).
export const BANDS = ['31', '63', '125', '250', '500', '1k', '2k', '4k', '8k', '16k']

// EQ slider position 0..100 -> gain in dB (-12..+12).
export const posToDb = (pos: number): number => (pos - 50) * 0.24

// Effect slider 0..100 -> normalized 0..1.
export const norm = (v: number): number => Math.max(0, Math.min(1, v / 100))
