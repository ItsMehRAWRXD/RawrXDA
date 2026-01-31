/**
 * Enhanced Chatbox JavaScript for AI Desktop Helper
 * Provides sophisticated chat interface with external AI integration
 */

import { ApiService } from './services/api.service.js';
import { NativeService } from './services/native.service.js';
import { StateService } from './services/state.service.js';

class EnhancedChatbox {
    constructor() {
        this.apiService = new ApiService('https://your-api-server.com');
        this.nativeService = new NativeService();
        this.stateService = new StateService();
        
        this.isConnected = false;
        this.isTyping = false;
        this.currentSettings = {};
        
        this.initialize();
    }

    /**
     * Initializes the chatbox
     */
    async initialize() {
        console.log('Initializing Enhanced Chatbox');
        
        // Set up event listeners
        this.setupEventListeners();
        
        // Load settings and state
        await this.loadSettings();
        await this.loadChatHistory();
        
        // Check connections
        await this.checkConnections();
        
        // Set welcome message timestamp
        this.setWelcomeTime();
        
        console.log('Enhanced Chatbox initialized');
    }

    /**
     * Sets up event listeners
     */
    setupEventListeners() {
        // Chat input events
        const chatInput = document.getElementById('chatInput');
        const sendButton = document.getElementById('sendButton');
        const fileInput = document.getElementById('fileInput');
        const fileUpload = document.getElementById('fileUpload');

        // Send message events
        sendButton.addEventListener('click', () => this.sendMessage());
        chatInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendMessage();
            }
        });

        // Auto-resize textarea
        chatInput.addEventListener('input', () => {
            this.autoResizeTextarea(chatInput);
        });

        // File upload events
        fileUpload.addEventListener('click', () => fileInput.click());
        fileInput.addEventListener('change', (e) => this.handleFileUpload(e));

        // Settings panel events
        const settingsToggle = document.getElementById('settingsToggle');
        const settingsClose = document.getElementById('settingsClose');
        const settingsPanel = document.getElementById('settingsPanel');

        settingsToggle.addEventListener('click', () => {
            settingsPanel.classList.toggle('open');
        });

        settingsClose.addEventListener('click', () => {
            settingsPanel.classList.remove('open');
        });

        // Settings form events
        this.setupSettingsEvents();

        // Connection status updates
        this.stateService.subscribe('connectionStatus', (status) => {
            this.updateConnectionStatus(status);
        });

        // Message updates
        this.stateService.subscribe('messageAdded', (message) => {
            this.addMessageToUI(message);
        });
    }

    /**
     * Sets up settings panel events
     */
    setupSettingsEvents() {
        const saveSettings = document.getElementById('saveSettings');
        const clearHistory = document.getElementById('clearHistory');
        const exportChat = document.getElementById('exportChat');
        const testConnection = document.getElementById('testConnection');
        const temperature = document.getElementById('temperature');
        const temperatureValue = document.getElementById('temperatureValue');

        saveSettings.addEventListener('click', () => this.saveSettings());
        clearHistory.addEventListener('click', () => this.clearHistory());
        exportChat.addEventListener('click', () => this.exportChat());
        testConnection.addEventListener('click', () => this.testConnection());

        // Temperature slider
        temperature.addEventListener('input', (e) => {
            temperatureValue.textContent = e.target.value;
        });
    }

    /**
     * Loads settings from storage
     */
    async loadSettings() {
        try {
            const settings = await this.stateService.getState();
            this.currentSettings = settings.settings || {};
            
            // Populate settings form
            document.getElementById('aiProvider').value = this.currentSettings.aiProvider || 'openai';
            document.getElementById('apiKey').value = this.currentSettings.apiKey || '';
            document.getElementById('apiUrl').value = this.currentSettings.apiUrl || '';
            document.getElementById('temperature').value = this.currentSettings.temperature || 0.7;
            document.getElementById('temperatureValue').textContent = this.currentSettings.temperature || 0.7;
            document.getElementById('maxTokens').value = this.currentSettings.maxTokens || 1000;
            
        } catch (error) {
            console.error('Error loading settings:', error);
        }
    }

    /**
     * Loads chat history
     */
    async loadChatHistory() {
        try {
            const history = this.stateService.getConversationHistory();
            
            // Clear existing messages (except welcome message)
            const chatLog = document.getElementById('chatLog');
            const welcomeMessage = chatLog.querySelector('.message.assistant');
            chatLog.innerHTML = '';
            if (welcomeMessage) {
                chatLog.appendChild(welcomeMessage);
            }
            
            // Add history messages
            history.forEach(message => {
                this.addMessageToUI(message);
            });
            
        } catch (error) {
            console.error('Error loading chat history:', error);
        }
    }

    /**
     * Checks connection status
     */
    async checkConnections() {
        try {
            const apiConnected = await this.apiService.isAuthenticated();
            const nativeConnected = this.nativeService.isServiceConnected();
            
            await this.stateService.updateConnectionStatus('api', apiConnected);
            await this.stateService.updateConnectionStatus('native', nativeConnected);
            
            this.isConnected = apiConnected || nativeConnected;
            
        } catch (error) {
            console.error('Error checking connections:', error);
        }
    }

    /**
     * Updates connection status display
     */
    updateConnectionStatus(status) {
        const statusIndicator = document.getElementById('connectionStatus');
        const statusText = document.getElementById('connectionText');
        
        if (status.api || status.native) {
            statusIndicator.classList.add('connected');
            statusText.textContent = 'Connected';
            this.isConnected = true;
        } else {
            statusIndicator.classList.remove('connected');
            statusText.textContent = 'Disconnected';
            this.isConnected = false;
        }
    }

    /**
     * Sends a message
     */
    async sendMessage() {
        const chatInput = document.getElementById('chatInput');
        const message = chatInput.value.trim();
        
        if (!message || this.isTyping) return;
        
        // Clear input
        chatInput.value = '';
        this.autoResizeTextarea(chatInput);
        
        // Add user message to UI
        const userMessage = {
            role: 'user',
            content: message,
            timestamp: new Date().toISOString()
        };
        
        this.addMessageToUI(userMessage);
        await this.stateService.addMessage(userMessage);
        
        // Show typing indicator
        this.showTypingIndicator();
        
        try {
            // Send message to AI
            const response = await this.apiService.sendChatMessage(
                message,
                '',
                this.currentSettings.aiProvider || 'openai'
            );
            
            // Hide typing indicator
            this.hideTypingIndicator();
            
            // Add AI response to UI
            const aiMessage = {
                role: 'assistant',
                content: response.message,
                timestamp: new Date().toISOString()
            };
            
            this.addMessageToUI(aiMessage);
            await this.stateService.addMessage(aiMessage);
            
        } catch (error) {
            this.hideTypingIndicator();
            this.showError('Failed to send message: ' + error.message);
        }
    }

    /**
     * Adds a message to the UI
     */
    addMessageToUI(message) {
        const chatLog = document.getElementById('chatLog');
        const messageElement = this.createMessageElement(message);
        chatLog.appendChild(messageElement);
        chatLog.scrollTop = chatLog.scrollHeight;
    }

    /**
     * Creates a message element
     */
    createMessageElement(message) {
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${message.role}`;
        
        const avatar = document.createElement('div');
        avatar.className = 'message-avatar';
        avatar.textContent = message.role === 'user' ? '' : '';
        
        const content = document.createElement('div');
        content.className = 'message-content';
        
        // Process content for code blocks and formatting
        content.innerHTML = this.formatMessageContent(message.content);
        
        const time = document.createElement('div');
        time.className = 'message-time';
        time.textContent = new Date(message.timestamp).toLocaleTimeString();
        
        messageDiv.appendChild(avatar);
        messageDiv.appendChild(content);
        content.appendChild(time);
        
        return messageDiv;
    }

    /**
     * Formats message content for display
     */
    formatMessageContent(content) {
        // Handle code blocks
        content = content.replace(/```(\w+)?\n([\s\S]*?)```/g, (match, lang, code) => {
            const language = lang || 'text';
            return `
                <div class="code-block">
                    <pre><code class="language-${language}">${this.escapeHtml(code.trim())}</code></pre>
                    <button class="copy-button" onclick="navigator.clipboard.writeText('${this.escapeHtml(code.trim())}')">Copy</button>
                </div>
            `;
        });
        
        // Handle inline code
        content = content.replace(/`([^`]+)`/g, '<code>$1</code>');
        
        // Handle line breaks
        content = content.replace(/\n/g, '<br>');
        
        return content;
    }

    /**
     * Escapes HTML characters
     */
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    /**
     * Shows typing indicator
     */
    showTypingIndicator() {
        this.isTyping = true;
        document.getElementById('typingIndicator').classList.add('show');
        document.getElementById('sendButton').disabled = true;
    }

    /**
     * Hides typing indicator
     */
    hideTypingIndicator() {
        this.isTyping = false;
        document.getElementById('typingIndicator').classList.remove('show');
        document.getElementById('sendButton').disabled = false;
    }

    /**
     * Handles file upload
     */
    async handleFileUpload(event) {
        const file = event.target.files[0];
        if (!file) return;
        
        try {
            // Show upload progress
            this.showMessage('Uploading file: ' + file.name, 'system');
            
            // Upload file to native service
            const fileContent = await this.readFileAsText(file);
            const response = await this.nativeService.saveFile(file.name, fileContent);
            
            if (response.response) {
                this.showMessage('File uploaded successfully: ' + file.name, 'system');
                
                // Add file to state
                await this.stateService.addFile({
                    name: file.name,
                    size: file.size,
                    timestamp: new Date().toISOString()
                });
            } else {
                this.showError('Failed to upload file: ' + (response.error || 'Unknown error'));
            }
            
        } catch (error) {
            this.showError('Error uploading file: ' + error.message);
        }
        
        // Clear file input
        event.target.value = '';
    }

    /**
     * Reads file as text
     */
    readFileAsText(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = (e) => resolve(e.target.result);
            reader.onerror = (e) => reject(e);
            reader.readAsText(file);
        });
    }

    /**
     * Shows a system message
     */
    showMessage(message, type = 'system') {
        const chatLog = document.getElementById('chatLog');
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${type}`;
        
        const content = document.createElement('div');
        content.className = 'message-content';
        content.textContent = message;
        
        messageDiv.appendChild(content);
        chatLog.appendChild(messageDiv);
        chatLog.scrollTop = chatLog.scrollHeight;
    }

    /**
     * Shows an error message
     */
    showError(message) {
        const chatLog = document.getElementById('chatLog');
        const errorDiv = document.createElement('div');
        errorDiv.className = 'error-message';
        errorDiv.textContent = message;
        
        chatLog.appendChild(errorDiv);
        chatLog.scrollTop = chatLog.scrollHeight;
        
        // Remove error message after 5 seconds
        setTimeout(() => {
            if (errorDiv.parentNode) {
                errorDiv.parentNode.removeChild(errorDiv);
            }
        }, 5000);
    }

    /**
     * Auto-resizes textarea
     */
    autoResizeTextarea(textarea) {
        textarea.style.height = 'auto';
        textarea.style.height = Math.min(textarea.scrollHeight, 120) + 'px';
    }

    /**
     * Sets welcome message timestamp
     */
    setWelcomeTime() {
        const welcomeTime = document.getElementById('welcomeTime');
        if (welcomeTime) {
            welcomeTime.textContent = new Date().toLocaleTimeString();
        }
    }

    /**
     * Saves settings
     */
    async saveSettings() {
        try {
            const settings = {
                aiProvider: document.getElementById('aiProvider').value,
                apiKey: document.getElementById('apiKey').value,
                apiUrl: document.getElementById('apiUrl').value,
                temperature: parseFloat(document.getElementById('temperature').value),
                maxTokens: parseInt(document.getElementById('maxTokens').value)
            };
            
            await this.stateService.updateSettings(settings);
            this.currentSettings = settings;
            
            // Update API service configuration
            if (settings.apiKey) {
                this.apiService.setAuthToken(settings.apiKey);
            }
            
            this.showMessage('Settings saved successfully', 'system');
            
        } catch (error) {
            this.showError('Failed to save settings: ' + error.message);
        }
    }

    /**
     * Clears chat history
     */
    async clearHistory() {
        if (confirm('Are you sure you want to clear the chat history?')) {
            try {
                await this.stateService.clearConversationHistory();
                
                // Clear UI (except welcome message)
                const chatLog = document.getElementById('chatLog');
                const welcomeMessage = chatLog.querySelector('.message.assistant');
                chatLog.innerHTML = '';
                if (welcomeMessage) {
                    chatLog.appendChild(welcomeMessage);
                }
                
                this.showMessage('Chat history cleared', 'system');
                
            } catch (error) {
                this.showError('Failed to clear history: ' + error.message);
            }
        }
    }

    /**
     * Exports chat history
     */
    async exportChat() {
        try {
            const history = this.stateService.getConversationHistory();
            const exportData = {
                exportDate: new Date().toISOString(),
                messages: history
            };
            
            const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = `chat-export-${new Date().toISOString().split('T')[0]}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            this.showMessage('Chat exported successfully', 'system');
            
        } catch (error) {
            this.showError('Failed to export chat: ' + error.message);
        }
    }

    /**
     * Tests connection
     */
    async testConnection() {
        try {
            this.showMessage('Testing connection...', 'system');
            
            const response = await this.apiService.sendChatMessage('Hello, this is a connection test.', '', 'openai');
            
            if (response.message) {
                this.showMessage('Connection test successful!', 'system');
            } else {
                this.showError('Connection test failed: No response received');
            }
            
        } catch (error) {
            this.showError('Connection test failed: ' + error.message);
        }
    }
}

// Initialize the enhanced chatbox when the page loads
document.addEventListener('DOMContentLoaded', () => {
    new EnhancedChatbox();
});
