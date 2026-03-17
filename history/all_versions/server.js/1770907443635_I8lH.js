const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const ffi = require('ffi-napi');
const ref = require('ref-napi');

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
    // Enable CORS
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }

    const url = new URL(req.url, `http://${req.headers.host}`);

    // ── Serve GUI pages ──
    const GUI_ROUTES = {
        '/':                    'launcher.html',
        '/launcher':            'launcher.html',
        '/chatbot':             'ide_chatbot.html',
        '/agents':              'agents.html',
        '/multiwindow':         'multiwindow_ide.html',
        '/feature-tester':      'feature_tester.html',
        '/bruteforce':          'model_bruteforce.html',
        '/test-jumper':         'test_jumper.html',
        '/test-swarm':          'test_swarm.html',
        '/chatbot-standalone':  'ide_chatbot_standalone.html',
        '/chatbot-win32':       'ide_chatbot_win32.html',
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
            version: '7.4.0',
            backend: 'rawrxd-win32ide',
            port: PORT,
            dllLoaded: !!phase3Dll,
            kernelReady: !!(agentContext && !agentContext?.isNull?.()),
            uptime: Math.round((Date.now() - serverMetrics.startTime) / 1000)
        }));
        return;
    }
    if (url.pathname === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'ok', version: '7.4.0', models_loaded: true }));
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
                const ollamaBody = JSON.stringify({
                    model: 'llama2',
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
                            res.writeHead(200, { 'Content-Type': 'application/json' });
                            res.end(JSON.stringify({
                                response: parsed.response || '',
                                model: parsed.model || 'ollama',
                                evalCount: parsed.eval_count || 0,
                                tokensPerSec: parsed.eval_count && parsed.eval_duration
                                    ? (parsed.eval_count / (parsed.eval_duration / 1e9)).toFixed(1)
                                    : '0',
                                totalDurationMs: (parsed.total_duration || 0) / 1e6
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
    const ollamaBody = JSON.stringify({
        model: model || 'llama2', prompt, stream: false,
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
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({
                    answer: p.response || '',
                    response: p.response || '',
                    model: p.model || 'ollama',
                    evalCount: p.eval_count || 0,
                    tokensPerSec: p.eval_count && p.eval_duration
                        ? (p.eval_count / (p.eval_duration / 1e9)).toFixed(1) : '0',
                    totalDurationMs: (p.total_duration || 0) / 1e6
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

// Auto-build DLL if missing
function buildDllIfNeeded() {
    const dllPath = path.join(__dirname, 'bin', 'Phase3_Agent_Kernel.dll');
    if (!fs.existsSync(dllPath)) {
        console.log('🔧 DLL not found, building...');

        const buildScript = path.join(__dirname, 'build_phase3.bat');
        const build = spawn('cmd', ['/c', buildScript], {
            stdio: 'inherit',
            cwd: __dirname
        });

        build.on('close', (code) => {
            if (code === 0) {
                console.log('✅ Build successful, restarting server...');
                process.exit(0); // Restart to load new DLL
            } else {
                console.log('❌ Build failed with code:', code);
            }
        });

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
const serverMetrics = { requests: 0, generates: 0, errors: 0, startTime: Date.now() };

// Wrap handlers to count
const origListeners = server.listeners('request');
server.removeAllListeners('request');
server.on('request', (req, res) => {
    serverMetrics.requests++;
    // Metrics endpoint
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (url.pathname === '/api/metrics' || url.pathname === '/metrics') {
        res.setHeader('Access-Control-Allow-Origin', '*');
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            uptimeMs: Date.now() - serverMetrics.startTime,
            requests: serverMetrics.requests,
            dllLoaded: !!phase3Dll,
            kernelReady: !!(agentContext && !agentContext?.isNull?.()),
            memUsedMB: Math.round(process.memoryUsage().rss / 1048576)
        }));
        return;
    }
    origListeners.forEach(fn => fn.call(server, req, res));
});

if (buildDllIfNeeded()) {
    if (initializeKernel()) {
        server.listen(PORT, () => {
            console.log(`RawrXD IDE Server on http://localhost:${PORT}`);
            console.log(`  Phase-3 Kernel: loaded  |  Model-server proxy: :${MODEL_SERVER_PORT}`);
            console.log(`  Endpoints: /api/models (real scan)  /api/generate (DLL→Ollama→model-server chain)`);
            console.log(`  Metrics: /api/metrics`);
        });
    } else {
        server.listen(PORT, () => {
            console.log(`RawrXD IDE Server on http://localhost:${PORT} (DLL unavailable — Ollama/model-server fallback)`);
            console.log(`  Endpoints: /api/models  /api/generate  /api/metrics`);
        });
    }
}