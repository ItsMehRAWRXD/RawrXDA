// Enhanced Chatbox Controller with Full Integration
class ChatboxController {
    constructor() {
        this.isConnected = false;
        this.isLoggedIn = false;
        this.currentUser = null;
        this.messageHistory = [];
        this.init();
    }

    async init() {
        this.setupEventListeners();
        this.setupMessageHandlers();
        await this.checkAuthStatus();
        this.updateConnectionStatus();
        this.setupAutoResize();
        console.log('Chatbox initialized');
    }

    setupEventListeners() {
        // Send button and enter key
        const sendBtn = document.getElementById('sendBtn');
        const messageInput = document.getElementById('messageInput');

        sendBtn?.addEventListener('click', () => this.sendMessage());
        
        messageInput?.addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendMessage();
            }
        });

        // Toolbar buttons
        document.getElementById('fileBtn')?.addEventListener('click', () => this.showFileManager());
        document.getElementById('commandBtn')?.addEventListener('click', () => this.showCommandDialog());
        document.getElementById('captureBtn')?.addEventListener('click', () => this.capturePageContent());
        document.getElementById('clearBtn')?.addEventListener('click', () => this.clearChat());

        // Auto-resize textarea
        messageInput?.addEventListener('input', () => this.autoResizeTextarea());
    }

    setupMessageHandlers() {
        chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
            switch (request.action) {
                case 'aiResponse':
                    this.displayMessage('assistant', request.response);
                    this.hideTypingIndicator();
                    break;
                case 'nativeResponse':
                    this.handleNativeResponse(request.data);
                    break;
                case 'loginSuccess':
                    this.handleLoginSuccess(request.user);
                    break;
                case 'logoutSuccess':
                    this.handleLogoutSuccess();
                    break;
                case 'nativeHostStatus':
                    this.updateConnectionStatus(request.status === 'connected');
                    break;
                case 'updateStatus':
                    this.showSystemMessage(request.message);
                    break;
            }
        });
    }

    async checkAuthStatus() {
        try {
            const result = await chrome.storage.local.get(['jwtToken', 'userInfo']);
            if (result.jwtToken && result.userInfo) {
                this.isLoggedIn = true;
                this.currentUser = result.userInfo;
                this.showSystemMessage(`Welcome back, ${this.currentUser.username}!`);
            } else {
                this.showLoginPrompt();
            }
        } catch (error) {
            console.error('Error checking auth status:', error);
        }
    }

    async sendMessage() {
        const messageInput = document.getElementById('messageInput');
        const sendBtn = document.getElementById('sendBtn');
        const message = messageInput?.value.trim();

        if (!message) return;

        // Check if logged in
        if (!this.isLoggedIn) {
            this.showSystemMessage('Please log in to send messages');
            this.showLoginPrompt();
            return;
        }

        // Disable input while processing
        if (messageInput) messageInput.disabled = true;
        if (sendBtn) sendBtn.disabled = true;

        // Display user message
        this.displayMessage('user', message);
        messageInput.value = '';
        this.autoResizeTextarea();

        // Show typing indicator
        this.showTypingIndicator();

        try {
            // Determine if this is a special command
            const isCommand = message.startsWith('/');
            const useLocalAI = message.includes('--local') || message.startsWith('/local');

            if (isCommand) {
                await this.handleCommand(message);
            } else {
                // Send regular chat message
                chrome.runtime.sendMessage({
                    action: 'chatMessage',
                    message: message,
                    useLocalAI: useLocalAI
                });
            }
        } catch (error) {
            console.error('Error sending message:', error);
            this.displayMessage('system', 'Error sending message. Please try again.');
            this.hideTypingIndicator();
        } finally {
            // Re-enable input
            if (messageInput) messageInput.disabled = false;
            if (sendBtn) sendBtn.disabled = false;
            messageInput?.focus();
        }
    }

    async handleCommand(command) {
        const parts = command.slice(1).split(' ');
        const cmd = parts[0].toLowerCase();
        const args = parts.slice(1);

        switch (cmd) {
            case 'help':
                this.showHelpMessage();
                break;
            case 'login':
                this.showLoginPrompt();
                break;
            case 'logout':
                await this.logout();
                break;
            case 'save':
                await this.saveFile(args.join(' '));
                break;
            case 'load':
                await this.loadFile(args.join(' '));
                break;
            case 'encrypt':
                await this.encryptData(args.join(' '));
                break;
            case 'decrypt':
                await this.decryptData(args.join(' '));
                break;
            case 'system':
                await this.getSystemInfo();
                break;
            case 'files':
                this.showFileManager();
                break;
            case 'clear':
                this.clearChat();
                break;
            default:
                this.displayMessage('system', `Unknown command: ${cmd}. Type /help for available commands.`);
        }
        this.hideTypingIndicator();
    }

    displayMessage(role, content) {
        const messagesContainer = document.getElementById('messages');
        if (!messagesContainer) return;

        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${role}`;

        const timestamp = new Date().toLocaleTimeString();
        const headerDiv = document.createElement('div');
        headerDiv.className = 'message-header';
        headerDiv.textContent = `${role.charAt(0).toUpperCase() + role.slice(1)} • ${timestamp}`;

        const contentDiv = document.createElement('div');
        contentDiv.className = 'message-content';
        contentDiv.textContent = content;

        messageDiv.appendChild(headerDiv);
        messageDiv.appendChild(contentDiv);
        messagesContainer.appendChild(messageDiv);

        // Scroll to bottom
        messagesContainer.scrollTop = messagesContainer.scrollHeight;

        // Store in history
        this.messageHistory.push({ role, content, timestamp });
    }

    showSystemMessage(message) {
        this.displayMessage('system', message);
    }

    showTypingIndicator() {
        const indicator = document.getElementById('typingIndicator');
        if (indicator) {
            indicator.classList.add('show');
        }
    }

    hideTypingIndicator() {
        const indicator = document.getElementById('typingIndicator');
        if (indicator) {
            indicator.classList.remove('show');
        }
    }

    updateConnectionStatus(connected = true) {
        const statusDot = document.getElementById('statusDot');
        const statusText = document.getElementById('statusText');

        if (statusDot && statusText) {
            if (connected) {
                statusDot.classList.remove('disconnected');
                statusText.textContent = 'Connected';
            } else {
                statusDot.classList.add('disconnected');
                statusText.textContent = 'Disconnected';
            }
        }
        this.isConnected = connected;
    }

    autoResizeTextarea() {
        const textarea = document.getElementById('messageInput');
        if (textarea) {
            textarea.style.height = 'auto';
            textarea.style.height = Math.min(textarea.scrollHeight, 120) + 'px';
        }
    }

    setupAutoResize() {
        const textarea = document.getElementById('messageInput');
        if (textarea) {
            textarea.style.height = '44px';
        }
    }

    // File Management
    async saveFile(filename) {
        if (!filename) {
            this.showSystemMessage('Usage: /save <filename>');
            return;
        }

        try {
            // Get content from current page or prompt user
            const content = await this.getContentToSave();
            
            chrome.runtime.sendMessage({
                action: 'saveFile',
                fileName: filename,
                content: content,
                encrypt: true,
                saveLocal: true // Save to native host by default
            }, (response) => {
                if (response.success) {
                    this.showSystemMessage(`File saved: ${filename}`);
                } else {
                    this.showSystemMessage(`Error saving file: ${response.error}`);
                }
            });
        } catch (error) {
            this.showSystemMessage(`Error: ${error.message}`);
        }
    }

    async loadFile(filename) {
        if (!filename) {
            this.showSystemMessage('Usage: /load <filename>');
            return;
        }

        chrome.runtime.sendMessage({
            action: 'loadFile',
            fileName: filename,
            decrypt: true,
            fromLocal: true
        }, (response) => {
            if (response.success) {
                this.displayMessage('system', `Loaded file: ${filename}`);
                this.displayMessage('assistant', response.content);
            } else {
                this.showSystemMessage(`Error loading file: ${response.error}`);
            }
        });
    }

    async getContentToSave() {
        // Try to get content from current page
        return new Promise((resolve, reject) => {
            chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
                if (tabs[0]) {
                    chrome.tabs.sendMessage(tabs[0].id, { action: 'getPageContent' }, (response) => {
                        if (response && response.content) {
                            resolve(response.content.text);
                        } else {
                            // Fallback: prompt user for content
                            const content = prompt('Enter content to save:');
                            if (content) {
                                resolve(content);
                            } else {
                                reject(new Error('No content provided'));
                            }
                        }
                    });
                } else {
                    reject(new Error('No active tab found'));
                }
            });
        });
    }

    // Encryption Operations
    async encryptData(data) {
        if (!data) {
            this.showSystemMessage('Usage: /encrypt <data>');
            return;
        }

        chrome.runtime.sendMessage({
            action: 'encryptData',
            data: data,
            algorithm: 'AES256'
        }, (response) => {
            if (response.success) {
                this.displayMessage('assistant', `Encrypted: ${response.result.encrypted}`);
            } else {
                this.showSystemMessage(`Encryption failed: ${response.error}`);
            }
        });
    }

    async decryptData(encryptedData) {
        if (!encryptedData) {
            this.showSystemMessage('Usage: /decrypt <encrypted_data>');
            return;
        }

        chrome.runtime.sendMessage({
            action: 'decryptData',
            encrypted: encryptedData
        }, (response) => {
            if (response.success) {
                this.displayMessage('assistant', `Decrypted: ${response.result.decrypted}`);
            } else {
                this.showSystemMessage(`Decryption failed: ${response.error}`);
            }
        });
    }

    // System Operations
    async getSystemInfo() {
        chrome.runtime.sendMessage({
            action: 'getSystemInfo'
        }, (response) => {
            if (response.success) {
                this.showSystemMessage('System info request sent to native host');
            } else {
                this.showSystemMessage(`Error: ${response.error}`);
            }
        });
    }

    async capturePageContent() {
        chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
            if (tabs[0]) {
                chrome.tabs.sendMessage(tabs[0].id, { action: 'captureContent' }, (response) => {
                    if (response && response.success) {
                        this.showSystemMessage('Page content captured successfully');
                    } else {
                        this.showSystemMessage('Failed to capture page content');
                    }
                });
            }
        });
    }

    // UI Operations
    clearChat() {
        const messagesContainer = document.getElementById('messages');
        if (messagesContainer) {
            // Keep the welcome message
            const welcomeMessage = messagesContainer.querySelector('.message.system');
            messagesContainer.innerHTML = '';
            if (welcomeMessage) {
                messagesContainer.appendChild(welcomeMessage);
            }
        }
        this.messageHistory = [];
        this.showSystemMessage('Chat cleared');
    }

    showFileManager() {
        // Create a simple file manager modal
        const modal = document.createElement('div');
        modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.8);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 10000;
        `;

        const content = document.createElement('div');
        content.style.cssText = `
            background: white;
            padding: 20px;
            border-radius: 8px;
            max-width: 500px;
            width: 90%;
            color: black;
        `;

        content.innerHTML = `
            <h3>File Manager</h3>
            <p>File operations:</p>
            <ul>
                <li>/save &lt;filename&gt; - Save content to file</li>
                <li>/load &lt;filename&gt; - Load file content</li>
                <li>/files - Show this dialog</li>
            </ul>
            <button id="closeFileManager" style="margin-top: 10px; padding: 8px 16px;">Close</button>
        `;

        modal.appendChild(content);
        document.body.appendChild(modal);

        document.getElementById('closeFileManager').addEventListener('click', () => {
            document.body.removeChild(modal);
        });

        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                document.body.removeChild(modal);
            }
        });
    }

    showCommandDialog() {
        const command = prompt('Enter command to execute (e.g., "ping google.com"):');
        if (command) {
            chrome.runtime.sendMessage({
                action: 'executeCommand',
                command: command
            }, (response) => {
                if (response.success) {
                    this.showSystemMessage('Command sent to native host');
                } else {
                    this.showSystemMessage(`Error: ${response.error}`);
                }
            });
        }
    }

    showHelpMessage() {
        const helpText = `
Available commands:
/help - Show this help message
/login - Show login prompt
/logout - Logout current user
/save <filename> - Save content to file
/load <filename> - Load file content
/encrypt <data> - Encrypt data
/decrypt <data> - Decrypt data
/system - Get system information
/files - Show file manager
/clear - Clear chat history

You can also add --local to any message to use local AI processing.
        `;
        this.displayMessage('system', helpText.trim());
    }

    // Authentication
    showLoginPrompt() {
        const username = prompt('Username:');
        if (username) {
            const password = prompt('Password:');
            if (password) {
                this.login(username, password);
            }
        }
    }

    async login(username, password) {
        chrome.runtime.sendMessage({
            action: 'login',
            username: username,
            password: password
        }, (response) => {
            if (response.success) {
                this.handleLoginSuccess(response.user);
            } else {
                this.showSystemMessage(`Login failed: ${response.error}`);
            }
        });
    }

    async logout() {
        chrome.runtime.sendMessage({
            action: 'logout'
        }, (response) => {
            if (response.success) {
                this.handleLogoutSuccess();
            } else {
                this.showSystemMessage(`Logout failed: ${response.error}`);
            }
        });
    }

    handleLoginSuccess(user) {
        this.isLoggedIn = true;
        this.currentUser = user;
        this.showSystemMessage(`Welcome, ${user.username}! You are now logged in.`);
        this.updateConnectionStatus(true);
    }

    handleLogoutSuccess() {
        this.isLoggedIn = false;
        this.currentUser = null;
        this.showSystemMessage('You have been logged out.');
        this.updateConnectionStatus(false);
    }

    handleNativeResponse(data) {
        if (data.success) {
            switch (data.action) {
                case 'systemInfo':
                    this.displayMessage('assistant', `System Info: ${JSON.stringify(data.systemInfo, null, 2)}`);
                    break;
                case 'commandResult':
                    this.displayMessage('assistant', `Command Output:\n${data.output}`);
                    if (data.error) {
                        this.displayMessage('system', `Command Error: ${data.error}`);
                    }
                    break;
                default:
                    this.displayMessage('assistant', data.response || JSON.stringify(data));
            }
        } else {
            this.showSystemMessage(`Native host error: ${data.error}`);
        }
    }
}

// Initialize the chatbox when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    new ChatboxController();
});
