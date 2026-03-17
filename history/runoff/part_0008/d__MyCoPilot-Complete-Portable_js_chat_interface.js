// Advanced IDE Chat Integration
class IDEChatInterface {
    constructor(options = {}) {
        this.apiBase = options.apiBase || 'http://localhost:8080';
        this.wsBase = options.wsBase || 'ws://localhost:8080';
        this.chatEngine = null;
        this.messageQueue = [];
        this.isConnected = false;
        this.initialize();
    }

    async initialize() {
        try {
            // Initialize WebSocket connection
            this.ws = new WebSocket(`${this.wsBase}/chat/ws`);
            this.ws.onopen = () => {
                this.isConnected = true;
                this.processMessageQueue();
                this.dispatchEvent('connected');
            };
            this.ws.onclose = () => {
                this.isConnected = false;
                this.dispatchEvent('disconnected');
            };
            this.ws.onmessage = (event) => this.handleWebSocketMessage(event);

            // Initialize UI elements
            this.initializeUI();
            
            // Connect to chat backend
            await this.connectChatEngine();
            
            return true;
        } catch (error) {
            console.error('Chat interface initialization failed:', error);
            return false;
        }
    }

    initializeUI() {
        // Create chat interface elements
        const chatContainer = document.createElement('div');
        chatContainer.id = 'ide-chat-container';
        chatContainer.innerHTML = `
            <div class="chat-header">
                <h3>AI Assistant</h3>
                <div class="chat-controls">
                    <button id="clear-chat">Clear</button>
                    <button id="toggle-chat">Hide</button>
                </div>
            </div>
            <div id="chat-messages" class="chat-messages"></div>
            <div class="chat-input-container">
                <textarea id="chat-input" placeholder="Ask me anything..."></textarea>
                <button id="send-message">Send</button>
            </div>
        `;

        // Add styles
        const styles = document.createElement('style');
        styles.textContent = `
            #ide-chat-container {
                position: fixed;
                right: 20px;
                bottom: 20px;
                width: 350px;
                height: 500px;
                background: #1e1e1e;
                border: 1px solid #333;
                display: flex;
                flex-direction: column;
                border-radius: 8px;
                overflow: hidden;
                box-shadow: 0 4px 12px rgba(0,0,0,0.15);
            }
            .chat-header {
                padding: 12px;
                background: #252526;
                border-bottom: 1px solid #333;
                display: flex;
                justify-content: space-between;
                align-items: center;
            }
            .chat-header h3 {
                margin: 0;
                color: #fff;
            }
            .chat-controls button {
                padding: 4px 8px;
                margin-left: 8px;
                background: #333;
                border: none;
                color: #fff;
                border-radius: 4px;
                cursor: pointer;
            }
            .chat-messages {
                flex: 1;
                overflow-y: auto;
                padding: 12px;
            }
            .chat-message {
                margin: 8px 0;
                padding: 8px;
                border-radius: 4px;
                max-width: 80%;
            }
            .user-message {
                background: #2d2d2d;
                color: #fff;
                margin-left: auto;
            }
            .assistant-message {
                background: #264f78;
                color: #fff;
            }
            .chat-input-container {
                padding: 12px;
                border-top: 1px solid #333;
                display: flex;
                gap: 8px;
            }
            #chat-input {
                flex: 1;
                padding: 8px;
                border: 1px solid #333;
                border-radius: 4px;
                background: #1e1e1e;
                color: #fff;
                resize: none;
                height: 60px;
            }
            #send-message {
                padding: 8px 16px;
                background: #0e639c;
                color: #fff;
                border: none;
                border-radius: 4px;
                cursor: pointer;
            }
            #send-message:hover {
                background: #1177bb;
            }
        `;

        document.head.appendChild(styles);
        document.body.appendChild(chatContainer);

        // Add event listeners
        document.getElementById('send-message').addEventListener('click', () => this.sendMessage());
        document.getElementById('chat-input').addEventListener('keydown', (e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                this.sendMessage();
            }
        });
        document.getElementById('clear-chat').addEventListener('click', () => this.clearChat());
        document.getElementById('toggle-chat').addEventListener('click', () => this.toggleChat());
    }

    async connectChatEngine() {
        try {
            const response = await fetch(`${this.apiBase}/api/chat/connect`, {
                method: 'POST'
            });
            if (!response.ok) throw new Error('Failed to connect to chat engine');
            this.dispatchEvent('engineConnected');
            return true;
        } catch (error) {
            console.error('Chat engine connection failed:', error);
            return false;
        }
    }

    async sendMessage() {
        const input = document.getElementById('chat-input');
        const content = input.value.trim();
        if (!content) return;

        input.value = '';
        
        const message = {
            role: 'user',
            content,
            timestamp: new Date().toISOString()
        };

        this.addMessageToUI(message);

        if (this.isConnected) {
            await this.sendToServer(message);
        } else {
            this.messageQueue.push(message);
        }
    }

    async sendToServer(message) {
        try {
            const response = await fetch(`${this.apiBase}/api/chat/message`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(message)
            });

            if (!response.ok) throw new Error('Failed to send message');
            
            const result = await response.json();
            if (result.response) {
                this.addMessageToUI({
                    role: 'assistant',
                    content: result.response,
                    timestamp: new Date().toISOString()
                });
            }
        } catch (error) {
            console.error('Failed to send message:', error);
            this.addMessageToUI({
                role: 'system',
                content: 'Failed to send message. Please try again.',
                timestamp: new Date().toISOString()
            });
        }
    }

    addMessageToUI(message) {
        const messagesContainer = document.getElementById('chat-messages');
        const messageElement = document.createElement('div');
        messageElement.className = `chat-message ${message.role}-message`;
        messageElement.innerHTML = `
            <div class="message-content">${this.escapeHtml(message.content)}</div>
            <div class="message-timestamp">${new Date(message.timestamp).toLocaleTimeString()}</div>
        `;
        messagesContainer.appendChild(messageElement);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    async processMessageQueue() {
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            await this.sendToServer(message);
        }
    }

    async clearChat() {
        try {
            await fetch(`${this.apiBase}/api/chat/clear`, { method: 'POST' });
            document.getElementById('chat-messages').innerHTML = '';
        } catch (error) {
            console.error('Failed to clear chat:', error);
        }
    }

    toggleChat() {
        const container = document.getElementById('ide-chat-container');
        const isVisible = container.style.display !== 'none';
        container.style.display = isVisible ? 'none' : 'flex';
        document.getElementById('toggle-chat').textContent = isVisible ? 'Show' : 'Hide';
    }

    handleWebSocketMessage(event) {
        try {
            const data = JSON.parse(event.data);
            if (data.type === 'message') {
                this.addMessageToUI({
                    role: data.role,
                    content: data.content,
                    timestamp: data.timestamp || new Date().toISOString()
                });
            }
        } catch (error) {
            console.error('Failed to handle WebSocket message:', error);
        }
    }

    dispatchEvent(eventName) {
        const event = new CustomEvent(`ide-chat-${eventName}`, {
            detail: { timestamp: new Date().toISOString() }
        });
        document.dispatchEvent(event);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize the chat interface when the page loads
window.addEventListener('DOMContentLoaded', () => {
    window.ide = window.ide || {};
    window.ide.chat = new IDEChatInterface({
        apiBase: 'http://localhost:8080',
        wsBase: 'ws://localhost:8080'
    });
});