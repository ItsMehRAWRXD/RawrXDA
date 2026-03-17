import * as vscode from 'vscode';
import { AICopilotProvider } from '../ai/AICopilotProvider';
import { Logger } from '../utils/Logger';

export class ChatPanel {
  private logger: Logger;
  private context: vscode.ExtensionContext;
  private aiProvider: AICopilotProvider;
  private panel: vscode.WebviewPanel | undefined;
  private conversationHistory: Array<{ role: string; content: string }> = [];

  constructor(context: vscode.ExtensionContext, aiProvider: AICopilotProvider) {
    this.logger = new Logger('ChatPanel');
    this.context = context;
    this.aiProvider = aiProvider;
  }

  show(): void {
    try {
      if (this.panel) {
        this.panel.reveal(vscode.ViewColumn.Beside);
        return;
      }

      this.panel = vscode.window.createWebviewPanel(
        'cursorAiChat',
        'Cursor AI Chat',
        vscode.ViewColumn.Beside,
        {
          enableScripts: true,
          retainContextWhenHidden: true
        }
      );

      this.panel.webview.html = this.getWebviewContent();

      this.panel.webview.onDidReceiveMessage(
        message => this.handleMessage(message),
        undefined,
        this.context.subscriptions
      );

      this.panel.onDidDispose(
        () => {
          this.panel = undefined;
        },
        undefined,
        this.context.subscriptions
      );

      this.logger.debug('Chat panel opened');
    } catch (error) {
      this.logger.error('Error opening chat panel:', error);
      vscode.window.showErrorMessage('Failed to open chat panel');
    }
  }

  showWithMessage(message: string): void {
    this.show();
    if (this.panel) {
      this.panel.webview.postMessage({ command: 'insertMessage', text: message });
    }
  }

  private async handleMessage(message: any): Promise<void> {
    try {
      switch (message.command) {
        case 'sendMessage':
          await this.processMessage(message.text);
          break;
        case 'clearChat':
          this.conversationHistory = [];
          break;
      }
    } catch (error) {
      this.logger.error('Error handling message:', error);
    }
  }

  private async processMessage(userMessage: string): Promise<void> {
    try {
      this.conversationHistory.push({
        role: 'user',
        content: userMessage
      });

      this.panel?.webview.postMessage({
        command: 'showTyping'
      });

      const response = await this.aiProvider.chat(this.conversationHistory);

      this.conversationHistory.push({
        role: 'assistant',
        content: response
      });

      this.panel?.webview.postMessage({
        command: 'addMessage',
        role: 'assistant',
        text: response
      });
    } catch (error) {
      this.logger.error('Error processing message:', error);
      this.panel?.webview.postMessage({
        command: 'addMessage',
        role: 'error',
        text: `Error: ${error}`
      });
    }
  }

  private getWebviewContent(): string {
    return `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cursor AI Chat</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
            display: flex;
            flex-direction: column;
            height: 100vh;
            overflow: hidden;
        }

        .container {
            display: flex;
            flex-direction: column;
            height: 100%;
        }

        .header {
            padding: 12px 16px;
            background: var(--vscode-titleBar-activeBackground);
            border-bottom: 1px solid var(--vscode-titleBar-border);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .header h1 {
            font-size: 14px;
            font-weight: 600;
        }

        .header-buttons button {
            background: none;
            border: none;
            color: var(--vscode-editor-foreground);
            cursor: pointer;
            padding: 4px 8px;
            border-radius: 4px;
            transition: background 0.2s;
        }

        .header-buttons button:hover {
            background: var(--vscode-button-hoverBackground);
        }

        #chatMessages {
            flex: 1;
            overflow-y: auto;
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .message {
            display: flex;
            margin-bottom: 8px;
            animation: slideIn 0.3s ease-out;
        }

        @keyframes slideIn {
            from {
                opacity: 0;
                transform: translateY(10px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .message.user {
            justify-content: flex-end;
        }

        .message.assistant {
            justify-content: flex-start;
        }

        .message.error {
            justify-content: flex-start;
        }

        .message-content {
            max-width: 80%;
            padding: 8px 12px;
            border-radius: 6px;
            word-wrap: break-word;
            overflow-x: auto;
        }

        .message.user .message-content {
            background: var(--vscode-inputOption-activeBorder);
            color: var(--vscode-editor-foreground);
        }

        .message.assistant .message-content {
            background: var(--vscode-editor-inlineValuesBorder);
            color: var(--vscode-editor-foreground);
        }

        .message.error .message-content {
            background: var(--vscode-errorForeground);
            color: white;
        }

        .typing {
            display: flex;
            gap: 4px;
            align-items: center;
        }

        .typing span {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--vscode-editor-foreground);
            animation: bounce 1.4s infinite;
        }

        .typing span:nth-child(2) {
            animation-delay: 0.2s;
        }

        .typing span:nth-child(3) {
            animation-delay: 0.4s;
        }

        @keyframes bounce {
            0%, 80%, 100% {
                opacity: 0.5;
                transform: translateY(0);
            }
            40% {
                opacity: 1;
                transform: translateY(-8px);
            }
        }

        .input-area {
            padding: 12px 16px;
            border-top: 1px solid var(--vscode-editor-lineHighlightBorder);
            display: flex;
            gap: 8px;
        }

        #messageInput {
            flex: 1;
            background: var(--vscode-input-background);
            border: 1px solid var(--vscode-input-border);
            color: var(--vscode-input-foreground);
            padding: 8px 12px;
            border-radius: 4px;
            font-family: inherit;
            font-size: 13px;
        }

        #messageInput:focus {
            outline: none;
            border-color: var(--vscode-focusBorder);
        }

        button {
            background: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 13px;
            transition: background 0.2s;
        }

        button:hover {
            background: var(--vscode-button-hoverBackground);
        }

        code {
            background: var(--vscode-textCodeBlock-background);
            padding: 2px 4px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
        }

        pre {
            background: var(--vscode-textCodeBlock-background);
            padding: 8px;
            border-radius: 4px;
            overflow-x: auto;
            margin: 4px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Cursor AI Chat</h1>
            <div class="header-buttons">
                <button onclick="clearChat()" title="Clear chat">🗑️</button>
            </div>
        </div>
        
        <div id="chatMessages"></div>
        
        <div class="input-area">
            <input 
                type="text" 
                id="messageInput" 
                placeholder="Ask me anything..."
                onkeypress="if(event.key==='Enter' && !event.shiftKey) sendMessage(); return event.key!=='Enter';"
            />
            <button onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const vscode = acquireVsCodeApi();
        
        function sendMessage() {
            const input = document.getElementById('messageInput');
            const text = input.value.trim();
            
            if (!text) return;
            
            addMessage('user', text);
            vscode.postMessage({ command: 'sendMessage', text });
            input.value = '';
            input.focus();
        }

        function addMessage(role, text) {
            const messagesDiv = document.getElementById('chatMessages');
            const messageDiv = document.createElement('div');
            messageDiv.className = \`message \${role}\`;
            
            const contentDiv = document.createElement('div');
            contentDiv.className = 'message-content';
            contentDiv.innerHTML = text.replace(/\\n/g, '<br>');
            
            messageDiv.appendChild(contentDiv);
            messagesDiv.appendChild(messageDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        function showTyping() {
            const messagesDiv = document.getElementById('chatMessages');
            const typingDiv = document.createElement('div');
            typingDiv.className = 'message assistant';
            typingDiv.id = 'typingIndicator';
            
            const contentDiv = document.createElement('div');
            contentDiv.className = 'message-content typing';
            contentDiv.innerHTML = '<span></span><span></span><span></span>';
            
            typingDiv.appendChild(contentDiv);
            messagesDiv.appendChild(typingDiv);
            messagesDiv.scrollTop = messagesDiv.scrollHeight;
        }

        function clearChat() {
            document.getElementById('chatMessages').innerHTML = '';
            vscode.postMessage({ command: 'clearChat' });
        }

        window.addEventListener('message', event => {
            const message = event.data;
            
            switch (message.command) {
                case 'addMessage':
                    const typingDiv = document.getElementById('typingIndicator');
                    if (typingDiv) typingDiv.remove();
                    addMessage(message.role, message.text);
                    break;
                    
                case 'showTyping':
                    showTyping();
                    break;
                    
                case 'insertMessage':
                    const input = document.getElementById('messageInput');
                    input.value = message.text;
                    input.focus();
                    break;
            }
        });

        // Welcome message
        window.addEventListener('load', () => {
            addMessage('assistant', 'Hello! I\\'m Cursor AI Copilot. How can I help you today? 👋');
        });
    </script>
</body>
</html>`;
  }
}
