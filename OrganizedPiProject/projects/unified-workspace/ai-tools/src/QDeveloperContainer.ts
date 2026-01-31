import * as vscode from 'vscode';
import { createProvider, setProvider } from './ai/provider';

export class QDeveloperContainer {
  private panel: vscode.WebviewPanel | undefined;
  private context: vscode.ExtensionContext;

  constructor(context: vscode.ExtensionContext) {
    this.context = context;
  }

  async activate(): Promise<void> {
    this.panel = vscode.window.createWebviewPanel(
      'qDeveloper',
      'Q Developer Pro',
      vscode.ViewColumn.Beside,
      {
        enableScripts: true,
        retainContextWhenHidden: true,
        localResourceRoots: [this.context.extensionUri]
      }
    );

    this.panel.webview.html = this.getWebviewContent();
    this.setupMessageHandling();
  }

  private getWebviewContent(): string {
    return `
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 0; padding: 20px; background: #1e1e1e; color: #fff; }
          .container { max-width: 800px; margin: 0 auto; }
          .header { text-align: center; margin-bottom: 30px; }
          .chat-container { height: 500px; border: 1px solid #333; border-radius: 8px; overflow-y: auto; padding: 15px; background: #252526; margin-bottom: 15px; }
          .message { margin: 10px 0; padding: 10px; border-radius: 8px; }
          .user { background: #0078d4; text-align: right; }
          .assistant { background: #2d2d30; }
          .input-area { display: flex; gap: 10px; }
          #messageInput { flex: 1; padding: 12px; border: 1px solid #333; border-radius: 6px; background: #3c3c3c; color: #fff; }
          button { padding: 12px 20px; background: #0078d4; color: white; border: none; border-radius: 6px; cursor: pointer; }
          button:hover { background: #106ebe; }
          .features { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-top: 20px; }
          .feature { background: #2d2d30; padding: 15px; border-radius: 8px; border: 1px solid #333; }
          .status { background: #0e7a0d; padding: 8px; border-radius: 4px; text-align: center; margin-bottom: 20px; }
        </style>
      </head>
      <body>
        <div class="container">
          <div class="header">
            <h1>🤖 Q Developer Pro Container</h1>
            <div class="status">✅ Unlimited Conversations Active</div>
          </div>
          
          <div class="chat-container" id="chatContainer"></div>
          
          <div class="input-area">
            <input type="text" id="messageInput" placeholder="Ask Q Developer anything..." />
            <button onclick="sendMessage()">Send</button>
            <button onclick="clearChat()">Clear</button>
          </div>
          
          <div class="features">
            <div class="feature">
              <h3>🚀 Code Generation</h3>
              <p>Generate complete functions, classes, and applications</p>
            </div>
            <div class="feature">
              <h3>🔍 Code Review</h3>
              <p>Analyze code for bugs, performance, and best practices</p>
            </div>
            <div class="feature">
              <h3>🛠️ Debugging</h3>
              <p>Find and fix issues in your code</p>
            </div>
            <div class="feature">
              <h3>📚 Documentation</h3>
              <p>Generate comprehensive documentation</p>
            </div>
          </div>
        </div>

        <script>
          const vscode = acquireVsCodeApi();
          
          function sendMessage() {
            const input = document.getElementById('messageInput');
            const message = input.value.trim();
            if (message) {
              addMessage('user', message);
              vscode.postMessage({ type: 'chat', message });
              input.value = '';
            }
          }
          
          function addMessage(sender, text) {
            const container = document.getElementById('chatContainer');
            const div = document.createElement('div');
            div.className = 'message ' + sender;
            div.innerHTML = text.replace(/\\n/g, '<br>');
            container.appendChild(div);
            container.scrollTop = container.scrollHeight;
          }
          
          function clearChat() {
            document.getElementById('chatContainer').innerHTML = '';
          }
          
          window.addEventListener('message', event => {
            if (event.data.type === 'response') {
              addMessage('assistant', event.data.message);
            }
          });
          
          document.getElementById('messageInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') sendMessage();
          });
          
          // Welcome message
          addMessage('assistant', 'Hello! I\\'m your Q Developer Pro assistant. I have unlimited conversations and enhanced capabilities. How can I help you today?');
        </script>
      </body>
      </html>`;
  }

  private setupMessageHandling(): void {
    if (!this.panel) return;

    this.panel.webview.onDidReceiveMessage(async (message) => {
      if (message.type === 'chat') {
        try {
          setProvider('openai'); // Use your preferred provider
          const provider = createProvider();
          const response = await provider.respond({
            messages: [{ role: 'user', content: message.message }]
          });
          
          this.panel?.webview.postMessage({
            type: 'response',
            message: response
          });
        } catch (error) {
          this.panel?.webview.postMessage({
            type: 'response',
            message: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`
          });
        }
      }
    });
  }
}