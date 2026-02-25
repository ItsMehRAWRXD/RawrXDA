/**
 * BigDaddyG Master Orchestrator
 * Connects all components and provides unified system interface
 */

const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

// Import all available components
const AgenticBrowser = require('./agent.js').AgenticBrowser;
const ProductionBrowserAgent = require('./production-browser-agent.js');
const TextProcessor = require('./text-processor.js');
const BulkTextHandler = require('./bulk-handler.js');
const ModelManager = require('./model-manager.js');
const ProductionHardening = require('./agents/BrowserHardeningAgent.js').ProductionHardening;
const AgentGuidance = require('./agent-guidance.js');
const EnhancedOrchestrator = require('./enhanced-orchestrator.js');

class BigDaddyGMasterOrchestrator {
    constructor() {
        this.components = new Map();
        this.activeAgents = new Set();
        this.ollamaProcess = null;
        this.systemStatus = 'initializing';

        this.initializeComponents();
        this.startOllamaService();
    }

    /**
     * Initialize all system components
     */
    async initializeComponents() {
        console.log('🚀 Initializing BigDaddyG Master Orchestrator...');

        try {
            // Core Browser Agent
            this.components.set('browser', new AgenticBrowser());
            console.log('✅ Agentic Browser initialized');

            // Production Browser Agent
            this.components.set('productionBrowser', new ProductionBrowserAgent());
            console.log('✅ Production Browser Agent initialized');

            // Text Processing
            this.components.set('textProcessor', new TextProcessor());
            console.log('✅ Text Processor initialized');

            // Bulk Handler
            this.components.set('bulkHandler', new BulkTextHandler());
            console.log('✅ Bulk Text Handler initialized');

            // Model Manager
            this.components.set('modelManager', new ModelManager());
            console.log('✅ Model Manager initialized');

            // Production Hardening
            this.components.set('hardening', new ProductionHardening());
            console.log('✅ Production Hardening initialized');

            // Agent Guidance
            this.components.set('guidance', new AgentGuidance());
            console.log('✅ Agent Guidance initialized');

            // Enhanced Orchestrator
            this.components.set('orchestrator', new EnhancedOrchestrator());
            console.log('✅ Enhanced Orchestrator initialized');

            this.systemStatus = 'ready';
            console.log('🎉 All BigDaddyG components initialized successfully!');

        } catch (error) {
            console.error('❌ Component initialization failed:', error);
            this.systemStatus = 'error';
        }
    }

    /**
     * Start Ollama service with BigDaddyG model
     */
    startOllamaService() {
        console.log('🔄 Starting Ollama service...');

        try {
            // Check if Ollama is running
            this.ollamaProcess = spawn('ollama', ['serve'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                detached: true
            });

            this.ollamaProcess.stdout.on('data', (data) => {
                console.log('Ollama:', data.toString().trim());
            });

            this.ollamaProcess.stderr.on('data', (data) => {
                console.log('Ollama Error:', data.toString().trim());
            });

            // Wait a bit then check if model is available
            setTimeout(() => {
                this.checkBigDaddyGModel();
            }, 3000);

        } catch (error) {
            console.log('⚠️  Ollama not available, using fallback mode');
            this.systemStatus = 'fallback';
        }
    }

    /**
     * Check if BigDaddyG model is available
     */
    async checkBigDaddyGModel() {
        try {
            const response = await fetch('http://localhost:11434/api/tags');
            const data = await response.json();

            const hasBigDaddyG = data.models?.some(model =>
                model.name.includes('bigdaddyg') || model.name.includes('BigDaddyG')
            );

            if (hasBigDaddyG) {
                console.log('✅ BigDaddyG model available');
                this.systemStatus = 'full';
            } else {
                console.log('⚠️  BigDaddyG model not found, attempting to pull...');
                this.pullBigDaddyGModel();
            }
        } catch (error) {
            console.log('⚠️  Cannot connect to Ollama, using fallback mode');
            this.systemStatus = 'fallback';
        }
    }

    /**
     * Pull BigDaddyG model from Ollama
     */
    pullBigDaddyGModel() {
        const pullProcess = spawn('ollama', ['pull', 'bigdaddyg:latest'], {
            stdio: ['pipe', 'pipe', 'pipe']
        });

        pullProcess.stdout.on('data', (data) => {
            console.log('Pulling BigDaddyG:', data.toString().trim());
        });

        pullProcess.on('close', (code) => {
            if (code === 0) {
                console.log('✅ BigDaddyG model pulled successfully');
                this.systemStatus = 'full';
            } else {
                console.log('❌ Failed to pull BigDaddyG model');
                this.systemStatus = 'fallback';
            }
        });
    }

    /**
     * Execute task using appropriate components
     */
    async executeTask(task) {
        console.log(`🎯 Executing task: ${task}`);

        try {
            // Determine task type and route to appropriate components
            const taskType = this.analyzeTask(task);

            switch (taskType) {
                case 'web':
                    return await this.handleWebTask(task);
                case 'text':
                    return await this.handleTextTask(task);
                case 'bulk':
                    return await this.handleBulkTask(task);
                case 'model':
                    return await this.handleModelTask(task);
                case 'system':
                    return await this.handleSystemTask(task);
                default:
                    return await this.handleGeneralTask(task);
            }
        } catch (error) {
            console.error('❌ Task execution failed:', error);
            return `Error: ${error.message}`;
        }
    }

    /**
     * Analyze task type
     */
    analyzeTask(task) {
        const lowerTask = task.toLowerCase();

        if (lowerTask.includes('web') || lowerTask.includes('browser') || lowerTask.includes('url')) {
            return 'web';
        }
        if (lowerTask.includes('bulk') || lowerTask.includes('multiple') || lowerTask.includes('batch')) {
            return 'bulk';
        }
        if (lowerTask.includes('model') || lowerTask.includes('train') || lowerTask.includes('ai')) {
            return 'model';
        }
        if (lowerTask.includes('system') || lowerTask.includes('status') || lowerTask.includes('health')) {
            return 'system';
        }
        if (lowerTask.includes('text') || lowerTask.includes('process') || lowerTask.includes('analyze')) {
            return 'text';
        }

        return 'general';
    }

    /**
     * Handle web-related tasks
     */
    async handleWebTask(task) {
        const browser = this.components.get('browser');
        const productionBrowser = this.components.get('productionBrowser');

        if (task.includes('production') || task.includes('hardened')) {
            return await productionBrowser.run(task);
        } else {
            return await browser.run(task);
        }
    }

    /**
     * Handle text processing tasks
     */
    async handleTextTask(task) {
        const textProcessor = this.components.get('textProcessor');
        return await textProcessor.process(task);
    }

    /**
     * Handle bulk processing tasks
     */
    async handleBulkTask(task) {
        const bulkHandler = this.components.get('bulkHandler');
        return await bulkHandler.process(task);
    }

    /**
     * Handle model-related tasks
     */
    async handleModelTask(task) {
        const modelManager = this.components.get('modelManager');
        return await modelManager.handle(task);
    }

    /**
     * Handle system tasks
     */
    async handleSystemTask(task) {
        const status = {
            systemStatus: this.systemStatus,
            activeComponents: Array.from(this.components.keys()),
            activeAgents: Array.from(this.activeAgents),
            timestamp: new Date().toISOString()
        };

        return JSON.stringify(status, null, 2);
    }

    /**
     * Handle general tasks using AI
     */
    async handleGeneralTask(task) {
        if (this.systemStatus === 'full') {
            // Use BigDaddyG model
            try {
                const response = await fetch('http://localhost:11434/api/generate', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        model: 'bigdaddyg:latest',
                        prompt: task,
                        stream: false
                    })
                });

                const result = await response.json();
                return result.response;
            } catch (error) {
                console.log('⚠️  Ollama error, using fallback');
                return this.fallbackResponse(task);
            }
        } else {
            return this.fallbackResponse(task);
        }
    }

    /**
     * Fallback response when AI model is not available
     */
    fallbackResponse(task) {
        const responses = [
            `I understand you want me to: ${task}. As BigDaddyG, I'm designed to help with AI development, web automation, and system orchestration.`,
            `Task acknowledged: ${task}. I'm running in fallback mode without the full AI model. My core capabilities include web automation, text processing, and system orchestration.`,
            `Processing: ${task}. While I don't have the full AI model loaded, I can still help with technical tasks, system monitoring, and component orchestration.`,
            `Request received: ${task}. I'm operating with limited capabilities but can still assist with browser automation, file processing, and system management.`
        ];

        return responses[Math.floor(Math.random() * responses.length)];
    }

    /**
     * Get system status
     */
    getStatus() {
        return {
            systemStatus: this.systemStatus,
            components: Array.from(this.components.keys()),
            agents: Array.from(this.activeAgents),
            capabilities: [
                'web_automation',
                'text_processing',
                'bulk_operations',
                'model_management',
                'system_monitoring',
                'ai_assistance'
            ]
        };
    }

    /**
     * Clean shutdown
     */
    shutdown() {
        console.log('🔄 Shutting down BigDaddyG Master Orchestrator...');

        if (this.ollamaProcess) {
            this.ollamaProcess.kill();
        }

        for (const [name, component] of this.components) {
            if (component.shutdown) {
                component.shutdown();
            }
        }

        console.log('✅ BigDaddyG system shut down gracefully');
    }
}

// Export singleton instance
const masterOrchestrator = new BigDaddyGMasterOrchestrator();

module.exports = {
    BigDaddyGMasterOrchestrator,
    masterOrchestrator
};
