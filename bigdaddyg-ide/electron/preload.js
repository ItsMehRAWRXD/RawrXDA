const { contextBridge, ipcRenderer } = require('electron');

/**
 * Preload runs in an isolated world; this IIFE captures `ipcRenderer` once and exposes a small,
 * consistent `electronAPI` (one `invoke` helper; no mutable top-level channel bindings).
 *
 * IPC surface (see `electron/main.js` → `setupIPC()`):
 * ai:*, native:status, agent:*, fs:*, project:open, menu:set-accelerators, ide:action, terminal:*,
 * knowledge:*, ollama:probe, enterprise:export-compliance-bundle
 *
 * Win32 lane note:
 * Keeps the IPC surface minimal for AI, agent, project, and file operations.
 */
const electronAPI = (() => {
  const invoke = (channel, ...args) => ipcRenderer.invoke(channel, ...args);

  return {
    invokeAI: (provider, prompt, context) => invoke('ai:invoke', provider, prompt, context),

    listAIProviders: () => invoke('ai:list-providers'),

    setActiveAIProvider: (providerId) => invoke('ai:set-active', providerId),

    startAgent: (goal, policy) => invoke('agent:start', goal, policy),

    getAgentStatus: (taskId) => invoke('agent:status', taskId),

    approveAgentStep: (taskId, approved) => invoke('agent:approve', taskId, approved),

    cancelAgentTask: (taskId) => invoke('agent:cancel', taskId),

    listAgentTasks: () => invoke('agent:list-tasks'),

    rollbackAgentMutations: (taskId) => invoke('agent:rollback', taskId),

    readFile: (filePath) => invoke('fs:read-file', filePath),

    writeFile: (filePath, content) => invoke('fs:write-file', filePath, content),

    readDir: (dirPath) => invoke('fs:read-dir', dirPath),

    openProject: () => invoke('project:open'),

    /** Subscribe to main/menu `webContents.send('project:opened', root)`. Returns unsubscribe (call on unmount). */
    onProjectOpened: (callback) => {
      if (typeof callback !== 'function') return undefined;
      const fn = (_, path) => callback(path);
      ipcRenderer.on('project:opened', fn);
      return () => ipcRenderer.removeListener('project:opened', fn);
    },

    setMenuAccelerators: (accelerators) => invoke('menu:set-accelerators', accelerators),

    /** Menu / shell: command-palette | settings | chat | agent | modules | symbols | models | sidebar-toggle | save */
    onIdeAction: (callback) => {
      if (typeof callback !== 'function') return undefined;
      const fn = (_, action) => callback(action);
      ipcRenderer.on('ide:action', fn);
      return () => ipcRenderer.removeListener('ide:action', fn);
    },

    platform: process.platform,
    isDev: process.env.NODE_ENV === 'development',

    nativeStatus: () => invoke('native:status'),

    probeOllama: (baseUrl) => invoke('ollama:probe', baseUrl),

    appendKnowledgeArtifact: (rec) => invoke('knowledge:append-artifact', rec),

    rankKnowledgeSignatures: (payload) => invoke('knowledge:rank-signatures', payload),

    recordKnowledgeSignatureOutcome: (signature, success) =>
      invoke('knowledge:record-signature-outcome', signature, success),

    exportComplianceBundle: () => invoke('enterprise:export-compliance-bundle'),

    /** Integrated PTY — implemented in main via `electron/terminal_pty_bridge.js` + node-pty */
    terminalCreate: (opts) => invoke('terminal:create', opts),

    terminalWrite: (id, data) => invoke('terminal:write', id, data),

    terminalResize: (id, cols, rows) => invoke('terminal:resize', id, cols, rows),

    terminalKill: (id) => invoke('terminal:kill', id),

    terminalOpenElevated: (opts) => invoke('terminal:open-elevated', opts),

    onTerminalData: (callback) => {
      if (typeof callback !== 'function') return undefined;
      const fn = (_, sid, data) => callback(sid, data);
      ipcRenderer.on('terminal:data', fn);
      return () => ipcRenderer.removeListener('terminal:data', fn);
    },

    onTerminalExit: (callback) => {
      if (typeof callback !== 'function') return undefined;
      const fn = (_, sid, code) => callback(sid, code);
      ipcRenderer.on('terminal:exit', fn);
      return () => ipcRenderer.removeListener('terminal:exit', fn);
    }
  };
})();

contextBridge.exposeInMainWorld('electronAPI', electronAPI);
