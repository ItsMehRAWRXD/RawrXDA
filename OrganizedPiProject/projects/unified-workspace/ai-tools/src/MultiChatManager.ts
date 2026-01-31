import * as vscode from 'vscode';

export class MultiChatManager {
  private chatPanels: Map<string, vscode.WebviewPanel> = new Map();
  private chatCounter = 0;

  createNewChat(): string {
    this.chatCounter++;
    const chatId = `chat-${this.chatCounter}`;
    
    const panel = vscode.window.createWebviewPanel(
      chatId,
      `AI Chat ${this.chatCounter}`,
      vscode.ViewColumn.One,
      { enableScripts: true, retainContextWhenHidden: true }
    );

    panel.webview.html = this.getChatHTML(chatId);
    
    panel.onDidDispose(() => {
      this.chatPanels.delete(chatId);
    });

    this.chatPanels.set(chatId, panel);
    return chatId;
  }

  private getChatHTML(chatId: string): string {
    return `
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { font-family: Arial; margin: 0; padding: 20px; }
          #messages { height: 400px; overflow-y: auto; border: 1px solid #ccc; padding: 10px; margin-bottom: 10px; }
          #input { width: 80%; padding: 10px; }
          button { padding: 10px 20px; }
          .message { margin: 10px 0; padding: 10px; border-radius: 5px; }
          .user { background: #e3f2fd; text-align: right; }
          .ai { background: #f3e5f5; }
        </style>
      </head>
      <body>
        <h3>AI Chat ${chatId}</h3>
        <div id="messages"></div>
        <input type="text" id="input" placeholder="Type your message...">
        <button onclick="sendMessage()">Send</button>
        <script>
          const vscode = acquireVsCodeApi();
          function sendMessage() {
            const input = document.getElementById('input');
            const message = input.value.trim();
            if (message) {
              addMessage('user', message);
              vscode.postMessage({ type: 'chat', message, chatId: '${chatId}' });
              input.value = '';
            }
          }
          function addMessage(sender, text) {
            const messages = document.getElementById('messages');
            const div = document.createElement('div');
            div.className = 'message ' + sender;
            div.textContent = text;
            messages.appendChild(div);
            messages.scrollTop = messages.scrollHeight;
          }
          window.addEventListener('message', event => {
            if (event.data.type === 'response') {
              addMessage('ai', event.data.message);
            }
          });
          document.getElementById('input').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') sendMessage();
          });
        </script>
      </body>
      </html>`;
  }

  sendToChat(chatId: string, message: string): void {
    const panel = this.chatPanels.get(chatId);
    if (panel) {
      panel.webview.postMessage({ type: 'response', message });
    }
  }

  getAllChats(): string[] {
    return Array.from(this.chatPanels.keys());
  }
}