import type { Strings } from '../i18n'

type EngineBarProps = {
  t: Strings
  engineInstalled: boolean
  onInstall: () => void
}

export default function EngineBar({ t, engineInstalled, onInstall }: EngineBarProps) {
  return (
    <div className="engine-bar">
      <div className={`engine-dot ${engineInstalled ? 'green' : 'red'}`} />
      <span className="engine-text">{engineInstalled ? t.engineActive : t.engineMissing}</span>
      {!engineInstalled && <button className="engine-btn" onClick={onInstall}>{t.install}</button>}
    </div>
  )
}
