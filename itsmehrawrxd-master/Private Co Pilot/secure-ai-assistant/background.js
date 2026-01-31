// Secure AI Desktop Helper - Background Service Worker
class BackgroundService {
    constructor() {
        this.conversationHistory = [];
        this.rateLimitMap = new Map();
        this.init();
    }

    init() {
        this.setupMessageListeners();
        this.setupAlarms();
        console.log('Background service initialized');
    }

    setupMessageListeners() {
        chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
            this.handleMessage(request, sender, sendResponse);
            return true; // Keep message channel open for async responses
        });
    }

    async handleMessage(request, sender, sendResponse) {
        try {
            switch (request.action) {
                case 'quickCommand':
                    await this.handleQuickCommand(request, sender, sendResponse);
                    break;
                case 'chatMessage':
                    await this.handleChatMessage(request, sender, sendResponse);
                    break;
                case 'sendFile':
                    await this.handleFileTransfer(request, sender, sendResponse);
                    break;
                case 'captureContent':
                    await this.handleContentCapture(request, sender, sendResponse);
                    break;
                case 'getApiKey':
                    await this.handleApiKeyRequest(sendResponse);
                    break;
                default:
                    sendResponse({ success: false, error: 'Unknown action' });
            }
        } catch (error) {
            console.error('Error handling message:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async handleQuickCommand(request, sender, sendResponse) {
        const { command } = request;
        
        // Rate limiting check
        if (!this.checkRateLimit(sender.tab?.id || 'popup')) {
            sendResponse({ success: false, error: 'Rate limit exceeded' });
            return;
        }

        try {
            const apiKey = await this.getApiKey();
            if (!apiKey) {
                sendResponse({ success: false, error: 'API key not configured' });
                return;
            }

            const response = await this.callOpenAI(apiKey, command, []);
            
            // Send response to active tab
            const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
            if (tab) {
                chrome.tabs.sendMessage(tab.id, {
                    action: 'displayResponse',
                    response: response
                });
            }

            sendResponse({ success: true, response: response });
        } catch (error) {
            console.error('Quick command error:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async handleChatMessage(request, sender, sendResponse) {
        const { message } = request;
        
        // Add to conversation history
        this.conversationHistory.push({ role: 'user', content: message });
        
        // Keep only last 20 messages to manage context
        if (this.conversationHistory.length > 20) {
            this.conversationHistory = this.conversationHistory.slice(-20);
        }

        try {
            const apiKey = await this.getApiKey();
            if (!apiKey) {
                sendResponse({ success: false, error: 'API key not configured' });
                return;
            }

            const response = await this.callOpenAI(apiKey, message, this.conversationHistory);
            
            // Add AI response to history
            this.conversationHistory.push({ role: 'assistant', content: response });
            
            // Save conversation to storage
            await this.saveConversationHistory();

            // Send response to chatbox
            chrome.runtime.sendMessage({
                action: 'aiResponse',
                response: response
            });

            sendResponse({ success: true, response: response });
        } catch (error) {
            console.error('Chat message error:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async handleFileTransfer(request, sender, sendResponse) {
        const { fileData, fileName, fileType } = request;
        
        try {
            // Encrypt file data before processing
            const encryptedData = await this.encryptData(fileData);
            
            // Store file temporarily
            const fileId = this.generateFileId();
            await chrome.storage.local.set({
                [`file_${fileId}`]: {
                    data: encryptedData,
                    fileName: fileName,
                    fileType: fileType,
                    timestamp: Date.now()
                }
            });

            // Process file with AI
            const apiKey = await this.getApiKey();
            if (apiKey) {
                const prompt = `Analyze this file: ${fileName} (${fileType}). Provide a summary and any relevant insights.`;
                const response = await this.callOpenAI(apiKey, prompt, []);
                
                sendResponse({ 
                    success: true, 
                    fileId: fileId,
                    analysis: response 
                });
            } else {
                sendResponse({ success: false, error: 'API key not configured' });
            }
        } catch (error) {
            console.error('File transfer error:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async handleContentCapture(request, sender, sendResponse) {
        try {
            const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
            if (!tab) {
                sendResponse({ success: false, error: 'No active tab found' });
                return;
            }

            // Capture page content
            const results = await chrome.tabs.sendMessage(tab.id, {
                action: 'getPageContent'
            });

            if (results && results.content) {
                // Store content for AI analysis
                const contentId = this.generateFileId();
                await chrome.storage.local.set({
                    [`content_${contentId}`]: {
                        content: results.content,
                        url: tab.url,
                        title: tab.title,
                        timestamp: Date.now()
                    }
                });

                sendResponse({ 
                    success: true, 
                    contentId: contentId,
                    content: results.content.substring(0, 500) + '...' // Preview
                });
            } else {
                sendResponse({ success: false, error: 'Failed to capture content' });
            }
        } catch (error) {
            console.error('Content capture error:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async handleApiKeyRequest(sendResponse) {
        try {
            const apiKey = await this.getApiKey();
            sendResponse({ success: true, hasKey: !!apiKey });
        } catch (error) {
            sendResponse({ success: false, error: error.message });
        }
    }

    async callOpenAI(apiKey, message, history = []) {
        const messages = [
            {
                role: 'system',
                content: 'You are a helpful AI assistant integrated into a secure browser extension. Provide clear, concise responses and help users with their tasks.'
            },
            ...history,
            {
                role: 'user',
                content: message
            }
        ];

        const response = await fetch('https://api.openai.com/v1/chat/completions', {
            method: 'POST',
            headers: {
                'Authorization': `Bearer ${apiKey}`,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                model: 'gpt-3.5-turbo',
                messages: messages,
                max_tokens: 1000,
                temperature: 0.7
            })
        });

        if (!response.ok) {
            throw new Error(`OpenAI API error: ${response.status}`);
        }

        const data = await response.json();
        return data.choices[0].message.content;
    }

    async getApiKey() {
        try {
            const result = await chrome.storage.local.get(['apiKey']);
            if (result.apiKey) {
                return await this.decryptData(result.apiKey);
            }
            return null;
        } catch (error) {
            console.error('Error getting API key:', error);
            return null;
        }
    }

    async saveConversationHistory() {
        try {
            await chrome.storage.local.set({
                conversationHistory: this.conversationHistory
            });
        } catch (error) {
            console.error('Error saving conversation history:', error);
        }
    }

    async loadConversationHistory() {
        try {
            const result = await chrome.storage.local.get(['conversationHistory']);
            if (result.conversationHistory) {
                this.conversationHistory = result.conversationHistory;
            }
        } catch (error) {
            console.error('Error loading conversation history:', error);
        }
    }

    checkRateLimit(identifier) {
        const now = Date.now();
        const windowMs = 15 * 60 * 1000; // 15 minutes
        const maxRequests = 50;

        if (!this.rateLimitMap.has(identifier)) {
            this.rateLimitMap.set(identifier, []);
        }

        const requests = this.rateLimitMap.get(identifier);
        
        // Remove old requests outside the window
        const validRequests = requests.filter(time => now - time < windowMs);
        this.rateLimitMap.set(identifier, validRequests);

        if (validRequests.length >= maxRequests) {
            return false;
        }

        validRequests.push(now);
        return true;
    }

    generateFileId() {
        return 'file_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
    }

    // Simple encryption/decryption (in production, use proper encryption)
    async encryptData(data) {
        return btoa(data);
    }

    async decryptData(encryptedData) {
        return atob(encryptedData);
    }

    setupAlarms() {
        // Clean up old files every hour
        chrome.alarms.create('cleanupFiles', { periodInMinutes: 60 });
        
        chrome.alarms.onAlarm.addListener((alarm) => {
            if (alarm.name === 'cleanupFiles') {
                this.cleanupOldFiles();
            }
        });
    }

    async cleanupOldFiles() {
        try {
            const storage = await chrome.storage.local.get();
            const now = Date.now();
            const maxAge = 24 * 60 * 60 * 1000; // 24 hours

            for (const [key, value] of Object.entries(storage)) {
                if ((key.startsWith('file_') || key.startsWith('content_')) && 
                    value.timestamp && (now - value.timestamp) > maxAge) {
                    await chrome.storage.local.remove([key]);
                }
            }
        } catch (error) {
            console.error('Error cleaning up files:', error);
        }
    }
}

// Initialize the background service
new BackgroundService();

// Load conversation history on startup
chrome.runtime.onStartup.addListener(() => {
    const service = new BackgroundService();
    service.loadConversationHistory();
});
