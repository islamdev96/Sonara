import { useState, useEffect, useRef, useCallback } from 'react'
import './App.css'
import { i18n, type Lang } from './i18n'
import { DEFAULTS, type Preset } from './presets'
import { posToDb, norm } from './audio'
import { useEngine } from './hooks/useEngine'
import { useLocalStorage } from './hooks/useLocalStorage'
import { usePresets } from './hooks/usePresets'
import Header from './components/Header'
import EngineBar from './components/EngineBar'
import TopBar from './components/TopBar'
import ControlSidebar from './components/ControlSidebar'
import Equalizer from './components/Equalizer'
import Modals from './components/Modals'

export default function App() {
  const [lang, setLang] = useLocalStorage<Lang>('lang', 'en')
  const t = i18n[lang]
  const [isOn, setIsOn] = useState(true)
  const [menuOpen, setMenuOpen] = useState(false)
  const [autoStart, setAutoStart] = useLocalStorage<boolean>('autoStart', true)
  const [licenseInput, setLicenseInput] = useState('')
  const [licenseMsg, setLicenseMsg] = useState('')

  // Native engine status + license come from the main process.
  const { engineInstalled, license } = useEngine()

  // Live audio parameters.
  const [boost, setBoost] = useState(150)
  const [eq, setEq] = useState<number[]>([...DEFAULTS.Movies.eq])
  const [clarity, setClarity] = useState(65)
  const [ambience, setAmbience] = useState(75)
  const [surround, setSurround] = useState(65)
  const [dynamic, setDynamic] = useState(70)
  const [bass, setBass] = useState(85)
  const [limiter, setLimiter] = useState(true)

  // Preset storage + selection (custom presets persisted to localStorage).
  const { current, setCurrent, allPresets, isDefaultPreset, saveCustom, overwriteCurrent, deleteCurrent } = usePresets()
  const [modal, setModal] = useState<'save'|'delete'|'license'|null>(null)
  const [newName, setNewName] = useState('')
  const [bars, setBars] = useState<number[]>(Array(56).fill(5))

  const maxBoost = license.maxBoostPercent
  const isPro = license.tier === 'pro'
  const licenseLabel = license.launch ? t.launchFree
    : license.tier === 'pro' ? t.pro
    : license.tier === 'trial' ? t.trial(license.trialDaysLeft ?? 0) : t.free

  // ===== global hotkey =====
  useEffect(() => {
    window.api?.onHotkey(d => setBoost(p => Math.max(0, Math.min(maxBoost, p + (d === 'up' ? 10 : -10)))))
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  // ===== visualizer =====
  useEffect(() => {
    const iv = setInterval(() => setBars(prev => prev.map(() => {
      if (!isOn) return 3
      return Math.random() * 38 + (boost / 300) * 55 + 8
    })), 90)
    return () => clearInterval(iv)
  }, [isOn, boost])

  // ===== push params to the self-contained engine (debounced) =====
  const sendRef = useRef<ReturnType<typeof setTimeout>>(undefined)
  useEffect(() => {
    clearTimeout(sendRef.current)
    sendRef.current = setTimeout(() => {
      window.api?.setParams({
        enabled: isOn,
        boostPercent: Math.min(maxBoost, boost),
        eqGainsDb: eq.map(posToDb),
        bass: norm(bass), clarity: norm(clarity), ambience: norm(ambience),
        surround: norm(surround), dynamic: norm(dynamic),
        limiterOn: limiter,
      })
    }, 120)
  }, [isOn, boost, eq, bass, clarity, ambience, surround, dynamic, limiter, maxBoost])

  // Sync autostart to the OS (persistence handled by useLocalStorage).
  useEffect(() => { window.api?.toggleAutostart(autoStart) }, [autoStart])

  // ===== preset <-> audio state bridge =====
  const loadPreset = (name: string) => {
    setCurrent(name); const p = allPresets[name]; if (!p) return
    setEq([...p.eq]); setBoost(Math.min(maxBoost, p.boost)); setClarity(p.clarity)
    setAmbience(p.ambience); setSurround(p.surround ?? 0); setDynamic(p.dynamic); setBass(p.bass)
  }
  const snapshot = (): Preset => ({ eq, boost, clarity, ambience, surround, dynamic, bass })
  const savePreset = () => {
    if (!newName.trim()) return
    saveCustom(newName, snapshot()); setModal(null); setNewName('')
  }
  const overwritePreset = () => { overwriteCurrent(snapshot()); setMenuOpen(false) }
  const deletePreset = () => { deleteCurrent(); loadPreset('Default'); setModal(null) }

  const activate = useCallback(async () => {
    if (!window.api) return
    const r = await window.api.activateLicense(licenseInput.trim())
    if (r.ok) { setLicenseMsg(t.activated); setTimeout(() => setModal(null), 800) }
    else setLicenseMsg(r.error || t.badKey)
  }, [licenseInput, t])

  return (
    <div className={`app-container ${lang === 'ar' ? 'rtl-layout' : ''}`} dir={lang === 'ar' ? 'rtl' : 'ltr'}>
      <Header
        t={t} license={license} licenseLabel={licenseLabel} isPro={isPro}
        isOn={isOn} menuOpen={menuOpen} autoStart={autoStart} limiter={limiter}
        isDefaultPreset={isDefaultPreset}
        onToggleLang={() => { setLang(lang === 'en' ? 'ar' : 'en'); setMenuOpen(false) }}
        onToggleAutoStart={() => setAutoStart(!autoStart)}
        onToggleLimiter={() => setLimiter(!limiter)}
        onTogglePower={() => setIsOn(!isOn)}
        onToggleMenu={() => setMenuOpen(!menuOpen)}
        onOpenLicense={() => setModal('license')}
        onBuy={() => window.api?.openBuy()}
        onSaveNew={() => { setModal('save'); setMenuOpen(false) }}
        onOverwrite={overwritePreset}
        onDeletePreset={() => { setModal('delete'); setMenuOpen(false) }}
      />

      <EngineBar t={t} engineInstalled={engineInstalled} onInstall={() => window.api?.installEngine()} />

      <TopBar t={t} current={current} presetNames={Object.keys(allPresets)} onSelect={loadPreset} bars={bars} isOn={isOn} />

      <div className="main-content">
        <ControlSidebar
          t={t} isOn={isOn} maxBoost={maxBoost}
          boost={boost} setBoost={setBoost}
          bass={bass} setBass={setBass}
          clarity={clarity} setClarity={setClarity}
          dynamic={dynamic} setDynamic={setDynamic}
          surround={surround} setSurround={setSurround}
          ambience={ambience} setAmbience={setAmbience}
        />
        <Equalizer eq={eq} setEq={setEq} isOn={isOn} />
      </div>

      <Modals
        modal={modal} t={t}
        newName={newName} setNewName={setNewName}
        licenseInput={licenseInput} setLicenseInput={setLicenseInput}
        licenseMsg={licenseMsg} licenseLabel={licenseLabel}
        onClose={() => setModal(null)} onSave={savePreset} onDelete={deletePreset}
        onActivate={activate} onBuy={() => window.api?.openBuy()}
      />
    </div>
  )
}
