/**
 * AI Code to Tabs System
 * 
 * Automatically extracts code from AI responses and creates Monaco tabs
 * Supports multiple files, syntax detection, and smart tab management
 */

(function() {
'use strict';

class AICodeToTabs {
    constructor() {
        this.autoCreateTabs = true; // Enable by default
        this.closeExistingTabs = false; // Don't close by default (user preference)
        this.extractedFiles = [];
        this.init();
    }
    
    init() {
        console.log('[CodeToTabs] 🎯 Initializing AI Code to Tabs system...');
        
        // Listen for AI responses
        this.interceptAIResponses();
        
        console.log('[CodeToTabs] ✅ AI Code to Tabs ready!');
        console.log('[CodeToTabs] 💡 Code blocks will auto-create tabs');
    }
    
    interceptAIResponses() {
        // Hook into AI response rendering
        // We'll intercept after AI response is added to chat
        
        // Create a MutationObserver to watch for new AI messages
        const observer = new MutationObserver((mutations) => {
            mutations.forEach((mutation) => {
                if (mutation.addedNodes.length > 0) {
                    mutation.addedNodes.forEach((node) => {
                        if (node.nodeType === 1) { // Element node
                            // Check if it's an AI message
                            if (this.isAIMessage(node)) {
                                this.processAIMessage(node);
                            }
                        }
                    });
                }
            });
        });
        
        // Observe floating chat
        setTimeout(() => {
            const floatingChatMessages = document.getElementById('floating-chat-messages');
            if (floatingChatMessages) {
                observer.observe(floatingChatMessages, { childList: true, subtree: true });
                console.log('[CodeToTabs] 👀 Watching floating chat for code');
            }
            
            // Observe orchestra chat
            const orchestraMessages = document.getElementById('orchestra-messages');
            if (orchestraMessages) {
                observer.observe(orchestraMessages, { childList: true, subtree: true });
                console.log('[CodeToTabs] 👀 Watching orchestra chat for code');
            }
            
            // Observe sidebar chat
            const sidebarMessages = document.getElementById('ai-chat-messages');
            if (sidebarMessages) {
                observer.observe(sidebarMessages, { childList: true, subtree: true });
                console.log('[CodeToTabs] 👀 Watching sidebar chat for code');
            }
        }, 1000);
    }
    
    isAIMessage(node) {
        // Check if node is an AI message (not user message)
        const text = node.textContent || '';
        const hasAIIndicator = text.includes('BigDaddyG') || 
                              text.includes('🤖') ||
                              node.innerHTML?.includes('BigDaddyG');
        
        // Also check for code blocks
        const hasCodeBlock = node.innerHTML?.includes('```') || 
                            node.querySelector?.('pre') ||
                            node.querySelector?.('code');
        
        return hasAIIndicator && hasCodeBlock;
    }
    
    async processAIMessage(node) {
        if (!this.autoCreateTabs) return;
        
        console.log('[CodeToTabs] 🔍 Processing AI message for code blocks...');
        
        // Extract code blocks from the message
        const codeBlocks = this.extractCodeBlocks(node);
        
        if (codeBlocks.length === 0) {
            console.log('[CodeToTabs] ℹ️ No code blocks found');
            return;
        }
        
        console.log(`[CodeToTabs] 📦 Found ${codeBlocks.length} code block(s)`);
        
        // Optional: Close existing tabs
        if (this.closeExistingTabs) {
            this.closeAllTabs();
        }
        
        // Create tabs for each code block
        for (const block of codeBlocks) {
            await this.createTabFromCode(block);
        }
        
        // Show notification
        this.showNotification(`✅ Created ${codeBlocks.length} tab(s) from AI code`);
    }
    
    extractCodeBlocks(node) {
        const blocks = [];
        const content = node.textContent || '';
        
        // Match code blocks with language and optional filename
        // Format: ```language filename\ncode\n```
        // Or: ```language\ncode\n```
        const regex = /```(\w+)(?:\s+([^\n]+))?\n([\s\S]*?)```/g;
        let match;
        
        while ((match = regex.exec(content)) !== null) {
            const language = match[1];
            const filename = match[2] ? match[2].trim() : null;
            const code = match[3].trim();
            
            blocks.push({
                language,
                filename: filename || this.generateFilename(language),
                code
            });
        }
        
        // Also try to extract from <pre><code> tags
        const preElements = node.querySelectorAll('pre code');
        preElements.forEach((codeEl) => {
            const code = codeEl.textContent.trim();
            if (code && code.length > 10) { // Ignore tiny snippets
                // Try to detect language from class
                const classMatch = codeEl.className.match(/language-(\w+)/);
                const language = classMatch ? classMatch[1] : 'text';
                
                // Check if we already extracted this (avoid duplicates)
                const isDuplicate = blocks.some(b => b.code === code);
                if (!isDuplicate) {
                    blocks.push({
                        language,
                        filename: this.generateFilename(language),
                        code
                    });
                }
            }
        });
        
        return blocks;
    }
    
    generateFilename(language) {
        const extensions = {
            'javascript': 'js',
            'typescript': 'ts',
            'python': 'py',
            'java': 'java',
            'cpp': 'cpp',
            'c': 'c',
            'csharp': 'cs',
            'go': 'go',
            'rust': 'rs',
            'php': 'php',
            'ruby': 'rb',
            'swift': 'swift',
            'kotlin': 'kt',
            'html': 'html',
            'css': 'css',
            'sql': 'sql',
            'bash': 'sh',
            'shell': 'sh',
            'powershell': 'ps1',
            'asm': 'asm',
            'assembly': 'asm',
            'json': 'json',
            'xml': 'xml',
            'yaml': 'yaml',
            'markdown': 'md'
        };
        
        const ext = extensions[language.toLowerCase()] || 'txt';
        const timestamp = Date.now();
        return `ai-generated-${timestamp}.${ext}`;
    }
    
    async createTabFromCode(block) {
        try {
            console.log(`[CodeToTabs] 📝 Creating tab: ${block.filename}`);
            
            // Use the agentic global API to create tab
            if (window.agenticFileOps && typeof window.agenticFileOps.createNewTab === 'function') {
                const tabId = window.agenticFileOps.createNewTab(
                    block.filename,
                    block.language,
                    block.code,
                    null // No file path yet (not saved)
                );
                
                if (tabId) {
                    console.log(`[CodeToTabs] ✅ Created tab: ${block.filename} (${tabId})`);
                    this.extractedFiles.push({
                        tabId,
                        filename: block.filename,
                        language: block.language
                    });
                    
                    // Add visual indicator to the chat message
                    this.addTabCreatedIndicator(block.filename);
                } else {
                    console.error(`[CodeToTabs] ❌ Failed to create tab for ${block.filename}`);
                }
            } else if (typeof createNewTab === 'function') {
                // Fallback to direct function call
                const tabId = createNewTab(
                    block.filename,
                    block.language,
                    block.code,
                    null
                );
                
                if (tabId) {
                    console.log(`[CodeToTabs] ✅ Created tab: ${block.filename} (${tabId})`);
                }
            } else {
                console.error('[CodeToTabs] ❌ No tab creation function available');
            }
            
        } catch (error) {
            console.error(`[CodeToTabs] ❌ Error creating tab:`, error);
        }
    }
    
    closeAllTabs() {
        console.log('[CodeToTabs] 🗑️ Closing all existing tabs...');
        
        if (window.openTabs) {
            const tabIds = Object.keys(window.openTabs);
            let closed = 0;
            
            tabIds.forEach(tabId => {
                // Don't close the welcome tab
                if (tabId === 'welcome' || tabId.includes('welcome')) return;
                
                // Delete the tab directly
                delete window.openTabs[tabId];
                closed++;
            });
            
            // Re-render tabs and switch to welcome/first tab
            if (typeof renderTabs === 'function') {
                renderTabs();
            }
            
            // Switch to welcome or first remaining tab
            const remainingTabs = Object.keys(window.openTabs);
            if (remainingTabs.length > 0 && typeof switchTab === 'function') {
                switchTab(remainingTabs[0]);
            }
            
            console.log(`[CodeToTabs] ✅ Closed ${closed} tab(s)`);
        }
    }
    
    addTabCreatedIndicator(filename) {
        // Add a small badge to show a tab was created
        // This helps users know the code is now in a tab
        console.log(`[CodeToTabs] 📌 Tab created: ${filename}`);
    }
    
    showNotification(message) {
        // Show a notification using the agentic API if available
        if (window.agentic && window.agentic.ui && window.agentic.ui.notify) {
            window.agentic.ui.notify(message, 'success');
        } else {
            console.log(`[CodeToTabs] 📢 ${message}`);
        }
    }
    
    // Public API for controlling behavior
    setAutoCreate(enabled) {
        this.autoCreateTabs = enabled;
        console.log(`[CodeToTabs] ⚙️ Auto-create tabs: ${enabled ? 'ON' : 'OFF'}`);
    }
    
    setCloseExisting(enabled) {
        this.closeExistingTabs = enabled;
        console.log(`[CodeToTabs] ⚙️ Close existing tabs: ${enabled ? 'ON' : 'OFF'}`);
    }
    
    getExtractedFiles() {
        return this.extractedFiles;
    }
    
    // Manual extraction for specific content
    async extractAndCreateTabs(content) {
        const tempDiv = document.createElement('div');
        tempDiv.textContent = content;
        
        const codeBlocks = this.extractCodeBlocks(tempDiv);
        
        for (const block of codeBlocks) {
            await this.createTabFromCode(block);
        }
        
        return codeBlocks.length;
    }
}

// Initialize and expose globally
window.aiCodeToTabs = new AICodeToTabs();

// Expose quick toggle functions
window.toggleAutoCreateTabs = () => {
    window.aiCodeToTabs.setAutoCreate(!window.aiCodeToTabs.autoCreateTabs);
};

window.toggleCloseExistingTabs = () => {
    window.aiCodeToTabs.setCloseExisting(!window.aiCodeToTabs.closeExistingTabs);
};

console.log('[CodeToTabs] 📦 AI Code to Tabs system loaded');
console.log('[CodeToTabs] 💡 Usage:');
console.log('  • AI code blocks auto-create tabs');
console.log('  • toggleAutoCreateTabs() - Enable/disable');
console.log('  • toggleCloseExistingTabs() - Close old tabs first');
console.log('  • window.aiCodeToTabs - Full API access');

})();

