const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs').promises;
const crypto = require('crypto');
const { startEmbeddedServer, stopEmbeddedServer } = require('./src/embedded-api-server');

let mainWindow;

async function createWindow() {
  // Boot the embedded API server BEFORE creating the window
  // so all panel fetch() calls succeed immediately.
  try {
    await startEmbeddedServer();
    console.log('[RawrZ] Embedded API server started on :3000');
  } catch (err) {
    console.error('[RawrZ] Embedded API server error:', err.message);
  }

  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  mainWindow.loadFile('src/index.html');
  // mainWindow.webContents.openDevTools();
}

app.on('ready', createWindow);

app.on('window-all-closed', () => {
  stopEmbeddedServer();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('before-quit', () => {
  stopEmbeddedServer();
});

// IPC handlers
ipcMain.handle('select-file', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile']
  });
  return result.filePaths[0];
});

ipcMain.handle('execute-engine', async (event, engineName, params) => {
  console.log(`Executing: ${engineName}`, params);
  return { success: true, message: `${engineName} executed` };
});

// Stub generation — runs locally, no fetch needed
ipcMain.handle('generate-stub', async (event, payloadPath, options) => {
  const startTime = Date.now();
  const data = await fs.readFile(payloadPath);
  const key = crypto.randomBytes(32);
  const iv = crypto.randomBytes(16);
  const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
  const encrypted = Buffer.concat([cipher.update(data), cipher.final()]);

  const ext = { cpp: '.cpp', csharp: '.cs', python: '.py', powershell: '.ps1', java: '.java', go: '.go', rust: '.rs', javascript: '.js', asm: '.asm', advanced: '.exe' };
  const outName = options.outputPath || `stub_${Date.now()}${ext[options.stubType] || '.bin'}`;
  const outPath = path.join(path.dirname(payloadPath), outName);

  // Write encrypted payload alongside stub
  await fs.writeFile(outPath + '.payload', encrypted);

  return {
    success: true,
    outputPath: outPath,
    payloadSize: data.length,
    encryptedSize: encrypted.length,
    duration: Date.now() - startTime
  };
});

// Security tool handlers — use Node crypto directly (no external deps needed)
ipcMain.handle('encrypt-file', async (event, filePath, algorithm, password) => {
  try {
    const data = await fs.readFile(filePath);
    const salt = crypto.randomBytes(16);
    const key = crypto.scryptSync(password, salt, 32);
    const iv = crypto.randomBytes(16);

    let encrypted;
    if (algorithm === 'aes-256-gcm') {
      const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
      const enc = Buffer.concat([cipher.update(data), cipher.final()]);
      const tag = cipher.getAuthTag();
      encrypted = Buffer.concat([salt, iv, tag, enc]);
    } else {
      const alg = algorithm === 'des' ? 'des-ede3-cbc' : (algorithm === 'triple-des' ? 'des-ede3-cbc' : 'aes-256-cbc');
      const keyLen = alg === 'des-ede3-cbc' ? 24 : 32;
      const ivLen = alg === 'des-ede3-cbc' ? 8 : 16;
      const cipher = crypto.createCipheriv(alg, key.slice(0, keyLen), iv.slice(0, ivLen));
      encrypted = Buffer.concat([salt, iv.slice(0, ivLen), cipher.update(data), cipher.final()]);
    }

    const encryptedPath = filePath + '.enc';
    await fs.writeFile(encryptedPath, encrypted);
    return { success: true, path: encryptedPath };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('decrypt-file', async (event, filePath, algorithm, password) => {
  try {
    const data = await fs.readFile(filePath);
    const salt = data.slice(0, 16);
    const key = crypto.scryptSync(password, salt, 32);

    let decrypted;
    if (algorithm === 'aes-256-gcm') {
      const iv = data.slice(16, 28);
      const tag = data.slice(28, 44);
      const enc = data.slice(44);
      const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
      decipher.setAuthTag(tag);
      decrypted = Buffer.concat([decipher.update(enc), decipher.final()]);
    } else {
      const alg = (algorithm === 'des' || algorithm === 'triple-des') ? 'des-ede3-cbc' : 'aes-256-cbc';
      const ivLen = alg === 'des-ede3-cbc' ? 8 : 16;
      const keyLen = alg === 'des-ede3-cbc' ? 24 : 32;
      const iv = data.slice(16, 16 + ivLen);
      const enc = data.slice(16 + ivLen);
      const decipher = crypto.createDecipheriv(alg, key.slice(0, keyLen), iv);
      decrypted = Buffer.concat([decipher.update(enc), decipher.final()]);
    }

    const decryptedPath = filePath.replace('.enc', '.dec');
    await fs.writeFile(decryptedPath, decrypted);
    return { success: true, path: decryptedPath };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('hash-file', async (event, filePath, algorithm) => {
  try {
    const data = await fs.readFile(filePath);
    const alg = algorithm === 'sha3' ? 'sha3-256' : (algorithm || 'sha256');
    const hash = crypto.createHash(alg).update(data).digest('hex');
    return { success: true, hash };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('generate-password', async () => {
  return crypto.randomBytes(12).toString('base64url');
});

ipcMain.handle('run-security-cli', async () => {
  const { spawn } = require('child_process');
  const cli = spawn('node', [path.join(__dirname, 'src', 'rawrz-standalone.js'), 'help']);
  let output = '';
  cli.stdout.on('data', (d) => { output += d.toString(); });
  cli.stderr.on('data', (d) => { output += d.toString(); });
  return new Promise((resolve) => { cli.on('close', () => resolve(output)); });
});

// Open a panel window
ipcMain.handle('open-panel', async (event, panelName) => {
  const panelPath = path.join(__dirname, 'src', 'panels', panelName);
  const panelWindow = new BrowserWindow({
    width: 1200, height: 800, parent: mainWindow,
    webPreferences: { nodeIntegration: true, contextIsolation: false }
  });
  panelWindow.loadFile(panelPath);
  return { success: true };
});