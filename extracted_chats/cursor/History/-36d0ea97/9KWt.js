/**
 * Mock Federated Memory
 * Simulates shared memory and context across multiple agents
 */

export class MockFederatedMemory {
    constructor() {
        this.globalMemory = new Map();
        this.agentMemories = new Map();
        this.sharedContexts = new Map();
        this.knowledgeGraph = new Map();
        this.initialized = false;
    }

    async initialize() {
        if (this.initialized) return;

        console.log('🧠 MockFederatedMemory: Initializing...');

        // Simulate memory initialization
        await this.sleep(100);

        this.initialized = true;
        console.log('✅ MockFederatedMemory: Ready');
    }

    async sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    store(agentId, key, value, metadata = {}) {
        if (!this.agentMemories.has(agentId)) {
            this.agentMemories.set(agentId, new Map());
        }

        const agentMemory = this.agentMemories.get(agentId);
        agentMemory.set(key, {
            value,
            metadata,
            timestamp: Date.now(),
            accessCount: 0
        });

        // Also store in global memory if marked as shareable
        if (metadata.shareable) {
            this.globalMemory.set(`${agentId}:${key}`, {
                agentId,
                key,
                value,
                metadata,
                timestamp: Date.now()
            });
        }

        console.log(`🧠 MockFederatedMemory: Stored ${key} for agent ${agentId}`);
    }

    retrieve(agentId, key) {
        // Try agent-specific memory first
        const agentMemory = this.agentMemories.get(agentId);
        if (agentMemory && agentMemory.has(key)) {
            const item = agentMemory.get(key);
            item.accessCount++;
            item.lastAccessed = Date.now();
            return item.value;
        }

        // Try global memory
        const globalKey = `${agentId}:${key}`;
        if (this.globalMemory.has(globalKey)) {
            const item = this.globalMemory.get(globalKey);
            item.accessCount++;
            item.lastAccessed = Date.now();
            return item.value;
        }

        // Try other agents' shareable memory
        for (const [otherAgentId, memory] of this.agentMemories) {
            if (otherAgentId !== agentId && memory.has(key)) {
                const item = memory.get(key);
                if (item.metadata.shareable) {
                    item.accessCount++;
                    item.lastAccessed = Date.now();
                    return item.value;
                }
            }
        }

        return null;
    }

    async shareContext(fromAgentId, toAgentId, contextKey, contextData) {
        console.log(`🧠 MockFederatedMemory: Sharing context ${contextKey} from ${fromAgentId} to ${toAgentId}`);

        await this.sleep(50);

        // Store in shared contexts
        const sharedKey = `${fromAgentId}->${toAgentId}:${contextKey}`;
        this.sharedContexts.set(sharedKey, {
            fromAgent: fromAgentId,
            toAgent: toAgentId,
            key: contextKey,
            data: contextData,
            timestamp: Date.now()
        });

        // Add to recipient's memory
        this.store(toAgentId, `shared:${contextKey}`, contextData, {
            shared: true,
            fromAgent: fromAgentId,
            shareable: false
        });

        return { success: true, sharedKey };
    }

    async createKnowledgeNode(nodeId, content, type = 'concept', connections = []) {
        const node = {
            id: nodeId,
            content,
            type,
            connections,
            created: Date.now(),
            accessCount: 0,
            importance: Math.random()
        };

        this.knowledgeGraph.set(nodeId, node);

        // Create connections
        for (const connection of connections) {
            if (!this.knowledgeGraph.has(connection)) {
                await this.createKnowledgeNode(connection, `Related to ${nodeId}`, 'reference');
            }

            this.addKnowledgeEdge(nodeId, connection);
        }

        console.log(`🧠 MockFederatedMemory: Created knowledge node ${nodeId}`);
        return node;
    }

    addKnowledgeEdge(fromNode, toNode, weight = 1) {
        const fromNodeData = this.knowledgeGraph.get(fromNode);
        const toNodeData = this.knowledgeGraph.get(toNode);

        if (fromNodeData && toNodeData) {
            if (!fromNodeData.connections.includes(toNode)) {
                fromNodeData.connections.push(toNode);
            }
            if (!toNodeData.connections.includes(fromNode)) {
                toNodeData.connections.push(fromNode);
            }
        }
    }

    async queryKnowledge(query, maxResults = 10) {
        console.log(`🧠 MockFederatedMemory: Querying knowledge for "${query}"`);

        await this.sleep(100);

        const results = [];
        const queryLower = query.toLowerCase();

        // Search through knowledge graph
        for (const [nodeId, node] of this.knowledgeGraph) {
            if (node.content.toLowerCase().includes(queryLower)) {
                results.push({
                    nodeId,
                    content: node.content,
                    type: node.type,
                    importance: node.importance,
                    connections: node.connections.length
                });
            }
        }

        // Sort by relevance (importance + access count)
        results.sort((a, b) => (b.importance + b.connections * 0.1) - (a.importance + a.connections * 0.1));

        return results.slice(0, maxResults);
    }

    async getContextForAgent(agentId, contextType = 'all') {
        const agentMemory = this.agentMemories.get(agentId);
        if (!agentMemory) return {};

        const context = {};

        for (const [key, item] of agentMemory) {
            if (contextType === 'all' || item.metadata.type === contextType) {
                context[key] = {
                    value: item.value,
                    metadata: item.metadata,
                    lastAccessed: item.lastAccessed,
                    accessCount: item.accessCount
                };
            }
        }

        return context;
    }

    async consolidateMemory() {
        console.log('🧠 MockFederatedMemory: Consolidating memory...');

        await this.sleep(200);

        // Simulate memory consolidation
        const consolidation = {
            totalNodes: this.knowledgeGraph.size,
            totalEdges: this.calculateTotalEdges(),
            sharedContexts: this.sharedContexts.size,
            agentCount: this.agentMemories.size,
            consolidationRatio: Math.random() * 0.3 + 0.7,
            timestamp: Date.now()
        };

        console.log(`🧠 MockFederatedMemory: Consolidation complete - ${consolidation.consolidationRatio * 100}% efficiency`);
        return consolidation;
    }

    calculateTotalEdges() {
        let totalEdges = 0;
        for (const node of this.knowledgeGraph.values()) {
            totalEdges += node.connections.length;
        }
        return totalEdges / 2; // Divide by 2 since edges are bidirectional
    }

    getMemoryStats() {
        const agentStats = [];
        let totalAgentMemory = 0;
        let totalGlobalMemory = this.globalMemory.size;

        for (const [agentId, memory] of this.agentMemories) {
            const itemCount = memory.size;
            const totalSize = Array.from(memory.values()).reduce((sum, item) => {
                return sum + JSON.stringify(item.value).length;
            }, 0);

            totalAgentMemory += totalSize;

            agentStats.push({
                agentId,
                itemCount,
                totalSize,
                averageItemSize: itemCount > 0 ? totalSize / itemCount : 0
            });
        }

        return {
            totalAgents: this.agentMemories.size,
            totalGlobalMemory,
            totalAgentMemory,
            knowledgeNodes: this.knowledgeGraph.size,
            sharedContexts: this.sharedContexts.size,
            agentStats,
            consolidationHistory: this.consolidationHistory || []
        };
    }

    async cleanup(maxAge = 7 * 24 * 60 * 60 * 1000) { // 7 days
        console.log('🧠 MockFederatedMemory: Cleaning up old memories...');

        const now = Date.now();
        let cleanedCount = 0;

        // Clean up agent memories
        for (const [agentId, memory] of this.agentMemories) {
            for (const [key, item] of memory) {
                if (item.metadata.ephemeral && now - item.timestamp > maxAge) {
                    memory.delete(key);
                    cleanedCount++;
                }
            }
        }

        // Clean up shared contexts
        for (const [key, context] of this.sharedContexts) {
            if (now - context.timestamp > maxAge) {
                this.sharedContexts.delete(key);
                cleanedCount++;
            }
        }

        console.log(`🧠 MockFederatedMemory: Cleaned up ${cleanedCount} old memory items`);
        return cleanedCount;
    }

    async exportMemory(agentId = null) {
        if (agentId) {
            // Export specific agent memory
            const agentMemory = this.agentMemories.get(agentId);
            if (!agentMemory) {
                throw new Error(`Agent memory not found: ${agentId}`);
            }

            return {
                agentId,
                memory: Object.fromEntries(agentMemory),
                exported: Date.now()
            };
        } else {
            // Export all memory
            const allMemory = {};
            for (const [id, memory] of this.agentMemories) {
                allMemory[id] = Object.fromEntries(memory);
            }

            return {
                globalMemory: Object.fromEntries(this.globalMemory),
                knowledgeGraph: Object.fromEntries(this.knowledgeGraph),
                sharedContexts: Object.fromEntries(this.sharedContexts),
                exported: Date.now()
            };
        }
    }

    async importMemory(memoryData) {
        console.log('🧠 MockFederatedMemory: Importing memory...');

        await this.sleep(100);

        if (memoryData.agentId) {
            // Import agent-specific memory
            this.agentMemories.set(memoryData.agentId, new Map(Object.entries(memoryData.memory)));
        } else {
            // Import global memory
            this.globalMemory = new Map(Object.entries(memoryData.globalMemory));
            this.knowledgeGraph = new Map(Object.entries(memoryData.knowledgeGraph));
            this.sharedContexts = new Map(Object.entries(memoryData.sharedContexts));
        }

        console.log('✅ MockFederatedMemory: Memory imported successfully');
        return { success: true };
    }

    getFederationStatus() {
        return {
            initialized: this.initialized,
            agentCount: this.agentMemories.size,
            globalMemoryEntries: this.globalMemory.size,
            knowledgeNodes: this.knowledgeGraph.size,
            sharedContexts: this.sharedContexts.size,
            memoryStats: this.getMemoryStats()
        };
    }
}
