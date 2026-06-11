import type { Strings } from '../i18n'

type EngineBarProps = {
  t: Strings
  engineInstalled: boolean
  engineActive: boolean
  onInstall: () => void
}

export default function EngineBar({ t, engineInstalled, engineActive, onInstall }: EngineBarProps) {
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

  return (
    <div className="engine-bar">
      <div className={`engine-dot ${dotClass}`} />
      <span className="engine-text">{text}</span>

      {!engineInstalled && <button className="engine-btn" id="install-engine-btn" onClick={onInstall}>{t.install}</button>}
    </div>
  )
}
