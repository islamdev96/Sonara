// Built-in presets. EQ values are 0..100 slider positions; effects are 0..100;
// boost is a percentage.
export type Preset = {
  eq: number[]
  boost: number
  clarity: number
  ambience: number
  surround: number
  dynamic: number
  bass: number
}

export const DEFAULTS: Record<string, Preset> = {
  Default: { eq: [50,50,50,50,50,50,50,50,50,50], boost:100, clarity:0, ambience:0, surround:0, dynamic:0, bass:0 },
  Music:   { eq: [60,58,52,48,50,55,60,64,62,58], boost:140, clarity:55, ambience:35, surround:35, dynamic:45, bass:65 },
  Movies:  { eq: [72,66,52,44,52,54,60,70,76,72], boost:170, clarity:65, ambience:75, surround:65, dynamic:70, bass:85 },
  Gaming:  { eq: [55,48,44,50,66,74,78,68,60,55], boost:160, clarity:75, ambience:55, surround:70, dynamic:55, bass:50 },
  Voice:   { eq: [40,42,50,62,74,78,72,60,52,48], boost:130, clarity:85, ambience:10, surround:0, dynamic:30, bass:20 },
  Bass:    { eq: [80,76,66,54,48,48,50,52,54,52], boost:150, clarity:30, ambience:25, surround:30, dynamic:55, bass:100 },
}
