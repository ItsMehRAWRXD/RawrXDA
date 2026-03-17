/**
 * Multi-Agent Collaboration Swarm
 * Revolutionary: Multiple specialized AIs work together on complex tasks
 * Each agent has a role: Architect, Coder, Tester, Reviewer, Security Expert
 */

class MultiAgentSwarm {
    constructor(editor) {
        this.editor = editor;
        this.agents = [];
        this.swarmPanel = null;
        this.activeTask = null;
        this.chatHistory = [];
        
        this.initializeAgents();
        console.log('[AgentSwarm] 🐝 Multi-Agent Swarm initialized');
    }
    
    // ========================================================================
    // AGENT DEFINITIONS
    // ========================================================================
    
    initializeAgents() {
        this.agents = [
            {
                id: 'architect',
                name: 'Architect',
                emoji: '🏗️',
                color: '#00d4ff',
                role: 'System design and architecture planning',
                model: 'BigDaddyG:Latest',
                specialties: ['design patterns', 'system architecture', 'scalability'],
                active: false
            },
            {
                id: 'coder',
                name: 'Coder',
                emoji: '👨‍💻',
                color: '#00ff88',
                role: 'Writing clean, efficient code',
                model: 'BigDaddyG:Code',
                specialties: ['algorithms', 'code generation', 'refactoring'],
                active: false
            },
            {
                id: 'security',
                name: 'Security Expert',
                emoji: '🛡️',
                color: '#ff6b35',
                role: 'Security analysis and vulnerability detection',
                model: 'BigDaddyG:Security',
                specialties: ['penetration testing', 'secure coding', 'cryptography'],
                active: false
            },
            {
                id: 'tester',
                name: 'Tester',
                emoji: '🧪',
                color: '#a855f7',
                role: 'Test generation and quality assurance',
                model: 'BigDaddyG:Code',
                specialties: ['unit testing', 'integration testing', 'edge cases'],
                active: false
            },
            {
                id: 'reviewer',
                name: 'Code Reviewer',
                emoji: '🔍',
                color: '#ffa726',
                role: 'Code review and best practices',
                model: 'BigDaddyG:Latest',
                specialties: ['code quality', 'best practices', 'maintainability'],
                active: false
            },
            {
                id: 'optimizer',
                name: 'Performance Optimizer',
                emoji: '⚡',
                color: '#1dd1a1',
                role: 'Performance analysis and optimization',
                model: 'BigDaddyG:Code',
                specialties: ['performance tuning', 'profiling', 'big-O analysis'],
                active: false
            }
        ];
    }
    
    // ========================================================================
    // SWARM PANEL UI
    // ========================================================================
    
    createSwarmPanel() {
        const panel = document.createElement('div');
        panel.id = 'multi-agent-swarm-panel';
        panel.style.cssText = `
            position: fixed;
            left: 50%;
            top: 50%;
            transform: translate(-50%, -50%);
            width: 95vw;
            height: 95vh;
            background: rgba(10, 10, 30, 0.98);
            backdrop-filter: blur(20px);
            border: 2px solid var(--purple);
            border-radius: 15px;
            z-index: 10001;
            display: flex;
            flex-direction: column;
            box-shadow: 0 20px 80px rgba(168, 85, 247, 0.6);
        `;
        
        panel.innerHTML = `
            <!-- Header -->
            <div style="padding: 20px; background: rgba(0, 0, 0, 0.5); border-bottom: 2px solid var(--purple); display: flex; justify-content: space-between; align-items: center;">
                <div>
                    <h2 style="margin: 0 0 5px 0; color: var(--purple); font-size: 22px;">🐝 Multi-Agent Collaboration Swarm</h2>
                    <p style="margin: 0; color: #888; font-size: 13px;">6 specialized AI agents working together on your task</p>
                </div>
                <div style="display: flex; gap: 10px;">
                    <button onclick="window.multiAgentSwarm.startSwarm()" style="background: var(--green); color: #000; border: none; padding: 10px 20px; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 13px;">🚀 Start Swarm</button>
                    <button onclick="window.multiAgentSwarm.stopSwarm()" style="background: rgba(255, 71, 87, 0.2); border: 1px solid var(--red); color: var(--red); padding: 10px 20px; border-radius: 6px; cursor: pointer; font-size: 13px;">🛑 Stop</button>
                    <button onclick="window.multiAgentSwarm.closePanel()" style="background: rgba(255, 107, 53, 0.2); border: 1px solid var(--orange); color: var(--orange); padding: 10px 20px; border-radius: 6px; cursor: pointer; font-size: 13px;">✕ Close</button>
                </div>
            </div>
            
            <!-- Main Content -->
            <div style="flex: 1; display: flex; overflow: hidden;">
                <!-- Agent Status Panel -->
                <div style="width: 280px; background: rgba(0, 0, 0, 0.3); border-right: 1px solid rgba(168, 85, 247, 0.3); overflow-y: auto; padding: 20px;">
                    <h3 style="color: var(--purple); font-size: 14px; margin-bottom: 15px;">🤖 Agent Status</h3>
                    <div id="agent-status-list">
                        ${this.agents.map(agent => this.renderAgentCard(agent)).join('')}
                    </div>
                    
                    <div style="margin-top: 30px; padding-top: 20px; border-top: 1px solid rgba(168, 85, 247, 0.3);">
                        <h3 style="color: var(--purple); font-size: 14px; margin-bottom: 15px;">⚙️ Swarm Config</h3>
                        
                        <label style="display: block; margin-bottom: 12px; color: #ccc; font-size: 12px;">
                            <input type="checkbox" id="swarm-parallel" checked> Parallel Execution
                        </label>
                        <label style="display: block; margin-bottom: 12px; color: #ccc; font-size: 12px;">
                            <input type="checkbox" id="swarm-consensus" checked> Require Consensus
                        </label>
                        <label style="display: block; margin-bottom: 12px; color: #ccc; font-size: 12px;">
                            <input type="checkbox" id="swarm-auto-iterate"> Auto-iterate on Feedback
                        </label>
                    </div>
                </div>
                
                <!-- Collaboration Arena -->
                <div style="flex: 1; display: flex; flex-direction: column;">
                    <!-- Task Input -->
                    <div style="padding: 20px; background: rgba(0, 0, 0, 0.2); border-bottom: 1px solid rgba(168, 85, 247, 0.3);">
                        <label style="color: var(--cyan); font-size: 13px; font-weight: bold; display: block; margin-bottom: 8px;">📋 Swarm Task:</label>
                        <textarea id="swarm-task-input" placeholder="Describe what you want the agent swarm to build... (e.g., 'Create a secure REST API with authentication')" style="width: 100%; padding: 12px; background: rgba(0, 0, 0, 0.5); border: 1px solid rgba(168, 85, 247, 0.3); border-radius: 6px; color: #fff; font-size: 13px; resize: vertical; min-height: 80px; font-family: 'Consolas', monospace;"></textarea>
                    </div>
                    
                    <!-- Collaboration Feed -->
                    <div id="swarm-collaboration-feed" style="flex: 1; overflow-y: auto; padding: 20px; background: rgba(0, 0, 0, 0.2);">
                        <div style="text-align: center; padding: 60px 20px; color: #888;">
                            <div style="font-size: 64px; margin-bottom: 20px;">🐝</div>
                            <div style="font-size: 16px; font-weight: bold; color: var(--purple);">Ready for Collaboration</div>
                            <div style="font-size: 13px; margin-top: 10px;">Enter a task above and click "Start Swarm" to begin</div>
                        </div>
                    </div>
                    
                    <!-- Progress Bar -->
                    <div style="padding: 15px 20px; background: rgba(0, 0, 0, 0.3); border-top: 1px solid rgba(168, 85, 247, 0.3);">
                        <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
                            <span style="color: #888; font-size: 12px;">Swarm Progress</span>
                            <span id="swarm-progress-text" style="color: var(--cyan); font-size: 12px; font-weight: bold;">0%</span>
                        </div>
                        <div style="background: rgba(0, 0, 0, 0.5); border-radius: 10px; height: 12px; overflow: hidden;">
                            <div id="swarm-progress-bar" style="background: linear-gradient(90deg, var(--purple), var(--cyan)); height: 100%; width: 0%; transition: width 0.3s;"></div>
                        </div>
                    </div>
                </div>
                
                <!-- Results Panel -->
                <div style="width: 400px; background: rgba(0, 0, 0, 0.3); border-left: 1px solid rgba(168, 85, 247, 0.3); overflow-y: auto; padding: 20px;">
                    <h3 style="color: var(--purple); font-size: 14px; margin-bottom: 15px;">📊 Swarm Results</h3>
                    <div id="swarm-results">
                        <div style="text-align: center; padding: 40px 20px; color: #888; font-size: 12px;">
                            Results will appear here after swarm execution
                        </div>
                    </div>
                </div>
            </div>
        `;
        
        document.body.appendChild(panel);
        this.swarmPanel = panel;
        
        console.log('[AgentSwarm] ✅ Swarm panel created');
    }
    
    renderAgentCard(agent) {
        return `
            <div class="agent-card" style="margin-bottom: 12px; padding: 12px; background: rgba(0, 0, 0, 0.3); border-left: 3px solid ${agent.color}; border-radius: 6px; transition: all 0.2s; cursor: pointer;" onmouseover="this.style.background='rgba(0,0,0,0.5)'" onmouseout="this.style.background='rgba(0,0,0,0.3)'">
                <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
                    <div style="display: flex; align-items: center; gap: 8px;">
                        <span style="font-size: 20px;">${agent.emoji}</span>
                        <div>
                            <div style="color: ${agent.color}; font-weight: bold; font-size: 13px;">${agent.name}</div>
                            <div style="color: #888; font-size: 10px;">${agent.model}</div>
                        </div>
                    </div>
                    <div id="agent-status-${agent.id}" style="width: 10px; height: 10px; border-radius: 50%; background: #444;"></div>
                </div>
                <div style="color: #aaa; font-size: 11px; line-height: 1.4;">${agent.role}</div>
            </div>
        `;
    }
    
    // ========================================================================
    // SWARM ORCHESTRATION
    // ========================================================================
    
    async startSwarm() {
        const taskInput = document.getElementById('swarm-task-input');
        const task = taskInput.value.trim();
        
        if (!task) {
            this.addSwarmMessage('system', 'Please enter a task description first');
            return;
        }
        
        this.activeTask = task;
        this.chatHistory = [];
        
        console.log('[AgentSwarm] 🚀 Starting swarm for task:', task);
        
        // Clear feed
        const feed = document.getElementById('swarm-collaboration-feed');
        feed.innerHTML = '';
        
        this.addSwarmMessage('system', `🚀 Swarm initiated for task: "${task}"`);
        this.updateProgress(0);
        
        // Phase 1: Architect designs system
        await this.runAgentPhase('architect', `Design the architecture for: ${task}`);
        this.updateProgress(16);
        
        // Phase 2: Security reviews design
        await this.runAgentPhase('security', `Review the architecture for security concerns: ${this.getLastAgentResponse()}`);
        this.updateProgress(32);
        
        // Phase 3: Coder implements
        await this.runAgentPhase('coder', `Implement code based on this design: ${this.getLastAgentResponse()}`);
        this.updateProgress(50);
        
        // Phase 4: Tester generates tests
        await this.runAgentPhase('tester', `Generate comprehensive tests for: ${this.getLastAgentResponse()}`);
        this.updateProgress(66);
        
        // Phase 5: Optimizer checks performance
        await this.runAgentPhase('optimizer', `Analyze and optimize: ${this.getLastAgentResponse()}`);
        this.updateProgress(82);
        
        // Phase 6: Reviewer does final review
        await this.runAgentPhase('reviewer', `Final code review of: ${this.getLastAgentResponse()}`);
        this.updateProgress(100);
        
        // Compile results
        this.compileSwarmResults();
        
        this.addSwarmMessage('system', '✅ Swarm collaboration complete!');
    }
    
    async runAgentPhase(agentId, prompt) {
        const agent = this.agents.find(a => a.id === agentId);
        if (!agent) return;
        
        // Mark agent as active
        this.setAgentStatus(agentId, 'active');
        
        this.addSwarmMessage('agent-start', `${agent.emoji} ${agent.name} is working...`);
        
        // Query relevant memories for agent context
        let memoryContext = '';
        if (window.memory) {
            try {
                const relevantMemories = await window.memory.query(prompt, 3);
                if (relevantMemories && relevantMemories.length > 0) {
                    memoryContext = '\n\n[Previous context from memory]:\n' + 
                        relevantMemories.map(m => `- ${m.Content}`).join('\n');
                    console.log(`[AgentSwarm] 🧠 ${agent.name} retrieved`, relevantMemories.length, 'memories');
                }
            } catch (err) {
                console.warn('[AgentSwarm] Failed to query memories:', err);
            }
        }
        
        try {
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `You are ${agent.name}, specialized in ${agent.role}. ${prompt}${memoryContext}`,
                    model: agent.model
                })
            });
            
            const data = await response.json();
            
            // Store agent response in memory
            if (window.memory && data.response) {
                await window.memory.store(data.response, {
                    type: 'agent_response',
                    source: 'swarm',
                    context: { 
                        agent: agent.name, 
                        role: agent.role, 
                        prompt: prompt.substring(0, 200),
                        timestamp: new Date().toISOString() 
                    }
                }).catch(err => console.warn('[AgentSwarm] Failed to store response:', err));
            }
            
            this.chatHistory.push({
                agent: agentId,
                prompt,
                response: data.response
            });
            
            this.addSwarmMessage('agent-response', `${agent.emoji} **${agent.name}**: ${data.response}`, agent.color);
            
            // Mark agent as complete
            this.setAgentStatus(agentId, 'complete');
            
            // Small delay for visualization
            await new Promise(resolve => setTimeout(resolve, 500));
            
        } catch (error) {
            console.error(`[AgentSwarm] ❌ Error in ${agent.name}:`, error);
            this.addSwarmMessage('error', `❌ ${agent.name} encountered an error: ${error.message}`);
            this.setAgentStatus(agentId, 'error');
        }
    }
    
    getLastAgentResponse() {
        if (this.chatHistory.length === 0) return '';
        return this.chatHistory[this.chatHistory.length - 1].response;
    }
    
    stopSwarm() {
        console.log('[AgentSwarm] 🛑 Swarm stopped by user');
        this.addSwarmMessage('system', '🛑 Swarm execution stopped');
        
        // Reset all agent statuses
        this.agents.forEach(agent => {
            this.setAgentStatus(agent.id, 'idle');
        });
    }
    
    // ========================================================================
    // RESULTS COMPILATION
    // ========================================================================
    
    compileSwarmResults() {
        const resultsDiv = document.getElementById('swarm-results');
        
        // Extract code blocks from all responses
        const codeBlocks = [];
        this.chatHistory.forEach(entry => {
            const blocks = this.extractCodeBlocks(entry.response);
            codeBlocks.push(...blocks);
        });
        
        // Compile final output
        let finalCode = '';
        let finalTests = '';
        let finalDocs = '';
        
        this.chatHistory.forEach(entry => {
            const agent = this.agents.find(a => a.id === entry.agent);
            
            if (entry.agent === 'coder') {
                const blocks = this.extractCodeBlocks(entry.response);
                if (blocks.length > 0) {
                    finalCode = blocks[0].code;
                }
            } else if (entry.agent === 'tester') {
                const blocks = this.extractCodeBlocks(entry.response);
                if (blocks.length > 0) {
                    finalTests = blocks[0].code;
                }
            } else if (entry.agent === 'architect') {
                finalDocs += `## Architecture\n${entry.response}\n\n`;
            }
        });
        
        resultsDiv.innerHTML = `
            <div style="margin-bottom: 15px;">
                <button onclick="window.multiAgentSwarm.insertToEditor()" style="width: 100%; background: var(--green); color: #000; border: none; padding: 12px; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 13px;">📝 Insert to Editor</button>
            </div>
            
            <div style="margin-bottom: 15px;">
                <button onclick="window.multiAgentSwarm.exportResults()" style="width: 100%; background: rgba(0, 212, 255, 0.2); border: 1px solid var(--cyan); color: var(--cyan); padding: 10px; border-radius: 6px; cursor: pointer; font-size: 12px;">💾 Export All</button>
            </div>
            
            <div style="margin-top: 20px; padding-top: 20px; border-top: 1px solid rgba(168, 85, 247, 0.3);">
                <h4 style="color: var(--cyan); font-size: 12px; margin-bottom: 10px;">📊 Summary</h4>
                <div style="font-size: 11px; color: #ccc; line-height: 1.6;">
                    <div style="margin-bottom: 8px;">
                        <strong>Code Lines:</strong> <span style="color: var(--green);">${finalCode.split('\n').length}</span>
                    </div>
                    <div style="margin-bottom: 8px;">
                        <strong>Test Lines:</strong> <span style="color: var(--purple);">${finalTests.split('\n').length}</span>
                    </div>
                    <div style="margin-bottom: 8px;">
                        <strong>Agents Used:</strong> <span style="color: var(--orange);">${this.chatHistory.length}</span>
                    </div>
                    <div>
                        <strong>Total Tokens:</strong> <span style="color: var(--cyan);">${this.estimateTokens(this.chatHistory)}</span>
                    </div>
                </div>
            </div>
        `;
        
        // Store for insertion
        this.compiledResults = { code: finalCode, tests: finalTests, docs: finalDocs };
    }
    
    insertToEditor() {
        if (!this.compiledResults) return;
        
        const { code, tests, docs } = this.compiledResults;
        
        const fullOutput = `// Multi-Agent Swarm Results
// Task: ${this.activeTask}
// Generated by: Architect → Security → Coder → Tester → Optimizer → Reviewer

${docs}

// ==== IMPLEMENTATION ====
${code}

// ==== TESTS ====
${tests}
`;
        
        if (window.editor) {
            window.editor.setValue(fullOutput);
            console.log('[AgentSwarm] ✅ Results inserted to editor');
            this.addSwarmMessage('system', '✅ Results inserted to editor');
        }
    }
    
    exportResults() {
        if (!this.compiledResults) return;
        
        const { code, tests, docs } = this.compiledResults;
        
        const exportData = {
            task: this.activeTask,
            timestamp: new Date().toISOString(),
            agents: this.chatHistory.map(h => ({
                agent: this.agents.find(a => a.id === h.agent)?.name,
                response: h.response
            })),
            results: {
                code,
                tests,
                documentation: docs
            }
        };
        
        const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `agent-swarm-${Date.now()}.json`;
        a.click();
        
        console.log('[AgentSwarm] 💾 Results exported');
    }
    
    // ========================================================================
    // UI HELPERS
    // ========================================================================
    
    addSwarmMessage(type, message, color = null) {
        const feed = document.getElementById('swarm-collaboration-feed');
        const msgDiv = document.createElement('div');
        
        let bgColor, borderColor, textColor;
        
        switch(type) {
            case 'system':
                bgColor = 'rgba(0, 212, 255, 0.1)';
                borderColor = 'var(--cyan)';
                textColor = 'var(--cyan)';
                break;
            case 'agent-start':
                bgColor = 'rgba(168, 85, 247, 0.1)';
                borderColor = 'var(--purple)';
                textColor = 'var(--purple)';
                break;
            case 'agent-response':
                bgColor = 'rgba(0, 0, 0, 0.3)';
                borderColor = color || 'var(--green)';
                textColor = '#ccc';
                break;
            case 'error':
                bgColor = 'rgba(255, 71, 87, 0.1)';
                borderColor = 'var(--red)';
                textColor = 'var(--red)';
                break;
            default:
                bgColor = 'rgba(0, 0, 0, 0.3)';
                borderColor = '#444';
                textColor = '#ccc';
        }
        
        msgDiv.style.cssText = `
            margin-bottom: 15px;
            padding: 15px;
            background: ${bgColor};
            border-left: 3px solid ${borderColor};
            border-radius: 8px;
            font-size: 13px;
            color: ${textColor};
            line-height: 1.6;
            animation: slideIn 0.3s ease-out;
        `;
        
        msgDiv.innerHTML = message.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
        
        feed.appendChild(msgDiv);
        feed.scrollTop = feed.scrollHeight;
    }
    
    setAgentStatus(agentId, status) {
        const statusDot = document.getElementById(`agent-status-${agentId}`);
        if (!statusDot) return;
        
        const colors = {
            idle: '#444',
            active: '#ffa726',
            complete: '#00ff88',
            error: '#ff4757'
        };
        
        statusDot.style.background = colors[status] || '#444';
        
        if (status === 'active') {
            statusDot.style.animation = 'pulse 1s infinite';
        } else {
            statusDot.style.animation = 'none';
        }
    }
    
    updateProgress(percent) {
        const bar = document.getElementById('swarm-progress-bar');
        const text = document.getElementById('swarm-progress-text');
        
        if (bar) bar.style.width = percent + '%';
        if (text) text.textContent = percent + '%';
    }
    
    closePanel() {
        if (this.swarmPanel) {
            this.swarmPanel.remove();
            this.swarmPanel = null;
        }
    }
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    extractCodeBlocks(text) {
        const regex = /```(\w+)?\n([\s\S]*?)```/g;
        const blocks = [];
        let match;
        
        while ((match = regex.exec(text)) !== null) {
            blocks.push({
                language: match[1] || 'text',
                code: match[2].trim()
            });
        }
        
        return blocks;
    }
    
    estimateTokens(history) {
        const totalText = history.map(h => h.prompt + h.response).join('');
        return Math.ceil(totalText.length / 4);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

window.MultiAgentSwarm = MultiAgentSwarm;

setTimeout(() => {
    if (window.editor) {
        window.multiAgentSwarm = new MultiAgentSwarm(window.editor);
        
        // Keyboard shortcut: Ctrl+Shift+M
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.shiftKey && e.key === 'M') {
                e.preventDefault();
                if (!window.multiAgentSwarm.swarmPanel) {
                    window.multiAgentSwarm.createSwarmPanel();
                }
            }
        });
        
        console.log('[AgentSwarm] 🎯 Ready! Press Ctrl+Shift+M to activate swarm');
    }
}, 2000);

console.log('[AgentSwarm] 📦 Multi-Agent Swarm module loaded');

