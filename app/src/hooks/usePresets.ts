import { useState } from 'react'
import { DEFAULTS, type Preset } from '../presets'

const STORAGE_KEY = 'customPresets'

// Owns the user's custom presets (persisted to localStorage) and the currently
// selected preset name. The mapping between a preset and the live audio state
// stays in App, which owns that state.
export function usePresets() {
  const [customs, setCustoms] = useState<Record<string, Preset>>(() => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY)
      return stored ? (JSON.parse(stored) as Record<string, Preset>) : {}
    } catch {
      return {}
    }
  })
  const [current, setCurrent] = useState('Movies')

  const allPresets: Record<string, Preset> = { ...DEFAULTS, ...customs }
  const isDefaultPreset = Object.prototype.hasOwnProperty.call(DEFAULTS, current)

  const persist = (next: Record<string, Preset>) => {
    setCustoms(next)
    try { localStorage.setItem(STORAGE_KEY, JSON.stringify(next)) } catch { /* ignore */ }
  }

  const saveCustom = (name: string, preset: Preset) => {
    persist({ ...customs, [name]: preset })
    setCurrent(name)
  }
  const overwriteCurrent = (preset: Preset) => {
    if (isDefaultPreset) return
    persist({ ...customs, [current]: preset })
  }
  const deleteCurrent = () => {
    const next = { ...customs }
    delete next[current]
    persist(next)
  }

  return { customs, current, setCurrent, allPresets, isDefaultPreset, saveCustom, overwriteCurrent, deleteCurrent }
}
