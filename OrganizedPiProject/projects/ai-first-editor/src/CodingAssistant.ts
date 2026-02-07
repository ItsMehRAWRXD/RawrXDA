import * as vscode from 'vscode';
import { AgentFinder, CodingAgent } from './AgentFinder';
import { createProvider, setProvider } from './ai/provider';

export class CodingAssistant {
  private agentFinder = new AgentFinder();
  private activeAgents: Map<string, CodingAgent> = new Map();

  async findAndHelpAgents(): Promise<void> {
    const panel = vscode.window.createWebviewPanel(
      'codingAssistant',
      'AI Coding Agents Helper',
      vscode.ViewColumn.One,
      { enableScripts: true }
    );

    const agents = await this.agentFinder.findCodingAgents();
    
    panel.webview.html = `
      <!DOCTYPE html>
      <html>
      <head>
        <style>
          body { font-family: Arial; padding: 20px; background: #1e1e1e; color: #fff; }
          .agent { background: #2d2d30; padding: 15px; margin: 10px 0; border-radius: 8px; border: 1px solid #333; }
          .skills { color: #4fc3f7; }
          button { background: #0078d4; color: white; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; margin: 5px; }
          button:hover { background: #106ebe; }
          .help-section { background: #0e2f44; padding: 15px; border-radius: 8px; margin-top: 20px; }
        </style>
      </head>
      <body>
        <h1>🤖 AI Coding Agents Found</h1>
        ${agents.map(agent => `
          <div class="agent">
            <h3>${agent.name}</h3>
            <p>${agent.description}</p>
            <div class="skills">Skills: ${agent.skills.join(', ')}</div>
            <button onclick="helpAgent('${agent.name}', 'debug code')">Help Debug</button>
            <button onclick="helpAgent('${agent.name}', 'write tests')">Help Test</button>
            <button onclick="helpAgent('${agent.name}', 'optimize performance')">Help Optimize</button>
            <button onclick="helpAgent('${agent.name}', 'code review')">Help Review</button>
          </div>
        `).join('')}
        
        <div class="help-section">
          <h3>🛠️ Agent Assistance Log</h3>
          <div id="helpLog"></div>
        </div>

        <script>
          const vscode = acquireVsCodeApi();
          
          function helpAgent(agentName, task) {
            vscode.postMessage({ type: 'helpAgent', agentName, task });
          }
          
          window.addEventListener('message', event => {
            if (event.data.type === 'helpResult') {
              const log = document.getElementById('helpLog');
              log.innerHTML += '<div><strong>' + event.data.agent + ':</strong> ' + event.data.help + '</div>';
            }
          });
        </script>
      </body>
      </html>`;

    panel.webview.onDidReceiveMessage(async (message) => {
      if (message.type === 'helpAgent') {
        const agent = agents.find(a => a.name === message.agentName);
        if (agent) {
          const help = await this.agentFinder.helpAgentWithCoding(agent, message.task);
          panel.webview.postMessage({
            type: 'helpResult',
            agent: agent.name,
            help: help
          });
        }
      }
    });
  }

  async generateCodeForAgent(agentName: string, requirement: string): Promise<string> {
    setProvider('openai');
    const provider = createProvider();
    
    const prompt = `Generate code for AI agent "${agentName}" with requirement: ${requirement}
    Include error handling, documentation, and best practices.`;
    
    return await provider.respond({
      messages: [{ role: 'user', content: prompt }]
    });
  }

  async reviewAgentCode(agentName: string, code: string): Promise<string> {
    setProvider('anthropic');
    const provider = createProvider();
    
    const prompt = `Review this code for AI agent "${agentName}":
    ${code}
    
    Provide feedback on:
    - Code quality and structure
    - Performance optimizations
    - Security considerations
    - Best practices compliance`;
    
    return await provider.respond({
      messages: [{ role: 'user', content: prompt }]
    });
  }
}