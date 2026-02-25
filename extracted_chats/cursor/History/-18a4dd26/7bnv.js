const http = require('http');
const https = require('https');

// ============================================================================
// BIGDADDYG ORCHESTRA SERVER
// ============================================================================
// Your own Ollama-like server that connects to Ollama behind the scenes
// but provides a unified API for all your BigDaddyG models
//
// Port: 11441
// Compatible with: OpenAI API, Ollama API
// ============================================================================

const PORT = process.env.MODEL_PORT_ASSEMBLY || 11441;
const HOST = 'localhost';
const OLLAMA_URL = 'http://localhost:11434';

console.log('🎼 Starting BigDaddyG Orchestra Server...');
console.log(`📍 Port: ${PORT}`);
console.log(`🔗 Ollama backend: ${OLLAMA_URL}`);

// CORS headers
const corsHeaders = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    'Content-Type': 'application/json'
};

// Health check
const healthCheck = (res) => {
    res.writeHead(200, corsHeaders);
    res.end(JSON.stringify({ 
        status: 'healthy', 
        service: 'BigDaddyG Orchestra Server',
        port: PORT,
        backend: OLLAMA_URL,
        models: ['BigDaddyG:Latest', 'BigDaddyG:Code', 'BigDaddyG:Debug', 'BigDaddyG:Crypto'],
        timestamp: new Date().toISOString()
    }));
};

// Query Ollama backend
async function queryOllama(model, prompt, options = {}) {
    return new Promise((resolve, reject) => {
        const data = JSON.stringify({
            model: model,
            prompt: prompt,
            stream: false,
            options: {
                temperature: options.temperature || 0.7,
                top_p: options.top_p || 0.9,
                top_k: options.top_k || 40,
                num_predict: options.num_predict || 2048
            }
        });

        const req = http.request({
            hostname: 'localhost',
            port: 11434,
            path: '/api/generate',
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Content-Length': data.length
            }
        }, (res) => {
            let body = '';
            res.on('data', chunk => body += chunk);
            res.on('end', () => {
                try {
                    const result = JSON.parse(body);
                    resolve(result);
                } catch (e) {
                    reject(new Error('Failed to parse Ollama response'));
                }
            });
        });

        req.on('error', (e) => {
            reject(new Error(`Ollama connection failed: ${e.message}`));
        });

        req.write(data);
        req.end();
    });
}

// OpenAI-compatible chat completion endpoint
const handleChatCompletion = async (req, res) => {
    let body = '';
    
    req.on('data', chunk => {
        body += chunk.toString();
    });
    
    req.on('end', async () => {
        try {
            const data = JSON.parse(body);
            const { messages, model = 'BigDaddyG:Latest', temperature, max_tokens } = data;
            
            console.log(`[Request] Model: ${model} | Messages: ${messages.length}`);
            
            // Extract the actual prompt from messages
            const lastMessage = messages[messages.length - 1];
            const prompt = messages.map(m => {
                if (m.role === 'system') return `System: ${m.content}`;
                if (m.role === 'user') return `User: ${m.content}`;
                if (m.role === 'assistant') return `Assistant: ${m.content}`;
                return m.content;
            }).join('\n\n');
            
            // Map BigDaddyG models to Ollama models
            const modelMap = {
                'BigDaddyG:Latest': 'bigdaddyg:latest',
                'BigDaddyG:Code': 'bigdaddyg:latest',  // Use same model, different system prompt
                'BigDaddyG:Debug': 'bigdaddyg:latest',
                'BigDaddyG:Crypto': 'bigdaddyg:latest'
            };
            
            const ollamaModel = modelMap[model] || 'bigdaddyg:latest';
            
            // Add model-specific system prompts
            let enhancedPrompt = prompt;
            if (model === 'BigDaddyG:Code') {
                enhancedPrompt = `You are a code generation specialist. Generate clean, working code with explanations.\n\n${prompt}`;
            } else if (model === 'BigDaddyG:Debug') {
                enhancedPrompt = `You are a debugging expert. Analyze errors and provide solutions.\n\n${prompt}`;
            } else if (model === 'BigDaddyG:Crypto') {
                enhancedPrompt = `You are a security and encryption specialist. Provide secure code examples.\n\n${prompt}`;
            }
            
            console.log(`[Ollama] Querying ${ollamaModel}...`);
            
            // Query Ollama with the REAL model
            const result = await queryOllama(ollamaModel, enhancedPrompt, {
                temperature: temperature || 0.7,
                num_predict: max_tokens || 2048
            });
            
            console.log(`[Response] Generated ${result.eval_count || 0} tokens`);
            
            // Return OpenAI-compatible response
            const completion = {
                id: `chatcmpl-${Date.now()}`,
                object: 'chat.completion',
                created: Math.floor(Date.now() / 1000),
                model: model,
                choices: [{
                    index: 0,
                    message: {
                        role: 'assistant',
                        content: result.response
                    },
                    finish_reason: 'stop'
                }],
                usage: {
                    prompt_tokens: result.prompt_eval_count || 0,
                    completion_tokens: result.eval_count || 0,
                    total_tokens: (result.prompt_eval_count || 0) + (result.eval_count || 0)
                }
            };
            
            res.writeHead(200, corsHeaders);
            res.end(JSON.stringify(completion));
            
        } catch (error) {
            console.error('[Error]', error.message);
            res.writeHead(500, corsHeaders);
            res.end(JSON.stringify({ 
                error: {
                    message: error.message,
                    type: 'server_error'
                }
            }));
        }
    });
};

// List available models
const listModels = (res) => {
    const models = {
        object: 'list',
        data: [
            {
                id: 'BigDaddyG:Latest',
                object: 'model',
                created: Date.now(),
                owned_by: 'bigdaddyg',
                permission: [],
                root: 'bigdaddyg:latest',
                parent: null
            },
            {
                id: 'BigDaddyG:Code',
                object: 'model',
                created: Date.now(),
                owned_by: 'bigdaddyg',
                permission: [],
                root: 'bigdaddyg:latest',
                parent: null
            },
            {
                id: 'BigDaddyG:Debug',
                object: 'model',
                created: Date.now(),
                owned_by: 'bigdaddyg',
                permission: [],
                root: 'bigdaddyg:latest',
                parent: null
            },
            {
                id: 'BigDaddyG:Crypto',
                object: 'model',
                created: Date.now(),
                owned_by: 'bigdaddyg',
                permission: [],
                root: 'bigdaddyg:latest',
                parent: null
            }
        ]
    };
    
    res.writeHead(200, corsHeaders);
    res.end(JSON.stringify(models));
};

// Main server
const server = http.createServer((req, res) => {
    const { pathname, query } = new URL(req.url, `http://${req.headers.host}`);
    const method = req.method;
    
    // Handle CORS preflight
    if (method === 'OPTIONS') {
        res.writeHead(204, corsHeaders);
        res.end();
        return;
    }
    
    // Route requests
    if (pathname === '/health' && method === 'GET') {
        healthCheck(res);
    } else if (pathname === '/v1/chat/completions' && method === 'POST') {
        handleChatCompletion(req, res);
    } else if (pathname === '/v1/models' && method === 'GET') {
        listModels(res);
    } else if (pathname === '/api/tags' && method === 'GET') {
        // Ollama-compatible endpoint
        listModels(res);
    } else if (pathname === '/' && method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(`
            <html>
            <head><title>BigDaddyG Orchestra Server</title></head>
            <body style="font-family: monospace; background: #1e1e1e; color: #0098ff; padding: 20px;">
                <h1>🎼 BigDaddyG Orchestra Server</h1>
                <p>Status: <span style="color: #4ec9b0;">✅ Running</span></p>
                <p>Port: ${PORT}</p>
                <p>Backend: ${OLLAMA_URL}</p>
                <h2>Available Models:</h2>
                <ul>
                    <li>BigDaddyG:Latest - General AI (4.7GB)</li>
                    <li>BigDaddyG:Code - Code Generation</li>
                    <li>BigDaddyG:Debug - Debugging Expert</li>
                    <li>BigDaddyG:Crypto - Security Specialist</li>
                </ul>
                <h2>Endpoints:</h2>
                <ul>
                    <li>GET /health - Health check</li>
                    <li>POST /v1/chat/completions - OpenAI-compatible chat</li>
                    <li>GET /v1/models - List models</li>
                    <li>GET /api/tags - Ollama-compatible model list</li>
                </ul>
                <p style="color: #858585; margin-top: 40px;">
                    Powered by Ollama • Your own AI orchestra
                </p>
            </body>
            </html>
        `);
    } else {
        res.writeHead(404, corsHeaders);
        res.end(JSON.stringify({ error: 'Not found' }));
    }
});

server.listen(PORT, HOST, () => {
    console.log('');
    console.log('✅ BigDaddyG Orchestra Server is running!');
    console.log(`📍 http://${HOST}:${PORT}`);
    console.log('');
    console.log('🎯 Available endpoints:');
    console.log(`   GET  http://${HOST}:${PORT}/health`);
    console.log(`   POST http://${HOST}:${PORT}/v1/chat/completions`);
    console.log(`   GET  http://${HOST}:${PORT}/v1/models`);
    console.log('');
    console.log('🧠 Models: BigDaddyG:Latest, Code, Debug, Crypto');
    console.log('🔗 Backend: Ollama (loading REAL 4.7GB model)');
    console.log('');
    console.log('💡 Now start your IDE and it will use this server!');
    console.log('');
});

