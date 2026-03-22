/**
 * Ollama /api/tags probe — mirrors Win32 BackendSwitcher IPv4 loopback behavior (providers.js).
 */
const axios = require('axios');
const http = require('http');

const httpIpv4Agent = new http.Agent({ keepAlive: true, family: 4 });

function preferIpv4Loopback(url) {
  const s = String(url || '').trim().replace(/\/+$/, '');
  if (!s) return 'http://127.0.0.1:11434';
  try {
    const u = new URL(s.includes('://') ? s : `http://${s}`);
    if (u.hostname === 'localhost') {
      u.hostname = '127.0.0.1';
    }
    return u.toString().replace(/\/$/, '');
  } catch {
    return s;
  }
}

/**
 * @param {string} [baseUrl]
 * @returns {Promise<{ ok: boolean, base: string, latencyMs: number, models: string[], error?: string }>}
 */
async function probeOllama(baseUrl) {
  const base = preferIpv4Loopback(baseUrl || 'http://127.0.0.1:11434');
  const t0 = Date.now();
  const response = await axios.get(`${base}/api/tags`, {
    timeout: 5000,
    httpAgent: httpIpv4Agent,
    proxy: false
  });
  const models = (response.data && response.data.models) || [];
  const names = models.map((m) => m && m.name).filter(Boolean);
  return {
    ok: true,
    base,
    latencyMs: Date.now() - t0,
    models: names
  };
}

module.exports = { probeOllama, preferIpv4Loopback };
