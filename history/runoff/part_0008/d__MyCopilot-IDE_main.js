const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      enableRemoteModule: true
    }
  });

  mainWindow.loadFile('index.html');
  mainWindow.on('closed', () => mainWindow = null);
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});

// IPC handlers for PowerShell integration
ipcMain.handle('process-request', async (event, request) => {
  return new Promise((resolve, reject) => {
    const ps = spawn('powershell.exe', [
      '-ExecutionPolicy', 'Bypass',
      '-File', path.join(__dirname, 'Setup-UnifiedAgent.ps1'),
      '-Request', request
    ]);

    let output = '';
    ps.stdout.on('data', (data) => output += data.toString());
    ps.stderr.on('data', (data) => output += data.toString());
    ps.on('close', (code) => {
      if (code === 0) {
        resolve(output);
      } else {
        reject(output);
      }
    });
  });
});