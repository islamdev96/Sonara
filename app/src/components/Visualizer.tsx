type VisualizerProps = {
  bars: number[]
  isOn: boolean
}

export default function Visualizer({ bars, isOn }: VisualizerProps) {
  return (
    <div className="visualizer-box">
      {bars.map((h, i) => (
        <div key={i} className="viz-bar" style={{ height: `${h}%`, opacity: isOn ? 1 : 0.15 }} />
      ))}
    </div>
  )
}
