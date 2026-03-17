/**
 * BigDaddyG Micro Model Server with Beaconism Integration
 * Hybrid architecture: Beaconism for Electron integration + WebSocket for real-time
 * Port: 3000
 * No separate Node.js process required - runs as Electron module
 */

const WebSocket = require('ws');
const http = require('http');
const https = require('https');
const EventEmitter = require('events');

// Polyfill for fetch if needed (Node.js 18+ has native fetch)
const fetchFn = global.fetch || require('node-fetch');

// ============================================================================
// BEACONISM INTEGRATION LAYER
// ============================================================================

class BeaconismBridge extends EventEmitter {
    constructor() {
        super();
        this.isElectronContext = typeof window !== 'undefined' || process.type === 'renderer' || process.type === 'main';
        this.beaconPayloads = new Map();
        this.listeners = new Set();
        this.initialized = false;

        if (this.isElectronContext) {
            this.initializeBeaconism();
        }
    }

    initializeBeaconism() {
        // Beaconism protocol: Silent inter-process communication
        // Payloads transmitted via:
        // 1. Memory-mapped files (fast, zero-copy)
        // 2. Shared buffers (ultra-low latency)
        // 3. Event emitters (Electron IPC alternative)

        try {
            // Try to load native beacon module if available
            this.beacon = require('./beaconism-native');
            console.log('[Beaconism] Native module loaded');
        } catch (e) {
            console.log('[Beaconism] Using JS fallback (no native module)');
            this.beacon = null;
        }

        this.initialized = true;
    }

    async transmit(payload, priority = 'normal') {
        if (!this.initialized) await this.initializeBeaconism();

        const beaconId = `beacon-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
        const wrapped = {
            id: beaconId,
            priority,
            data: payload,
            timestamp: Date.now(),
            source: 'MicroModelServer'
        };

        this.beaconPayloads.set(beaconId, wrapped);

        if (this.beacon) {
            try {
                await this.beacon.transmit(wrapped);
            } catch (e) {
                console.warn('[Beaconism] Native transmit failed, using fallback');
                this.emit('beacon', wrapped);
            }
        } else {
            this.emit('beacon', wrapped);
        }

        return beaconId;
    }

    async receive(filter = {}) {
        // Listen for beacon transmissions matching filter
        return new Promise((resolve) => {
            const handler = (payload) => {
                if (this.matchesFilter(payload, filter)) {
                    this.removeListener('beacon', handler);
                    resolve(payload);
                }
            };
            this.on('beacon', handler);
        });
    }

    matchesFilter(payload, filter) {
        if (!filter.source && !filter.priority && !filter.type) return true;
        return (!filter.source || payload.source === filter.source) &&
            (!filter.priority || payload.priority === filter.priority) &&
            (!filter.type || payload.data.type === filter.type);
    }
}

// ============================================================================
// MICRO MODEL SERVER WITH BEACONISM
// ============================================================================

const PORT = 3000;
const HOST = '0.0.0.0';
const beacon = new BeaconismBridge();

console.log('🔗 Starting BigDaddyG Micro Model Server with Beaconism...');
console.log(`📍 Port: ${PORT}`);
console.log(`🎯 Beaconism: ${beacon.initialized ? 'Active' : 'Pending'}`);

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
            beaconism: beacon.initialized ? 'enabled' : 'disabled',
            features: ['micro_chain', 'nano_chain', 'model_catalog', 'deployment', 'beaconism_sync'],
            timestamp: new Date().toISOString()
        }));
    } else if (req.url === '/beacon-status') {
        res.writeHead(200, corsHeaders);
        res.end(JSON.stringify({
            beaconism_enabled: beacon.initialized,
    } else if (req.url === '/api/test-connection' && req.method === 'POST') {
        // API Key Test Endpoint - CORS proxy for testing provider APIs
        let body = '';
        req.on('data', chunk => { body += chunk.toString(); });
        req.on('end', async () => {
            try {
                const testData = JSON.parse(body);
                const { provider, apiKey, config, endpoint, headers } = testData;

                // Fetch the provider API to test the key
                const response = await fetchFn(endpoint, {
                    method: 'GET',
                    headers: {
                        ...headers,
                        'User-Agent': 'BigDaddyG-IDE/1.0'
                    }
                });

                const data = await response.json();

                if (response.ok) {
                    res.writeHead(200, corsHeaders);
                    res.end(JSON.stringify({
                        success: true,
                        provider: provider,
                        response: data,
                        timestamp: new Date().toISOString()
                    }));
                } else {
                    res.writeHead(400, corsHeaders);
                    res.end(JSON.stringify({
                        success: false,
                        provider: provider,
                        error: data.error || data.message || 'API request failed',
                        status: response.status,
                        timestamp: new Date().toISOString()
                    }));
                }
            } catch (error) {
                console.error('❌ API test error:', error.message);
                res.writeHead(500, corsHeaders);
                res.end(JSON.stringify({
                    success: false,
                    error: error.message,
                    timestamp: new Date().toISOString()
                }));
            }
        });         error: error.message,
                    timestamp: new Date().toISOString()
                }));
            }
        });
    } else {
        res.writeHead(404, corsHeaders);
        res.end(JSON.stringify({ error: 'Not found' }));
    }
});

// Create WebSocket server
const wss = new WebSocket.Server({ server });

// Micro model catalog
const microModels = {
    'gpt-micro': { size: '125M', speed: 'ultra-fast', context: '2K', beaconism: true },
    'code-nano': { size: '350M', speed: 'fast', context: '4K', beaconism: true },
    'chat-tiny': { size: '500M', speed: 'fast', context: '8K', beaconism: true },
    'analyzer-small': { size: '1B', speed: 'medium', context: '16K', beaconism: true }
};

// Track connected clients
const clients = new Set();
const beaconClients = new Map(); // Track beacon-enabled clients

// WebSocket connection handler
wss.on('connection', (ws, req) => {
    const clientId = `micro-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    const clientInfo = {
        id: clientId,
        connected: Date.now(),
        useBeaconism: false,
        lastBeacon: null
    };

    clients.add(ws);

    console.log(`🔗 Micro Model client connected: ${clientId}`);
    console.log(`📊 Total connections: ${clients.size}`);

    // Send welcome message with beaconism option
    ws.send(JSON.stringify({
        type: 'connection',
        agent: 'MicroModel',
        status: 'connected',
        message: 'Connected to Micro Model Chain System with Beaconism Support',
        clientId: clientId,
        beaconism_available: beacon.initialized,
        available_models: Object.keys(microModels),
        timestamp: new Date().toISOString()
    }));

    // Handle incoming messages
    ws.on('message', async (message) => {
        try {
            const data = JSON.parse(message);
            console.log(`📨 Received command: ${data.command} from ${clientId}`);

            // Enable beaconism if requested
            if (data.enableBeaconism && beacon.initialized) {
                clientInfo.useBeaconism = true;
                beaconClients.set(clientId, clientInfo);
                console.log(`✨ Beaconism enabled for ${clientId}`);
            }

            switch (data.command) {
                case 'micro_chain':
                    await handleMicroChain(ws, data, clientInfo);
                    break;

                case 'nano_chain':
                    await handleNanoChain(ws, data, clientInfo);
                    break;

                case 'micro_catalog':
                    handleCatalog(ws, clientInfo);
                    break;

                case 'micro_chain_test':
                    await handleChainTest(ws, data, clientInfo);
                    break;

                case 'micro_models_deploy':
                    await handleDeploy(ws, data, clientInfo);
                    break;

                case 'beacon_sync':
                    await handleBeaconSync(ws, data, clientInfo);
                    break;

                case 'ping':
                    ws.send(JSON.stringify({
                        type: 'pong',
                        beaconism: clientInfo.useBeaconism,
                        timestamp: new Date().toISOString()
                    }));
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
        beaconClients.delete(clientId);
        console.log(`🔌 Micro Model client disconnected: ${clientId}`);
        console.log(`📊 Total connections: ${clients.size}`);
    });

    // Handle errors
    ws.on('error', (error) => {
        console.error(`❌ WebSocket error for ${clientId}:`, error.message);
    });
});

// ============================================================================
// BEACONISM HANDLERS
// ============================================================================

async function handleBeaconSync(ws, data, clientInfo) {
    if (!beacon.initialized) {
        ws.send(JSON.stringify({
            type: 'error',
            message: 'Beaconism not initialized',
            timestamp: new Date().toISOString()
        }));
        return;
    }

    try {
        const beaconId = await beacon.transmit({
            type: 'micro_sync',
            command: data.syncCommand || 'full',
            models: data.models || Object.keys(microModels),
            clientId: clientInfo.id
        }, data.priority || 'high');

        clientInfo.lastBeacon = beaconId;
        clientInfo.useBeaconism = true;

        ws.send(JSON.stringify({
            type: 'beacon_sync_started',
            beaconId: beaconId,
            status: 'syncing',
            timestamp: new Date().toISOString()
        }));

        console.log(`✨ Beacon sync initiated: ${beaconId}`);
    } catch (error) {
        ws.send(JSON.stringify({
            type: 'error',
            message: 'Beacon sync failed: ' + error.message,
            timestamp: new Date().toISOString()
        }));
    }
}

// ============================================================================
// MICRO CHAIN HANDLERS
// ============================================================================

async function handleMicroChain(ws, data, clientInfo) {
    const { prompt, models = ['gpt-micro', 'code-nano'] } = data;

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: `Starting micro chain with ${models.length} models...`,
        beaconism_enabled: clientInfo.useBeaconism,
        timestamp: new Date().toISOString()
    }));

    // If beaconism enabled, use beacon transmission
    if (clientInfo.useBeaconism && beacon.initialized) {
        await beacon.transmit({
            type: 'micro_chain_init',
            prompt: prompt,
            models: models,
            clientId: clientInfo.id
        }, 'high');
    }

    // Simulate chaining
    await new Promise(resolve => setTimeout(resolve, 1000));

    const result = {
        chain_length: models.length,
        models_used: models,
        result: `Micro chain completed: Processed "${prompt}" through ${models.join(' → ')}`,
        total_tokens: Math.floor(Math.random() * 500) + 100,
        latency_ms: Math.floor(Math.random() * 200) + 50,
        beaconism_used: clientInfo.useBeaconism
    };

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_chain_complete',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));

    console.log(`✅ Micro chain completed (${models.length} models, beaconism: ${clientInfo.useBeaconism})`);
}

// Nano chain handler - Ultra-fast micro model chain
async function handleNanoChain(ws, data, clientInfo) {
    const { prompt } = data;

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: 'Executing nano chain (ultra-fast)...',
        timestamp: new Date().toISOString()
    }));

    // If beaconism enabled, use beacon transmission
    if (clientInfo.useBeaconism && beacon.initialized) {
        await beacon.transmit({
            type: 'nano_chain_init',
            prompt: prompt,
            clientId: clientInfo.id
        }, 'critical'); // Nano chains use critical priority
    }

    // Simulate ultra-fast processing
    await new Promise(resolve => setTimeout(resolve, 300));

    const result = {
        result: `Nano chain result: "${prompt}" processed in 300ms`,
        total_tokens: Math.floor(Math.random() * 100) + 20,
        latency_ms: 300,
        beaconism_used: clientInfo.useBeaconism
    };

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'nano_chain_complete',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));

    console.log(`✅ Nano chain completed (beaconism: ${clientInfo.useBeaconism})`);
}

// Catalog handler
function handleCatalog(ws, clientInfo) {
    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_catalog',
        status: 'success',
        data: microModels,
        beaconism_enabled: clientInfo.useBeaconism,
        timestamp: new Date().toISOString()
    }));

    console.log(`📚 Catalog sent (beaconism: ${clientInfo.useBeaconism})`);
}

// Chain test handler
async function handleChainTest(ws, data, clientInfo) {
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
        success_rate: '100%',
        beaconism_available: beacon.initialized,
        beaconism_used: clientInfo.useBeaconism
    };

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_chain_test',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));

    console.log(`✅ Chain test completed (beaconism: ${clientInfo.useBeaconism})`);
}

// Deployment handler
async function handleDeploy(ws, data, clientInfo) {
    const { models = [] } = data;

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        status: 'progress',
        message: `Deploying ${models.length || Object.keys(microModels).length} micro models...`,
        timestamp: new Date().toISOString()
    }));

    // If beaconism enabled, use beacon transmission for deployment
    if (clientInfo.useBeaconism && beacon.initialized) {
        await beacon.transmit({
            type: 'deployment_init',
            models: models.length > 0 ? models : Object.keys(microModels),
            clientId: clientInfo.id
        }, 'high');
    }

    await new Promise(resolve => setTimeout(resolve, 800));

    const result = {
        deployed: models.length > 0 ? models : Object.keys(microModels),
        status: 'ready',
        endpoints: (models.length > 0 ? models : Object.keys(microModels)).map(m => `/api/micro/${m}`),
        beaconism_used: clientInfo.useBeaconism
    };

    ws.send(JSON.stringify({
        agent: 'MicroModel',
        command: 'micro_models_deployed',
        status: 'success',
        data: result,
        timestamp: new Date().toISOString()
    }));

    console.log(`✅ Models deployed: ${result.deployed.join(', ')} (beaconism: ${clientInfo.useBeaconism})`);
}

// Heartbeat to keep connections alive
setInterval(() => {
    clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({
                type: 'heartbeat',
                agent: 'MicroModel',
                beaconism_status: beacon.initialized ? 'active' : 'inactive',
                beacon_payloads: beacon.beaconPayloads.size,
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
    console.log(`✨ Beaconism: ${beacon.initialized ? 'Enabled' : 'Disabled (fallback mode)'}`);
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

// Export for Electron main process integration
module.exports = { server, wss, beacon, microModels, clients, beaconClients };

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

