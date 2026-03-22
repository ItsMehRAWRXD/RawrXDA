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
    desc: 'Off: keyboard shortcut, toolbar button, and menu entry for the palette are disabled.',
    title: 'Command palette (Ctrl+Shift+P). No extensions installed from the network.'
  },
  {
    key: 'inlineCompletion',
    label: 'Inline completion',
    desc: 'On: Monaco shows buffer-local inline hints from the open file (with Settings › Copilot inline hints).',
    title: 'Inline hint provider in the editor; local regex/identifier picks, not a cloud Copilot service.'
  },
  {
    key: 'gitIntegration',
    label: 'Git integration',
    desc: 'Reserved hook for future SCM context in agents. Does not run git commands yet.',
    title: 'Future: git-aware agent context. No auto-commit or push.'
  },
  {
    key: 'testRunner',
    label: 'Test runner',
    desc: 'Reserved hook for test discovery from the palette.',
    title: 'Future: palette test commands. Does not run shell without an explicit action.'
  },
  {
    key: 'lspBridge',
    label: 'LSP bridge',
    desc: 'Reserved hook for language-server wiring.',
    title: 'Experimental. No language server is bundled.'
  },
  {
    key: 'telemetryOptIn',
    label: 'Telemetry',
    desc: 'Reserved hook for opt-in analytics. Default off.',
    title: 'Opt-in analytics hook. Off unless you enable this and a collector exists.'
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
          title="Restore defaults: palette on, inline on, git/tests/LSP/telemetry hooks off."
        >
          Reset defaults
        </button>
      </div>
      <ul className="flex-1 min-h-0 overflow-y-auto p-2 space-y-1">
        {MODULE_META.length === 0 ? (
          <li className="text-xs text-gray-500 px-2 py-4">
            No modules defined for this panel.
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
