const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  mainWindow.loadFile('src/index.html');
  mainWindow.webContents.openDevTools();
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

// Basic IPC handlers for dynamic engine system
ipcMain.handle('select-file', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile'],
    filters: [{ name: 'All Files', extensions: ['*'] }]
  });
  return result.filePaths[0];
});

ipcMain.handle('save-file', async () => {
  const result = await dialog.showSaveDialog(mainWindow, {
    filters: [{ name: 'All Files', extensions: ['*'] }]
  });
  return result.filePath;
});

ipcMain.handle('execute-engine', async (event, engineName, params) => {
  console.log(`Executing engine: ${engineName}`, params);
  return { success: true, message: `${engineName} executed with params`, params };
});

ipcMain.handle('generate-stub', async (event, payloadPath, options) => {
  console.log('Generating stub:', payloadPath, options);
  return {
    success: true,
    outputPath: `${payloadPath}_${options.stubType}_stub.${options.stubType === 'advanced' ? 'exe' : options.stubType}`,
    payloadSize: 49152,
    encryptedSize: 49180,
    duration: 27
  };
});