/**
 * Cursor / GitHub Copilot–style IDE shell state:
 * - Command palette, settings, right dock
 * - Copilot-like toggles + module flags (persisted)
 * - Noisy feedback: toasts, status line, optional UI sounds
 *
 * M01 — Intent: shell prefs and feedback only.
 * Does not: alter inference routing, approval_policy.json, or agent step execution (those live in providers / main / AgentContext).
 *
 * M02 — Persistence: `STORAGE_KEY` + `LAST_DOCK_TAB_KEY` in localStorage. Reset modules via `resetModulesToDefaults`;
 * reset shortcuts via `resetShortcutsToDefaults`; clear keys manually for a full shell reset (no global “restore all defaults” UI here).
 *
 * M05 — Dev logs: `noisyLog` / `noisyLogVerbose` use a stable `[shell]` prefix (grep-friendly).
 *
 * M06 — Defaults: conservative agent toggles (e.g. writes not auto-approved). Main-process inference uses
 * `config/providers.json` (Ollama `/api/chat`) plus optional `rawrxd-local` when selected.
 *
 * M07 — Verify: open app → toggle a shell setting → reload → value persists; open dock tab → close → reopen → last tab restores when enabled.
 *
 * A11y batch 1 — Parity with Win32 UltimateAgenticChatSystem + IDE themes: high-contrast dock chrome, reduced motion,
 * screen-reader announcements for agent lifecycle, text scale for AI panels, enhanced focus, verbose local-model hints, skip link target.
 */
import React, {
  createContext,
  useContext,
  useState,
  useCallback,
  useEffect,
  useMemo,
  useRef
} from 'react';
import { playUiSound as rawPlayUiSound, playNoiseCelebration } from '../utils/noisyAudio';
import { DEFAULT_SHORTCUTS, mergeShortcuts } from '../utils/keyboardShortcuts';

const STORAGE_KEY = 'rawrxd.ide.shell.v1';

const defaultSettings = {
  temperature: 0.2,
  maxTokens: 4096,
  /** Copilot-like: show ghost-style hints in editor (Monaco inline completions when enabled) */
  copilotInlineHints: true,
  /** Auto-run mutating agent steps (write/append) without approval */
  agentAutoApprove: false,
  /** Auto-run read-only steps (read_file, list_dir, search_repo) without approval */
  agentAutoApproveReads: true,
  /** Pause before every ask_ai step (human gate for LLM-only actions) */
  agentRequireAskAiApproval: false,
  /** Final LLM reflection pass after step execution */
  agentReflectionEnabled: true,
  /** Approve the full JSON plan before any step executes */
  agentRequirePlanApproval: false,
  /** Single approval for all write/append steps in the plan (when writes are not auto-approved) */
  agentBatchApproveMutations: false,
  /** Snapshot files before agent writes (enables rollback) */
  agentEnableRollbackSnapshots: true,
  /**
   * Master switch: when false, main-process `agent:start` refuses new tasks (Settings › Copilot).
   * Does not stop in-flight tasks.
   */
  agentAutonomousEnabled: true,
  /**
   * When true, planner may emit `run_terminal` steps; main runs bounded `spawn(..., { shell, cwd: projectRoot })`.
   * Does not stream into the UI xterm — separate from the integrated terminal panel.
   */
  agentTerminalEnabled: false,
  /** Auto-run run_terminal without approval (dangerous; requires agentTerminalEnabled). */
  agentAutoApproveTerminal: false,
  /**
   * When true, planner may emit `task` steps (parallel or sequential sub-goals); each sub-goal runs a full nested plan.
   */
  agentSwarmingEnabled: false,
  /** Require approval before each `task` / swarm step (when swarming is enabled). */
  agentRequireSubagentApproval: true,
  /** Raise planner + ask_ai token budgets (capped in orchestrator). */
  agentMaxMode: false,
  /** Extra planner headroom + larger reflection pass (real token bumps in main orchestrator). */
  agentDeepThinking: false,
  /** Max characters of workspace tree text injected into the planner prompt (truncation is explicit in logs). */
  agentContextLimitChars: 48000,
  /** E03: at session start, set toolbar AI provider to bigdaddyg when enabled in providers.json (Ollama). */
  preferLocalInferenceFirst: true,
  /** Include open file in every chat request */
  attachActiveFileToChat: true,
  /** System prompt flavor */
  chatPersona: 'copilot', // copilot | cursor | raw
  /** Web Audio bleeps for palette, dock, chat, etc. */
  uiSounds: true,
  /** Show toast notifications for major actions */
  showToastNotifications: true,
  /** Legacy alias for showToastNotifications (kept for backward compatibility) */
  noisyToasts: true,
  /** 'normal' | 'maximum' — overall UI feedback intensity (extra motion + sounds) */
  feedbackIntensity: 'normal',
  /** When true, shell emits tagged dev console output (see noisyLog / M05). */
  noisyConsole: true,
  /**
   * Preferred, descriptive name for dev console logging control.
   * Mirrors `noisyConsole` but is clearer for new callers.
   */
  enableDevConsoleLogging: true,
  /** M13 — tagged verbose lines when dev + (this flag OR noise MAX) */
  verboseDevLogs: false,
  /** 'normal' | 'maximum' — extra motion + sounds */
  noiseIntensity: 'normal',
  /** 'wasm-local' | 'ollama-http' — WASM is renderer-only; HTTP uses main-process ai:invoke. */
  chatTransport: 'wasm-local',
  /** Relative to page origin or absolute URL; default matches `invokeRawrxdWasmChat` default asset path */
  wasmChatUrl: '/rawrxd-inference.wasm',

  /** Win32 parity: IDM_THEME_HIGH_CONTRAST–style chrome for Chat / Agent / Models dock */
  accessibilityHighContrastAgentPanels: false,
  /** Force reduced UI motion (dock animation, chat scroll) */
  accessibilityReducedMotion: false,
  /** Honor prefers-reduced-motion (Win32 accessibility / system setting) */
  accessibilityRespectSystemReducedMotion: true,
  /** aria-live announcements for agent task status (screen readers) */
  accessibilityAnnounceAgentStatus: true,
  /** 100–150 — scales Chat + Agent + Models body text (UltimateAgenticChatSystem accessibilityTextScale) */
  accessibilityAgentTextScalePercent: 100,
  /** Stronger :focus-visible outlines on interactive controls */
  accessibilityEnhancedFocusRings: true,
  /** Extra aria-describedby copy for WASM + GGUF streamer controls */
  accessibilityVerboseLocalModelHints: true,
  /** Legacy Ollama probe URL (unused in local-only mode; retained for compatibility). */
  ollamaProbeBaseUrl: 'http://127.0.0.1:11434'
};

const defaultModules = {
  gitIntegration: true,
  testRunner: true,
  lspBridge: false,
  inlineCompletion: true,
  commandPalette: true,
  telemetryOptIn: false
};

function loadPersisted() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return {};
    return JSON.parse(raw);
  } catch {
    return {};
  }
}

function savePersisted(settings, modules, shortcuts) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ settings, modules, shortcuts }));
  } catch {
    /* ignore */
  }
}

function nextToastId() {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) return crypto.randomUUID();
  return `t-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;
}

const IdeFeaturesContext = createContext(null);

const LAST_DOCK_TAB_KEY = 'rawrxd.ide.lastDockTab';
const VALID_DOCK_TABS = new Set(['chat', 'agent', 'modules', 'symbols', 'models']);

function readInitialDockTab() {
  try {
    const v = localStorage.getItem(LAST_DOCK_TAB_KEY);
    if (v && VALID_DOCK_TABS.has(v)) return v;
  } catch {
    /* ignore */
  }
  return null;
}

/**
 * Provides IDE shell UI state (dock, toasts, status, sounds, persisted toggles).
 * Does not: run the agent loop, open projects, or call Ollama/WASM — consumers do that via other contexts / IPC.
 */
export function IdeFeaturesProvider({ children }) {
  const persisted = loadPersisted();
  const [settings, setSettingsState] = useState(() => ({ ...defaultSettings, ...persisted.settings }));
  const [modules, setModulesState] = useState({ ...defaultModules, ...persisted.modules });
  const [shortcuts, setShortcutsState] = useState(() => mergeShortcuts(persisted.shortcuts));
  const [systemPrefersReducedMotion, setSystemPrefersReducedMotion] = useState(false);
  const a11yLiveRef = useRef(null);
  const a11yAssertiveRef = useRef(null);
  const [commandPaletteOpen, setCommandPaletteOpen] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  /** 'chat' | 'agent' | 'modules' | 'symbols' | 'models' | null — M02 restore last tab from localStorage */
  const [rightDockTab, setRightDockTab] = useState(readInitialDockTab);
  const [toasts, setToasts] = useState([]);
  const [statusLine, setStatusLine] = useState('Ready — Ctrl+Shift+P palette · Ctrl+, settings · Ctrl+L chat · Ctrl+Shift+G models');

  useEffect(() => {
    savePersisted(settings, modules, shortcuts);
  }, [settings, modules, shortcuts]);

  useEffect(() => {
    if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') return undefined;
    const mq = window.matchMedia('(prefers-reduced-motion: reduce)');
    const onChange = () => setSystemPrefersReducedMotion(Boolean(mq.matches));
    onChange();
    mq.addEventListener('change', onChange);
    return () => mq.removeEventListener('change', onChange);
  }, []);

  const accessibilityReducedMotionEffective = useMemo(() => {
    if (settings.accessibilityReducedMotion) return true;
    if (settings.accessibilityRespectSystemReducedMotion && systemPrefersReducedMotion) return true;
    return false;
  }, [
    settings.accessibilityReducedMotion,
    settings.accessibilityRespectSystemReducedMotion,
    systemPrefersReducedMotion
  ]);

  useEffect(() => {
    if (typeof document === 'undefined') return;
    const root = document.documentElement;
    const pct = Number(settings.accessibilityAgentTextScalePercent);
    const scale = Number.isFinite(pct) ? Math.min(1.5, Math.max(1, pct / 100)) : 1;
    root.style.setProperty('--rawrxd-a11y-text-scale', String(scale));
    if (settings.accessibilityHighContrastAgentPanels) {
      root.setAttribute('data-rawrxd-a11y-high-contrast', '1');
    } else {
      root.removeAttribute('data-rawrxd-a11y-high-contrast');
    }
    if (settings.accessibilityEnhancedFocusRings) {
      root.setAttribute('data-rawrxd-a11y-focus', '1');
    } else {
      root.removeAttribute('data-rawrxd-a11y-focus');
    }
    if (accessibilityReducedMotionEffective) {
      root.setAttribute('data-rawrxd-a11y-reduced-motion', '1');
    } else {
      root.removeAttribute('data-rawrxd-a11y-reduced-motion');
    }
  }, [
    settings.accessibilityHighContrastAgentPanels,
    settings.accessibilityEnhancedFocusRings,
    settings.accessibilityAgentTextScalePercent,
    accessibilityReducedMotionEffective
  ]);

  /**
   * Screen-reader announcements (agent/chat/model). Polite uses ref flush; assertive for errors.
   * @param {string} message
   * @param {{ assertive?: boolean }} [opts]
   */
  const announceA11y = useCallback(
    (message, opts = {}) => {
      if (!settings.accessibilityAnnounceAgentStatus) return;
      const text = typeof message === 'string' ? message.trim().slice(0, 800) : '';
      if (!text) return;
      const stamp = `${text} (${Date.now()})`;
      if (opts.assertive) {
        const el = a11yAssertiveRef.current;
        if (!el) return;
        el.textContent = '';
        window.requestAnimationFrame(() => {
          el.textContent = stamp;
        });
      } else {
        const el = a11yLiveRef.current;
        if (!el) return;
        el.textContent = '';
        window.requestAnimationFrame(() => {
          el.textContent = stamp;
        });
      }
    },
    [settings.accessibilityAnnounceAgentStatus]
  );

  useEffect(() => {
    try {
      if (rightDockTab) {
        localStorage.setItem(LAST_DOCK_TAB_KEY, rightDockTab);
      } else {
        localStorage.removeItem(LAST_DOCK_TAB_KEY);
      }
    } catch {
      /* ignore */
    }
  }, [rightDockTab]);

  const setSettings = useCallback((patch) => {
    setSettingsState((s) => ({ ...s, ...patch }));
  }, []);

  const toggleModule = useCallback((key) => {
    setModulesState((m) => ({ ...m, [key]: !m[key] }));
  }, []);

  const setShortcut = useCallback((id, def) => {
    setShortcutsState((prev) => mergeShortcuts({ ...prev, [id]: def }));
  }, []);

  const resetShortcutsToDefaults = useCallback(() => {
    setShortcutsState(mergeShortcuts(null));
  }, []);

  /**
   * M02 — restore extension module flags to shipped defaults (persisted on next save).
   * Does not: reset AI provider selection, IRC, or full `STORAGE_KEY` — only the `modules` slice.
   */
  const resetModulesToDefaults = useCallback(() => {
    setModulesState({ ...defaultModules });
  }, []);

  const openRightDock = useCallback((tab) => {
    setRightDockTab(tab);
  }, []);

  const toggleRightDock = useCallback((tab) => {
    setRightDockTab((t) => (t === tab ? null : tab));
  }, []);

  const playUiSound = useCallback(
    (kind) => {
      if (!settings.uiSounds) return;
      rawPlayUiSound(kind, { intensity: settings.noiseIntensity });
    },
    [settings.uiSounds, settings.noiseIntensity]
  );

  /**
   * Dev-only console line with stable `[shell]` prefix (requires `noisyConsole` + dev build).
   * Does not: update `statusLine` or send IPC — call `setStatusLine` / IPC separately if needed.
   */
  const noisyLog = useCallback(
    (...args) => {
      if (!settings.noisyConsole) return;
      try {
        if (process.env.NODE_ENV === 'development' || window.electronAPI?.isDev) {
          // eslint-disable-next-line no-console
          console.log('[shell]', ...args);
        }
      } catch {
        /* ignore */
      }
    },
    [settings.noisyConsole]
  );

  /**
   * M13 — category-tagged dev logs when verbose flag or MAX noise (still requires `noisyConsole` + dev).
   * Does not: replace structured main-process logging.
   */
  const noisyLogVerbose = useCallback(
    (tag, ...args) => {
      if (!settings.noisyConsole) return;
      if (!settings.verboseDevLogs && settings.noiseIntensity !== 'maximum') return;
      try {
        if (process.env.NODE_ENV === 'development' || window.electronAPI?.isDev) {
          // eslint-disable-next-line no-console
          console.log('[shell]', `[${tag}]`, ...args);
        }
      } catch {
        /* ignore */
      }
    },
    [settings.noisyConsole, settings.verboseDevLogs, settings.noiseIntensity]
  );

  const featureGates = useMemo(
    () => ({
      /** M11 — build-time gate for experimental shell bits (CRA/Vite: set at bundle time). */
      experimentalUi: String(process.env.REACT_APP_RAWRXD_EXPERIMENTAL || '').trim() === '1'
    }),
    []
  );

  /**
   * Queues a toast when `noisyToasts` is on and `message` is non-empty.
   * Does not: persist to disk or mirror to the main-process log — renderer-only.
   */
  const pushToast = useCallback(
    ({ title, message, variant = 'info', durationMs = 4200 }) => {
      if (!settings.noisyToasts || !message) return;
      const id = nextToastId();
      const max = settings.noiseIntensity === 'maximum';
      setToasts((list) => {
        const next = [...list, { id, title, message, variant }];
        return next.slice(-(max ? 16 : 8));
      });
      if (durationMs > 0) {
        window.setTimeout(() => {
          setToasts((list) => list.filter((t) => t.id !== id));
        }, max ? durationMs + 1200 : durationMs);
      }
    },
    [settings.noisyToasts, settings.noiseIntensity]
  );

  const dismissToast = useCallback((id) => {
    setToasts((list) => list.filter((t) => t.id !== id));
  }, []);

  const celebrate = useCallback(() => {
    if (!settings.uiSounds) return;
    if (settings.noiseIntensity === 'maximum') playNoiseCelebration();
    else rawPlayUiSound('success', { intensity: settings.noiseIntensity });
  }, [settings.uiSounds, settings.noiseIntensity]);

  const value = {
    settings,
    setSettings,
    modules,
    toggleModule,
    resetModulesToDefaults,
    shortcuts,
    setShortcut,
    resetShortcutsToDefaults,
    commandPaletteOpen,
    setCommandPaletteOpen,
    settingsOpen,
    setSettingsOpen,
    rightDockTab,
    setRightDockTab,
    openRightDock,
    toggleRightDock,
    toasts,
    dismissToast,
    statusLine,
    setStatusLine,
    pushToast,
    playUiSound,
    noisyLog,
    noisyLogVerbose,
    featureGates,
    celebrate,
    accessibilityReducedMotionEffective,
    announceA11y
  };

  return (
    <IdeFeaturesContext.Provider value={value}>
      <div
        ref={a11yLiveRef}
        role="status"
        aria-live={settings.accessibilityAnnounceAgentStatus ? 'polite' : 'off'}
        aria-relevant="additions text"
        aria-atomic="true"
        className="sr-only"
      />
      <div
        ref={a11yAssertiveRef}
        role="alert"
        aria-live={settings.accessibilityAnnounceAgentStatus ? 'assertive' : 'off'}
        aria-atomic="true"
        className="sr-only"
      />
      {children}
    </IdeFeaturesContext.Provider>
  );
}

/**
 * Hook for shell features (status line, toasts, dock tab, settings patch).
 * Does not: start agents or load models — use AgentContext / IPC for that.
 */
export function useIdeFeatures() {
  const ctx = useContext(IdeFeaturesContext);
  if (!ctx) throw new Error('useIdeFeatures must be used within IdeFeaturesProvider');
  return ctx;
}

export default IdeFeaturesContext;
