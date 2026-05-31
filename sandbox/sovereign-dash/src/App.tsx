import { startTransition, useEffect, useState } from 'react'

type Stats = {
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

type BootstrapPayload = {
  stats: Stats | null
  error: string | null
}

function getBootstrap(): BootstrapPayload {
  if (typeof window === 'undefined') {
    return { stats: null, error: null }
  }
  return window.__SOVEREIGN_BOOTSTRAP__ ?? { stats: null, error: null }
}

function formatBytes(value: number) {
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let size = value
  let unitIndex = 0
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex += 1
  }
  return `${size.toFixed(2)} ${units[unitIndex]}`
}

function formatTimestamp(value: string) {
  return value.replace('T', ' ').replace('Z', ' UTC')
}

export default function App() {
  const bootstrap = getBootstrap()
  const [stats, setStats] = useState<Stats | null>(bootstrap.stats)
  const [error, setError] = useState<string | null>(bootstrap.error)

  useEffect(() => {
    let active = true

    async function loadStats() {
      try {
        const response = await fetch('/api/stats')
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`)
        }
        const json = (await response.json()) as Stats
        if (active) {
          startTransition(() => {
            setStats(json)
            setError(null)
          })
        }
      } catch (loadError) {
        if (active) {
          startTransition(() => {
            setError(loadError instanceof Error ? loadError.message : 'Unknown error')
          })
        }
      }
    }

    loadStats()
    const timer = window.setInterval(loadStats, 3000)
    return () => {
      active = false
      window.clearInterval(timer)
    }
  }, [])

  return (
    <main className="shell">
      <section className="hero">
        <p className="eyebrow">Sovereign Express + React SSR Bridge</p>
        <h1>Local telemetry rendered on the server, hydrated on the client</h1>
        <p className="lede">
          Express renders the first frame, React hydrates it in place, and the client keeps
          polling the sovereign backend every 3 seconds for memory, CPU, and GPU probe signals.
        </p>
        <div className="heroMeta">
          <span className="badge">SSR active</span>
          <span className="heroTime">{stats ? formatTimestamp(stats.timestamp) : 'Bootstrap pending'}</span>
        </div>
      </section>

      {error ? <div className="error">API error: {error}</div> : null}

      <section className="panel panelGrid">
        <article className="metricCard">
          <span className="metricLabel">Memory Load</span>
          <strong>{stats ? `${stats.memory.usagePercent}%` : '...'}</strong>
          <p>{stats ? `${formatBytes(stats.memory.used)} used` : 'Waiting for API...'}</p>
        </article>
        <article className="metricCard">
          <span className="metricLabel">CPU Threads</span>
          <strong>{stats?.cpuCount ?? '...'}</strong>
          <p>{stats ? `Load avg ${stats.loadavg.join(', ')}` : 'Waiting for API...'}</p>
        </article>
        <article className="metricCard">
          <span className="metricLabel">GPU Probe</span>
          <strong>{stats?.gpu.method ?? '...'}</strong>
          <p>
            {stats && stats.gpu.detected.length > 0
              ? `${stats.gpu.detected.length} vendor path(s) detected`
              : 'No vendor directories detected'}
          </p>
        </article>
      </section>

      <section className="panel">
        <header className="panelHeader">
          <h2>System Stats</h2>
          <span>{stats ? formatTimestamp(stats.timestamp) : 'Waiting for API...'}</span>
        </header>
        <table>
          <tbody>
            <tr><th>Hostname</th><td>{stats?.hostname ?? '...'}</td></tr>
            <tr><th>Platform</th><td>{stats ? `${stats.platform} (${stats.arch})` : '...'}</td></tr>
            <tr><th>CPU Threads</th><td>{stats?.cpuCount ?? '...'}</td></tr>
            <tr><th>Uptime</th><td>{stats ? `${stats.uptimeSeconds}s` : '...'}</td></tr>
            <tr><th>Load Avg</th><td>{stats ? stats.loadavg.join(', ') : '...'}</td></tr>
            <tr><th>Memory Used</th><td>{stats ? `${formatBytes(stats.memory.used)} / ${formatBytes(stats.memory.total)} (${stats.memory.usagePercent}%)` : '...'}</td></tr>
            <tr><th>Memory Free</th><td>{stats ? formatBytes(stats.memory.free) : '...'}</td></tr>
            <tr><th>GPU Probe Mode</th><td>{stats?.gpu.method ?? '...'}</td></tr>
            <tr>
              <th>GPU Signals</th>
              <td>
                {stats && stats.gpu.detected.length > 0
                  ? stats.gpu.detected.map((gpu) => `${gpu.vendor} (${gpu.entryCount} entries)`).join(', ')
                  : 'No vendor directories detected'}
              </td>
            </tr>
          </tbody>
        </table>
      </section>

      <section className="panel bridgeNote">
        <header className="panelHeader">
          <h2>Bridge Notes</h2>
          <span>Minimal stack</span>
        </header>
        <ul>
          <li>Express owns the shell and API surface.</li>
          <li>React hydrates server-rendered markup instead of booting an empty client root.</li>
          <li>Vite is used for development HMR and production asset builds, not app-shell ownership.</li>
        </ul>
      </section>
    </main>
  )
}
