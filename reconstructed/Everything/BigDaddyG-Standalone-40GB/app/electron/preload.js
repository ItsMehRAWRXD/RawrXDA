/**
 * BigDaddyG IDE - Electron Preload Script
 * Secure bridge between main process and renderer
 */

const { contextBridge, ipcRenderer } = require('electron');

// Expose protected methods to renderer process
contextBridge.exposeInMainWorld('electron', {
  // File system operations
  readFile: (filePath) => ipcRenderer.invoke('read-file', filePath),
  writeFile: (filePath, content) => ipcRenderer.invoke('write-file', filePath, content),

  // App info
  getAppVersion: () => ipcRenderer.invoke('get-app-version'),
  getAppPath: () => ipcRenderer.invoke('get-app-path'),

  // Orchestra server control
  startOrchestra: () => ipcRenderer.invoke('orchestra:start'),
  stopOrchestra: () => ipcRenderer.invoke('orchestra:stop'),
  getOrchestraStatus: () => ipcRenderer.invoke('orchestra:status'),

  // Micro-Model-Server & Beaconism Integration
  microModel: {
    connect: (useBeaconism = true) => ipcRenderer.invoke('micro-model:connect', useBeaconism),
    disconnect: () => ipcRenderer.invoke('micro-model:disconnect'),
    sendCommand: (command, data) => ipcRenderer.invoke('micro-model:send', command, data),

    // Micro chain operations
    executeMicroChain: (prompt, models) => ipcRenderer.invoke('micro-model:chain', prompt, models),
    executeNanoChain: (prompt) => ipcRenderer.invoke('micro-model:nano', prompt),
    getCatalog: () => ipcRenderer.invoke('micro-model:catalog'),
    runTest: () => ipcRenderer.invoke('micro-model:test'),
    deploy: (models) => ipcRenderer.invoke('micro-model:deploy', models),

    // Beaconism operations
    enableBeaconism: () => ipcRenderer.invoke('micro-model:beacon-enable'),
    disableBeaconism: () => ipcRenderer.invoke('micro-model:beacon-disable'),
    syncWithBeacon: (syncCommand) => ipcRenderer.invoke('micro-model:beacon-sync', syncCommand),
    getBeaconStatus: () => ipcRenderer.invoke('micro-model:beacon-status'),

    // Server status
    getStatus: () => ipcRenderer.invoke('micro-model:status'),
    getHealth: () => ipcRenderer.invoke('micro-model:health'),

    // Events
    onMessage: (callback) => ipcRenderer.on('micro-model:message', (_, data) => callback(data)),
    onError: (callback) => ipcRenderer.on('micro-model:error', (_, data) => callback(data)),
    onBeaconMessage: (callback) => ipcRenderer.on('micro-model:beacon', (_, data) => callback(data)),
    onConnectionChange: (callback) => ipcRenderer.on('micro-model:connection', (_, data) => callback(data))
  },

  // Browser operations
  browser: {
    navigate: (url) => ipcRenderer.invoke('browser:navigate', url),
    back: () => ipcRenderer.invoke('browser:back'),
    forward: () => ipcRenderer.invoke('browser:forward'),
    reload: () => ipcRenderer.invoke('browser:reload'),
    stop: () => ipcRenderer.invoke('browser:stop'),
    show: () => ipcRenderer.invoke('browser:show'),
    hide: () => ipcRenderer.invoke('browser:hide'),
    screenshot: (options) => ipcRenderer.invoke('browser:screenshot', options),
    analyze: () => ipcRenderer.invoke('browser:analyze'),
    suggestUI: () => ipcRenderer.invoke('browser:suggest-ui'),
    devTools: () => ipcRenderer.invoke('browser:devtools'),
    getConsoleLogs: () => ipcRenderer.invoke('browser:get-console-logs'),
    getNetworkLogs: () => ipcRenderer.invoke('browser:get-network-logs'),
    getScreenshots: () => ipcRenderer.invoke('browser:get-screenshots'),
    clearLogs: () => ipcRenderer.invoke('browser:clear-logs'),

    // Browser events
    onLoading: (callback) => ipcRenderer.on('browser:loading', (_, data) => callback(data)),
    onLoaded: (callback) => ipcRenderer.on('browser:loaded', (_, data) => callback(data)),
    onError: (callback) => ipcRenderer.on('browser:error', (_, data) => callback(data)),
    onTitle: (callback) => ipcRenderer.on('browser:title', (_, data) => callback(data)),
    onConsole: (callback) => ipcRenderer.on('browser:console', (_, data) => callback(data)),
    onNetwork: (callback) => ipcRenderer.on('browser:network', (_, data) => callback(data)),
    onScreenshot: (callback) => ipcRenderer.on('browser:screenshot', (_, data) => callback(data)),
    onIssues: (callback) => ipcRenderer.on('browser:issues', (_, data) => callback(data)),
    onUISuggestions: (callback) => ipcRenderer.on('browser:ui-suggestions', (_, data) => callback(data))
  },

  // Menu events (receive only)
  onMenuEvent: (callback) => {
    ipcRenderer.on('menu-new-file', () => callback('new-file'));
    ipcRenderer.on('menu-open-file', () => callback('open-file'));
    ipcRenderer.on('menu-save-file', () => callback('save-file'));
    ipcRenderer.on('menu-save-as', () => callback('save-as'));
    ipcRenderer.on('menu-toggle-sidebar', () => callback('toggle-sidebar'));
    ipcRenderer.on('menu-toggle-terminal', () => callback('toggle-terminal'));
    ipcRenderer.on('menu-ask-ai', () => callback('ask-ai'));
    ipcRenderer.on('menu-ai-explain', () => callback('ai-explain'));
    ipcRenderer.on('menu-ai-fix', () => callback('ai-fix'));
    ipcRenderer.on('menu-ai-optimize', () => callback('ai-optimize'));
    ipcRenderer.on('menu-ai-tune', () => callback('ai-tune'));
    ipcRenderer.on('menu-about', () => callback('about'));

    // Browser menu events
    ipcRenderer.on('menu-show-browser', () => callback('show-browser'));
    ipcRenderer.on('menu-browser-navigate', () => callback('browser-navigate'));
    ipcRenderer.on('menu-browser-back', () => callback('browser-back'));
    ipcRenderer.on('menu-browser-forward', () => callback('browser-forward'));
    ipcRenderer.on('menu-browser-reload', () => callback('browser-reload'));
    ipcRenderer.on('menu-browser-screenshot', () => callback('browser-screenshot'));
    ipcRenderer.on('menu-browser-analyze', () => callback('browser-analyze'));
    ipcRenderer.on('menu-browser-suggest-ui', () => callback('browser-suggest-ui'));
    ipcRenderer.on('menu-browser-devtools', () => callback('browser-devtools'));
  }
});

console.log('[BigDaddyG] ✅ Preload script loaded');

