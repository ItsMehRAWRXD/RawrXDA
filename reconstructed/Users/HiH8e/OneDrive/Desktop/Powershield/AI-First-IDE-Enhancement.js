/**
 * AI-First IDE JavaScript Enhancement
 * Implements sophisticated contextual AI integration with subtle, non-obtrusive UX
 */

class AIFirstIDE {
    constructor() {
        this.aiAgent = {
            status: 'idle',
            currentTask: null,
            suggestions: [],
            searchResults: []
        };
        
        this.ui = {
            statusIndicator: null,
            chatPanel: null,
            suggestionOverlay: null,
            contextMenu: null
        };
        
        this.settings = {
            autoSuggestions: true,
            contextualActions: true,
            intelligentSearch: true,
            subtleAnimations: true
        };
        
        this.init();
    }
    
    init() {
        this.createUIElements();
        this.setupEventListeners();
        this.setupAIIntegration();
        this.applyAestheticEnhancements();
        
        console.log('🤖 AI-First IDE Enhanced - Contextual & Subtle');
    }
    
    createUIElements() {
        // AI Agent Status Indicator
        this.ui.statusIndicator = this.createElement('div', 'ai-agent-status', `
            <div class="ai-status-dot"></div>
            <span class="ai-status-text">AI Ready</span>
        `);
        
        // Collapsible AI Chat Panel
        this.ui.chatPanel = this.createElement('div', 'ai-chat-panel', `
            <div class="ai-chat-header">
                <span class="ai-chat-title">🤖 AI Assistant</span>
                <button class="ai-chat-toggle">⚡</button>
            </div>
            <div class="ai-chat-content">
                <div class="ai-chat-messages"></div>
                <div class="ai-chat-input-container">
                    <textarea class="ai-chat-input" placeholder="Ask AI about your code..."></textarea>
                </div>
            </div>
        `);
        
        // Subtle Suggestion Overlay
        this.ui.suggestionOverlay = this.createElement('div', 'ai-suggestion-overlay hidden');
        
        // Context-Aware Action Menu
        this.ui.contextMenu = this.createElement('div', 'context-ai-menu', `
            <div class="ai-action-item" data-action="refactor">
                <span class="ai-action-icon">🔄</span>
                <span>Refactor with AI</span>
            </div>
            <div class="ai-action-item" data-action="explain">
                <span class="ai-action-icon">💡</span>
                <span>Explain Code</span>
            </div>
            <div class="ai-action-item" data-action="optimize">
                <span class="ai-action-icon">⚡</span>
                <span>Optimize Performance</span>
            </div>
            <div class="ai-action-item" data-action="test">
                <span class="ai-action-icon">🧪</span>
                <span>Generate Tests</span>
            </div>
            <div class="ai-action-item" data-action="fix">
                <span class="ai-action-icon">🔧</span>
                <span>Fix Issues</span>
            </div>
        `);
        
        // Append to document
        document.body.appendChild(this.ui.statusIndicator);
        document.body.appendChild(this.ui.chatPanel);
        document.body.appendChild(this.ui.suggestionOverlay);
        document.body.appendChild(this.ui.contextMenu);
    }
    
    createElement(tag, className, innerHTML = '') {
        const element = document.createElement(tag);
        element.className = className;
        if (innerHTML) element.innerHTML = innerHTML;
        return element;
    }
    
    setupEventListeners() {
        // Chat panel toggle
        this.ui.chatPanel.querySelector('.ai-chat-toggle').addEventListener('click', () => {
            this.toggleChatPanel();
        });
        
        // Context menu actions
        this.ui.contextMenu.addEventListener('click', (e) => {
            const actionItem = e.target.closest('.ai-action-item');
            if (actionItem) {
                const action = actionItem.dataset.action;
                this.executeContextualAction(action);
            }
        });
        
        // Intelligent text selection
        document.addEventListener('selectionchange', () => {
            this.handleTextSelection();
        });
        
        // Smart right-click context
        document.addEventListener('contextmenu', (e) => {
            this.showContextualActions(e);
        });
        
        // Hide context menu on click elsewhere
        document.addEventListener('click', (e) => {
            if (!this.ui.contextMenu.contains(e.target)) {
                this.hideContextMenu();
            }
        });
        
        // Keyboard shortcuts for AI actions
        document.addEventListener('keydown', (e) => {
            this.handleKeyboardShortcuts(e);
        });
    }
    
    setupAIIntegration() {
        // Connect to PowerShell backend if available
        if (window.PowerShellBridge) {
            this.setupPowerShellBridge();
        }
        
        // Setup intelligent autocomplete
        this.setupIntelligentAutocomplete();
        
        // Initialize AI models
        this.loadAvailableModels();
    }
    
    setupPowerShellBridge() {
        this.powerShell = window.PowerShellBridge;
        
        // Test connection
        this.updateAgentStatus('connecting', 'Connecting to AI...');
        
        this.powerShell.getOllamaModels()
            .then(models => {
                this.aiAgent.availableModels = models;
                this.updateAgentStatus('active', `${models.length} AI models ready`);
            })
            .catch(error => {
                this.updateAgentStatus('error', 'AI connection failed');
                console.error('PowerShell bridge error:', error);
            });
    }
    
    setupIntelligentAutocomplete() {
        // Enhanced autocomplete with AI suggestions
        let autocompleteTimer;
        
        document.addEventListener('input', (e) => {
            if (this.isCodeEditor(e.target)) {
                clearTimeout(autocompleteTimer);
                autocompleteTimer = setTimeout(() => {
                    this.generateIntelligentSuggestions(e.target);
                }, 300);
            }
        });
    }
    
    async generateIntelligentSuggestions(editor) {
        if (!this.settings.autoSuggestions) return;
        
        const cursor = this.getCursorPosition(editor);
        const context = this.getCodeContext(editor, cursor);
        
        if (context.shouldSuggest) {
            this.updateAgentStatus('thinking', 'Generating suggestions...');
            
            try {
                const suggestions = await this.getAISuggestions(context);
                if (suggestions && suggestions.length > 0) {
                    this.showSuggestionOverlay(suggestions[0], cursor);
                }
            } catch (error) {
                console.error('Suggestion error:', error);
            } finally {
                this.updateAgentStatus('active', 'AI Ready');
            }
        }
    }
    
    async getAISuggestions(context) {
        if (!this.powerShell) return null;
        
        const prompt = `Complete this code context:\n\n${context.code}\n\nSuggest the next line or completion:`;
        
        try {
            const result = await this.powerShell.chatWithOllama(
                prompt, 
                'llama3.2', 
                context.fullFile, 
                context.language
            );
            
            if (result.success) {
                return [{
                    text: this.extractSuggestion(result.response),
                    confidence: 0.85,
                    type: 'completion'
                }];
            }
        } catch (error) {
            console.error('AI suggestion error:', error);
        }
        
        return null;
    }
    
    extractSuggestion(aiResponse) {
        // Extract clean code suggestion from AI response
        const lines = aiResponse.split('\n');
        const codeLine = lines.find(line => 
            line.trim() && 
            !line.startsWith('#') && 
            !line.toLowerCase().includes('here') &&
            !line.toLowerCase().includes('suggest')
        );
        
        return codeLine ? codeLine.trim() : '';
    }
    
    showSuggestionOverlay(suggestion, position) {
        if (!suggestion.text) return;
        
        this.ui.suggestionOverlay.innerHTML = `
            <div class="ai-suggestion-text">${this.escapeHtml(suggestion.text)}</div>
            <div class="ai-suggestion-meta">
                <span>AI suggestion</span>
                <span>${Math.round(suggestion.confidence * 100)}% confident</span>
            </div>
        `;
        
        // Position near cursor
        this.ui.suggestionOverlay.style.left = `${position.x}px`;
        this.ui.suggestionOverlay.style.top = `${position.y + 25}px`;
        
        // Show with subtle animation
        this.ui.suggestionOverlay.classList.add('visible');
        
        // Auto-hide after 5 seconds
        setTimeout(() => {
            this.ui.suggestionOverlay.classList.remove('visible');
        }, 5000);
    }
    
    handleTextSelection() {
        const selection = window.getSelection();
        const selectedText = selection.toString().trim();
        
        if (selectedText && selectedText.length > 5 && this.isCodeText(selectedText)) {
            // Subtle indication that AI actions are available
            this.highlightSelectedCode(selection);
        }
    }
    
    showContextualActions(e) {
        const selection = window.getSelection();
        const selectedText = selection.toString().trim();
        
        if (selectedText && this.isCodeText(selectedText)) {
            e.preventDefault();
            
            // Position context menu
            this.ui.contextMenu.style.left = `${e.pageX}px`;
            this.ui.contextMenu.style.top = `${e.pageY}px`;
            
            // Show with animation
            this.ui.contextMenu.classList.add('visible');
            
            // Update actions based on selection
            this.updateContextualActions(selectedText);
        }
    }
    
    updateContextualActions(selectedText) {
        const actions = this.ui.contextMenu.querySelectorAll('.ai-action-item');
        
        // Show/hide actions based on code context
        actions.forEach(action => {
            const actionType = action.dataset.action;
            const relevant = this.isActionRelevant(actionType, selectedText);
            action.style.display = relevant ? 'flex' : 'none';
        });
    }
    
    isActionRelevant(actionType, code) {
        switch (actionType) {
            case 'refactor':
                return code.length > 20;
            case 'explain':
                return true;
            case 'optimize':
                return code.includes('for') || code.includes('while') || code.includes('function');
            case 'test':
                return code.includes('function') || code.includes('def') || code.includes('method');
            case 'fix':
                return code.includes('error') || code.includes('bug') || code.includes('//');
            default:
                return true;
        }
    }
    
    async executeContextualAction(action) {
        const selection = window.getSelection();
        const selectedText = selection.toString().trim();
        
        this.hideContextMenu();
        this.updateAgentStatus('thinking', `AI ${action}ing code...`);
        
        try {
            const result = await this.performAIAction(action, selectedText);
            this.displayAIResult(result, action);
        } catch (error) {
            this.updateAgentStatus('error', 'AI action failed');
            console.error(`AI ${action} error:`, error);
        }
    }
    
    async performAIAction(action, code) {
        if (!this.powerShell) throw new Error('PowerShell bridge not available');
        
        const prompts = {
            refactor: `Refactor this code to be more efficient and readable:\n\n${code}`,
            explain: `Explain what this code does in simple terms:\n\n${code}`,
            optimize: `Optimize this code for better performance:\n\n${code}`,
            test: `Generate unit tests for this code:\n\n${code}`,
            fix: `Find and fix any issues in this code:\n\n${code}`
        };
        
        const result = await this.powerShell.chatWithOllama(
            prompts[action] || prompts.explain,
            'llama3.2',
            code,
            this.detectCodeLanguage(code)
        );
        
        return result;
    }
    
    displayAIResult(result, action) {
        if (!result.success) {
            this.updateAgentStatus('error', 'AI request failed');
            return;
        }
        
        // Create inline diff view for code changes
        if (['refactor', 'optimize', 'fix'].includes(action)) {
            this.showInlineDiff(result.response, action);
        } else {
            // Show in chat panel for explanations
            this.showInChatPanel(result.response, action);
        }
        
        this.updateAgentStatus('active', 'AI Ready');
    }
    
    showInlineDiff(aiResponse, action) {
        const diffContainer = this.createElement('div', 'ai-diff-container', `
            <div class="ai-diff-header">
                <span class="ai-diff-title">🤖 AI ${action.charAt(0).toUpperCase() + action.slice(1)}</span>
                <div class="ai-diff-actions">
                    <button class="ai-diff-btn accept">Accept</button>
                    <button class="ai-diff-btn">Reject</button>
                </div>
            </div>
            <div class="ai-diff-content">
                <pre class="diff-line added">${this.escapeHtml(aiResponse)}</pre>
            </div>
        `);
        
        // Insert after current selection
        const selection = window.getSelection();
        if (selection.rangeCount > 0) {
            const range = selection.getRangeAt(0);
            range.collapse(false);
            range.insertNode(diffContainer);
        }
        
        // Handle accept/reject
        diffContainer.querySelector('.accept').addEventListener('click', () => {
            this.acceptAIChange(diffContainer, aiResponse);
        });
        
        diffContainer.querySelector('.ai-diff-btn:not(.accept)').addEventListener('click', () => {
            diffContainer.remove();
        });
    }
    
    showInChatPanel(response, action) {
        this.toggleChatPanel(true);
        
        const messagesContainer = this.ui.chatPanel.querySelector('.ai-chat-messages');
        const messageDiv = this.createElement('div', 'ai-message', `
            <div class="ai-message-header">🤖 ${action.charAt(0).toUpperCase() + action.slice(1)}</div>
            <div class="ai-message-content">${this.formatAIResponse(response)}</div>
        `);
        
        messagesContainer.appendChild(messageDiv);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }
    
    // Intelligent Search Visualization
    highlightSearchResults(query, results) {
        results.forEach((result, index) => {
            const element = document.querySelector(`[data-file="${result.file}"]`);
            if (element) {
                element.classList.add('search-highlight');
                
                // Add match count indicator
                if (index === 0) {
                    this.showSearchMatchCount(results.length);
                }
            }
        });
    }
    
    showSearchMatchCount(count) {
        const existing = document.querySelector('.search-match-count');
        if (existing) existing.remove();
        
        const counter = this.createElement('div', 'search-match-count', `${count} matches found`);
        document.body.appendChild(counter);
        
        setTimeout(() => counter.remove(), 3000);
    }
    
    // Status and UI Updates
    updateAgentStatus(status, message) {
        this.aiAgent.status = status;
        
        const dot = this.ui.statusIndicator.querySelector('.ai-status-dot');
        const text = this.ui.statusIndicator.querySelector('.ai-status-text');
        
        dot.className = `ai-status-dot ${status}`;
        text.textContent = message;
        
        if (status === 'thinking' || status === 'active') {
            this.ui.statusIndicator.classList.add('active');
        } else {
            this.ui.statusIndicator.classList.remove('active');
        }
    }
    
    toggleChatPanel(show = null) {
        const isVisible = this.ui.chatPanel.classList.contains('visible');
        
        if (show === null) {
            this.ui.chatPanel.classList.toggle('visible');
        } else if (show) {
            this.ui.chatPanel.classList.add('visible');
        } else {
            this.ui.chatPanel.classList.remove('visible');
        }
    }
    
    hideContextMenu() {
        this.ui.contextMenu.classList.remove('visible');
    }
    
    // Utility Functions
    isCodeEditor(element) {
        return element.tagName === 'TEXTAREA' || 
               element.contentEditable === 'true' ||
               element.classList.contains('editor') ||
               element.closest('.monaco-editor');
    }
    
    isCodeText(text) {
        const codeIndicators = [
            /function\s+\w+\s*\(/,
            /\w+\s*=\s*function/,
            /\bif\s*\(/,
            /\bfor\s*\(/,
            /\bwhile\s*\(/,
            /\w+\.\w+\(/,
            /\$\w+/,
            /\{\s*$/,
            /;\s*$/
        ];
        
        return codeIndicators.some(pattern => pattern.test(text));
    }
    
    getCursorPosition(editor) {
        // Simplified cursor position detection
        const rect = editor.getBoundingClientRect();
        return {
            x: rect.left + 20,
            y: rect.top + 20
        };
    }
    
    getCodeContext(editor, cursor) {
        const value = editor.value || editor.textContent;
        const lines = value.split('\n');
        const currentLine = lines[Math.min(cursor.line || 0, lines.length - 1)];
        
        return {
            code: currentLine,
            fullFile: value,
            language: this.detectCodeLanguage(value),
            shouldSuggest: currentLine.length > 3 && !currentLine.trim().startsWith('//')
        };
    }
    
    detectCodeLanguage(code) {
        if (code.includes('function') && code.includes('{')) return 'javascript';
        if (code.includes('def ') && code.includes(':')) return 'python';
        if (code.includes('param(') || code.includes('Write-Host')) return 'powershell';
        if (code.includes('<') && code.includes('>')) return 'html';
        return 'text';
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    formatAIResponse(response) {
        // Basic formatting for AI responses
        return response
            .replace(/```(\w+)?\n([\s\S]*?)```/g, '<pre><code>$2</code></pre>')
            .replace(/`([^`]+)`/g, '<code>$1</code>')
            .replace(/\n/g, '<br>');
    }
    
    handleKeyboardShortcuts(e) {
        // Ctrl+/ for AI chat
        if (e.ctrlKey && e.key === '/') {
            e.preventDefault();
            this.toggleChatPanel();
        }
        
        // Ctrl+Shift+A for AI actions on selection
        if (e.ctrlKey && e.shiftKey && e.key === 'A') {
            e.preventDefault();
            const selection = window.getSelection().toString();
            if (selection.trim()) {
                this.showContextualActions({
                    pageX: window.innerWidth / 2,
                    pageY: window.innerHeight / 2,
                    preventDefault: () => {}
                });
            }
        }
    }
    
    applyAestheticEnhancements() {
        // Apply subtle visual enhancements to existing elements
        const existingEditors = document.querySelectorAll('textarea, [contenteditable="true"]');
        existingEditors.forEach(editor => {
            editor.style.transition = 'all 0.2s cubic-bezier(0.4, 0, 0.2, 1)';
        });
        
        // Add loading CSS if not already present
        if (!document.querySelector('#ai-first-styles')) {
            const link = document.createElement('link');
            link.id = 'ai-first-styles';
            link.rel = 'stylesheet';
            link.href = 'AI-First-IDE-Styles.css';
            document.head.appendChild(link);
        }
    }
}

// Initialize the AI-First IDE Enhancement
document.addEventListener('DOMContentLoaded', () => {
    window.aiFirstIDE = new AIFirstIDE();
});

// Also initialize if DOM is already loaded
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.aiFirstIDE = new AIFirstIDE();
    });
} else {
    window.aiFirstIDE = new AIFirstIDE();
}

console.log('🚀 AI-First IDE JavaScript Module Loaded');