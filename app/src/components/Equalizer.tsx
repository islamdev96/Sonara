import { useState } from 'react'
import { BANDS } from '../audio'
import type { Strings } from '../i18n'

export type ParametricBand = {
  freq: number
  q: number
  gain: number
  type: number // 0=Peaking, 1=LowShelf, 2=HighShelf, 3=LowPass, 4=HighPass
}

type EqualizerProps = {
  eq: number[]
  setEq: (v: number[]) => void
  isOn: boolean
  eqMode: 'graphic' | 'parametric'
  setEqMode: (v: 'graphic' | 'parametric') => void
  parametricBands: ParametricBand[]
  setParametricBands: (v: ParametricBand[]) => void
  t: Strings
}

export default function Equalizer(props: EqualizerProps) {
  const { eq, setEq, isOn, eqMode, setEqMode, parametricBands, setParametricBands, t } = props
  const [selectedBand, setSelectedBand] = useState(0)

  const handleBandChange = (index: number, key: keyof ParametricBand, value: number) => {
    const updated = [...parametricBands]
    updated[index] = { ...updated[index], [key]: value }
    setParametricBands(updated)
  }

  const activeBand = parametricBands[selectedBand] || { freq: 1000, q: 1.4, gain: 0, type: 0 }
  const isGainEnabled = activeBand.type <= 2

  return (
    <div className="eq-section">
      <div className="eq-header-tabs">
        <button className={`eq-tab-btn ${eqMode === 'graphic' ? 'active' : ''}`} onClick={() => setEqMode('graphic')}>
          {t.eqModeGraphic || 'Graphic EQ'}
        </button>
        <button className={`eq-tab-btn ${eqMode === 'parametric' ? 'active' : ''}`} onClick={() => setEqMode('parametric')}>
          {t.eqModeParametric || 'Parametric EQ'}
        </button>
      </div>

      {eqMode === 'graphic' ? (
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
      ) : (
        <div className="parametric-eq-container">
          {/* Left panel: list of bands */}
          <div className="peq-bands-list">
            {parametricBands.map((band, idx) => (
              <div key={idx} className={`peq-band-item ${selectedBand === idx ? 'selected' : ''}`}
                onClick={() => setSelectedBand(idx)}>
                <span className="peq-band-num">#{idx + 1}</span>
                <span className="peq-band-details">
                  {band.freq}Hz | {band.gain.toFixed(1)}dB
                </span>
                <span className={`peq-band-type-badge type-${band.type}`}>
                  {band.type === 0 ? 'PK' : band.type === 1 ? 'LS' : band.type === 2 ? 'HS' : band.type === 3 ? 'LP' : 'HP'}
                </span>
              </div>
            ))}
          </div>

          {/* Right panel: editor for selected band */}
          <div className="peq-editor-panel">
            <div className="peq-editor-title">Band #{selectedBand + 1} Settings</div>
            
            <div className="peq-editor-row">
              <label>{t.filterType || 'Type'}</label>
              <select value={activeBand.type} disabled={!isOn}
                onChange={e => handleBandChange(selectedBand, 'type', Number(e.target.value))}>
                <option value={0}>Peaking (PK)</option>
                <option value={1}>Low Shelf (LS)</option>
                <option value={2}>High Shelf (HS)</option>
                <option value={3}>Low Pass (LP)</option>
                <option value={4}>High Pass (HP)</option>
              </select>
            </div>

            <div className="peq-editor-row">
              <div className="peq-label-val">
                <label>{t.freq || 'Frequency'}</label>
                <span>{activeBand.freq} Hz</span>
              </div>
              <input type="range" min="20" max="20000" step="1" value={activeBand.freq} disabled={!isOn}
                onChange={e => handleBandChange(selectedBand, 'freq', Number(e.target.value))} />
            </div>

            <div className="peq-editor-row">
              <div className="peq-label-val">
                <label>{t.qFactor || 'Q Factor'}</label>
                <span>{activeBand.q.toFixed(2)}</span>
              </div>
              <input type="range" min="0.1" max="10" step="0.05" value={activeBand.q} disabled={!isOn}
                onChange={e => handleBandChange(selectedBand, 'q', Number(e.target.value))} />
            </div>

            <div className="peq-editor-row">
              <div className="peq-label-val">
                <label style={{ opacity: isGainEnabled ? 1 : 0.3 }}>Gain</label>
                <span style={{ opacity: isGainEnabled ? 1 : 0.3 }}>{activeBand.gain.toFixed(1)} dB</span>
              </div>
              <input type="range" min="-12" max="12" step="0.1" value={activeBand.gain} disabled={!isOn || !isGainEnabled}
                onChange={e => handleBandChange(selectedBand, 'gain', Number(e.target.value))} />
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
