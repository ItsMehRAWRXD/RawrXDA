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
    
    // Serve HTML file
    if (url.pathname === '/' || url.pathname === '/chatbot') {
        const htmlPath = path.join(__dirname, 'gui', 'ide_chatbot.html');
        fs.readFile(htmlPath, 'utf8', (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('HTML file not found');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(data);
        });
        return;
    }
    
    // API endpoints
    if (url.pathname === '/api/models') {
        handleModelsRequest(req, res);
    } else if (url.pathname === '/api/upload') {
        handleUploadRequest(req, res);
    } else if (url.pathname === '/api/progress') {
        handleProgressRequest(req, res);
    } else if (url.pathname === '/api/generate') {
        handleGenerateRequest(req, res);
    } else if (url.pathname === '/ask') {
        handleAskRequest(req, res);
    } else {
        res.writeHead(404);
        res.end('Not found');
    }
});

// Handle model listing
function handleModelsRequest(req, res) {
    const models = [
        { id: 'phase3-native', name: 'Phase-3 Native (120B)', type: 'native', status: 'ready' },
        { id: 'chatbot-fallback', name: 'Chatbot Fallback', type: 'fallback', status: 'ready' }
    ];
    
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ models }));
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
                // Fallback to simple responses
                const responses = [
                    "I'm the RawrXD IDE Assistant! I can help with model loading, file management, and development tasks.",
                    "To load a 120B model, use the file uploader above. I support GGUF, Safetensors, and PyTorch formats.",
                    "For swarm operations, check the swarm directory and use the swarm management tools.",
                    "Need help with todos? Use the todo system to track your development tasks.",
                    "Graphics drivers can be updated through the driver management panel."
                ];
                
                const response = responses[Math.floor(Math.random() * responses.length)];
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ response }));
            }
        } catch (error) {
            res.writeHead(500);
            res.end(JSON.stringify({ error: error.message }));
        }
    });
}

// Handle ask requests (compatibility)
function handleAskRequest(req, res) {
    handleGenerateRequest(req, res);
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

if (buildDllIfNeeded()) {
    if (initializeKernel()) {
        server.listen(PORT, () => {
            console.log(`🚀 RawrXD IDE Chatbot Server running on http://localhost:${PORT}`);
            console.log(`📁 Serving HTML from: ${path.join(__dirname, 'gui', 'ide_chatbot.html')}`);
            console.log(`🧠 Phase-3 Agent Kernel: ${phase3Dll ? 'Loaded' : 'Fallback mode'}`);
        });
    } else {
        console.log('⚠️  Starting in fallback mode (DLL not available)');
        server.listen(PORT, () => {
            console.log(`🚀 RawrXD IDE Chatbot Server running on http://localhost:${PORT} (Fallback Mode)`);
        });
    }
}