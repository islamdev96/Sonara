import { useState, useEffect, useRef } from 'react'
import './App.css'

// =============================================================================
// Translations
// =============================================================================
const i18n = {
  en: {
    title: 'WinAudio Booster Pro',
    settings: 'SETTINGS',
    lang: 'عربي / Arabic',
    autoStart: 'Run on Startup',
    presets: 'PRESETS',
    saveNew: 'Save New Preset',
    overwrite: 'Overwrite Current',
    deletePreset: 'Delete Preset',
    preset: 'Select Preset...',
    device: 'System Default Audio Device',
    master: 'Master Volume',
    clarity: 'Clarity',
    ambience: 'Ambience',
    surround: 'Surround Sound',
    dynamic: 'Dynamic Boost',
    bass: 'Bass Boost',
    engineActive: 'Boost Engine: Active — Ready for 300% boost',
    engineMissing: 'Boost Engine: Not Installed — Volume limited to 100%',
    install: 'Install',
    enterName: 'Preset Name',
    save: 'Save',
    cancel: 'Cancel',
    confirmDel: 'Delete this preset?',
    delete: 'Delete',
  },
  ar: {
    title: 'مضخم الصوت برو',
    settings: 'الإعدادات',
    lang: 'English',
    autoStart: 'التشغيل مع الويندوز',
    presets: 'الإعدادات المسبقة',
    saveNew: 'حفظ إعداد جديد',
    overwrite: 'تحديث الإعداد الحالي',
    deletePreset: 'مسح الإعداد',
    preset: 'اختر إعداد...',
    device: 'جهاز الصوت الافتراضي',
    master: 'مستوى الصوت',
    clarity: 'الوضوح',
    ambience: 'المحيط',
    surround: 'صوت محيطي',
    dynamic: 'تضخيم ديناميكي',
    bass: 'تضخيم البيز',
    engineActive: 'محرك التضخيم: مفعّل — جاهز للتضخيم حتى 300%',
    engineMissing: 'محرك التضخيم: غير مثبت — الصوت محدود بـ 100%',
    install: 'تثبيت',
    enterName: 'اسم الإعداد',
    save: 'حفظ',
    cancel: 'إلغاء',
    confirmDel: 'هل أنت متأكد من مسح هذا الإعداد؟',
    delete: 'مسح',
  }
}

// =============================================================================
// Default Presets
// =============================================================================
const DEFAULTS = {
  Default:  { eq: [50,50,50,50,50,50,50,50,50], boost:100, clarity:0, ambience:0, surround:0, dynamic:0, bass:0 },
  Music:    { eq: [65,60,50,45,55,60,65,70,65], boost:120, clarity:60, ambience:40, surround:30, dynamic:50, bass:70 },
  Movies:   { eq: [75,70,50,40,60,55,65,75,80], boost:150, clarity:70, ambience:80, surround:60, dynamic:80, bass:90 },
  Gaming:   { eq: [55,45,40,50,75,80,85,70,60], boost:130, clarity:80, ambience:60, surround:70, dynamic:60, bass:50 },
  Voice:    { eq: [40,40,50,65,80,85,75,60,50], boost:110, clarity:90, ambience:10, surround:0, dynamic:30, bass:20 },
}

const BANDS = ['115', '250', '450', '630', '1.25k', '2.7k', '5.3k', '7.5k', '13k']

// =============================================================================
// App Component
// =============================================================================
export default function App() {
  const [isOn, setIsOn] = useState(true)
  const [lang, setLang] = useState<'en'|'ar'>('en')
  const t = i18n[lang]

  const [menuOpen, setMenuOpen] = useState(false)
  const [autoStart, setAutoStart] = useState(true)
  const [apoInstalled, setApoInstalled] = useState(false)

  // Audio state
  const [boost, setBoost] = useState(150)
  const [eq, setEq] = useState([50,45,60,75,65,55,45,50,55])
  const [clarity, setClarity] = useState(50)
  const [ambience, setAmbience] = useState(30)
  const [surround, setSurround] = useState(40)
  const [dynamic, setDynamic] = useState(70)
  const [bass, setBass] = useState(40)

  // Presets
  const [current, setCurrent] = useState('Movies')
  const [customs, setCustoms] = useState<Record<string, any>>({})
  const [modal, setModal] = useState<'save'|'delete'|null>(null)
  const [newName, setNewName] = useState('')

  // Visualizer
  const [bars, setBars] = useState<number[]>(Array(55).fill(5))

  const allPresets = { ...DEFAULTS, ...customs }
  const maxBoost = apoInstalled ? 300 : 100

  // ===== Init =====
  useEffect(() => {
    const saved = localStorage.getItem('customPresets')
    if (saved) setCustoms(JSON.parse(saved))

    const savedAuto = localStorage.getItem('autoStart')
    if (savedAuto !== null) setAutoStart(JSON.parse(savedAuto))

    if (window.api) {
      window.api.onHotkeyAction?.((action) => {
        if (action === 'volume-up') setBoost(p => Math.min(maxBoost, p + 10))
        if (action === 'volume-down') setBoost(p => Math.max(0, p - 10))
      })
      window.api.onAPOStatus?.((data) => setApoInstalled(data.installed))
      window.api.onVolumeSync?.((level) => setBoost(level))
    }
  }, [])

  // ===== Visualizer =====
  useEffect(() => {
    const iv = setInterval(() => {
      setBars(prev => prev.map(() => {
        if (!isOn) return 3
        const base = (boost / 300) * 60
        return Math.random() * 40 + base + 10
      }))
    }, 100)
    return () => clearInterval(iv)
  }, [isOn, boost])

  // ===== Send to backend =====
  const sendRef = useRef<ReturnType<typeof setTimeout>>()
  useEffect(() => {
    clearTimeout(sendRef.current)
    sendRef.current = setTimeout(() => {
      if (!window.api) return
      if (isOn) {
        const finalBoost = Math.min(maxBoost, boost + (dynamic * 0.3))
        window.api.setMasterBoost?.(finalBoost)

        const finalEq = eq.map((v, i) => {
          let val = v
          if (i <= 1) val += bass * 0.3
          if (i >= 4 && i <= 6) val += clarity * 0.2
          return Math.min(100, Math.max(0, val))
        })
        window.api.setEqBands?.(finalEq)
      } else {
        window.api.setMasterBoost?.(0)
        window.api.setEqBands?.(DEFAULTS.Default.eq)
      }
    }, 150) // Debounce 150ms to avoid file write storms
  }, [isOn, eq, boost, bass, clarity, dynamic])

  useEffect(() => {
    if (window.api?.toggleAutoStart) {
      window.api.toggleAutoStart(autoStart)
      localStorage.setItem('autoStart', JSON.stringify(autoStart))
    }
  }, [autoStart])

  // ===== Preset actions =====
  const loadPreset = (name: string) => {
    setCurrent(name)
    const p = allPresets[name]
    if (!p) return
    setEq([...p.eq])
    setBoost(Math.min(maxBoost, p.boost))
    setClarity(p.clarity); setAmbience(p.ambience)
    setSurround(p.surround ?? 0); setDynamic(p.dynamic); setBass(p.bass)
  }

  const savePreset = () => {
    if (!newName.trim()) return
    const next = { ...customs, [newName]: { eq, boost, clarity, ambience, surround, dynamic, bass } }
    setCustoms(next); localStorage.setItem('customPresets', JSON.stringify(next))
    setCurrent(newName); setModal(null); setNewName('')
  }

  const overwritePreset = () => {
    if (DEFAULTS.hasOwnProperty(current)) return
    const next = { ...customs, [current]: { eq, boost, clarity, ambience, surround, dynamic, bass } }
    setCustoms(next); localStorage.setItem('customPresets', JSON.stringify(next))
    setMenuOpen(false)
  }

  const deletePreset = () => {
    if (DEFAULTS.hasOwnProperty(current)) return
    const next = { ...customs }; delete next[current]
    setCustoms(next); localStorage.setItem('customPresets', JSON.stringify(next))
    loadPreset('Default'); setModal(null)
  }

  // ===== Render =====
  return (
    <div className={`app-container ${lang === 'ar' ? 'rtl-layout' : ''}`} dir={lang === 'ar' ? 'rtl' : 'ltr'}>

      {/* Header */}
      <div className="header">
        <div className="brand"><span className="brand-icon">ılı.</span>{t.title}</div>
        <div className="header-right">
          <button className={`power-btn ${isOn ? 'active' : ''}`} onClick={() => setIsOn(!isOn)}>⏻</button>
          <div className="menu-wrap">
            <button className="menu-btn" onClick={() => setMenuOpen(!menuOpen)}>≡</button>
            {menuOpen && (
              <div className="settings-menu">
                <div className="menu-header">{t.settings}</div>
                <button className="menu-item" onClick={() => { setLang(lang === 'en' ? 'ar' : 'en'); setMenuOpen(false) }}>{t.lang}</button>
                <button className="menu-item" onClick={() => setAutoStart(!autoStart)}>{autoStart ? '✓ ' : ''}{t.autoStart}</button>
                <div className="menu-sep" />
                <div className="menu-header">{t.presets}</div>
                <button className="menu-item" onClick={() => { setModal('save'); setMenuOpen(false) }}>{t.saveNew}</button>
                {!DEFAULTS.hasOwnProperty(current) && (
                  <>
                    <button className="menu-item" onClick={overwritePreset}>{t.overwrite}</button>
                    <button className="menu-item" onClick={() => { setModal('delete'); setMenuOpen(false) }}>{t.deletePreset}</button>
                  </>
                )}
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Engine Status */}
      <div className="engine-bar">
        <div className={`engine-dot ${apoInstalled ? 'green' : 'red'}`} />
        <span className="engine-text">{apoInstalled ? t.engineActive : t.engineMissing}</span>
        {!apoInstalled && (
          <button className="engine-btn" onClick={() => window.api?.installAPO?.()}>{t.install}</button>
        )}
      </div>

      {/* Top Section: Presets + Visualizer */}
      <div className="top-section">
        <div className="top-bar">
          <select className="preset-selector" value={current} onChange={e => loadPreset(e.target.value)}>
            {Object.keys(allPresets).map(n => <option key={n} value={n}>{n}</option>)}
          </select>
          <div className="device-label">{t.device}</div>
        </div>
        <div className="visualizer-box">
          {bars.map((h, i) => <div key={i} className="viz-bar" style={{ height: `${h}%`, opacity: isOn ? 1 : 0.15 }} />)}
        </div>
      </div>

      {/* Main: Sidebar + EQ */}
      <div className="main-content">
        <div className="sidebar">
          {([
            [t.master, boost, (v: number) => setBoost(v), 0, maxBoost],
            [t.clarity, clarity, setClarity, 0, 100],
            [t.ambience, ambience, setAmbience, 0, 100],
            [t.surround, surround, setSurround, 0, 100],
            [t.dynamic, dynamic, setDynamic, 0, 100],
            [t.bass, bass, setBass, 0, 100],
          ] as [string, number, (v: number) => void, number, number][]).map(([label, val, setter, min, max], i) => (
            <div className="slider-group" key={i}>
              <div className="slider-header">
                <span>{label}</span>
                <span>{label === t.master ? `${val}%` : val}</span>
              </div>
              <input type="range" min={min} max={max} value={val} disabled={!isOn}
                onChange={e => setter(Number(e.target.value))} />
            </div>
          ))}
        </div>

        <div className="eq-section">
          <div className="eq-container">
            {BANDS.map((freq, idx) => (
              <div className="eq-band" key={idx}>
                <div className="eq-slider-wrap">
                  <input type="range" min="0" max="100" value={eq[idx]} disabled={!isOn}
                    onChange={e => {
                      const next = [...eq]; next[idx] = Number(e.target.value); setEq(next)
                    }} />
                </div>
                <div className="eq-freq">{freq}</div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Modals */}
      {modal && (
        <div className="modal-overlay" onClick={() => setModal(null)}>
          <div className="modal-box" onClick={e => e.stopPropagation()}>
            {modal === 'save' ? (
              <>
                <h3>{t.saveNew}</h3>
                <input type="text" autoFocus placeholder={t.enterName}
                  value={newName} onChange={e => setNewName(e.target.value)}
                  onKeyDown={e => e.key === 'Enter' && savePreset()} />
                <div className="modal-actions">
                  <button className="btn-cancel" onClick={() => setModal(null)}>{t.cancel}</button>
                  <button className="btn-primary" onClick={savePreset}>{t.save}</button>
                </div>
              </>
            ) : (
              <>
                <h3>{t.confirmDel}</h3>
                <div className="modal-actions">
                  <button className="btn-cancel" onClick={() => setModal(null)}>{t.cancel}</button>
                  <button className="btn-primary" onClick={deletePreset}>{t.delete}</button>
                </div>
              </>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
