/**
 * BigDaddyG IDE - Electron Main Process
 * Professional desktop IDE with dedicated tabs and syntax highlighting
 */

const electronModule = require('electron');
const { app, BrowserWindow, ipcMain, Menu, dialog } = electronModule;

if (!app || typeof app.whenReady !== 'function') {
  console.error('[BigDaddyG] ❌ Electron "app" module unavailable.');
  console.error('[BigDaddyG]    • ELECTRON_RUN_AS_NODE =', process.env.ELECTRON_RUN_AS_NODE || 'undefined');
  console.error('[BigDaddyG]    • process.type =', process.type || 'undefined');
  console.error('[BigDaddyG] 🚫 This file must be launched with the Electron runtime (e.g. `npm start`).');
  process.exit(1);
}
const path = require('path');
const fs = require('fs');
const http = require('http');
const { spawn } = require('child_process');
const { validateFsPath } = require('./security/path-utils');
const windowStateKeeper = require('electron-window-state');
const { EmbeddedBrowser } = require('./browser-view');
const SafeModeDetector = require('./safe-mode-detector');
const memoryService = require('./memory-service');
const nativeOllamaClient = require('./native-ollama-node');
const { getBigDaddyGCore } = require('./bigdaddyg-agentic-core');
const { createExtensionHost, ExtensionBridge } = require('./extension-host/extension-host');
const MarketplaceClient = require('./marketplace/marketplace-client');
const ExtensionManager = require('./marketplace/extension-manager');
const SettingsImporter = require('./settings/settings-importer');
const ApiKeyStore = require('./settings/api-key-store');
const settingsService = require('./settings/settings-service');

let mainWindow;
let orchestraServer = null;
let remoteLogServer = null;
let embeddedBrowser;
let safeModeDetector = new SafeModeDetector();
let marketplaceClient = null;
let extensionHostInstance = null;
let extensionBridge = null;
let extensionManager = null;
let settingsImporter = null;
let marketplaceReady = false;
let marketplaceInitPromise = null;
let apiKeyStore = null;
let modelInterfaceInstance = null;
let bigDaddyGCore = null;

if (!global.modelDiscoveryCache) {
  global.modelDiscoveryCache = null;
}

const FEATURED_MARKETPLACE_PATH = path.join(__dirname, 'marketplace', 'featured.json');

const MARKETPLACE_EVENT_CHANNEL = 'marketplace:event';

function broadcastMarketplaceEvent(event) {
  const windows = BrowserWindow.getAllWindows();
  windows.forEach((win) => {
    if (win && !win.isDestroyed()) {
      win.webContents.send(MARKETPLACE_EVENT_CHANNEL, event);
    }
  });
}

function parseExtensionId(extensionId) {
  if (typeof extensionId !== 'string') {
    throw new Error('Extension ID must be a string');
  }
  const parts = extensionId.split('.');
  if (parts.length < 2) {
    throw new Error(`Invalid extension ID: ${extensionId}`);
  }
  const publisher = parts.shift();
  const name = parts.join('.');
  if (!publisher || !name) {
    throw new Error(`Invalid extension ID: ${extensionId}`);
  }
  return { publisher, name };
}

function getPluginDirectory() {
  const dir = path.join(app.getPath('userData'), 'plugins');
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  return dir;
}

async function initializeExtensionEcosystem() {
  if (marketplaceInitPromise) {
    return marketplaceInitPromise;
  }

  marketplaceInitPromise = (async () => {
    try {
      if (!mainWindow) {
        throw new Error('Main window not initialized');
      }

      console.log('[Marketplace] 🚀 Initializing extension marketplace...');

      marketplaceClient = new MarketplaceClient();
      await marketplaceClient.initialize();

      extensionHostInstance = createExtensionHost(mainWindow, null);
      await extensionHostInstance.initialize();

      extensionBridge = new ExtensionBridge(extensionHostInstance);

      extensionManager = new ExtensionManager(marketplaceClient, extensionHostInstance);
      await extensionManager.initialize();

      settingsImporter = new SettingsImporter(marketplaceClient, extensionHostInstance);

      extensionManager.on('extension-installed', (payload) => {
        broadcastMarketplaceEvent({ type: 'installed', payload });
      });

      extensionManager.on('extension-uninstalled', (payload) => {
        broadcastMarketplaceEvent({ type: 'uninstalled', payload });
      });

      extensionManager.on('extension-enabled', (payload) => {
        broadcastMarketplaceEvent({ type: 'enabled', payload });
      });

      extensionManager.on('extension-disabled', (payload) => {
        broadcastMarketplaceEvent({ type: 'disabled', payload });
      });

      extensionManager.on('extension-updated', (payload) => {
        broadcastMarketplaceEvent({ type: 'updated', payload });
      });

      marketplaceReady = true;
      console.log('[Marketplace] ✅ Extension marketplace ready');
    } catch (error) {
      marketplaceReady = false;
      marketplaceInitPromise = null;
      console.error('[Marketplace] ❌ Failed to initialize:', error);
      throw error;
    }
  })();

  return marketplaceInitPromise;
}

async function ensureMarketplaceReady() {
  if (!marketplaceReady) {
    await initializeExtensionEcosystem();
  }
  return marketplaceReady;
}

async function getInstalledExtensionMetadata(extensionId) {
  if (!marketplaceClient) {
    return null;
  }
  const installed = await marketplaceClient.listInstalledExtensions();
  return installed.find((ext) => ext.id === extensionId) || null;
}

function getExtensionState(extensionId) {
  if (!extensionManager) {
    return null;
  }
  const state = extensionManager.getExtensionState(extensionId);
  return state ? { ...state } : null;
}

class ModelInterface {
  constructor() {
    this.initialized = false;
  }

  async initialize() {
    try {
      const health = await checkOrchestraHealth();
      this.initialized = health;
      return health;
    } catch (error) {
      console.error('[ModelInterface] Initialization failed:', error);
      return false;
    }
  }

  async listModels() {
    if (!this.initialized) {
      await this.initialize();
    }
    try {
      const response = await fetch('http://localhost:11441/api/models');
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      const data = await response.json();
      return data.models || [];
    } catch (error) {
      console.warn('[ModelInterface] Failed to list models:', error.message);
      return [];
    }
  }
}

async function getModelInterface() {
  if (!modelInterfaceInstance) {
    modelInterfaceInstance = new ModelInterface();
  }
  const ready = await modelInterfaceInstance.initialize();
  if (!ready) {
    return null;
  }
  return modelInterfaceInstance;
}

async function listOrchestraModels() {
  try {
    const instance = await getModelInterface();
    if (!instance) {
      return { available: false, models: [] };
    }
    const models = await instance.listModels();
    return { available: true, models };
  } catch (error) {
    console.warn('[Orchestra] Failed to list models:', error.message || error);
    return { available: false, models: [], error: error.message };
  }
}

// ============================================================================
// GPU & PERFORMANCE OPTIMIZATIONS
// ============================================================================

// Apply performance optimizations BEFORE app.whenReady()
try {
  const maxMemory = Math.min(8192, Math.max(2048, require('os').totalmem() / (1024 * 1024 * 4)));
  app.commandLine.appendSwitch('disable-gpu-vsync');
  app.commandLine.appendSwitch('disable-frame-rate-limit');
  app.commandLine.appendSwitch('max-gum-fps', '240');
  app.commandLine.appendSwitch('disable-gpu');
  app.commandLine.appendSwitch('disable-software-rasterizer', 'false');
  app.commandLine.appendSwitch('js-flags', `--max-old-space-size=${Math.floor(maxMemory)}`);
  console.log('[BigDaddyG] ✅ Performance optimizations applied');
} catch (error) {
  console.error('[BigDaddyG] ❌ Performance optimization error:', error.message);
}

console.log('[BigDaddyG] ⚡ Performance optimizations ready');
console.log('[BigDaddyG] 🎯 Target: 240 FPS');

// ============================================================================
// APPLICATION LIFECYCLE
// ============================================================================

app.whenReady().then(async () => {
  console.log('[BigDaddyG] 🚀 Starting Electron app...');

  settingsService.initialize(app);

  // Initialize BigDaddyG Core
  bigDaddyGCore = await getBigDaddyGCore();

  bigDaddyGCore.attachNativeClient(nativeOllamaClient);
  
  // Start Orchestra server
  startOrchestraServer();
  
  // Start Remote Log Server
  startRemoteLogServer();
  
  // Create main window
  createMainWindow();
  
  // Set up application menu
  createMenu();

  try {
    await ensureApiKeyStore();
  } catch (error) {
    console.error('[BigDaddyG] ❌ Failed to initialize API key store:', error);
  }

  initializeExtensionEcosystem().catch((error) => {
    console.error('[Marketplace] Startup initialization failed:', error);
  });
  
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
    const orchestraModule = require(serverPath);

    if (orchestraModule && typeof orchestraModule.start === 'function') {
      const instance = orchestraModule.start();
      if (instance && typeof instance.on === 'function') {
        orchestraServer = instance;
      }
    } else if (orchestraModule && orchestraModule.server) {
      orchestraServer = orchestraModule.server;
    } else {
      orchestraServer = null;
    }

    if (orchestraServer && typeof orchestraServer.on === 'function') {
      orchestraServer.on('error', (error) => {
        console.error('[Orchestra] Server error:', error);
      });
    }

    console.log('[BigDaddyG] ✅ Orchestra server loaded and running!');
    if (mainWindow) {
      mainWindow.webContents.send('orchestra-status', {
        running: true,
        alreadyRunning: false,
      });
    }
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
  
  // Event handlers only apply if using spawn (which we don't)
  if (orchestraServer) {
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
    
    console.log('[BigDaddyG] ✅ Orchestra server process started');
  }
  */
  
  // Notify renderer that orchestra loaded directly (not spawned)
  if (mainWindow) {
    setTimeout(() => {
      mainWindow.webContents.send('orchestra-status', {
        running: true,
        loadedDirectly: true
      });
    }, 1000);
  }
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
// MARKETPLACE & EXTENSION HELPERS
// ============================================================================

async function loadFeaturedExtensionIds() {
    try {
        const content = await fs.promises.readFile(FEATURED_MARKETPLACE_PATH, 'utf8');
        const data = JSON.parse(content);
        if (Array.isArray(data.featured)) {
            return data.featured;
        }
    } catch (error) {
        console.warn('[Marketplace] ⚠️ Could not load featured catalog:', error.message);
    }
    return [];
}

async function fetchFeaturedExtensions() {
    const ids = await loadFeaturedExtensionIds();
    const extensions = [];

    for (const id of ids) {
        const [publisher, name] = (id || '').split('.');
        if (!publisher || !name) {
            continue;
        }

        try {
            const ext = await marketplaceClient.getExtension(publisher, name);
            extensions.push(ext);
        } catch (error) {
            console.warn(`[Marketplace] ⚠️ Failed to fetch featured extension ${id}:`, error.message);
        }
    }

    return extensions;
}

async function ensureApiKeyStore() {
    if (!apiKeyStore) {
        apiKeyStore = new ApiKeyStore(app);
        await apiKeyStore.initialize();
    }
    return apiKeyStore;
}

function maskKeyValue(value) {
    if (!value || typeof value !== 'string') {
        return '';
    }

    if (value.length <= 4) {
        return '*'.repeat(value.length);
    }

    return `${value.slice(0, 4)}...${value.slice(-4)}`;
}

// ============================================================================
// OLLAMA HELPERS
// ============================================================================

async function listOllamaModels() {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 4000);

    try {
        const response = await fetch('http://localhost:11434/api/tags', { signal: controller.signal });
        clearTimeout(timeout);

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        return data.models || [];
    } catch (error) {
        clearTimeout(timeout);
        throw error;
    }
}

async function getOllamaStatus() {
    try {
        await listOllamaModels();
        return { available: true };
    } catch (error) {
        return { available: false, error: error.message };
    }
}

function sanitizeModelIdentifier(model) {
    if (typeof model !== 'string') {
        throw new Error('Model name must be a string');
    }

    const trimmed = model.trim();
    if (!trimmed) {
        throw new Error('Model name is required');
    }

    if (trimmed.length > 128) {
        throw new Error('Model name too long');
    }

    const pattern = /^[A-Za-z0-9._:@\/-]+$/;
    if (!pattern.test(trimmed)) {
        throw new Error('Model name contains invalid characters');
    }

    return trimmed;
}

function sanitizeCliArgument(arg) {
    if (typeof arg !== 'string') {
        throw new Error('CLI argument must be a string');
    }

    const trimmed = arg.trim();
    if (!trimmed) {
        throw new Error('CLI argument cannot be empty');
    }

    if (trimmed.length > 200) {
        throw new Error('CLI argument too long');
    }

    const pattern = /^[A-Za-z0-9._:@\/=-]+$/;
    if (!pattern.test(trimmed)) {
        throw new Error(`CLI argument contains invalid characters: ${arg}`);
    }

    return trimmed;
}

function sanitizeCliArguments(args) {
    if (!Array.isArray(args)) {
        throw new Error('CLI arguments must be an array');
    }

    return args.map(sanitizeCliArgument);
}

function runOllamaCommand(args) {
    return new Promise((resolve) => {
        try {
            const sanitizedArgs = sanitizeCliArguments(args);
            const binary = process.platform === 'win32' ? 'ollama.exe' : 'ollama';

            const proc = spawn(binary, sanitizedArgs, {
                shell: false,
                stdio: ['ignore', 'pipe', 'pipe']
            });

            let stdout = '';
            let stderr = '';

            const timeoutMs = 60000;
            const timeoutHandle = setTimeout(() => {
                if (!proc.killed) {
                    stderr += `\n[Ollama] Command timed out after ${timeoutMs}ms`;
                    proc.kill('SIGKILL');
                }
            }, timeoutMs);

            proc.stdout.on('data', (data) => {
                stdout += data.toString();
            });

            proc.stderr.on('data', (data) => {
                stderr += data.toString();
            });

            proc.on('close', (code) => {
                clearTimeout(timeoutHandle);
                if (code === 0) {
                    resolve({ success: true, output: stdout.trim(), error: stderr.trim() });
                } else {
                    const message = (stderr || stdout || `Exit code ${code}`).trim();
                    resolve({ success: false, error: message });
                }
            });

            proc.on('error', (error) => {
                clearTimeout(timeoutHandle);
                resolve({ success: false, error: error.message });
            });
        } catch (error) {
            resolve({ success: false, error: error.message });
        }
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
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      webviewTag: false,
      preload: path.join(__dirname, 'preload.js'),
      webSecurity: true,
      allowRunningInsecureContent: false,
      enableRemoteModule: false,
      worldSafeExecuteJavaScript: true,
      experimentalFeatures: false
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
  
  // Open DevTools only in development
  if (process.env.NODE_ENV === 'development') {
    mainWindow.webContents.openDevTools({ mode: 'detach' });
  }
  
  // Log any page load errors
  mainWindow.webContents.on('did-fail-load', (event, errorCode, errorDescription) => {
    console.error('[BigDaddyG] ❌ Page failed to load:', errorCode, errorDescription);
  });
  
  mainWindow.webContents.on('crashed', () => {
    console.error('[BigDaddyG] ❌ Renderer process crashed!');
    
    // Auto-recovery: Show dialog with options
    const { dialog } = require('electron');
    dialog.showMessageBox(mainWindow, {
      type: 'error',
      title: 'IDE Crashed',
      message: 'The IDE renderer process has crashed.',
      detail: 'This might be due to:\n• Out of memory\n• GPU driver issue\n• Corrupted file\n\nWould you like to reload?',
      buttons: ['Reload', 'Quit'],
      defaultId: 0
    }).then((result) => {
      if (result.response === 0) {
        // Reload
        console.log('[BigDaddyG] 🔄 Reloading after crash...');
        mainWindow.reload();
      } else {
        // Quit
        app.quit();
      }
    });
  });
  
  mainWindow.webContents.on('did-finish-load', () => {
    console.log('[BigDaddyG] ✅ Page loaded successfully');

    try {
      const snapshot = settingsService.getAll();
      mainWindow.webContents.send('settings:bootstrap', snapshot);
    } catch (error) {
      console.error('[SettingsService] ❌ Failed to send bootstrap settings:', error);
    }
    
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
            // Also try to show embedded browser if available
            if (embeddedBrowser) {
              embeddedBrowser.show();
            }
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

ipcMain.handle('plugin:get-directory', async () => {
  try {
    return getPluginDirectory();
  } catch (error) {
    console.error('[PluginSystem] Failed to resolve plugin directory:', error);
    throw error;
  }
});

ipcMain.handle('marketplace:status', async () => {
  try {
    await ensureMarketplaceReady();
    if (!marketplaceReady || !marketplaceClient) {
      throw new Error('Marketplace not initialized');
    }

    const installed = await marketplaceClient.listInstalledExtensions();
    const states = installed.reduce((acc, ext) => {
      acc[ext.id] = getExtensionState(ext.id);
      return acc;
    }, {});

    return {
      success: true,
      ready: marketplaceReady,
      installed,
      states,
      autoActivate: extensionManager ? Array.from(extensionManager.autoActivate || []) : []
    };
  } catch (error) {
    console.error('[Marketplace] Status error:', error);
    return {
      success: false,
      ready: marketplaceReady,
      error: error.message
    };
  }
});

ipcMain.handle('marketplace:search', async (event, options = {}) => {
  try {
    await ensureMarketplaceReady();
    if (!marketplaceReady || !marketplaceClient) {
      throw new Error('Marketplace not initialized');
    }

    const {
      query = '',
      pageNumber = 1,
      pageSize = 30
    } = options;

    const results = await marketplaceClient.searchExtensions(query, pageNumber, pageSize);
    return { success: true, results };
  } catch (error) {
    console.error('[Marketplace] Search error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:featured', async () => {
  try {
    await ensureMarketplaceReady();
    if (!marketplaceClient) {
      throw new Error('Marketplace not initialized');
    }

    const extensions = await fetchFeaturedExtensions();
    const installed = extensionManager ? await extensionManager.listExtensionsWithStates() : [];

    return {
      success: true,
      extensions,
      installed
    };
  } catch (error) {
    console.error('[Marketplace] Featured error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:get-extension', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!marketplaceClient) {
      throw new Error('Marketplace not initialized');
    }

    const { publisher, name } = parseExtensionId(extensionId);
    const extension = await marketplaceClient.getExtension(publisher, name);
    const installed = await getInstalledExtensionMetadata(extensionId);
    const state = getExtensionState(extensionId);

    return { success: true, extension, installed, state };
  } catch (error) {
    console.error('[Marketplace] Get extension error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:list-installed', async () => {
  try {
    await ensureMarketplaceReady();
    if (!marketplaceClient) {
      throw new Error('Marketplace not initialized');
    }

    const installed = await marketplaceClient.listInstalledExtensions();
    return { success: true, installed };
  } catch (error) {
    console.error('[Marketplace] List installed error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:list-with-state', async () => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    const extensions = await extensionManager.listExtensionsWithStates();
    return { success: true, extensions };
  } catch (error) {
    console.error('[Marketplace] List with state error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:install', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    await extensionManager.installExtension(extensionId);
    const installed = await getInstalledExtensionMetadata(extensionId);
    const state = getExtensionState(extensionId);

    return { success: true, installed, state };
  } catch (error) {
    console.error('[Marketplace] Install error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:uninstall', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    await extensionManager.uninstallExtension(extensionId);
    return { success: true };
  } catch (error) {
    console.error('[Marketplace] Uninstall error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:enable', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    await extensionManager.enableExtension(extensionId);
    return { success: true, state: getExtensionState(extensionId) };
  } catch (error) {
    console.error('[Marketplace] Enable error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:disable', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    await extensionManager.disableExtension(extensionId);
    return { success: true, state: getExtensionState(extensionId) };
  } catch (error) {
    console.error('[Marketplace] Disable error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:check-updates', async () => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    const updates = await extensionManager.checkForUpdates();
    return { success: true, updates };
  } catch (error) {
    console.error('[Marketplace] Check updates error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:update', async (event, extensionId) => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    await extensionManager.updateExtension(extensionId);
    const installed = await getInstalledExtensionMetadata(extensionId);
    const state = getExtensionState(extensionId);

    return { success: true, installed, state };
  } catch (error) {
    console.error('[Marketplace] Update error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('marketplace:update-all', async () => {
  try {
    await ensureMarketplaceReady();
    if (!extensionManager) {
      throw new Error('Extension manager not initialized');
    }

    const results = await extensionManager.updateAllExtensions();
    return { success: true, results };
  } catch (error) {
    console.error('[Marketplace] Update all error:', error);
    return { success: false, error: error.message };
  }
});

// Path validation function
function getAllowedBasePaths() {
  const bases = new Set([path.resolve(process.cwd())]);

  const appReady = app && typeof app.isReady === 'function' ? app.isReady() : true;

  if (!appReady) {
    return Array.from(bases);
  }

  ['userData', 'documents', 'desktop', 'downloads'].forEach((key) => {
    try {
      const candidate = app.getPath(key);
      if (candidate) {
        bases.add(path.resolve(candidate));
      }
    } catch (error) {
      console.warn(`[Security] Unable to resolve app path for '${key}':`, error.message);
    }
  });

  return Array.from(bases);
}

function validateFilePath(filePath, { mustExist = true, label = 'File path' } = {}) {
  return validateFsPath(filePath, {
    allowedBasePaths: getAllowedBasePaths(),
    allowFiles: true,
    allowDirectories: false,
    mustExist,
    label,
  });
}

function validateDirectoryPath(dirPath, { mustExist = true, label = 'Directory path' } = {}) {
  return validateFsPath(dirPath, {
    allowedBasePaths: getAllowedBasePaths(),
    allowFiles: false,
    allowDirectories: true,
    mustExist,
    label,
  });
}

function validateFileSystemPath(targetPath, { mustExist = true, label = 'Path' } = {}) {
  return validateFsPath(targetPath, {
    allowedBasePaths: getAllowedBasePaths(),
    allowFiles: true,
    allowDirectories: true,
    mustExist,
    label,
  });
}

ipcMain.handle('read-file', async (event, filePath) => {
  const fs = require('fs').promises;
  try {
    const validatedPath = validateFilePath(filePath, { mustExist: true });
    const content = await fs.readFile(validatedPath, 'utf-8');
    return { success: true, content };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('write-file', async (event, filePath, content) => {
  const fs = require('fs').promises;
  try {
    const validatedPath = validateFilePath(filePath, { mustExist: false });
    
    // Additional validation for content
    if (typeof content !== 'string') {
      throw new Error('Content must be a string');
    }
    
    // Limit file size to prevent abuse
    if (content.length > 10 * 1024 * 1024) { // 10MB limit
      throw new Error('File content too large (max 10MB)');
    }
    
    await fs.writeFile(validatedPath, content, 'utf-8');
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
    const validatedDir = validateDirectoryPath(dirPath);
    const entries = await fs.readdir(validatedDir, { withFileTypes: true });
    const files = entries.map(entry => ({
      name: entry.name,
      isDirectory: entry.isDirectory(),
      isFile: entry.isFile(),
      path: path.join(validatedDir, entry.name)
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
    const validatedPath = validateFileSystemPath(filePath, { label: 'File path' });
    const stats = await fs.stat(validatedPath);
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
// ERROR LOGGING TO FILE
// ============================================================================

ipcMain.handle('get-log-file-path', async () => {
  const logsDir = path.join(app.getPath('userData'), 'logs');
  
  // Create logs directory if it doesn't exist
  try {
    await fs.promises.mkdir(logsDir, { recursive: true });
  } catch (err) {
    console.error('[Main] Failed to create logs directory:', err);
  }
  
  const timestamp = new Date().toISOString().replace(/:/g, '-').split('.')[0];
  return path.join(logsDir, `bigdaddyg-session-${timestamp}.log`);
});

ipcMain.handle('write-log-file', async (event, filePath, content) => {
  try {
    // Append to log file
    await fs.promises.appendFile(filePath, content, 'utf8');
    return { success: true };
  } catch (error) {
    console.error('[Main] Failed to write log file:', error);
    return { success: false, error: error.message };
  }
});

// ============================================================================
// AGENTIC FILE SYSTEM OPERATIONS (UNLIMITED)
// ============================================================================

// Search files by content or name
ipcMain.handle('search-files', async (event, query, options = {}) => {
  const fs = require('fs').promises;
  const results = [];
  
  // Validate search parameters
  if (!query || typeof query !== 'string' || query.length < 2) {
    return { success: false, error: 'Query must be at least 2 characters' };
  }
  
  if (query.length > 100) {
    return { success: false, error: 'Query too long (max 100 characters)' };
  }
  
  let searchPath;
  try {
    searchPath = options.path ? validateFilePath(options.path) : app.getAppPath();
  } catch (error) {
    return { success: false, error: `Invalid search path: ${error.message}` };
  }
  
  const maxResults = Math.min(options.maxResults || 100, 500); // Limit to 500 results
  const maxDepth = Math.min(options.maxDepth || 5, 10); // Limit depth to 10
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

// Scan workspace for file search (used by command palette)
ipcMain.handle('scanWorkspace', async (event, options = {}) => {
  const fs = require('fs').promises;
  const workspacePath = options.path || process.cwd();
  const maxFiles = options.maxFiles || 500;
  const maxDepth = options.maxDepth || 5;
  const files = [];
  
  async function scan(dirPath, depth = 0) {
    if (depth > maxDepth || files.length >= maxFiles) return;
    
    try {
      const entries = await fs.readdir(dirPath, { withFileTypes: true });
      
      for (const entry of entries) {
        if (files.length >= maxFiles) break;
        
        // Skip common excluded directories
        const skipDirs = ['node_modules', '.git', 'dist', 'build', '.next', '.cache', '__pycache__', 'vendor'];
        if (skipDirs.includes(entry.name) || entry.name.startsWith('.')) continue;
        
        const fullPath = path.join(dirPath, entry.name);
        
        files.push({
          name: entry.name,
          path: fullPath,
          isDirectory: entry.isDirectory(),
          isFile: entry.isFile(),
          depth: depth
        });
        
        if (entry.isDirectory() && depth < maxDepth) {
          await scan(fullPath, depth + 1);
        }
      }
    } catch (error) {
      // Skip inaccessible directories silently
    }
  }
  
  try {
    console.log(`[FileSystem] 🔍 Scanning workspace: ${workspacePath}`);
    await scan(workspacePath);
    console.log(`[FileSystem] ✅ Found ${files.length} files`);
    return files;
  } catch (error) {
    console.error('[FileSystem] ❌ Workspace scan error:', error);
    return [];
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
// MEMORY BRIDGE (OpenMemory Integration)
// ============================================================================

ipcMain.handle('memory:getStats', async () => {
  return memoryService.getStats();
});

ipcMain.handle('memory:store', async (event, { content, metadata }) => {
  return memoryService.storeMemory(content, metadata);
});

ipcMain.handle('memory:query', async (event, { query, limit }) => {
  return memoryService.queryMemory(query, limit);
});

ipcMain.handle('memory:recent', async (event, limit) => {
  return memoryService.getRecentMemories(limit);
});

ipcMain.handle('memory:embedding', async (event, { text, model }) => {
  return memoryService.createEmbedding(text, model);
});

ipcMain.handle('memory:similar', async (event, { embedding, threshold, limit }) => {
  return memoryService.similaritySearch(embedding, threshold, limit);
});

ipcMain.handle('memory:decay', async () => {
  return memoryService.applyDecay();
});

ipcMain.handle('memory:clear', async () => {
  return memoryService.clearAllMemories();
});

// ============================================================================
// API KEY MANAGEMENT
// ============================================================================

ipcMain.handle('apikeys:list', async () => {
  try {
    const store = await ensureApiKeyStore();
    const keys = await store.list();

    const response = {};
    Object.entries(keys).forEach(([provider, info]) => {
      response[provider] = {
        ...info,
        masked: maskKeyValue(info?.key)
      };
    });

    return { success: true, keys: response };
  } catch (error) {
    console.error('[API Keys] List error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('apikeys:set', async (event, { provider, key, metadata = {} }) => {
  try {
    if (!provider) {
      throw new Error('Provider is required');
    }

    const store = await ensureApiKeyStore();
    const record = await store.set(provider, key, metadata);

    return {
      success: true,
      provider,
      record: {
        ...record,
        masked: maskKeyValue(record?.key)
      }
    };
  } catch (error) {
    console.error('[API Keys] Set error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('apikeys:delete', async (event, provider) => {
  try {
    await apiKeyStore.deleteKey(provider);
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

// ============================================================================
// MODEL DISCOVERY (ORCHESTRA + OLLAMA)
// ============================================================================

function normalizeModelStatus(model, status = {}) {
  return {
    ...model,
    loaded: Boolean(status.loaded),
    cached: Boolean(status.cached),
    orchestraReady: Boolean(status.loaded || status.cached),
    error: status.error,
  };
}

ipcMain.handle('models:discover', async (event, options = {}) => {
  try {
    console.log('[Models] Discovering BigDaddyG models...');

    const coreInstance = bigDaddyGCore || (await getBigDaddyGCore());
    const allModels = await coreInstance.listModels();

    const bigdaddygModels = allModels.filter(
      (model) => model.type === 'bigdaddyg' || model.isCustom || model.family === 'bigdaddyg'
    );

    const ollamaModels = allModels.filter((model) => model.type === 'ollama' || model.source === 'ollama-api');
    const customModels = allModels.filter((model) => model.isCustom || model.source === 'custom');

    const modelStatuses = await Promise.all(
      bigdaddygModels.map(async (model) => {
        try {
          const status = await nativeOllamaClient.checkModelStatus(model.name);
          return normalizeModelStatus(model, status);
        } catch (error) {
          return normalizeModelStatus(model, { error: error.message });
        }
      })
    );

    if (options.loadPriorityModels) {
      const priorityModels = modelStatuses.filter((m) => m.priority && !m.loaded && m.available !== false);
      await Promise.allSettled(
        priorityModels.map((model) =>
          nativeOllamaClient
            .loadModel(model.name)
            .catch((err) => console.error(`[Models] Failed to load priority model ${model.name}:`, err))
        )
      );
    }

    const discoveryResult = {
      success: true,
      timestamp: new Date().toISOString(),
      orchestra: {
        available: true,
        connected: true,
        version: '1.0.0',
        models: {
          total: bigdaddygModels.length,
          loaded: modelStatuses.filter((m) => m.loaded).length,
          available: modelStatuses.filter((m) => m.available !== false).length,
          list: modelStatuses.map((model) => ({
            name: model.name,
            id: model.id,
            size: model.size,
            type: model.type,
            family: model.family,
            description: model.description,
            loaded: model.loaded,
            cached: model.cached,
            available: model.available,
            lastUsed: model.lastUsed,
            priority: model.priority || false,
            capabilities: model.capabilities || [],
            parameters: model.parameters || {},
            orchestraReady: model.orchestraReady,
            error: model.error,
          })),
        },
      },
      catalog: {
        bigdaddyg: {
          total: bigdaddygModels.length,
          models: bigdaddygModels.map((model) => ({
            name: model.name,
            id: model.id,
            size: model.size,
            type: model.type,
            family: model.family,
            description: model.description,
            loaded: model.loaded,
            available: model.available,
            capabilities: model.capabilities || [],
            parameters: model.parameters || {},
            metadata: model.metadata || {},
          })),
        },
        ollama: {
          total: ollamaModels.length,
          models: ollamaModels,
        },
        custom: {
          total: customModels.length,
          models: customModels,
        },
      },
      statistics: {
        totalModels: allModels.length,
        loadedModels: modelStatuses.filter((m) => m.loaded).length,
        cachedModels: modelStatuses.filter((m) => m.cached).length,
        availableModels: modelStatuses.filter((m) => m.available !== false).length,
        totalSize: allModels.reduce((sum, m) => sum + (m.size || 0), 0),
        averageModelSize:
          allModels.length > 0
            ? allModels.reduce((sum, m) => sum + (m.size || 0), 0) / allModels.length
            : 0,
      },
    };

    global.modelDiscoveryCache = discoveryResult;

    console.log(
      `[Models] Discovery complete: ${bigdaddygModels.length} BigDaddyG models indexed (loaded: ${discoveryResult.orchestra.models.loaded})`
    );

    return discoveryResult;
  } catch (error) {
    console.error('[Models] Model discovery failed:', error);

    return {
      success: false,
      error: error.message,
      stack: process.env.NODE_ENV === 'development' ? error.stack : undefined,
      orchestra: {
        available: false,
        connected: false,
        error: error.message,
      },
      catalog: {
        bigdaddyg: { total: 0, models: [] },
        ollama: { total: 0, models: [] },
        custom: { total: 0, models: [] },
      },
      statistics: {
        totalModels: 0,
        loadedModels: 0,
        cachedModels: 0,
        availableModels: 0,
        totalSize: 0,
        averageModelSize: 0,
      },
    };
  }
});

ipcMain.handle('models:load', async (event, modelName, options = {}) => {
  try {
    console.log(`[Models] Loading model: ${modelName}`);

    const result = await nativeOllamaClient.loadModel(modelName, options);

    if (global.modelDiscoveryCache?.orchestra?.models?.list) {
      const model = global.modelDiscoveryCache.orchestra.models.list.find((m) => m.name === modelName);
      if (model) {
        model.loaded = true;
        model.orchestraReady = true;
      }
    }

    return {
      success: true,
      model: result,
    };
  } catch (error) {
    console.error(`[Models] Failed to load model ${modelName}:`, error);
    return {
      success: false,
      error: error.message,
    };
  }
});

ipcMain.handle('models:unload', async (event, modelName) => {
  try {
    console.log(`[Models] Unloading model: ${modelName}`);

    await nativeOllamaClient.unloadModel(modelName);

    if (global.modelDiscoveryCache?.orchestra?.models?.list) {
      const model = global.modelDiscoveryCache.orchestra.models.list.find((m) => m.name === modelName);
      if (model) {
        model.loaded = false;
        model.orchestraReady = model.cached;
      }
    }

    return {
      success: true,
      message: `Model ${modelName} unloaded successfully`,
    };
  } catch (error) {
    console.error(`[Models] Failed to unload model ${modelName}:`, error);
    return {
      success: false,
      error: error.message,
    };
  }
});

ipcMain.handle('models:info', async (event, modelName) => {
  try {
    const coreInstance = bigDaddyGCore || (await getBigDaddyGCore());
    const info = await coreInstance.getModelInfo(modelName);
    return {
      success: true,
      info,
    };
  } catch (error) {
    return {
      success: false,
      error: error.message,
    };
  }
});

ipcMain.handle('models:stats', async () => {
  try {
    const coreInstance = bigDaddyGCore || (await getBigDaddyGCore());
    const stats = {
      client: {
        initialized: nativeOllamaClient.initialized,
        activeModels: nativeOllamaClient.getActiveModels(),
        cacheStats: nativeOllamaClient.getCacheStats(),
        loadedModels: nativeOllamaClient.getLoadedModels().length,
        usage: nativeOllamaClient.getStats(),
      },
      discovery: global.modelDiscoveryCache || null,
      core: coreInstance?.getStats() || null,
    };

    return {
      success: true,
      stats,
    };
  } catch (error) {
    return {
      success: false,
      error: error.message,
    };
  }
});

ipcMain.handle('models:clear-cache', async () => {
  try {
    nativeOllamaClient.clearCache();
    return {
      success: true,
      message: 'Cache cleared successfully',
    };
  } catch (error) {
    return {
      success: false,
      error: error.message,
    };
  }
});

// ============================================================================
// OLLAMA MODEL MANAGEMENT
// ============================================================================

ipcMain.handle('ollama:list-models', async () => {
  try {
    const models = await listOllamaModels();
    return { success: true, models };
  } catch (error) {
    console.error('[Ollama] List models error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('ollama:status', async () => {
  try {
    const status = await getOllamaStatus();
    return { success: true, status };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('ollama:pull-model', async (event, modelName) => {
  try {
    const model = sanitizeModelIdentifier(modelName);
    const result = await runOllamaCommand(['pull', model]);
    return { ...result, model };
  } catch (error) {
    console.error('[Ollama] Pull error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('ollama:delete-model', async (event, modelName) => {
  try {
    const model = sanitizeModelIdentifier(modelName);
    const result = await runOllamaCommand(['rm', model]);
    return { ...result, model };
  } catch (error) {
    console.error('[Ollama] Delete error:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('ollama:show-model', async (event, modelName) => {
  try {
    const model = sanitizeModelIdentifier(modelName);
    const result = await runOllamaCommand(['show', model]);
    return { ...result, model };
  } catch (error) {
    console.error('[Ollama] Show error:', error);
    return { success: false, error: error.message };
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

// Watch for USB drive changes (Windows) - Optimized with start/stop control
let drivePollingInterval = null;
let isFileExplorerOpen = false;

function startDrivePolling() {
  if (process.platform !== 'win32') return;
  if (drivePollingInterval) return; // Already running
  
  const { exec } = require('child_process');
  
  console.log('[Main] 🔄 Starting drive polling...');
  
  // Initial scan
  exec('wmic logicaldisk get name', (error, stdout) => {
    if (!error && stdout && mainWindow && !mainWindow.isDestroyed()) {
      const currentDrives = stdout.match(/[A-Z]:/g) || [];
      mainWindow.webContents.send('drives-changed', currentDrives);
    }
  });
  
  // Poll for drive changes every 30 seconds
  drivePollingInterval = setInterval(() => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      exec('wmic logicaldisk get name', (error, stdout) => {
        if (!error && stdout) {
          const currentDrives = stdout.match(/[A-Z]:/g) || [];
          mainWindow.webContents.send('drives-changed', currentDrives);
        }
      });
    }
  }, 30000);
}

function stopDrivePolling() {
  if (drivePollingInterval) {
    console.log('[Main] ⏹️ Stopping drive polling...');
    clearInterval(drivePollingInterval);
    drivePollingInterval = null;
  }
}

// IPC handlers for drive polling control
ipcMain.handle('drive-polling:start', () => {
  isFileExplorerOpen = true;
  startDrivePolling();
  return { success: true };
});

ipcMain.handle('drive-polling:stop', () => {
  isFileExplorerOpen = false;
  stopDrivePolling();
  return { success: true };
});

ipcMain.handle('drive-polling:status', () => {
  return { 
    isRunning: drivePollingInterval !== null,
    isFileExplorerOpen
  };
});

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

// NOTE: launchProgram and openInExplorer handlers moved to line ~1640
// to avoid duplicate IPC handler registration (which causes Electron crashes)

// Create directory
ipcMain.handle('createDirectory', async (event, dirPath) => {
  try {
    const validatedDir = validateDirectoryPath(dirPath, { mustExist: false });
    console.log('[FileSystem] Creating directory:', validatedDir);
    
    fs.mkdirSync(validatedDir, { recursive: true });
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Create directory error:', error);
    return { success: false, error: error.message };
  }
});

// Delete file or directory
ipcMain.handle('deleteItem', async (event, itemPath, isDirectory = false) => {
  try {
    const validatedPath = isDirectory
      ? validateDirectoryPath(itemPath)
      : validateFilePath(itemPath);

    console.log('[FileSystem] Deleting:', validatedPath, '(isDirectory:', isDirectory, ')');
    
    if (isDirectory) {
      fs.rmSync(validatedPath, { recursive: true, force: true });
    } else {
      fs.unlinkSync(validatedPath);
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
    const validatedSource = validateFileSystemPath(sourcePath, { label: 'Source path' });
    const validatedDestination = validateFileSystemPath(destPath, {
      mustExist: false,
      label: 'Destination path',
    });

    console.log('[FileSystem] Copying:', validatedSource, 'to', validatedDestination);
    
    const stats = fs.statSync(validatedSource);
    
    if (stats.isDirectory()) {
      // Copy directory recursively
      fs.cpSync(validatedSource, validatedDestination, { recursive: true });
    } else {
      // Copy file
      fs.copyFileSync(validatedSource, validatedDestination);
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
    const validatedSource = validateFileSystemPath(sourcePath, { label: 'Source path' });
    const validatedDestination = validateFileSystemPath(destPath, {
      mustExist: false,
      label: 'Destination path',
    });

    console.log('[FileSystem] Moving:', validatedSource, 'to', validatedDestination);
    
    fs.renameSync(validatedSource, validatedDestination);
    
    return { success: true };
  } catch (error) {
    console.error('[FileSystem] Move error:', error);
    return { success: false, error: error.message };
  }
});

// Get file/directory stats
ipcMain.handle('getStats', async (event, itemPath) => {
  try {
    const validatedPath = validateFileSystemPath(itemPath, { label: 'Item path' });
    const stats = fs.statSync(validatedPath);
    
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

// Command validation and sanitization
function validateCommand(command) {
  if (!command || typeof command !== 'string') {
    throw new Error('Command must be a non-empty string');
  }
  
  if (command.length > 1000) {
    throw new Error('Command too long (max 1000 characters)');
  }
  
  // Check for dangerous patterns
  const dangerousPatterns = [
    /[;&|`$(){}\[\]<>]/,  // Command injection chars
    /\\x[0-9a-fA-F]{2}/,  // Hex encoding
    /\\u[0-9a-fA-F]{4}/,  // Unicode encoding
    /\$\{.*\}/,           // Variable expansion
    /\$\(.*\)/            // Command substitution
  ];
  
  for (const pattern of dangerousPatterns) {
    if (pattern.test(command)) {
      throw new Error('Command contains dangerous characters or patterns');
    }
  }
  
  return command.trim();
}

function validateShell(shell) {
  const allowedShells = ['powershell', 'cmd', 'bash'];
  if (!allowedShells.includes(shell)) {
    throw new Error(`Shell '${shell}' not allowed`);
  }
  return shell;
}

function validateCwd(cwd) {
  if (!cwd || typeof cwd !== 'string') {
    return process.cwd();
  }
  
  // Resolve and validate path
  const resolvedCwd = path.resolve(cwd);
  
  // Check if path exists and is a directory
  try {
    const stats = fs.statSync(resolvedCwd);
    if (!stats.isDirectory()) {
      throw new Error('Working directory must be a directory');
    }
  } catch (error) {
    throw new Error(`Invalid working directory: ${error.message}`);
  }
  
  return resolvedCwd;
}

ipcMain.handle('execute-command', async (event, { command, shell = 'powershell', cwd = process.cwd() }) => {
  const { spawn } = require('child_process');
  
  try {
    // Validate inputs
    const validatedCommand = validateCommand(command);
    const validatedShell = validateShell(shell);
    const validatedCwd = validateCwd(cwd);
    
    console.log(`[Terminal] Executing: ${validatedCommand} (shell: ${validatedShell}, cwd: ${validatedCwd})`);
    
    return new Promise((resolve) => {
      let output = '';
      let error = '';
      
      // Select shell command with restricted options
      let shellCmd, shellArgs;
      switch (validatedShell) {
        case 'powershell':
          shellCmd = 'powershell.exe';
          shellArgs = ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Restricted', '-Command', validatedCommand];
          break;
        case 'cmd':
          shellCmd = 'cmd.exe';
          shellArgs = ['/c', validatedCommand];
          break;
        case 'bash':
          shellCmd = 'bash';
          shellArgs = ['--noprofile', '--norc', '-c', validatedCommand];
          break;
        default:
          shellCmd = 'powershell.exe';
          shellArgs = ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Restricted', '-Command', validatedCommand];
      }
      
      const proc = spawn(shellCmd, shellArgs, { 
        cwd: validatedCwd, 
        shell: false,
        timeout: 30000, // 30 second timeout
        stdio: ['pipe', 'pipe', 'pipe']
      });
      
      proc.stdout?.on('data', (data) => {
        output += data.toString();
        event.sender.send('terminal-output', { output: data.toString(), isError: false });
      });
      
      proc.stderr?.on('data', (data) => {
        error += data.toString();
        event.sender.send('terminal-output', { output: data.toString(), isError: true });
      });
      
      proc.on('close', (code) => {
        event.sender.send('terminal-exit', { code });
        resolve({
          output,
          error,
          code,
          cwd: validatedCwd
        });
      });
      
      proc.on('error', (err) => {
        error = err.message;
        resolve({
          output,
          error,
          code: 1,
          cwd: validatedCwd
        });
      });
    });
  } catch (validationError) {
    console.error('[Terminal] Command validation failed:', validationError.message);
    return {
      output: '',
      error: `Security validation failed: ${validationError.message}`,
      code: 1,
      cwd
    };
  }
});

// ============================================================================
// PROGRAM LAUNCHING & SYSTEM INTEGRATION
// ============================================================================

// Validate program path for security
function validateProgramPath(programPath) {
  if (!programPath || typeof programPath !== 'string') {
    throw new Error('Program path must be a non-empty string');
  }
  
  // Check for path traversal attempts
  if (programPath.includes('..') || programPath.includes('\\..\\') || programPath.includes('/../')) {
    throw new Error('Path traversal attempt detected');
  }
  
  // Resolve absolute path
  const resolvedPath = path.resolve(programPath);
  
  // Check if file exists
  if (!fs.existsSync(resolvedPath)) {
    throw new Error('Program not found');
  }
  
  // Check if it's a file
  const stats = fs.statSync(resolvedPath);
  if (!stats.isFile()) {
    throw new Error('Path must point to a file');
  }
  
  // Check file extension (allow only safe executables)
  const ext = path.extname(resolvedPath).toLowerCase();
  const allowedExtensions = ['.exe', '.msi', '.app', '.deb', '.rpm', '.dmg'];
  
  if (!allowedExtensions.includes(ext)) {
    throw new Error(`File type '${ext}' not allowed for execution`);
  }
  
  return resolvedPath;
}

// Launch external program
ipcMain.handle('launchProgram', async (event, programPath) => {
  try {
    console.log('[System] Launching program:', programPath);
    
    const { shell } = require('electron');
    
    // Validate program path
    const validatedPath = validateProgramPath(programPath);
    
    const result = await shell.openPath(validatedPath);
    
    if (result === '') {
      console.log('[System] ✅ Program launched successfully');
      return { success: true };
    } else {
      console.error('[System] ❌ Failed to launch:', result);
      return { success: false, error: result };
    }
  } catch (error) {
    console.error('[System] Launch error:', error);
    return { success: false, error: error.message };
  }
});

// Open in system explorer/finder
ipcMain.handle('openInExplorer', async (event, itemPath) => {
  try {
    console.log('[System] Opening in explorer:', itemPath);
    const { shell } = require('electron');
    
    if (fs.existsSync(itemPath)) {
      // Show item in folder (works on Windows, macOS, Linux)
      shell.showItemInFolder(itemPath);
      return { success: true };
    } else {
      return { success: false, error: 'Path not found' };
    }
  } catch (error) {
    console.error('[System] Open in explorer error:', error);
    return { success: false, error: error.message };
  }
});

// Validate URL for security
function validateUrl(url) {
  if (!url || typeof url !== 'string') {
    throw new Error('URL must be a non-empty string');
  }
  
  try {
    const urlObj = new URL(url);
    
    // Only allow safe protocols
    const allowedProtocols = ['http:', 'https:', 'mailto:', 'tel:'];
    if (!allowedProtocols.includes(urlObj.protocol)) {
      throw new Error(`Protocol '${urlObj.protocol}' not allowed`);
    }
    
    // Block localhost and private IPs for security
    if (urlObj.hostname === 'localhost' || 
        urlObj.hostname === '127.0.0.1' ||
        urlObj.hostname.startsWith('192.168.') ||
        urlObj.hostname.startsWith('10.') ||
        urlObj.hostname.startsWith('172.')) {
      throw new Error('Local and private URLs not allowed');
    }
    
    return urlObj.href;
  } catch (error) {
    throw new Error(`Invalid URL: ${error.message}`);
  }
}

// Open URL in default browser
ipcMain.handle('openUrl', async (event, url) => {
  try {
    console.log('[System] Opening URL:', url);
    const { shell } = require('electron');
    
    // Validate URL
    const validatedUrl = validateUrl(url);
    
    await shell.openExternal(validatedUrl);
    return { success: true };
  } catch (error) {
    console.error('[System] Open URL error:', error);
    return { success: false, error: error.message };
  }
});

// Get system information
ipcMain.handle('getSystemInfo', async () => {
  const os = require('os');
  
  try {
    return {
      success: true,
      info: {
        platform: process.platform,
        arch: process.arch,
        release: os.release(),
        cpus: os.cpus().length,
        totalMemory: os.totalmem(),
        freeMemory: os.freemem(),
        hostname: os.hostname(),
        username: os.userInfo().username,
        homeDir: os.homedir(),
        tempDir: os.tmpdir()
      }
    };
  } catch (error) {
    console.error('[System] Get system info error:', error);
    return { success: false, error: error.message };
  }
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

// ============================================================================
// NATIVE OLLAMA NODE.JS HTTP CLIENT
// ============================================================================

ipcMain.handle('bigdaddyg:generate', async (event, model, prompt, options) => {
  try {
    const response = await nativeOllamaClient.generate(model, prompt, options || {});
    return { success: true, response };
  } catch (error) {
    console.error('[BigDaddyG] Native generate failed:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('bigdaddyg:stats', async () => {
  try {
    const stats = nativeOllamaClient.getStats();
    return { success: true, stats };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('bigdaddyg:clear-cache', async () => {
  try {
    nativeOllamaClient.clearCache();
    return { success: true };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('native-ollama-node:generate', async (event, model, prompt, options = {}) => {
  try {
    const response = await nativeOllamaClient.generate(model, prompt, options);
    return { success: true, response };
  } catch (error) {
    console.error('[NativeOllama] Generation failed:', error);
    return { success: false, error: error.message };
  }
});

ipcMain.handle('native-ollama-node:stats', async () => {
  try {
    const stats = nativeOllamaClient.getStats();
    return { success: true, stats };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

console.log('[BigDaddyG] ⚡ Native Ollama Node.js client registered');
console.log('[BigDaddyG] 🌌 Main process initialized');

settingsService.on('updated', (payload) => {
  BrowserWindow.getAllWindows().forEach((win) => {
    if (win && !win.isDestroyed()) {
      win.webContents.send('settings:updated', payload);
    }
  });
});

// ============================================================================
// SETTINGS SERVICE
// ============================================================================

ipcMain.handle('settings:get-all', async () => {
  try {
    return { success: true, settings: settingsService.getAll() };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:get-defaults', async () => {
  try {
    return { success: true, settings: settingsService.getDefaults() };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:get', async (event, pathString) => {
  try {
    if (typeof pathString !== 'string') {
      throw new Error('Path must be a string');
    }
    return { success: true, value: settingsService.get(pathString) };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:set', async (event, pathString, value, options = {}) => {
  try {
    if (typeof pathString !== 'string') {
      throw new Error('Path must be a string');
    }
    const settings = settingsService.set(pathString, value, options);
    return { success: true, settings };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:update', async (event, patch, options = {}) => {
  try {
    if (!patch || typeof patch !== 'object') {
      throw new Error('Patch must be an object');
    }
    const settings = settingsService.update(patch, options);
    return { success: true, settings };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:reset', async (event, section = null, options = {}) => {
  try {
    const settings = settingsService.reset(section, options);
    return { success: true, settings };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:hotkeys:get', async () => {
  try {
    return { success: true, hotkeys: settingsService.getHotkeys() };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('settings:hotkeys:set', async (event, action, combo, options = {}) => {
  try {
    if (typeof action !== 'string' || !action) {
      throw new Error('Action is required');
    }
    if (typeof combo !== 'string' || !combo) {
      throw new Error('Combo is required');
    }
    const updated = settingsService.setHotkey(action, combo, options);
    return { success: true, hotkey: updated };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

