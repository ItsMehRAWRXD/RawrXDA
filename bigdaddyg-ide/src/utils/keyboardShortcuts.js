/**
 * Dynamic IDE shortcuts — persisted in IdeFeaturesContext, synced to Electron menu as CmdOrCtrl+* accelerators.
 *
 * Keep in sync with App.js:
 * - paletteCommands[].accelerator (display strings use "Ctrl+…"; Electron uses CmdOrCtrl via toElectronAccelerator)
 * - onIdeAction(action) cases (menu IPC / ide:action ids)
 *
 * | id              | Default accelerator   | Palette label (App.js)           | ide:action id     |
 * |-----------------|----------------------|----------------------------------|-------------------|
 * | commandPalette  | Ctrl+Shift+P         | View: Command Palette            | command-palette   |
 * | settings        | Ctrl+,               | Preferences: Open Settings       | settings          |
 * | chat            | Ctrl+L               | Chat: Open AI Chat               | chat              |
 * | agent           | Ctrl+Shift+A         | Agent: Open Agent Panel          | agent             |
 * | modules         | Ctrl+Shift+M         | View: Extension Modules          | modules           |
 * | symbols         | Ctrl+Shift+Y         | RE: Symbols & xrefs              | symbols           |
 * | models          | Ctrl+Shift+G         | Models: Open GGUF Streamer       | models            |
 * | sidebarToggle   | Ctrl+B               | View: Toggle project sidebar     | sidebar-toggle    |
 * | openProject     | Ctrl+O               | File: Open Project…              | (palette only*)   |
 *
 * *openProject: bound in App.js keydown; not in onIdeAction switch (no ipc id). Palette-only: close-dock, etc.
 */

/** @typedef {{ requireMod: boolean, shift: boolean, alt: boolean, key: string }} ShortcutDef */

/** @type {Record<string, ShortcutDef>} */
export const DEFAULT_SHORTCUTS = {
  commandPalette: { requireMod: true, shift: true, alt: false, key: 'p' },
  settings: { requireMod: true, shift: false, alt: false, key: ',' },
  chat: { requireMod: true, shift: false, alt: false, key: 'l' },
  agent: { requireMod: true, shift: true, alt: false, key: 'a' },
  modules: { requireMod: true, shift: true, alt: false, key: 'm' },
  symbols: { requireMod: true, shift: true, alt: false, key: 'y' },
  models: { requireMod: true, shift: true, alt: false, key: 'g' },
  sidebarToggle: { requireMod: true, shift: false, alt: false, key: 'b' },
  openProject: { requireMod: true, shift: false, alt: false, key: 'o' }
};

const ACTION_ORDER = [
  'commandPalette',
  'settings',
  'chat',
  'agent',
  'modules',
  'symbols',
  'models',
  'sidebarToggle',
  'openProject'
];

function normKey(k) {
  if (k == null) return '';
  const s = String(k).trim().toLowerCase();
  if (s === 'comma' || s === ',') return ',';
  return s.length === 1 ? s : s;
}

/**
 * @param {KeyboardEvent} e
 * @returns {ShortcutDef | null}
 */
export function keyboardEventToShortcut(e) {
  const mod = e.ctrlKey || e.metaKey;
  if (!mod) return null;
  const key = e.key;
  if (!key || key === 'Control' || key === 'Meta' || key === 'Shift' || key === 'Alt') return null;
  const k = key === ',' ? ',' : key.length === 1 ? key.toLowerCase() : key.toLowerCase();
  return {
    requireMod: true,
    shift: e.shiftKey,
    alt: e.altKey,
    key: k
  };
}

/**
 * @param {KeyboardEvent} e
 * @param {ShortcutDef} s
 */
export function matchesShortcut(e, s) {
  if (!s || !s.key) return false;
  const mod = e.ctrlKey || e.metaKey;
  if (s.requireMod && !mod) return false;
  if (!s.requireMod && mod) return false;
  if (!!s.shift !== e.shiftKey) return false;
  if (!!s.alt !== e.altKey) return false;
  const want = normKey(s.key);
  const ek =
    e.key === ','
      ? ','
      : e.key.length === 1
        ? e.key.toLowerCase()
        : e.key.toLowerCase();
  return ek === want;
}

/**
 * Display string (Win/Linux style; macOS users still see Ctrl+ for clarity in shell copy).
 * @param {ShortcutDef} s
 */
export function formatShortcutDisplay(s) {
  if (!s) return '';
  const parts = [];
  parts.push('Ctrl');
  if (s.alt) parts.push('Alt');
  if (s.shift) parts.push('Shift');
  const k = s.key === ',' ? ',' : s.key.length === 1 ? s.key.toUpperCase() : s.key;
  parts.push(k === ',' ? ',' : k);
  return parts.slice(0, -1).join('+') + (parts.length > 1 && k !== ',' ? '+' : k === ',' ? '+,' : '+' + k);
}

/** Fix display for comma: "Ctrl+," */
export function formatShortcutDisplayFixed(s) {
  if (!s) return '';
  const bits = ['Ctrl'];
  if (s.alt) bits.push('Alt');
  if (s.shift) bits.push('Shift');
  if (s.key === ',') return `${bits.join('+')},`;
  const k = s.key.length === 1 ? s.key.toUpperCase() : s.key;
  return `${bits.join('+')}+${k}`;
}

/**
 * Electron accelerator (cross-platform CmdOrCtrl).
 * @param {ShortcutDef} s
 */
export function toElectronAccelerator(s) {
  if (!s) return '';
  let out = 'CmdOrCtrl';
  if (s.alt) out += '+Alt';
  if (s.shift) out += '+Shift';
  if (s.key === ',') return `${out}+,`;
  if (s.key === ' ') return `${out}+Space`;
  if (s.key.length === 1) return `${out}+${s.key.toUpperCase()}`;
  return `${out}+${s.key}`;
}

/**
 * @param {Record<string, ShortcutDef>} shortcuts
 * @returns {Record<string, string>}
 */
export function shortcutsToElectronAccelerators(shortcuts) {
  const base = {};
  for (const id of ACTION_ORDER) {
    const s = shortcuts[id] || DEFAULT_SHORTCUTS[id];
    if (s) {
      switch (id) {
        case 'commandPalette':
          base.commandPalette = toElectronAccelerator(s);
          break;
        case 'settings':
          base.settings = toElectronAccelerator(s);
          break;
        case 'chat':
          base.chat = toElectronAccelerator(s);
          break;
        case 'agent':
          base.agent = toElectronAccelerator(s);
          break;
        case 'modules':
          base.modules = toElectronAccelerator(s);
          break;
        case 'symbols':
          base.symbols = toElectronAccelerator(s);
          break;
        case 'models':
          base.models = toElectronAccelerator(s);
          break;
        case 'sidebarToggle':
          base.sidebarToggle = toElectronAccelerator(s);
          break;
        case 'openProject':
          base.openProject = toElectronAccelerator(s);
          break;
        default:
          break;
      }
    }
  }
  return base;
}

/**
 * Merge persisted shortcuts with defaults (drop unknown keys).
 * @param {Record<string, Partial<ShortcutDef>> | null | undefined} raw
 */
export function mergeShortcuts(raw) {
  const out = { ...DEFAULT_SHORTCUTS };
  if (!raw || typeof raw !== 'object') return out;
  for (const id of ACTION_ORDER) {
    const p = raw[id];
    if (!p || typeof p !== 'object') continue;
    const key = normKey(p.key);
    if (!key) continue;
    out[id] = {
      requireMod: p.requireMod !== false,
      shift: !!p.shift,
      alt: !!p.alt,
      key
    };
  }
  return out;
}

/**
 * Loose parse: "ctrl+shift+p", "Ctrl+Shift+Y", "cmd+,"
 * @param {string} text
 * @returns {ShortcutDef | null}
 */
export function parseShortcutString(text) {
  const t = String(text || '').trim().toLowerCase();
  if (!t) return null;
  const parts = t.split(/\+|·|\s+/).map((x) => x.trim()).filter(Boolean);
  if (parts.length === 0) return null;
  let shift = false;
  let alt = false;
  const keys = [];
  for (const p of parts) {
    if (p === 'shift' || p === '⇧') shift = true;
    else if (p === 'alt' || p === 'option') alt = true;
    else if (p === 'ctrl' || p === 'control' || p === 'cmd' || p === 'command' || p === 'meta' || p === '⌘') {
      /* mod implied */
    } else keys.push(p);
  }
  const last = keys[keys.length - 1];
  if (!last) return null;
  const key = last === ',' || last === 'comma' ? ',' : last.length === 1 ? last : last;
  return { requireMod: true, shift, alt, key: normKey(key) };
}

/** @returns {string[]} duplicate action ids */
export function findShortcutConflicts(shortcuts) {
  const seen = new Map();
  const dups = [];
  for (const id of ACTION_ORDER) {
    const s = shortcuts[id];
    if (!s) continue;
    const sig = `${s.requireMod ? 1 : 0}:${s.shift ? 1 : 0}:${s.alt ? 1 : 0}:${normKey(s.key)}`;
    if (seen.has(sig)) dups.push(id, seen.get(sig));
    else seen.set(sig, id);
  }
  return [...new Set(dups)];
}

/** Short labels for status-line “Keys:” hints (NoisyLayer, etc.) */
export const SHORTCUT_STATUS_HINTS = {
  commandPalette: 'palette',
  settings: 'settings',
  chat: 'chat',
  agent: 'agent',
  modules: 'modules',
  symbols: 'symbols',
  models: 'models',
  sidebarToggle: 'sidebar',
  openProject: 'project'
};

export { ACTION_ORDER };
