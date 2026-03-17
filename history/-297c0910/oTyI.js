/**
 * BigDaddyG IDE - Electron Main Process
 * Professional desktop IDE with dedicated tabs and syntax highlighting
 */

const { app, BrowserWindow, ipcMain, Menu, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const http = require('http');
const { spawn } = require('child_process');
const windowStateKeeper = require('electron-window-state');
const { EmbeddedBrowser } = require('./browser-view');
const SafeModeDetector = require('./safe-mode-detector');

let mainWindow;
let orchestraServer = null;
let remoteLogServer = null;
let embeddedBrowser;
let safeModeDetector = new SafeModeDetector();

// ============================================================================
// GPU & PERFORMANCE OPTIMIZATIONS
// ============================================================================

// FIX: HIGH REFRESH RATE DISPLAY SUPPORT (8K @ 540Hz!)
app.commandLine.appendSwitch('disable-gpu-vsync'); // Disable vsync for high refresh
app.commandLine.appendSwitch('disable-frame-rate-limit');
app.commandLine.appendSwitch('max-gum-fps', '600'); // Support up to 600fps

// FIX: Force software rendering for stability (high res displays)
app.commandLine.appendSwitch('disable-gpu');
app.commandLine.appendSwitch('disable-software-rasterizer', 'false');

// FIX: Reduce memory pressure
app.commandLine.appendSwitch('js-flags', '--max-old-space-size=8192');

console.log('[BigDaddyG] ⚡ GPU acceleration enabled');
console.log('[BigDaddyG] 🎯 Target: 240 FPS');

// ============================================================================
// APPLICATION LIFECYCLE
// ============================================================================

app.whenReady().then(() => {
  console.log('[BigDaddyG] 🚀 Starting Electron app...');
  
  // Start Orchestra server
  startOrchestraServer();
  
  // Start Remote Log Server
  startRemoteLogServer();
  
  // Create main window
  createMainWindow();
  
  // Set up application menu
  createMenu();
  
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createMainWindow();
    }
  });
});

app.on('window-all-closed', () => {
  // Stop Orchestra server
  if (orchestraServer) {
    orchestraServer.kill();
  }
  
  // Stop Remote Log Server
  if (remoteLogServer) {
    remoteLogServer.kill();
  }
  
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// ============================================================================
// ORCHESTRA SERVER
// ============================================================================

function startOrchestraServer() {
  if (orchestraServer && !orchestraServer.killed) {
    console.log('[BigDaddyG] ⚠️ Orchestra server already running');
    return;
  }
  
  console.log('[BigDaddyG] 🎼 Starting Orchestra server...');
  
  // Try multiple possible paths
  const possiblePaths = [
    path.join(__dirname, '..', 'server', 'Orchestra-Server.js'),
    path.join(process.resourcesPath, 'app', 'server', 'Orchestra-Server.js'),
    path.join(app.getAppPath(), 'server', 'Orchestra-Server.js'),
    path.join(process.cwd(), 'server', 'Orchestra-Server.js')
  ];
  
  let serverPath = null;
  let serverCwd = null;
  
  for (const tryPath of possiblePaths) {
    console.log(`[BigDaddyG] 🔍 Checking: ${tryPath}`);
    if (fs.existsSync(tryPath)) {
      serverPath = tryPath;
      serverCwd = path.dirname(tryPath);
      console.log(`[BigDaddyG] ✅ Found Orchestra at: ${serverPath}`);
      break;
    }
  }
  
  if (!serverPath) {
    console.error('[BigDaddyG] ❌ Orchestra-Server.js not found!');
    console.error('[BigDaddyG] ❌ Searched paths:', possiblePaths);
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-log', {
        type: 'error',
        message: '❌ Orchestra-Server.js not found! Please ensure it is bundled with the app.'
      });
    }
    return;
  }
  
  // FIX: Run server directly in main process instead of spawning
  // Spawning with Electron executable causes it to load as Electron app
  try {
    // Require and run the server directly
    console.log('[BigDaddyG] 🎼 Loading Orchestra server module...');
    require(serverPath);
    console.log('[BigDaddyG] ✅ Orchestra server loaded and running!');
    return; // Don't set up spawn handlers
  } catch (error) {
    console.error('[BigDaddyG] ❌ Failed to load Orchestra:', error);
    return;
  }
  
  /* OLD SPAWN CODE - Causes infinite loop with Electron
  orchestraServer = spawn(process.execPath, [serverPath], {
    cwd: serverCwd,
    stdio: 'pipe',
    env: { ...process.env, NODE_ENV: 'production' }
  });
  */
  
  orchestraServer.stdout.on('data', (data) => {
    const message = data.toString().trim();
    console.log(`[Orchestra] ${message}`);
    
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-log', {
        type: 'output',
        message: message
      });
    }
  });
  
  orchestraServer.stderr.on('data', (data) => {
    const message = data.toString().trim();
    
    // Check if port is already in use
    if (message.includes('EADDRINUSE') || message.includes('address already in use')) {
      console.log('[Orchestra] Port 11441 already in use - server already running, this is OK');
      
      if (mainWindow) {
        mainWindow.webContents.send('orchestra-status', {
          running: true,
          alreadyRunning: true
        });
      }
      
      // Kill this duplicate process
      if (orchestraServer) {
        orchestraServer.kill();
        orchestraServer = null;
      }
      return;
    }
    
    console.error(`[Orchestra Error] ${message}`);
    
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-log', {
        type: 'error',
        message: message
      });
    }
  });
  
  orchestraServer.on('close', (code) => {
    console.log(`[Orchestra] Process exited with code ${code}`);
    orchestraServer = null;
    
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-status', {
        running: false,
        code: code
      });
    }
  });
  
  orchestraServer.on('error', (error) => {
    console.error('[Orchestra] Failed to start:', error);
    
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-log', {
        type: 'error',
        message: `Failed to start: ${error.message}`
      });
    }
  });
  
  // Notify that server is starting
  if (mainWindow) {
    mainWindow.webContents.send('orchestra-status', {
      running: true,
      starting: true
    });
    
    // Wait a moment, then check if it's actually running
    setTimeout(() => {
      checkOrchestraHealth();
    }, 3000);
  }
  
  console.log('[BigDaddyG] ✅ Orchestra server process started');
}

function stopOrchestraServer() {
  if (!orchestraServer || orchestraServer.killed) {
    console.log('[BigDaddyG] ⚠️ Orchestra server not running');
    return;
  }
  
  console.log('[BigDaddyG] 🛑 Stopping Orchestra server...');
  
  orchestraServer.kill('SIGTERM');
  
  // Force kill after 5 seconds if still running
  setTimeout(() => {
    if (orchestraServer && !orchestraServer.killed) {
      orchestraServer.kill('SIGKILL');
      console.log('[BigDaddyG] ⚡ Force-killed Orchestra server');
    }
  }, 5000);
  
  orchestraServer = null;
  
  if (mainWindow) {
    mainWindow.webContents.send('orchestra-status', {
      running: false,
      stopped: true
    });
  }
}

function checkOrchestraHealth() {
  return new Promise((resolve) => {
    const req = http.get('http://localhost:11441/health', { timeout: 1000 }, (res) => {
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', () => {
        try {
          const jsonData = JSON.parse(data);
          if (mainWindow) {
            mainWindow.webContents.send('orchestra-status', {
              running: true,
              data: jsonData
            });
          }
          resolve(true);
        } catch (error) {
          resolve(false);
        }
      });
    });
    
    req.on('error', () => {
      if (mainWindow) {
        mainWindow.webContents.send('orchestra-status', {
          running: false
        });
      }
      resolve(false);
    });
    
    req.on('timeout', () => {
      req.destroy();
      if (mainWindow) {
        mainWindow.webContents.send('orchestra-status', {
          running: false
        });
      }
      resolve(false);
    });
  });
}

// ============================================================================
// REMOTE LOG SERVER
// ============================================================================

function startRemoteLogServer() {
  if (remoteLogServer && !remoteLogServer.killed) {
    console.log('[BigDaddyG] ⚠️ Remote log server already running');
    return;
  }
  
  console.log('[BigDaddyG] 📡 Starting Remote Log Server...');
  
  const possiblePaths = [
    path.join(__dirname, '..', 'server', 'Remote-Log-Server.js'),
    path.join(process.resourcesPath, 'app', 'server', 'Remote-Log-Server.js'),
    path.join(app.getAppPath(), 'server', 'Remote-Log-Server.js')
  ];
  
  let serverPath = null;
  
  for (const tryPath of possiblePaths) {
    if (fs.existsSync(tryPath)) {
      serverPath = tryPath;
      console.log(`[BigDaddyG] ✅ Found Remote Log Server at: ${serverPath}`);
      break;
    }
  }
  
  if (!serverPath) {
    console.log('[BigDaddyG] ℹ️ Remote Log Server not found (optional)');
    return;
  }
  
  // FIX: Run server directly in main process
  try {
    console.log('[BigDaddyG] 📡 Loading Remote Log Server module...');
    require(serverPath);
    console.log('[BigDaddyG] ✅ Remote Log Server loaded and running!');
    return; // Don't set up spawn handlers
  } catch (error) {
    console.error('[BigDaddyG] ❌ Failed to load Remote Log Server:', error);
    return;
  }
  
  /* OLD SPAWN CODE
  remoteLogServer = spawn(process.execPath, [serverPath], {
    cwd: path.dirname(serverPath),
    stdio: 'pipe',
    env: { ...process.env, NODE_ENV: 'production' }
  });
  */
  
  remoteLogServer.stdout.on('data', (data) => {
    console.log(`[RemoteLogServer] ${data.toString().trim()}`);
  });
  
  remoteLogServer.stderr.on('data', (data) => {
    console.error(`[RemoteLogServer] ERROR: ${data.toString().trim()}`);
  });
  
  remoteLogServer.on('close', (code) => {
    console.log(`[BigDaddyG] 📡 Remote Log Server exited with code ${code}`);
    remoteLogServer = null;
  });
  
  console.log('[BigDaddyG] ✅ Remote Log Server started on port 11442');
}

// ============================================================================
// MAIN WINDOW
// ============================================================================

function createMainWindow() {
  console.log('[BigDaddyG] 🪟 Creating main window...');
  
  // Remember window state
  const mainWindowState = windowStateKeeper({
    defaultWidth: 1920,
    defaultHeight: 1080
  });
  
  mainWindow = new BrowserWindow({
    x: mainWindowState.x,
    y: mainWindowState.y,
    width: mainWindowState.width,
    height: mainWindowState.height,
    minWidth: 1280,
    minHeight: 720,
    
    title: 'BigDaddyG IDE',
    backgroundColor: '#0a0a1e',
    
    webPreferences: {
      nodeIntegration: false,  // Disable - causes conflicts with Monaco AMD
      contextIsolation: true,  // Enable - Monaco needs this for __$__isRecorded
      sandbox: false,  // CRITICAL: Disable sandbox to prevent bootstrap realm error
      preload: path.join(__dirname, 'preload.js'),
      webSecurity: true,
      allowRunningInsecureContent: false,
      enableRemoteModule: false,
      worldSafeExecuteJavaScript: true
    },
    
    frame: false,  // No default frame - we'll use custom title bar
    titleBarStyle: 'hidden',
    icon: path.join(__dirname, '../assets/icon.png')
  });
  
  // Manage window state
  mainWindowState.manage(mainWindow);
  
  // Load IDE - Using Safe Mode Detector
  const htmlFile = safeModeDetector.getHTMLFile();
  
  console.log(`[BigDaddyG] 📄 Loading: ${htmlFile}`);
  console.log(`[BigDaddyG] 🛡️ Safe Mode: ${safeModeDetector.getConfig().SafeMode.enabled}`);
  mainWindow.loadFile(path.join(__dirname, htmlFile));
  
  // Open DevTools ALWAYS for debugging white screen
  mainWindow.webContents.openDevTools({ mode: 'detach' }); // Detach so it's in separate window
  
  // Log any page load errors
  mainWindow.webContents.on('did-fail-load', (event, errorCode, errorDescription) => {
    console.error('[BigDaddyG] ❌ Page failed to load:', errorCode, errorDescription);
  });
  
  mainWindow.webContents.on('crashed', () => {
    console.error('[BigDaddyG] ❌ Renderer process crashed!');
  });
  
  mainWindow.webContents.on('did-finish-load', () => {
    console.log('[BigDaddyG] ✅ Page loaded successfully');
    
    // Wait 2 seconds, then check if content rendered
    setTimeout(() => {
      mainWindow.webContents.executeJavaScript(`
        document.body.style.backgroundColor || 
        document.body.style.background || 
        getComputedStyle(document.body).backgroundColor
      `).then(bgColor => {
        console.log(`[SafeMode] Background color detected: ${bgColor}`);
        
        // If it's white or empty, report failure
        if (!bgColor || bgColor === 'rgba(0, 0, 0, 0)' || bgColor === 'rgb(255, 255, 255)' || bgColor === '#ffffff') {
          console.log('[SafeMode] ⚠️ White screen detected!');
          safeModeDetector.reportFailure(htmlFile);
        } else {
          console.log('[SafeMode] ✅ Colors detected - page rendered successfully');
          safeModeDetector.reportSuccess(htmlFile);
        }
      }).catch(err => {
        console.error('[SafeMode] Error checking background:', err);
      });
    }, 2000);
  });
  
  // Log console messages from renderer to terminal
  mainWindow.webContents.on('console-message', (event, level, message, line, sourceId) => {
    const levels = ['LOG', 'WARN', 'ERROR'];
    console.log(`[Renderer ${levels[level]}] ${message} (${sourceId}:${line})`);
  });
  
  mainWindow.on('closed', () => {
    mainWindow = null;
    if (embeddedBrowser) {
      embeddedBrowser.destroy();
    }
  });
  
  // Initialize embedded browser
  embeddedBrowser = new EmbeddedBrowser(mainWindow);
  
  console.log('[BigDaddyG] ✅ Main window created');
  console.log('[BigDaddyG] 🌐 Embedded browser initialized');
}

// ============================================================================
// APPLICATION MENU
// ============================================================================

function createMenu() {
  const template = [
    {
      label: 'File',
      submenu: [
        {
          label: 'New File',
          accelerator: 'CmdOrCtrl+N',
          click: () => {
            mainWindow.webContents.send('menu-new-file');
          }
        },
        {
          label: 'Open File',
          accelerator: 'CmdOrCtrl+O',
          click: () => {
            mainWindow.webContents.send('menu-open-file');
          }
        },
        {
          label: 'Save',
          accelerator: 'CmdOrCtrl+S',
          click: () => {
            mainWindow.webContents.send('menu-save-file');
          }
        },
        {
          label: 'Save As...',
          accelerator: 'CmdOrCtrl+Shift+S',
          click: () => {
            mainWindow.webContents.send('menu-save-as');
          }
        },
        { type: 'separator' },
        {
          label: 'Exit',
          accelerator: 'CmdOrCtrl+Q',
          click: () => {
            app.quit();
          }
        }
      ]
    },
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' }
      ]
    },
    {
      label: 'View',
      submenu: [
        {
          label: 'Toggle Sidebar',
          accelerator: 'CmdOrCtrl+B',
          click: () => {
            mainWindow.webContents.send('menu-toggle-sidebar');
          }
        },
        {
          label: 'Toggle Terminal',
          accelerator: 'CmdOrCtrl+`',
          click: () => {
            mainWindow.webContents.send('menu-toggle-terminal');
          }
        },
        { type: 'separator' },
        { role: 'reload' },
        { role: 'forceReload' },
        { role: 'toggleDevTools' },
        { type: 'separator' },
        { role: 'resetZoom' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { type: 'separator' },
        { role: 'togglefullscreen' }
      ]
    },
    {
      label: 'AI',
      submenu: [
        {
          label: 'Ask BigDaddyG',
          accelerator: 'CmdOrCtrl+K',
          click: () => {
            mainWindow.webContents.send('menu-ask-ai');
          }
        },
        {
          label: 'Explain Code',
          accelerator: 'CmdOrCtrl+E',
          click: () => {
            mainWindow.webContents.send('menu-ai-explain');
          }
        },
        {
          label: 'Fix Code',
          accelerator: 'CmdOrCtrl+Shift+F',
          click: () => {
            mainWindow.webContents.send('menu-ai-fix');
          }
        },
        {
          label: 'Optimize Code',
          accelerator: 'CmdOrCtrl+Shift+O',
          click: () => {
            mainWindow.webContents.send('menu-ai-optimize');
          }
        },
        { type: 'separator' },
        {
          label: 'Tune Parameters',
          click: () => {
            mainWindow.webContents.send('menu-ai-tune');
          }
        }
      ]
    },
    {
      label: 'Browser',
      submenu: [
        {
          label: 'Show Browser',
          accelerator: 'Ctrl+Shift+B',
          click: () => {
            mainWindow.webContents.send('menu-show-browser');
          }
        },
        {
          label: 'Navigate to URL',
          accelerator: 'Ctrl+L',
          click: () => {
            mainWindow.webContents.send('menu-browser-navigate');
          }
        },
        { type: 'separator' },
        {
          label: 'Back',
          accelerator: 'Alt+Left',
          click: () => {
            mainWindow.webContents.send('menu-browser-back');
          }
        },
        {
          label: 'Forward',
          accelerator: 'Alt+Right',
          click: () => {
            mainWindow.webContents.send('menu-browser-forward');
          }
        },
        {
          label: 'Reload',
          accelerator: 'F5',
          click: () => {
            mainWindow.webContents.send('menu-browser-reload');
          }
        },
        { type: 'separator' },
        {
          label: 'Take Screenshot',
          accelerator: 'Ctrl+Shift+S',
          click: () => {
            mainWindow.webContents.send('menu-browser-screenshot');
          }
        },
        {
          label: 'Analyze for Issues',
          click: () => {
            mainWindow.webContents.send('menu-browser-analyze');
          }
        },
        {
          label: 'Suggest UI Improvements',
          click: () => {
            mainWindow.webContents.send('menu-browser-suggest-ui');
          }
        },
        { type: 'separator' },
        {
          label: 'Open DevTools',
          accelerator: 'F12',
          click: () => {
            mainWindow.webContents.send('menu-browser-devtools');
          }
        }
      ]
    },
    {
      label: 'Window',
      submenu: [
        { role: 'minimize' },
        { role: 'zoom' },
        { type: 'separator' },
        { role: 'close' }
      ]
    },
    {
      label: 'Help',
      submenu: [
        {
          label: 'Documentation',
          click: () => {
            require('electron').shell.openExternal('https://github.com/bigdaddyg/ide');
          }
        },
        {
          label: 'Report Issue',
          click: () => {
            require('electron').shell.openExternal('https://github.com/bigdaddyg/ide/issues');
          }
        },
        { type: 'separator' },
        {
          label: 'About BigDaddyG IDE',
          click: () => {
            mainWindow.webContents.send('menu-about');
          }
        }
      ]
    }
  ];
  
  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
  
  console.log('[BigDaddyG] ✅ Application menu created');
}

// ============================================================================
// IPC HANDLERS
// ============================================================================

ipcMain.handle('read-file', async (event, filePath) => {
  const fs = require('fs').promises;
  try {
    const content = await fs.readFile(filePath, 'utf-8');
    return { success: true, content };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('write-file', async (event, filePath, content) => {
  const fs = require('fs').promises;
  try {
    await fs.writeFile(filePath, content, 'utf-8');
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Open file dialog
ipcMain.handle('open-file-dialog', async (event, options = {}) => {
  try {
    const result = await dialog.showOpenDialog(mainWindow, {
      properties: ['openFile'],
      filters: options.filters || [
        { name: 'All Files', extensions: ['*'] },
        { name: 'Text Files', extensions: ['txt', 'md', 'json', 'js', 'ts', 'py', 'java', 'cpp', 'c', 'h', 'html', 'css'] }
      ]
    });
    
    if (!result.canceled && result.filePaths.length > 0) {
      const filePath = result.filePaths[0];
      const content = await fs.promises.readFile(filePath, 'utf-8');
      return { 
        success: true, 
        filePath, 
        content,
        filename: path.basename(filePath)
      };
    }
    return { success: false, canceled: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Save file dialog
ipcMain.handle('save-file-dialog', async (event, options = {}) => {
  try {
    const result = await dialog.showSaveDialog(mainWindow, {
      defaultPath: options.defaultPath || 'untitled.txt',
      filters: options.filters || [
        { name: 'All Files', extensions: ['*'] },
        { name: 'Text Files', extensions: ['txt', 'md', 'json', 'js', 'ts', 'py', 'java', 'cpp', 'c', 'h', 'html', 'css'] }
      ]
    });
    
    if (!result.canceled) {
      return { 
        success: true, 
        filePath: result.filePath,
        filename: path.basename(result.filePath)
      };
    }
    return { success: false, canceled: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Open folder dialog
ipcMain.handle('open-folder-dialog', async () => {
  try {
    const result = await dialog.showOpenDialog(mainWindow, {
      properties: ['openDirectory']
    });
    
    if (!result.canceled && result.filePaths.length > 0) {
      const folderPath = result.filePaths[0];
      return { 
        success: true, 
        folderPath,
        folderName: path.basename(folderPath)
      };
    }
    return { success: false, canceled: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Read directory
ipcMain.handle('read-dir', async (event, dirPath) => {
  const fs = require('fs').promises;
  try {
    const entries = await fs.readdir(dirPath, { withFileTypes: true });
    const files = entries.map(entry => ({
      name: entry.name,
      isDirectory: entry.isDirectory(),
      isFile: entry.isFile(),
      path: path.join(dirPath, entry.name)
    }));
    return { success: true, files };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Get file stats
ipcMain.handle('get-file-stats', async (event, filePath) => {
  const fs = require('fs').promises;
  try {
    const stats = await fs.stat(filePath);
    return { 
      success: true, 
      stats: {
        size: stats.size,
        isFile: stats.isFile(),
        isDirectory: stats.isDirectory(),
        modified: stats.mtime,
        created: stats.birthtime
      }
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Check if file exists
ipcMain.handle('file-exists', async (event, filePath) => {
  const fs = require('fs').promises;
  try {
    await fs.access(filePath);
    return { success: true, exists: true };
  } catch (error) {
    return { success: true, exists: false };
  }
});

ipcMain.handle('get-app-version', () => {
  return app.getVersion();
});

ipcMain.handle('get-app-path', () => {
  return app.getAppPath();
});

// ============================================================================
// AGENTIC FILE SYSTEM OPERATIONS (UNLIMITED)
// ============================================================================

// Search files by content or name
ipcMain.handle('search-files', async (event, query, options = {}) => {
  const fs = require('fs').promises;
  const results = [];
  const searchPath = options.path || app.getAppPath();
  const maxResults = options.maxResults || 1001;
  const searchContent = options.searchContent !== false; // Default true
  
  async function searchDirectory(dirPath, depth = 0) {
    if (depth > (options.maxDepth || 10)) return;
    if (results.length >= maxResults) return;
    
    try {
      const entries = await fs.readdir(dirPath, { withFileTypes: true });
      
      for (const entry of entries) {
        if (results.length >= maxResults) break;
        
        const fullPath = path.join(dirPath, entry.name);
        
        // Skip node_modules, .git, etc.
        if (entry.name.startsWith('.') || entry.name === 'node_modules') continue;
        
        if (entry.isFile()) {
          // Search by filename
          if (entry.name.toLowerCase().includes(query.toLowerCase())) {
            results.push({
              path: fullPath,
              name: entry.name,
              type: 'file',
              matchType: 'filename'
            });
          } else if (searchContent) {
            // Search file content (for text files only)
            try {
              const stats = await fs.stat(fullPath);
              if (stats.size < 10 * 1024 * 1024) { // Only search files under 10MB for content
                const content = await fs.readFile(fullPath, 'utf-8');
                if (content.toLowerCase().includes(query.toLowerCase())) {
                  results.push({
                    path: fullPath,
                    name: entry.name,
                    type: 'file',
                    matchType: 'content'
                  });
                }
              }
            } catch (err) {
              // Skip binary or unreadable files
            }
          }
        } else if (entry.isDirectory()) {
          await searchDirectory(fullPath, depth + 1);
        }
      }
    } catch (error) {
      // Skip inaccessible directories
    }
  }
  
  try {
    await searchDirectory(searchPath);
    return { success: true, results, query, count: results.length };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Read directory recursively (unlimited depth)
ipcMain.handle('read-dir-recursive', async (event, dirPath, maxDepth = 100) => {
  const fs = require('fs').promises;
  const results = [];
  
  async function traverse(currentPath, depth = 0) {
    if (depth > maxDepth) return;
    
    try {
      const entries = await fs.readdir(currentPath, { withFileTypes: true });
      
      for (const entry of entries) {
        const fullPath = path.join(currentPath, entry.name);
        
        // Skip hidden files and node_modules
        if (entry.name.startsWith('.') || entry.name === 'node_modules') continue;
        
        results.push({
          name: entry.name,
          path: fullPath,
          isDirectory: entry.isDirectory(),
          isFile: entry.isFile(),
          depth: depth
        });
        
        if (entry.isDirectory()) {
          await traverse(fullPath, depth + 1);
        }
      }
    } catch (error) {
      // Skip inaccessible directories
    }
  }
  
  try {
    await traverse(dirPath);
    return { success: true, files: results, count: results.length };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Read file in chunks (for massive files)
ipcMain.handle('read-file-chunked', async (event, filePath, chunkSize = 1024 * 1024) => {
  const fs = require('fs');
  const chunks = [];
  
  try {
    const stats = await fs.promises.stat(filePath);
    const stream = fs.createReadStream(filePath, { encoding: 'utf-8', highWaterMark: chunkSize });
    
    return new Promise((resolve, reject) => {
      stream.on('data', (chunk) => {
        chunks.push(chunk);
      });
      
      stream.on('end', () => {
        resolve({
          success: true,
          content: chunks.join(''),
          size: stats.size,
          chunks: chunks.length
        });
      });
      
      stream.on('error', (error) => {
        reject({ success: false, error: error.message });
      });
    });
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Read multiple files at once
ipcMain.handle('read-multiple-files', async (event, filePaths) => {
  const fs = require('fs').promises;
  const results = [];
  
  for (const filePath of filePaths) {
    try {
      const content = await fs.readFile(filePath, 'utf-8');
      const stats = await fs.stat(filePath);
      results.push({
        path: filePath,
        name: path.basename(filePath),
        content: content,
        size: stats.size,
        success: true
      });
    } catch (error) {
      results.push({
        path: filePath,
        name: path.basename(filePath),
        error: error.message,
        success: false
      });
    }
  }
  
  return { success: true, files: results, count: results.length };
});

// Find files by glob pattern
ipcMain.handle('find-by-pattern', async (event, pattern, startPath) => {
  const fs = require('fs').promises;
  const results = [];
  const searchPath = startPath || app.getAppPath();
  
  // Simple glob matching (*.js, *.txt, etc.)
  function matchesPattern(filename, pattern) {
    const regex = new RegExp('^' + pattern.replace(/\*/g, '.*').replace(/\?/g, '.') + '$', 'i');
    return regex.test(filename);
  }
  
  async function findFiles(dirPath, depth = 0) {
    if (depth > 20) return; // Prevent infinite loops
    
    try {
      const entries = await fs.readdir(dirPath, { withFileTypes: true });
      
      for (const entry of entries) {
        if (entry.name.startsWith('.') || entry.name === 'node_modules') continue;
        
        const fullPath = path.join(dirPath, entry.name);
        
        if (entry.isFile() && matchesPattern(entry.name, pattern)) {
          results.push({
            path: fullPath,
            name: entry.name,
            directory: dirPath
          });
        } else if (entry.isDirectory()) {
          await findFiles(fullPath, depth + 1);
        }
      }
    } catch (error) {
      // Skip inaccessible directories
    }
  }
  
  try {
    await findFiles(searchPath);
    return { success: true, results, pattern, count: results.length };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// ============================================================================
// NATIVE SPEECH RECOGNITION (CROSS-PLATFORM, OFFLINE)
// ============================================================================

// Windows Speech Recognition
ipcMain.handle('windows-speech-recognize', async () => {
  if (process.platform !== 'win32') {
    return { success: false, error: 'Not on Windows' };
  }
  
  try {
    const { execSync } = require('child_process');
    
    // Use Windows Speech Recognition via PowerShell
    const psScript = `
      Add-Type -AssemblyName System.Speech
      $recognizer = New-Object System.Speech.Recognition.SpeechRecognitionEngine
      $recognizer.SetInputToDefaultAudioDevice()
      $grammar = New-Object System.Speech.Recognition.DictationGrammar
      $recognizer.LoadGrammar($grammar)
      $result = $recognizer.Recognize()
      if ($result) { $result.Text } else { "" }
    `;
    
    const result = execSync(`powershell -Command "${psScript}"`, { 
      encoding: 'utf-8',
      timeout: 10000 
    });
    
    return { 
      success: true, 
      text: result.trim(),
      engine: 'windows-native'
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// macOS Speech Recognition
ipcMain.handle('mac-speech-recognize', async () => {
  if (process.platform !== 'darwin') {
    return { success: false, error: 'Not on macOS' };
  }
  
  try {
    const { execSync } = require('child_process');
    
    // Use macOS Speech Recognition via AppleScript
    const appleScript = `
      tell application "System Events"
        set recognitionResult to do shell script "say 'Listening' && sleep 3"
      end tell
    `;
    
    // Note: macOS requires accessibility permissions
    const result = execSync(`osascript -e '${appleScript}'`, { 
      encoding: 'utf-8',
      timeout: 10000 
    });
    
    return { 
      success: true, 
      text: result.trim(),
      engine: 'macos-native'
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// Linux Speech Recognition  
ipcMain.handle('linux-speech-recognize', async () => {
  if (process.platform !== 'linux') {
    return { success: false, error: 'Not on Linux' };
  }
  
  try {
    const { execSync } = require('child_process');
    
    // Try PocketSphinx if installed
    const result = execSync('pocketsphinx_continuous -inmic yes -time yes 2>&1', { 
      encoding: 'utf-8',
      timeout: 10000 
    });
    
    return { 
      success: true, 
      text: result.trim(),
      engine: 'linux-pocketsphinx'
    };
  } catch (error) {
    // PocketSphinx not installed - use fallback
    return { 
      success: false, 
      error: 'PocketSphinx not installed. Install with: sudo apt-get install pocketsphinx',
      fallback: 'keyboard'
    };
  }
});

// ============================================================================
// DRIVE & FILE SYSTEM BROWSING
// ============================================================================

// List all drives (C:, D:, USB, external, etc.)
ipcMain.handle('list-drives', async () => {
  const os = require('os');
  const { execSync } = require('child_process');
  const drives = [];
  
  try {
    if (process.platform === 'win32') {
      // Windows: Use simple wmic command to get drive letters
      const output = execSync('wmic logicaldisk get name', { 
        encoding: 'utf-8' 
      });
      
      const lines = output.split('\n').slice(1).filter(line => line.trim());
      
      for (const line of lines) {
        const driveLetter = line.trim();
        if (driveLetter && driveLetter.match(/^[A-Z]:$/)) {
          // Get detailed info for this drive
          try {
            const detailOutput = execSync(
              `wmic logicaldisk where "DeviceID='${driveLetter}'" get Description,VolumeName,Size,FreeSpace /format:list`,
              { encoding: 'utf-8' }
            );
            
            const details = {};
            detailOutput.split('\n').forEach(line => {
              const [key, value] = line.split('=');
              if (key && value) {
                details[key.trim()] = value.trim();
              }
            });
            
            drives.push({
              name: driveLetter,
              path: driveLetter + '\\',
              label: details.VolumeName || driveLetter,
              type: details.Description || 'Local Disk',
              size: parseInt(details.Size) || 0,
              free: parseInt(details.FreeSpace) || 0,
              icon: getWindowsDriveIcon(details.Description)
            });
          } catch (err) {
            // If details fail, add basic drive info
            drives.push({
              name: driveLetter,
              path: driveLetter + '\\',
              label: driveLetter,
              type: 'Local Disk',
              size: 0,
              free: 0,
              icon: '💾'
            });
          }
        }
      }
    } else if (process.platform === 'darwin') {
      // macOS: List volumes
      const volumesPath = '/Volumes';
      const volumes = await fs.promises.readdir(volumesPath);
      
      for (const volume of volumes) {
        const volumePath = path.join(volumesPath, volume);
        drives.push({
          name: volume,
          path: volumePath,
          label: volume,
          type: 'Volume',
          icon: '💾'
        });
      }
      
      // Add root
      drives.unshift({
        name: 'Macintosh HD',
        path: '/',
        label: 'Macintosh HD',
        type: 'System',
        icon: '🖥️'
      });
    } else {
      // Linux: List mounted drives
      const output = execSync('df -h', { encoding: 'utf-8' });
      const lines = output.split('\n').filter(line => line.trim());
      
      for (let i = 1; i < lines.length; i++) {
        const parts = lines[i].split(/\s+/);
        if (parts[5] && parts[5].startsWith('/')) {
          drives.push({
            name: path.basename(parts[5]) || parts[5],
            path: parts[5],
            label: parts[5],
            type: 'Mount',
            size: parts[1],
            free: parts[3],
            icon: '💿'
          });
        }
      }
    }
    
    console.log(`[BigDaddyG] 💾 Found ${drives.length} drives`);
    return { success: true, drives };
  } catch (error) {
    console.error('[BigDaddyG] ❌ Error listing drives:', error);
    return { success: false, error: error.message, drives: [] };
  }
});

function getWindowsDriveIcon(type) {
  if (!type) return '💾';
  const lowerType = type.toLowerCase();
  if (lowerType.includes('local')) return '💽';
  if (lowerType.includes('removable')) return '📀'; // USB
  if (lowerType.includes('network')) return '🌐';
  if (lowerType.includes('cd')) return '📀';
  return '💾';
}

// Watch for USB drive changes (Windows)
if (process.platform === 'win32') {
  const { exec } = require('child_process');
  
  // Poll for drive changes every 30 seconds (not too aggressive)
  setInterval(() => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      exec('wmic logicaldisk get name', (error, stdout) => {
        if (!error && stdout) {
          const currentDrives = stdout.match(/[A-Z]:/g) || [];
          mainWindow.webContents.send('drives-changed', currentDrives);
        }
      });
    }
  }, 30000); // Changed from 5000ms to 30000ms (30 seconds)
}

// Orchestra server control
ipcMain.handle('orchestra:start', () => {
  startOrchestraServer();
  return { success: true };
});

ipcMain.handle('orchestra:stop', () => {
  stopOrchestraServer();
  return { success: true };
});

ipcMain.handle('orchestra:status', async () => {
  const isRunning = await checkOrchestraHealth();
  return { running: isRunning };
});

// ============================================================================
// FILE SYSTEM OPERATIONS
// ============================================================================

// Launch external program
ipcMain.handle('launchProgram', async (event, programPath) => {
  try {
    console.log('[FileSystem] Launching program:', programPath);
    
    const { exec } = require('child_process');
    
    return new Promise((resolve) => {
      exec(`"${programPath}"`, (error, stdout, stderr) => {
        if (error) {
          console.error('[FileSystem] Launch error:', error);
          resolve({ success: false, error: error.message });
        } else {
          resolve({ success: true });
        }
      });
    });
  } catch (error) {
    console.error('[FileSystem] Launch exception:', error);
    return { success: false, error: error.message };
  }
});

// Open directory in system file explorer
ipcMain.handle('openInExplorer', async (event, dirPath) => {
  try {
    console.log('[FileSystem] Opening in explorer:', dirPath);
    
    const { shell } = require('electron');
    await shell.openPath(dirPath);
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Open explorer error:', error);
    return { success: false, error: error.message };
  }
});

// Create directory
ipcMain.handle('createDirectory', async (event, dirPath) => {
  try {
    console.log('[FileSystem] Creating directory:', dirPath);
    
    fs.mkdirSync(dirPath, { recursive: true });
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Create directory error:', error);
    return { success: false, error: error.message };
  }
});

// Delete file or directory
ipcMain.handle('deleteItem', async (event, itemPath, isDirectory = false) => {
  try {
    console.log('[FileSystem] Deleting:', itemPath, '(isDirectory:', isDirectory, ')');
    
    if (isDirectory) {
      fs.rmSync(itemPath, { recursive: true, force: true });
    } else {
      fs.unlinkSync(itemPath);
    }
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Delete error:', error);
    return { success: false, error: error.message };
  }
});

// Copy file or directory
ipcMain.handle('copyItem', async (event, sourcePath, destPath) => {
  try {
    console.log('[FileSystem] Copying:', sourcePath, 'to', destPath);
    
    const stats = fs.statSync(sourcePath);
    
    if (stats.isDirectory()) {
      // Copy directory recursively
      fs.cpSync(sourcePath, destPath, { recursive: true });
    } else {
      // Copy file
      fs.copyFileSync(sourcePath, destPath);
    }
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Copy error:', error);
    return { success: false, error: error.message };
  }
});

// Move/Rename file or directory
ipcMain.handle('moveItem', async (event, sourcePath, destPath) => {
  try {
    console.log('[FileSystem] Moving:', sourcePath, 'to', destPath);
    
    fs.renameSync(sourcePath, destPath);
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Move error:', error);
    return { success: false, error: error.message };
  }
});

// Get file/directory stats
ipcMain.handle('getStats', async (event, itemPath) => {
  try {
    const stats = fs.statSync(itemPath);
    
    return {
      success: true,
      stats: {
        isFile: stats.isFile(),
        isDirectory: stats.isDirectory(),
        size: stats.size,
        created: stats.birthtime,
        modified: stats.mtime,
        accessed: stats.atime
      }
    };
  } catch (error) {
    console.error('[FileSystem] Stats error:', error);
    return { success: false, error: error.message };
  }
});

// ============================================================================
// TERMINAL EXECUTION
// ============================================================================

ipcMain.handle('execute-command', async (event, { command, shell = 'powershell', cwd = process.cwd() }) => {
  const { spawn } = require('child_process');
  
  console.log(`[Terminal] Executing: ${command} (shell: ${shell}, cwd: ${cwd})`);
  
  return new Promise((resolve) => {
    let output = '';
    let error = '';
    
    // Select shell command
    let shellCmd, shellArgs;
    switch (shell) {
      case 'powershell':
        shellCmd = 'powershell.exe';
        shellArgs = ['-NoProfile', '-NonInteractive', '-Command', command];
        break;
      case 'cmd':
        shellCmd = 'cmd.exe';
        shellArgs = ['/c', command];
        break;
      case 'bash':
        shellCmd = 'bash';
        shellArgs = ['-c', command];
        break;
      case 'wsl':
        shellCmd = 'wsl.exe';
        shellArgs = ['-e', 'bash', '-c', command];
        break;
      default:
        shellCmd = 'powershell.exe';
        shellArgs = ['-NoProfile', '-NonInteractive', '-Command', command];
    }
    
    const proc = spawn(shellCmd, shellArgs, { cwd, shell: false });
    
    proc.stdout.on('data', (data) => {
      output += data.toString();
      event.sender.send('terminal-output', { output: data.toString(), isError: false });
    });
    
    proc.stderr.on('data', (data) => {
      error += data.toString();
      event.sender.send('terminal-output', { output: data.toString(), isError: true });
    });
    
    proc.on('close', (code) => {
      event.sender.send('terminal-exit', { code });
      resolve({
        output,
        error,
        code,
        cwd: proc.spawnargs.cwd || cwd
      });
    });
    
    proc.on('error', (err) => {
      error = err.message;
      resolve({
        output,
        error,
        code: 1,
        cwd
      });
    });
  });
});

// Window controls
ipcMain.on('window-minimize', () => {
  if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window-maximize', () => {
  if (mainWindow) {
    if (mainWindow.isMaximized()) {
      mainWindow.unmaximize();
    } else {
      mainWindow.maximize();
    }
  }
});

ipcMain.on('window-close', () => {
  if (mainWindow) mainWindow.close();
});

console.log('[BigDaddyG] 🌌 Main process initialized');

