// Swarm Tracker - Live telemetry for 50+ agent swarm
// Tracks agent activity, finds "mode twinsies", and provides tactical HUD data

const SwarmTracker = {
    agents: new Map(),
    telemetryInterval: 3000, // 3 seconds
    maxHistory: 100,

    // Register/update agent in swarm
    update(name, stats) {
        const existing = this.agents.get(name) || {
            name,
            registeredAt: Date.now(),
            lastSeen: Date.now(),
            taskHistory: [],
            capabilities: [],
            ...stats
        };

        // Update with new stats
        Object.assign(existing, stats, {
            lastSeen: Date.now(),
            uptime: Date.now() - existing.registeredAt
        });

        this.agents.set(name, existing);

        // Keep swarm size manageable
        if (this.agents.size > 50) {
            const oldest = Array.from(this.agents.entries())
                .sort((a, b) => a[1].lastSeen - b[1].lastSeen)[0];
            this.agents.delete(oldest[0]);
        }

        Telemetry.log('SwarmAgentUpdated', {
            agent: name,
            activeAgents: this.agents.size,
            uptime: existing.uptime
        });
    },

    // Find "mode twinsies" - agents with overlapping capabilities
    getTwinsies() {
        const entries = Array.from(this.agents.entries());
        const twins = [];

        for (let i = 0; i < entries.length; i++) {
            for (let j = i + 1; j < entries.length; j++) {
                const [aName, aStats] = entries[i];
                const [bName, bStats] = entries[j];

                // Find shared capabilities
                const shared = aStats.capabilities.filter(cap =>
                    bStats.capabilities.includes(cap)
                );

                if (shared.length > 0) {
                    twins.push({
                        a: aName,
                        b: bName,
                        shared,
                        overlap: shared.length / Math.max(aStats.capabilities.length, bStats.capabilities.length)
                    });
                }
            }
        }

        // Sort by overlap percentage (highest first)
        return twins.sort((a, b) => b.overlap - a.overlap);
    },

    // Get comprehensive swarm statistics
    getStats() {
        const agents = Array.from(this.agents.values());

        if (agents.length === 0) {
            return {
                total: 0,
                online: 0,
                byDomain: {},
                avgUptime: 0,
                avgTasks: 0,
                avgSuccessRate: 0,
                avgLatency: 0
            };
        }

        const online = agents.filter(a => a.status === 'online');
        const byDomain = agents.reduce((acc, agent) => {
            acc[agent.domain] = (acc[agent.domain] || 0) + 1;
            return acc;
        }, {});

        const totalTasks = agents.reduce((sum, a) => sum + (a.taskCount || 0), 0);
        const totalSuccesses = agents.reduce((sum, a) => sum + (a.successes || 0), 0);
        const totalLatency = agents.reduce((sum, a) => sum + (a.avgLatency || 0), 0);

        return {
            total: agents.length,
            online: online.length,
            byDomain,
            avgUptime: agents.reduce((sum, a) => sum + a.uptime, 0) / agents.length,
            avgTasks: totalTasks / agents.length,
            avgSuccessRate: totalSuccesses / (totalTasks || 1),
            avgLatency: totalLatency / agents.length,
            twinsies: this.getTwinsies().length
        };
    },

    // Record task completion for an agent
    recordTask(name, task, result, metadata = {}) {
        const agent = this.agents.get(name);
        if (agent) {
            const taskRecord = {
                task,
                result,
                timestamp: Date.now(),
                duration: metadata.duration || 0,
                success: result === 'success'
            };

            agent.taskHistory.push(taskRecord);

            // Keep history manageable
            if (agent.taskHistory.length > this.maxHistory) {
                agent.taskHistory = agent.taskHistory.slice(-this.maxHistory);
            }

            // Update agent stats
            agent.taskCount = (agent.taskCount || 0) + 1;
            agent.successes = (agent.successes || 0) + (result === 'success' ? 1 : 0);
            agent.avgLatency = metadata.duration ?
                ((agent.avgLatency || 0) * (agent.taskCount - 1) + metadata.duration) / agent.taskCount :
                (agent.avgLatency || 0);

            Telemetry.log('SwarmTaskRecorded', {
                agent: name,
                task,
                result,
                duration: metadata.duration
            });
        }
    },

    // Get agent recommendations for a task
    recommendForTask(task) {
        const domain = this.getTaskDomain(task);
        const candidates = Array.from(this.agents.values())
            .filter(agent =>
                agent.capabilities.some(cap => task.includes(cap) || cap === 'general') &&
                agent.status === 'online'
            )
            .sort((a, b) => {
                // Score based on capability match, power level, and recent performance
                const aScore = this.calculateAgentScore(a, task, domain);
                const bScore = this.calculateAgentScore(b, task, domain);
                return bScore - aScore;
            })
            .slice(0, 3); // Top 3 recommendations

        return candidates;
    },

    calculateAgentScore(agent, task, domain) {
        let score = 0;

        // Capability match bonus
        const hasRelevantCapability = agent.capabilities.some(cap =>
            task.includes(cap) || cap === 'general'
        );
        if (hasRelevantCapability) score += 50;

        // Domain match bonus
        if (agent.domain === domain) score += 30;

        // Power level bonus
        score += agent.powerLevel * 0.2;

        // Recent success rate bonus
        const recentTasks = agent.taskHistory.slice(-5);
        const successRate = recentTasks.filter(t => t.success).length / recentTasks.length;
        score += successRate * 20;

        return Math.min(score, 100);
    },

    getTaskDomain(task) {
        const lowerTask = task.toLowerCase();

        if (lowerTask.includes('compile') || lowerTask.includes('asm') || lowerTask.includes('pe')) {
            return 'compiler';
        }
        if (lowerTask.includes('shellcode') || lowerTask.includes('payload')) {
            return 'security';
        }
        if (lowerTask.includes('browser') || lowerTask.includes('web')) {
            return 'web';
        }
        if (lowerTask.includes('ui') || lowerTask.includes('interface')) {
            return 'ui';
        }
        if (lowerTask.includes('test') || lowerTask.includes('debug')) {
            return 'testing';
        }

        return 'general';
    },

    // Get collaboration opportunities
    getCollaborationOpportunities() {
        const twins = this.getTwinsies();
        return twins.map(twin => ({
            agents: [twin.a, twin.b],
            sharedCapabilities: twin.shared,
            collaborationType: 'capability-sharing',
            potential: twin.overlap > 0.5 ? 'high' : twin.overlap > 0.3 ? 'medium' : 'low'
        }));
    }
};

// Auto-update swarm telemetry
if (typeof window !== 'undefined') {
    // Update agent heartbeats
    setInterval(() => {
        for (const [name] of SwarmTracker.agents) {
            SwarmTracker.update(name, { status: 'online' });
        }
    }, SwarmTracker.telemetryInterval);

    // Export to global scope
    window.SwarmTracker = SwarmTracker;
}
