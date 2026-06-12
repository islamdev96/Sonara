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
import Equalizer, { type ParametricBand } from './components/Equalizer'
import Modals from './components/Modals'
import { computeSpectrum } from './fft'

const kEqFrequencies = [31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0]

export default function App() {
  const [lang, setLang] = useLocalStorage<Lang>('lang', 'en')
  const t = i18n[lang]
  const [isOn, setIsOn] = useState(true)
  const [menuOpen, setMenuOpen] = useState(false)
  const [autoStart, setAutoStart] = useLocalStorage<boolean>('autoStart', true)
  const [licenseInput, setLicenseInput] = useState('')
  const [licenseMsg, setLicenseMsg] = useState('')

  // Native engine status + license come from the main process.
  const { engineInstalled, engineActive, license, levels } = useEngine()

  // Live audio parameters.
  const [boost, setBoost] = useState(150)
  const [eq, setEq] = useState<number[]>([...DEFAULTS.Movies.eq])
  const [clarity, setClarity] = useState(65)
  const [ambience, setAmbience] = useState(75)
  const [surround, setSurround] = useState(65)
  const [dynamic, setDynamic] = useState(70)
  const [bass, setBass] = useState(85)
  const [limiter, setLimiter] = useState(true)

  // Parametric EQ state
  const [eqMode, setEqMode] = useLocalStorage<'graphic' | 'parametric'>('eqMode', 'graphic')
  const [parametricBands, setParametricBands] = useLocalStorage<ParametricBand[]>('parametricBands',
    kEqFrequencies.map(f => ({ freq: f, q: 1.4, gain: 0, type: 0 }))
  )

  // Per-Device Profiles
  const [deviceProfiles, setDeviceProfiles] = useLocalStorage<Record<string, {
    boost: number
    eqMode: 'graphic' | 'parametric'
    eq: number[]
    parametricBands: ParametricBand[]
    bass: number
    clarity: number
    ambience: number
    surround: number
    dynamic: number
    limiter: boolean
  }>>('deviceProfiles', {})

  const lastActiveDeviceRef = useRef<string | null>(null)

  // Load profile when the active device changes
  useEffect(() => {
    const activeDevice = levels.activeDevice || 'System Default Output'
    if (activeDevice === lastActiveDeviceRef.current) return
    lastActiveDeviceRef.current = activeDevice

    const profile = deviceProfiles[activeDevice]
    if (profile) {
      setBoost(profile.boost)
      setEqMode(profile.eqMode)
      setEq(profile.eq)
      setParametricBands(profile.parametricBands)
      setBass(profile.bass)
      setClarity(profile.clarity)
      setAmbience(profile.ambience)
      setSurround(profile.surround)
      setDynamic(profile.dynamic)
      setLimiter(profile.limiter)
    } else {
      // Create initial profile for new device
      setDeviceProfiles(prev => ({
        ...prev,
        [activeDevice]: {
          boost,
          eqMode,
          eq,
          parametricBands,
          bass,
          clarity,
          ambience,
          surround,
          dynamic,
          limiter
        }
      }))
    }
  }, [levels.activeDevice])

  // Save changes to current active device's profile
  useEffect(() => {
    const activeDevice = levels.activeDevice || 'System Default Output'
    setDeviceProfiles(prev => ({
      ...prev,
      [activeDevice]: {
        boost,
        eqMode,
        eq,
        parametricBands,
        bass,
        clarity,
        ambience,
        surround,
        dynamic,
        limiter
      }
    }))
  }, [boost, eqMode, eq, parametricBands, bass, clarity, ambience, surround, dynamic, limiter, levels.activeDevice])

  // Preset storage + selection (custom presets persisted to localStorage).
  const { current, setCurrent, allPresets, isDefaultPreset, saveCustom, overwriteCurrent, deleteCurrent } = usePresets()
  const [modal, setModal] = useState<'save'|'delete'|'license'|null>(null)
  const [newName, setNewName] = useState('')
  const [bars, setBars] = useState<number[]>(Array(56).fill(3))
  
  // Refs to manage 60FPS fluid physics visualizer
  const latestFftRef = useRef<Float32Array | null>(null)
  const heightsRef = useRef<number[]>(Array(56).fill(3))
  const velocitiesRef = useRef<number[]>(Array(56).fill(0))

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

  // ===== real FFT visualizer =====
  // 1. Instantly update the current FFT sample ref when new audio levels arrive from the engine (20 FPS)
  useEffect(() => {
    if (isOn && levels.rawSamples && levels.rawSamples.length > 0) {
      latestFftRef.current = computeSpectrum(levels.rawSamples)
    } else {
      latestFftRef.current = null
    }
  }, [levels.rawSamples, isOn])

  // 2. High-performance requestAnimationFrame loop running at native screen refresh rate (60FPS+)
  // driving fluid spring-damper physics for all 56 bars.
  useEffect(() => {
    let animId: number
    
    const updatePhysics = () => {
      const fftMagnitudes = latestFftRef.current
      
      const newHeights = heightsRef.current.map((h, i) => {
        let targetHeight = 3
        if (isOn && fftMagnitudes) {
          const ratio = i / 55
          
          // Logarithmic bin mapping from bin 1 to bin 80 (covers up to ~15 kHz at 48kHz SR)
          const minBin = 1
          const maxBin = 80
          const binIdx = Math.round(minBin * Math.pow(maxBin / minBin, ratio))
          const mag = fftMagnitudes[binIdx] || 0
          
          // High-frequency pre-emphasis: scale the magnitude as a function of the bar index
          // to make higher frequencies visually active.
          const boost = 1.0 + Math.pow(ratio, 1.5) * 8.0
          const boostedMag = mag * boost
          
          // Convert linear magnitude to Decibels
          const db = 20 * Math.log10(boostedMag + 0.0001)
          const minDb = -36
          const maxDb = -3
          const dbRatio = Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb)))
          
          targetHeight = 3 + dbRatio * 92
        }
        
        // Spring physics parameters (tuned for 60fps refresh rate)
        const stiffness = 0.28 // speed of acceleration towards target
        const damping = 0.58   // bounciness friction (lower = more bounce, higher = more sluggish)
        
        const force = (targetHeight - h) * stiffness
        let velocity = velocitiesRef.current[i] + force
        velocity *= damping
        velocitiesRef.current[i] = velocity
        
        let newH = h + velocity
        // Clamp heights to avoid rendering overflows and clean bottom state
        if (newH < 3) {
          newH = 3
          velocitiesRef.current[i] = 0
        } else if (newH > 95) {
          newH = 95
          velocitiesRef.current[i] = 0
        }
        return newH
      })
      
      heightsRef.current = newHeights
      setBars(newHeights)
      
      animId = requestAnimationFrame(updatePhysics)
    }
    
    animId = requestAnimationFrame(updatePhysics)
    return () => cancelAnimationFrame(animId)
  }, [isOn])

  // ===== push params to the self-contained engine (debounced) =====
  const sendRef = useRef<ReturnType<typeof setTimeout>>(undefined)
  useEffect(() => {
    clearTimeout(sendRef.current)
    sendRef.current = setTimeout(() => {
      // Map current EQ state to parametric bands if in graphic mode
      const bandsToSend = eqMode === 'graphic'
        ? eq.map((gainPos, i) => ({
            freq: kEqFrequencies[i],
            q: 1.4,
            gain: posToDb(gainPos),
            type: 0 // Peaking
          }))
        : parametricBands

      window.api?.setParams({
        enabled: isOn,
        boostPercent: Math.min(maxBoost, boost),
        eqBands: bandsToSend,
        bass: norm(bass), clarity: norm(clarity), ambience: norm(ambience),
        surround: norm(surround), dynamic: norm(dynamic),
        limiterOn: limiter,
      })
    }, 120)
  }, [isOn, boost, eq, eqMode, parametricBands, bass, clarity, ambience, surround, dynamic, limiter, maxBoost])

  // Sync autostart to the OS (persistence handled by useLocalStorage).
  useEffect(() => { window.api?.toggleAutostart(autoStart) }, [autoStart])

  // ===== preset <-> audio state bridge =====
  const loadPreset = (name: string) => {
    setCurrent(name); const p = allPresets[name]; if (!p) return
    setEq([...p.eq]); setBoost(Math.min(maxBoost, p.boost)); setClarity(p.clarity)
    setAmbience(p.ambience); setSurround(p.surround ?? 0); setDynamic(p.dynamic); setBass(p.bass)
    setEqMode('graphic') // presets default to graphic mode
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

      <EngineBar t={t} engineInstalled={engineInstalled} engineActive={engineActive} onInstall={() => window.api?.installEngine()} />

      <TopBar t={t} current={current} presetNames={Object.keys(allPresets)} onSelect={loadPreset} bars={bars} isOn={isOn} activeDevice={levels.activeDevice} />

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
        <Equalizer 
          eq={eq} setEq={setEq} isOn={isOn} 
          eqMode={eqMode} setEqMode={setEqMode}
          parametricBands={parametricBands} setParametricBands={setParametricBands}
          t={t}
        />
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
