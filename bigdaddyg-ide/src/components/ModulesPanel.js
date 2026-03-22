import React from 'react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  focusVisibleRing,
  MinimalSurfaceM814Footer,
  MINIMALISTIC_DOC,
  CopySupportLineButton
} from '../utils/minimalisticM08M14';

const MODULE_META = [
  {
    key: 'commandPalette',
    label: 'Command palette',
    desc: 'When off: Ctrl+Shift+P, toolbar Palette, and menu “Command Palette” are blocked (enable here to restore).',
    title:
      'M01 — Gates the renderer command palette (shortcut, toolbar, ide:action command-palette). Does not install from network. M06 — Known module id only.'
  },
  {
    key: 'inlineCompletion',
    label: 'Inline completion',
    desc: 'Ghost-text style suggestions (editor integration)',
    title:
      'M01 — Toggles load hook for Monaco inline hints. Toggles load hook; does not install from network. Does not call cloud APIs by itself.'
  },
  {
    key: 'gitIntegration',
    label: 'Git integration',
    desc: 'SCM-aware context for agents',
    title:
      'M01 — Toggles load hook for Git context. Toggles load hook; does not install from network. Does not push or commit automatically.'
  },
  {
    key: 'testRunner',
    label: 'Test runner',
    desc: 'Discover & run tests from palette',
    title:
      'M01 — Toggles load hook for test discovery. Toggles load hook; does not install from network. Does not execute shell without explicit command.'
  },
  {
    key: 'lspBridge',
    label: 'LSP bridge',
    desc: 'Language server protocol (experimental)',
    title:
      'M01 — Toggles load hook for LSP wiring (experimental). Toggles load hook; does not install from network. Does not bundle language servers.'
  },
  {
    key: 'telemetryOptIn',
    label: 'Telemetry',
    desc: 'Optional usage analytics',
    title:
      'M01 — Toggles load hook for opt-in analytics. Toggles load hook; does not install from network. Default off (M06).'
  }
];

const ModulesPanel = ({ modules, toggleModule }) => {
  const { setStatusLine, noisyLog, playUiSound, pushToast, resetModulesToDefaults } = useIdeFeatures();
  const safe = modules && typeof modules === 'object' ? modules : {};

  const onToggle = (key) => {
    toggleModule(key);
    const next = !safe[key];
    setStatusLine(`[modules] ${key}=${next ? 'on' : 'off'}`);
    noisyLog('[modules]', 'toggle', key, next);
    playUiSound('tick');
  };

  const onReset = () => {
    resetModulesToDefaults();
    setStatusLine('[modules] reset to defaults');
    noisyLog('[modules]', 'reset defaults');
    playUiSound('warn');
    pushToast({
      title: 'Modules',
      message: 'Restored default module profile (saved to local storage).',
      variant: 'info',
      durationMs: 3200
    });
  };

  return (
    <div className="flex flex-col h-full bg-ide-sidebar text-white">
      <div className="p-3 border-b border-gray-700 flex items-start justify-between gap-2">
        <div>
          <h3 className="font-semibold text-sm">Modules</h3>
          <p className="text-[10px] text-gray-500 mt-0.5">Extension-like capabilities (persisted)</p>
        </div>
        <button
          type="button"
          onClick={onReset}
          className={`shrink-0 text-[10px] px-2 py-1 rounded border border-gray-600 text-gray-400 hover:text-white hover:border-gray-500 ${focusVisibleRing}`}
          title="M02 — Restore default module flags (git, tests, palette on; LSP off; telemetry off)."
        >
          Reset defaults
        </button>
      </div>
      <ul className="flex-1 min-h-0 overflow-y-auto p-2 space-y-1">
        {MODULE_META.length === 0 ? (
          <li className="text-xs text-gray-500 px-2 py-4">
            M03 — No modules in list — check MODULE_META in source. If a hook fails at runtime, see main process log.
          </li>
        ) : null}
        {MODULE_META.map((m) => (
          <li
            key={m.key}
            className="flex items-start gap-3 rounded-lg px-2 py-2 hover:bg-gray-800/50 border border-transparent hover:border-gray-700"
          >
            <label className="flex items-start gap-3 cursor-pointer flex-1 min-w-0" title={m.title}>
              <input
                type="checkbox"
                className={`mt-1 ${focusVisibleRing}`}
                checked={!!safe[m.key]}
                onChange={() => onToggle(m.key)}
              />
              <span>
                <span className="font-medium text-sm block">{m.label}</span>
                <span className="text-xs text-gray-500">{m.desc}</span>
              </span>
            </label>
          </li>
        ))}
      </ul>
      <MinimalSurfaceM814Footer
        surfaceId="modules"
        offlineHint="Toggles apply offline; agent/LSP hooks need backends when used."
        docPath={MINIMALISTIC_DOC}
        m13Hint="Dev: noisyLog('[modules]', …) on toggle."
      >
        <div className="flex flex-wrap gap-1 items-center px-2 pb-1">
          <CopySupportLineButton
            getText={() => `[modules] keys=${MODULE_META.map((x) => `${x.key}=${safe[x.key] ? 1 : 0}`).join(',')}`}
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[modules]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('[modules] support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[modules]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
};

export default ModulesPanel;
