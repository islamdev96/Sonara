import type { Strings } from '../i18n'
import Visualizer from './Visualizer'

type TopBarProps = {
  t: Strings
  current: string
  presetNames: string[]
  onSelect: (name: string) => void
  bars: number[]
  isOn: boolean
  activeDevice?: string
}

export default function TopBar({ t, current, presetNames, onSelect, bars, isOn, activeDevice }: TopBarProps) {
  return (
    <div className="top-section">
      <div className="top-bar">
        <select className="preset-selector" value={current} onChange={e => onSelect(e.target.value)}>
          {presetNames.map(n => <option key={n} value={n}>{n}</option>)}
        </select>
        <div className="device-label" title={activeDevice || t.device}>
          🎧 {activeDevice || t.device}
        </div>
      </div>
      <Visualizer bars={bars} isOn={isOn} />
    </div>
  )
}
