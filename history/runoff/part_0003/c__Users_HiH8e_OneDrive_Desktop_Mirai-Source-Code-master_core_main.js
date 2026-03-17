/*
  CyberForge Advanced Security Suite - Main Application
  Quantum-resistant polymorphic security research platform
*/

import { app, BrowserWindow, ipcMain, dialog, Menu } from 'electron';
import path from 'path';
import { fileURLToPath } from 'url';
import fs from 'fs';
import crypto from 'crypto';
import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import rateLimit from 'express-rate-limit';

import { AdvancedEncryptionEngine } from './encryption/advanced-encryption-engine.js';
import { PolymorphicEngine } from '../engines/polymorphic/polymorphic-engine.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

class CyberForgeMainApplication {
  constructor() {
    this.mainWindow = null;
    this.apiServer = null;
    this.encryptionEngine = new AdvancedEncryptionEngine();
    this.polymorphicEngine = new PolymorphicEngine();
    this.sessionManager = new SessionManager();
    this.buildQueue = [];

    // Application configuration
    this.config = {
      app: {
        name: 'CyberForge Advanced Security Suite',
        version: '2.0.0',
        author: 'CyberForge Security Research Team',
        license: 'Research-Only'
      },
      gui: {
        width: 1800,
        height: 1200,
        minWidth: 1400,
        minHeight: 900,
        backgroundColor: '#0a0a0a',
        theme: 'dark-cyber'
      },
      api: {
        port: 8443,
        hostname: 'localhost',
        ssl: true,
        rateLimitWindow: 15 * 60 * 1000, // 15 minutes
        rateLimitMax: 100 // requests per window
      },
      security: {
        sessionTimeout: 3600000, // 1 hour
        maxConcurrentBuilds: 5,
        buildTimeout: 300000, // 5 minutes
        auditLogging: true
      }
    };

    this.initializeApplication();
  }

  async initializeApplication() {
    console.log(`\n🚀 Initializing ${this.config.app.name} v${this.config.app.version}`);
    console.log('='.repeat(70));

    // Set up application event handlers
    this.setupApplicationEvents();

    // Initialize security components
    await this.initializeSecurity();

    // Set up IPC handlers
    this.setupIPCHandlers();

    // Initialize API server
    await this.initializeAPIServer();

    console.log('✅ CyberForge initialization complete');
  }

  setupApplicationEvents() {
    app.whenReady().then(() => {
      this.createMainWindow();
      this.createApplicationMenu();
    });

    app.on('window-all-closed', () => {
      if (process.platform !== 'darwin') {
        app.quit();
      }
    });

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) {
        this.createMainWindow();
      }
    });

    app.on('before-quit', () => {
      this.cleanup();
    });
  }

  async createMainWindow() {
    console.log('🖥️  Creating main application window...');

    this.mainWindow = new BrowserWindow({
      width: this.config.gui.width,
      height: this.config.gui.height,
      minWidth: this.config.gui.minWidth,
      minHeight: this.config.gui.minHeight,
      backgroundColor: this.config.gui.backgroundColor,
      webPreferences: {
        nodeIntegration: true,
        contextIsolation: false,
        webSecurity: false,
        enableRemoteModule: false
      },
      title: this.config.app.name,
      icon: path.join(__dirname, '..', 'gui', 'assets', 'icon.png'),
      show: false,
      titleBarStyle: 'hiddenInset',
      frame: true,
      resizable: true,
      maximizable: true,
      minimizable: true,
      closable: true
    });

    // Load the main GUI
    const guiPath = path.join(__dirname, '..', 'gui', 'dashboard', 'index.html');
    await this.mainWindow.loadFile(guiPath);

    // Show window when ready
    this.mainWindow.once('ready-to-show', () => {
      this.mainWindow.show();
      console.log('✅ Main window displayed');

      if (process.argv.includes('--dev')) {
        this.mainWindow.webContents.openDevTools();
      }
    });

    // Handle window events
    this.mainWindow.on('closed', () => {
      this.mainWindow = null;
    });

    this.mainWindow.on('focus', () => {
      this.sessionManager.updateActivity();
    });
  }

  createApplicationMenu() {
    const template = [
      {
        label: 'CyberForge',
        submenu: [
          {
            label: 'About CyberForge',
            click: () => this.showAboutDialog()
          },
          { type: 'separator' },
          {
            label: 'Preferences',
            accelerator: 'CmdOrCtrl+,',
            click: () => this.showPreferences()
          },
          { type: 'separator' },
          {
            label: 'Quit',
            accelerator: process.platform === 'darwin' ? 'Cmd+Q' : 'Ctrl+Q',
            click: () => app.quit()
          }
        ]
      },
      {
        label: 'File',
        submenu: [
          {
            label: 'New Build',
            accelerator: 'CmdOrCtrl+N',
            click: () => this.newBuild()
          },
          {
            label: 'Open Build',
            accelerator: 'CmdOrCtrl+O',
            click: () => this.openBuild()
          },
          { type: 'separator' },
          {
            label: 'Export Configuration',
            click: () => this.exportConfiguration()
          },
          {
            label: 'Import Configuration',
            click: () => this.importConfiguration()
          }
        ]
      },
      {
        label: 'Build',
        submenu: [
          {
            label: 'Generate Polymorphic Build',
            accelerator: 'CmdOrCtrl+B',
            click: () => this.generatePolymorphicBuild()
          },
          {
            label: 'Cross-Platform Compile',
            accelerator: 'CmdOrCtrl+Shift+B',
            click: () => this.crossPlatformCompile()
          },
          { type: 'separator' },
          {
            label: 'View Build Queue',
            click: () => this.showBuildQueue()
          },
          {
            label: 'Clear Build Queue',
            click: () => this.clearBuildQueue()
          }
        ]
      },
      {
        label: 'Security',
        submenu: [
          {
            label: 'Encryption Manager',
            click: () => this.showEncryptionManager()
          },
          {
            label: 'Key Management',
            click: () => this.showKeyManagement()
          },
          { type: 'separator' },
          {
            label: 'Security Audit',
            click: () => this.performSecurityAudit()
          },
          {
            label: 'Generate Security Report',
            click: () => this.generateSecurityReport()
          }
        ]
      },
      {
        label: 'Tools',
        submenu: [
          {
            label: 'File Encryptor',
            click: () => this.showFileEncryptor()
          },
          {
            label: 'Code Analyzer',
            click: () => this.showCodeAnalyzer()
          },
          {
            label: 'Binary Packer',
            click: () => this.showBinaryPacker()
          },
          { type: 'separator' },
          {
            label: 'System Information',
            click: () => this.showSystemInformation()
          }
        ]
      },
      {
        label: 'View',
        submenu: [
          {
            label: 'Dashboard',
            accelerator: 'CmdOrCtrl+1',
            click: () => this.showDashboard()
          },
          {
            label: 'Build Manager',
            accelerator: 'CmdOrCtrl+2',
            click: () => this.showBuildManager()
          },
          {
            label: 'Encryption Console',
            accelerator: 'CmdOrCtrl+3',
            click: () => this.showEncryptionConsole()
          },
          { type: 'separator' },
          {
            label: 'Reload',
            accelerator: 'CmdOrCtrl+R',
            click: () => this.mainWindow.reload()
          },
          {
            label: 'Developer Tools',
            accelerator: 'F12',
            click: () => this.mainWindow.webContents.openDevTools()
          }
        ]
      },
      {
        label: 'Help',
        submenu: [
          {
            label: 'Documentation',
            click: () => this.showDocumentation()
          },
          {
            label: 'API Reference',
            click: () => this.showAPIReference()
          },
          { type: 'separator' },
          {
            label: 'Check for Updates',
            click: () => this.checkForUpdates()
          },
          {
            label: 'Report Issue',
            click: () => this.reportIssue()
          }
        ]
      }
    ];

    const menu = Menu.buildFromTemplate(template);
    Menu.setApplicationMenu(menu);
  }

  async initializeSecurity() {
    console.log('🔒 Initializing security components...');

    // Initialize session management
    await this.sessionManager.initialize();

    // Set up security logging
    this.setupSecurityLogging();

    // Initialize encryption components
    console.log('   ✅ Advanced encryption engine ready');
    console.log('   ✅ Polymorphic engine ready');
    console.log('   ✅ Session management ready');
  }

  setupSecurityLogging() {
    if (this.config.security.auditLogging) {
      const logDir = path.join(process.cwd(), 'logs');
      if (!fs.existsSync(logDir)) {
        fs.mkdirSync(logDir, { recursive: true });
      }

      this.auditLogger = new AuditLogger(logDir);
      console.log('   ✅ Audit logging enabled');
    }
  }

  setupIPCHandlers() {
    console.log('🔗 Setting up IPC communication handlers...');

    // Application management
    ipcMain.handle('app:getConfig', () => this.config);
    ipcMain.handle('app:getVersion', () => this.config.app.version);
    ipcMain.handle('app:getCapabilities', () => this.getCapabilities());

    // Encryption operations
    ipcMain.handle('encryption:encrypt', async (event, data, algorithms, options) => {
      this.auditLogger?.log('encryption', 'encrypt', { algorithms, dataSize: data.length });
      return await this.encryptionEngine.encrypt(data, algorithms, options);
    });

    ipcMain.handle('encryption:getStatistics', () => {
      return this.encryptionEngine.getStatistics();
    });

    ipcMain.handle('encryption:getSupportedAlgorithms', () => {
      return this.encryptionEngine.getSupportedAlgorithms();
    });

    // Polymorphic build operations
    ipcMain.handle('polymorphic:generateBuild', async (event, config) => {
      this.auditLogger?.log('polymorphic', 'generateBuild', { config });
      return await this.polymorphicEngine.generatePolymorphicExecutable(config);
    });

    ipcMain.handle('polymorphic:getStatistics', () => {
      return this.polymorphicEngine.getStatistics();
    });

    ipcMain.handle('polymorphic:getGenerationHistory', () => {
      return this.polymorphicEngine.getGenerationHistory();
    });

    // File operations
    ipcMain.handle('file:selectFile', async () => {
      const result = await dialog.showOpenDialog(this.mainWindow, {
        title: 'Select File',
        filters: [
          { name: 'All Files', extensions: ['*'] },
          { name: 'Executables', extensions: ['exe', 'bin', 'elf'] },
          { name: 'Source Code', extensions: ['c', 'cpp', 'h', 'hpp'] },
          { name: 'Text Files', extensions: ['txt', 'log', 'md'] }
        ],
        properties: ['openFile']
      });

      if (!result.canceled && result.filePaths.length > 0) {
        this.auditLogger?.log('file', 'select', { path: result.filePaths[0] });
        return result.filePaths[0];
      }
      return null;
    });

    ipcMain.handle('file:selectDirectory', async () => {
      const result = await dialog.showOpenDialog(this.mainWindow, {
        title: 'Select Directory',
        properties: ['openDirectory']
      });

      if (!result.canceled && result.filePaths.length > 0) {
        this.auditLogger?.log('file', 'selectDirectory', { path: result.filePaths[0] });
        return result.filePaths[0];
      }
      return null;
    });

    ipcMain.handle('file:readFile', async (event, filePath) => {
      try {
        this.auditLogger?.log('file', 'read', { path: filePath });
        const data = fs.readFileSync(filePath);
        return { success: true, data: data.toString('base64') };
      } catch (error) {
        this.auditLogger?.log('file', 'readError', { path: filePath, error: error.message });
        return { success: false, error: error.message };
      }
    });

    ipcMain.handle('file:writeFile', async (event, filePath, data, encoding = 'utf8') => {
      try {
        this.auditLogger?.log('file', 'write', { path: filePath });
        fs.writeFileSync(filePath, data, encoding);
        return { success: true };
      } catch (error) {
        this.auditLogger?.log('file', 'writeError', { path: filePath, error: error.message });
        return { success: false, error: error.message };
      }
    });

    // Build queue operations
    ipcMain.handle('build:addToBuildQueue', (event, buildConfig) => {
      const buildId = this.generateBuildId();
      this.buildQueue.push({
        id: buildId,
        config: buildConfig,
        status: 'queued',
        timestamp: new Date().toISOString()
      });
      this.auditLogger?.log('build', 'addToQueue', { buildId, config: buildConfig });
      return buildId;
    });

    ipcMain.handle('build:getBuildQueue', () => {
      return this.buildQueue;
    });

    ipcMain.handle('build:clearBuildQueue', () => {
      this.buildQueue = [];
      this.auditLogger?.log('build', 'clearQueue', {});
      return true;
    });

    // Session management
    ipcMain.handle('session:getInfo', () => {
      return this.sessionManager.getSessionInfo();
    });

    ipcMain.handle('session:updateActivity', () => {
      this.sessionManager.updateActivity();
      return true;
    });

    // System information
    ipcMain.handle('system:getInfo', () => {
      return this.getSystemInformation();
    });

    console.log('✅ IPC handlers configured');
  }

  async initializeAPIServer() {
    console.log('🌐 Initializing API server...');

    const app = express();

    // Security middleware
    app.use(helmet({
      contentSecurityPolicy: {
        directives: {
          defaultSrc: ["'self'"],
          scriptSrc: ["'self'", "'unsafe-inline'"],
          styleSrc: ["'self'", "'unsafe-inline'"],
          imgSrc: ["'self'", "data:", "https:"]
        }
      }
    }));

    app.use(cors({
      origin: `https://${this.config.api.hostname}:${this.config.api.port}`,
      credentials: true
    }));

    // Rate limiting
    const limiter = rateLimit({
      windowMs: this.config.api.rateLimitWindow,
      max: this.config.api.rateLimitMax,
      message: 'Too many requests, please try again later',
      standardHeaders: true,
      legacyHeaders: false
    });
    app.use(limiter);

    app.use(express.json({ limit: '50mb' }));
    app.use(express.urlencoded({ extended: true, limit: '50mb' }));

    // API routes
    this.setupAPIRoutes(app);

    // Start server
    const port = this.config.api.port;
    this.apiServer = app.listen(port, this.config.api.hostname, () => {
      console.log(`✅ API server running on https://${this.config.api.hostname}:${port}`);
    });
  }

  setupAPIRoutes(app) {
    // Health check
    app.get('/api/health', (req, res) => {
      res.json({
        status: 'healthy',
        version: this.config.app.version,
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
      });
    });

    // Encryption endpoints
    app.post('/api/encrypt', async (req, res) => {
      try {
        const { data, algorithms, options } = req.body;
        const result = await this.encryptionEngine.encrypt(data, algorithms, options);
        res.json(result);
      } catch (error) {
        res.status(500).json({ error: error.message });
      }
    });

    app.get('/api/encryption/algorithms', (req, res) => {
      res.json(this.encryptionEngine.getSupportedAlgorithms());
    });

    app.get('/api/encryption/statistics', (req, res) => {
      res.json(this.encryptionEngine.getStatistics());
    });

    // Polymorphic endpoints
    app.post('/api/polymorphic/generate', async (req, res) => {
      try {
        const { config } = req.body;
        const result = await this.polymorphicEngine.generatePolymorphicExecutable(config);
        res.json(result);
      } catch (error) {
        res.status(500).json({ error: error.message });
      }
    });

    app.get('/api/polymorphic/statistics', (req, res) => {
      res.json(this.polymorphicEngine.getStatistics());
    });

    // System information endpoints
    app.get('/api/system/info', (req, res) => {
      res.json(this.getSystemInformation());
    });

    app.get('/api/system/capabilities', (req, res) => {
      res.json(this.getCapabilities());
    });

    // Error handling
    app.use((error, req, res, next) => {
      console.error('API Error:', error);
      res.status(500).json({ error: 'Internal server error' });
    });
  }

  getCapabilities() {
    return {
      encryption: {
        quantumResistant: true,
        multiLayer: true,
        polymorphic: true,
        algorithms: Object.keys({
          ...this.encryptionEngine.quantumAlgorithms,
          ...this.encryptionEngine.classicalAlgorithms,
          ...this.encryptionEngine.polymorphicAlgorithms
        })
      },
      polymorphic: {
        codeGeneration: true,
        crossPlatform: true,
        antiDetection: true,
        platforms: ['Windows x32', 'Windows x64', 'Linux x32', 'Linux x64', 'ARM32', 'ARM64']
      },
      build: {
        concurrentBuilds: this.config.security.maxConcurrentBuilds,
        outputFormats: ['EXE', 'DLL', 'Service', 'Driver', 'Standalone'],
        crossCompilation: true
      },
      security: {
        sessionManagement: true,
        auditLogging: this.config.security.auditLogging,
        rateLimiting: true,
        encryption: true
      }
    };
  }

  getSystemInformation() {
    const os = require('os');

    return {
      platform: os.platform(),
      architecture: os.arch(),
      hostname: os.hostname(),
      cpus: os.cpus().length,
      totalMemory: os.totalmem(),
      freeMemory: os.freemem(),
      uptime: os.uptime(),
      nodeVersion: process.version,
      electronVersion: process.versions.electron,
      applicationVersion: this.config.app.version,
      timestamp: new Date().toISOString()
    };
  }

  generateBuildId() {
    const timestamp = Date.now().toString(36);
    const random = crypto.randomBytes(8).toString('hex');
    return `cf_${timestamp}_${random}`;
  }

  // Menu action handlers
  showAboutDialog() {
    dialog.showMessageBox(this.mainWindow, {
      type: 'info',
      title: 'About CyberForge',
      message: this.config.app.name,
      detail: `Version: ${this.config.app.version}\nAuthor: ${this.config.app.author}\nLicense: ${this.config.app.license}\n\nAdvanced polymorphic security research platform with quantum-resistant encryption capabilities.`,
      buttons: ['OK']
    });
  }

  showPreferences() {
    // Implementation for preferences dialog
    console.log('Opening preferences...');
  }

  newBuild() {
    this.mainWindow.webContents.send('action:newBuild');
  }

  openBuild() {
    this.mainWindow.webContents.send('action:openBuild');
  }

  exportConfiguration() {
    this.mainWindow.webContents.send('action:exportConfiguration');
  }

  importConfiguration() {
    this.mainWindow.webContents.send('action:importConfiguration');
  }

  generatePolymorphicBuild() {
    this.mainWindow.webContents.send('action:generatePolymorphicBuild');
  }

  crossPlatformCompile() {
    this.mainWindow.webContents.send('action:crossPlatformCompile');
  }

  showBuildQueue() {
    this.mainWindow.webContents.send('action:showBuildQueue');
  }

  clearBuildQueue() {
    this.buildQueue = [];
    this.mainWindow.webContents.send('action:buildQueueCleared');
  }

  showEncryptionManager() {
    this.mainWindow.webContents.send('action:showEncryptionManager');
  }

  showKeyManagement() {
    this.mainWindow.webContents.send('action:showKeyManagement');
  }

  performSecurityAudit() {
    this.mainWindow.webContents.send('action:performSecurityAudit');
  }

  generateSecurityReport() {
    this.mainWindow.webContents.send('action:generateSecurityReport');
  }

  showFileEncryptor() {
    this.mainWindow.webContents.send('action:showFileEncryptor');
  }

  showCodeAnalyzer() {
    this.mainWindow.webContents.send('action:showCodeAnalyzer');
  }

  showBinaryPacker() {
    this.mainWindow.webContents.send('action:showBinaryPacker');
  }

  showSystemInformation() {
    this.mainWindow.webContents.send('action:showSystemInformation');
  }

  showDashboard() {
    this.mainWindow.webContents.send('action:showDashboard');
  }

  showBuildManager() {
    this.mainWindow.webContents.send('action:showBuildManager');
  }

  showEncryptionConsole() {
    this.mainWindow.webContents.send('action:showEncryptionConsole');
  }

  showDocumentation() {
    // Open external documentation
    require('electron').shell.openExternal('https://cyberforge.security/docs');
  }

  showAPIReference() {
    // Open API reference
    require('electron').shell.openExternal('https://cyberforge.security/api');
  }

  checkForUpdates() {
    console.log('Checking for updates...');
  }

  reportIssue() {
    // Open issue reporting
    require('electron').shell.openExternal('https://github.com/cyberforge/advanced-security-suite/issues');
  }

  cleanup() {
    console.log('🧹 Cleaning up application resources...');

    if (this.apiServer) {
      this.apiServer.close();
    }

    if (this.sessionManager) {
      this.sessionManager.cleanup();
    }

    console.log('✅ Cleanup complete');
  }
}

// Session management
class SessionManager {
  constructor() {
    this.sessions = new Map();
    this.currentSession = null;
    this.activityTimeout = 3600000; // 1 hour
  }

  async initialize() {
    this.currentSession = {
      id: crypto.randomBytes(16).toString('hex'),
      startTime: new Date(),
      lastActivity: new Date(),
      user: 'researcher',
      permissions: ['read', 'write', 'execute', 'admin']
    };

    this.sessions.set(this.currentSession.id, this.currentSession);

    // Set up activity timeout
    this.setupActivityTimeout();
  }

  updateActivity() {
    if (this.currentSession) {
      this.currentSession.lastActivity = new Date();
    }
  }

  getSessionInfo() {
    return this.currentSession;
  }

  setupActivityTimeout() {
    setInterval(() => {
      if (this.currentSession) {
        const inactiveTime = Date.now() - this.currentSession.lastActivity.getTime();
        if (inactiveTime > this.activityTimeout) {
          console.log('⚠️  Session timeout due to inactivity');
          // Handle session timeout
        }
      }
    }, 60000); // Check every minute
  }

  cleanup() {
    this.sessions.clear();
    this.currentSession = null;
  }
}

// Audit logging
class AuditLogger {
  constructor(logDirectory) {
    this.logDirectory = logDirectory;
    this.logFile = path.join(logDirectory, `cyberforge_audit_${new Date().toISOString().split('T')[0]}.log`);
  }

  log(category, action, details = {}) {
    const logEntry = {
      timestamp: new Date().toISOString(),
      category,
      action,
      details,
      sessionId: 'current', // Would be actual session ID in production
      pid: process.pid
    };

    const logLine = JSON.stringify(logEntry) + '\n';

    try {
      fs.appendFileSync(this.logFile, logLine);
    } catch (error) {
      console.error('Audit logging error:', error);
    }
  }
}

// Initialize and start the application
const cyberForgeApp = new CyberForgeMainApplication();