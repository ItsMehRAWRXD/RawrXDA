const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs').promises;
const isDev = require('electron-is-dev');

const AIProviderManager = require(path.join(__dirname, '..', 'src', 'agentic', 'providers'));
const AgentOrchestrator = require(path.join(__dirname, '..', 'src', 'agentic', 'orchestrator'));

let mainWindow = null;
let projectRoot = null;
const aiManager = new AIProviderManager();
const agentRuntime = {
  getProjectRoot: () => projectRoot,
  readFile: async (relPath) => {
    if (!projectRoot) {
      throw new Error('No project opened');
    }
    const resolved = path.resolve(projectRoot, relPath);
    if (!isPathAllowed(resolved)) {
      throw new Error('Access denied: path not under project root');
    }
    return fs.readFile(resolved, 'utf8');
  },
  writeFile: async (relPath, content) => {
    if (!projectRoot) {
      throw new Error('No project opened');
    }
    const resolved = path.resolve(projectRoot, relPath);
    if (!isPathAllowed(resolved)) {
      throw new Error('Access denied: path not under project root');
    }
    await fs.mkdir(path.dirname(resolved), { recursive: true });
    await fs.writeFile(resolved, content, 'utf8');
  },
  appendFile: async (relPath, content) => {
    if (!projectRoot) {
      throw new Error('No project opened');
    }
    const resolved = path.resolve(projectRoot, relPath);
    if (!isPathAllowed(resolved)) {
      throw new Error('Access denied: path not under project root');
    }
    await fs.mkdir(path.dirname(resolved), { recursive: true });
    await fs.appendFile(resolved, content, 'utf8');
  },
  readDir: async (relPath) => {
    if (!projectRoot) {
      throw new Error('No project opened');
    }
    const resolved = path.resolve(projectRoot, relPath);
    if (!isPathAllowed(resolved)) {
      throw new Error('Access denied: path not under project root');
    }
    const entries = await fs.readdir(resolved, { withFileTypes: true });
    return entries.map((e) => ({ name: e.name, isDirectory: e.isDirectory() }));
  }
};
const agentOrchestrator = new AgentOrchestrator(aiManager, agentRuntime);

function isPathAllowed(filePath) {
  if (!projectRoot) return false;
  const resolved = path.resolve(filePath);
  const rootResolved = path.resolve(projectRoot);
  return resolved === rootResolved || resolved.startsWith(rootResolved + path.sep);
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1600,
    height: 900,
    minWidth: 1200,
    minHeight: 800,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      preload: path.join(__dirname, 'preload.js'),
      webSecurity: !isDev
    },
    titleBarStyle: process.platform === 'darwin' ? 'hiddenInset' : 'default',
    show: false
  });

  const startUrl = isDev
    ? 'http://localhost:3000'
    : `file://${path.join(__dirname, '../build/index.html')}`;

  mainWindow.loadURL(startUrl);

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
    if (isDev) {
      mainWindow.webContents.openDevTools();
    }
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

function setupIPC() {
  ipcMain.handle('ai:invoke', async (_, provider, prompt, context) => {
    try {
      const result = await aiManager.invoke(provider, prompt, context || {});
      return { success: true, data: result };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });

  ipcMain.handle('ai:list-providers', async () => {
    return aiManager.getAvailableProviders();
  });

  ipcMain.handle('ai:set-active', async (_, providerId) => {
    const ok = aiManager.setActiveProvider(providerId);
    return ok ? { success: true } : { success: false, error: 'Provider is unavailable or disabled' };
  });

  ipcMain.handle('agent:start', async (_, goal) => {
    try {
      const taskId = await agentOrchestrator.startTask(goal);
      return { success: true, data: taskId };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });

  ipcMain.handle('agent:status', async (_, taskId) => {
    const status = agentOrchestrator.getTaskStatus(taskId);
    return { success: true, data: status };
  });

  ipcMain.handle('fs:read-file', async (_, filePath) => {
    try {
      if (!isPathAllowed(filePath)) {
        return { success: false, error: 'Access denied: path not under project root' };
      }
      const content = await fs.readFile(filePath, 'utf8');
      return { success: true, data: content };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });

  ipcMain.handle('fs:write-file', async (_, filePath, content) => {
    try {
      if (!isPathAllowed(filePath)) {
        return { success: false, error: 'Access denied: path not under project root' };
      }
      await fs.writeFile(filePath, content, 'utf8');
      return { success: true };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });

  ipcMain.handle('fs:read-dir', async (_, dirPath) => {
    try {
      if (!isPathAllowed(dirPath)) {
        return { success: false, error: 'Access denied: path not under project root' };
      }
      const entries = await fs.readdir(dirPath, { withFileTypes: true });
      const result = entries.map((e) => ({
        name: e.name,
        isDirectory: e.isDirectory(),
        path: path.join(dirPath, e.name)
      }));
      return { success: true, data: result };
    } catch (error) {
      return { success: false, error: error.message };
    }
  });

  ipcMain.handle('project:open', async () => {
    if (!mainWindow) return { success: false, error: 'No window' };
    const result = await dialog.showOpenDialog(mainWindow, {
      properties: ['openDirectory'],
      title: 'Open Project Folder'
    });
    if (result.canceled || !result.filePaths.length) {
      return { success: true, data: null };
    }
    projectRoot = result.filePaths[0];
    return { success: true, data: projectRoot };
  });
}

function setupAutoUpdater() {
  if (!app.isPackaged) return;
  try {
    const { autoUpdater } = require('electron-updater');
    autoUpdater.checkForUpdatesAndNotify();
  } catch (err) {
    console.error('Auto-updater not available:', err);
  }
}

function setupMenu() {
  try {
    const menuModule = require(path.join(__dirname, 'menu.js'));
    if (typeof menuModule.setApplicationMenu === 'function') {
      menuModule.setApplicationMenu(mainWindow, (root) => {
        projectRoot = root;
      });
    }
  } catch {
    // menu optional
  }
}

app.whenReady().then(() => {
  setupIPC();
  createWindow();
  setupMenu();
  setupAutoUpdater();
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});
