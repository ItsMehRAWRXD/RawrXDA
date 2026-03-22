import React from 'react';
import { motion, AnimatePresence } from 'framer-motion';
// @ts-ignore TS1261: workspace opened with mixed root casing (d:/RawrXD vs d:/rawrxd)
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
// @ts-ignore TS1261: workspace opened with mixed root casing (d:/RawrXD vs d:/rawrxd)
import { ACTION_ORDER, findShortcutConflicts, formatShortcutDisplayFixed, keyboardEventToShortcut } from '../utils/keyboardShortcuts';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  MINIMALISTIC_DOC,
  focusVisibleRing
} from '../utils/minimalisticM08M14';

const KEYBOARD_ACTION_LABELS = {
  commandPalette: 'Command palette',
  settings: 'Open settings',
  chat: 'AI chat (dock)',
  agent: 'Agent panel',
  modules: 'Extension modules',
  symbols: 'Symbols & xrefs',
  models: 'GGUF streamer',
  sidebarToggle: 'Toggle project sidebar',
  openProject: 'Open project…'
};

const tabs = [
  { id: 'general', label: 'General' },
  { id: 'ai', label: 'AI & models' },
  { id: 'copilot', label: 'Copilot / Cursor' },
  { id: 'accessibility', label: 'Accessibility' },
  { id: 'noise', label: 'Noise & feedback' },
  { id: 'keys', label: 'Keyboard' }
];

const SettingsPanel = ({ open, onClose, settings, setSettings }) => {
  const {
    shortcuts,
    setShortcut,
    resetShortcutsToDefaults,
    pushToast,
    setStatusLine,
    noisyLog,
    accessibilityReducedMotionEffective
  } = useIdeFeatures();
  const [recordingAction, setRecordingAction] = React.useState(null);
  const [tab, setTab] = React.useState('general');

  React.useEffect(() => {
    if (!open) return;
    noisyLog('[settings]', 'panel opened');
    setStatusLine('settings: open');
  }, [open, noisyLog, setStatusLine]);

  React.useEffect(() => {
    if (recordingAction == null) return undefined;
    const onKey = (e) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        setRecordingAction(null);
        return;
      }
      const s = keyboardEventToShortcut(e);
      if (!s) return;
      e.preventDefault();
      e.stopPropagation();
      setShortcut(recordingAction, s);
      setRecordingAction(null);
    };
    window.addEventListener('keydown', onKey, true);
    return () => window.removeEventListener('keydown', onKey, true);
  }, [recordingAction, setShortcut]);

  const shortcutConflicts = React.useMemo(() => findShortcutConflicts(shortcuts), [shortcuts]);

  return (
    <AnimatePresence>
      {open && (
      <motion.div
        className="fixed inset-0 z-[190] flex items-center justify-center bg-black/60 p-4"
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        exit={{ opacity: 0 }}
        onMouseDown={(e) => e.target === e.currentTarget && onClose()}
      >
        <motion.div
          className="w-full max-w-2xl max-h-[85vh] rounded-xl border border-gray-600 bg-ide-sidebar flex flex-col shadow-2xl"
          initial={{ scale: 0.96, opacity: 0 }}
          animate={{ scale: 1, opacity: 1 }}
          exit={{ scale: 0.96, opacity: 0 }}
        >
          <div className="flex border-b border-gray-700">
            <div
              className="flex-1 px-4 py-3 font-semibold text-white"
              title="Shell preferences and IRC form. Copilot/agent toggles persist with other shell settings unless noted."
            >
              Settings
            </div>
            <button
              type="button"
              onClick={onClose}
              className="px-4 text-gray-400 hover:text-white"
              aria-label="Close settings"
              title="Close — unsaved IRC edits stay in memory until you Save IRC config."
            >
              ✕
            </button>
          </div>
          <div className="flex flex-1 min-h-0">
            <nav className="w-40 border-r border-gray-700 py-2 shrink-0">
              {tabs.map((t) => (
                <button
                  key={t.id}
                  type="button"
                  onClick={() => {
                    setTab(t.id);
                    noisyLog('[settings]', 'tab', t.id);
                    setStatusLine(`settings: ${t.label}`);
                  }}
                  className={`w-full text-left px-3 py-2 text-sm ${
                    tab === t.id ? 'bg-ide-accent/20 text-white' : 'text-gray-400 hover:bg-gray-700/40'
                  }`}
                  title={`${t.label} — values persist via shell storage or IRC save where noted.`}
                >
                  {t.label}
                </button>
              ))}
            </nav>
            <div className="flex-1 overflow-y-auto p-4 text-sm text-gray-200 space-y-4">
              {tab === 'general' && (
                <>
                  <p className="text-gray-400 text-xs">
                    RawrXD shell — Cursor / Copilot–style preferences (persisted locally).
                  </p>
                  <p className="text-[10px] text-gray-500 border border-gray-700/80 rounded p-2 bg-gray-950/40">
                    Support snapshot export is disabled in this Win32-focused lane. Use local logs and the copy support line
                    action below for diagnostics.
                  </p>
                  <label
                    className="flex items-center gap-2"
                    title="Includes the open editor buffer in chat prompts when supported. Does not auto-save the file to disk."
                  >
                    <input
                      type="checkbox"
                      checked={settings.attachActiveFileToChat}
                      onChange={(e) => setSettings({ attachActiveFileToChat: e.target.checked })}
                    />
                    Attach active file to chat requests
                  </label>
                </>
              )}
              {tab === 'ai' && (
                <>
                  {settings.accessibilityVerboseLocalModelHints ? (
                    <p id="rawrxd-settings-chat-transport-hint" className="sr-only">
                      Local WASM chat runs RawrXD inference in this browser window. External HTTP providers are disabled
                      in this build mode.
                    </p>
                  ) : null}
                  <div
                    className="rounded border border-gray-700/80 p-3 space-y-2 bg-black/20"
                    role="group"
                    aria-label="Chat transport: local WASM"
                    {...(settings.accessibilityVerboseLocalModelHints
                      ? { 'aria-describedby': 'rawrxd-settings-chat-transport-hint' }
                      : {})}
                  >
                    <p className="text-gray-400 text-xs font-medium">Chat transport (dock Chat panel)</p>
                    <p className="text-gray-500 text-xs">
                      <strong className="text-gray-200">Local WASM</strong> is always enabled in this mode.
                    </p>
                    <label className="block">
                      <span className="text-gray-400 text-xs">WASM file name (local only - no http://)</span>
                      <input
                        className="mt-1 w-full bg-ide-bg border border-gray-600 rounded px-2 py-1 font-mono text-xs"
                        value={settings.wasmChatUrl || '/rawrxd-inference.wasm'}
                        onChange={(e) => setSettings({ wasmChatUrl: e.target.value })}
                        placeholder="/rawrxd-inference.wasm"
                      />
                      <p className="text-[10px] text-gray-500 mt-1">
                        Build: <code className="text-gray-400">npm run build:wasm-inference</code> →{' '}
                        <code className="text-gray-400">public/rawrxd-inference.wasm</code>.{' '}
                        Browser shell uses embedded default bytes only; HTTP URLs are rejected.{' '}
                        <code className="text-gray-400">docs/ELECTRON_WASM_CHAT.md</code>
                      </p>
                    </label>
                  </div>
                  <div>
                    <label className="block text-gray-400 mb-1">Temperature</label>
                    <input
                      type="range"
                      min="0"
                      max="1"
                      step="0.05"
                      value={settings.temperature}
                      onChange={(e) => setSettings({ temperature: parseFloat(e.target.value) })}
                      className="w-full"
                    />
                    <span className="text-xs text-gray-500">{settings.temperature}</span>
                  </div>
                  <div>
                    <label className="block text-gray-400 mb-1">Max tokens</label>
                    <select
                      value={settings.maxTokens}
                      onChange={(e) => setSettings({ maxTokens: Number(e.target.value) })}
                      className="bg-ide-bg border border-gray-600 rounded px-2 py-1 w-full"
                    >
                      {[1024, 2048, 4096, 8192, 16384].map((n) => (
                        <option key={n} value={n}>
                          {n}
                        </option>
                      ))}
                    </select>
                  </div>
                  <p className="text-[10px] text-gray-500">
                    Self-contained mode: external Ollama probes and HTTP provider routing are disabled.
                  </p>
                </>
              )}
              {tab === 'accessibility' && (
                <>
                  <p className="text-gray-400 text-xs">
                    Matches Win32 RawrXD patterns: high contrast themes, reduced motion, focus indicators, and agent
                    screen-reader cues — tuned for local lanes (WASM chat + GGUF streamer).
                  </p>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={Boolean(settings.accessibilityHighContrastAgentPanels)}
                      onChange={(e) => setSettings({ accessibilityHighContrastAgentPanels: e.target.checked })}
                    />
                    High-contrast right dock (Chat · Agent · Models)
                  </label>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={Boolean(settings.accessibilityReducedMotion)}
                      onChange={(e) => setSettings({ accessibilityReducedMotion: e.target.checked })}
                    />
                    Reduce motion (dock animation, scrolling)
                  </label>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={settings.accessibilityRespectSystemReducedMotion !== false}
                      onChange={(e) => setSettings({ accessibilityRespectSystemReducedMotion: e.target.checked })}
                    />
                    Honor system &quot;prefers-reduced-motion&quot;
                  </label>
                  <p className="text-[10px] text-gray-500">
                    Effective reduced motion now:{' '}
                    <strong className="text-gray-300">{accessibilityReducedMotionEffective ? 'on' : 'off'}</strong>
                  </p>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={settings.accessibilityAnnounceAgentStatus !== false}
                      onChange={(e) => setSettings({ accessibilityAnnounceAgentStatus: e.target.checked })}
                    />
                    Announce agent status to screen readers (polite live region)
                  </label>
                  <div>
                    <label className="block text-gray-400 mb-1">
                      AI panel text size ({settings.accessibilityAgentTextScalePercent || 100}%)
                    </label>
                    <input
                      type="range"
                      min={100}
                      max={150}
                      step={5}
                      value={Number(settings.accessibilityAgentTextScalePercent) || 100}
                      onChange={(e) =>
                        setSettings({ accessibilityAgentTextScalePercent: Number(e.target.value) || 100 })
                      }
                      className="w-full"
                    />
                    <p className="text-[10px] text-gray-500 mt-1">
                      Scales Chat, Agent, and Models dock text (Win32{' '}
                      <code className="text-gray-400">accessibilityTextScale</code> analogue).
                    </p>
                  </div>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={settings.accessibilityEnhancedFocusRings !== false}
                      onChange={(e) => setSettings({ accessibilityEnhancedFocusRings: e.target.checked })}
                    />
                    Stronger focus rings in the AI dock
                  </label>
                  <label className="flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={settings.accessibilityVerboseLocalModelHints !== false}
                      onChange={(e) => setSettings({ accessibilityVerboseLocalModelHints: e.target.checked })}
                    />
                    Verbose descriptions for local model controls (WASM + GGUF)
                  </label>
                  <p className="text-[10px] text-gray-500 border border-gray-700/80 rounded p-2 bg-gray-950/40">
                    Keyboard: use <strong className="text-gray-300">Tab</strong> from the top of the window; the first
                    focusable control is a skip link to the AI dock (Chat, agent, models). Approve / Deny in the Agent
                    panel are labeled for assistive tech.
                  </p>
                </>
              )}
              {tab === 'copilot' && (
                <>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.copilotInlineHints}
                      onChange={(e) => setSettings({ copilotInlineHints: e.target.checked })}
                    />
                    Inline hints (Copilot-style ghost text — when editor supports it)
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentAutoApprove}
                      onChange={(e) => setSettings({ agentAutoApprove: e.target.checked })}
                    />
                    Auto-approve agent writes (write/append)
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentAutoApproveReads}
                      onChange={(e) => setSettings({ agentAutoApproveReads: e.target.checked })}
                    />
                    Auto-approve agent reads (read/list/search_repo)
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentRequireAskAiApproval}
                      onChange={(e) => setSettings({ agentRequireAskAiApproval: e.target.checked })}
                    />
                    Require approval for each ask_ai step
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentReflectionEnabled}
                      onChange={(e) => setSettings({ agentReflectionEnabled: e.target.checked })}
                    />
                    Agent reflection pass after steps
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentRequirePlanApproval}
                      onChange={(e) => setSettings({ agentRequirePlanApproval: e.target.checked })}
                    />
                    Require approval for full plan before execution
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentBatchApproveMutations}
                      onChange={(e) => setSettings({ agentBatchApproveMutations: e.target.checked })}
                    />
                    Batch-approve all write/append steps (one gate for the whole mutation queue)
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.agentEnableRollbackSnapshots}
                      onChange={(e) => setSettings({ agentEnableRollbackSnapshots: e.target.checked })}
                    />
                    Snapshot files before agent writes (rollback in Agent panel)
                  </label>
                  <p className="text-[10px] text-gray-500 border border-gray-700/80 rounded p-2 bg-gray-950/40">
                    <strong className="text-gray-300">Agent usage &amp; power</strong> — these map to real main-process
                    policy (orchestrator + bounded <code className="text-cyan-300">spawn</code> for shell). No fictional
                    toggles.
                  </p>
                  <label className="flex items-center gap-2" title="When off, agent:start IPC refuses new tasks.">
                    <input
                      type="checkbox"
                      checked={settings.agentAutonomousEnabled !== false}
                      onChange={(e) => setSettings({ agentAutonomousEnabled: e.target.checked })}
                    />
                    Enable autonomous agent (new tasks)
                  </label>
                  <label
                    className="flex items-center gap-2"
                    title="Allows planner run_terminal steps — one-shot shell under project root in main (not the UI xterm)."
                  >
                    <input
                      type="checkbox"
                      checked={Boolean(settings.agentTerminalEnabled)}
                      onChange={(e) => setSettings({ agentTerminalEnabled: e.target.checked })}
                    />
                    Agent terminal / shell steps (run_terminal)
                  </label>
                  <label
                    className="flex items-center gap-2"
                    title="Auto-run run_terminal without approval — extremely dangerous."
                  >
                    <input
                      type="checkbox"
                      checked={Boolean(settings.agentAutoApproveTerminal)}
                      onChange={(e) => setSettings({ agentAutoApproveTerminal: e.target.checked })}
                    />
                    Auto-approve agent shell steps
                  </label>
                  <label
                    className="flex items-center gap-2"
                    title="Allows task steps: nested full agent plans per sub-goal (parallel or sequential)."
                  >
                    <input
                      type="checkbox"
                      checked={Boolean(settings.agentSwarmingEnabled)}
                      onChange={(e) => setSettings({ agentSwarmingEnabled: e.target.checked })}
                    />
                    Swarming (task / sub-agent steps)
                  </label>
                  <label className="flex items-center gap-2" title="Pause before each swarm task step unless off.">
                    <input
                      type="checkbox"
                      checked={settings.agentRequireSubagentApproval !== false}
                      onChange={(e) => setSettings({ agentRequireSubagentApproval: e.target.checked })}
                    />
                    Require approval for swarm (task) steps
                  </label>
                  <label
                    className="flex items-center gap-2"
                    title="Raises planner and ask_ai token caps in the orchestrator (capped at 32k/16k)."
                  >
                    <input
                      type="checkbox"
                      checked={Boolean(settings.agentMaxMode)}
                      onChange={(e) => setSettings({ agentMaxMode: e.target.checked })}
                    />
                    Max mode (higher token budgets)
                  </label>
                  <label
                    className="flex items-center gap-2"
                    title="Extra planner headroom, larger reflection, plus a second critique LLM pass."
                  >
                    <input
                      type="checkbox"
                      checked={Boolean(settings.agentDeepThinking)}
                      onChange={(e) => setSettings({ agentDeepThinking: e.target.checked })}
                    />
                    Deep thinking (extra reflection pass + tokens)
                  </label>
                  <div>
                    <label className="block text-gray-400 mb-1" title="Truncates workspace tree text in the planner prompt.">
                      Agent context limit (chars)
                    </label>
                    <input
                      type="number"
                      min={2000}
                      max={200000}
                      step={1000}
                      value={Number(settings.agentContextLimitChars) || 48000}
                      onChange={(e) => {
                        const n = Number(e.target.value);
                        if (!Number.isFinite(n)) return;
                        setSettings({
                          agentContextLimitChars: Math.min(200000, Math.max(2000, Math.floor(n)))
                        });
                      }}
                      className="bg-ide-bg border border-gray-600 rounded px-2 py-1 w-full max-w-xs"
                    />
                  </div>
                  <div>
                    <label className="block text-gray-400 mb-1">Chat persona</label>
                    <select
                      value={settings.chatPersona}
                      onChange={(e) => setSettings({ chatPersona: e.target.value })}
                      className="bg-ide-bg border border-gray-600 rounded px-2 py-1 w-full"
                    >
                      <option value="copilot">GitHub Copilot</option>
                      <option value="cursor">Cursor Agent</option>
                      <option value="raw">RawrXD default</option>
                    </select>
                  </div>
                </>
              )}
              {tab === 'noise' && (
                <>
                  <p className="text-gray-400 text-xs">
                    Turn the shell up to eleven: bleeps, toasts, scrolling status, and extra Monaco inline noise.
                  </p>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.uiSounds}
                      onChange={(e) => setSettings({ uiSounds: e.target.checked })}
                    />
                    UI sounds (Web Audio palette / dock / chat)
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.noisyToasts}
                      onChange={(e) => setSettings({ noisyToasts: e.target.checked })}
                    />
                    Toast notifications
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={settings.noisyConsole}
                      onChange={(e) => setSettings({ noisyConsole: e.target.checked })}
                    />
                    Verbose dev console <span className="text-gray-500">[RAWRXD]</span>
                  </label>
                  <label className="flex items-center gap-2">
                    <input
                      type="checkbox"
                      checked={Boolean(settings.verboseDevLogs)}
                      onChange={(e) => setSettings({ verboseDevLogs: e.target.checked })}
                    />
                    M13 — Tagged verbose logs <span className="text-gray-500">(noisyLogVerbose in dev)</span>
                  </label>
                  <div>
                    <label className="block text-gray-400 mb-1">Noise intensity</label>
                    <select
                      value={settings.noiseIntensity}
                      onChange={(e) => setSettings({ noiseIntensity: e.target.value })}
                      className="bg-ide-bg border border-gray-600 rounded px-2 py-1 w-full"
                    >
                      <option value="normal">Normal</option>
                      <option value="maximum">Maximum — fanfare, extra motion, palette party</option>
                    </select>
                  </div>
                </>
              )}
              {tab === 'keys' && (
                <div className="space-y-3 text-xs text-gray-300">
                  <p className="text-gray-400">
                    Shortcuts persist with the shell and sync to the Electron menu. Click <strong>Set…</strong>, then press
                    the new combo (Esc to cancel).
                  </p>
                  {shortcutConflicts.length > 0 && (
                    <p className="text-amber-400 text-[11px] border border-amber-800/50 rounded p-2 bg-amber-950/20">
                      Duplicate bindings: {shortcutConflicts.join(', ')} — change one of them.
                    </p>
                  )}
                  {recordingAction && (
                    <p className="text-cyan-300 text-[11px] animate-pulse">
                      Recording for <strong>{KEYBOARD_ACTION_LABELS[recordingAction] || recordingAction}</strong> — press
                      keys (Esc cancel).
                    </p>
                  )}
                  <ul className="space-y-2 font-mono">
                    {ACTION_ORDER.map((id) => (
                      <li key={id} className="flex items-center justify-between gap-2 flex-wrap">
                        <span className="text-gray-400 shrink-0 min-w-[10rem]">
                          {KEYBOARD_ACTION_LABELS[id] || id}
                        </span>
                        <kbd className="bg-gray-800 px-2 py-0.5 rounded text-gray-200">
                          {formatShortcutDisplayFixed(shortcuts[id])}
                        </kbd>
                        <button
                          type="button"
                          className="text-[10px] px-2 py-1 rounded border border-gray-600 text-cyan-400 hover:bg-gray-800"
                          onClick={() => setRecordingAction(id)}
                        >
                          Set…
                        </button>
                      </li>
                    ))}
                  </ul>
                  <button
                    type="button"
                    className={`text-[11px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white ${focusVisibleRing}`}
                    title="M02 — Restores default key bindings in shell storage. Does not change IRC or AI model files."
                    onClick={() => {
                      resetShortcutsToDefaults();
                      noisyLog('[settings]', 'shortcuts reset to defaults');
                      setStatusLine('settings: keyboard shortcuts reset');
                    }}
                  >
                    Reset all shortcuts to defaults
                  </button>
                </div>
              )}
              <MinimalSurfaceM814Footer
                surfaceId="settings"
                offlineHint="UI works offline; IRC/bridge actions need Electron main + network when used."
                docPath={MINIMALISTIC_DOC}
                m13Hint="IRC → docs/IRC_MIRC_IDE_BRIDGE.md · AI → docs/INFERENCE_PATH_MATRIX.md · M14: no secrets in this line."
                className="mt-4"
              >
                <div className="flex flex-wrap gap-1 px-1 pb-1">
                  <CopySupportLineButton
                    getText={() => `[settings] tab=${tab} (no secrets)`}
                    label="Copy support line"
                    className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
                    onCopied={() => {
                      pushToast({ title: '[settings]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
                      setStatusLine('settings: support line copied');
                    }}
                    onFailed={() =>
                      pushToast({ title: '[settings]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
                    }
                  />
                </div>
              </MinimalSurfaceM814Footer>
            </div>
          </div>
        </motion.div>
      </motion.div>
      )}
    </AnimatePresence>
  );
};

export default SettingsPanel;
