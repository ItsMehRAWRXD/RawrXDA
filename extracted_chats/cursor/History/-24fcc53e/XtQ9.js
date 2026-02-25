/**
 * BigDaddyG IDE - AI Response Handler
 * Streaming terminal output, message queue, code actions (accept/reject)
 */

(function() {
'use strict';

class AIResponseHandler {
    constructor() {
        this.isProcessing = false;
        this.currentAbortController = null;
        this.messageQueue = [];
        this.queueBanner = null;
        this.init();
    }
    
    init() {
        console.log('[AIResponse] 🎬 Initializing AI response handler...');
        
        // Ctrl+Shift+X now handled by hotkey-manager.js
        // Register handler with hotkey-manager if available
        if (window.hotkeyManager) {
            window.hotkeyManager.register('Ctrl+Shift+X', (e) => {
                this.stopCurrentExecution();
            }, 'Stop AI Execution');
        }
        
        console.log('[AIResponse] ✅ AI response handler ready');
    }
    
    async processMessage(message, attachments = []) {
        // If already processing, add to queue
        if (this.isProcessing) {
            this.addToQueue(message, attachments);
            return;
        }
        
        this.isProcessing = true;
        this.currentAbortController = new AbortController();
        
        try {
            await this.executeMessage(message, attachments);
        } catch (error) {
            if (error.name === 'AbortError') {
                console.log('[AIResponse] ⏹️ Execution stopped by user');
                this.addAIMessage('⏹️ Stopped by user', true);
            } else {
                console.error('[AIResponse] ❌ Error:', error);
                this.addAIMessage(`Error: ${error.message}`, true);
            }
        } finally {
            this.isProcessing = false;
            this.currentAbortController = null;
            
            // Process next in queue
            if (this.messageQueue.length > 0) {
                const next = this.messageQueue.shift();
                this.updateQueueBanner();
                setTimeout(() => this.processMessage(next.message, next.attachments), 100);
            } else {
                this.hideQueueBanner();
            }
        }
    }
    
    addToQueue(message, attachments) {
        this.messageQueue.push({ message, attachments, timestamp: Date.now() });
        this.updateQueueBanner();
        console.log(`[AIResponse] 📥 Message queued (${this.messageQueue.length} in queue)`);
    }
    
    updateQueueBanner() {
        if (this.messageQueue.length === 0) {
            this.hideQueueBanner();
            return;
        }
        
        if (!this.queueBanner) {
            this.queueBanner = document.createElement('div');
            this.queueBanner.id = 'queue-banner';
            this.queueBanner.style.cssText = `
                position: sticky;
                top: 0;
                z-index: 1000;
                background: linear-gradient(135deg, var(--cursor-accent), var(--cursor-jade-dark));
                color: white;
                padding: 10px 15px;
                display: flex;
                justify-content: space-between;
                align-items: center;
                font-size: 12px;
                font-weight: 600;
                box-shadow: 0 2px 8px rgba(0,0,0,0.3);
                animation: slideDown 0.3s ease-out;
            `;
            
            const container = document.getElementById('ai-chat-messages');
            if (container) {
                container.insertBefore(this.queueBanner, container.firstChild);
            }
        }
        
        const count = this.messageQueue.length;
        this.queueBanner.innerHTML = `
            <div style="display: flex; align-items: center; gap: 10px;">
                <span style="font-size: 18px;">📬</span>
                <span>${count} message${count > 1 ? 's' : ''} queued</span>
            </div>
            <div style="display: flex; gap: 8px;">
                ${this.messageQueue.map((msg, idx) => `
                    <button onclick="aiResponseHandler.sendQueuedNow(${idx})" style="background: rgba(255,255,255,0.2); border: 1px solid white; color: white; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 10px; font-weight: 600;">
                        ▶️ Send #${idx + 1} Now
                    </button>
                    <button onclick="aiResponseHandler.deleteQueued(${idx})" style="background: rgba(255,71,87,0.3); border: 1px solid #ff4757; color: white; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 10px; font-weight: 600;">
                        🗑️ Delete #${idx + 1}
                    </button>
                `).join('')}
            </div>
        `;
    }
    
    hideQueueBanner() {
        if (this.queueBanner) {
            this.queueBanner.remove();
            this.queueBanner = null;
        }
    }
    
    sendQueuedNow(index) {
        if (index >= 0 && index < this.messageQueue.length) {
            const msg = this.messageQueue.splice(index, 1)[0];
            this.updateQueueBanner();
            
            // Stop current execution and start this one
            this.stopCurrentExecution();
            setTimeout(() => this.processMessage(msg.message, msg.attachments), 100);
        }
    }
    
    deleteQueued(index) {
        if (index >= 0 && index < this.messageQueue.length) {
            this.messageQueue.splice(index, 1);
            this.updateQueueBanner();
            console.log('[AIResponse] 🗑️ Queued message deleted');
        }
    }
    
    stopCurrentExecution() {
        if (this.currentAbortController) {
            console.log('[AIResponse] ⏹️ Stopping current execution...');
            this.currentAbortController.abort();
        }
    }
    
    async executeMessage(message, attachments) {
        // Add user message with attachments info
        if (typeof addUserMessage === 'function') {
            const attachInfo = attachments.length > 0 ? 
                `\n\n📎 ${attachments.length} file(s) attached` : '';
            addUserMessage(message + attachInfo, attachments);
        }
        
        // Create streaming response container
        const responseId = this.createStreamingResponse();
        
        try {
            // Build request
            const requestBody = {
                message,
                model: 'BigDaddyG:Latest',
                attachments: attachments.length,
                stream: true
            };
            
            const response = await fetch('http://localhost:11441/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(requestBody),
                signal: this.currentAbortController.signal
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            // Stream response
            const reader = response.body.getReader();
            const decoder = new TextDecoder();
            let fullResponse = '';
            
            while (true) {
                const { done, value } = await reader.read();
                
                if (done) break;
                
                const chunk = decoder.decode(value, { stream: true });
                fullResponse += chunk;
                
                // Update streaming display
                this.updateStreamingResponse(responseId, fullResponse);
            }
            
            // Finalize response with code actions
            this.finalizeResponse(responseId, fullResponse);
            
        } catch (error) {
            if (error.name !== 'AbortError') {
                this.updateStreamingResponse(responseId, `❌ Error: ${error.message}`, true);
            }
            throw error;
        }
    }
    
    createStreamingResponse() {
        const container = document.getElementById('ai-chat-messages');
        if (!container) return null;
        
        const id = `stream-${Date.now()}`;
        const div = document.createElement('div');
        div.id = id;
        div.className = 'ai-message streaming';
        div.style.cssText = `
            margin: 12px 16px;
            padding: 14px 16px;
            background: rgba(119, 221, 190, 0.1);
            border-left: 4px solid var(--cursor-jade-dark);
            border-radius: 8px;
            font-size: 13px;
            line-height: 1.6;
            position: relative;
        `;
        
        div.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                <div style="display: flex; align-items: center; gap: 8px;">
                    <span style="font-size: 18px;">🤖</span>
                    <span style="font-weight: 600; color: var(--cursor-jade-dark);">BigDaddyG</span>
                    <span style="font-size: 10px; color: var(--cursor-text-muted); animation: pulse 1.5s ease-in-out infinite;">● Streaming...</span>
                </div>
                <button onclick="aiResponseHandler.stopCurrentExecution()" style="background: rgba(255,71,87,0.2); border: 1px solid var(--red); color: var(--red); padding: 4px 12px; border-radius: 4px; cursor: pointer; font-size: 11px; font-weight: 600;">
                    ⏹️ Stop (Ctrl+Shift+X)
                </button>
            </div>
            <div id="${id}-content" style="white-space: pre-wrap; word-wrap: break-word; color: var(--cursor-text);">
            </div>
        `;
        
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
        
        return id;
    }
    
    updateStreamingResponse(id, content, isError = false) {
        const contentEl = document.getElementById(`${id}-content`);
        if (contentEl) {
            if (isError) {
                contentEl.style.color = 'var(--red)';
            }
            contentEl.textContent = content;
            
            // Auto-scroll
            const container = document.getElementById('ai-chat-messages');
            if (container) {
                container.scrollTop = container.scrollHeight;
            }
        }
    }
    
    finalizeResponse(id, content) {
        const responseEl = document.getElementById(id);
        if (!responseEl) return;
        
        // Remove streaming indicator
        responseEl.classList.remove('streaming');
        
        // Parse and render with code actions
        const rendered = this.renderWithCodeActions(content);
        
        responseEl.innerHTML = `
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                <div style="display: flex; align-items: center; gap: 8px;">
                    <span style="font-size: 18px;">🤖</span>
                    <span style="font-weight: 600; color: var(--cursor-jade-dark);">BigDaddyG</span>
                    <span style="font-size: 10px; color: var(--cursor-text-secondary);">${new Date().toLocaleTimeString()}</span>
                </div>
            </div>
            <div style="color: var(--cursor-text);">
                ${rendered}
            </div>
        `;
        
        // Save to chat history
        if (window.chatHistory) {
            window.chatHistory.addMessage('assistant', content);
        }
    }
    
    renderWithCodeActions(content) {
        // Parse code blocks with file info
        const codeBlockRegex = /```(\w+)?(?:\:(.+?))?\n([\s\S]*?)```/g;
        let lastIndex = 0;
        let result = '';
        let blockIndex = 0;
        
        let match;
        while ((match = codeBlockRegex.exec(content)) !== null) {
            // Add text before code block
            result += this.escapeHtml(content.substring(lastIndex, match.index));
            
            const language = match[1] || 'plaintext';
            const filePath = match[2] || null;
            const code = match[3];
            
            // Count line changes
            const lines = code.split('\n');
            const additions = lines.filter(l => l.trim().startsWith('+')).length;
            const deletions = lines.filter(l => l.trim().startsWith('-')).length;
            
            const blockId = `code-block-${Date.now()}-${blockIndex}`;
            blockIndex++;
            
            result += this.renderCodeBlock(blockId, language, filePath, code, additions, deletions);
            
            lastIndex = match.index + match[0].length;
        }
        
        // Add remaining text
        result += this.escapeHtml(content.substring(lastIndex));
        
        return result;
    }
    
    renderCodeBlock(blockId, language, filePath, code, additions, deletions) {
        const fileName = filePath ? filePath.split(/[/\\]/).pop() : 'code';
        const changeInfo = additions > 0 || deletions > 0 ? 
            `<span style="color: #4caf50;">+${additions}</span> <span style="color: #f44336;">-${deletions}</span>` : '';
        
        return `
            <div id="${blockId}" class="code-block-container" style="margin: 15px 0; border: 1px solid var(--cursor-border); border-radius: 8px; overflow: hidden; background: var(--cursor-bg-secondary);">
                <!-- Code Block Header -->
                <div style="padding: 8px 12px; background: var(--cursor-bg-tertiary); display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid var(--cursor-border);">
                    <div style="display: flex; align-items: center; gap: 10px; font-size: 12px;">
                        <span style="color: var(--cursor-jade-dark); font-weight: 600;">${language}</span>
                        ${filePath ? `<span style="color: var(--cursor-text-secondary);">📄 ${filePath}</span>` : ''}
                        ${changeInfo ? `<span style="font-size: 11px;">${changeInfo} lines</span>` : ''}
                    </div>
                    <div style="display: flex; gap: 6px;">
                        <button onclick="aiResponseHandler.toggleCodeBlock('${blockId}')" title="Expand/Collapse" style="background: none; border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 11px; font-weight: 600;">
                            <span id="${blockId}-toggle">📖 Expand</span>
                        </button>
                        ${filePath ? `
                            <button onclick="aiResponseHandler.acceptCode('${blockId}', '${this.escapeAttr(filePath)}', \`${this.escapeCode(code)}\`)" title="Accept & Apply" style="background: rgba(76, 175, 80, 0.2); border: 1px solid #4caf50; color: #4caf50; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 11px; font-weight: 600;">
                                ✓ Accept
                            </button>
                            <button onclick="aiResponseHandler.rejectCode('${blockId}')" title="Reject" style="background: rgba(244, 67, 54, 0.2); border: 1px solid #f44336; color: #f44336; padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 11px; font-weight: 600;">
                                ✗ Reject
                            </button>
                            <button onclick="aiResponseHandler.openInTerminal('${this.escapeAttr(filePath)}')" title="Open in Terminal" style="background: none; border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 4px 10px; border-radius: 4px; cursor: pointer; font-size: 11px; font-weight: 600;">
                                🖥️ Terminal
                            </button>
                        ` : ''}
                    </div>
                </div>
                
                <!-- Code Content (Initially Collapsed) -->
                <div id="${blockId}-content" style="display: none; max-height: 400px; overflow-y: auto;">
                    <pre style="margin: 0; padding: 12px; background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; line-height: 1.5; overflow-x: auto;"><code>${this.escapeHtml(code)}</code></pre>
                </div>
                
                <!-- Status Badge (Hidden by default) -->
                <div id="${blockId}-status" style="display: none; padding: 8px 12px; background: var(--cursor-bg-tertiary); border-top: 1px solid var(--cursor-border); font-size: 11px; font-weight: 600;">
                </div>
            </div>
        `;
    }
    
    toggleCodeBlock(blockId) {
        const content = document.getElementById(`${blockId}-content`);
        const toggle = document.getElementById(`${blockId}-toggle`);
        
        if (content && toggle) {
            if (content.style.display === 'none') {
                content.style.display = 'block';
                toggle.textContent = '📕 Collapse';
            } else {
                content.style.display = 'none';
                toggle.textContent = '📖 Expand';
            }
        }
    }
    
    async acceptCode(blockId, filePath, code) {
        const status = document.getElementById(`${blockId}-status`);
        if (status) {
            status.style.display = 'block';
            status.style.background = 'rgba(76, 175, 80, 0.2)';
            status.style.color = '#4caf50';
            status.innerHTML = '✅ Accepted - Applying to ' + filePath;
        }
        
        try {
            // Apply to Monaco editor
            if (typeof openFile === 'function' && typeof window.editor !== 'undefined') {
                // Open file
                await openFile(filePath);
                
                // Replace content
                window.editor.setValue(code);
                
                // Save
                if (typeof saveCurrentFile === 'function') {
                    await saveCurrentFile();
                }
                
                if (status) {
                    status.innerHTML = '✅ Accepted & Applied to ' + filePath;
                }
                
                console.log('[AIResponse] ✅ Code accepted and applied');
            } else {
                throw new Error('Editor not available');
            }
        } catch (error) {
            console.error('[AIResponse] ❌ Error applying code:', error);
            if (status) {
                status.style.background = 'rgba(244, 67, 54, 0.2)';
                status.style.color = '#f44336';
                status.innerHTML = '❌ Error applying: ' + error.message;
            }
        }
    }
    
    rejectCode(blockId) {
        const status = document.getElementById(`${blockId}-status`);
        if (status) {
            status.style.display = 'block';
            status.style.background = 'rgba(244, 67, 54, 0.2)';
            status.style.color = '#f44336';
            status.innerHTML = '❌ Rejected';
        }
        console.log('[AIResponse] ❌ Code rejected');
    }
    
    openInTerminal(filePath) {
        // Extract directory
        const dir = filePath.substring(0, filePath.lastIndexOf('\\') || filePath.lastIndexOf('/'));
        
        // Open terminal panel and navigate
        const terminalPanel = document.getElementById('bottom-panel');
        if (terminalPanel) {
            terminalPanel.classList.remove('collapsed');
        }
        
        // Send cd command to terminal
        if (typeof sendTerminalCommand === 'function') {
            sendTerminalCommand(`cd "${dir}"`);
        }
        
        console.log('[AIResponse] 🖥️ Opened in terminal:', dir);
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    escapeAttr(text) {
        return text.replace(/'/g, "\\'").replace(/"/g, '\\"');
    }
    
    escapeCode(code) {
        return code.replace(/`/g, '\\`').replace(/\$/g, '\\$');
    }
    
    addAIMessage(text, isError = false) {
        if (typeof addAIMessage === 'function') {
            addAIMessage(text, isError);
        }
    }
}

// Initialize
window.aiResponseHandler = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.aiResponseHandler = new AIResponseHandler();
    });
} else {
    window.aiResponseHandler = new AIResponseHandler();
}

// Export
window.AIResponseHandler = AIResponseHandler;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = AIResponseHandler;
}

console.log('[AIResponse] 📦 AI response handler module loaded');

})(); // End IIFE

