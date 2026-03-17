/**
 * BigDaddyG IDE - Electron Preload Script
 * Secure bridge between main process and renderer
 */

const { contextBridge, ipcRenderer } = require('electron');

// Input validation helpers
const validateString = (input, maxLength = 1000) => {
  if (typeof input !== 'string') return '';
  return input.substring(0, maxLength);
};

const validatePath = (path) => {
  if (typeof path !== 'string') return '';
  // Basic path validation - remove dangerous characters
  return path.replace(/[<>"|?*]/g, '').substring(0, 500);
};

const validateOptions = (options) => {
  if (!options || typeof options !== 'object') return {};
  return options;
};

// Expose protected methods to renderer process
contextBridge.exposeInMainWorld('electron', {
  // Window controls
  minimizeWindow: () => ipcRenderer.send('window-minimize'),
  maximizeWindow: () => ipcRenderer.send('window-maximize'),
  closeWindow: () => ipcRenderer.send('window-close'),
  
  // File system operations
  readFile: (filePath) => ipcRenderer.invoke('read-file', validatePath(filePath)),
  writeFile: (filePath, content) => ipcRenderer.invoke('write-file', validatePath(filePath), validateString(content, 1000000)),
  openFileDialog: (options) => ipcRenderer.invoke('open-file-dialog', validateOptions(options)),
  saveFileDialog: (options) => ipcRenderer.invoke('save-file-dialog', validateOptions(options)),
  openFolderDialog: () => ipcRenderer.invoke('open-folder-dialog'),
  readDir: (dirPath) => ipcRenderer.invoke('read-dir', validatePath(dirPath)),
  getFileStats: (filePath) => ipcRenderer.invoke('get-file-stats', validatePath(filePath)),
  fileExists: (filePath) => ipcRenderer.invoke('file-exists', validatePath(filePath)),
  getPluginDir: () => ipcRenderer.invoke('plugin:get-directory'),
  
  // Agentic file system operations (unlimited)
  scanWorkspace: (options) => ipcRenderer.invoke('scanWorkspace', options),
  searchFiles: (query, options) => ipcRenderer.invoke('search-files', validateString(query, 200), validateOptions(options)),
  readDirRecursive: (dirPath, maxDepth) => ipcRenderer.invoke('read-dir-recursive', validatePath(dirPath), Math.min(maxDepth || 10, 20)),
  readFileChunked: (filePath, chunkSize) => ipcRenderer.invoke('read-file-chunked', validatePath(filePath), Math.min(chunkSize || 1024, 10485760)),
  readMultipleFiles: (filePaths) => ipcRenderer.invoke('read-multiple-files', filePaths),
  findByPattern: (pattern, startPath) => ipcRenderer.invoke('find-by-pattern', pattern, startPath),
  
  // App info
  getAppVersion: () => ipcRenderer.invoke('get-app-version'),
  getAppPath: () => ipcRenderer.invoke('get-app-path'),
  
  // Orchestra server control
  startOrchestra: () => ipcRenderer.invoke('orchestra:start'),
  stopOrchestra: () => ipcRenderer.invoke('orchestra:stop'),
  getOrchestraStatus: () => ipcRenderer.invoke('orchestra:status'),
  
  // Native speech recognition (offline, cross-platform)
  windowsSpeechRecognize: () => ipcRenderer.invoke('windows-speech-recognize'),
  macSpeechRecognize: () => ipcRenderer.invoke('mac-speech-recognize'),
  linuxSpeechRecognize: () => ipcRenderer.invoke('linux-speech-recognize'),
  
  // Drive & file system browsing
  listDrives: () => ipcRenderer.invoke('list-drives'),
  onDrivesChanged: (callback) => ipcRenderer.on('drives-changed', (_, drives) => callback(drives)),
  
  // Drive polling control (optimization)
  drivePolling: {
    start: () => ipcRenderer.invoke('drive-polling:start'),
    stop: () => ipcRenderer.invoke('drive-polling:stop'),
    status: () => ipcRenderer.invoke('drive-polling:status')
  },
  
  // Advanced file operations
  createDirectory: (dirPath) => ipcRenderer.invoke('createDirectory', dirPath),
  deleteItem: (itemPath, isDirectory) => ipcRenderer.invoke('deleteItem', itemPath, isDirectory),
  copyItem: (sourcePath, destPath) => ipcRenderer.invoke('copyItem', sourcePath, destPath),
  moveItem: (sourcePath, destPath) => ipcRenderer.invoke('moveItem', sourcePath, destPath),
  getStats: (itemPath) => ipcRenderer.invoke('getStats', itemPath),

  marketplace: {
    status: () => ipcRenderer.invoke('marketplace:status'),
    featured: () => ipcRenderer.invoke('marketplace:featured'),
    search: (options) => ipcRenderer.invoke('marketplace:search', options),
    getExtension: (extensionId) => ipcRenderer.invoke('marketplace:get-extension', extensionId),
    listInstalled: () => ipcRenderer.invoke('marketplace:list-installed'),
    listWithState: () => ipcRenderer.invoke('marketplace:list-with-state'),
    install: (extensionId) => ipcRenderer.invoke('marketplace:install', extensionId),
    uninstall: (extensionId) => ipcRenderer.invoke('marketplace:uninstall', extensionId),
    enable: (extensionId) => ipcRenderer.invoke('marketplace:enable', extensionId),
    disable: (extensionId) => ipcRenderer.invoke('marketplace:disable', extensionId),
    checkUpdates: () => ipcRenderer.invoke('marketplace:check-updates'),
    update: (extensionId) => ipcRenderer.invoke('marketplace:update', extensionId),
    updateAll: () => ipcRenderer.invoke('marketplace:update-all'),
    onEvent: (callback) => {
      if (typeof callback !== 'function') {
        return () => {};
      }
      const listener = (_, event) => callback(event);
      ipcRenderer.on('marketplace:event', listener);
      return () => ipcRenderer.removeListener('marketplace:event', listener);
    }
  },

  apiKeys: {
    list: () => ipcRenderer.invoke('apikeys:list'),
    set: (provider, key, metadata) => ipcRenderer.invoke('apikeys:set', { provider, key, metadata }),
    delete: (provider) => ipcRenderer.invoke('apikeys:delete', provider)
  },

  ollama: {
    listModels: () => ipcRenderer.invoke('ollama:list-models'),
    pullModel: (model) => ipcRenderer.invoke('ollama:pull-model', model),
    deleteModel: (model) => ipcRenderer.invoke('ollama:delete-model', model),
    showModel: (model) => ipcRenderer.invoke('ollama:show-model', model),
    status: () => ipcRenderer.invoke('ollama:status')
  },

  models: {
    discover: () => ipcRenderer.invoke('models:discover')
  },
  
  settings: {
    getAll: () => ipcRenderer.invoke('settings:get-all'),
    getDefaults: () => ipcRenderer.invoke('settings:get-defaults'),
    get: (pathString) => ipcRenderer.invoke('settings:get', pathString),
    set: (pathString, value, options) => ipcRenderer.invoke('settings:set', pathString, value, options),
    update: (patch, options) => ipcRenderer.invoke('settings:update', patch, options),
    reset: (section, options) => ipcRenderer.invoke('settings:reset', section, options),
    getHotkeys: () => ipcRenderer.invoke('settings:hotkeys:get'),
    setHotkey: (action, combo, options) => ipcRenderer.invoke('settings:hotkeys:set', action, combo, options),
    onDidChange: (callback) => {
      if (typeof callback !== 'function') return () => {};
      const listener = (_, payload) => callback(payload);
      ipcRenderer.on('settings:updated', listener);
      return () => ipcRenderer.removeListener('settings:updated', listener);
    },
    onBootstrap: (callback) => {
      if (typeof callback !== 'function') return;
      ipcRenderer.once('settings:bootstrap', (_, payload) => callback(payload));
    }
  },
  
  // Memory bridge
  memory: {
    getStats: () => ipcRenderer.invoke('memory:getStats'),
    store: (content, metadata = {}) => ipcRenderer.invoke('memory:store', { content, metadata }),
    query: (query, limit = 10) => ipcRenderer.invoke('memory:query', { query, limit }),
    recent: (limit = 20) => ipcRenderer.invoke('memory:recent', limit),
    embedding: (text, model = 'local') => ipcRenderer.invoke('memory:embedding', { text, model }),
    similar: (embedding, threshold = 0.7, limit = 10) => ipcRenderer.invoke('memory:similar', { embedding, threshold, limit }),
    decay: () => ipcRenderer.invoke('memory:decay'),
    clear: () => ipcRenderer.invoke('memory:clear')
  },

  // System integration
  launchProgram: (programPath) => ipcRenderer.invoke('launchProgram', programPath),
  openInExplorer: (itemPath) => ipcRenderer.invoke('openInExplorer', itemPath),
  openUrl: (url) => ipcRenderer.invoke('openUrl', url),
  getSystemInfo: () => ipcRenderer.invoke('getSystemInfo'),
  
  // Terminal execution
  executeCommand: (command, shell, cwd) => ipcRenderer.invoke('execute-command', { 
    command: validateString(command, 500), 
    shell: validateString(shell, 50), 
    cwd: validatePath(cwd) 
  }),
  onTerminalOutput: (callback) => ipcRenderer.on('terminal-output', (_, data) => callback(data)),
  onTerminalExit: (callback) => ipcRenderer.on('terminal-exit', (_, data) => callback(data)),
  
  // Browser operations
  browser: {
    navigate: (url) => ipcRenderer.invoke('browser:navigate', url),
    back: () => ipcRenderer.invoke('browser:back'),
    forward: () => ipcRenderer.invoke('browser:forward'),
    reload: () => ipcRenderer.invoke('browser:reload'),
    stop: () => ipcRenderer.invoke('browser:stop'),
    show: () => ipcRenderer.invoke('browser:show'),
    hide: () => ipcRenderer.invoke('browser:hide'),
    openYouTube: () => ipcRenderer.invoke('browser:open-youtube'),
    searchYouTube: (query) => ipcRenderer.invoke('browser:search-youtube', query),
    playYouTube: () => ipcRenderer.invoke('browser:play-youtube'),
    togglePlayback: () => ipcRenderer.invoke('browser:toggle-playback'),
    enterPictureInPicture: () => ipcRenderer.invoke('browser:enter-pip'),
    getMediaState: () => ipcRenderer.invoke('browser:get-media-state'),
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
    onUISuggestions: (callback) => ipcRenderer.on('browser:ui-suggestions', (_, data) => callback(data)),
    onMediaState: (callback) => ipcRenderer.on('browser:media-state', (_, data) => callback(data))
  },
  
  // Native Ollama Node.js HTTP client
  nativeOllamaNode: {
    generate: (model, prompt) => ipcRenderer.invoke('native-ollama-node:generate', model, prompt),
    getStats: () => ipcRenderer.invoke('native-ollama-node:stats')
  },
  
  // Error logging to file
  getLogFilePath: () => ipcRenderer.invoke('get-log-file-path'),
  writeLogFile: (filePath, content) => ipcRenderer.invoke('write-log-file', filePath, content),
  
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

// ============================================================================
// EXPOSE PROCESS ENVIRONMENT (FIX: process is not defined)
// ============================================================================
const os = require('os');
const path = require('path');

contextBridge.exposeInMainWorld('env', {
  platform: os.platform(),
  arch: os.arch(),
  cwd: () => process.cwd(),
  env: {
    ...process.env,
    // Sanitize sensitive vars
    PATH: process.env.PATH,
    HOME: process.env.HOME || process.env.USERPROFILE,
    NODE_ENV: process.env.NODE_ENV || 'production'
  },
  versions: {
    node: process.versions.node,
    chrome: process.versions.chrome,
    electron: process.versions.electron
  },
  sep: path.sep,
  delimiter: path.delimiter
});

console.log('[BigDaddyG] ✅ Preload script loaded');

