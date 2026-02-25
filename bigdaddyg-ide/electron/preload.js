const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  invokeAI: (provider, prompt, context) =>
    ipcRenderer.invoke('ai:invoke', provider, prompt, context),

  listAIProviders: () =>
    ipcRenderer.invoke('ai:list-providers'),

  setActiveAIProvider: (providerId) =>
    ipcRenderer.invoke('ai:set-active', providerId),

  startAgent: (goal) =>
    ipcRenderer.invoke('agent:start', goal),

  getAgentStatus: (taskId) =>
    ipcRenderer.invoke('agent:status', taskId),

  readFile: (filePath) =>
    ipcRenderer.invoke('fs:read-file', filePath),

  writeFile: (filePath, content) =>
    ipcRenderer.invoke('fs:write-file', filePath, content),

  readDir: (dirPath) =>
    ipcRenderer.invoke('fs:read-dir', dirPath),

  openProject: () =>
    ipcRenderer.invoke('project:open'),

  onProjectOpened: (callback) => {
    if (typeof callback === 'function') {
      ipcRenderer.on('project:opened', (_, path) => callback(path));
    }
  },

  platform: process.platform,
  isDev: process.env.NODE_ENV === 'development'
});
