const http = require('http');
const url = require('url');

// BigDaddyG Assembly Model Server
// This server provides AI model services for the BigDaddyG Assembly project

const PORT = process.env.MODEL_PORT_ASSEMBLY || 11441;
const HOST = 'localhost';

// Simple in-memory storage for conversation history
let conversationHistory = [];

// CORS headers for web requests
const corsHeaders = {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    'Content-Type': 'application/json'
};

// Health check endpoint
const healthCheck = (res) => {
    res.writeHead(200, corsHeaders);
    res.end(JSON.stringify({ 
        status: 'healthy', 
        service: 'BigDaddyG Assembly Model Server',
        port: PORT,
        timestamp: new Date().toISOString()
    }));
};

// Chat completion endpoint
const handleChatCompletion = (req, res) => {
    let body = '';
    
    req.on('data', chunk => {
        body += chunk.toString();
    });
    
    req.on('end', () => {
        try {
            const data = JSON.parse(body);
            const { messages, model = 'bigdaddyg-assembly' } = data;
            
            // Add to conversation history
            conversationHistory.push(...messages);
            
            // Simple response generation (replace with actual AI model)
            const lastMessage = messages[messages.length - 1];
            const response = generateResponse(lastMessage.content);
            
            const completion = {
                id: `chatcmpl-${Date.now()}`,
                object: 'chat.completion',
                created: Math.floor(Date.now() / 1000),
                model: model,
                choices: [{
                    index: 0,
                    message: {
                        role: 'assistant',
                        content: response
                    },
                    finish_reason: 'stop'
                }],
                usage: {
                    prompt_tokens: 0,
                    completion_tokens: 0,
                    total_tokens: 0
                }
            };
            
            res.writeHead(200, corsHeaders);
            res.end(JSON.stringify(completion));
            
        } catch (error) {
            res.writeHead(400, corsHeaders);
            res.end(JSON.stringify({ error: 'Invalid JSON in request body' }));
        }
    });
};

// Simple response generation (replace with actual AI model)
const generateResponse = (input) => {
    const responses = [
        `I understand you're asking about: "${input}". This is a response from the BigDaddyG Assembly model server.`,
        `Assembly language is powerful for low-level programming. Your question "${input}" is noted.`,
        `The BigDaddyG project is designed for efficient assembly programming. Regarding "${input}", here's my response.`,
        `Assembly programming requires precision. Your query "${input}" has been processed by the BigDaddyG model.`
    ];
    
    return responses[Math.floor(Math.random() * responses.length)];
};

// Main server handler
const server = http.createServer((req, res) => {
    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const method = req.method;
    
    // Handle CORS preflight
    if (method === 'OPTIONS') {
        res.writeHead(200, corsHeaders);
        res.end();
        return;
    }
    
    // Route handling
    switch (path) {
        case '/health':
        case '/status':
            healthCheck(res);
            break;
            
        case '/v1/chat/completions':
            if (method === 'POST') {
                handleChatCompletion(req, res);
            } else {
                res.writeHead(405, corsHeaders);
                res.end(JSON.stringify({ error: 'Method not allowed' }));
            }
            break;
            
        case '/':
            res.writeHead(200, corsHeaders);
            res.end(JSON.stringify({ 
                message: 'BigDaddyG Assembly Model Server',
                version: '1.0.0',
                endpoints: ['/health', '/v1/chat/completions']
            }));
            break;
            
        default:
            res.writeHead(404, corsHeaders);
            res.end(JSON.stringify({ error: 'Not found' }));
    }
});

// Error handling
server.on('error', (err) => {
    console.error('Server error:', err);
});

// Start server
server.listen(PORT, HOST, () => {
    console.log(`BigDaddyG Assembly Model Server running on http://${HOST}:${PORT}`);
    console.log('Available endpoints:');
    console.log('  GET  /health - Health check');
    console.log('  POST /v1/chat/completions - Chat completions');
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\nShutting down BigDaddyG Assembly Model Server...');
    server.close(() => {
        console.log('Server closed');
        process.exit(0);
    });
});

module.exports = server;
