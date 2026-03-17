const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const ffi = require('ffi-napi');
const ref = require('ref-napi');

// Import RawrXD Security Module
const { RawrXDSecurity } = require('./rawrxd-security');

// ─────────────────────────────────────────────────────────────────────────────
// Security hardening (local IDE backend)
// ─────────────────────────────────────────────────────────────────────────────
const MAX_BODY_BYTES = 10 * 1024 * 1024;        // 10MB (test_jumper expects 11MB → 413)
const MAX_READ_FILE_BYTES = 2 * 1024 * 1024;    // 2MB cap for /api/read-file
const RATE_WINDOW_MS = 10 * 1000;
const RATE_MAX_REQ_PER_WINDOW = 200;
const RATE_MAX_GEN_PER_WINDOW = 30;

const rateState = new Map();

function getClientIp(req) {
    // Trust direct socket for local IDE; don't trust X-Forwarded-For by default.
    const addr = (req.socket && req.socket.remoteAddress) ? String(req.socket.remoteAddress) : '';
    return addr;
}

function isLoopbackIp(ip) {
    if (!ip) return false;
    // Node may present IPv4-mapped IPv6 as ::ffff:127.0.0.1
    if (ip === '127.0.0.1' || ip === '::1') return true;
    if (ip.startsWith('::ffff:')) {
        const v4 = ip.substring('::ffff:'.length);
        return v4 === '127.0.0.1';
    }
    return false;
}

function applySecurityHeaders(req, res) {
    // Conservative headers that won't break the GUI (many pages use inline scripts/styles)
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'DENY');
    res.setHeader('Referrer-Policy', 'no-referrer');
    res.setHeader('Permissions-Policy', 'geolocation=(), microphone=(), camera=()');
    res.setHeader('Cross-Origin-Resource-Policy', 'same-origin');

    // CSP: allow self + localhost backends used by the IDE
    // Note: 'unsafe-inline' is required by existing GUI pages.
    const csp = [
        "default-src 'self'",
        "base-uri 'self'",
        "object-src 'none'",
        "img-src 'self' data:",
        "style-src 'self' 'unsafe-inline'",
        "script-src 'self' 'unsafe-inline'",
        "connect-src 'self' http://localhost:11434 http://127.0.0.1:11434 http://localhost:3000 http://127.0.0.1:3000",
        "frame-ancestors 'none'",
    ].join('; ');
    res.setHeader('Content-Security-Policy', csp);
}

function applyCors(req, res) {
    // This backend is intended for local use. Allow same-origin + loopback origins.
    const origin = req.headers.origin ? String(req.headers.origin) : '';
    const allowedOrigins = new Set([
        `http://localhost:${PORT}`,
        `http://127.0.0.1:${PORT}`,
        `http://localhost`,
        `http://127.0.0.1`,
        'null', // some WebView2 contexts
    ]);
    if (origin && allowedOrigins.has(origin)) {
        res.setHeader('Access-Control-Allow-Origin', origin);
        res.setHeader('Vary', 'Origin');
    } else if (!origin && isLoopbackIp(getClientIp(req))) {
        // Non-browser / same-process clients
        res.setHeader('Access-Control-Allow-Origin', '*');
    }
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

function enforceBodySizeLimit(req, res) {
    // Attach a passive guard; endpoint handlers may also read body.
    if (req.method === 'GET' || req.method === 'HEAD' || req.method === 'OPTIONS') return;
    let seen = 0;
    let tripped = false;
    req.on('data', (chunk) => {
        if (tripped) return;
        seen += chunk.length;
        if (seen > MAX_BODY_BYTES) {
            tripped = true;
            try {
                res.writeHead(413, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Payload too large', limitBytes: MAX_BODY_BYTES }));
            } catch (_) { }
            try { req.destroy(); } catch (_) { }
        }
    });
}

function rateLimit(req, res, urlPath) {
    const ip = getClientIp(req) || 'unknown';
    const now = Date.now();
    const key = ip;
    const cur = rateState.get(key) || { start: now, total: 0, gen: 0 };

    if (now - cur.start > RATE_WINDOW_MS) {
        cur.start = now;
        cur.total = 0;
        cur.gen = 0;
    }

    cur.total++;
    if (urlPath === '/api/generate' || urlPath === '/v1/chat/completions' || urlPath === '/ask' || urlPath === '/api/generate') {
        cur.gen++;
    }
    rateState.set(key, cur);

    if (cur.total > RATE_MAX_REQ_PER_WINDOW || cur.gen > RATE_MAX_GEN_PER_WINDOW) {
        res.writeHead(429, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            error: 'Rate limit exceeded',
            windowMs: RATE_WINDOW_MS,
            maxRequests: RATE_MAX_REQ_PER_WINDOW,
            maxGenerates: RATE_MAX_GEN_PER_WINDOW
        }));
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Secure API Key Management (from E:\SECURITY_AND_API_KEY_MANAGEMENT_GUIDE.md)
// ─────────────────────────────────────────────────────────────────────────────
function loadApiKeys() {
    const keys = {};
    const envVars = [
        'OPENAI_API_KEY', 'ANTHROPIC_API_KEY', 'GOOGLE_API_KEY',
        'MOONSHOT_API_KEY', 'AZURE_OPENAI_KEY', 'AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY'
    ];
    for (const varName of envVars) {
        const value = process.env[varName];
        if (value && value.trim()) {
            keys[varName] = value.trim();
        }
    }
    return keys;
}

function maskApiKey(key) {
    if (!key || key.length < 8) return '***';
    return key.substring(0, 4) + '...' + key.substring(key.length - 4);
}

function secureLog(message, ...args) {
    // Replace any potential API keys in args with masked versions
    const maskedArgs = args.map(arg => {
        if (typeof arg === 'string' && arg.length > 20 && /^[a-zA-Z0-9_-]+$/.test(arg)) {
            // Heuristic: looks like an API key
            return maskApiKey(arg);
        }
        return arg;
    });
    console.log(message, ...maskedArgs);
}

const apiKeys = loadApiKeys();

// Centralized version — single source of truth from package.json
const PKG = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json'), 'utf8'));
const SERVER_VERSION = PKG.version || '0.0.0';

// Load the compiled DLL
let phase3Dll = null;
try {
    phase3Dll = ffi.Library(path.join(__dirname, 'bin', 'Phase3_Agent_Kernel.dll'), {
        'Phase3Initialize': ['pointer', ['pointer', 'pointer']],
        'GenerateTokens': ['int', ['pointer', 'string', 'pointer']],
        'Phase3Shutdown': ['void', ['pointer']],
        'ModelUploader_CreateContext': ['pointer', []],
        'ModelUploader_ShowDialog': ['int', ['pointer', 'pointer', 'uint32']],
        'ModelUploader_LoadFiles': ['int', ['pointer', 'string']],
        'ModelUploader_GetProgress': ['int', ['pointer', 'pointer', 'pointer', 'string', 'uint32']],
        'ModelUploader_UnloadModel': ['void', ['pointer']],
        'ModelUploader_GetTensor': ['int', ['pointer', 'string', 'pointer', 'pointer']],
        'DragDrop_RegisterWindow': ['int', ['pointer']],
        'DragDrop_HandleMessage': ['int', ['pointer', 'pointer', 'uint32', 'pointer', 'pointer']]
    });
    console.log('✅ Phase-3 Agent Kernel DLL loaded successfully');
} catch (error) {
    console.log('❌ Failed to load DLL:', error.message);
    console.log('🔧 Building DLL first...');
}

// Global context
let agentContext = null;
let uploaderContext = null;

// Initialize contexts
function initializeKernel() {
    try {
        if (!phase3Dll) return false;

        // Create uploader context
        uploaderContext = phase3Dll.ModelUploader_CreateContext();
        if (uploaderContext.isNull()) {
            console.log('❌ Failed to create uploader context');
            return false;
        }

        console.log('✅ Kernel initialized successfully');
        return true;
    } catch (error) {
        console.log('❌ Kernel initialization failed:', error.message);
        return false;
    }
}

// HTTP Server
const server = http.createServer((req, res) => {
    applySecurityHeaders(req, res);
    applyCors(req, res);
    enforceBodySizeLimit(req, res);

    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!rateLimit(req, res, url.pathname)) return;

    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }

    // ── Serve GUI pages ──
    const GUI_ROUTES = {
        '/': 'launcher.html',
        '/gui': 'launcher.html',
        '/launcher': 'launcher.html',
        '/chatbot': 'ide_chatbot.html',
        '/agents': 'agents.html',
        '/multiwindow': 'multiwindow_ide.html',
        '/feature-tester': 'feature_tester.html',
        '/bruteforce': 'model_bruteforce.html',
        '/test-jumper': 'test_jumper.html',
        '/test-swarm': 'test_swarm.html',
        '/chatbot-standalone': 'ide_chatbot_standalone.html',
        '/chatbot-win32': 'ide_chatbot_win32.html',
    };
    if (GUI_ROUTES[url.pathname]) {
        const htmlPath = path.join(__dirname, 'gui', GUI_ROUTES[url.pathname]);
        fs.readFile(htmlPath, 'utf8', (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('HTML file not found: ' + GUI_ROUTES[url.pathname]);
                return;
            }
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(data);
        });
        return;
    }
    // Serve static assets from gui/ (JS, CSS, etc.)
    if (url.pathname.startsWith('/gui/')) {
        const assetPath = path.join(__dirname, url.pathname);
        const ext = path.extname(assetPath).toLowerCase();
        const mimeTypes = { '.js': 'application/javascript', '.css': 'text/css', '.html': 'text/html', '.json': 'application/json', '.png': 'image/png', '.svg': 'image/svg+xml' };
        fs.readFile(assetPath, (err, data) => {
            if (err) { res.writeHead(404); res.end('Not found'); return; }
            res.writeHead(200, { 'Content-Type': mimeTypes[ext] || 'application/octet-stream' });
            res.end(data);
        });
        return;
    }

    // ── Backend discovery endpoints (chatbot probes these) ──
    if (url.pathname === '/status') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            status: 'ok',
            server: 'RawrXD-IDE-Server',
            version: SERVER_VERSION,
            backend: 'rawrxd-win32ide',
            port: PORT,
            dllLoaded: !!phase3Dll,
            kernelReady: !!(agentContext && !agentContext?.isNull?.()),
            uptime: Math.round((Date.now() - serverMetrics.startTime) / 1000)
        }));
        return;
    }
    if (url.pathname === '/health') {
        // Real model-loaded check: DLL kernel active OR Ollama reachable
        const dllReady = !!(phase3Dll && agentContext && !agentContext?.isNull?.());
        const modelsLoaded = dllReady || serverMetrics.ollamaReachable;
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            status: 'ok',
            version: SERVER_VERSION,
            models_loaded: modelsLoaded,
            dll_ready: dllReady,
            ollama_reachable: !!serverMetrics.ollamaReachable,
            model_count: serverMetrics.modelCount,
            total_generates: serverMetrics.generates
        }));
        return;
    }

    // ── Ollama-compatible /api/tags ──
    if (url.pathname === '/api/tags') {
        handleOllamaTagsRequest(req, res);
        return;
    }

    // ── Ollama-compatible /api/generate ──
    if (url.pathname === '/api/generate') {
        handleOllamaGenerateRequest(req, res);
        return;
    }

    // ── OpenAI-compatible /v1/chat/completions ──
    if (url.pathname === '/v1/chat/completions') {
        handleOpenAIChatCompletions(req, res);
        return;
    }

    // ── Model listing (both /models and /api/models) ──
    if (url.pathname === '/models' || url.pathname === '/api/models') {
        handleModelsRequest(req, res);
        return;
    }

    // ── Directory listing for IDE file tree ──
    if (url.pathname === '/api/list-dir') {
        handleListDirRequest(req, res);
        return;
    }

    // ── File read endpoint for IDE file bridge ──
    if (url.pathname === '/api/read-file') {
        handleReadFileRequest(req, res);
        return;
    }

    // ── CLI execution endpoint (build, deploy, etc.) ──
    if (url.pathname === '/api/cli' || url.pathname === '/api/build' || url.pathname === '/api/deploy') {
        handleCliRequest(req, res, url.pathname);
        return;
    }

    // API endpoints
    if (url.pathname === '/api/upload') {
        handleUploadRequest(req, res);
    } else if (url.pathname === '/api/progress') {
        handleProgressRequest(req, res);
    } else if (url.pathname === '/api/generate') {
        handleGenerateRequest(req, res);
    } else if (url.pathname === '/ask') {
        handleAskRequest(req, res);
    } else if (url.pathname === '/models/load' || url.pathname === '/models/unload'
        || url.pathname === '/models/status' || url.pathname === '/models/switch'
        || url.pathname === '/inference/generate') {
        // Proxy to the unified model-server on port 3000
        proxyToModelServer(req, res, url);
    } else {
        res.writeHead(404);
        res.end('Not found');
    }
});

// Handle model listing — real discovery: scan disk + check DLL + probe Ollama
function handleModelsRequest(req, res) {
    const models = [];

    // 1. Phase-3 DLL (real runtime check)
    if (phase3Dll && agentContext && !agentContext.isNull()) {
        models.push({ id: 'phase3-native', name: 'Phase-3 Agent Kernel', type: 'native', status: 'ready' });
    }

    // 2. Scan disk for GGUF / safetensors / bin model files
    const searchDirs = [
        __dirname,
        path.join(__dirname, 'models'),
        path.join(__dirname, 'Modelfiles'),
        path.join(__dirname, 'test_40gb_models')
    ];
    const exts = ['.gguf', '.bin', '.safetensors', '.pt', '.pth', '.onnx'];
    for (const dir of searchDirs) {
        try {
            if (!fs.existsSync(dir)) continue;
            const entries = fs.readdirSync(dir, { withFileTypes: true });
            for (const e of entries) {
                if (e.isFile() && exts.some(ext => e.name.toLowerCase().endsWith(ext))) {
                    const full = path.join(dir, e.name);
                    const stat = fs.statSync(full);
                    models.push({
                        id: 'local-' + e.name,
                        name: e.name,
                        type: 'local-file',
                        format: path.extname(e.name).replace('.', '').toUpperCase(),
                        sizeMB: Math.round(stat.size / 1048576),
                        path: full,
                        status: 'available'
                    });
                }
            }
        } catch { }
    }

    // 3. Probe Ollama for running models
    const ollamaReq = http.get('http://localhost:11434/api/tags', { timeout: 2000 }, (ollamaRes) => {
        let buf = '';
        ollamaRes.on('data', c => buf += c);
        ollamaRes.on('end', () => {
            try {
                const tags = JSON.parse(buf);
                for (const m of (tags.models || [])) {
                    models.push({
                        id: 'ollama-' + m.name,
                        name: m.name,
                        type: 'ollama',
                        sizeMB: Math.round((m.size || 0) / 1048576),
                        status: 'ready'
                    });
                }
            } catch { }
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ models, count: models.length }));
        });
    });
    ollamaReq.on('error', () => {
        // Ollama not running — return what we have
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ models, count: models.length, ollamaStatus: 'unreachable' }));
    });
    ollamaReq.on('timeout', () => {
        ollamaReq.destroy();
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ models, count: models.length, ollamaStatus: 'timeout' }));
    });
}

// Handle file upload
function handleUploadRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405);
        res.end('Method not allowed');
        return;
    }

    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        try {
            const { files } = JSON.parse(body);

            if (!uploaderContext || uploaderContext.isNull()) {
                res.writeHead(500);
                res.end(JSON.stringify({ error: 'Uploader not initialized' }));
                return;
            }

            // Process files through DLL
            const result = phase3Dll.ModelUploader_LoadFiles(uploaderContext, files.join(';'));

            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({
                success: result === 1,
                message: result === 1 ? 'Upload started' : 'Upload failed'
            }));
        } catch (error) {
            res.writeHead(500);
            res.end(JSON.stringify({ error: error.message }));
        }
    });
}

// Handle progress polling
function handleProgressRequest(req, res) {
    if (!uploaderContext || uploaderContext.isNull()) {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ stage: 0, percent: 0, status: 'Not initialized' }));
        return;
    }

    try {
        const stageBuffer = Buffer.alloc(4);
        const percentBuffer = Buffer.alloc(4);
        const statusBuffer = Buffer.alloc(256);

        const result = phase3Dll.ModelUploader_GetProgress(
            uploaderContext,
            stageBuffer,
            percentBuffer,
            statusBuffer,
            256
        );

        const stage = stageBuffer.readUInt32LE(0);
        const percent = percentBuffer.readUInt32LE(0);
        const status = statusBuffer.toString('utf8').replace(/\0.*$/, '');

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ stage, percent, status }));
    } catch (error) {
        res.writeHead(500);
        res.end(JSON.stringify({ error: error.message }));
    }
}

// Handle text generation
function handleGenerateRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405);
        res.end('Method not allowed');
        return;
    }

    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        try {
            const { prompt, model } = JSON.parse(body);

            if (model === 'phase3-native' && agentContext && !agentContext.isNull()) {
                // Use native DLL
                const outputBuffer = Buffer.alloc(4096);
                const result = phase3Dll.GenerateTokens(agentContext, prompt, outputBuffer);

                if (result === 1) {
                    const response = outputBuffer.toString('utf8').replace(/\0.*$/, '');
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ response }));
                } else {
                    throw new Error('Generation failed');
                }
            } else {
                // Real fallback: route through Ollama if available, else model-server
                // Use the first available model from Ollama probe, not a hardcoded name
                const ollamaModel = serverMetrics._firstOllamaModel || 'llama3.2';
                const ollamaBody = JSON.stringify({
                    model: ollamaModel,
                    prompt: prompt,
                    stream: false,
                    options: { num_predict: 512 }
                });
                const ollamaOpts = {
                    hostname: 'localhost', port: 11434, path: '/api/generate', method: 'POST',
                    headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(ollamaBody) },
                    timeout: 120000
                };
                const ollamaReq = http.request(ollamaOpts, (ollamaRes) => {
                    let buf = '';
                    ollamaRes.on('data', c => buf += c);
                    ollamaRes.on('end', () => {
                        try {
                            const parsed = JSON.parse(buf);
                            // Track real token metrics from Ollama response
                            const evalCount = parsed.eval_count || 0;
                            const tps = (evalCount && parsed.eval_duration)
                                ? parseFloat((evalCount / (parsed.eval_duration / 1e9)).toFixed(1))
                                : 0;
                            const durationMs = (parsed.total_duration || 0) / 1e6;
                            serverMetrics.totalTokens += evalCount;
                            if (tps > 0) {
                                serverMetrics.lastTps = tps;
                                if (tps > serverMetrics.peakTps) serverMetrics.peakTps = tps;
                            }
                            if (durationMs > 0) {
                                serverMetrics.latencySum += durationMs;
                                serverMetrics.latencyCount++;
                                serverMetrics.lastGenerateMs = Math.round(durationMs);
                            }
                            res.writeHead(200, { 'Content-Type': 'application/json' });
                            res.end(JSON.stringify({
                                response: parsed.response || '',
                                model: parsed.model || 'ollama',
                                evalCount: evalCount,
                                tokensPerSec: tps.toString(),
                                totalDurationMs: durationMs
                            }));
                        } catch (parseErr) {
                            res.writeHead(200, { 'Content-Type': 'application/json' });
                            res.end(JSON.stringify({ response: buf, model: 'ollama-raw' }));
                        }
                    });
                });
                ollamaReq.on('error', (ollamaErr) => {
                    // Ollama not available — try model-server proxy
                    const msBody = JSON.stringify({ prompt, params: {} });
                    const msOpts = {
                        hostname: 'localhost', port: MODEL_SERVER_PORT,
                        path: '/inference/generate', method: 'POST',
                        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(msBody) },
                        timeout: 120000
                    };
                    const msReq = http.request(msOpts, (msRes) => {
                        let mbuf = '';
                        msRes.on('data', c => mbuf += c);
                        msRes.on('end', () => {
                            res.writeHead(msRes.statusCode, { 'Content-Type': 'application/json' });
                            res.end(mbuf);
                        });
                    });
                    msReq.on('error', () => {
                        res.writeHead(503, { 'Content-Type': 'application/json' });
                        res.end(JSON.stringify({
                            error: 'No inference backend available',
                            hint: 'Start Ollama (ollama serve) or the model-server (node D:\\rawrxd\\ide\\model-server\\proxy-server.js)',
                            ollamaError: ollamaErr.message
                        }));
                    });
                    msReq.write(msBody);
                    msReq.end();
                });
                ollamaReq.on('timeout', () => ollamaReq.destroy());
                ollamaReq.write(ollamaBody);
                ollamaReq.end();
            }
        } catch (error) {
            res.writeHead(500);
            res.end(JSON.stringify({ error: error.message }));
        }
    });
}

// Handle ask requests (compatibility) — extracts 'question' field and forwards
function handleAskRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405);
        res.end('Method not allowed');
        return;
    }
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        try {
            const parsed = JSON.parse(body);
            // The chatbot sends {question, model, context, stream}
            // Rewrite it to {prompt, model} for handleGenerateRequest's inner logic
            const prompt = parsed.question || parsed.prompt || '';
            const model = parsed.model || null;
            // Bridge: resubmit as a generate request
            req._parsedBody = { prompt, model };
            handleGenerateInner(req, res, prompt, model);
        } catch (error) {
            res.writeHead(400);
            res.end(JSON.stringify({ error: 'Invalid JSON: ' + error.message }));
        }
    });
}

// ════════════════════════════════════════════════════════════════════
// Ollama-compatible /api/tags — returns model list in Ollama format
// The chatbot calls GET /api/tags expecting {models: [{name, size}]}
// ════════════════════════════════════════════════════════════════════
function handleOllamaTagsRequest(req, res) {
    // First try to get real Ollama tags
    const ollamaReq = http.get('http://localhost:11434/api/tags', { timeout: 2000 }, (ollamaRes) => {
        let buf = '';
        ollamaRes.on('data', c => buf += c);
        ollamaRes.on('end', () => {
            // Forward real Ollama response
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(buf);
        });
    });
    ollamaReq.on('error', () => {
        // Ollama not running — scan disk and return Ollama-format list
        const models = [];
        const searchDirs = [__dirname, path.join(__dirname, 'models'), path.join(__dirname, 'Modelfiles')];
        const exts = ['.gguf', '.bin', '.safetensors'];
        for (const dir of searchDirs) {
            try {
                if (!fs.existsSync(dir)) continue;
                for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
                    if (e.isFile() && exts.some(ext => e.name.toLowerCase().endsWith(ext))) {
                        const stat = fs.statSync(path.join(dir, e.name));
                        models.push({
                            name: e.name.replace(/\.[^.]+$/, ''),
                            model: e.name.replace(/\.[^.]+$/, ''),
                            size: stat.size,
                            modified_at: stat.mtime.toISOString(),
                            details: { format: path.extname(e.name).replace('.', '').toUpperCase() }
                        });
                    }
                }
            } catch { }
        }
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ models }));
    });
    ollamaReq.on('timeout', () => {
        ollamaReq.destroy();
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ models: [] }));
    });
}

// ════════════════════════════════════════════════════════════════════
// Ollama-compatible POST /api/generate — the chatbot's fallback path
// Forwards to real Ollama, or model-server, or returns 503
// ════════════════════════════════════════════════════════════════════
function handleOllamaGenerateRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405);
        res.end('Method not allowed');
        return;
    }
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        // Forward directly to Ollama
        const ollamaOpts = {
            hostname: 'localhost', port: 11434, path: '/api/generate', method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) },
            timeout: 120000
        };
        const ollamaReq = http.request(ollamaOpts, (ollamaRes) => {
            // Stream-forward the response headers and body
            res.writeHead(ollamaRes.statusCode, {
                'Content-Type': ollamaRes.headers['content-type'] || 'application/json',
                'Transfer-Encoding': ollamaRes.headers['transfer-encoding'] || 'identity'
            });
            ollamaRes.pipe(res);
        });
        ollamaReq.on('error', (ollamaErr) => {
            // Ollama not available — try model-server
            const msOpts = {
                hostname: 'localhost', port: MODEL_SERVER_PORT,
                path: '/inference/generate', method: 'POST',
                headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) },
                timeout: 120000
            };
            const msReq = http.request(msOpts, (msRes) => {
                // Convert model-server response to Ollama format
                let mbuf = '';
                msRes.on('data', c => mbuf += c);
                msRes.on('end', () => {
                    try {
                        const parsed = JSON.parse(mbuf);
                        res.writeHead(200, { 'Content-Type': 'application/json' });
                        res.end(JSON.stringify({
                            model: parsed.model || 'rawrxd',
                            response: parsed.response || parsed.text || '',
                            done: true,
                            total_duration: (parsed.wallClockMs || 0) * 1e6,
                            eval_count: parsed.evalCount || 0
                        }));
                    } catch {
                        res.writeHead(200, { 'Content-Type': 'application/json' });
                        res.end(JSON.stringify({ model: 'rawrxd', response: mbuf, done: true }));
                    }
                });
            });
            msReq.on('error', () => {
                res.writeHead(503, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    error: 'No inference backend available',
                    hint: 'Start Ollama (ollama serve) or model-server',
                    ollamaError: ollamaErr.message
                }));
            });
            msReq.write(body);
            msReq.end();
        });
        ollamaReq.on('timeout', () => ollamaReq.destroy());
        ollamaReq.write(body);
        ollamaReq.end();
    });
}

// ════════════════════════════════════════════════════════════════════
// OpenAI-compatible POST /v1/chat/completions
// The chatbot tries this FIRST. Forward to Ollama's OpenAI compat
// endpoint, or convert to /api/generate, or forward to model-server
// ════════════════════════════════════════════════════════════════════
function handleOpenAIChatCompletions(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405);
        res.end('Method not allowed');
        return;
    }
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        let parsed;
        try { parsed = JSON.parse(body); } catch (e) {
            res.writeHead(400);
            res.end(JSON.stringify({ error: { message: 'Invalid JSON' } }));
            return;
        }

        const isStream = parsed.stream === true;

        // Try 1: Forward to Ollama's OpenAI-compatible endpoint
        const ollamaOpts = {
            hostname: 'localhost', port: 11434, path: '/v1/chat/completions', method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) },
            timeout: 120000
        };
        const ollamaReq = http.request(ollamaOpts, (ollamaRes) => {
            if (ollamaRes.statusCode >= 400) {
                // Ollama returned error — fallback to /api/generate conversion
                let errBuf = '';
                ollamaRes.on('data', c => errBuf += c);
                ollamaRes.on('end', () => {
                    fallbackToApiGenerate(parsed, isStream, res);
                });
                return;
            }
            // Stream-forward the response
            res.writeHead(ollamaRes.statusCode, {
                'Content-Type': ollamaRes.headers['content-type'] || 'application/json',
                'Transfer-Encoding': ollamaRes.headers['transfer-encoding'] || 'identity',
                'Cache-Control': 'no-cache'
            });
            ollamaRes.pipe(res);
        });
        ollamaReq.on('error', () => {
            // Ollama not reachable — try model-server
            fallbackToModelServer(parsed, isStream, res);
        });
        ollamaReq.on('timeout', () => {
            ollamaReq.destroy();
            fallbackToModelServer(parsed, isStream, res);
        });
        ollamaReq.write(body);
        ollamaReq.end();
    });
}

// Fallback: convert OpenAI chat to Ollama /api/generate
function fallbackToApiGenerate(parsed, isStream, res) {
    const messages = parsed.messages || [];
    const prompt = messages.map(m => `${m.role}: ${m.content}`).join('\n') + '\nassistant:';
    const model = parsed.model || 'rawrxd';
    const genBody = JSON.stringify({
        model, prompt, stream: false,
        options: { temperature: parsed.temperature || 0.7, num_predict: parsed.max_tokens || 512 }
    });
    const ollamaOpts = {
        hostname: 'localhost', port: 11434, path: '/api/generate', method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(genBody) },
        timeout: 120000
    };
    const ollamaReq = http.request(ollamaOpts, (ollamaRes) => {
        let buf = '';
        ollamaRes.on('data', c => buf += c);
        ollamaRes.on('end', () => {
            try {
                const ollamaData = JSON.parse(buf);
                const openaiResponse = {
                    id: 'chatcmpl-' + Date.now(),
                    object: 'chat.completion',
                    created: Math.floor(Date.now() / 1000),
                    model: ollamaData.model || model,
                    choices: [{
                        index: 0,
                        message: { role: 'assistant', content: ollamaData.response || '' },
                        finish_reason: 'stop'
                    }],
                    usage: {
                        prompt_tokens: ollamaData.prompt_eval_count || 0,
                        completion_tokens: ollamaData.eval_count || 0,
                        total_tokens: (ollamaData.prompt_eval_count || 0) + (ollamaData.eval_count || 0)
                    }
                };
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(openaiResponse));
            } catch {
                res.writeHead(502, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: { message: 'Failed to parse Ollama response' } }));
            }
        });
    });
    ollamaReq.on('error', () => {
        fallbackToModelServer(parsed, isStream, res);
    });
    ollamaReq.on('timeout', () => {
        ollamaReq.destroy();
        fallbackToModelServer(parsed, isStream, res);
    });
    ollamaReq.write(genBody);
    ollamaReq.end();
}

// Fallback: forward to model-server (port 3000) /inference/chat or /inference/generate
function fallbackToModelServer(parsed, isStream, res) {
    const messages = parsed.messages || [];
    const prompt = messages.map(m => `${m.role}: ${m.content}`).join('\n') + '\nassistant:';
    const msBody = JSON.stringify({
        prompt,
        params: { temperature: parsed.temperature || 0.7, maxTokens: parsed.max_tokens || 512 }
    });
    const msOpts = {
        hostname: 'localhost', port: MODEL_SERVER_PORT,
        path: '/inference/generate', method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(msBody) },
        timeout: 120000
    };
    const msReq = http.request(msOpts, (msRes) => {
        let mbuf = '';
        msRes.on('data', c => mbuf += c);
        msRes.on('end', () => {
            try {
                const msData = JSON.parse(mbuf);
                const openaiResponse = {
                    id: 'chatcmpl-' + Date.now(),
                    object: 'chat.completion',
                    created: Math.floor(Date.now() / 1000),
                    model: msData.model || parsed.model || 'rawrxd',
                    choices: [{
                        index: 0,
                        message: { role: 'assistant', content: msData.response || msData.text || '' },
                        finish_reason: 'stop'
                    }],
                    usage: {
                        prompt_tokens: 0,
                        completion_tokens: msData.evalCount || 0,
                        total_tokens: msData.evalCount || 0
                    }
                };
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(openaiResponse));
            } catch {
                res.writeHead(502, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: { message: 'Model server response parse error' } }));
            }
        });
    });
    msReq.on('error', (msErr) => {
        res.writeHead(503, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            error: {
                message: 'No inference backend available. Start Ollama (ollama serve) or the model-server.',
                type: 'server_error',
                details: msErr.message
            }
        }));
    });
    msReq.write(msBody);
    msReq.end();
}

// Inner generate handler (called by both /api/generate and /ask)
function handleGenerateInner(req, res, prompt, model) {
    if (!prompt) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'prompt or question is required' }));
        return;
    }

    if (model === 'phase3-native' && phase3Dll && agentContext && !agentContext.isNull()) {
        // Use native DLL
        const outputBuffer = Buffer.alloc(4096);
        const result = phase3Dll.GenerateTokens(agentContext, prompt, outputBuffer);
        if (result === 1) {
            const response = outputBuffer.toString('utf8').replace(/\0.*$/, '');
            res.writeHead(200, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ answer: response, response, model: 'phase3-native' }));
        } else {
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'DLL generation failed' }));
        }
        return;
    }

    // Cascade: Ollama → model-server → error
    const ollamaModel = model || serverMetrics._firstOllamaModel || 'llama3.2';
    const ollamaBody = JSON.stringify({
        model: ollamaModel, prompt, stream: false,
        options: { num_predict: 512 }
    });
    const ollamaOpts = {
        hostname: 'localhost', port: 11434, path: '/api/generate', method: 'POST',
        headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(ollamaBody) },
        timeout: 120000
    };
    const ollamaReq = http.request(ollamaOpts, (ollamaRes) => {
        let buf = '';
        ollamaRes.on('data', c => buf += c);
        ollamaRes.on('end', () => {
            try {
                const p = JSON.parse(buf);
                // Track real token metrics from inner generate
                const innerEval = p.eval_count || 0;
                const innerTps = (innerEval && p.eval_duration)
                    ? parseFloat((innerEval / (p.eval_duration / 1e9)).toFixed(1)) : 0;
                const innerDurMs = (p.total_duration || 0) / 1e6;
                serverMetrics.totalTokens += innerEval;
                if (innerTps > 0) {
                    serverMetrics.lastTps = innerTps;
                    if (innerTps > serverMetrics.peakTps) serverMetrics.peakTps = innerTps;
                }
                if (innerDurMs > 0) {
                    serverMetrics.latencySum += innerDurMs;
                    serverMetrics.latencyCount++;
                    serverMetrics.lastGenerateMs = Math.round(innerDurMs);
                }
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    answer: p.response || '',
                    response: p.response || '',
                    model: p.model || 'ollama',
                    evalCount: innerEval,
                    tokensPerSec: innerTps.toString(),
                    totalDurationMs: innerDurMs
                }));
            } catch {
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ answer: buf, response: buf, model: 'ollama-raw' }));
            }
        });
    });
    ollamaReq.on('error', (ollamaErr) => {
        const msBody = JSON.stringify({ prompt, params: {} });
        const msOpts = {
            hostname: 'localhost', port: MODEL_SERVER_PORT,
            path: '/inference/generate', method: 'POST',
            headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(msBody) },
            timeout: 120000
        };
        const msReq = http.request(msOpts, (msRes) => {
            let mbuf = '';
            msRes.on('data', c => mbuf += c);
            msRes.on('end', () => {
                try {
                    const msData = JSON.parse(mbuf);
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({
                        answer: msData.response || '',
                        response: msData.response || '',
                        model: msData.model || 'rawrxd'
                    }));
                } catch {
                    res.writeHead(msRes.statusCode, { 'Content-Type': 'application/json' });
                    res.end(mbuf);
                }
            });
        });
        msReq.on('error', () => {
            res.writeHead(503, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({
                error: 'No inference backend available',
                hint: 'Start Ollama (ollama serve) or the model-server',
                ollamaError: ollamaErr.message
            }));
        });
        msReq.write(msBody);
        msReq.end();
    });
    ollamaReq.on('timeout', () => ollamaReq.destroy());
    ollamaReq.write(ollamaBody);
    ollamaReq.end();
}

// Check if DLL exists (build is a separate offline step)
function buildDllIfNeeded() {
    const dllPath = path.join(__dirname, 'bin', 'Phase3_Agent_Kernel.dll');
    if (!fs.existsSync(dllPath)) {
        console.log('⚠️  DLL not found at', dllPath);
        console.log('   Build it offline: cmake --build . --config Release --target RawrXD-Shell');
        console.log('   Server will run in Ollama/model-server fallback mode.');
        return false;
    }
    return true;
}

// Start server
const PORT = 8080;
const MODEL_SERVER_PORT = 3000;

// Proxy requests to the model-server (D:\rawrxd\ide\model-server)
function proxyToModelServer(req, res, url) {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        const options = {
            hostname: 'localhost',
            port: MODEL_SERVER_PORT,
            path: url.pathname,
            method: req.method,
            headers: { 'Content-Type': 'application/json' }
        };
        const proxyReq = http.request(options, (proxyRes) => {
            let buf = '';
            proxyRes.on('data', (c) => buf += c);
            proxyRes.on('end', () => {
                res.writeHead(proxyRes.statusCode, { 'Content-Type': 'application/json' });
                res.end(buf);
            });
        });
        proxyReq.on('error', (e) => {
            res.writeHead(502);
            res.end(JSON.stringify({
                error: 'Model server unreachable: ' + e.message,
                hint: 'Start with: node D:\\rawrxd\\ide\\model-server\\proxy-server.js'
            }));
        });
        if (body) proxyReq.write(body);
        proxyReq.end();
    });
}

// ── Runtime metrics ──
const serverMetrics = {
    requests: 0, generates: 0, errors: 0, startTime: Date.now(),
    totalTokens: 0, lastTps: 0, peakTps: 0,
    latencySum: 0, latencyCount: 0,
    ollamaReachable: false, modelCount: 0,
    endpointHits: {},        // { '/v1/chat/completions': N, '/api/generate': N, ... }
    lastGenerateMs: 0,       // wall-clock ms of last generate
    _firstOllamaModel: null, // populated by probe — used as fallback when no model specified
};

// Periodic Ollama reachability probe (every 30s)
function probeOllamaReachability() {
    const req = http.get('http://localhost:11434/api/tags', { timeout: 2000 }, (res) => {
        let buf = '';
        res.on('data', c => buf += c);
        res.on('end', () => {
            try {
                const tags = JSON.parse(buf);
                const models = tags.models || [];
                serverMetrics.ollamaReachable = true;
                serverMetrics.modelCount = models.length;
                // Capture the first available model for fallback routing
                if (models.length > 0 && models[0].name) {
                    serverMetrics._firstOllamaModel = models[0].name;
                }
            } catch { serverMetrics.ollamaReachable = false; }
        });
    });
    req.on('error', () => { serverMetrics.ollamaReachable = false; });
    req.on('timeout', () => { req.destroy(); serverMetrics.ollamaReachable = false; });
}
probeOllamaReachability();
setInterval(probeOllamaReachability, 30000);

// Wrap handlers to count
const origListeners = server.listeners('request');
server.removeAllListeners('request');
server.on('request', (req, res) => {
    serverMetrics.requests++;
    const url = new URL(req.url, `http://${req.headers.host}`);

    // Apply the same hardening even if we short-circuit in this wrapper.
    applySecurityHeaders(req, res);
    applyCors(req, res);
    enforceBodySizeLimit(req, res);
    if (!rateLimit(req, res, url.pathname)) return;

    // Track per-endpoint hits
    serverMetrics.endpointHits[url.pathname] = (serverMetrics.endpointHits[url.pathname] || 0) + 1;

    // Track generate calls for real metrics
    if (url.pathname === '/api/generate' || url.pathname === '/v1/chat/completions' || url.pathname === '/ask') {
        serverMetrics.generates++;
    }

    // ── Shutdown endpoint — allows the launcher to gracefully kill the backend ──
    if (url.pathname === '/api/shutdown' && req.method === 'POST') {
        // Loopback-only safety.
        if (!isLoopbackIp(getClientIp(req))) {
            res.writeHead(403, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Forbidden' }));
            return;
        }
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'shutting_down', message: 'Server will exit in 1s' }));
        console.log('🛑 Shutdown requested via /api/shutdown');
        setTimeout(() => {
            if (phase3Dll && agentContext && !agentContext.isNull()) {
                try { phase3Dll.Phase3Shutdown(agentContext); } catch (_) { }
            }
            process.exit(0);
        }, 1000);
        return;
    }

    // Metrics endpoint — all fields are real, computed from runtime state
    if (url.pathname === '/api/metrics' || url.pathname === '/metrics') {
        const mem = process.memoryUsage();
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            uptimeMs: Date.now() - serverMetrics.startTime,
            requests: serverMetrics.requests,
            generates: serverMetrics.generates,
            errors: serverMetrics.errors,
            dllLoaded: !!phase3Dll,
            kernelReady: !!(agentContext && !agentContext?.isNull?.()),
            memUsedMB: Math.round(mem.rss / 1048576),
            heapUsedMB: Math.round(mem.heapUsed / 1048576),
            heapTotalMB: Math.round(mem.heapTotal / 1048576),
            tokensPerSec: serverMetrics.lastTps,
            peakTps: serverMetrics.peakTps,
            totalTokens: serverMetrics.totalTokens,
            avgLatencyMs: serverMetrics.latencyCount > 0
                ? Math.round(serverMetrics.latencySum / serverMetrics.latencyCount)
                : 0,
            lastGenerateMs: serverMetrics.lastGenerateMs,
            ollamaReachable: serverMetrics.ollamaReachable,
            modelCount: serverMetrics.modelCount,
            endpointHits: serverMetrics.endpointHits,
        }));
        return;
    }
    origListeners.forEach(fn => fn.call(server, req, res));
});

// Always start the server — DLL is optional, fallback to Ollama/model-server
const dllAvailable = buildDllIfNeeded();
let kernelLoaded = false;
if (dllAvailable) {
    kernelLoaded = initializeKernel();
}

server.listen(PORT, () => {
    if (kernelLoaded) {
        console.log(`RawrXD IDE Server on http://localhost:${PORT}`);
        console.log(`  Phase-3 Kernel: loaded  |  Model-server proxy: :${MODEL_SERVER_PORT}`);
        console.log(`  Endpoints: /api/models (real scan)  /api/generate (DLL→Ollama→model-server chain)`);
    } else {
        console.log(`RawrXD IDE Server on http://localhost:${PORT} (DLL unavailable — Ollama/model-server fallback)`);
        console.log(`  Endpoints: /api/models  /api/generate  /api/metrics`);
    }
    console.log(`  Metrics: /api/metrics`);
});

// ── /api/read-file — read a local file (loopback-only) ──
function handleReadFileRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Method not allowed' }));
        return;
    }

    // Only allow loopback clients to read local files.
    const ip = getClientIp(req);
    if (!isLoopbackIp(ip)) {
        res.writeHead(403, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Forbidden' }));
        return;
    }

    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        let parsed;
        try { parsed = body ? JSON.parse(body) : {}; } catch (e) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Invalid JSON body', detail: e.message }));
            return;
        }

        const rawPath = String(parsed.path || '').trim();
        if (!rawPath) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'path is required' }));
            return;
        }

        // Reject UNC/network paths explicitly (test_jumper expects 403)
        if (rawPath.startsWith('\\\\') || rawPath.startsWith('//')) {
            res.writeHead(403, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Network paths are not allowed' }));
            return;
        }

        // Normalize. Allow absolute local paths, otherwise resolve relative to repo root.
        const normalizedInput = rawPath.replace(/\//g, path.sep);
        const resolved = path.isAbsolute(normalizedInput)
            ? path.resolve(normalizedInput)
            : path.resolve(__dirname, normalizedInput);

        // Basic sanity: reject device paths.
        const lower = resolved.toLowerCase();
        if (lower.startsWith('\\\\.\\') || lower.startsWith('\\\\?\\')) {
            res.writeHead(403, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Device paths are not allowed' }));
            return;
        }

        fs.stat(resolved, (err, st) => {
            if (err) {
                res.writeHead(404, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'File not found', path: rawPath }));
                return;
            }
            if (!st.isFile()) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Path is not a file', path: rawPath }));
                return;
            }
            if (st.size > MAX_READ_FILE_BYTES) {
                res.writeHead(413, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'File too large', size: st.size, limitBytes: MAX_READ_FILE_BYTES }));
                return;
            }

            fs.readFile(resolved, 'utf8', (readErr, data) => {
                if (readErr) {
                    res.writeHead(500, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Read failed', detail: readErr.message }));
                    return;
                }
                // Include a lightweight integrity hash to support audits.
                const sha256 = crypto.createHash('sha256').update(data, 'utf8').digest('hex');
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    path: rawPath,
                    resolved,
                    size: st.size,
                    sha256,
                    content: data,
                }));
            });
        });
    });
}

// ── /api/list-dir — directory listing for IDE file tree (loopback-only) ──
function handleListDirRequest(req, res) {
    if (req.method !== 'POST') {
        res.writeHead(405, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Method not allowed' }));
        return;
    }

    const ip = getClientIp(req);
    if (!isLoopbackIp(ip)) {
        res.writeHead(403, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Forbidden' }));
        return;
    }

    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        try {
            const parsed = body ? JSON.parse(body) : {};
            const reqPath = parsed.path || '.';

            // Reject UNC/network paths explicitly.
            const p = String(reqPath);
            if (p.startsWith('\\\\') || p.startsWith('//')) {
                res.writeHead(403, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Network paths are not allowed' }));
                return;
            }

            // Resolve relative to project root, prevent traversal above it.
            const resolved = path.resolve(__dirname, p);
            if (!resolved.startsWith(__dirname)) {
                res.writeHead(403, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Path traversal blocked' }));
                return;
            }

            fs.readdir(resolved, { withFileTypes: true }, (err, dirents) => {
                if (err) {
                    res.writeHead(404, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({ error: 'Directory not found: ' + reqPath, detail: err.message }));
                    return;
                }
                const entries = dirents
                    .filter(d => !d.name.startsWith('.'))
                    .map(d => {
                        const fullPath = path.join(resolved, d.name);
                        const isDir = d.isDirectory();
                        const entry = {
                            name: d.name,
                            type: isDir ? 'directory' : 'file',
                            path: path.relative(__dirname, fullPath).replace(/\\/g, '/')
                        };
                        if (!isDir) {
                            try {
                                const stat = fs.statSync(fullPath);
                                entry.size = stat.size;
                                entry.modified = stat.mtime.toISOString();
                            } catch (_) { }
                        }
                        return entry;
                    })
                    .sort((a, b) => {
                        if (a.type !== b.type) return a.type === 'directory' ? -1 : 1;
                        return a.name.localeCompare(b.name);
                    });
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ entries, path: reqPath, count: entries.length }));
            });
        } catch (e) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Invalid JSON body', detail: e.message }));
        }
    });
}

// ── /api/cli (/api/build, /api/deploy) — allowlisted command execution (loopback-only) ──
function handleCliRequest(req, res, endpoint) {
    if (req.method !== 'POST') {
        res.writeHead(405, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Method not allowed' }));
        return;
    }

    const ip = getClientIp(req);
    if (!isLoopbackIp(ip)) {
        res.writeHead(403, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Forbidden' }));
        return;
    }

    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', () => {
        try {
            const parsed = body ? JSON.parse(body) : {};
            const command = parsed.command || '';
            if (!command) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'No command provided' }));
                return;
            }

            // Default-deny chaining / redirection unless explicitly opted out.
            const allowUnsafe = String(process.env.RAWRXD_ALLOW_UNSAFE_CLI || '').trim() === '1';
            if (!allowUnsafe && /[\r\n&|;<>]/.test(command)) {
                res.writeHead(403, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Command contains forbidden shell metacharacters', endpoint }));
                return;
            }

            // Restrict to safe commands (build-related only)
            const allowedPrefixes = [
                'cmake', 'msbuild', 'ninja', 'nmake', 'cl ', 'ml64', 'link', 'lib ',
                'node ', 'npm ', 'git ', 'dir ', 'type ', 'echo ', 'deploy', 'build'
            ];
            const cmdLower = command.toLowerCase().trim();
            const isSafe = allowedPrefixes.some(p => cmdLower.startsWith(p));
            if (!isSafe) {
                res.writeHead(403, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Command not in allowlist', command, endpoint }));
                return;
            }

            const { exec } = require('child_process');
            exec(command, { cwd: __dirname, timeout: 60000, maxBuffer: 1024 * 1024 }, (err, stdout, stderr) => {
                const exitCode = err ? (err.code || 1) : 0;
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    success: exitCode === 0,
                    exitCode,
                    stdout: stdout || '',
                    stderr: stderr || '',
                    command,
                    endpoint,
                    duration: 0
                }));
            });
        } catch (e) {
            res.writeHead(400, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Invalid request', detail: e.message }));
        }
    });
}