import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('rawrz', {
  selectFile: () => ipcRenderer.invoke('dialog:openFile'),
  selectDirectory: () => ipcRenderer.invoke('dialog:openDirectory'),

  hashFile: (filePath) => ipcRenderer.invoke('hash:file', filePath),
  compressFile: (inputPath, outputDir) => ipcRenderer.invoke('compress:file', { inputPath, outputDir }),
  decompressFile: (inputPath, outputDir) => ipcRenderer.invoke('decompress:file', { inputPath, outputDir }),

  encryptTextDemo: (text, keyHex, method) => ipcRenderer.invoke('encrypt:text', { text, hexKey: keyHex, method }),
  decryptTextDemo: (cipherTextHex, keyHex, method) => ipcRenderer.invoke('decrypt:text', { cipherTextHex, hexKey: keyHex, method }),

  encryptFile: (inputPath, outputDir, outputName, keyHex, method, extension) => ipcRenderer.invoke('encrypt:file', { inputPath, outputDir, outputName, hexKey: keyHex, method, extension }),
  decryptFile: (inputPath, outputDir, outputName, keyHex) => ipcRenderer.invoke('decrypt:file', { inputPath, outputDir, outputName, hexKey: keyHex }),

  parseJotti: (resultText) => ipcRenderer.invoke('parse:jotti', { resultText }),
  generateStub: (payloadPath, options) => ipcRenderer.invoke('generate-stub', payloadPath, options),
  
  executeEngine: (engineName, params) => ipcRenderer.invoke('execute-engine', engineName, params),
  
  // Enhanced engine system
  getEngineConfig: (engineId) => ipcRenderer.invoke('get-engine-config', engineId),
  generateEngineMenu: (engineId) => ipcRenderer.invoke('generate-engine-menu', engineId),
  
  // Legacy support for the current UI
  selectFiles: () => ipcRenderer.invoke('select-files'),
  encryptText: (text, password) => ipcRenderer.invoke('encrypt:text', { text, hexKey: password }),
  decryptText: (encryptedData, password) => ipcRenderer.invoke('decrypt:text', { cipherTextHex: encryptedData, hexKey: password }),
  protectBot: (filePath, password) => ipcRenderer.invoke('protect-bot', filePath, password),
  obfuscateBot: (scriptPath) => ipcRenderer.invoke('obfuscate-bot', scriptPath),
  getEngines: () => ipcRenderer.invoke('get-engines'),
  fudEncrypt: (filePath, outputPath) => ipcRenderer.invoke('fud-encrypt', filePath, outputPath),
  getStubStatus: () => ipcRenderer.invoke('get-stub-status'),
  burnStub: (stubId) => ipcRenderer.invoke('burn-stub', stubId)
});

// Also expose under electronAPI for backwards compatibility
contextBridge.exposeInMainWorld('electronAPI', {
  hashFile: (filePath) => ipcRenderer.invoke('hash:file', filePath),
  compressFile: (filePath) => ipcRenderer.invoke('compress:file', { inputPath: filePath }),
  decompressFile: (filePath) => ipcRenderer.invoke('decompress:file', { inputPath: filePath }),
  encryptText: (text, password) => ipcRenderer.invoke('encrypt:text', { text, hexKey: password }),
  decryptText: (encryptedData, password) => ipcRenderer.invoke('decrypt:text', { cipherTextHex: encryptedData, hexKey: password }),
  selectFile: () => ipcRenderer.invoke('select-files'),
  selectFiles: () => ipcRenderer.invoke('select-files'),
  selectDirectory: () => ipcRenderer.invoke('select-directory'),
  protectBot: (filePath, password) => ipcRenderer.invoke('protect-bot', filePath, password),
  obfuscateBot: (scriptPath) => ipcRenderer.invoke('obfuscate-bot', scriptPath),
  getEngines: () => ipcRenderer.invoke('get-engines'),
  executeEngine: (engineName, params) => ipcRenderer.invoke('execute-engine', engineName, params),
  generateStub: (payloadPath, options) => ipcRenderer.invoke('generate-stub', payloadPath, options),
  fudEncrypt: (filePath, outputPath) => ipcRenderer.invoke('fud-encrypt', filePath, outputPath),
  getStubStatus: () => ipcRenderer.invoke('get-stub-status'),
  burnStub: (stubId) => ipcRenderer.invoke('burn-stub', stubId)
});