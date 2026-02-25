/**
 * AgentWrapper - Self-contained Node.js runtime carrier for swarm agents
 * Each agent spawns its own HTTP microservice with introspection capabilities
 * Enhanced with cryptographic fingerprinting and security monitoring
 */

const http = require('http');
const { fork } = require('child_process');
const crypto = require('crypto');
const SwarmAnalytics = require('./swarm-analytics');

class AgentWrapper {
    constructor(name, capabilities, logic, options = {}) {
        this.name = name;
        this.capabilities = capabilities;
        this.logic = logic;

        // Generate cryptographic fingerprint of the agent's logic
        this.signature = this.generateFingerprint(logic.toString());

        this.options = {
            port: options.port || this.getAvailablePort(),
            autoRestart: options.autoRestart !== false,
            maxRestarts: options.maxRestarts || 3,
            restartDelay: options.restartDelay || 5000,
            ...options
        };

        this.port = this.options.port;
        this.server = null;
        this.restartCount = 0;
        this.glyphRenderer = null;
        this.syncVisualizer = null;
        this.status = 'active';
        this.quarantined = false;

        // Initialize analytics tracking
        this.analytics = SwarmAnalytics.getInstance();

        // Verify agent signature before spawning
        this.verifySignature();

        // Spawn the agent service
        this.spawnServer();

        // Set up health monitoring
        this.startHealthMonitoring();

        // Emit boot glyph
        this.emitGlyph('boot', 'success', 0.42);
    }

    /**
     * Generate cryptographic fingerprint of agent logic
     */
    generateFingerprint(codeString) {
        return crypto.createHash('sha256').update(codeString).digest('hex');
    }

    /**
     * Verify agent signature against registry
     */
    verifySignature() {
        // In a real implementation, this would check against a secure registry
        // For now, we'll register the signature when the agent is created
        this.registerSignature();
    }

    /**
     * Register agent signature in secure registry
     */
    registerSignature() {
        // Store signature for validation
        this.knownSignature = this.signature;

        // Track signature registration
        this.analytics.track({
            type: 'security',
            agent: this.name,
            action: 'signature_registered',
            success: true,
            signature: this.signature,
            timestamp: new Date().toISOString()
        });
    }

    /**
     * Validate incoming packet signature
     */
    validateSignature(packet) {
        if (packet.signature && packet.signature !== this.knownSignature) {
            this.handleSecurityBreach(packet);
            return false;
        }
        return true;
    }

    /**
     * Handle security breach detection
     */
    handleSecurityBreach(packet) {
        console.warn(`🛡️ Security breach detected for ${this.name}: signature mismatch`);

        this.analytics.track({
            type: 'security',
            agent: this.name,
            action: 'heist_detected',
            success: false,
            signature: packet.signature,
            expected: this.knownSignature,
            timestamp: new Date().toISOString()
        });

        // Trigger quarantine
        this.quarantine();

        // Emit security glyph
        this.emitGlyph('heist_detected', 'breach', 0.99);
    }

    /**
     * Quarantine agent due to security breach
     */
    quarantine() {
        this.quarantined = true;
        this.status = 'quarantined';

        // Disable agent functionality
        this.shutdown();

        // Replace logic with access denied
        this.logic = () => ({ error: 'access denied - quarantined' });

        console.log(`🚨 Agent ${this.name} quarantined due to security breach`);
    }

    /**
     * Get an available port starting from a random high number
     */
    getAvailablePort(start = 49152) {
        return start + Math.floor(Math.random() * 1000);
    }

    /**
     * Spawn the HTTP server for this agent
     */
    spawnServer() {
        try {
            this.server = http.createServer((req, res) => {
                this.handleRequest(req, res);
            });

            this.server.listen(this.port, () => {
                console.log(`🤖 Agent ${this.name} listening on port ${this.port}`);
                this.analytics.track({
                    type: 'agent',
                    agent: this.name,
                    action: 'boot',
                    success: true,
                    latency: 0,
                    entropy: 0.42
                });

                // Track port binding success
                this.analytics.track({
                    type: 'swarm',
                    swarmId: 'port-binding',
                    action: 'port_bound',
                    success: true,
                    port: this.port,
                    agent: this.name,
                    timestamp: new Date().toISOString()
                });
            });

            this.server.on('error', (err) => {
                console.error(`❌ Agent ${this.name} server error:`, err);
                this.handleServerError(err);
            });

        } catch (error) {
            console.error(`❌ Failed to spawn server for ${this.name}:`, error);
            this.handleServerError(error);
        }
    }

    /**
     * Handle server errors and trigger fallback if port binding fails
     */
    handleServerError(error) {
        console.error(`❌ Agent ${this.name} server error:`, error);

        // Check if it's a port binding error
        if (error.code === 'EADDRINUSE' || error.code === 'EACCES') {
            console.log(`🚨 Port ${this.port} unavailable for ${this.name}, triggering fallback`);

            // Emit port failure glyph
            this.emitGlyph('port_failure', 'error', 0.99);

            // Track port conflict
            this.analytics.track({
                type: 'swarm',
                swarmId: 'port-conflict',
                action: 'port_conflict',
                success: false,
                port: this.port,
                agent: this.name,
                timestamp: new Date().toISOString()
            });

            // Trigger fallback agent
            this.spawnFallbackAgent();
        } else {
            // Handle other errors with restart
            if (this.options.autoRestart && this.restartCount < this.options.maxRestarts) {
                this.restartCount++;
                console.log(`🔄 Restarting ${this.name} (attempt ${this.restartCount}/${this.options.maxRestarts})`);

                setTimeout(() => {
                    this.spawnServer();
                }, this.options.restartDelay);
            } else {
                this.emitGlyph('crash', 'failed', 0.9);
            }
        }
    }

    /**
     * Spawn fallback agent with different port
     */
    spawnFallbackAgent() {
        const fallbackName = `${this.name}_fallback`;
        const fallbackPort = this.getAvailablePort();

        console.log(`🔄 Spawning fallback agent: ${fallbackName} on port ${fallbackPort}`);

        // Create fallback agent with same logic
        const fallbackAgent = new AgentWrapper(fallbackName, this.capabilities, this.logic, {
            port: fallbackPort,
            autoRestart: false, // Disable auto-restart for fallback to prevent loops
            maxRestarts: 1
        });

        // Track fallback spawning
        this.analytics.track({
            type: 'swarm',
            swarmId: 'fallback-chain',
            action: 'fallback_spawned',
            success: true,
            originalAgent: this.name,
            fallbackAgent: fallbackName,
            port: fallbackPort,
            timestamp: new Date().toISOString()
        });

        // Emit fallback glyph
        fallbackAgent.emitGlyph('fallback_spawned', 'recovery', 0.77);

        // Update primary agent status
        this.status = 'fallback_triggered';
        this.port = null; // Disable primary port

        return fallbackAgent;
    }

    /**
     * Handle incoming HTTP requests
     */
    handleRequest(req, res) {
        const startTime = Date.now();

        try {
            // Set CORS headers
            res.setHeader('Access-Control-Allow-Origin', '*');
            res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
            res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

            if (req.method === 'OPTIONS') {
                res.writeHead(200);
                res.end();
                return;
            }

            let body = '';
            req.on('data', chunk => body += chunk);
            req.on('end', () => {
                this.processRequest(req, res, body, startTime);
            });

        } catch (error) {
            console.error(`❌ Request handling error for ${this.name}:`, error);
            this.sendResponse(res, 500, { error: error.message });
        }
    }

    /**
     * Process the actual request
     */
    async processRequest(req, res, body, startTime) {
        const latency = Date.now() - startTime;

        try {
            let result;
            let entropy = 0.5;

            switch (req.url) {
                case '/health':
                    result = {
                        status: 'healthy',
                        agent: this.name,
                        port: this.port,
                        uptime: this.getUptime(),
                        capabilities: this.capabilities
                    };
                    break;

                case '/introspect':
                    result = {
                        agent: this.name,
                        capabilities: this.capabilities,
                        context: this.getContext(),
                        telemetry: this.analytics.analyticsData.agentMetrics[this.name] || {},
                        introspection: this.introspect()
                    };
                    break;

                case '/task':
                    if (req.method !== 'POST') {
                        return this.sendResponse(res, 405, { error: 'Method not allowed' });
                    }

                    const taskData = JSON.parse(body || '{}');
                    result = await this.executeTask(taskData);
                    entropy = this.calculateEntropy(result);
                    break;

                case '/restart':
                    this.restart();
                    result = { status: 'restarting' };
                    break;

                case '/shutdown':
                    this.shutdown();
                    result = { status: 'shutting down' };
                    break;

                default:
                    result = { error: 'Unknown endpoint' };
            }

            // Track analytics
            this.analytics.track({
                type: 'agent',
                agent: this.name,
                action: req.url.substring(1) || 'unknown',
                success: !result.error,
                latency,
                entropy
            });

            // Emit glyph for visual feedback
            this.emitGlyph(req.url.substring(1) || 'unknown', result.error ? 'error' : 'success', entropy);

            this.sendResponse(res, 200, result);

        } catch (error) {
            console.error(`❌ Processing error for ${this.name}:`, error);
            this.sendResponse(res, 500, { error: error.message });

            // Track error
            this.analytics.track({
                type: 'agent',
                agent: this.name,
                action: 'error',
                success: false,
                latency,
                entropy: 0.9
            });
        }
    }

    /**
     * Execute a task using the agent's logic
     */
    async executeTask(taskData) {
        const startTime = Date.now();

        try {
            // Call the agent's logic function
            const result = await this.logic(taskData);

            // Calculate execution metrics
            const executionTime = Date.now() - startTime;

            // Track task execution
            this.analytics.track({
                type: 'agent',
                agent: this.name,
                action: 'task_execution',
                success: true,
                latency: executionTime,
                entropy: this.calculateEntropy(result)
            });

            return {
                success: true,
                result,
                executionTime,
                agent: this.name
            };

        } catch (error) {
            const executionTime = Date.now() - startTime;

            // Track failed execution
            this.analytics.track({
                type: 'agent',
                agent: this.name,
                action: 'task_error',
                success: false,
                latency: executionTime,
                entropy: 0.9
            });

            throw error;
        }
    }

    /**
     * Self-introspection capability
     */
    introspect() {
        return {
            name: this.name,
            capabilities: this.capabilities,
            port: this.port,
            uptime: this.getUptime(),
            restartCount: this.restartCount,
            memory: process.memoryUsage(),
            telemetry: this.analytics.analyticsData.agentMetrics[this.name] || {}
        };
    }

    /**
     * Get current context for the agent
     */
    getContext() {
        return {
            active: true,
            port: this.port,
            uptime: this.getUptime(),
            capabilities: this.capabilities,
            lastActivity: new Date().toISOString()
        };
    }

    /**
     * Calculate entropy of a result for visualization
     */
    calculateEntropy(result) {
        if (typeof result === 'string') {
            // Simple entropy calculation based on character diversity
            const chars = result.split('');
            const uniqueChars = new Set(chars);
            return Math.min(uniqueChars.size / chars.length, 1.0);
        }
        return 0.5; // Default entropy
    }

    /**
     * Get uptime in milliseconds
     */
    getUptime() {
        return Date.now() - (this.startTime || Date.now());
    }

    /**
     * Emit a visual glyph for the agent action
     */
    emitGlyph(intent, result, entropy) {
        if (this.glyphRenderer) {
            this.glyphRenderer.render({
                agent: this.name,
                intent,
                result,
                entropy,
                port: this.port,
                timestamp: new Date().toISOString()
            });
        }
    }

    /**
     * Set glyph renderer for visual feedback
     */
    setGlyphRenderer(renderer) {
        this.glyphRenderer = renderer;
    }

    /**
     * Set sync visualizer for twinsie connections
     */
    setSyncVisualizer(visualizer) {
        this.syncVisualizer = visualizer;
    }

    /**
     * Send HTTP response
     */
    sendResponse(res, statusCode, data) {
        res.writeHead(statusCode, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(data));
    }

    /**
     * Handle server errors and auto-restart if enabled
     */
    handleServerError(error) {
        console.error(`❌ Agent ${this.name} server error:`, error);

        if (this.options.autoRestart && this.restartCount < this.options.maxRestarts) {
            this.restartCount++;
            console.log(`🔄 Restarting ${this.name} (attempt ${this.restartCount}/${this.options.maxRestarts})`);

            setTimeout(() => {
                this.spawnServer();
            }, this.options.restartDelay);
        } else {
            this.emitGlyph('crash', 'failed', 0.9);
        }
    }

    /**
     * Restart the agent service
     */
    restart() {
        console.log(`🔄 Manually restarting ${this.name}`);
        this.shutdown();
        setTimeout(() => this.spawnServer(), 1000);
    }

    /**
     * Shutdown the agent service
     */
    shutdown() {
        if (this.server) {
            this.server.close();
            this.server = null;
        }
        this.emitGlyph('shutdown', 'complete', 0.1);
    }

    /**
     * Start health monitoring
     */
    startHealthMonitoring() {
        this.healthCheckInterval = setInterval(async () => {
            try {
                // Self-health check
                const response = await this.makeRequest('GET', '/health');
                if (response.status !== 'healthy') {
                    throw new Error('Health check failed');
                }
            } catch (error) {
                console.warn(`⚠️ Agent ${this.name} health check failed, triggering restart`);
                this.handleServerError(error);
            }
        }, 10000); // Check every 10 seconds
    }

    /**
     * Make an internal request to this agent's server
     */
    async makeRequest(method, path, data = null) {
        return new Promise((resolve, reject) => {
            const options = {
                hostname: 'localhost',
                port: this.port,
                path,
                method,
                headers: data ? { 'Content-Type': 'application/json' } : {}
            };

            const req = http.request(options, (res) => {
                let body = '';
                res.on('data', chunk => body += chunk);
                res.on('end', () => {
                    try {
                        resolve(JSON.parse(body));
                    } catch (error) {
                        resolve({ status: 'error', message: body });
                    }
                });
            });

            req.on('error', reject);

            if (data) {
                req.write(JSON.stringify(data));
            }

            req.end();
        });
    }

    /**
     * Get agent status for HUD display
     */
    getStatus() {
        return {
            name: this.name,
            port: this.port,
            uptime: this.getUptime(),
            capabilities: this.capabilities,
            restartCount: this.restartCount,
            healthy: this.server && this.server.listening,
            telemetry: this.analytics.analyticsData.agentMetrics[this.name] || {}
        };
    }
}

module.exports = AgentWrapper;
