import type { Strings } from '../i18n'
import type { LicenseStatus } from '../global'

type HeaderProps = {
  t: Strings
  license: LicenseStatus
  licenseLabel: string
  isPro: boolean
  isOn: boolean
  menuOpen: boolean
  autoStart: boolean
  limiter: boolean
  isDefaultPreset: boolean
  onToggleLang: () => void
  onToggleAutoStart: () => void
  onToggleLimiter: () => void
  onTogglePower: () => void
  onToggleMenu: () => void
  onOpenLicense: () => void
  onBuy: () => void
  onSaveNew: () => void
  onOverwrite: () => void
  onDeletePreset: () => void
}

export default function Header(props: HeaderProps) {
  const { t, license, licenseLabel, isPro, isOn, menuOpen, autoStart, limiter, isDefaultPreset } = props
  return (
    <div className="header">
      <div className="brand"><span className="brand-icon">ılı.</span>{t.title}</div>
      <div className="header-right">
        <span className={`license-pill ${license.launch ? 'pro' : license.tier}`} onClick={props.onOpenLicense}>{licenseLabel}</span>
        {!isPro && !license.launch && <button className="buy-btn" onClick={props.onBuy}>{t.buy}</button>}
        <button className={`power-btn ${isOn ? 'active' : ''}`} onClick={props.onTogglePower}>⏻</button>
        <div className="menu-wrap">
          <button className="menu-btn" onClick={props.onToggleMenu}>≡</button>
          {menuOpen && (
            <div className="settings-menu">
              <div className="menu-header">{t.settings}</div>
              <button className="menu-item" onClick={props.onToggleLang}>{t.lang}</button>
              <button className="menu-item" onClick={props.onToggleAutoStart}>{autoStart ? '✓ ' : ''}{t.autoStart}</button>
              <button className="menu-item" onClick={props.onToggleLimiter}>{limiter ? '✓ ' : ''}{t.limiter}</button>
              <div className="menu-sep" />
              <div className="menu-header">{t.presets}</div>
              <button className="menu-item" onClick={props.onSaveNew}>{t.saveNew}</button>
              {!isDefaultPreset && (<>
                <button className="menu-item" onClick={props.onOverwrite}>{t.overwrite}</button>
                <button className="menu-item" onClick={props.onDeletePreset}>{t.deletePreset}</button>
              </>)}
            </div>
          )}
        </div>
      </div>
    </div>
  )
}
