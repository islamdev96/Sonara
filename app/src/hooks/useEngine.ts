import { useEffect, useState } from 'react'
import type { LicenseStatus, EngineLevels } from '../global'

// Owns the live connection to the native engine: initial status, plus the
// engine-installed and license push events from the native C++ main process.
// Also receives real-time audio levels from the APO's status.bin heartbeat.
export function useEngine() {
  const [status, setStatus] = useState({ installed: false, active: false })
  const [license, setLicense] = useState<LicenseStatus>({ tier: 'free', maxBoostPercent: 500, launch: true })
  const [levels, setLevels] = useState<EngineLevels>({
    rmsLeft: 0, rmsRight: 0, peakLeft: 0, peakRight: 0, sampleRate: 0, channels: 0,
  })

  useEffect(() => {
    if (!window.api) return
    window.api.getStatus().then(s => {
      setStatus({ installed: s.installed, active: s.active })
      if (s.license) setLicense(s.license)
    })
    window.api.onEngineStatus(d => setStatus({ installed: d.installed, active: d.active }))
    window.api.onEngineLevels(d => setLevels(d))
    window.api.onLicenseStatus(d => setLicense(d))
  }, [])

  return {
    engineInstalled: status.installed,
    engineActive: status.active,
    license,
    levels,
  }
}
