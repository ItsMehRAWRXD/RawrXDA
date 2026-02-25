/**
 * BigDaddyG IDE - Background Agent Manager
 * 
 * Manages background agents (Web Workers) for autonomous task processing
 * 
 * Features:
 * - Create and manage multiple agents
 * - Task queue management
 * - Progress tracking
 * - Notifications
 * - Results display
 */

class BackgroundAgentManager {
    constructor() {
        this.worker = null;
        this.agents = [];
        this.taskQueue = [];
        this.nextTaskId = 1;
        this.isWorkerReady = false;
        
        // Initialize worker
        this.initializeWorker();
        
        // Initialize UI
        this.initializeUI();
        
        // Request notification permission
        this.requestNotificationPermission();
    }
    
    // ============================================================================
    // WORKER INITIALIZATION
    // ============================================================================
    
    initializeWorker() {
        try {
            this.worker = new Worker('background-agent-worker.js');
            
            this.worker.addEventListener('message', (event) => {
                this.handleWorkerMessage(event.data);
            });
            
            this.worker.addEventListener('error', (error) => {
                console.error('Worker error:', error);
                this.showNotification('Worker error: ' + error.message, 'error');
            });
            
            console.log('✅ Background agent worker initialized');
            
        } catch (error) {
            console.error('Failed to initialize worker:', error);
            this.showNotification('Failed to initialize background agents', 'error');
        }
    }
    
    handleWorkerMessage(data) {
        const { taskId, type } = data;
        
        switch (type) {
            case 'WORKER_READY':
                this.isWorkerReady = true;
                console.log('🤖 Background agents ready:', data.capabilities);
                break;
                
            case 'PROGRESS':
                this.updateAgentProgress(taskId, data.progress, data.message);
                break;
                
            case 'COMPLETE':
                this.handleAgentComplete(taskId, data);
                break;
                
            case 'ERROR':
                this.handleAgentError(taskId, data.error);
                break;
        }
    }
    
    // ============================================================================
    // AGENT CREATION
    // ============================================================================
    
    createAgent(type, data, description = '') {
        if (!this.isWorkerReady) {
            this.showNotification('Background agents not ready yet', 'warning');
            return null;
        }
        
        const taskId = this.nextTaskId++;
        
        const agent = {
            id: taskId,
            type: type,
            description: description || this.getDefaultDescription(type),
            status: 'running',
            progress: 0,
            startTime: Date.now(),
            endTime: null,
            duration: null,
            result: null,
            error: null
        };
        
        this.agents.push(agent);
        this.updateAgentUI();
        
        // Send task to worker
        this.worker.postMessage({
            taskId: taskId,
            type: type,
            data: data
        });
        
        console.log(`🚀 Agent ${taskId} started: ${type}`);
        
        return agent;
    }
    
    getDefaultDescription(type) {
        const descriptions = {
            'FIX_BUG': 'Fixing bugs in code',
            'IMPLEMENT_FEATURE': 'Implementing new feature',
            'REFACTOR_CODE': 'Refactoring code for better quality',
            'GENERATE_TESTS': 'Generating unit tests',
            'OPTIMIZE_CODE': 'Optimizing code performance'
        };
        
        return descriptions[type] || 'Processing task';
    }
    
    // ============================================================================
    // AGENT LIFECYCLE
    // ============================================================================
    
    updateAgentProgress(taskId, progress, message) {
        const agent = this.agents.find(a => a.id === taskId);
        if (agent) {
            agent.progress = progress;
            agent.progressMessage = message;
            this.updateAgentUI();
        }
    }
    
    handleAgentComplete(taskId, data) {
        const agent = this.agents.find(a => a.id === taskId);
        if (!agent) return;
        
        agent.status = 'completed';
        agent.progress = 100;
        agent.endTime = Date.now();
        agent.duration = agent.endTime - agent.startTime;
        agent.result = data.result;
        
        this.updateAgentUI();
        this.showCompletionNotification(agent);
        
        console.log(`✅ Agent ${taskId} completed in ${(agent.duration / 1000).toFixed(2)}s`);
    }
    
    handleAgentError(taskId, error) {
        const agent = this.agents.find(a => a.id === taskId);
        if (!agent) return;
        
        agent.status = 'error';
        agent.endTime = Date.now();
        agent.duration = agent.endTime - agent.startTime;
        agent.error = error;
        
        this.updateAgentUI();
        this.showNotification(`Agent failed: ${error.message}`, 'error');
        
        console.error(`❌ Agent ${taskId} failed:`, error);
    }
    
    cancelAgent(taskId) {
        const agent = this.agents.find(a => a.id === taskId);
        if (!agent) return;
        
        agent.status = 'cancelled';
        agent.endTime = Date.now();
        agent.duration = agent.endTime - agent.startTime;
        
        this.updateAgentUI();
        this.showNotification('Agent cancelled', 'info');
    }
    
    clearAgent(taskId) {
        this.agents = this.agents.filter(a => a.id !== taskId);
        this.updateAgentUI();
    }
    
    clearAllCompleted() {
        this.agents = this.agents.filter(a => a.status === 'running');
        this.updateAgentUI();
    }
    
    // ============================================================================
    // UI MANAGEMENT
    // ============================================================================
    
    initializeUI() {
        const sidebar = document.getElementById('right-sidebar') || 
                        document.querySelector('.right-sidebar') || 
                        document.querySelector('.sidebar');
        
        if (!sidebar) {
            console.warn('Sidebar not found for agent panel');
            return;
        }
        
        const agentPanel = document.createElement('div');
        agentPanel.id = 'agent-panel';
        agentPanel.className = 'panel agent-panel';
        agentPanel.innerHTML = `
            <div class="panel-header">
                <h3>🤖 Background Agents</h3>
                <button id="agent-clear-all-btn" class="btn btn-sm btn-secondary" style="display: none;">
                    Clear Completed
                </button>
            </div>
            
            <div class="panel-content">
                <div class="agent-controls">
                    <h4>Create New Agent</h4>
                    
                    <select id="agent-type-selector" class="form-select">
                        <option value="">Select agent type...</option>
                        <option value="FIX_BUG">🐛 Fix Bug</option>
                        <option value="IMPLEMENT_FEATURE">✨ Implement Feature</option>
                        <option value="REFACTOR_CODE">♻️ Refactor Code</option>
                        <option value="GENERATE_TESTS">🧪 Generate Tests</option>
                        <option value="OPTIMIZE_CODE">⚡ Optimize Code</option>
                    </select>
                    
                    <textarea id="agent-task-input" 
                              class="form-textarea" 
                              placeholder="Describe the task..." 
                              rows="3"></textarea>
                    
                    <button id="agent-start-btn" class="btn btn-primary" disabled>
                        🚀 Start Agent
                    </button>
                </div>
                
                <div class="agent-list-section">
                    <h4>Active & Recent Agents</h4>
                    <div id="agent-list" class="agent-list">
                        <p class="empty-state">No agents running</p>
                    </div>
                </div>
            </div>
        `;
        
        sidebar.appendChild(agentPanel);
        
        this.attachEventListeners();
    }
    
    attachEventListeners() {
        // Agent type selector
        const typeSelector = document.getElementById('agent-type-selector');
        const taskInput = document.getElementById('agent-task-input');
        const startBtn = document.getElementById('agent-start-btn');
        
        if (typeSelector && taskInput && startBtn) {
            const updateStartButton = () => {
                startBtn.disabled = !typeSelector.value || !taskInput.value.trim();
            };
            
            typeSelector.addEventListener('change', updateStartButton);
            taskInput.addEventListener('input', updateStartButton);
            
            startBtn.addEventListener('click', () => {
                this.startAgentFromUI();
            });
        }
        
        // Clear all button
        const clearAllBtn = document.getElementById('agent-clear-all-btn');
        if (clearAllBtn) {
            clearAllBtn.addEventListener('click', () => {
                this.clearAllCompleted();
            });
        }
    }
    
    startAgentFromUI() {
        const typeSelector = document.getElementById('agent-type-selector');
        const taskInput = document.getElementById('agent-task-input');
        
        if (!typeSelector || !taskInput) return;
        
        const type = typeSelector.value;
        const description = taskInput.value.trim();
        
        if (!type || !description) return;
        
        // Prepare task data based on type
        let data = {};
        
        switch (type) {
            case 'FIX_BUG':
                data = {
                    code: window.editor ? window.editor.getValue() : '',
                    errorMessage: description,
                    context: {}
                };
                break;
                
            case 'IMPLEMENT_FEATURE':
                data = {
                    description: description,
                    existingCode: window.editor ? window.editor.getValue() : '',
                    language: 'javascript'
                };
                break;
                
            case 'REFACTOR_CODE':
                data = {
                    code: window.editor ? window.editor.getValue() : '',
                    options: {}
                };
                break;
                
            case 'GENERATE_TESTS':
                data = {
                    code: window.editor ? window.editor.getValue() : '',
                    framework: 'jest'
                };
                break;
                
            case 'OPTIMIZE_CODE':
                data = {
                    code: window.editor ? window.editor.getValue() : ''
                };
                break;
        }
        
        // Create agent
        this.createAgent(type, data, description);
        
        // Clear input
        taskInput.value = '';
        typeSelector.value = '';
        document.getElementById('agent-start-btn').disabled = true;
    }
    
    updateAgentUI() {
        const agentList = document.getElementById('agent-list');
        if (!agentList) return;
        
        if (this.agents.length === 0) {
            agentList.innerHTML = '<p class="empty-state">No agents running</p>';
            return;
        }
        
        agentList.innerHTML = this.agents.map(agent => {
            const icon = this.getAgentIcon(agent.type, agent.status);
            const statusClass = `agent-status-${agent.status}`;
            
            return `
                <div class="agent-card ${statusClass}" data-agent-id="${agent.id}">
                    <div class="agent-header">
                        <span class="agent-icon">${icon}</span>
                        <span class="agent-type">${this.formatAgentType(agent.type)}</span>
                        <span class="agent-status-badge ${statusClass}">
                            ${agent.status}
                        </span>
                    </div>
                    
                    <div class="agent-description">
                        ${agent.description}
                    </div>
                    
                    ${agent.status === 'running' ? `
                        <div class="agent-progress">
                            <div class="progress-bar">
                                <div class="progress-fill" style="width: ${agent.progress}%"></div>
                            </div>
                            <div class="progress-text">
                                ${agent.progressMessage || 'Working...'}
                            </div>
                        </div>
                    ` : ''}
                    
                    ${agent.status === 'completed' ? `
                        <div class="agent-result">
                            <div class="agent-duration">
                                ⏱️ ${this.formatDuration(agent.duration)}
                            </div>
                            <div class="agent-actions">
                                <button onclick="agentManager.viewResult(${agent.id})" class="btn btn-sm btn-primary">
                                    👁️ View Result
                                </button>
                                <button onclick="agentManager.applyResult(${agent.id})" class="btn btn-sm btn-success">
                                    ✅ Apply
                                </button>
                                <button onclick="agentManager.clearAgent(${agent.id})" class="btn btn-sm btn-secondary">
                                    🗑️ Clear
                                </button>
                            </div>
                        </div>
                    ` : ''}
                    
                    ${agent.status === 'error' ? `
                        <div class="agent-error">
                            <div class="error-message">❌ ${agent.error.message}</div>
                            <button onclick="agentManager.clearAgent(${agent.id})" class="btn btn-sm btn-secondary">
                                🗑️ Clear
                            </button>
                        </div>
                    ` : ''}
                    
                    ${agent.status === 'running' ? `
                        <div class="agent-actions">
                            <button onclick="agentManager.cancelAgent(${agent.id})" class="btn btn-sm btn-secondary">
                                ⏸️ Cancel
                            </button>
                        </div>
                    ` : ''}
                </div>
            `;
        }).join('');
        
        // Show/hide clear all button
        const clearAllBtn = document.getElementById('agent-clear-all-btn');
        if (clearAllBtn) {
            const hasCompleted = this.agents.some(a => a.status === 'completed' || a.status === 'error');
            clearAllBtn.style.display = hasCompleted ? 'inline-block' : 'none';
        }
    }
    
    getAgentIcon(type, status) {
        if (status === 'running') return '⚙️';
        if (status === 'completed') return '✅';
        if (status === 'error') return '❌';
        if (status === 'cancelled') return '⏸️';
        
        const icons = {
            'FIX_BUG': '🐛',
            'IMPLEMENT_FEATURE': '✨',
            'REFACTOR_CODE': '♻️',
            'GENERATE_TESTS': '🧪',
            'OPTIMIZE_CODE': '⚡'
        };
        
        return icons[type] || '🤖';
    }
    
    formatAgentType(type) {
        return type.split('_').map(word => 
            word.charAt(0) + word.slice(1).toLowerCase()
        ).join(' ');
    }
    
    formatDuration(ms) {
        if (ms < 1000) return `${ms}ms`;
        const seconds = (ms / 1000).toFixed(1);
        if (seconds < 60) return `${seconds}s`;
        const minutes = Math.floor(seconds / 60);
        const secs = (seconds % 60).toFixed(0);
        return `${minutes}m ${secs}s`;
    }
    
    // ============================================================================
    // RESULT HANDLING
    // ============================================================================
    
    viewResult(taskId) {
        const agent = this.agents.find(a => a.id === taskId);
        if (!agent || !agent.result) return;
        
        // Create result modal
        this.showResultModal(agent);
    }
    
    applyResult(taskId) {
        const agent = this.agents.find(a => a.id === taskId);
        if (!agent || !agent.result) return;
        
        // Apply result based on agent type
        switch (agent.type) {
            case 'FIX_BUG':
                if (window.editor && agent.result.fixedCode) {
                    window.editor.setValue(agent.result.fixedCode);
                    this.showNotification('Bug fix applied!', 'success');
                }
                break;
                
            case 'IMPLEMENT_FEATURE':
                if (window.editor && agent.result.code) {
                    const currentCode = window.editor.getValue();
                    window.editor.setValue(currentCode + '\n\n' + agent.result.code);
                    this.showNotification('Feature implementation added!', 'success');
                }
                break;
                
            case 'REFACTOR_CODE':
                if (window.editor && agent.result.refactoredCode) {
                    window.editor.setValue(agent.result.refactoredCode);
                    this.showNotification('Refactored code applied!', 'success');
                }
                break;
                
            case 'GENERATE_TESTS':
                if (window.editor && agent.result.testCode) {
                    // Create new file or append to current
                    const fileName = 'test.spec.js';
                    this.createNewFile(fileName, agent.result.testCode);
                    this.showNotification('Test file created!', 'success');
                }
                break;
                
            case 'OPTIMIZE_CODE':
                if (window.editor && agent.result.optimizedCode) {
                    window.editor.setValue(agent.result.optimizedCode);
                    this.showNotification(`Code optimized! Est. ${agent.result.performanceGain} improvement`, 'success');
                }
                break;
        }
    }
    
    showResultModal(agent) {
        const modal = document.createElement('div');
        modal.className = 'modal-overlay';
        modal.innerHTML = `
            <div class="modal agent-result-modal">
                <div class="modal-header">
                    <h3>${this.getAgentIcon(agent.type, agent.status)} ${this.formatAgentType(agent.type)} Result</h3>
                    <button class="modal-close" onclick="this.closest('.modal-overlay').remove()">×</button>
                </div>
                
                <div class="modal-body">
                    <div class="result-section">
                        <h4>Description</h4>
                        <p>${agent.description}</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Duration</h4>
                        <p>${this.formatDuration(agent.duration)}</p>
                    </div>
                    
                    ${this.renderAgentResult(agent)}
                </div>
                
                <div class="modal-footer">
                    <button onclick="agentManager.applyResult(${agent.id}); this.closest('.modal-overlay').remove();" class="btn btn-success">
                        ✅ Apply Result
                    </button>
                    <button onclick="this.closest('.modal-overlay').remove()" class="btn btn-secondary">
                        Close
                    </button>
                </div>
            </div>
        `;
        
        document.body.appendChild(modal);
    }
    
    renderAgentResult(agent) {
        const result = agent.result;
        
        switch (agent.type) {
            case 'FIX_BUG':
                return `
                    <div class="result-section">
                        <h4>Explanation</h4>
                        <p>${result.explanation}</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Changes</h4>
                        <ul>
                            ${result.changes.map(c => `<li>${c}</li>`).join('')}
                        </ul>
                    </div>
                    
                    <div class="result-section">
                        <h4>Fixed Code</h4>
                        <pre><code>${this.escapeHtml(result.fixedCode)}</code></pre>
                    </div>
                `;
                
            case 'IMPLEMENT_FEATURE':
                return `
                    <div class="result-section">
                        <h4>Explanation</h4>
                        <p>${result.explanation}</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Implementation</h4>
                        <pre><code>${this.escapeHtml(result.code)}</code></pre>
                    </div>
                    
                    ${result.testCases && result.testCases.length > 0 ? `
                        <div class="result-section">
                            <h4>Test Cases</h4>
                            <ul>
                                ${result.testCases.map(t => `<li><code>${this.escapeHtml(t)}</code></li>`).join('')}
                            </ul>
                        </div>
                    ` : ''}
                `;
                
            case 'REFACTOR_CODE':
                return `
                    <div class="result-section">
                        <h4>Improvements</h4>
                        <ul>
                            ${result.improvements.map(i => `<li>${i}</li>`).join('')}
                        </ul>
                    </div>
                    
                    <div class="result-section">
                        <h4>Metrics</h4>
                        <p>Lines changed: ${result.metrics.linesChanged}</p>
                        <p>Improvement score: ${(result.metrics.improvementScore * 100).toFixed(0)}%</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Refactored Code</h4>
                        <pre><code>${this.escapeHtml(result.refactoredCode)}</code></pre>
                    </div>
                `;
                
            case 'GENERATE_TESTS':
                return `
                    <div class="result-section">
                        <h4>Coverage</h4>
                        <p>${result.coverage} functions, ${result.testCases} test cases</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Test Code</h4>
                        <pre><code>${this.escapeHtml(result.testCode)}</code></pre>
                    </div>
                `;
                
            case 'OPTIMIZE_CODE':
                return `
                    <div class="result-section">
                        <h4>Optimizations Applied</h4>
                        <ul>
                            ${result.optimizations.map(o => `<li>${o}</li>`).join('')}
                        </ul>
                    </div>
                    
                    <div class="result-section">
                        <h4>Performance Gain</h4>
                        <p class="performance-gain">${result.performanceGain}</p>
                    </div>
                    
                    <div class="result-section">
                        <h4>Optimized Code</h4>
                        <pre><code>${this.escapeHtml(result.optimizedCode)}</code></pre>
                    </div>
                `;
                
            default:
                return `<pre><code>${JSON.stringify(result, null, 2)}</code></pre>`;
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    createNewFile(fileName, content) {
        // This should integrate with the IDE's file management system
        // For now, just show the content
        console.log(`Creating file: ${fileName}`);
        console.log(content);
        
        // If there's a file creation API, use it here
        if (window.createFile) {
            window.createFile(fileName, content);
        }
    }
    
    // ============================================================================
    // NOTIFICATIONS
    // ============================================================================
    
    requestNotificationPermission() {
        if ('Notification' in window && Notification.permission === 'default') {
            Notification.requestPermission();
        }
    }
    
    showCompletionNotification(agent) {
        // Browser notification
        if ('Notification' in window && Notification.permission === 'granted') {
            new Notification('🤖 Agent Complete!', {
                body: `${this.formatAgentType(agent.type)} finished in ${this.formatDuration(agent.duration)}`,
                icon: '/icon-agent.png',
                tag: `agent-${agent.id}`
            });
        }
        
        // In-app notification
        this.showNotification(
            `Agent completed: ${agent.description}`,
            'success'
        );
    }
    
    showNotification(message, type = 'info') {
        // Use existing notification system if available
        if (window.showNotification) {
            window.showNotification(message, type);
            return;
        }
        
        // Fallback notification
        const notification = document.createElement('div');
        notification.className = `notification notification-${type}`;
        notification.textContent = message;
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 12px 20px;
            background: ${type === 'success' ? '#4caf50' : type === 'error' ? '#f44336' : type === 'warning' ? '#ff9800' : '#2196f3'};
            color: white;
            border-radius: 4px;
            z-index: 10000;
            animation: slideIn 0.3s ease;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        `;
        
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.style.animation = 'slideOut 0.3s ease';
            setTimeout(() => notification.remove(), 300);
        }, 3000);
    }
}

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

let agentManager;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        agentManager = new BackgroundAgentManager();
        window.agentManager = agentManager;
    });
} else {
    agentManager = new BackgroundAgentManager();
    window.agentManager = agentManager;
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackgroundAgentManager;
}

