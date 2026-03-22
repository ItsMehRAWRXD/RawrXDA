/**
 * Main-process AI provider routing: real Ollama-compatible HTTP (`/api/chat`) per `config/providers.json`.
 * Optional `rawrxd-local` deterministic lane remains for offline / tests when selected explicitly.
 */

const fs = require('fs');
const path = require('path');
const http = require('http');
const axios = require('axios');

const { preferIpv4Loopback } = require(path.join(__dirname, '..', '..', 'electron', 'ollama_probe_util'));

const httpIpv4Agent = new http.Agent({ keepAlive: true, family: 4 });

const CONFIG_KEYS = ['bigdaddyg', 'copilot', 'amazonq', 'cursor'];

function loadProvidersJson(configPath) {
  try {
    const raw = fs.readFileSync(configPath, 'utf8');
    return JSON.parse(raw);
  } catch {
    return {};
  }
}

function resolveConfigPath(explicit) {
  if (explicit && fs.existsSync(explicit)) return explicit;
  const env = process.env.RAWRXD_PROVIDERS_JSON;
  if (env && fs.existsSync(env)) return env;
  return path.join(__dirname, '..', '..', 'config', 'providers.json');
}

function extractUserPrompt(prompt) {
  const raw = String(prompt || '').trim();
  if (!raw) return '';
  const marker = raw.lastIndexOf('User:');
  if (marker >= 0) return raw.slice(marker + 'User:'.length).trim();
  return raw;
}

function extractGoalFromPlanPrompt(prompt) {
  const raw = String(prompt || '');
  const m = raw.match(/User goal:\s*\n([\s\S]*?)\n\nProject root:/i);
  if (!m) return extractUserPrompt(raw);
  return String(m[1] || '').trim();
}

function parseAllowedTypes(prompt) {
  const raw = String(prompt || '');
  const m = raw.match(/step types allowed for this run:\s*([^\n]+)/i);
  if (!m) {
    return new Set(['ask_ai', 'read_file', 'write_file', 'append_file', 'list_dir', 'search_repo']);
  }
  const out = new Set();
  String(m[1])
    .split(',')
    .map((s) => s.trim().toLowerCase())
    .forEach((t) => {
      if (t) out.add(t);
    });
  if (!out.has('ask_ai')) out.add('ask_ai');
  return out;
}

function keepAllowedSteps(steps, allowed) {
  return steps.filter((s) => allowed.has(String(s.type || '').toLowerCase()));
}

function buildPlanSteps(goal, allowed) {
  const g = String(goal || '').toLowerCase();
  const steps = [];

  const add = (step) => {
    if (allowed.has(String(step.type || '').toLowerCase())) steps.push(step);
  };

  add({
    type: 'list_dir',
    description: 'Survey workspace layout before mutating files',
    path: '.',
    risk: 'low'
  });

  if (/test|failing|regress|ci|spec/.test(g)) {
    add({
      type: 'search_repo',
      description: 'Find related tests and failing surfaces',
      prompt: 'test|spec|assert|expect|failing',
      maxHits: 80,
      risk: 'low'
    });
  }

  if (/bug|fix|error|crash|exception|broken|fail/.test(g)) {
    add({
      type: 'search_repo',
      description: 'Locate likely error sources and affected symbols',
      prompt: 'error|exception|TODO|FIXME|fail|broken',
      maxHits: 120,
      risk: 'low'
    });
  }

  if (/feature|implement|add|build|create|reroute|rewrite|refactor/.test(g)) {
    add({
      type: 'search_repo',
      description: 'Find integration points and existing architecture patterns',
      prompt: 'orchestrator|provider|context|panel|settings|route',
      maxHits: 100,
      risk: 'low'
    });
  }

  add({
    type: 'ask_ai',
    description: 'Generate precise implementation plan from gathered evidence',
    prompt: `Create an implementation strategy for: ${goal}`,
    risk: 'medium'
  });

  if (allowed.has('write_file')) {
    const planSummary = steps.length
      ? steps
          .map(
            (s, i) =>
              `${i + 1}. **${String(s.type)}** — ${String(s.description || s.prompt || '').slice(0, 220)}${String(s.description || s.prompt || '').length > 220 ? '…' : ''}`
          )
          .join('\n')
      : '_No prior steps queued._';
    add({
      type: 'write_file',
      description: 'Persist deterministic planner trace under docs/ (audit / handoff)',
      path: 'docs/RAWRXD_LOCAL_PLAN_TRACE.md',
      content: [
        '# Local planner trace',
        '',
        `- **Generated:** ${new Date().toISOString()}`,
        '- **Provider:** rawrxd-local (deterministic, no external HTTP)',
        '',
        '## Goal',
        String(goal || '').trim() || '_(empty)_',
        '',
        '## Planned steps (before this write)',
        planSummary,
        '',
        '---',
        'Safe to delete. Replace with real edits via Agent tool steps when you have concrete paths.'
      ].join('\n'),
      risk: 'low'
    });
  }

  const kept = keepAllowedSteps(steps, allowed);
  return kept.length > 0
    ? kept
    : [
        {
          type: 'ask_ai',
          description: 'Fallback reasoning step',
          prompt: `No executable plan types were available for: ${goal}`,
          risk: 'medium'
        }
      ];
}

function buildReflection(prompt) {
  const raw = String(prompt || '');
  const goal = raw.match(/Original goal:\s*\n([\s\S]*?)\n\nStep outcomes/i)?.[1]?.trim() || 'Unknown goal';
  return [
    'Goal coverage appears partial and should be validated against concrete outputs.',
    `Primary target: ${goal.slice(0, 220)}${goal.length > 220 ? '…' : ''}`,
    'Highest-leverage next action: run a targeted verification pass on changed paths and summarize unresolved risks.'
  ].join(' ');
}

function buildDeepReflection(prompt) {
  const raw = String(prompt || '');
  const goal = raw.match(/Original goal:\s*\n([\s\S]*?)\n\nPrior reflection/i)?.[1]?.trim() || 'Unknown goal';
  return [
    'Overlooked risk: integration boundaries may still contain stale assumptions from previous transport/provider models.',
    `Follow-up focus for "${goal.slice(0, 180)}${goal.length > 180 ? '…' : ''}": verify every runtime path and user-facing hint references local-only behavior.`,
    'Single best action: execute a grep-based audit for legacy provider terms, patch mismatches, then rebuild.'
  ].join(' ');
}

function buildGeneralResponse(prompt, context = {}) {
  const userText = extractUserPrompt(prompt);
  const maxChars = Math.max(96, Math.min(2600, Number(context.maxTokens) || 4096));
  const clipped = userText.slice(0, maxChars);
  if (!clipped) {
    return '[rawrxd-local] Request was empty. Provide a concrete goal, file/symbol target, and desired output.';
  }
  return [
    '[rawrxd-local] Local deterministic responder active (no external provider calls).',
    'Action summary:',
    `1) Interpreted request: ${clipped}${userText.length > clipped.length ? ' …[truncated]' : ''}`,
    '2) Recommended execution mode: read evidence first, then apply minimal high-signal edits.',
    '3) Verification rule: rebuild/test affected targets and report residual risks explicitly.'
  ].join('\n');
}

/**
 * Ollama HTTP API: POST /api/chat (non-streaming).
 * @see https://github.com/ollama/ollama/blob/main/docs/api.md
 */
class OllamaCompatibleProvider {
  constructor(id, displayName, cfg) {
    this.id = id;
    this.name = displayName;
    this.baseUrl = String(cfg.url || '').trim().replace(/\/+$/, '');
    this.model = String(cfg.model || '').trim();
    this.enabled = cfg.enabled !== false;
    const token = (cfg.token || cfg.apiKey || '').trim();
    this.authHeader = token ? { Authorization: `Bearer ${token}` } : {};
    this.capabilities = ['code', 'plan', 'chat', 'ollama-http'];
  }

  isConfigured() {
    return Boolean(this.enabled && this.baseUrl && this.model);
  }

  async invoke(prompt, context = {}) {
    const base = preferIpv4Loopback(this.baseUrl);
    const url = `${base.replace(/\/+$/, '')}/api/chat`;
    const temperature =
      context.temperature != null && Number.isFinite(Number(context.temperature))
        ? Number(context.temperature)
        : 0.2;
    const maxTok = Math.min(131072, Math.max(1, Number(context.maxTokens) || 4096));
    const body = {
      model: this.model,
      messages: [{ role: 'user', content: String(prompt || '') }],
      stream: false,
      options: {
        temperature,
        num_predict: maxTok
      }
    };

    let response;
    try {
      response = await axios.post(url, body, {
        timeout: Math.min(600_000, Math.max(30_000, maxTok * 200)),
        httpAgent: httpIpv4Agent,
        headers: {
          'Content-Type': 'application/json',
          ...this.authHeader
        },
        proxy: false,
        validateStatus: (s) => s >= 200 && s < 300
      });
    } catch (err) {
      const msg =
        err.response?.data?.error ||
        err.response?.data?.message ||
        err.message ||
        String(err);
      throw new Error(`[${this.id}] Ollama /api/chat failed: ${msg}`);
    }

    const data = response.data || {};
    const content =
      (data.message && typeof data.message.content === 'string' && data.message.content) ||
      (typeof data.response === 'string' && data.response) ||
      '';

    if (!String(content).trim()) {
      throw new Error(
        `[${this.id}] Empty model response — check model name "${this.model}" is pulled on the host (ollama pull).`
      );
    }

    return {
      content,
      model: data.model || this.model,
      provider: this.id,
      lane: 'ollama-http',
      tokens: Math.max(1, Math.ceil(String(content).length / 4))
    };
  }
}

class RawrxdLocalProvider {
  constructor(wasmRuntime) {
    this.id = 'rawrxd-local';
    this.name = 'RawrXD Local (offline)';
    this.model = 'rawrxd-local-v1';
    this.enabled = true;
    this.wasmRuntime = wasmRuntime || null;
    this.capabilities = ['code', 'plan', 'debug', 'test', 'explain', 'offline'];
  }

  isConfigured() {
    return this.enabled;
  }

  async invoke(prompt, context = {}) {
    const raw = String(prompt || '');
    let content;
    let lane = 'local-deterministic';
    if (/Return ONLY a JSON object with a "steps" array/i.test(raw) || /planning brain of an autonomous IDE agent/i.test(raw)) {
      const goal = extractGoalFromPlanPrompt(raw);
      const allowed = parseAllowedTypes(raw);
      const steps = buildPlanSteps(goal, allowed);
      content = JSON.stringify({ steps }, null, 2);
    } else if (/You are a senior principal engineer reviewing an autonomous IDE run/i.test(raw)) {
      content = buildReflection(raw);
    } else if (/Deep critique|auditing an autonomous IDE run/i.test(raw)) {
      content = buildDeepReflection(raw);
    } else {
      if (this.wasmRuntime && typeof this.wasmRuntime.invoke === 'function') {
        const wasm = await this.wasmRuntime.invoke(raw, {
          maxTokens: context.maxTokens
        });
        if (wasm && wasm.ok && typeof wasm.content === 'string' && wasm.content.trim()) {
          content = wasm.content;
          lane = wasm.lane || 'wasm-main-runtime';
        } else {
          content = `${buildGeneralResponse(raw, context)}\n\n[fallback] ${
            (wasm && wasm.error) || 'WASM runtime unavailable; deterministic local engine used.'
          }`;
        }
      } else {
        content = buildGeneralResponse(raw, context);
      }
    }

    return {
      content,
      model: this.model,
      provider: this.id,
      lane,
      tokens: Math.max(1, Math.ceil(String(content).length / 4))
    };
  }
}

const DISPLAY_NAMES = {
  bigdaddyg: 'BigDaddyG / Ollama',
  copilot: 'Copilot slot (Ollama HTTP)',
  amazonq: 'Amazon Q slot (Ollama HTTP)',
  cursor: 'Cursor slot (Ollama HTTP)'
};

function buildProviderMap(json, wasmRuntime) {
  const providers = {
    'rawrxd-local': new RawrxdLocalProvider(wasmRuntime)
  };
  for (const key of CONFIG_KEYS) {
    const cfg = json[key];
    if (!cfg || typeof cfg !== 'object') continue;
    providers[key] = new OllamaCompatibleProvider(key, DISPLAY_NAMES[key] || key, cfg);
  }
  return providers;
}

function firstConfiguredProviderId(providers, preferOrder) {
  for (const id of preferOrder) {
    const p = providers[id];
    if (p && p.isConfigured()) return id;
  }
  if (providers['rawrxd-local']?.isConfigured()) return 'rawrxd-local';
  return null;
}

class AIProviderManager {
  constructor(options = {}) {
    this.configPath = resolveConfigPath(options.configPath);
    this.json = loadProvidersJson(this.configPath);
    this.providers = buildProviderMap(this.json, options.wasmRuntime || null);
    const prefer = [...CONFIG_KEYS, 'rawrxd-local'];
    this.activeProvider = firstConfiguredProviderId(this.providers, prefer) || 'rawrxd-local';
  }

  reloadConfig() {
    this.json = loadProvidersJson(this.configPath);
    const wasm = this.providers['rawrxd-local']?.wasmRuntime || null;
    this.providers = buildProviderMap(this.json, wasm);
    if (!this.providers[this.activeProvider] || !this.providers[this.activeProvider].isConfigured()) {
      const prefer = [...CONFIG_KEYS, 'rawrxd-local'];
      this.activeProvider = firstConfiguredProviderId(this.providers, prefer) || 'rawrxd-local';
    }
  }

  getActiveProviderId() {
    return this.activeProvider;
  }

  async invoke(providerName, prompt, context = {}) {
    const requested = providerName != null && String(providerName).trim() !== '' ? String(providerName).trim() : null;
    let pid = requested;
    if (!pid || !this.providers[pid] || !this.providers[pid].isConfigured()) {
      pid = this.activeProvider;
    }
    if (!this.providers[pid] || !this.providers[pid].isConfigured()) {
      pid = firstConfiguredProviderId(this.providers, [...CONFIG_KEYS, 'rawrxd-local']);
    }
    const provider = pid ? this.providers[pid] : null;
    if (!provider || !provider.isConfigured()) {
      throw new Error(
        'No AI provider is available. Enable an entry in config/providers.json (url + model) and start Ollama, or select RawrXD Local.'
      );
    }
    this.activeProvider = pid;
    return provider.invoke(prompt, {
      ...context,
      projectContext: context.projectContext || {}
    });
  }

  getAvailableProviders() {
    return Object.values(this.providers)
      .map((p) => ({
        id: p.id,
        name: p.name,
        capabilities: p.capabilities,
        enabled: p.isConfigured(),
        active: p.id === this.activeProvider
      }))
      .sort((a, b) => {
        const order = (id) => {
          const i = CONFIG_KEYS.indexOf(id);
          if (i >= 0) return i;
          return id === 'rawrxd-local' ? 99 : 50;
        };
        return order(a.id) - order(b.id);
      });
  }

  setActiveProvider(providerId) {
    const id = String(providerId || '').trim();
    const p = this.providers[id];
    if (!p || !p.isConfigured()) {
      return false;
    }
    this.activeProvider = id;
    return true;
  }
}

module.exports = AIProviderManager;
module.exports.OllamaCompatibleProvider = OllamaCompatibleProvider;
module.exports.RawrxdLocalProvider = RawrxdLocalProvider;
