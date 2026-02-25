// NeuroSymphonicTray.js
// Desktop Application with System Tray Icon
// Like Ollama - runs in background, accessible from taskbar

const { app, BrowserWindow, Tray, Menu, ipcMain, shell } = require('electron');
const path = require('path');
const express = require('express');
const http = require('http');

// Import our core modules
const NeuroSymphonicOrchestrator = require('../ai-orchestration/NeuroSymphonicOrchestrator.js');
const CrossRepoSync = require('../orchestrator/agent_registry/CrossRepoSync.js');

class NeuroSymphonicApp {
    constructor() {
        this.tray = null;
        this.mainWindow = null;
        this.overlayWindow = null;
        this.expressApp = null;
        this.httpServer = null;
        
        // Core services
        this.orchestrator = null;
        this.sync = null;
        
        // App state
        this.emotionalState = 'CALM';
        this.isRunning = false;
        this.port = 8080;
        this.syncPort = 9000;
        
        // Model management (like Ollama)
        this.models = {
            'emotional-state-machine': { loaded: true, size: '2.3 MB' },
            'voice-recognition': { loaded: false, size: '45 MB' },
            'webgl-renderer': { loaded: true, size: '1.2 MB' },
            'ai-orchestrator': { loaded: false, size: '120 MB' },
            'cross-repo-sync': { loaded: true, size: '890 KB' }
        };
        
        this.initializeApp();
    }
    
    initializeApp() {
        console.log('🧠 [NeuroSymphonic] Initializing desktop application...');
        
        // Electron app events
        app.whenReady().then(() => {
            this.createTray();
            this.startServices();
            this.createExpressServer();
        });
        
        app.on('window-all-closed', (e) => {
            // Don't quit on window close - stay in tray
            e.preventDefault();
        });
        
        app.on('before-quit', () => {
            this.stopServices();
        });
    }
    
    createTray() {
        console.log('🎨 [NeuroSymphonic] Creating system tray icon...');
        
        // Create tray icon
        const iconPath = path.join(__dirname, 'assets', 'icon.png');
        this.tray = new Tray(iconPath);
        this.tray.setToolTip('Neuro-Symphonic Workspace');
        
        // Build context menu
        this.updateTrayMenu();
        
        // Tray click - show main window
        this.tray.on('click', () => {
            this.toggleMainWindow();
        });
        
        console.log('✅ [NeuroSymphonic] System tray icon created');
    }
    
    updateTrayMenu() {
        const contextMenu = Menu.buildFromTemplate([
            {
                label: '🧠 Neuro-Symphonic Workspace',
                enabled: false
            },
            { type: 'separator' },
            {
                label: `Status: ${this.isRunning ? '🟢 Running' : '🔴 Stopped'}`,
                enabled: false
            },
            {
                label: `Emotional State: ${this.getEmotionalStateEmoji()} ${this.emotionalState}`,
                enabled: false
            },
            { type: 'separator' },
            {
                label: '🎯 Emotional States',
                submenu: [
                    {
                        label: '😌 Switch to CALM',
                        click: () => this.setEmotionalState('CALM')
                    },
                    {
                        label: '🎯 Switch to FOCUSED',
                        click: () => this.setEmotionalState('FOCUSED')
                    },
                    {
                        label: '🔥 Switch to INTENSE',
                        click: () => this.setEmotionalState('INTENSE')
                    },
                    {
                        label: '😵 Emergency Rest (OVERWHELMED)',
                        click: () => this.setEmotionalState('OVERWHELMED')
                    }
                ]
            },
            {
                label: '🤖 Models',
                submenu: this.buildModelsMenu()
            },
            {
                label: '🔗 Services',
                submenu: [
                    {
                        label: this.isRunning ? '⏸️ Stop Services' : '▶️ Start Services',
                        click: () => this.toggleServices()
                    },
                    { type: 'separator' },
                    {
                        label: '🌐 Open Dashboard',
                        click: () => this.openDashboard()
                    },
                    {
                        label: '👁️ Show Overlay',
                        click: () => this.showOverlay()
                    },
                    { type: 'separator' },
                    {
                        label: '📊 View Telemetry',
                        click: () => this.showTelemetry()
                    },
                    {
                        label: '📈 Performance Stats',
                        click: () => this.showPerformanceStats()
                    }
                ]
            },
            { type: 'separator' },
            {
                label: '⚙️ Settings',
                click: () => this.showSettings()
            },
            {
                label: '📚 Documentation',
                click: () => this.openDocumentation()
            },
            {
                label: '🔍 About',
                click: () => this.showAbout()
            },
            { type: 'separator' },
            {
                label: '🚪 Quit Neuro-Symphonic',
                click: () => {
                    app.isQuitting = true;
                    app.quit();
                }
            }
        ]);
        
        this.tray.setContextMenu(contextMenu);
    }
    
    buildModelsMenu() {
        const modelItems = Object.entries(this.models).map(([name, info]) => ({
            label: `${info.loaded ? '✅' : '⬜'} ${name} (${info.size})`,
            click: () => this.toggleModel(name)
        }));
        
        return [
            ...modelItems,
            { type: 'separator' },
            {
                label: '📥 Download New Models',
                click: () => this.showModelManager()
            }
        ];
    }
    
    getEmotionalStateEmoji() {
        const emojis = {
            'CALM': '😌',
            'FOCUSED': '🎯',
            'INTENSE': '🔥',
            'OVERWHELMED': '😵'
        };
        return emojis[this.emotionalState] || '🤔';
    }
    
    async startServices() {
        if (this.isRunning) {
            console.log('⚠️ [NeuroSymphonic] Services already running');
            return;
        }
        
        console.log('🚀 [NeuroSymphonic] Starting services...');
        
        try {
            // Initialize orchestrator
            this.orchestrator = new NeuroSymphonicOrchestrator({
                preferred_provider: 'local',
                cost_tracking: true,
                emotional_awareness: true
            });
            
            // Initialize cross-repo sync
            this.sync = new CrossRepoSync({
                sync_interval: 1000,
                broadcast_port: this.syncPort,
                emotional_sync_enabled: true,
                voice_sync_enabled: true
            });
            
            this.isRunning = true;
            this.updateTrayMenu();
            
            // Show notification
            this.showNotification('Services Started', 'Neuro-Symphonic is now running');
            
            console.log('✅ [NeuroSymphonic] All services started successfully');
        } catch (error) {
            console.error('❌ [NeuroSymphonic] Failed to start services:', error);
            this.showNotification('Error', 'Failed to start services: ' + error.message);
        }
    }
    
    stopServices() {
        if (!this.isRunning) return;
        
        console.log('🛑 [NeuroSymphonic] Stopping services...');
        
        // Close HTTP server
        if (this.httpServer) {
            this.httpServer.close();
        }
        
        // Close WebSocket servers
        if (this.sync && this.sync.wsServer) {
            this.sync.wsServer.close();
        }
        
        this.isRunning = false;
        this.updateTrayMenu();
        
        console.log('✅ [NeuroSymphonic] Services stopped');
    }
    
    toggleServices() {
        if (this.isRunning) {
            this.stopServices();
        } else {
            this.startServices();
        }
    }
    
    createExpressServer() {
        console.log('🌐 [NeuroSymphonic] Creating web server...');
        
        this.expressApp = express();
        
        // Serve static files
        this.expressApp.use(express.static(path.join(__dirname, '..', 'visualization-engines', 'webgl-renderer')));
        this.expressApp.use(express.json());
        
        // API endpoints
        this.expressApp.get('/api/status', (req, res) => {
            res.json({
                running: this.isRunning,
                emotional_state: this.emotionalState,
                models: this.models,
                uptime: process.uptime(),
                version: '1.0.0'
            });
        });
        
        this.expressApp.get('/api/emotional-state', (req, res) => {
            res.json({
                state: this.emotionalState,
                timestamp: Date.now()
            });
        });
        
        this.expressApp.post('/api/emotional-state', (req, res) => {
            const { state } = req.body;
            if (state) {
                this.setEmotionalState(state);
                res.json({ success: true, state });
            } else {
                res.status(400).json({ error: 'State required' });
            }
        });
        
        this.expressApp.get('/api/models', (req, res) => {
            res.json(this.models);
        });
        
        // Health check
        this.expressApp.get('/health', (req, res) => {
            res.json({ status: 'ok', timestamp: Date.now() });
        });
        
        // Start server
        this.httpServer = this.expressApp.listen(this.port, () => {
            console.log(`✅ [NeuroSymphonic] Web server running on http://localhost:${this.port}`);
        });
    }
    
    setEmotionalState(newState) {
        const oldState = this.emotionalState;
        this.emotionalState = newState;
        
        console.log(`💭 [NeuroSymphonic] Emotional state: ${oldState} → ${newState}`);
        
        // Update sync service
        if (this.sync) {
            this.sync.updateEmotionalState(newState, 'desktop-app');
        }
        
        // Update tray
        this.updateTrayMenu();
        
        // Show notification
        this.showNotification(
            'Emotional State Changed',
            `${this.getEmotionalStateEmoji()} Switched to ${newState}`
        );
    }
    
    toggleModel(modelName) {
        if (this.models[modelName]) {
            this.models[modelName].loaded = !this.models[modelName].loaded;
            
            const status = this.models[modelName].loaded ? 'loaded' : 'unloaded';
            console.log(`📦 [NeuroSymphonic] Model ${modelName} ${status}`);
            
            this.updateTrayMenu();
            this.showNotification(
                'Model Updated',
                `${modelName} ${status}`
            );
        }
    }
    
    toggleMainWindow() {
        if (this.mainWindow && !this.mainWindow.isDestroyed()) {
            if (this.mainWindow.isVisible()) {
                this.mainWindow.hide();
            } else {
                this.mainWindow.show();
                this.mainWindow.focus();
            }
        } else {
            this.createMainWindow();
        }
    }
    
    createMainWindow() {
        console.log('🪟 [NeuroSymphonic] Creating main window...');
        
        this.mainWindow = new BrowserWindow({
            width: 1200,
            height: 800,
            show: false,
            icon: path.join(__dirname, 'assets', 'icon.png'),
            webPreferences: {
                nodeIntegration: false,
                contextIsolation: true,
                preload: path.join(__dirname, 'preload.js')
            }
        });
        
        this.mainWindow.loadURL(`http://localhost:${this.port}`);
        
        this.mainWindow.once('ready-to-show', () => {
            this.mainWindow.show();
        });
        
        this.mainWindow.on('close', (e) => {
            if (!app.isQuitting) {
                e.preventDefault();
                this.mainWindow.hide();
            }
        });
    }
    
    showOverlay() {
        if (this.overlayWindow && !this.overlayWindow.isDestroyed()) {
            this.overlayWindow.focus();
            return;
        }
        
        console.log('👁️ [NeuroSymphonic] Creating overlay window...');
        
        this.overlayWindow = new BrowserWindow({
            width: 400,
            height: 300,
            frame: false,
            transparent: true,
            alwaysOnTop: true,
            skipTaskbar: true,
            webPreferences: {
                nodeIntegration: false
            }
        });
        
        this.overlayWindow.loadURL(`http://localhost:${this.port}?overlay=true`);
        
        this.overlayWindow.on('closed', () => {
            this.overlayWindow = null;
        });
    }
    
    openDashboard() {
        shell.openExternal(`http://localhost:${this.port}`);
    }
    
    showTelemetry() {
        if (this.orchestrator) {
            this.orchestrator.printReport();
        }
        if (this.sync) {
            this.sync.printStatus();
        }
    }
    
    showPerformanceStats() {
        const stats = {
            uptime: process.uptime(),
            memory: process.memoryUsage(),
            cpu: process.cpuUsage(),
            emotional_state: this.emotionalState,
            services_running: this.isRunning
        };
        
        console.log('📈 [NeuroSymphonic] Performance Stats:', stats);
        
        // Could open a window with detailed stats
        this.showNotification(
            'Performance',
            `Uptime: ${Math.floor(stats.uptime / 60)}m | Memory: ${Math.round(stats.memory.heapUsed / 1024 / 1024)}MB`
        );
    }
    
    showSettings() {
        // TODO: Create settings window
        console.log('⚙️ [NeuroSymphonic] Opening settings...');
        this.showNotification('Settings', 'Settings panel coming soon!');
    }
    
    showModelManager() {
        // TODO: Create model manager window
        console.log('📦 [NeuroSymphonic] Opening model manager...');
        this.showNotification('Models', 'Model manager coming soon!');
    }
    
    openDocumentation() {
        const docsPath = path.join(__dirname, '..', 'QUICK-START.md');
        shell.openPath(docsPath);
    }
    
    showAbout() {
        this.showNotification(
            'Neuro-Symphonic Workspace v1.0.0',
            'Emotional intelligence for your development environment\n\nManaging 102 repositories with:\n• Real-time emotional synchronization\n• Voice-first interactions\n• AI orchestration\n• 240Hz visualization'
        );
    }
    
    showNotification(title, body) {
        if (this.tray) {
            this.tray.displayBalloon({
                title: title,
                content: body,
                icon: path.join(__dirname, 'assets', 'icon.png')
            });
        }
    }
}

// Start the application
const neuroSymphonicApp = new NeuroSymphonicApp();

module.exports = NeuroSymphonicApp;

