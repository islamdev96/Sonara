import { useEffect, useState } from 'react'
import type { LicenseStatus } from '../global'

// Owns the live connection to the native engine: initial status, plus the
// engine-installed and license push events from the Electron main process.
export function useEngine() {
  const [engineInstalled, setEngineInstalled] = useState(false)
  const [license, setLicense] = useState<LicenseStatus>({ tier: 'free', maxBoostPercent: 500, launch: true })

  useEffect(() => {
    if (!window.api) return
    window.api.getStatus().then(s => {
      setEngineInstalled(s.installed)
      if (s.license) setLicense(s.license)
    })
    window.api.onEngineStatus(d => setEngineInstalled(d.installed))
    window.api.onLicenseStatus(d => setLicense(d))
  }, [])

  return { engineInstalled, license }
}
