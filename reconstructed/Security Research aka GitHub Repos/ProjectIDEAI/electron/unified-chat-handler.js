/**
 * BigDaddyG IDE - Unified Chat Handler
 * Consolidates floating chat, sidebar chat, and command system
 */

(function() {
'use strict';

class UnifiedChatHandler {
    constructor() {
        this.inputs = new Map(); // Track all chat input surfaces
        this.commandSystem = null;
        this.aiResponseHandler = null;
        this.primaryInput = null;
        
        console.log('[UnifiedChat] 🎯 Initializing unified chat handler...');
        this.init();
    }
    
    init() {
        // Wait for dependencies
        this.waitForDependencies();
        
        // Register all chat inputs
        this.registerChatInputs();
        
        // Set up unified event handling
        this.setupEventHandling();
        
        // Expose to window
        window.unifiedChat = this;
        
        console.log('[UnifiedChat] ✅ Unified chat handler ready');
    }
    
    waitForDependencies() {
        const check = () => {
            if (window.commandSystem) {
                this.commandSystem = window.commandSystem;
            }
            
            if (window.aiResponseHandler) {
                this.aiResponseHandler = window.aiResponseHandler;
            }
            
            return this.commandSystem !== null;
        };
        
        if (!check()) {
            const interval = setInterval(() => {
                if (check()) {
                    clearInterval(interval);
                    console.log('[UnifiedChat] Dependencies loaded');
                }
            }, 100);
        }
    }
    
    registerChatInputs() {
        // Register floating chat input
        this.registerInput('floating-chat', {
            inputId: 'floating-chat-input',
            sendButtonId: 'floating-chat-send',
            containerId: 'floating-chat',
            type: 'floating',
            priority: 2
        });
        
        // Register sidebar chat input
        this.registerInput('sidebar-chat', {
            inputId: 'ai-input',
            sendButtonId: 'ai-send-btn',
            containerId: 'right-sidebar',
            type: 'sidebar',
            priority: 1 // Primary
        });
        
        // Register center chat input (if exists)
        this.registerInput('center-chat', {
            inputId: 'center-chat-input',
            sendButtonId: 'center-chat-send',
            containerId: 'center-chat-panel',
            type: 'center',
            priority: 3
        });
        
        // Set primary input (highest priority available)
        this.setPrimaryInput();
    }
    
    registerInput(name, config) {
        this.inputs.set(name, {
            ...config,
            element: null,
            sendButton: null,
            active: false
        });
        
        // Try to find elements
        const input = document.getElementById(config.inputId);
        const sendBtn = document.getElementById(config.sendButtonId);
        
        if (input) {
            this.inputs.get(name).element = input;
            this.inputs.get(name).active = true;
            
            // Attach unified handler
            input.addEventListener('keydown', (e) => this.handleKeyDown(e, name));
        }
        
        if (sendBtn) {
            this.inputs.get(name).sendButton = sendBtn;
            sendBtn.addEventListener('click', () => this.handleSend(name));
        }
    }
    
    setPrimaryInput() {
        // Find active input with highest priority (lowest number)
        let primary = null;
        let lowestPriority = Infinity;
        
        this.inputs.forEach((config, name) => {
            if (config.active && config.priority < lowestPriority) {
                lowestPriority = config.priority;
                primary = name;
            }
        });
        
        if (primary) {
            this.primaryInput = primary;
            console.log(`[UnifiedChat] Primary input: ${primary}`);
        }
    }
    
    setupEventHandling() {
        // Override original send functions to use unified handler
        if (window.sendToAI) {
            const originalSend = window.sendToAI;
            window.sendToAI = () => {
                this.handleSend(this.primaryInput || 'sidebar-chat');
            };
        }
        
        // Override floating chat send if exists
        if (window.floatingChat && window.floatingChat.send) {
            const originalSend = window.floatingChat.send.bind(window.floatingChat);
            window.floatingChat.send = () => {
                this.handleSend('floating-chat');
            };
        }
    }
    
    handleKeyDown(e, inputName) {
        // Ctrl+Enter to send
        if (e.ctrlKey && e.key === 'Enter') {
            e.preventDefault();
            this.handleSend(inputName);
        }
        
        // Escape to clear/cancel
        if (e.key === 'Escape') {
            e.preventDefault();
            this.clearInput(inputName);
        }
    }
    
    async handleSend(inputName) {
        const config = this.inputs.get(inputName);
        if (!config || !config.element) {
            console.warn(`[UnifiedChat] Input not found: ${inputName}`);
            return;
        }
        
        const message = config.element.value.trim();
        if (!message) return;
        
        console.log(`[UnifiedChat] Sending from ${inputName}: ${message}`);
        
        // Check if it's a command (starts with !)
        if (message.startsWith('!')) {
            await this.handleCommand(message, inputName);
        } else {
            await this.handleMessage(message, inputName);
        }
        
        // Clear input
        this.clearInput(inputName);
    }
    
    async handleCommand(message, inputName) {
        if (!this.commandSystem) {
            console.warn('[UnifiedChat] Command system not available');
            this.handleMessage(message, inputName);
            return;
        }
        
        // Parse command
        const parts = message.substring(1).split(' ');
        const command = parts[0].toLowerCase();
        const args = parts.slice(1).join(' ');
        
        // Execute through command system
        try {
            await this.commandSystem.executeCommand(command, args);
        } catch (error) {
            console.error('[UnifiedChat] Command execution failed:', error);
            this.showError(`Command failed: ${error.message}`);
        }
    }
    
    async handleMessage(message, inputName) {
        // Send through AI response handler
        if (this.aiResponseHandler) {
            try {
                await this.aiResponseHandler.sendMessage(message);
            } catch (error) {
                console.error('[UnifiedChat] AI message failed:', error);
                this.showError(`Failed to send message: ${error.message}`);
            }
        } else if (window.sendToAI) {
            // Fallback to global sendToAI
            window.sendToAI();
        } else {
            console.warn('[UnifiedChat] No AI handler available');
            this.showError('AI service not available');
        }
    }
    
    clearInput(inputName) {
        const config = this.inputs.get(inputName);
        if (config && config.element) {
            config.element.value = '';
        }
    }
    
    focusInput(inputName) {
        const config = this.inputs.get(inputName || this.primaryInput);
        if (config && config.element) {
            config.element.focus();
        }
    }
    
    setInputValue(message, inputName) {
        const config = this.inputs.get(inputName || this.primaryInput);
        if (config && config.element) {
            config.element.value = message;
            config.element.focus();
            
            // Move cursor to end
            config.element.setSelectionRange(message.length, message.length);
        }
    }
    
    showError(message) {
        if (window.showNotification) {
            window.showNotification('Chat Error', message, 'error', 3000);
        } else {
            console.error(`[UnifiedChat] ${message}`);
        }
    }
    
    // Sync message across all inputs (optional - for mirroring)
    syncInputs(message, excludeInput = null) {
        this.inputs.forEach((config, name) => {
            if (name !== excludeInput && config.element) {
                config.element.value = message;
            }
        });
    }
}

// Initialize on DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new UnifiedChatHandler();
    });
} else {
    new UnifiedChatHandler();
}

})();
