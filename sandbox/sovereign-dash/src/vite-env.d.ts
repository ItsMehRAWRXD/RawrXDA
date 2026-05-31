interface SovereignBootstrapStats {
  ok: boolean
  timestamp: string
  hostname: string
  platform: string
  arch: string
  uptimeSeconds: number
  cpuCount: number
  loadavg: number[]
  memory: {
    total: number
    free: number
    used: number
    usagePercent: number
  }
  gpu: {
    method: string
    detected: Array<{ vendor: string; path: string; entryCount: number }>
    fallback: unknown
  }
}

interface Window {
  __SOVEREIGN_BOOTSTRAP__?: {
    stats: SovereignBootstrapStats | null
    error: string | null
  }
}