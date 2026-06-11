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
  Default: {
    eq: [50,50,50,50,50,50,50,50,50,50],
    boost: 100,
    clarity: 0,
    ambience: 0,
    surround: 0,
    dynamic: 0,
    bass: 0
  },

  Music: {
    eq: [56,55,52,49,50,54,58,60,58,55],
    boost: 135,
    clarity: 45,
    ambience: 20,
    surround: 25,
    dynamic: 35,
    bass: 45
  },

  Movies: {
    eq: [62,60,54,48,50,53,58,62,64,60],
    boost: 150,
    clarity: 50,
    ambience: 45,
    surround: 45,
    dynamic: 55,
    bass: 60
  },

  Gaming: {
    eq: [52,50,48,50,60,66,68,62,56,52],
    boost: 145,
    clarity: 60,
    ambience: 25,
    surround: 55,
    dynamic: 40,
    bass: 35
  },

  Voice: {
    eq: [42,44,50,60,70,74,68,58,50,46],
    boost: 125,
    clarity: 75,
    ambience: 0,
    surround: 0,
    dynamic: 25,
    bass: 10
  },

  Bass: {
    eq: [68,66,60,52,48,48,50,52,52,50],
    boost: 140,
    clarity: 25,
    ambience: 10,
    surround: 20,
    dynamic: 45,
    bass: 75
  },
}
