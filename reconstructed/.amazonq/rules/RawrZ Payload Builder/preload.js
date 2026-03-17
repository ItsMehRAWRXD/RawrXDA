const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('rawrz', {
  selectFile: () => ipcRenderer.invoke('dialog:openFile'),
  selectDirectory: () => ipcRenderer.invoke('dialog:openDirectory'),

  hashFile: (filePath) => ipcRenderer.invoke('hash:file', filePath),
  compressFile: (inputPath, outputDir) => ipcRenderer.invoke('compress:file', { inputPath, outputDir }),
  decompressFile: (inputPath, outputDir) => ipcRenderer.invoke('decompress:file', { inputPath, outputDir }),

  encryptTextDemo: (text, keyHex) => ipcRenderer.invoke('encrypt:text', { text, hexKey: keyHex }),
  decryptTextDemo: (cipherTextHex, keyHex) => ipcRenderer.invoke('decrypt:text', { cipherTextHex, hexKey: keyHex }),

  encryptFile: (inputPath, outputDir, outputName, keyHex) => ipcRenderer.invoke('encrypt:file', { inputPath, outputDir, outputName, hexKey: keyHex }),
  decryptFile: (inputPath, outputDir, outputName, keyHex) => ipcRenderer.invoke('decrypt:file', { inputPath, outputDir, outputName, hexKey: keyHex }),

  parseJotti: (resultText) => ipcRenderer.invoke('parse:jotti', { resultText }),
  
  executeEngine: (engineName, action) => ipcRenderer.invoke('execute:engine', { engineName, action })
});
