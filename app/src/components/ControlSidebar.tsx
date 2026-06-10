import type { Strings } from '../i18n'

type Setter = (v: number) => void

type ControlSidebarProps = {
  t: Strings
  isOn: boolean
  maxBoost: number
  boost: number; setBoost: Setter
  bass: number; setBass: Setter
  clarity: number; setClarity: Setter
  dynamic: number; setDynamic: Setter
  surround: number; setSurround: Setter
  ambience: number; setAmbience: Setter
}

export default function ControlSidebar(p: ControlSidebarProps) {
  const { t, isOn, maxBoost } = p

  // [label, value, setter, min, max, isMaster]
  const rows: [string, number, Setter, number, number, boolean][] = [
    [t.master, p.boost, p.setBoost, 0, maxBoost, true],
    [t.bass, p.bass, p.setBass, 0, 100, false],
    [t.clarity, p.clarity, p.setClarity, 0, 100, false],
    [t.dynamic, p.dynamic, p.setDynamic, 0, 100, false],
    [t.surround, p.surround, p.setSurround, 0, 100, false],
    [t.ambience, p.ambience, p.setAmbience, 0, 100, false],
  ]

  return (
    <div className="sidebar">
      {rows.map(([label, val, setter, min, max, isMaster], i) => (
        <div className="slider-group" key={i}>
          <div className="slider-header"><span>{label}</span><span>{isMaster ? `${val}%` : val}</span></div>
          <input type="range" min={min} max={max} value={val} disabled={!isOn}
            onChange={e => setter(Number(e.target.value))} />
        </div>
      ))}
    </div>
  )
}
