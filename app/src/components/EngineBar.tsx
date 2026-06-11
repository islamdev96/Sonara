import type { Strings } from '../i18n'
import type { EngineLevels } from '../global'

type EngineBarProps = {
  t: Strings
  engineInstalled: boolean
  engineActive: boolean
  levels: EngineLevels
  onInstall: () => void
}

// Convert a linear amplitude (0..1+) to a display percentage (0..100).
// Uses a dB-like curve so quiet signals are still visible.
function levelToPercent(linear: number): number {
  if (linear < 0.0001) return 0
  // Map -60dB..0dB to 0..100, then clamp.
  const db = 20 * Math.log10(Math.max(linear, 1e-6))
  return Math.max(0, Math.min(100, ((db + 60) / 60) * 100))
}

export default function EngineBar({ t, engineInstalled, engineActive, levels, onInstall }: EngineBarProps) {
  let dotClass = 'red';
  let text = t.engineMissing;

  if (engineInstalled) {
    if (engineActive) {
      dotClass = 'green';
      text = t.engineProcessing;
    } else {
      dotClass = 'orange';
      text = t.engineInstalled;
    }
  }

  const vuL = levelToPercent(levels.rmsLeft)
  const vuR = levelToPercent(levels.rmsRight)
  const showVU = engineActive && (levels.rmsLeft > 0 || levels.rmsRight > 0)

  return (
    <div className="engine-bar">
      <div className={`engine-dot ${dotClass}`} />
      <span className="engine-text">{text}</span>

      {/* Real VU meter — driven by APO heartbeat, not random animation */}
      {engineActive && (
        <div className="vu-meter" id="vu-meter">
          <div className="vu-channel">
            <span className="vu-label">L</span>
            <div className="vu-track">
              <div className="vu-fill" style={{ width: `${showVU ? vuL : 0}%` }} />
            </div>
          </div>
          <div className="vu-channel">
            <span className="vu-label">R</span>
            <div className="vu-track">
              <div className="vu-fill" style={{ width: `${showVU ? vuR : 0}%` }} />
            </div>
          </div>
        </div>
      )}

      {!engineInstalled && <button className="engine-btn" id="install-engine-btn" onClick={onInstall}>{t.install}</button>}
    </div>
  )
}
