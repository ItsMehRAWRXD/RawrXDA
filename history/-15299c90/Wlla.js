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
  generateStub: (payloadPath, options) => ipcRenderer.invoke('generate-stub', payloadPath, options)
});