const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const path = require('path');
require('dotenv').config();

const RawrZStandalone = require('./rawrz-standalone');
const rawrzEngine = require('./src/engines/rawrz-engine');

const app = express();
const port = parseInt(process.env.PORT || '8080', 10);
const authToken = process.env.AUTH_TOKEN || '';

const rawrz = new RawrZStandalone();

// Auth middleware
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

// Middleware
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-hashes'"],
            scriptSrcAttr: ["'unsafe-inline'"],
            styleSrc: ["'self'", "'unsafe-inline'"],
            imgSrc: ["'self'", "data:", "https:"],
            connectSrc: ["'self'"],
            fontSrc: ["'self'", "https:", "data:"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'"],
            frameSrc: ["'none'"],
        },
    },
}));
app.use(cors());
app.use(express.json({ limit: '5mb' }));
app.use(express.urlencoded({ extended: true }));

// Static file serving for all panels
app.use('/static', express.static(path.join(__dirname, 'public')));
app.use(express.static(path.join(__dirname, 'public')));

// Initialize RawrZ Engine
(async () => {
    try {
        await rawrzEngine.initializeModules();
        console.log('[OK] RawrZ core engine initialized');
    } catch (e) {
        console.error('[WARN] Core engine init failed:', e.message);
    }
})();

// Health check
app.get('/health', (_req, res) => res.json({ ok: true, status: 'healthy' }));

// Favicon
app.get('/favicon.ico', (_req, res) => res.status(204).end());

// Panel routes - serve all panels
app.get('/', (_req, res) => res.redirect('/unified-panel.html'));
app.get('/panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'panel.html')));
app.get('/unified-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'unified-panel.html')));
app.get('/health-dashboard', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'health-dashboard.html')));
app.get('/ev-cert-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'ev-cert-panel.html')));
app.get('/stub-generator-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'stub-generator-panel.html')));
app.get('/payload-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'payload-panel.html')));
app.get('/beaconism-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'beaconism-panel.html')));
app.get('/http-bot-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'http-bot-panel.html')));
app.get('/irc-bot-builder', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'irc-bot-builder.html')));
app.get('/bot-manager', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'bot-manager.html')));
app.get('/cve-analysis-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'cve-analysis-panel.html')));
app.get('/red-killer-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'red-killer-panel.html')));
app.get('/advanced-features-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'advanced-features-panel.html')));
app.get('/enhanced-payload-panel', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'enhanced-payload-panel.html')));

// Core API endpoints
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

// Network tools
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

// File operations
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

// CLI interface
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

// Engine status endpoints
app.get('/api/engines/status', requireAuth, async (req, res) => {
    try {
        const status = await rawrzEngine.getStatus();
        res.json(status);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

app.get('/api/engines/list', requireAuth, async (req, res) => {
    try {
        const status = await rawrzEngine.getStatus();
        res.json(status.modules.available);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// System logs endpoint
app.get('/api/logs', requireAuth, async (req, res) => {
    try {
        const fs = require('fs');
        const logPath = path.join(__dirname, 'logs', 'rawrz.log');
        if (fs.existsSync(logPath)) {
            const logs = fs.readFileSync(logPath, 'utf8');
            const lines = logs.split('\n').slice(-100); // Last 100 lines
            res.json({ logs: lines.filter(line => line.trim()) });
        } else {
            res.json({ logs: ['No logs available'] });
        }
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// System metrics endpoint
app.get('/api/metrics', requireAuth, async (req, res) => {
    try {
        const os = require('os');
        const metrics = {
            uptime: process.uptime(),
            memory: process.memoryUsage(),
            cpu: os.cpus(),
            platform: os.platform(),
            arch: os.arch(),
            totalMemory: os.totalmem(),
            freeMemory: os.freemem(),
            loadAverage: os.loadavg(),
            timestamp: new Date().toISOString()
        };
        res.json(metrics);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Engine details endpoint
app.get('/api/engines/:engineName', requireAuth, async (req, res) => {
    try {
        const { engineName } = req.params;
        // Try to load the specific engine
        const enginePath = path.join(__dirname, 'src', 'engines', `${engineName}.js`);
        if (require('fs').existsSync(enginePath)) {
            const engine = require(enginePath);
            const status = engine.getStatus ? await engine.getStatus() : { status: 'loaded' };
            res.json({ 
                name: engineName, 
                status: status,
                available: true 
            });
        } else {
            res.json({ 
                name: engineName, 
                status: { status: 'not found' },
                available: false 
            });
        }
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Start server
app.listen(port, () => {
    console.log('[OK] RawrZ API listening on port', port);
    console.log('[OK] All panels available at http://localhost:' + port);
    console.log('[OK] Unified dashboard: http://localhost:' + port + '/unified-panel.html');
});
