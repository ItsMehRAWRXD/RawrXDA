import React, { useState, useEffect } from 'react';
// @ts-ignore TS1261: workspace opened with mixed root casing (d:/RawrXD vs d:/rawrxd)
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import LocalInferenceStatusStrip from './LocalInferenceStatusStrip';
import {
  CopySupportLineButton,
  MinimalSurfaceM814Footer,
  focusVisibleRing,
  MINIMALISTIC_DOC
} from '../utils/minimalisticM08M14';

const Toolbar = ({
  title = 'RawrXD IDE',
  onOpenProject,
  commandPaletteModuleEnabled = true,
  onCommandPalette,
  onSettings,
  onOpenChat,
  onToggleAgent,
  onOpenModules,
  onOpenSymbols,
  onOpenModels,
  rightDockOpen,
  activeDockTab,
  settings: appSettings
}) => {
  const {
    playUiSound,
    settings: shellSettings,
    setStatusLine,
    noisyLog,
    noisyLogVerbose,
    pushToast,
    setToolbarActiveProviderId
  } = useIdeFeatures();
  const [providers, setProviders] = useState([]);
  const [activeProvider, setActiveProvider] = useState('');
  const [providerListError, setProviderListError] = useState('');

  const noisy = (kind) => {
    if (shellSettings.uiSounds) playUiSound(kind);
  };

  useEffect(() => {
    if (!window.electronAPI?.listAIProviders) {
      setProviderListError('Provider list needs the Electron app (preload exposes listAIProviders).');
      setProviders([]);
      setActiveProvider('');
      setToolbarActiveProviderId('');
      noisyLog('[toolbar]', 'listAIProviders unavailable (browser or old preload)');
      setStatusLine('toolbar: provider list needs Electron');
      return undefined;
    }
    let cancelled = false;
    window.electronAPI
      .listAIProviders()
      .then((list) => {
        if (cancelled) return;
        if (!Array.isArray(list)) {
          const ipcErr =
            list && typeof list === 'object' && list.error != null ? String(list.error) : 'Unexpected response from ai:list-providers.';
          setProviderListError(ipcErr);
          setProviders([]);
          setActiveProvider('');
          setToolbarActiveProviderId('');
          noisyLog('[toolbar]', 'listAIProviders non-array', ipcErr);
          setStatusLine(`toolbar: providers IPC error — ${ipcErr.slice(0, 120)}`);
          pushToast({ title: 'Providers', message: ipcErr, variant: 'error', durationMs: 6000 });
          return;
        }
        setProviders(list);
        setProviderListError('');
        const active = list.find((p) => p.active && p.enabled) || list.find((p) => p.enabled);
        if (active) {
          setActiveProvider(active.id);
          setToolbarActiveProviderId(active.id);
        } else {
          setActiveProvider('');
          setToolbarActiveProviderId('');
        }
        if (list.length === 0) {
          const msg =
            'No providers returned from main process — enable at least one entry in config/providers.json (e.g. bigdaddyg / Ollama).';
          setProviderListError(msg);
          noisyLog('[toolbar]', 'AI provider list empty', msg);
          setStatusLine('toolbar: no AI providers configured');
          pushToast({
            title: 'Providers',
            message: msg,
            variant: 'warn',
            durationMs: 5000
          });
        }
      })
      .catch((err) => {
        if (cancelled) return;
        const msg = err && err.message ? err.message : String(err);
        setProviderListError(msg);
        setProviders([]);
        setActiveProvider('');
        setToolbarActiveProviderId('');
        noisyLog('[toolbar]', 'listAIProviders failed', msg);
        setStatusLine(`toolbar: listAIProviders failed — ${msg.slice(0, 100)}`);
        pushToast({
          title: 'Providers',
          message: `listAIProviders: ${msg}`,
          variant: 'error',
          durationMs: 6000
        });
      });
    return () => {
      cancelled = true;
    };
  }, [noisyLog, setStatusLine, pushToast, setToolbarActiveProviderId]);

  const handleProviderChange = (e) => {
    const id = e.target.value;
    if (!id) return;
    setActiveProvider(id);
    setToolbarActiveProviderId(id);
    if (window.electronAPI?.setActiveAIProvider) {
      window.electronAPI.setActiveAIProvider(id);
    }
    setStatusLine(`toolbar: AI provider → ${id}`);
    noisyLog('[toolbar]', 'setActiveAIProvider', id);
    noisyLogVerbose('inference', 'setActiveAIProvider', id);
  };

  const dockBtn = (active) =>
    `px-2.5 py-1 rounded text-xs font-medium transition ${focusVisibleRing} ${
      active ? 'bg-ide-accent text-white' : 'bg-ide-sidebar text-gray-200 hover:bg-gray-600'
    }`;

  return (
    <div className="flex flex-col bg-ide-toolbar border-b border-gray-700 text-sm">
      <div className="flex items-center justify-between gap-2 px-3 py-2 flex-wrap">
      <div className="flex items-center gap-3 flex-wrap min-w-0">
        <span
          className="font-semibold text-white shrink-0"
          title="RawrXD IDE shell"
        >
          {title}
        </span>
        <button
          type="button"
          onClick={onOpenProject}
          className={`px-3 py-1 rounded bg-ide-accent hover:bg-blue-600 text-white text-xs ${focusVisibleRing}`}
          title="Choose project folder (file access stays under that root)"
        >
          Open project
        </button>
        <div className="h-4 w-px bg-gray-600 hidden sm:block" />
        <button
          type="button"
          disabled={!commandPaletteModuleEnabled}
          onClick={() => {
            if (!commandPaletteModuleEnabled) {
              pushToast({
                title: 'Command palette',
                message: 'Enable the Command palette module in the Modules dock tab.',
                variant: 'info',
                durationMs: 4000
              });
              setStatusLine('Palette disabled — turn on Command palette in Modules');
              return;
            }
            noisy('palette');
            onCommandPalette();
          }}
          className={`${dockBtn(false)} disabled:opacity-40 disabled:cursor-not-allowed disabled:hover:bg-ide-sidebar`}
          title={
            commandPaletteModuleEnabled
              ? 'Registered commands (Ctrl+Shift+P). Does not run shell commands by itself.'
              : 'Command palette module is off — open Modules and enable Command palette.'
          }
        >
          Palette
        </button>
        <button
          type="button"
          onClick={() => {
            noisy('open');
            onSettings();
          }}
          className={dockBtn(false)}
          title="Shell settings (this window)"
        >
          Settings
        </button>
        <button
          type="button"
          onClick={() => {
            noisy('tick');
            onOpenChat();
          }}
          className={dockBtn(rightDockOpen && activeDockTab === 'chat')}
          title="Chat: WASM first; optional delegation to toolbar provider (Settings › AI). Agent: main-process orchestrator."
        >
          Chat
        </button>
        <button
          type="button"
          onClick={() => {
            noisy('tick');
            onToggleAgent();
          }}
          className={dockBtn(rightDockOpen && activeDockTab === 'agent')}
          title="Main-process orchestrator; gates in Settings › Copilot and approval_policy.json."
        >
          Agent
        </button>
        <button
          type="button"
          onClick={() => {
            noisy('tick');
            onOpenModules();
          }}
          className={dockBtn(rightDockOpen && activeDockTab === 'modules')}
          title="Feature toggles (saved locally)"
        >
          Modules
        </button>
        {typeof onOpenSymbols === 'function' ? (
          <button
            type="button"
            onClick={() => {
              noisy('tick');
              onOpenSymbols();
            }}
            className={dockBtn(rightDockOpen && activeDockTab === 'symbols')}
            title="Text symbol index from the editor (Ctrl+Shift+Y) — not LSP"
          >
            Symbols
          </button>
        ) : null}
        <button
          type="button"
          onClick={() => {
            noisy('tick');
            onOpenModels();
          }}
          className={dockBtn(rightDockOpen && activeDockTab === 'models')}
          title="GGUF manifest / metadata in the browser"
        >
          Models
        </button>
      </div>
      <div className="flex items-center gap-2 shrink-0">
        <label htmlFor="rawrxd-toolbar-ai-provider" className="text-xs text-gray-400 hidden md:inline">
          AI
        </label>
        <select
          id="rawrxd-toolbar-ai-provider"
          value={activeProvider && providers.some((p) => p.id === activeProvider) ? activeProvider : ''}
          onChange={handleProviderChange}
          aria-label="Active AI provider for chat and invoke"
          className={`bg-ide-bg border border-gray-600 rounded px-2 py-1 text-xs text-white max-w-[140px] ${focusVisibleRing}`}
          title="Main-process AI provider (config/providers.json)"
        >
          {providers.length === 0 ? (
            <option value="">{providerListError ? 'No providers — see message below' : 'Loading providers…'}</option>
          ) : null}
          {providers.map((p) => (
            <option key={p.id} value={p.id} disabled={!p.enabled}>
              {p.name}
              {p.enabled ? '' : ' (disabled)'}
            </option>
          ))}
        </select>
        {providerListError ? (
          <span
            className="text-[10px] text-amber-400 max-w-[200px] leading-tight hidden lg:inline"
            title={providerListError}
            role="status"
          >
            {providerListError.length > 90 ? `${providerListError.slice(0, 90)}…` : providerListError}
          </span>
        ) : null}
      </div>
      </div>
      {appSettings ? (
        <LocalInferenceStatusStrip activeProviderId={activeProvider} settings={appSettings} />
      ) : null}
      <MinimalSurfaceM814Footer
        surfaceId="toolbar"
        offlineHint="Toolbar works offline; AI provider list loads in Electron."
        docPath={MINIMALISTIC_DOC}
        m13Hint="Dev: noisyLogVerbose('inference', …)"
      >
        <div className="flex flex-wrap gap-1 items-center px-3 pb-1">
          <CopySupportLineButton
            getText={() =>
              `[toolbar] dock_open=${Boolean(rightDockOpen)} dock_tab=${activeDockTab || 'none'} provider=${activeProvider}`
            }
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[toolbar]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('toolbar: support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[toolbar]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
};

export default Toolbar;
