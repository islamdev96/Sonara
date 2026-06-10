import { BANDS } from '../audio'

type EqualizerProps = {
  eq: number[]
  setEq: (v: number[]) => void
  isOn: boolean
}

export default function Equalizer({ eq, setEq, isOn }: EqualizerProps) {
  return (
    <div className="eq-section">
      <div className="eq-container">
        {BANDS.map((freq, idx) => (
          <div className="eq-band" key={idx}>
            <div className="eq-slider-wrap">
              <input type="range" min="0" max="100" value={eq[idx]} disabled={!isOn}
                onChange={e => { const n = [...eq]; n[idx] = Number(e.target.value); setEq(n) }} />
            </div>
            <div className="eq-freq">{freq}</div>
          </div>
        ))}
      </div>
    </div>
  )
}
