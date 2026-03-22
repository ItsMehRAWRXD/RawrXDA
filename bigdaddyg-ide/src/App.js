import React, { useMemo, useEffect, useCallback } from 'react';
import Layout from './components/Layout';
import Toolbar from './components/Toolbar';
import Sidebar from './components/Sidebar';
import EditorPanel from './components/EditorPanel';
import RightSidebarDock from './components/RightSidebarDock';
import CommandPalette from './components/CommandPalette';
import SettingsPanel from './components/SettingsPanel';
import Terminal from './components/Terminal';
import NoisyLayer from './components/NoisyLayer';
import { ProjectProvider, useProject } from './contexts/ProjectContext';
import { AgentProvider, useAgent } from './contexts/AgentContext';
import { IdeFeaturesProvider, useIdeFeatures } from './contexts/IdeFeaturesContext';
import useElectron from './hooks/useElectron';
import {
  matchesShortcut,
  shortcutsToElectronAccelerators,
  formatShortcutDisplayFixed
} from './utils/keyboardShortcuts';
import './styles/tailwind.css';

/**
 * IDE shell composition: providers, keyboard shortcuts, status line, and dock/palette toggles.
 * Renders UI only — does not merge `config/providers.json` or enforce `approval_policy.json` in this layer.
 * Terminal uses Electron PTY IPC when available (`electron/preload.js` terminal*); it is not a UI stub when that IPC is present — web-only previews show an honest offline shell message instead.
 */
function AppContent() {
  const { platform } = useElectron();
  const e03SuccessLoggedRef = React.useRef(false);
  /** Persisted: `rawrxd.ide.sidebarOpen` (`'1'` open, `'0'` hidden). Delete the key in devtools storage to reset sidebar default. */
  const [isSidebarOpen, setSidebarOpen] = React.useState(() => {
    try {
      return localStorage.getItem('rawrxd.ide.sidebarOpen') !== '0';
    } catch {
      return true;
    }
  });

  React.useEffect(() => {
    try {
      localStorage.setItem('rawrxd.ide.sidebarOpen', isSidebarOpen ? '1' : '0');
    } catch {
      /* ignore */
    }
  }, [isSidebarOpen]);

  const {
    projectRoot,
    rootListingError,
    fileTree,
    activeFile,
    openProject,
    openFile,
    updateFile,
    loadDirectoryChildren
  } = useProject();

  const {
    activeTaskId,
    taskStatus,
    steps,
    pendingApproval,
    taskGoal,
    taskLog,
    taskSnapshots,
    startAgent,
    refreshStatus,
    refreshTaskList,
    approveAgentStep,
    cancelAgentTask,
    selectAgentTask,
    rollbackCount,
    rollbackAgentMutations
  } = useAgent();

  const ide = useIdeFeatures();
  const {
    settings,
    setSettings,
    modules,
    toggleModule,
    commandPaletteOpen,
    setCommandPaletteOpen,
    settingsOpen,
    setSettingsOpen,
    rightDockTab,
    setRightDockTab,
    openRightDock,
    toggleRightDock,
    playUiSound,
    pushToast,
    setStatusLine,
    celebrate,
    noisyLog,
    shortcuts
  } = ide;

  const startAgentWithSettings = React.useCallback(
    (goal, panelPolicy) => {
      const po = panelPolicy && typeof panelPolicy === 'object' ? panelPolicy : {};
      return startAgent(goal, {
        autoApproveWrites: Boolean(po.autoApproveWrites),
        autoApproveReads: Boolean(po.autoApproveReads),
        requireAskAiApproval: Boolean(po.requireAskAiApproval),
        enableReflection: Boolean(po.enableReflection),
        requirePlanApproval: Boolean(po.requirePlanApproval),
        batchApproveMutations: Boolean(po.batchApproveMutations),
        enableRollbackSnapshots: po.enableRollbackSnapshots !== false,
        autonomousEnabled: settings.agentAutonomousEnabled !== false,
        allowShell: Boolean(settings.agentTerminalEnabled),
        swarmingEnabled: Boolean(settings.agentSwarmingEnabled),
        autoApproveTerminal: Boolean(settings.agentAutoApproveTerminal),
        requireSubagentApproval: settings.agentRequireSubagentApproval !== false,
        agentMaxMode: Boolean(settings.agentMaxMode),
        agentDeepThinking: Boolean(settings.agentDeepThinking),
        agentContextLimitChars: Math.min(
          200000,
          Math.max(2000, Number(settings.agentContextLimitChars) || 48000)
        )
      });
    },
    [startAgent, settings]
  );

  /** E03: default main-process AI lane for agent / HTTP tools (independent of dock chat transport). */
  React.useEffect(() => {
    if (!settings.preferLocalInferenceFirst) return;
    const api = window.electronAPI;
    if (!api?.setActiveAIProvider) return;
    api
      .setActiveAIProvider('bigdaddyg')
      .then((r) => {
        if (r?.success) {
          if (!e03SuccessLoggedRef.current) {
            e03SuccessLoggedRef.current = true;
            noisyLog('[E03] active AI provider = bigdaddyg (local-first)');
          }
        } else {
          setStatusLine(
            '[E03] Set AI provider failed — open Settings, confirm provider list, or check the main process log.'
          );
          noisyLog('[E03] setActiveAIProvider returned unsuccessful');
        }
      })
      .catch((err) => {
        setStatusLine(
          '[E03] Set AI provider failed — retry after the app is fully loaded; check Settings → AI / main log if it persists.'
        );
        noisyLog('[E03] setActiveAIProvider error', err?.message || String(err));
      });
  }, [settings.preferLocalInferenceFirst, noisyLog, setStatusLine]);

  /** Sync dynamic accelerators to Electron application menu */
  React.useEffect(() => {
    const api = window.electronAPI;
    if (!api?.setMenuAccelerators) return;
    const electronMap = shortcutsToElectronAccelerators(shortcuts);
    api.setMenuAccelerators(electronMap).catch((err) => {
      noisyLog('[menu] setMenuAccelerators failed', err?.message || String(err));
      setStatusLine('[menu] Shortcut sync failed — restart the app or check the main process log.');
    });
  }, [shortcuts, noisyLog, setStatusLine]);

  const prevDockRef = React.useRef(null);
  React.useEffect(() => {
    const prev = prevDockRef.current;
    if (rightDockTab !== prev) {
      if (rightDockTab) {
        playUiSound('open');
        setStatusLine(`Panel: ${rightDockTab} — Ctrl+L / A / M / Y / G`);
        noisyLog('[dock] open', rightDockTab);
      } else if (prev) {
        playUiSound('close');
        setStatusLine('Panel closed — reopen from toolbar or palette (Ctrl+Shift+P)');
        noisyLog('[dock] closed', prev);
      }
      prevDockRef.current = rightDockTab;
    }
  }, [rightDockTab, playUiSound, setStatusLine, noisyLog]);

  const paletteWasOpenRef = React.useRef(false);
  React.useEffect(() => {
    if (commandPaletteOpen && !paletteWasOpenRef.current) {
      playUiSound('palette');
      setStatusLine('Command palette — allow-listed commands only; type to filter, ↵ to run (does not execute shell)');
      noisyLog('[palette] opened');
    }
    paletteWasOpenRef.current = commandPaletteOpen;
  }, [commandPaletteOpen, playUiSound, setStatusLine, noisyLog]);

  const settingsWasOpenRef = React.useRef(false);
  React.useEffect(() => {
    if (settingsOpen && !settingsWasOpenRef.current) {
      playUiSound('open');
      setStatusLine(
        'Settings — shell prefs persist in localStorage (IdeFeaturesContext key); Noise tab for MAX volume (opt-in loud UI sounds)'
      );
      pushToast({
        title: 'Settings',
        message: 'Tweak AI, Copilot, and 🔊 Noise. Clear site storage to reset shell prefs if something feels stuck.',
        variant: 'info',
        durationMs: 2600
      });
      noisyLog('[settings] opened');
    }
    settingsWasOpenRef.current = settingsOpen;
  }, [settingsOpen, playUiSound, setStatusLine, pushToast, noisyLog]);

  const handleOpenProject = () => {
    openProject();
  };

  /** Cursor / Copilot–style command registry for the palette */
  const paletteCommands = useMemo(() => {
    const list = [
      {
        id: 'palette',
        label: 'View: Command Palette',
        category: 'View',
        accelerator: formatShortcutDisplayFixed(shortcuts.commandPalette),
        action: () => setCommandPaletteOpen(true)
      },
      {
        id: 'settings',
        label: 'Preferences: Open Settings',
        category: 'View',
        accelerator: 'Ctrl+,',
        action: () => setSettingsOpen(true)
      },
      {
        id: 'chat',
        label: 'Chat: Open AI Chat',
        category: 'Chat',
        accelerator: 'Ctrl+L',
        action: () => openRightDock('chat')
      },
      {
        id: 'agent',
        label: 'Agent: Open Agent Panel',
        category: 'Agent',
        accelerator: 'Ctrl+Shift+A',
        action: () => openRightDock('agent')
      },
      {
        id: 'modules',
        label: 'View: Extension Modules',
        category: 'View',
        accelerator: 'Ctrl+Shift+M',
        action: () => openRightDock('modules')
      },
      {
        id: 'symbols',
        label: 'RE: Symbols & xrefs',
        category: 'View',
        accelerator: 'Ctrl+Shift+Y',
        action: () => openRightDock('symbols')
      },
      {
        id: 'models',
        label: 'Models: Open GGUF Streamer',
        category: 'Models',
        accelerator: 'Ctrl+Shift+G',
        action: () => openRightDock('models')
      },
      {
        id: 'close-dock',
        label: 'View: Close Side Panel',
        category: 'View',
        action: () => setRightDockTab(null)
      },
      {
        id: 'open-project',
        label: 'File: Open Project…',
        category: 'File',
        accelerator: 'Ctrl+O',
        action: () => openProject()
      },
      {
        id: 'sidebar-toggle',
        label: 'View: Toggle project sidebar',
        category: 'View',
        accelerator: 'Ctrl+B',
        action: () => setSidebarOpen((o) => !o)
      }
    ];
    if (modules.commandPalette === false) {
      return list.filter((c) => c.id !== 'palette');
    }
    return list;
  }, [
    modules.commandPalette,
    shortcuts.commandPalette,
    setCommandPaletteOpen,
    setSettingsOpen,
    openRightDock,
    setRightDockTab,
    openProject,
    setSidebarOpen
  ]);

  const onIdeAction = useCallback(
    (action) => {
      switch (action) {
        case 'command-palette':
          if (modules.commandPalette === false) {
            pushToast({
              title: 'Command palette',
              message: 'Enable the Command palette module in the Modules dock tab.',
              variant: 'info',
              durationMs: 3800
            });
            setStatusLine('Palette disabled — turn on Command palette in Modules');
            return;
          }
          setCommandPaletteOpen(true);
          break;
        case 'settings':
          setSettingsOpen(true);
          break;
        case 'chat':
          openRightDock('chat');
          break;
        case 'agent':
          openRightDock('agent');
          break;
        case 'modules':
          openRightDock('modules');
          break;
        case 'models':
          openRightDock('models');
          break;
        case 'symbols':
          openRightDock('symbols');
          break;
        case 'sidebar-toggle':
          setSidebarOpen((o) => !o);
          break;
        default:
          break;
      }
    },
    [setCommandPaletteOpen, setSettingsOpen, openRightDock, setSidebarOpen]
  );

  useEffect(() => {
    const api = window.electronAPI;
    if (!api?.onIdeAction) return undefined;
    return api.onIdeAction(onIdeAction);
  }, [onIdeAction]);

  useEffect(() => {
    const onKey = (e) => {
      if (matchesShortcut(e, shortcuts.commandPalette)) {
        e.preventDefault();
        if (modules.commandPalette !== false) {
          setCommandPaletteOpen(true);
          celebrate();
        }
        return;
      }
      if (matchesShortcut(e, shortcuts.settings)) {
        e.preventDefault();
        setSettingsOpen(true);
        return;
      }
      if (matchesShortcut(e, shortcuts.chat)) {
        e.preventDefault();
        openRightDock('chat');
        return;
      }
      if (matchesShortcut(e, shortcuts.agent)) {
        e.preventDefault();
        toggleRightDock('agent');
        return;
      }
      if (matchesShortcut(e, shortcuts.modules)) {
        e.preventDefault();
        toggleRightDock('modules');
        return;
      }
      if (matchesShortcut(e, shortcuts.models)) {
        e.preventDefault();
        toggleRightDock('models');
        return;
      }
      if (matchesShortcut(e, shortcuts.symbols)) {
        e.preventDefault();
        toggleRightDock('symbols');
        return;
      }
      if (matchesShortcut(e, shortcuts.sidebarToggle)) {
        e.preventDefault();
        setSidebarOpen((o) => !o);
        return;
      }
      if (matchesShortcut(e, shortcuts.openProject)) {
        e.preventDefault();
        openProject();
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [
    modules.commandPalette,
    shortcuts,
    setCommandPaletteOpen,
    setSettingsOpen,
    openProject,
    openRightDock,
    toggleRightDock,
    celebrate,
    setSidebarOpen
  ]);

  const agentDockProps = {
    onStartAgent: startAgentWithSettings,
    taskId: activeTaskId,
    taskStatus,
    steps,
    onRefresh: refreshStatus,
    onRefreshTaskList: refreshTaskList,
    taskSnapshots,
    onSelectTask: selectAgentTask,
    pendingApproval,
    onApproveStep: approveAgentStep,
    onCancelTask: cancelAgentTask,
    taskGoal,
    taskLog,
    autoApproveWrites: settings.agentAutoApprove,
    onAutoApproveWritesChange: (v) => setSettings({ agentAutoApprove: v }),
    autoApproveReads: settings.agentAutoApproveReads,
    onAutoApproveReadsChange: (v) => setSettings({ agentAutoApproveReads: v }),
    requireAskAiApproval: settings.agentRequireAskAiApproval,
    onRequireAskAiApprovalChange: (v) => setSettings({ agentRequireAskAiApproval: v }),
    enableReflection: settings.agentReflectionEnabled,
    onEnableReflectionChange: (v) => setSettings({ agentReflectionEnabled: v }),
    requirePlanApproval: settings.agentRequirePlanApproval,
    onRequirePlanApprovalChange: (v) => setSettings({ agentRequirePlanApproval: v }),
    batchApproveMutations: settings.agentBatchApproveMutations,
    onBatchApproveMutationsChange: (v) => setSettings({ agentBatchApproveMutations: v }),
    enableRollbackSnapshots: settings.agentEnableRollbackSnapshots,
    onEnableRollbackSnapshotsChange: (v) => setSettings({ agentEnableRollbackSnapshots: v }),
    rollbackCount,
    onRollbackMutations: rollbackAgentMutations,
    modules,
    toggleModule
  };

  const main = (
    <div className="flex-1 flex flex-col min-h-0 pb-7">
      <div className="flex-1 flex min-h-0">
        <div
          id="rawrxd-region-main"
          className="flex-1 flex flex-col min-h-0 outline-none"
          tabIndex={-1}
        >
          <EditorPanel activeFile={activeFile} projectRoot={projectRoot} onUpdateFile={updateFile} />
        </div>
        <RightSidebarDock
          tab={rightDockTab}
          onSelectTab={setRightDockTab}
          onClose={() => setRightDockTab(null)}
          activeFile={activeFile}
          projectRoot={projectRoot}
          settings={settings}
          agentProps={agentDockProps}
        />
      </div>
      <Terminal projectRoot={projectRoot} />
    </div>
  );

  const sidebar = (
    <Sidebar
      isOpen={isSidebarOpen}
      onToggle={() => setSidebarOpen(!isSidebarOpen)}
      projectRoot={projectRoot}
      rootListingError={rootListingError}
      fileTree={fileTree}
      activeFile={activeFile}
      onOpenFolder={handleOpenProject}
      onOpenFile={openFile}
      loadDirectoryChildren={loadDirectoryChildren}
    />
  );

  return (
    <div
      className={`h-screen flex flex-col bg-gray-900 text-white overflow-hidden ${
        platform === 'darwin' ? 'pt-8' : ''
      }`}
    >
      <a
        href="#rawrxd-region-main"
        className="fixed left-3 top-3 z-[320] -translate-y-[220%] focus:translate-y-0 transition-transform px-4 py-2 rounded-lg bg-cyan-700 text-white text-sm font-semibold shadow-lg focus:outline focus:outline-2 focus:outline-white"
        onClick={(e) => {
          e.preventDefault();
          document.getElementById('rawrxd-region-main')?.focus({ preventScroll: false });
          document.getElementById('rawrxd-region-main')?.scrollIntoView({ block: 'nearest' });
        }}
      >
        Skip to editor
      </a>
      <a
        href="#rawrxd-dock-region"
        className="fixed left-3 top-16 z-[320] -translate-y-[220%] focus:translate-y-0 transition-transform px-4 py-2 rounded-lg bg-amber-500 text-black text-sm font-semibold shadow-lg focus:outline focus:outline-2 focus:outline-white"
        onClick={(e) => {
          e.preventDefault();
          const el = document.getElementById('rawrxd-dock-region');
          if (el) {
            el.focus({ preventScroll: false });
            el.scrollIntoView({ block: 'nearest' });
          } else {
            toggleRightDock('chat');
            window.requestAnimationFrame(() => {
              document.getElementById('rawrxd-dock-region')?.focus({ preventScroll: false });
            });
          }
        }}
      >
        Skip to AI dock (chat, agent, models)
      </a>
      {platform === 'darwin' && (
        <div className="fixed top-0 left-0 right-0 h-8 bg-transparent z-50 pointer-events-none" />
      )}
      <Toolbar
        title="RawrXD IDE"
        onOpenProject={handleOpenProject}
        commandPaletteModuleEnabled={modules.commandPalette !== false}
        onCommandPalette={() => setCommandPaletteOpen(true)}
        onSettings={() => setSettingsOpen(true)}
        onOpenChat={() => toggleRightDock('chat')}
        onToggleAgent={() => toggleRightDock('agent')}
        onOpenModules={() => toggleRightDock('modules')}
        onOpenSymbols={() => toggleRightDock('symbols')}
        onOpenModels={() => toggleRightDock('models')}
        rightDockOpen={rightDockTab != null}
        activeDockTab={rightDockTab}
        settings={settings}
      />
      <Layout sidebar={sidebar} main={main} />
      <CommandPalette
        open={commandPaletteOpen && modules.commandPalette !== false}
        onClose={() => setCommandPaletteOpen(false)}
        commands={paletteCommands}
      />
      <SettingsPanel
        open={settingsOpen}
        onClose={() => setSettingsOpen(false)}
        settings={settings}
        setSettings={setSettings}
      />
      <NoisyLayer />
    </div>
  );
}

/**
 * Root providers for IDE features, project tree, and agent panel state.
 * Manual verify: app loads → File: Open Project or palette → toggle a dock tab → status line updates.
 */
function App() {
  return (
    <IdeFeaturesProvider>
      <ProjectProvider>
        <AgentProvider>
          <AppContent />
        </AgentProvider>
      </ProjectProvider>
    </IdeFeaturesProvider>
  );
}

export default App;
