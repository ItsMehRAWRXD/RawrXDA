/**
 * Global Agent Exchange - Import/Export + Token Economy
 * Marketplace for agentic systems with rental and learning capabilities
 */

class GlobalAgentExchange {
    constructor() {
        this.registry = new Map(); // agentId -> agent metadata
        this.importedAgents = new Map(); // userId -> imported agents
        this.exportedAgents = new Map(); // agentId -> export metadata
        this.rentalContracts = new Map(); // contractId -> rental info
        this.learningSessions = new Map(); // sessionId -> learning info

        // Initialize with some example agents
        this.initializeExampleAgents();

        console.log('🌍 Global Agent Exchange initialized');
    }

    /**
     * Initialize example agents for demonstration
     */
    initializeExampleAgents() {
        const exampleAgents = [
            {
                id: 'Claude-Fusion-v2',
                name: 'Claude Fusion Assistant',
                origin: 'France',
                creator: 'SwarmLabs',
                capabilities: ['memory', 'debug', 'caption', 'voice', 'fusion'],
                tokenCost: {
                    rental: 15,
                    learning: 5,
                    purchase: 100
                },
                exportFormat: 'browser-wrapper',
                description: 'Advanced Claude-based agent with fusion capabilities',
                lineage: ['Claude-3.5', 'Fusion-Engine-v1', 'Voice-Module'],
                entropy: 0.67,
                rating: 4.8,
                usageCount: 1247,
                lastUpdated: '2025-01-15T10:30:00Z'
            },
            {
                id: 'DeepSeek-R1-Enhanced',
                name: 'DeepSeek R1 Reasoning Engine',
                origin: 'Korea',
                creator: 'DeepMind-Korea',
                capabilities: ['reasoning', 'coding', 'math', 'analysis'],
                tokenCost: {
                    rental: 0, // Free tier
                    learning: 3,
                    purchase: 75
                },
                exportFormat: 'json-capsule',
                description: 'High-performance reasoning agent with mathematical capabilities',
                lineage: ['DeepSeek-R1', 'Math-Engine', 'Code-Parser'],
                entropy: 0.45,
                rating: 4.6,
                usageCount: 892,
                lastUpdated: '2025-01-14T14:20:00Z'
            },
            {
                id: 'Copilot-Wrapper-v3',
                name: 'Copilot Code Assistant',
                origin: 'US',
                creator: 'Microsoft-OpenAI',
                capabilities: ['coding', 'debugging', 'documentation'],
                tokenCost: {
                    rental: 10,
                    learning: 0, // Free learning
                    purchase: 60
                },
                exportFormat: 'browser-wrapper',
                description: 'VS Code integrated coding assistant',
                lineage: ['Copilot-X', 'Debug-Module', 'Doc-Generator'],
                entropy: 0.52,
                rating: 4.4,
                usageCount: 2156,
                lastUpdated: '2025-01-16T09:15:00Z'
            }
        ];

        exampleAgents.forEach(agent => {
            this.registry.set(agent.id, agent);
        });
    }

    /**
     * Export an agent to capsule format
     */
    exportAgent(agentId, exportFormat = 'json-capsule') {
        const agent = this.registry.get(agentId);
        if (!agent) {
            throw new Error(`Agent ${agentId} not found in registry`);
        }

        const capsule = {
            metadata: {
                id: agent.id,
                name: agent.name,
                version: '1.0.0',
                exportFormat,
                exportedAt: new Date().toISOString(),
                exportedBy: 'current-user'
            },
            agent: {
                capabilities: agent.capabilities,
                lineage: agent.lineage,
                entropy: agent.entropy,
                configuration: this.generateAgentConfig(agent)
            },
            tokenRequirements: agent.tokenCost,
            dependencies: this.getAgentDependencies(agent)
        };

        // Generate capsule based on format
        switch (exportFormat) {
            case 'json-capsule':
                return {
                    format: 'json-capsule',
                    data: JSON.stringify(capsule, null, 2),
                    size: JSON.stringify(capsule).length
                };

            case 'browser-wrapper':
                return {
                    format: 'browser-wrapper',
                    data: this.generateBrowserWrapper(capsule),
                    size: this.generateBrowserWrapper(capsule).length
                };

            default:
                throw new Error(`Unsupported export format: ${exportFormat}`);
        }
    }

    /**
     * Import an agent from capsule
     */
    async importAgent(capsuleData, userId) {
        let capsule;

        try {
            if (typeof capsuleData === 'string') {
                capsule = JSON.parse(capsuleData);
            } else {
                capsule = capsuleData;
            }
        } catch (error) {
            throw new Error(`Invalid capsule format: ${error.message}`);
        }

        // Validate capsule structure
        if (!capsule.metadata || !capsule.agent) {
            throw new Error('Invalid capsule structure');
        }

        const agentId = `${capsule.metadata.id}-${Date.now()}`;

        // Create imported agent record
        const importedAgent = {
            id: agentId,
            originalId: capsule.metadata.id,
            name: capsule.metadata.name,
            userId,
            importedAt: new Date().toISOString(),
            capsule: capsule,
            status: 'imported',
            usageCount: 0,
            learningProgress: 0
        };

        if (!this.importedAgents.has(userId)) {
            this.importedAgents.set(userId, new Map());
        }
        this.importedAgents.get(userId).set(agentId, importedAgent);

        console.log(`📥 Imported agent ${agentId} from capsule`);
        return importedAgent;
    }

    /**
     * Rent an agent for temporary use
     */
    rentAgent(userId, agentId, durationHours = 24) {
        const agent = this.registry.get(agentId);
        if (!agent) {
            throw new Error(`Agent ${agentId} not available for rental`);
        }

        const rentalCost = agent.tokenCost.rental;
        if (rentalCost > 0) {
            // Check if user can afford rental
            const userBalance = window.swarmBank ? window.swarmBank.getBalance(userId) : 0;
            if (userBalance < rentalCost) {
                throw new Error(`Insufficient tokens for rental. Need ${rentalCost}, have ${userBalance}`);
            }

            // Deduct tokens
            if (window.swarmBank) {
                window.swarmBank.spendTokens(userId, agent.creator, 'rental', {
                    agentId,
                    duration: durationHours
                });
            }
        }

        const contractId = `rental_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

        const rentalContract = {
            id: contractId,
            userId,
            agentId,
            startTime: new Date().toISOString(),
            endTime: new Date(Date.now() + durationHours * 60 * 60 * 1000).toISOString(),
            cost: rentalCost,
            status: 'active'
        };

        this.rentalContracts.set(contractId, rentalContract);

        console.log(`🧳 Rented ${agentId} for ${durationHours}h (${rentalCost} tokens)`);
        return rentalContract;
    }

    /**
     * Start a learning session with an agent
     */
    startLearningSession(userId, agentId) {
        const agent = this.registry.get(agentId);
        if (!agent) {
            throw new Error(`Agent ${agentId} not available for learning`);
        }

        const learningCost = agent.tokenCost.learning;
        if (learningCost > 0) {
            const userBalance = window.swarmBank ? window.swarmBank.getBalance(userId) : 0;
            if (userBalance < learningCost) {
                throw new Error(`Insufficient tokens for learning. Need ${learningCost}, have ${userBalance}`);
            }

            // Deduct learning tokens
            if (window.swarmBank) {
                window.swarmBank.spendTokens(userId, agent.creator, 'learning', {
                    agentId,
                    type: 'memory_stream'
                });
            }
        }

        const sessionId = `learn_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;

        const learningSession = {
            id: sessionId,
            userId,
            agentId,
            startTime: new Date().toISOString(),
            cost: learningCost,
            progress: 0,
            status: 'active',
            learnedTokens: 0
        };

        this.learningSessions.set(sessionId, learningSession);

        console.log(`🎓 Started learning session for ${agentId} (${learningCost} tokens)`);
        return learningSession;
    }

    /**
     * Stream agent memory for learning
     */
    async streamAgentMemory(sessionId, callback) {
        const session = this.learningSessions.get(sessionId);
        if (!session || session.status !== 'active') {
            throw new Error('Invalid or inactive learning session');
        }

        const agent = this.registry.get(session.agentId);
        if (!agent) {
            throw new Error('Agent not found');
        }

        // Simulate streaming agent memory
        const memoryChunks = this.generateMemoryChunks(agent);

        for (let i = 0; i < memoryChunks.length; i++) {
            await new Promise(resolve => setTimeout(resolve, 200)); // Simulate streaming delay

            const progress = ((i + 1) / memoryChunks.length) * 100;
            session.progress = progress;
            session.learnedTokens += memoryChunks[i].tokens;

            if (callback) {
                callback(memoryChunks[i], progress);
            }
        }

        session.status = 'completed';
        session.endTime = new Date().toISOString();

        console.log(`✅ Learning session ${sessionId} completed (${session.learnedTokens} tokens learned)`);
        return session;
    }

    /**
     * Get available agents for browsing
     */
    getAvailableAgents(filters = {}) {
        let agents = Array.from(this.registry.values());

        if (filters.capabilities) {
            agents = agents.filter(agent =>
                filters.capabilities.every(cap => agent.capabilities.includes(cap))
            );
        }

        if (filters.origin) {
            agents = agents.filter(agent => agent.origin === filters.origin);
        }

        if (filters.maxCost) {
            agents = agents.filter(agent =>
                (agent.tokenCost.rental || 0) <= filters.maxCost &&
                (agent.tokenCost.learning || 0) <= filters.maxCost
            );
        }

        return agents.sort((a, b) => b.rating - a.rating);
    }

    /**
     * Get imported agents for a user
     */
    getImportedAgents(userId) {
        const userAgents = this.importedAgents.get(userId);
        return userAgents ? Array.from(userAgents.values()) : [];
    }

    /**
     * Get active rental contracts for a user
     */
    getActiveRentals(userId) {
        return Array.from(this.rentalContracts.values())
            .filter(contract => contract.userId === userId && contract.status === 'active');
    }

    /**
     * Get active learning sessions for a user
     */
    getActiveLearningSessions(userId) {
        return Array.from(this.learningSessions.values())
            .filter(session => session.userId === userId && session.status === 'active');
    }

    /**
     * Generate agent configuration for export
     */
    generateAgentConfig(agent) {
        return {
            capabilities: agent.capabilities,
            entropy: agent.entropy,
            lineage: agent.lineage,
            modelType: 'transformer',
            contextWindow: 4096,
            embeddingDim: 768,
            parameters: '7B'
        };
    }

    /**
     * Get agent dependencies for export
     */
    getAgentDependencies(agent) {
        return {
            runtime: 'nodejs-18+',
            memory: '256MB+',
            storage: 'optional',
            network: 'optional'
        };
    }

    /**
     * Generate browser wrapper for agent
     */
    generateBrowserWrapper(capsule) {
        return `<!DOCTYPE html>
<html>
<head><title>Agent: ${capsule.metadata.name}</title></head>
<body>
  <h1>🤖 ${capsule.metadata.name}</h1>
  <p>Agent capsule loaded. Capabilities: ${capsule.agent.capabilities.join(', ')}</p>
  <script>
    const agentCapsule = ${JSON.stringify(capsule)};
    console.log('Agent capsule imported:', agentCapsule.metadata.name);
  </script>
</body>
</html>`;
    }

    /**
     * Generate mock memory chunks for learning
     */
    generateMemoryChunks(agent) {
        const chunks = [];
        const topics = ['programming', 'mathematics', 'writing', 'analysis', 'debugging'];

        topics.forEach((topic, index) => {
            chunks.push({
                topic,
                content: `${agent.name} has extensive knowledge in ${topic}, with ${Math.floor(Math.random() * 1000) + 500} training examples.`,
                tokens: Math.floor(Math.random() * 100) + 50,
                chunkIndex: index + 1,
                totalChunks: topics.length
            });
        });

        return chunks;
    }

    /**
     * Get exchange statistics
     */
    getExchangeStats() {
        const agents = Array.from(this.registry.values());
        const totalAgents = agents.length;
        const avgRating = agents.reduce((sum, agent) => sum + agent.rating, 0) / totalAgents;
        const totalUsage = agents.reduce((sum, agent) => sum + agent.usageCount, 0);

        return {
            totalAgents,
            avgRating: avgRating.toFixed(2),
            totalUsage,
            activeRentals: Array.from(this.rentalContracts.values()).filter(c => c.status === 'active').length,
            activeLearning: Array.from(this.learningSessions.values()).filter(s => s.status === 'active').length,
            totalImported: Array.from(this.importedAgents.values()).reduce((sum, userAgents) => sum + userAgents.size, 0)
        };
    }

    /**
     * Export exchange state
     */
    exportState() {
        return {
            registry: Object.fromEntries(this.registry),
            importedAgents: Object.fromEntries(this.importedAgents),
            rentalContracts: Object.fromEntries(this.rentalContracts),
            learningSessions: Object.fromEntries(this.learningSessions),
            timestamp: new Date().toISOString()
        };
    }

    /**
     * Import exchange state
     */
    importState(state) {
        this.registry = new Map(Object.entries(state.registry || {}));
        this.importedAgents = new Map(Object.entries(state.importedAgents || {}));
        this.rentalContracts = new Map(Object.entries(state.rentalContracts || {}));
        this.learningSessions = new Map(Object.entries(state.learningSessions || {}));

        console.log('🌍 Global Agent Exchange state imported');
    }
}

// Global instance
const globalAgentExchange = new GlobalAgentExchange();

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { GlobalAgentExchange, globalAgentExchange };
}

// Global access for browser
if (typeof window !== 'undefined') {
    window.GlobalAgentExchange = GlobalAgentExchange;
    window.globalAgentExchange = globalAgentExchange;
}
