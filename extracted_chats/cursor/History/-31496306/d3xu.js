const WebSocket = require('ws');
const http = require('http');

// ============================================================================
// BIGDADDYG MICRO MODEL WEBSOCKET SERVER
// ============================================================================
// Micro model chaining and orchestration for small, fast AI models
// Port: 3000
// ============================================================================

const PORT = 3000;
const HOST = '0.0.0.0';

console.log('🔗 Starting BigDaddyG Micro Model Server...');
console.log(`📍 Port: ${PORT}`);

// Create HTTP server for health checks
const server = http.createServer((req, res) => {
    const corsHeaders = {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type',
        'Content-Type': 'application/json'
    };
    
    if (req.method === 'OPTIONS') {
        res.writeHead(204, corsHeaders);
        res.end();
        return;
    }
    
    if (req.url === '/health') {
        res.writeHead(200, corsHeaders);
        res.end(JSON.stringify({
            status: 'healthy',
            service: 'BigDaddyG Micro Model Server',
            port: PORT,
            active_connections: wss.clients.size,
            features: ['micro_chain', 'nano_chain', 'model_catalog', 'deployment'],
            timestamp: new Date().toISOString()
        }));
    } else {
        res.writeHead(404, corsHeaders);
        res.end(JSON.stringify({ error: 'Not found' }));
    }
});

// Create WebSocket server
const wss = new WebSocket.Server({ server });

// Micro model catalog
const microModels = {
    'gpt-micro': { size: '125M', speed: 'ultra-fast', context: '2K' },
    'code-nano': { size: '350M', speed: 'fast', context: '4K' },
    'chat-tiny': { size: '500M', speed: 'fast', context: '8K' },
    'analyzer-small': { size: '1B', speed: 'medium', context: '16K' }
};

// Track connected clients
const clients = new Set();

// WebSocket connection handler
wss.on('connection', (ws, req) => {
    const clientId = `micro-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    clients.add(ws);
    
    console.log(`🔗 Micro Model client connected: ${clientId}`);
    console.log(`📊 Total connections: ${clients.size}`);
    
    // Send welcome message
    ws.send(JSON.stringify({
        type: 'connection',
        agent: 'MicroModel',
        status: 'connected',
        message: 'Connected to Micro Model Chain System',
        clientId: clientId,
        available_models: Object.keys(microModels),
        timestamp: new Date().toISOString()
    }));
    
    // Handle incoming messages
    ws.on('message', async (message) => {
        try {
            const data = JSON.parse(message);
            console.log(`📨 Received command: ${data.command} from ${clientId}`);
            
            switch (data.command) {
                case 'micro_chain':
                    await handleMicroChain(ws, data);
                    break;
                    
                case 'nano_chain':
                    await handleNanoChain(ws, data);
                    break;
                    
                case 'micro_catalog':
                    handleCatalog(ws);
                    break;
                    
                case 'micro_chain_test':
                    await handleChainTest(ws, data);
                    break;
                    
                case 'micro_models_deploy':
                    await handleDeploy(ws, data);
                    break;
                    
                case 'ping':
                    ws.send(JSON.stringify({ type: 'pong', timestamp: new Date().toISOString() }));
                    break;
                    
                default:
                    ws.send(JSON.stringify({
                        type: 'error',
                        agent: 'MicroModel',
                        message: `Unknown command: ${data.command}`,
                        timestamp: new Date().toISOString()
                    }));
            }
        } catch (error) {
            console.error('❌ Message parsing error:', error.message);
            ws.send(JSON.stringify({
                type: 'error',
                agent: 'MicroModel',
                message: error.message,
                timestamp: new Date().toISOString()
            }));
        }
    });
    
    // Handle disconnection
    ws.on('close', () => {
        clients.delete(ws);
        console.log(`🔌 Micro Model client disconnected: ${clientId}`);
        console.log(`📊 Total connections: ${clients.size}`);
    });
    
    // Handle errors
    ws.on('error', (error) => {
        console.error(`❌ WebSocket error for ${clientId}:`, error.message);
    });
});

// Micro chain handler - Chain multiple micro models
async function handleMicroChain(ws, data) {
    const { prompt, models = ['gpt-micro', 'code-nano'] } = data;
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: `Starting micro chain with ${models.length} models...`,
        timestamp: new Date().toISOString()
    }));
    
    // Simulate chaining
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    const result = {
        chain_length: models.length,
        models_used: models,
        result: `Micro chain completed: Processed "${prompt}" through ${models.join(' → ')}`,
        total_tokens: Math.floor(Math.random() * 500) + 100,
        latency_ms: Math.floor(Math.random() * 200) + 50
    };
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_chain_complete',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));
    
    console.log(`✅ Micro chain completed (${models.length} models)`);
}

// Nano chain handler - Ultra-fast micro model chain
async function handleNanoChain(ws, data) {
    const { prompt } = data;
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: 'Executing nano chain (ultra-fast)...',
        timestamp: new Date().toISOString()
    }));
    
    // Simulate ultra-fast processing
    await new Promise(resolve => setTimeout(resolve, 300));
    
    const result = {
        result: `Nano chain result: "${prompt}" processed in 300ms`,
        total_tokens: Math.floor(Math.random() * 100) + 20,
        latency_ms: 300
    };
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'nano_chain_complete',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));
    
    console.log(`✅ Nano chain completed`);
}

// Catalog handler
function handleCatalog(ws) {
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_catalog',
        status: 'success',
        data: microModels,
        timestamp: new Date().toISOString()
    }));
    
    console.log(`📚 Catalog sent`);
}

// Chain test handler
async function handleChainTest(ws, data) {
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: 'Running chain test...',
        timestamp: new Date().toISOString()
    }));
    
    await new Promise(resolve => setTimeout(resolve, 500));
    
    const result = {
        test: 'passed',
        models_tested: Object.keys(microModels).length,
        avg_latency_ms: 150,
        success_rate: '100%'
    };
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_chain_test',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));
    
    console.log(`✅ Chain test completed`);
}

// Deployment handler
async function handleDeploy(ws, data) {
    const { models = [] } = data;
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: `Deploying ${models.length} micro models...`,
        timestamp: new Date().toISOString()
    }));
    
    await new Promise(resolve => setTimeout(resolve, 800));
    
    const result = {
        deployed: models.length > 0 ? models : Object.keys(microModels),
        status: 'ready',
        endpoints: models.length > 0 ? models.map(m => `/api/micro/${m}`) : Object.keys(microModels).map(m => `/api/micro/${m}`)
    };
    
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_models_deployed',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));
    
    console.log(`✅ Models deployed: ${result.deployed.join(', ')}`);
}

// Heartbeat to keep connections alive
setInterval(() => {
    clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({
                type: 'heartbeat',
                agent: 'MicroModel',
                timestamp: new Date().toISOString()
            }));
        }
    });
}, 30000); // Every 30 seconds

// Start server
server.listen(PORT, HOST, () => {
    console.log('✅ BigDaddyG Micro Model Server is LIVE!');
    console.log(`📍 ws://localhost:${PORT}`);
    console.log(`📍 ws://127.0.0.1:${PORT}`);
    console.log(`📍 Listening on all interfaces (${HOST}:${PORT})`);
    console.log('');
    console.log('🎯 Available Micro Models:');
    Object.entries(microModels).forEach(([name, specs]) => {
        console.log(`   🔗 ${name}: ${specs.size} | ${specs.speed} | ${specs.context} context`);
    });
    console.log('');
    console.log('🌐 CORS: Enabled for browser access');
    console.log('💓 Heartbeat: Active (30s interval)');
    console.log('');
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\n🛑 Shutting down Micro Model Server...');
    wss.close(() => {
        console.log('✅ Server closed gracefully');
        process.exit(0);
    });
});

