/**
 * BigDaddyG IDE - Enhanced Agent Panel
 * Full Cursor-style UI with model tuning, deep research, thinking toggles, TODO system
 */

const { EventEmitter } = require('events');

class AgentPanelEnhanced extends EventEmitter {
    constructor(orchestraClient) {
        super();
        
        this.orchestraClient = orchestraClient;
        this.orchestraReady = false;
        
        // Wait for Orchestra client to be ready
        this.initializeOrchestraClient();
        
        // Agent state
        this.currentAgent = 'agent'; // agent, composer, coder, chat, plan
        this.currentModel = 'bigdaddyg-latest';
        this.quality = 'auto'; // auto, fast, max
        this.isPinned = false;
        
        // Feature toggles
        this.features = {
            thinking: true,
            webSearch: true,
            deepResearch: false,
            codeExecution: true,
            fileAccess: true
        };
        
        // TODO system
        this.todos = [];
        this.showTodos = false;
        
        // Conversation history
        this.messages = [];
        this.currentInput = '';
        
        // Attachments
        this.attachedFiles = [];
        this.maxAttachmentSize = 10 * 1024 * 1024 * 1024; // 10GB
        
        console.log('[Agent Panel Enhanced] Initialized');
    }
    
    /**
     * Initialize Orchestra client connection
     */
    async initializeOrchestraClient() {
        // If client already provided and ready, use it
        if (this.orchestraClient && typeof this.orchestraClient.sendMessage === 'function') {
            this.orchestraReady = true;
            console.log('[Agent Panel] Orchestra client ready');
            return;
        }
        
        // Wait for global Orchestra client to become available
        const checkClient = () => {
            // Check window.orchestraClient
            if (window.orchestraClient && typeof window.orchestraClient.sendMessage === 'function') {
                this.orchestraClient = window.orchestraClient;
                this.orchestraReady = true;
                console.log('[Agent Panel] Orchestra client connected');
                return true;
            }
            
            // Check status manager for Orchestra availability
            if (window.statusManager && window.statusManager.isServiceRunning('orchestra')) {
                // Try to create client
                if (window.OrchestraClient) {
                    this.orchestraClient = new window.OrchestraClient();
                    this.orchestraReady = true;
                    console.log('[Agent Panel] Orchestra client created');
                    return true;
                }
            }
            
            return false;
        };
        
        // Try immediately
        if (!checkClient()) {
            // Poll until available
            const interval = setInterval(() => {
                if (checkClient()) {
                    clearInterval(interval);
                    this.updateControlsState();
                }
            }, 500);
            
            // Timeout after 30 seconds
            setTimeout(() => {
                clearInterval(interval);
                if (!this.orchestraReady) {
                    console.warn('[Agent Panel] Orchestra client not available - some features disabled');
                    this.updateControlsState();
                }
            }, 30000);
        }
    }
    
    /**
     * Update UI controls based on Orchestra availability
     */
    updateControlsState() {
        const buttons = document.querySelectorAll('.agent-quick-action, .btn-send');
        buttons.forEach(btn => {
            if (!this.orchestraReady) {
                btn.disabled = true;
                btn.title = 'Orchestra service not available';
                btn.style.opacity = '0.5';
            } else {
                btn.disabled = false;
                btn.title = '';
                btn.style.opacity = '1';
            }
        });
    }
    
    /**
     * Render the complete agent panel
     */
    render() {
        return `
            <div id="agent-panel-enhanced" class="agent-panel-enhanced">
                ${this.renderHeader()}
                ${this.renderTabs()}
                ${this.renderConversation()}
                ${this.renderTodoPanel()}
                ${this.renderInputArea()}
            </div>
        `;
    }
    
    /**
     * Render header with model selector and controls
     */
    renderHeader() {
        return `
            <div class="agent-header">
                <div class="agent-status">
                    <span class="agent-icon">🤖</span>
                    <span class="agent-label">Agent</span>
                    <span class="agent-state ${this.isPinned ? 'pinned' : 'ready'}">
                        ${this.isPinned ? 'Pinned' : 'Ready'}
                    </span>
                </div>
                
                <div class="agent-controls">
                    <button class="btn-pin ${this.isPinned ? 'active' : ''}" 
                            onclick="agentPanel.togglePin()"
                            title="Pin agent panel">
                        📌
                    </button>
                    <button class="btn-todos ${this.showTodos ? 'active' : ''}" 
                            onclick="agentPanel.toggleTodos()"
                            title="Toggle TODO list">
                        📋 ${this.todos.length > 0 ? `(${this.todos.filter(t => !t.completed).length})` : ''}
                    </button>
                    <button class="btn-clear" 
                            onclick="agentPanel.clearConversation()"
                            title="Clear conversation">
                        🗑️
                    </button>
                    <button class="btn-close" 
                            onclick="agentPanel.close()"
                            title="Close agent panel">
                        ✕
                    </button>
                </div>
            </div>
        `;
    }
    
    /**
     * Render agent mode tabs
     */
    renderTabs() {
        const tabs = [
            { id: 'agent', icon: '🎯', label: 'Agent' },
            { id: 'composer', icon: '🎼', label: 'Composer' },
            { id: 'coder', icon: '👨‍💻', label: 'Coder' },
            { id: 'chat', icon: '💬', label: 'Chat' },
            { id: 'plan', icon: '📋', label: 'Plan' }
        ];
        
        return `
            <div class="agent-tabs">
                ${tabs.map(tab => `
                    <button class="agent-tab ${this.currentAgent === tab.id ? 'active' : ''}"
                            onclick="agentPanel.switchAgent('${tab.id}')">
                        <span class="tab-icon">${tab.icon}</span>
                        <span class="tab-label">${tab.label}</span>
                    </button>
                `).join('')}
            </div>
            
            <div class="agent-settings">
                ${this.renderQualitySelector()}
                ${this.renderModelSelector()}
                ${this.renderFeatureToggles()}
            </div>
        `;
    }
    
    /**
     * Render quality selector
     */
    renderQualitySelector() {
        return `
            <div class="quality-selector">
                <label>Quality:</label>
                <div class="quality-options">
                    ${['auto', 'fast', 'max'].map(q => `
                        <button class="quality-btn ${this.quality === q ? 'active' : ''}"
                                onclick="agentPanel.setQuality('${q}')">
                            ${q.charAt(0).toUpperCase() + q.slice(1)}
                        </button>
                    `).join('')}
                </div>
            </div>
        `;
    }
    
    /**
     * Render model selector
     */
    renderModelSelector() {
        const models = [
            { id: 'bigdaddyg-latest', name: '🧠 BigDaddyG Latest', context: '1M' },
            { id: 'bigdaddyg-fast', name: '⚡ BigDaddyG Fast', context: '128K' },
            { id: 'claude-sonnet-4', name: '🤖 Claude Sonnet 4', context: '200K' },
            { id: 'gpt-4-turbo', name: '🌟 GPT-4 Turbo', context: '128K' },
            { id: 'ollama-codellama', name: '🦙 CodeLlama (Local)', context: '16K' }
        ];
        
        return `
            <div class="model-selector">
                <select id="model-select" 
                        onchange="agentPanel.setModel(this.value)"
                        value="${this.currentModel}">
                    ${models.map(model => `
                        <option value="${model.id}" ${this.currentModel === model.id ? 'selected' : ''}>
                            ${model.name} (${model.context})
                        </option>
                    `).join('')}
                </select>
            </div>
        `;
    }
    
    /**
     * Render feature toggles
     */
    renderFeatureToggles() {
        const features = [
            { id: 'thinking', icon: '🧠', label: 'Thinking', tooltip: 'Show agent reasoning process' },
            { id: 'webSearch', icon: '🌐', label: 'Web Search', tooltip: 'Enable real-time web search' },
            { id: 'deepResearch', icon: '🔬', label: 'Deep Research', tooltip: 'Enable comprehensive research mode' },
            { id: 'codeExecution', icon: '▶️', label: 'Execute', tooltip: 'Allow code execution' },
            { id: 'fileAccess', icon: '📁', label: 'Files', tooltip: 'Allow file system access' }
        ];
        
        return `
            <div class="feature-toggles">
                ${features.map(feature => `
                    <label class="toggle-item" title="${feature.tooltip}">
                        <input type="checkbox" 
                               id="toggle-${feature.id}"
                               ${this.features[feature.id] ? 'checked' : ''}
                               onchange="agentPanel.toggleFeature('${feature.id}', this.checked)">
                        <span class="toggle-icon">${feature.icon}</span>
                        <span class="toggle-label">${feature.label}</span>
                    </label>
                `).join('')}
            </div>
        `;
    }
    
    /**
     * Render conversation area
     */
    renderConversation() {
        return `
            <div class="agent-conversation" id="agent-conversation">
                ${this.messages.length === 0 ? this.renderEmptyState() : ''}
                ${this.messages.map((msg, index) => this.renderMessage(msg, index)).join('')}
            </div>
        `;
    }
    
    /**
     * Render empty state
     */
    renderEmptyState() {
        const suggestions = {
            agent: [
                'Create a complete authentication system',
                'Build a REST API with error handling',
                'Set up a CI/CD pipeline'
            ],
            composer: [
                'Generate a multi-file React component',
                'Create a full-stack application scaffold',
                'Build a microservices architecture'
            ],
            coder: [
                'Implement a specific algorithm',
                'Fix this bug in my code',
                'Optimize this function'
            ],
            chat: [
                'Explain how React hooks work',
                'What are design patterns?',
                'Help me debug this error'
            ],
            plan: [
                'Plan a migration to TypeScript',
                'Design a database schema',
                'Outline a testing strategy'
            ]
        };
        
        return `
            <div class="empty-state">
                <div class="empty-icon">✨</div>
                <h3>Start a conversation with ${this.currentAgent}</h3>
                <p>Try asking:</p>
                <div class="suggestions">
                    ${suggestions[this.currentAgent].map(suggestion => `
                        <button class="suggestion-btn" 
                                onclick="agentPanel.useSuggestion('${suggestion.replace(/'/g, "\\'")}')">
                            ${suggestion}
                        </button>
                    `).join('')}
                </div>
            </div>
        `;
    }
    
    /**
     * Render a single message
     */
    renderMessage(msg, index) {
        const isUser = msg.role === 'user';
        
        return `
            <div class="message ${isUser ? 'user-message' : 'agent-message'}" data-message-index="${index}">
                <div class="message-header">
                    <span class="message-avatar">${isUser ? '👤' : '🤖'}</span>
                    <span class="message-author">${isUser ? 'You' : this.currentAgent}</span>
                    <span class="message-time">${this.formatTime(msg.timestamp)}</span>
                    ${msg.thinking ? '<span class="thinking-badge">💭 Thinking...</span>' : ''}
                </div>
                <div class="message-content">
                    ${msg.thinking ? `<div class="thinking-process">${this.renderThinking(msg.thinking)}</div>` : ''}
                    ${this.renderMessageContent(msg.content)}
                    ${msg.attachments ? this.renderAttachments(msg.attachments) : ''}
                    ${msg.actions ? this.renderActions(msg.actions) : ''}
                </div>
            </div>
        `;
    }
    
    /**
     * Render thinking process
     */
    renderThinking(thinking) {
        return `
            <div class="thinking-box">
                <div class="thinking-header">🧠 Reasoning Process</div>
                <div class="thinking-content">${thinking}</div>
            </div>
        `;
    }
    
    /**
     * Render message content
     */
    renderMessageContent(content) {
        // Convert markdown to HTML (simplified)
        let html = content
            .replace(/```(\w+)?\n([\s\S]*?)```/g, (match, lang, code) => {
                return `<pre><code class="language-${lang || 'text'}">${this.escapeHtml(code)}</code></pre>`;
            })
            .replace(/`([^`]+)`/g, '<code>$1</code>')
            .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
            .replace(/\*([^*]+)\*/g, '<em>$1</em>')
            .replace(/\n/g, '<br>');
        
        return html;
    }
    
    /**
     * Render TODO panel
     */
    renderTodoPanel() {
        if (!this.showTodos) return '';
        
        return `
            <div class="todo-panel" id="todo-panel">
                <div class="todo-header">
                    <h4>📋 TODO List</h4>
                    <button class="btn-add-todo" onclick="agentPanel.addTodo()">+ Add</button>
                </div>
                <div class="todo-list">
                    ${this.todos.length === 0 ? 
                        '<div class="empty-todos">No tasks yet. Agent will create them as you work.</div>' :
                        this.todos.map((todo, index) => this.renderTodo(todo, index)).join('')
                    }
                </div>
            </div>
        `;
    }
    
    /**
     * Render a single TODO item
     */
    renderTodo(todo, index) {
        return `
            <div class="todo-item ${todo.completed ? 'completed' : ''} ${todo.status}" data-todo-index="${index}">
                <input type="checkbox" 
                       class="todo-checkbox"
                       ${todo.completed ? 'checked' : ''}
                       onchange="agentPanel.toggleTodo(${index})">
                <div class="todo-content">
                    <div class="todo-text">${todo.text}</div>
                    ${todo.subtasks ? `
                        <div class="todo-subtasks">
                            ${todo.subtasks.map((subtask, subIndex) => `
                                <div class="todo-subtask ${subtask.completed ? 'completed' : ''}">
                                    <input type="checkbox" 
                                           ${subtask.completed ? 'checked' : ''}
                                           onchange="agentPanel.toggleSubtask(${index}, ${subIndex})">
                                    <span>${subtask.text}</span>
                                </div>
                            `).join('')}
                        </div>
                    ` : ''}
                </div>
                <div class="todo-actions">
                    <span class="todo-status ${todo.status}">${todo.status || 'pending'}</span>
                    <button class="btn-delete-todo" onclick="agentPanel.deleteTodo(${index})">×</button>
                </div>
            </div>
        `;
    }
    
    /**
     * Render input area
     */
    renderInputArea() {
        return `
            <div class="agent-input-area">
                ${this.attachedFiles.length > 0 ? this.renderAttachedFiles() : ''}
                
                <div class="input-wrapper">
                    <button class="btn-attach" 
                            onclick="agentPanel.attachFiles()"
                            title="Attach files (up to 10GB)">
                        📎
                    </button>
                    
                    <textarea id="agent-input" 
                              class="agent-input"
                              placeholder="Ask ${this.currentAgent} anything... (supports @file mentions, images, and code)"
                              onkeydown="agentPanel.handleKeyDown(event)"
                              oninput="agentPanel.handleInput(event)"></textarea>
                    
                    <button class="btn-send ${this.currentInput.trim().length > 0 ? 'active' : ''}" 
                            onclick="agentPanel.sendMessage()"
                            title="Send (Ctrl+Enter)">
                        Send
                    </button>
                </div>
                
                <div class="input-hints">
                    <span class="hint">💡 Try @file.js to reference files</span>
                    <span class="hint">🎨 Paste images directly</span>
                    <span class="hint">⌨️ Ctrl+Enter to send</span>
                </div>
            </div>
        `;
    }
    
    /**
     * Render attached files
     */
    renderAttachedFiles() {
        return `
            <div class="attached-files">
                ${this.attachedFiles.map((file, index) => `
                    <div class="attached-file">
                        <span class="file-icon">${this.getFileIcon(file.type)}</span>
                        <span class="file-name">${file.name}</span>
                        <span class="file-size">${this.formatFileSize(file.size)}</span>
                        <button class="btn-remove-file" onclick="agentPanel.removeAttachment(${index})">×</button>
                    </div>
                `).join('')}
            </div>
        `;
    }
    
    /**
     * Switch agent mode
     */
    switchAgent(agentId) {
        this.currentAgent = agentId;
        this.emit('agent-changed', { agentId });
        this.updateView();
        console.log(`[Agent Panel] Switched to: ${agentId}`);
    }
    
    /**
     * Set quality
     */
    setQuality(quality) {
        this.quality = quality;
        this.emit('quality-changed', { quality });
        this.updateView();
        console.log(`[Agent Panel] Quality set to: ${quality}`);
    }
    
    /**
     * Set model
     */
    setModel(modelId) {
        this.currentModel = modelId;
        this.emit('model-changed', { modelId });
        this.updateView();
        console.log(`[Agent Panel] Model set to: ${modelId}`);
    }
    
    /**
     * Toggle feature
     */
    toggleFeature(featureId, enabled) {
        this.features[featureId] = enabled;
        this.emit('feature-toggled', { featureId, enabled });
        console.log(`[Agent Panel] ${featureId}: ${enabled ? 'enabled' : 'disabled'}`);
    }
    
    /**
     * Toggle pin
     */
    togglePin() {
        this.isPinned = !this.isPinned;
        this.updateView();
        console.log(`[Agent Panel] ${this.isPinned ? 'Pinned' : 'Unpinned'}`);
    }
    
    /**
     * Toggle TODO panel
     */
    toggleTodos() {
        this.showTodos = !this.showTodos;
        this.updateView();
    }
    
    /**
     * Add TODO
     */
    addTodo(text = null) {
        const todoText = text || prompt('Enter TODO:');
        if (!todoText) return;
        
        this.todos.push({
            text: todoText,
            completed: false,
            status: 'pending',
            createdAt: new Date().toISOString()
        });
        
        this.updateView();
        this.emit('todo-added', { text: todoText });
    }
    
    /**
     * Toggle TODO completion
     */
    toggleTodo(index) {
        this.todos[index].completed = !this.todos[index].completed;
        this.todos[index].status = this.todos[index].completed ? 'completed' : 'in_progress';
        this.updateView();
        this.emit('todo-toggled', { index, completed: this.todos[index].completed });
    }
    
    /**
     * Delete TODO
     */
    deleteTodo(index) {
        if (!confirm('Delete this TODO?')) return;
        this.todos.splice(index, 1);
        this.updateView();
        this.emit('todo-deleted', { index });
    }
    
    /**
     * Send message
     */
    async sendMessage() {
        const input = document.getElementById('agent-input');
        if (!input) return;
        
        const message = input.value.trim();
        if (!message && this.attachedFiles.length === 0) return;
        
        // Add user message
        this.messages.push({
            role: 'user',
            content: message,
            attachments: this.attachedFiles.slice(),
            timestamp: new Date()
        });
        
        // Clear input
        input.value = '';
        this.currentInput = '';
        this.attachedFiles = [];
        
        this.updateView();
        
        // Send to Orchestra
        try {
            // Check if Orchestra client is ready
            if (!this.orchestraReady || !this.orchestraClient) {
                throw new Error('Orchestra service not available. Please start Orchestra server.');
            }
            
            const response = await this.orchestraClient.sendMessage({
                agent: this.currentAgent,
                model: this.currentModel,
                quality: this.quality,
                features: this.features,
                message: message,
                history: this.messages
            });
            
            // Add agent response
            this.messages.push({
                role: 'assistant',
                content: response.content,
                thinking: this.features.thinking ? response.thinking : null,
                actions: response.actions,
                timestamp: new Date()
            });
            
            // Extract TODOs from response
            if (response.todos) {
                response.todos.forEach(todo => this.addTodo(todo));
            }
            
            this.updateView();
            this.scrollToBottom();
            
        } catch (error) {
            console.error('[Agent Panel] Error sending message:', error);
            this.showError('Failed to send message: ' + error.message);
        }
    }
    
    /**
     * Update view
     */
    updateView() {
        const panelElement = document.getElementById('agent-panel-enhanced');
        if (panelElement) {
            panelElement.innerHTML = this.render();
        }
    }
    
    /**
     * Utility: Format time
     */
    formatTime(date) {
        return new Date(date).toLocaleTimeString('en-US', { 
            hour: '2-digit', 
            minute: '2-digit' 
        });
    }
    
    /**
     * Utility: Format file size
     */
    formatFileSize(bytes) {
        if (bytes >= 1024 * 1024 * 1024) {
            return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
        } else if (bytes >= 1024 * 1024) {
            return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
        } else if (bytes >= 1024) {
            return (bytes / 1024).toFixed(2) + ' KB';
        }
        return bytes + ' B';
    }
    
    /**
     * Utility: Escape HTML
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    /**
     * Utility: Get file icon
     */
    getFileIcon(type) {
        const iconMap = {
            'image': '🖼️',
            'video': '🎥',
            'audio': '🎵',
            'pdf': '📄',
            'code': '💻',
            'zip': '📦'
        };
        
        for (const [key, icon] of Object.entries(iconMap)) {
            if (type.includes(key)) return icon;
        }
        
        return '📎';
    }
    
    /**
     * Scroll conversation to bottom
     */
    scrollToBottom() {
        const conversation = document.getElementById('agent-conversation');
        if (conversation) {
            conversation.scrollTop = conversation.scrollHeight;
        }
    }
}

module.exports = AgentPanelEnhanced;

