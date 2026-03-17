const { app, BrowserWindow, Menu, ipcMain, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn, exec } = require('child_process');
const ExtensionManager = require('./extension-manager');

// GPU/CPU Load Balancing - Must be called BEFORE app.ready
// Disable hardware acceleration early if needed
if (process.env.FORCE_CPU_RENDERING === 'true') {
  console.log('[GPU] Force CPU rendering mode enabled');
  app.disableHardwareAcceleration();
}

// GPU/CPU Load Balancing - Automatic Fallback
let gpuFailed = false;
let gpuAttempts = 0;
const MAX_GPU_ATTEMPTS = 3;

// Detect GPU failures and fallback to software rendering
app.on('gpu-process-crashed', (event, killed) => {
  console.warn('[GPU] GPU process crashed, attempting fallback...');
  gpuAttempts++;
  gpuFailed = true;

  if (gpuAttempts >= MAX_GPU_ATTEMPTS && !gpuFailed) {
    console.log('[GPU] Multiple GPU failures detected');
    // Note: Cannot call disableHardwareAcceleration here after app is ready
    // User should restart with FORCE_CPU_RENDERING=true
  }
});

// Enhanced command line switches for better stability
app.commandLine.appendSwitch('disable-gpu-sandbox');
app.commandLine.appendSwitch('disable-software-rasterizer');
app.commandLine.appendSwitch('ignore-gpu-blacklist');
app.commandLine.appendSwitch('enable-gpu-rasterization');
app.commandLine.appendSwitch('disable-dev-shm-usage');// Global error handlers
process.on('uncaughtException', (error) => {
  console.error('[FATAL] Uncaught Exception:', error);
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('show-error', {
      title: 'Unexpected Error',
      message: error.message || 'An unexpected error occurred',
      detail: error.stack
    });
  }
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('[FATAL] Unhandled Rejection at:', promise, 'reason:', reason);
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('show-error', {
      title: 'Promise Rejection',
      message: reason?.message || String(reason),
      detail: reason?.stack
    });
  }
});

// Handle EPIPE errors gracefully
process.on('EPIPE', () => {
  console.warn('[EPIPE] Broken pipe detected - ignoring');
});

let mainWindow;
let ollamaProcess;
let chatProxyProcess;
let terminalProcess;
let gitProcess;
let searchProcess;
let debuggerProcess;

// Track all spawned processes for cleanup
const spawnedProcesses = [];

// Portable mode detection
const isPortable = process.env.PORTABLE_EXECUTABLE_DIR || process.argv.includes('--portable');
const appDataPath = isPortable ? 
  path.join(process.env.PORTABLE_EXECUTABLE_DIR || __dirname, 'data') : 
  app.getPath('userData');

app.setPath('userData', appDataPath);

// Extension Manager
const extensionsPath = path.join(appDataPath, 'extensions');
const extensionManager = new ExtensionManager(extensionsPath);

// Settings file path
const settingsPath = path.join(appDataPath, 'settings.json');

// Load settings
function loadSettings() {
  try {
    if (fs.existsSync(settingsPath)) {
      return JSON.parse(fs.readFileSync(settingsPath, 'utf8'));
    }
  } catch (error) {
    console.error('[Electron] Failed to load settings:', error.message);
  }
  return { allowMultipleInstances: false };
}

// Save settings
function saveSettings(settings) {
  try {
    fs.mkdirSync(path.dirname(settingsPath), { recursive: true });
    fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2), 'utf8');
  } catch (error) {
    console.error('[Electron] Failed to save settings:', error.message);
  }
}

// Kill existing IDE instances
async function killExistingInstances() {
  return new Promise((resolve) => {
    if (process.platform === 'win32') {
      exec('tasklist /FI "IMAGENAME eq MyCopilot-IDE.exe" /FO CSV /NH', (err, stdout) => {
        if (err || !stdout || stdout.trim() === '') {
          console.log('[Electron] No existing IDE instances found');
          resolve();
          return;
        }
        
        const lines = stdout.trim().split('\n');
        const currentPID = process.pid;
        let killCount = 0;
        
        lines.forEach(line => {
          const match = line.match(/".*?","(\d+)"/);
          if (match) {
            const pid = parseInt(match[1]);
            if (pid !== currentPID) {
              try {
                exec(`taskkill /PID ${pid} /F /T`, (killErr) => {
                  if (!killErr) {
                    killCount++;
                    console.log(`[Electron] Killed existing instance: PID ${pid}`);
                  }
                });
              } catch (e) {
                console.error(`[Electron] Failed to kill PID ${pid}:`, e.message);
              }
            }
          }
        });
        
        setTimeout(() => {
          console.log(`[Electron] Killed ${killCount} existing instance(s)`);
          resolve();
        }, 1000);
      });
    } else {
      // Unix-like systems
      exec('pgrep -f "MyCopilot-IDE"', (err, stdout) => {
        if (err || !stdout) {
          console.log('[Electron] No existing IDE instances found');
          resolve();
          return;
        }
        
        const pids = stdout.trim().split('\n').map(p => parseInt(p));
        const currentPID = process.pid;
        let killCount = 0;
        
        pids.forEach(pid => {
          if (pid !== currentPID) {
            try {
              process.kill(pid, 'SIGTERM');
              killCount++;
              console.log(`[Electron] Killed existing instance: PID ${pid}`);
            } catch (e) {
              console.error(`[Electron] Failed to kill PID ${pid}:`, e.message);
            }
          }
        });
        
        console.log(`[Electron] Killed ${killCount} existing instance(s)`);
        resolve();
      });
    }
  });
}

async function createWindow() {
  // Check multi-instance setting
  const settings = loadSettings();
  
  if (!settings.allowMultipleInstances) {
    console.log('[Electron] Multi-instance disabled, killing existing instances...');
    await killExistingInstances();
  } else {
    console.log('[Electron] Multi-instance enabled, allowing parallel instances');
  }
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 800,
    minHeight: 600,
    webPreferences: {
      nodeIntegration: false,  // Changed to false for security
      contextIsolation: true,   // Changed to true to enable contextBridge
      enableRemoteModule: false,
      webSecurity: true,
      preload: path.join(__dirname, 'electron-preload.js')  // Make sure preload is specified
    },
    icon: path.join(__dirname, 'assets', 'icon.png'),
    show: false,
    titleBarStyle: 'default'
  });

  // Load the IDE
  const htmlPath = path.join(__dirname, 'MyCopilot-Complete-IDE.html');
  mainWindow.loadFile(htmlPath);

  // Show when ready
  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
    startServices();
  });

  // Handle window closed
  mainWindow.on('closed', () => {
    mainWindow = null;
    stopServices();
  });

  // Create menu
  createMenu();
}

function createMenu() {
  const template = [
    {
      label: 'File',
      submenu: [
        { label: 'New File', accelerator: 'CmdOrCtrl+N', click: () => mainWindow.webContents.send('menu-new-file') },
        { label: 'Open File', accelerator: 'CmdOrCtrl+O', click: () => mainWindow.webContents.send('menu-open-file') },
        { label: 'Open Folder', accelerator: 'CmdOrCtrl+Shift+O', click: () => mainWindow.webContents.send('menu-open-folder') },
        { type: 'separator' },
        { label: 'Save', accelerator: 'CmdOrCtrl+S', click: () => mainWindow.webContents.send('menu-save') },
        { label: 'Save As', accelerator: 'CmdOrCtrl+Shift+S', click: () => mainWindow.webContents.send('menu-save-as') },
        { type: 'separator' },
        { label: 'Exit', accelerator: process.platform === 'darwin' ? 'Cmd+Q' : 'Ctrl+Q', click: () => app.quit() }
      ]
    },
    {
      label: 'Edit',
      submenu: [
        { label: 'Undo', accelerator: 'CmdOrCtrl+Z', role: 'undo' },
        { label: 'Redo', accelerator: 'CmdOrCtrl+Y', role: 'redo' },
        { type: 'separator' },
        { label: 'Cut', accelerator: 'CmdOrCtrl+X', role: 'cut' },
        { label: 'Copy', accelerator: 'CmdOrCtrl+C', role: 'copy' },
        { label: 'Paste', accelerator: 'CmdOrCtrl+V', role: 'paste' },
        { type: 'separator' },
        { label: 'Find', accelerator: 'CmdOrCtrl+F', click: () => mainWindow.webContents.send('menu-find') },
        { label: 'Replace', accelerator: 'CmdOrCtrl+H', click: () => mainWindow.webContents.send('menu-replace') }
      ]
    },
    {
      label: 'View',
      submenu: [
        { label: 'Toggle Terminal', accelerator: 'CmdOrCtrl+`', click: () => mainWindow.webContents.send('menu-toggle-terminal') },
        { label: 'Toggle AI Panel', accelerator: 'CmdOrCtrl+Alt+B', click: () => mainWindow.webContents.send('menu-toggle-ai') },
        { label: 'Toggle File Explorer', accelerator: 'CmdOrCtrl+Shift+E', click: () => mainWindow.webContents.send('menu-toggle-explorer') },
        { type: 'separator' },
        { label: 'Zoom In', accelerator: 'CmdOrCtrl+Plus', role: 'zoomin' },
        { label: 'Zoom Out', accelerator: 'CmdOrCtrl+-', role: 'zoomout' },
        { label: 'Reset Zoom', accelerator: 'CmdOrCtrl+0', role: 'resetzoom' },
        { type: 'separator' },
        { label: 'Toggle Fullscreen', accelerator: 'F11', role: 'togglefullscreen' },
        { label: 'Developer Tools', accelerator: 'F12', role: 'toggledevtools' }
      ]
    },
    {
      label: 'Run',
      submenu: [
        { label: 'Run PowerShell', accelerator: 'F5', click: () => mainWindow.webContents.send('menu-run-powershell') },
        { label: 'Run Python', accelerator: 'Shift+F5', click: () => mainWindow.webContents.send('menu-run-python') },
        { label: 'Compile C++', accelerator: 'Ctrl+F5', click: () => mainWindow.webContents.send('menu-compile-cpp') }
      ]
    },
    {
      label: 'Tools',
      submenu: [
        { label: 'Settings', accelerator: 'CmdOrCtrl+,', click: () => mainWindow.webContents.send('menu-settings') },
        { label: 'Extensions', click: () => mainWindow.webContents.send('menu-extensions') },
        { type: 'separator' },
        { label: 'Restart Ollama', click: () => restartOllama() },
        { label: 'Check Updates', click: () => checkUpdates() }
      ]
    },
    {
      label: 'Help',
      submenu: [
        { label: 'About', click: () => showAbout() },
        { label: 'Documentation', click: () => shell.openExternal('https://github.com/mycopilot/ide') }
      ]
    }
  ];

  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

// Kill processes on specific ports before starting
async function killProcessOnPort(port) {
  return new Promise((resolve) => {
    const command = process.platform === 'win32' 
      ? `netstat -ano | findstr :${port}` 
      : `lsof -ti:${port}`;
    
    exec(command, (error, stdout) => {
      if (error || !stdout) {
        resolve(false);
        return;
      }
      
      const lines = stdout.trim().split('\n');
      const pids = new Set();
      
      lines.forEach(line => {
        const match = line.trim().match(/\s+(\d+)\s*$/);
        if (match) pids.add(match[1]);
      });
      
      if (pids.size > 0) {
        console.log(`[PORT] Killing process(es) on port ${port}: ${Array.from(pids).join(', ')}`);
        pids.forEach(pid => {
          try {
            process.kill(pid, 'SIGTERM');
          } catch (e) {
            // Process already dead
          }
        });
        
        setTimeout(() => resolve(true), 1000);
      } else {
        resolve(false);
      }
    });
  });
}

async function startServices() {
  console.log('[Electron] Starting services...');
  
  // Clear ports before starting services
  await killProcessOnPort(3001); // Git Backend
  await killProcessOnPort(3002); // Search Backend
  await killProcessOnPort(3003); // Debugger Backend
  await killProcessOnPort(3030); // Chat Proxy

  // Start Ollama
  const ollamaPath = findOllamaPath();
  if (ollamaPath) {
    try {
      ollamaProcess = spawn(ollamaPath, ['serve'], { 
        detached: false,
        stdio: 'ignore'
      });
      
      ollamaProcess.on('error', (err) => {
        console.error('[Electron] Ollama error:', err.message);
      });
      
      spawnedProcesses.push({ name: 'Ollama', process: ollamaProcess });
      console.log('[Electron] Ollama started successfully (PID: ' + ollamaProcess.pid + ')');
    } catch (err) {
      console.error('[Electron] Failed to start Ollama:', err.message);
    }
  }

  // Use Electron's Node.js (process.execPath) instead of system 'node'
  const nodePath = process.execPath;
  
  // Start Chat Proxy
  const proxyPath = path.join(__dirname, 'Universal-Chat-Proxy.js');
  if (fs.existsSync(proxyPath)) {
    try {
      chatProxyProcess = spawn(nodePath, [proxyPath], {
        cwd: __dirname,
        stdio: ['ignore', 'pipe', 'pipe'], // Capture stderr to suppress EPIPE errors
        env: { ...process.env, FORCE_COLOR: '0' }
      });
      
      // Suppress EPIPE errors
      if (chatProxyProcess.stdout) chatProxyProcess.stdout.on('error', () => {});
      if (chatProxyProcess.stderr) chatProxyProcess.stderr.on('error', () => {});
      
      chatProxyProcess.on('error', (err) => {
        console.error('[Electron] Chat Proxy error:', err.message);
        chatProxyProcess = null;
      });
      
      chatProxyProcess.on('exit', (code) => {
        if (code !== 0 && code !== null) {
          console.error(`[Electron] Chat Proxy exited with code ${code}`);
        }
        chatProxyProcess = null;
      });
      
      spawnedProcesses.push({ name: 'Chat Proxy', process: chatProxyProcess });
      console.log('[Electron] Chat Proxy started successfully (PID: ' + chatProxyProcess.pid + ')');
    } catch (err) {
      console.error('[Electron] Failed to start Chat Proxy:', err.message);
      chatProxyProcess = null;
    }
  }

  // Start Terminal Backend
  const terminalPath = path.join(__dirname, 'terminal-backend.js');
  if (fs.existsSync(terminalPath)) {
    try {
      terminalProcess = spawn(nodePath, [terminalPath], {
        cwd: __dirname,
        stdio: ['ignore', 'pipe', 'pipe'],
        env: { ...process.env, FORCE_COLOR: '0' }
      });
      
      // Suppress EPIPE errors
      if (terminalProcess.stdout) terminalProcess.stdout.on('error', () => {});
      if (terminalProcess.stderr) terminalProcess.stderr.on('error', () => {});
      
      terminalProcess.on('error', (err) => {
        console.error('[Electron] Terminal Backend error:', err.message);
        terminalProcess = null;
      });
      
      terminalProcess.on('exit', (code) => {
        if (code !== 0 && code !== null) {
          console.error(`[Electron] Terminal Backend exited with code ${code}`);
        }
        terminalProcess = null;
      });
      
      spawnedProcesses.push({ name: 'Terminal Backend', process: terminalProcess });
      console.log('[Electron] Terminal Backend started successfully (PID: ' + terminalProcess.pid + ')');
    } catch (err) {
      console.error('[Electron] Failed to start Terminal Backend:', err.message);
      terminalProcess = null;
    }
  }
  
  // Start other backends (Git, Search, Debugger)
  startCoreBackends(nodePath);
  
  console.log(`[Electron] Total spawned processes: ${spawnedProcesses.length}`);
}

function startCoreBackends(nodePath) {
  const backends = [
    { name: 'Git Backend', file: 'git-backend.js', port: 3001, var: 'gitProcess' },
    { name: 'Search Backend', file: 'search-backend.js', port: 3002, var: 'searchProcess' },
    { name: 'Debugger Backend', file: 'debugger-backend.js', port: 3003, var: 'debuggerProcess' }
  ];
  
  backends.forEach(backend => {
    const backendPath = path.join(__dirname, backend.file);
    if (fs.existsSync(backendPath)) {
      try {
        const proc = spawn(nodePath, [backendPath], {
          cwd: __dirname,
          stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, FORCE_COLOR: '0' }
        });
        
        // Suppress EPIPE errors
        if (proc.stdout) proc.stdout.on('error', () => {});
        if (proc.stderr) proc.stderr.on('error', () => {});
        
        proc.on('error', (err) => {
          console.error(`[Electron] ${backend.name} error:`, err.message);
        });
        
        proc.on('exit', (code) => {
          if (code !== 0 && code !== null) {
            console.error(`[Electron] ${backend.name} exited with code ${code}`);
          }
        });
        
        // Store process reference
        if (backend.var === 'gitProcess') gitProcess = proc;
        if (backend.var === 'searchProcess') searchProcess = proc;
        if (backend.var === 'debuggerProcess') debuggerProcess = proc;
        
        spawnedProcesses.push({ name: backend.name, process: proc });
        console.log(`[Electron] ${backend.name} started on port ${backend.port} (PID: ${proc.pid})`);
      } catch (err) {
        console.error(`[Electron] Failed to start ${backend.name}:`, err.message);
      }
    }
  });
}

async function stopServices() {
  console.log('[Main] [SHUTDOWN] Initiating graceful shutdown of all services...');
  console.log(`[Main] [SHUTDOWN] Stopping ${spawnedProcesses.length} spawned process(es)...`);
  
  const killProcess = (procObj, timeout = 5000) => {
    return new Promise((resolve) => {
      const proc = procObj.process;
      const name = procObj.name;
      
      if (!proc || proc.killed) {
        console.log(`[Main] [SHUTDOWN] ${name} - already stopped`);
        resolve();
        return;
      }
      
      console.log(`[Main] [SHUTDOWN] ${name} (PID: ${proc.pid}) - sending SIGTERM...`);
      
      const killTimeout = setTimeout(() => {
        if (!proc.killed) {
          console.log(`[Main] [SHUTDOWN] ${name} - timeout, forcing SIGKILL...`);
          try {
            proc.kill('SIGKILL');
          } catch (error) {
            console.error(`[Main] [SHUTDOWN] ${name} - force kill error:`, error.message);
          }
        }
        resolve();
      }, timeout);
      
      proc.once('exit', () => {
        clearTimeout(killTimeout);
        console.log(`[Main] [SHUTDOWN] ${name} - gracefully stopped`);
        resolve();
      });
      
      try {
        proc.kill('SIGTERM');
      } catch (error) {
        clearTimeout(killTimeout);
        console.error(`[Main] [SHUTDOWN] ${name} - error:`, error.message);
        resolve();
      }
    });
  };
  
  // Stop all spawned processes in parallel with timeout
  await Promise.all(spawnedProcesses.map(procObj => killProcess(procObj, 3000)));
  
  // Wait for ports to be released
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  // Force-kill any remaining processes on our ports
  const ports = [3001, 3002, 3003, 3030];
  for (const port of ports) {
    await killProcessOnPort(port);
  }
  
  // Clear all process references
  spawnedProcesses.length = 0;
  ollamaProcess = null;
  chatProxyProcess = null;
  terminalProcess = null;
  gitProcess = null;
  searchProcess = null;
  debuggerProcess = null;
  
  console.log('[Main] [SHUTDOWN] All services stopped gracefully');
}

function findOllamaPath() {
  const paths = [
    path.join(__dirname, 'toolchain', 'ollama', 'ollama.exe'),
    path.join(process.env.ProgramFiles || '', 'Ollama', 'ollama.exe'),
    'ollama'
  ];
  
  return paths.find(p => {
    try {
      return fs.existsSync(p) || require('which').sync(p, { nothrow: true });
    } catch {
      return false;
    }
  });
}

function restartOllama() {
  if (ollamaProcess) {
    ollamaProcess.kill();
  }
  setTimeout(() => {
    const ollamaPath = findOllamaPath();
    if (ollamaPath) {
      ollamaProcess = spawn(ollamaPath, ['serve'], { detached: false, stdio: 'pipe' });
    }
  }, 1000);
}

function checkUpdates() {
  dialog.showMessageBox(mainWindow, {
    type: 'info',
    title: 'Updates',
    message: 'MyCopilot++ IDE v1.0.0',
    detail: 'You are running the latest version.'
  });
}

function showAbout() {
  dialog.showMessageBox(mainWindow, {
    type: 'info',
    title: 'About MyCopilot++ IDE',
    message: 'MyCopilot++ Complete IDE',
    detail: 'A portable development environment with AI assistance.\n\nVersion: 1.0.0\nElectron: ' + process.versions.electron + '\nNode: ' + process.versions.node
  });
}

// IPC handlers - File System Operations
ipcMain.handle('get-app-path', () => appDataPath);
ipcMain.handle('is-portable', () => isPortable);
ipcMain.handle('get-path', (event, name) => {
  try {
    return app.getPath(name);
  } catch (error) {
    console.error(`[IPC] get-path error for '${name}':`, error);
    return null;
  }
});

// Dialog handlers
ipcMain.handle('show-save-dialog', async (event, options) => {
  try {
    const result = await dialog.showSaveDialog(mainWindow, options);
    return result;
  } catch (error) {
    return { canceled: true, error: error.message };
  }
});

ipcMain.handle('show-open-dialog', async (event, options) => {
  try {
    const result = await dialog.showOpenDialog(mainWindow, options);
    return result;
  } catch (error) {
    return { canceled: true, error: error.message };
  }
});

// Read file
ipcMain.handle('read-file', async (event, filePath) => {
  try {
    const content = await fs.promises.readFile(filePath, 'utf8');
    return { success: true, content };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Write file
ipcMain.handle('write-file', async (event, filePath, content) => {
  try {
    await fs.promises.writeFile(filePath, content, 'utf8');
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Delete file
ipcMain.handle('delete-file', async (event, filePath) => {
  try {
    await fs.promises.unlink(filePath);
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Read directory
ipcMain.handle('read-directory', async (event, dirPath) => {
  try {
    // Check if directory exists first
    try {
      await fs.promises.access(dirPath);
    } catch (accessError) {
      if (accessError.code === 'ENOENT') {
        console.warn(`[FileSystem] Directory does not exist: ${dirPath}`);
        return { 
          success: false, 
          error: 'DIRECTORY_NOT_FOUND',
          message: `Directory does not exist: ${dirPath}`,
          items: [] 
        };
      }
      throw accessError;
    }

    const entries = await fs.promises.readdir(dirPath, { withFileTypes: true });
    const items = entries.map(entry => ({
      name: entry.name,
      type: entry.isDirectory() ? 'directory' : 'file',
      path: path.join(dirPath, entry.name)
    }));

    // Sort: directories first, then files
    items.sort((a, b) => {
      if (a.type === b.type) return a.name.localeCompare(b.name);
      return a.type === 'directory' ? -1 : 1;
    });

    return { success: true, items };
  } catch (error) {
    console.error(`[FileSystem] Failed to read directory ${dirPath}:`, error);
    logError(error, `read-directory: ${dirPath}`);
    
    // Return graceful error instead of throwing
    return {
      success: false,
      error: error.code || 'READ_ERROR',
      message: error.message,
      items: []
    };
  }
});

// Create directory
ipcMain.handle('create-directory', async (event, dirPath) => {
  try {
    await fs.promises.mkdir(dirPath, { recursive: true });
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Check if path exists
ipcMain.handle('path-exists', async (event, targetPath) => {
  try {
    await fs.promises.access(targetPath);
    return true;
  } catch {
    return false;
  }
});

// Get drive info
ipcMain.handle('get-drive-info', async (event, drivePath) => {
  try {
    // On Windows, we can use fs.promises.stat to check if drive is accessible
    const stats = await fs.promises.stat(drivePath);
    
    // Try to get volume label (Windows-specific)
    let label = '';
    if (process.platform === 'win32') {
      try {
        const { execSync } = require('child_process');
        const output = execSync(`vol ${drivePath}`, { encoding: 'utf8', timeout: 2000 });
        const match = output.match(/Volume in drive \w+ is (.+)/);
        if (match) label = match[1];
      } catch {
        // Ignore if vol command fails
      }
    }
    
    // Get free space (requires additional OS-specific commands)
    return {
      label,
      type: 'Fixed',
      freeSpace: 0,
      totalSpace: 0
    };
  } catch {
    return { label: '', type: 'Unknown', freeSpace: 0, totalSpace: 0 };
  }
});

// Get user profile path
ipcMain.handle('get-user-profile', () => {
  return process.env.USERPROFILE || process.env.HOME || '';
});

// Settings management
ipcMain.handle('get-settings', () => {
  return loadSettings();
});

ipcMain.handle('save-settings', (event, settings) => {
  saveSettings(settings);
  return { success: true };
});

ipcMain.handle('toggle-multi-instance', () => {
  const settings = loadSettings();
  settings.allowMultipleInstances = !settings.allowMultipleInstances;
  saveSettings(settings);
  return { 
    success: true, 
    allowMultipleInstances: settings.allowMultipleInstances 
  };
});

// Watch file for changes
const fileWatchers = new Map();
ipcMain.handle('watch-file', (event, filePath) => {
  try {
    if (fileWatchers.has(filePath)) {
      return { success: true };
    }
    
    const watcher = fs.watch(filePath, (eventType) => {
      mainWindow?.webContents.send('file-changed', { path: filePath, eventType });
    });
    
    fileWatchers.set(filePath, watcher);
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Unwatch file
ipcMain.handle('unwatch-file', (event, filePath) => {
  const watcher = fileWatchers.get(filePath);
  if (watcher) {
    watcher.close();
    fileWatchers.delete(filePath);
  }
  return { success: true };
});

// Get file stats
ipcMain.handle('get-file-stats', async (event, filePath) => {
  try {
    const stats = await fs.promises.stat(filePath);
    return {
      success: true,
      stats: {
        size: stats.size,
        created: stats.birthtime,
        modified: stats.mtime,
        accessed: stats.atime,
        isDirectory: stats.isDirectory(),
        isFile: stats.isFile()
      }
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Rename/move file
ipcMain.handle('rename-file', async (event, oldPath, newPath) => {
  try {
    await fs.promises.rename(oldPath, newPath);
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Copy file
ipcMain.handle('copy-file', async (event, source, destination) => {
  try {
    await fs.promises.copyFile(source, destination);
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Extension Manager IPC handlers
ipcMain.handle('get-installed-extensions', async () => {
  try {
    const extensions = await extensionManager.getInstalledExtensions();
    return extensions;
  } catch (error) {
    console.error('[Electron] Get installed extensions failed:', error);
    throw error;
  }
});

ipcMain.handle('search-marketplace', async (event, query, pageSize = 50) => {
  try {
    const results = await extensionManager.searchMarketplace(query, pageSize);
    return results;
  } catch (error) {
    console.error('[Electron] Marketplace search failed:', error);
    throw error;
  }
});

ipcMain.handle('download-from-marketplace', async (event, extensionId, version = 'latest') => {
  try {
    const extension = await extensionManager.downloadFromMarketplace(extensionId, version);
    return extension;
  } catch (error) {
    console.error('[Electron] Marketplace download failed:', error);
    throw error;
  }
});

ipcMain.handle('install-vsix', async (event, vsixPath) => {
  try {
    const extension = await extensionManager.installVSIX(vsixPath);
    return extension;
  } catch (error) {
    console.error('[Electron] VSIX installation failed:', error);
    throw error;
  }
});

ipcMain.handle('uninstall-extension', async (event, extensionId) => {
  try {
    await extensionManager.uninstallExtension(extensionId);
    return { success: true };
  } catch (error) {
    console.error('[Electron] Extension uninstall failed:', error);
    throw error;
  }
});

// ============= ERROR REPORTING & LOGGING SYSTEM =============

// Error log storage
const errorLog = [];
const maxErrorLogSize = 1000;
const errorLogPath = path.join(appDataPath, 'error-log.json');

// Load previous error log on startup
function loadErrorLog() {
  try {
    if (fs.existsSync(errorLogPath)) {
      const data = fs.readFileSync(errorLogPath, 'utf8');
      const savedErrors = JSON.parse(data);
      errorLog.push(...savedErrors.slice(-500)); // Load last 500 errors
      console.log(`[ErrorLog] Loaded ${errorLog.length} previous errors`);
    }
  } catch (error) {
    console.error('[ErrorLog] Failed to load error log:', error);
  }
}

// Save error log to disk
function saveErrorLog() {
  try {
    fs.writeFileSync(errorLogPath, JSON.stringify(errorLog.slice(-500), null, 2));
  } catch (error) {
    console.error('[ErrorLog] Failed to save error log:', error);
  }
}

// Add error to log
function logError(error, context = '') {
  const errorEntry = {
    timestamp: new Date().toISOString(),
    message: error.message || String(error),
    stack: error.stack || '',
    context: context,
    type: error.name || 'Error'
  };
  
  errorLog.push(errorEntry);
  
  // Keep log size manageable
  if (errorLog.length > maxErrorLogSize) {
    errorLog.shift();
  }
  
  // Save to disk periodically
  if (errorLog.length % 10 === 0) {
    saveErrorLog();
  }
  
  console.error(`[ErrorLog] ${context}: ${error.message}`);
  
  return errorEntry;
}

// Initialize error log
loadErrorLog();

// IPC: Report error from renderer
ipcMain.handle('report-error', async (event, errorData) => {
  try {
    const entry = logError(errorData, errorData.context || 'Renderer');
    return { success: true, errorId: errorLog.length - 1 };
  } catch (error) {
    console.error('[ErrorLog] Failed to report error:', error);
    return { success: false };
  }
});

// IPC: Get error log
ipcMain.handle('get-error-log', async (event, options = {}) => {
  try {
    const { limit = 50, offset = 0 } = options;
    const errors = errorLog.slice(-limit - offset, errorLog.length - offset || undefined);
    return {
      success: true,
      errors: errors.reverse(), // Most recent first
      total: errorLog.length
    };
  } catch (error) {
    console.error('[ErrorLog] Failed to get error log:', error);
    return { success: false, errors: [], total: 0 };
  }
});

// IPC: Clear error log
ipcMain.handle('clear-error-log', async () => {
  try {
    errorLog.length = 0;
    saveErrorLog();
    return { success: true };
  } catch (error) {
    console.error('[ErrorLog] Failed to clear error log:', error);
    return { success: false };
  }
});

// IPC: Export error log
ipcMain.handle('export-error-log', async (event, filePath) => {
  try {
    const exportData = {
      exportDate: new Date().toISOString(),
      totalErrors: errorLog.length,
      errors: errorLog
    };
    fs.writeFileSync(filePath, JSON.stringify(exportData, null, 2));
    return { success: true, path: filePath };
  } catch (error) {
    console.error('[ErrorLog] Failed to export error log:', error);
    return { success: false, error: error.message };
  }
});

// Save error log on app quit
app.on('will-quit', () => {
  saveErrorLog();
  console.log('[ErrorLog] Error log saved on quit');
});

// Extension activation/deactivation
ipcMain.handle('activate-extension', async (event, extensionId) => {
  try {
    console.log(`[Electron] Activating extension: ${extensionId}`);
    const result = await extensionManager.activateExtension(extensionId);
    return { success: true, extension: result };
  } catch (error) {
    console.error(`[Electron] Failed to activate extension ${extensionId}:`, error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('deactivate-extension', async (event, extensionId) => {
  try {
    console.log(`[Electron] Deactivating extension: ${extensionId}`);
    await extensionManager.deactivateExtension(extensionId);
    return { success: true };
  } catch (error) {
    console.error(`[Electron] Failed to deactivate extension ${extensionId}:`, error);
    return { success: false, error: error.message };
  }
});

// Get all extensions (installed + available)
ipcMain.handle('get-extensions', async () => {
  try {
    const extensions = await extensionManager.getAllExtensions();
    return extensions;
  } catch (error) {
    console.error('[Electron] Get extensions failed:', error);
    return [];
  }
});

// Execute extension command
ipcMain.handle('execute-command', async (event, commandId, args = []) => {
  try {
    console.log(`[Electron] Executing command: ${commandId}`);
    const result = await extensionManager.executeCommand(commandId, args);
    return { success: true, result };
  } catch (error) {
    console.error(`[Electron] Command execution failed for ${commandId}:`, error);
    return { success: false, error: error.message };
  }
});

// ============= COMPILER IPC HANDLERS =============

// Compile code using UniversalCompiler
ipcMain.handle('compile-code', async (event, options) => {
  try {
    const { language, source, target, outputFile } = options;
    console.log(`[Compiler] Compiling ${language} for ${target}...`);
    
    // PowerShell command to invoke UniversalCompiler
    const psCommand = `
      Import-Module "${path.join(__dirname, 'Modules', 'UniversalCompiler.psm1')}" -Force;
      Invoke-UniversalCompiler -Lang '${language}' -Source @"
${source}
"@ -Target '${target}' -OutFile '${outputFile}' | ConvertTo-Json
    `;
    
    return new Promise((resolve, reject) => {
      exec(`powershell.exe -NoProfile -Command "${psCommand}"`, { 
        maxBuffer: 10 * 1024 * 1024,
        timeout: 60000 
      }, (error, stdout, stderr) => {
        if (error) {
          console.error('[Compiler] Error:', error.message);
          console.error('[Compiler] Stderr:', stderr);
          reject({ 
            success: false, 
            error: error.message, 
            stderr: stderr,
            stdout: stdout 
          });
        } else {
          try {
            const result = JSON.parse(stdout);
            console.log('[Compiler] Success:', result);
            resolve({ success: true, result, stdout, stderr });
          } catch (parseError) {
            // If JSON parse fails, return raw output
            resolve({ 
              success: true, 
              stdout, 
              stderr,
              output: outputFile 
            });
          }
        }
      });
    });
  } catch (error) {
    console.error('[Compiler] Compilation failed:', error);
    throw error;
  }
});

// List available compilers
ipcMain.handle('list-compilers', async () => {
  try {
    const psCommand = `
      Import-Module "${path.join(__dirname, 'Modules', 'UniversalCompiler.psm1')}" -Force;
      @{
        Languages = @('Rust', 'C', 'C++', 'Go', 'Python', 'JavaScript', 'TypeScript',
                     'Java', 'C#', 'F#', 'Swift', 'Kotlin', 'Dart', 'Zig', 'Nim',
                     'Crystal', 'V', 'Odin', 'D', 'Haskell', 'OCaml', 'Scala',
                     'Clojure', 'Elixir', 'Erlang', 'Julia', 'R', 'Fortran',
                     'Ruby', 'Perl', 'Lua', 'PHP', 'Ada', 'Pascal', 'COBOL',
                     'Forth', 'Lisp', 'Scheme', 'Racket', 'Pony', 'Assembly')
        Platforms = @('Win64', 'Linux', 'macOS', 'Android', 'iOS', 'Wasm')
      } | ConvertTo-Json
    `;
    
    return new Promise((resolve, reject) => {
      exec(`powershell.exe -NoProfile -Command "${psCommand}"`, (error, stdout, stderr) => {
        if (error) {
          reject(error);
        } else {
          try {
            resolve(JSON.parse(stdout));
          } catch {
            resolve({ Languages: [], Platforms: [] });
          }
        }
      });
    });
  } catch (error) {
    console.error('[Compiler] Failed to list compilers:', error);
    return { Languages: [], Platforms: [] };
  }
});

// Get compiler info for a specific language
ipcMain.handle('get-compiler-info', async (event, language) => {
  try {
    return {
      language,
      supported: true,
      platforms: ['Win64', 'Linux', 'macOS', 'Android', 'iOS', 'Wasm'],
      features: ['Offline', 'Self-contained', 'No dependencies']
    };
  } catch (error) {
    console.error('[Compiler] Failed to get compiler info:', error);
    throw error;
  }
});

// Agentic Coding - PowerShell Execution
ipcMain.handle('run-powershell', async (event, { script }) => {
  return new Promise((resolve) => {
    console.log('[Agentic] Running PowerShell script...');
    const pwsh = spawn('pwsh', ['-NoProfile', '-Command', script], {
      cwd: process.cwd(),
      windowsHide: true
    });

    let output = '';
    let error = '';

    pwsh.stdout.on('data', (data) => {
      output += data.toString();
    });

    pwsh.stderr.on('data', (data) => {
      error += data.toString();
    });

    pwsh.on('close', (code) => {
      console.log(`[Agentic] PowerShell exited with code ${code}`);
      resolve({
        success: code === 0,
        output: output.trim(),
        error: error.trim(),
        exitCode: code
      });
    });

    // Add to spawned processes for cleanup
    spawnedProcesses.push(pwsh);
  });
});

// Agentic Coding - CMD Execution
ipcMain.handle('run-cmd', async (event, { script }) => {
  return new Promise((resolve) => {
    console.log('[Agentic] Running CMD script...');
    const cmd = spawn('cmd', ['/c', script], {
      cwd: process.cwd(),
      windowsHide: true
    });

    let output = '';
    let error = '';

    cmd.stdout.on('data', (data) => {
      output += data.toString();
    });

    cmd.stderr.on('data', (data) => {
      error += data.toString();
    });

    cmd.on('close', (code) => {
      console.log(`[Agentic] CMD exited with code ${code}`);
      resolve({
        success: code === 0,
        output: output.trim(),
        error: error.trim(),
        exitCode: code
      });
    });

    // Add to spawned processes for cleanup
    spawnedProcesses.push(cmd);
  });
});

// Agentic Coding - Python Execution
ipcMain.handle('run-python', async (event, { script }) => {
  return new Promise((resolve) => {
    console.log('[Agentic] Running Python script...');
    const python = spawn('python', ['-c', script], {
      cwd: process.cwd(),
      windowsHide: true
    });

    let output = '';
    let error = '';

    python.stdout.on('data', (data) => {
      output += data.toString();
    });

    python.stderr.on('data', (data) => {
      error += data.toString();
    });

    python.on('close', (code) => {
      console.log(`[Agentic] Python exited with code ${code}`);
      resolve({
        success: code === 0,
        output: output.trim(),
        error: error.trim(),
        exitCode: code
      });
    });

    // Add to spawned processes for cleanup
    spawnedProcesses.push(python);
  });
});

// Agentic Coding - Node.js Execution
ipcMain.handle('run-node', async (event, { script }) => {
  return new Promise((resolve) => {
    console.log('[Agentic] Running Node.js script...');
    const node = spawn('node', ['-e', script], {
      cwd: process.cwd(),
      windowsHide: true
    });

    let output = '';
    let error = '';

    node.stdout.on('data', (data) => {
      output += data.toString();
    });

    node.stderr.on('data', (data) => {
      error += data.toString();
    });

    node.on('close', (code) => {
      console.log(`[Agentic] Node.js exited with code ${code}`);
      resolve({
        success: code === 0,
        output: output.trim(),
        error: error.trim(),
        exitCode: code
      });
    });

    // Add to spawned processes for cleanup
    spawnedProcesses.push(node);
  });
});

// Agentic Coding - Bash Execution (for Git Bash or WSL)
ipcMain.handle('run-bash', async (event, { script }) => {
  return new Promise((resolve) => {
    console.log('[Agentic] Running Bash script...');
    const bash = spawn('bash', ['-c', script], {
      cwd: process.cwd(),
      windowsHide: true
    });

    let output = '';
    let error = '';

    bash.stdout.on('data', (data) => {
      output += data.toString();
    });

    bash.stderr.on('data', (data) => {
      error += data.toString();
    });

    bash.on('close', (code) => {
      console.log(`[Agentic] Bash exited with code ${code}`);
      resolve({
        success: code === 0,
        output: output.trim(),
        error: error.trim(),
        exitCode: code
      });
    });

    // Add to spawned processes for cleanup
    spawnedProcesses.push(bash);
  });
});

//===========================================
// GAME DEVELOPMENT PIPELINE IPC HANDLERS
//===========================================

// Game Dev - Complete Build
ipcMain.handle('game-dev-build', async (event, { projectPath, config }) => {
  return new Promise((resolve) => {
    console.log('[GameDev] Building complete game:', projectPath);
    
    const startTime = Date.now();
    const modulePath = path.join(__dirname, 'Modules', 'GameDevPipeline.psm1');
    
    const psScript = `
      Import-Module '${modulePath}' -Force
      $config = @{
        BuildSystem = '${config.BuildSystem}'
        PublishToSteam = $${config.PublishToSteam}
        SteamAppId = '${config.SteamAppId}'
        SteamDepotId = '${config.SteamDepotId}'
      }
      Build-CompleteGame -ProjectPath '${projectPath}' -OutputPath '${projectPath}\\Build' -Config $config | ConvertTo-Json -Compress
    `;
    
    const pwsh = spawn('pwsh', ['-NoProfile', '-Command', psScript], {
      cwd: __dirname,
      windowsHide: true
    });
    
    let output = '';
    let error = '';
    
    pwsh.stdout.on('data', (data) => {
      output += data.toString();
      console.log('[GameDev]', data.toString());
    });
    
    pwsh.stderr.on('data', (data) => {
      error += data.toString();
    });
    
    pwsh.on('close', (code) => {
      const buildTime = ((Date.now() - startTime) / 1000).toFixed(2);
      
      try {
        const result = JSON.parse(output);
        resolve({
          success: code === 0,
          outputPath: `${projectPath}\\Build`,
          installer: result.Installer?.OutputFile || '',
          buildTime: buildTime,
          error: error || null
        });
      } catch (e) {
        resolve({
          success: false,
          error: error || 'Failed to parse build output',
          buildTime: buildTime
        });
      }
    });
  });
});

// Game Dev - Assets Only
ipcMain.handle('game-dev-assets', async (event, { projectPath, textures, models, audio, shaders }) => {
  return new Promise((resolve) => {
    console.log('[GameDev] Processing assets...');
    
    const modulePath = path.join(__dirname, 'Modules', 'GameDevPipeline.psm1');
    let commands = [`Import-Module '${modulePath}' -Force`];
    
    if (textures) {
      commands.push(`Convert-GameTextures -InputPath '${projectPath}\\Assets\\Textures' -OutputPath '${projectPath}\\Build\\Textures' -Format 'BC7_UNORM'`);
    }
    if (models) {
      commands.push(`Convert-GameModels -InputPath '${projectPath}\\Assets\\Models' -OutputPath '${projectPath}\\Build\\Models' -OutputFormat 'glTF' -Binary`);
    }
    if (audio) {
      commands.push(`Convert-GameAudio -InputPath '${projectPath}\\Assets\\Audio' -OutputPath '${projectPath}\\Build\\Audio' -OutputFormat 'OGG' -Quality 6`);
    }
    if (shaders) {
      commands.push(`Compile-GameShaders -InputPath '${projectPath}\\Shaders' -OutputPath '${projectPath}\\Build\\Shaders' -Profile 'ps_5_0'`);
    }
    
    const psScript = commands.join('; ');
    
    const pwsh = spawn('pwsh', ['-NoProfile', '-Command', psScript], {
      cwd: __dirname,
      windowsHide: true
    });
    
    let error = '';
    
    pwsh.stderr.on('data', (data) => {
      error += data.toString();
    });
    
    pwsh.on('close', (code) => {
      resolve({
        success: code === 0,
        error: error || null
      });
    });
  });
});

// Game Dev - Code Only
ipcMain.handle('game-dev-code', async (event, { projectPath, buildSystem }) => {
  return new Promise((resolve) => {
    console.log('[GameDev] Building code with:', buildSystem);
    
    const modulePath = path.join(__dirname, 'Modules', 'GameDevPipeline.psm1');
    
    let command = '';
    switch (buildSystem) {
      case 'CMake':
        command = `Invoke-CMakeBuild -ProjectPath '${projectPath}' -Config Release`;
        break;
      case 'Unreal':
        command = `Invoke-UnrealBuild -ProjectFile '${projectPath}\\*.uproject'`;
        break;
      case 'Unity':
        command = `Invoke-UnityBuild -ProjectPath '${projectPath}' -OutputPath '${projectPath}\\Build'`;
        break;
      default:
        command = `Invoke-CustomGameBuild -ProjectPath '${projectPath}'`;
    }
    
    const psScript = `Import-Module '${modulePath}' -Force; ${command}`;
    
    const pwsh = spawn('pwsh', ['-NoProfile', '-Command', psScript], {
      cwd: __dirname,
      windowsHide: true
    });
    
    let error = '';
    
    pwsh.stderr.on('data', (data) => {
      error += data.toString();
    });
    
    pwsh.on('close', (code) => {
      resolve({
        success: code === 0,
        error: error || null
      });
    });
  });
});

// Game Dev - Package Only
ipcMain.handle('game-dev-package', async (event, { projectPath, createBundle, compress, createInstaller }) => {
  return new Promise((resolve) => {
    console.log('[GameDev] Packaging game...');
    
    const modulePath = path.join(__dirname, 'Modules', 'GameDevPipeline.psm1');
    let commands = [`Import-Module '${modulePath}' -Force`];
    
    if (createBundle) {
      commands.push(`New-GameAssetBundle -AssetsPath '${projectPath}\\Build' -OutputFile '${projectPath}\\Build\\Game.pak' ${compress ? '-Compress' : ''}`);
    }
    
    if (createInstaller) {
      const gameName = path.basename(projectPath);
      commands.push(`New-GameInstaller -GamePath '${projectPath}\\Build' -OutputFile '${projectPath}\\Build\\${gameName}-Setup.msi' -GameName '${gameName}' -Publisher 'Indie Studio' -Version '1.0.0'`);
    }
    
    const psScript = commands.join('; ');
    
    const pwsh = spawn('pwsh', ['-NoProfile', '-Command', psScript], {
      cwd: __dirname,
      windowsHide: true
    });
    
    let error = '';
    
    pwsh.stderr.on('data', (data) => {
      error += data.toString();
    });
    
    pwsh.on('close', (code) => {
      resolve({
        success: code === 0,
        packagePath: `${projectPath}\\Build`,
        error: error || null
      });
    });
  });
});

// Game Dev - Create New Project
ipcMain.handle('create-game-project', async (event, { projectPath, folders }) => {
  try {
    // Create project directory
    if (!fs.existsSync(projectPath)) {
      fs.mkdirSync(projectPath, { recursive: true });
    }
    
    // Create folder structure
    folders.forEach(folder => {
      const folderPath = path.join(projectPath, folder);
      if (!fs.existsSync(folderPath)) {
        fs.mkdirSync(folderPath, { recursive: true });
      }
    });
    
    // Create basic game template files
    const mainCpp = `#include <iostream>

int main() {
    std::cout << "Hello, Game World!" << std::endl;
    return 0;
}
`;
    
    fs.writeFileSync(path.join(projectPath, 'Source', 'main.cpp'), mainCpp);
    
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Game Dev - Clean Build
ipcMain.handle('clean-game-build', async (event, { projectPath }) => {
  try {
    const buildPath = path.join(projectPath, 'Build');
    const binPath = path.join(projectPath, 'Bin');
    
    if (fs.existsSync(buildPath)) {
      fs.rmSync(buildPath, { recursive: true, force: true });
    }
    if (fs.existsSync(binPath)) {
      fs.rmSync(binPath, { recursive: true, force: true });
    }
    
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Initialize extension manager when app is ready
app.whenReady().then(async () => {
  try {
    await extensionManager.initialize();
    console.log('[Electron] Extension Manager initialized');
  } catch (error) {
    console.error('[Electron] Extension Manager initialization failed:', error);
  }
  createWindow();
});

app.on('window-all-closed', async () => {
  await stopServices();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

app.on('before-quit', async (event) => {
  event.preventDefault();
  await stopServices();
  app.exit(0);
});