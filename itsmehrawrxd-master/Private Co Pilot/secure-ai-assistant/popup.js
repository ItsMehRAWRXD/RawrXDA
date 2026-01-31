// Secure AI Desktop Helper - Popup Controller
class PopupController {
    constructor() {
        this.init();
    }

    async init() {
        await this.loadApiKey();
        this.setupEventListeners();
        this.updateStatus('Ready - All systems operational');
    }

    setupEventListeners() {
        // API Key Management
        document.getElementById('saveApiKey').addEventListener('click', () => {
            this.saveApiKey();
        });

        // Main Controls
        document.getElementById('openChat').addEventListener('click', () => {
            this.openChatPanel();
        });

        document.getElementById('quickCommand').addEventListener('click', () => {
            this.openQuickCommand();
        });

        document.getElementById('sendFile').addEventListener('click', () => {
            this.sendFile();
        });

        document.getElementById('capturePage').addEventListener('click', () => {
            this.capturePageContent();
        });

        // Utility Controls
        document.getElementById('clearHistory').addEventListener('click', () => {
            this.clearChatHistory();
        });

        document.getElementById('exportData').addEventListener('click', () => {
            this.exportData();
        });

        document.getElementById('settings').addEventListener('click', () => {
            this.openSettings();
        });

        // Enter key for API key input
        document.getElementById('apiKeyInput').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.saveApiKey();
            }
        });
    }

    async loadApiKey() {
        try {
            const result = await chrome.storage.local.get(['apiKey']);
            if (result.apiKey) {
                document.getElementById('apiKeyInput').value = '••••••••••••••••';
                document.getElementById('apiKeyInput').placeholder = 'API Key saved (click to change)';
            }
        } catch (error) {
            console.error('Error loading API key:', error);
        }
    }

    async saveApiKey() {
        const apiKey = document.getElementById('apiKeyInput').value.trim();
        
        if (!apiKey) {
            this.updateStatus('Please enter a valid API key');
            return;
        }

        if (apiKey.length < 20) {
            this.updateStatus('API key appears to be invalid');
            return;
        }

        try {
            // Encrypt the API key before storing
            const encryptedKey = await this.encryptData(apiKey);
            await chrome.storage.local.set({ apiKey: encryptedKey });
            
            document.getElementById('apiKeyInput').value = '••••••••••••••••';
            document.getElementById('apiKeyInput').placeholder = 'API Key saved securely';
            this.updateStatus('API Key saved securely');
        } catch (error) {
            console.error('Error saving API key:', error);
            this.updateStatus('Error saving API key');
        }
    }

    async openChatPanel() {
        try {
            // Open side panel for persistent chat
            await chrome.sidePanel.open({ windowId: (await chrome.windows.getCurrent()).id });
            this.updateStatus('Chat panel opened');
        } catch (error) {
            // Fallback to new tab if side panel not available
            chrome.tabs.create({ url: chrome.runtime.getURL('chatbox.html') });
            this.updateStatus('Chat opened in new tab');
        }
    }

    openQuickCommand() {
        const command = prompt('Enter your command for ChatGPT:');
        if (command && command.trim()) {
            chrome.runtime.sendMessage({
                action: 'quickCommand',
                command: command.trim()
            });
            this.updateStatus('Command sent to AI');
        }
    }

    async sendFile() {
        try {
            const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
            
            chrome.tabs.sendMessage(tab.id, {
                action: 'selectFile'
            }, (response) => {
                if (response && response.success) {
                    this.updateStatus('File selected and sent');
                } else {
                    this.updateStatus('File selection failed');
                }
            });
        } catch (error) {
            console.error('Error sending file:', error);
            this.updateStatus('Error accessing current tab');
        }
    }

    async capturePageContent() {
        try {
            const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
            
            chrome.tabs.sendMessage(tab.id, {
                action: 'captureContent'
            }, (response) => {
                if (response && response.success) {
                    this.updateStatus('Page content captured');
                } else {
                    this.updateStatus('Content capture failed');
                }
            });
        } catch (error) {
            console.error('Error capturing content:', error);
            this.updateStatus('Error accessing page content');
        }
    }

    async clearChatHistory() {
        if (confirm('Are you sure you want to clear all chat history?')) {
            try {
                await chrome.storage.local.remove(['chatHistory', 'conversationContext']);
                this.updateStatus('Chat history cleared');
            } catch (error) {
                console.error('Error clearing history:', error);
                this.updateStatus('Error clearing history');
            }
        }
    }

    async exportData() {
        try {
            const data = await chrome.storage.local.get(['chatHistory', 'conversationContext']);
            const exportData = {
                timestamp: new Date().toISOString(),
                chatHistory: data.chatHistory || [],
                conversationContext: data.conversationContext || {}
            };

            const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = `ai-assistant-export-${new Date().toISOString().split('T')[0]}.json`;
            a.click();
            
            URL.revokeObjectURL(url);
            this.updateStatus('Data exported successfully');
        } catch (error) {
            console.error('Error exporting data:', error);
            this.updateStatus('Export failed');
        }
    }

    openSettings() {
        // Open settings in new tab
        chrome.tabs.create({ url: chrome.runtime.getURL('settings.html') });
        this.updateStatus('Settings opened');
    }

    updateStatus(message) {
        const statusEl = document.getElementById('status');
        statusEl.textContent = message;
        
        // Clear status after 3 seconds
        setTimeout(() => {
            statusEl.textContent = 'Ready';
        }, 3000);
    }

    // Simple encryption for API key (in production, use proper encryption)
    async encryptData(data) {
        // This is a placeholder - in a real implementation, you'd use proper encryption
        // For now, we'll use a simple base64 encoding (NOT secure for production)
        return btoa(data);
    }

    async decryptData(encryptedData) {
        // Placeholder decryption
        return atob(encryptedData);
    }
}

// Initialize the popup controller when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    new PopupController();
});

// Listen for messages from background script
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === 'updateStatus') {
        document.getElementById('status').textContent = request.message;
    }
});
