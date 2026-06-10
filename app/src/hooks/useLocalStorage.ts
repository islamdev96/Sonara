import { useEffect, useState } from 'react'

// Small typed wrapper around localStorage-backed state. Reads the persisted
// value on first render and writes back on every change.
export function useLocalStorage<T>(key: string, initial: T): [T, (v: T) => void] {
  const [value, setValue] = useState<T>(() => {
    try {
      const stored = localStorage.getItem(key)
      return stored !== null ? (JSON.parse(stored) as T) : initial
    } catch {
      return initial
    }
  })

  useEffect(() => {
    try { localStorage.setItem(key, JSON.stringify(value)) } catch { /* ignore quota/serialization errors */ }
  }, [key, value])

  return [value, setValue]
}
