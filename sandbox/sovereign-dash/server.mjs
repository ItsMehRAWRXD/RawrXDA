import express from 'express'
import React from 'react'
import os from 'node:os'
import fs from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { renderToString } from 'react-dom/server'

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)
const PORT = Number(process.env.PORT || 4180)
const DEV_CLIENT_ORIGIN = process.env.SOVEREIGN_VITE_ORIGIN || 'http://127.0.0.1:5180'
const isProduction = process.env.NODE_ENV === 'production'

const app = express()
app.use(express.json())
app.use('/assets', express.static(path.join(__dirname, 'dist', 'assets'), { index: false }))

function formatTimestamp(value) {
  return value.replace('T', ' ').replace('Z', ' UTC')
}

function formatBytes(value) {
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let size = value
  let unitIndex = 0
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex += 1
  }
  return `${size.toFixed(2)} ${units[unitIndex]}`
}

function dashboardTree(stats, error) {
  return React.createElement(
    'main',
    { className: 'shell' },
    React.createElement(
      'section',
      { className: 'hero' },
      React.createElement('p', { className: 'eyebrow' }, 'Sovereign Express + React SSR Bridge'),
      React.createElement('h1', null, 'Local telemetry rendered on the server, hydrated on the client'),
      React.createElement(
        'p',
        { className: 'lede' },
        'Express renders the first frame, React hydrates it in place, and the client keeps polling the sovereign backend every 3 seconds for memory, CPU, and GPU probe signals.',
      ),
      React.createElement(
        'div',
        { className: 'heroMeta' },
        React.createElement('span', { className: 'badge' }, 'SSR active'),
        React.createElement('span', { className: 'heroTime' }, stats ? formatTimestamp(stats.timestamp) : 'Bootstrap pending'),
      ),
    ),
    error
      ? React.createElement('div', { className: 'error' }, `API error: ${error}`)
      : null,
    React.createElement(
      'section',
      { className: 'panel panelGrid' },
      React.createElement(
        'article',
        { className: 'metricCard' },
        React.createElement('span', { className: 'metricLabel' }, 'Memory Load'),
        React.createElement('strong', null, stats ? `${stats.memory.usagePercent}%` : '...'),
        React.createElement('p', null, stats ? `${formatBytes(stats.memory.used)} used` : 'Waiting for API...'),
      ),
      React.createElement(
        'article',
        { className: 'metricCard' },
        React.createElement('span', { className: 'metricLabel' }, 'CPU Threads'),
        React.createElement('strong', null, stats ? String(stats.cpuCount) : '...'),
        React.createElement('p', null, stats ? `Load avg ${stats.loadavg.join(', ')}` : 'Waiting for API...'),
      ),
      React.createElement(
        'article',
        { className: 'metricCard' },
        React.createElement('span', { className: 'metricLabel' }, 'GPU Probe'),
        React.createElement('strong', null, stats ? stats.gpu.method : '...'),
        React.createElement(
          'p',
          null,
          stats && stats.gpu.detected.length > 0
            ? `${stats.gpu.detected.length} vendor path(s) detected`
            : 'No vendor directories detected',
        ),
      ),
    ),
    React.createElement(
      'section',
      { className: 'panel' },
      React.createElement(
        'header',
        { className: 'panelHeader' },
        React.createElement('h2', null, 'System Stats'),
        React.createElement('span', null, stats ? formatTimestamp(stats.timestamp) : 'Waiting for API...'),
      ),
      React.createElement(
        'table',
        null,
        React.createElement(
          'tbody',
          null,
          ...[
            ['Hostname', stats?.hostname ?? '...'],
            ['Platform', stats ? `${stats.platform} (${stats.arch})` : '...'],
            ['CPU Threads', stats ? String(stats.cpuCount) : '...'],
            ['Uptime', stats ? `${stats.uptimeSeconds}s` : '...'],
            ['Load Avg', stats ? stats.loadavg.join(', ') : '...'],
            ['Memory Used', stats ? `${formatBytes(stats.memory.used)} / ${formatBytes(stats.memory.total)} (${stats.memory.usagePercent}%)` : '...'],
            ['Memory Free', stats ? formatBytes(stats.memory.free) : '...'],
            ['GPU Probe Mode', stats?.gpu.method ?? '...'],
            [
              'GPU Signals',
              stats && stats.gpu.detected.length > 0
                ? stats.gpu.detected.map((gpu) => `${gpu.vendor} (${gpu.entryCount} entries)`).join(', ')
                : 'No vendor directories detected',
            ],
          ].map(([label, value]) =>
            React.createElement(
              'tr',
              { key: label },
              React.createElement('th', null, label),
              React.createElement('td', null, value),
            ),
          ),
        ),
      ),
    ),
    React.createElement(
      'section',
      { className: 'panel bridgeNote' },
      React.createElement(
        'header',
        { className: 'panelHeader' },
        React.createElement('h2', null, 'Bridge Notes'),
        React.createElement('span', null, 'Minimal stack'),
      ),
      React.createElement(
        'ul',
        null,
        React.createElement('li', null, 'Express owns the shell and API surface.'),
        React.createElement('li', null, 'React hydrates server-rendered markup instead of booting an empty client root.'),
        React.createElement('li', null, 'Vite is used for development HMR and production asset builds, not app-shell ownership.'),
      ),
    ),
  )
}

function serializeBootstrap(payload) {
  return JSON.stringify(payload).replace(/</g, '\\u003c')
}

async function getClientTags() {
  if (!isProduction) {
    return {
      head: '',
      body: `<script type="module" src="${DEV_CLIENT_ORIGIN}/src/main.tsx"></script>`,
    }
  }

  try {
    const manifestPath = path.join(__dirname, 'dist', '.vite', 'manifest.json')
    const manifestRaw = await fs.readFile(manifestPath, 'utf8')
    const manifest = JSON.parse(manifestRaw)
    const entry = manifest['src/main.tsx']
      || manifest['index.html']
      || Object.values(manifest).find((candidate) => candidate && candidate.isEntry)
    if (!entry || !entry.file) {
      return { head: '', body: '' }
    }

    const styles = (entry.css || [])
      .map((href) => `<link rel="stylesheet" href="/${href}">`)
      .join('')

    return {
      head: styles,
      body: `<script type="module" src="/${entry.file}"></script>`,
    }
  } catch {
    return { head: '', body: '' }
  }
}

async function detectGpuInfo() {
  const candidates = [
    { vendor: 'NVIDIA', probe: 'C:/Program Files/NVIDIA Corporation' },
    { vendor: 'AMD', probe: 'C:/Program Files/AMD' },
    { vendor: 'Intel', probe: 'C:/Program Files/Intel' },
  ]

  const detected = []
  for (const candidate of candidates) {
    try {
      const stat = await fs.stat(candidate.probe)
      if (stat.isDirectory()) {
        const entries = await fs.readdir(candidate.probe)
        detected.push({
          vendor: candidate.vendor,
          path: candidate.probe,
          entryCount: entries.length,
        })
      }
    } catch {
    }
  }

  const fallbackPath = path.join(__dirname, 'gpu-info.json')
  let fallback = null
  try {
    const raw = await fs.readFile(fallbackPath, 'utf8')
    fallback = JSON.parse(raw)
  } catch {
  }

  return {
    detected,
    fallback,
    method: detected.length > 0 ? 'filesystem-probe' : 'filesystem-fallback',
  }
}

function getMemoryInfo() {
  const total = os.totalmem()
  const free = os.freemem()
  const used = total - free
  return {
    total,
    free,
    used,
    usagePercent: Number(((used / total) * 100).toFixed(2)),
  }
}

async function buildStatsPayload() {
  const gpu = await detectGpuInfo()
  return {
    ok: true,
    timestamp: new Date().toISOString(),
    hostname: os.hostname(),
    platform: os.platform(),
    arch: os.arch(),
    uptimeSeconds: Math.round(os.uptime()),
    memory: getMemoryInfo(),
    loadavg: os.loadavg(),
    cpuCount: os.cpus().length,
    gpu,
  }
}

app.get('/api/health', (_req, res) => {
  res.json({ ok: true, service: 'sovereign-dash-api', port: PORT })
})

app.get('/api/stats', async (_req, res) => {
  res.json(await buildStatsPayload())
})

app.get('/', async (_req, res) => {
  let stats = null
  let error = null

  try {
    stats = await buildStatsPayload()
  } catch (statsError) {
    error = statsError instanceof Error ? statsError.message : 'Unknown stats error'
  }

  const bootstrap = { stats, error }
  const appMarkup = renderToString(dashboardTree(stats, error))
  const clientTags = await getClientTags()

  res.type('html').send(`<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Sovereign Dash</title>
    ${clientTags.head}
  </head>
  <body>
    <div id="root">${appMarkup}</div>
    <script>window.__SOVEREIGN_BOOTSTRAP__ = ${serializeBootstrap(bootstrap)};</script>
    ${clientTags.body}
  </body>
</html>`)
})

app.listen(PORT, '127.0.0.1', () => {
  console.log(`Sovereign Dash API running at http://127.0.0.1:${PORT}`)
})
