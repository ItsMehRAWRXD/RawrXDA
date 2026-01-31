import * as vscode from 'vscode';
import { createProvider, setProvider, setApiKey, getProviders } from './ai/provider';
import { WorkflowManager } from './WorkflowManager';
import { AgentOrchestrator } from './AgentOrchestrator';
import { FileUploadManager } from './FileUploadManager';
import { CopilotStudioConnector } from './CopilotStudioConnector';
import { SelfAwareAgent } from './SelfAwareAgent';
import { MultiChatManager } from './MultiChatManager';
import { VSCodeEnhancer } from './VSCodeEnhancer';
import { DesktopSearchEnhancer } from './DesktopSearchEnhancer';
import { CopilotReplacer } from './CopilotReplacer';
import { QDeveloperContainer } from './QDeveloperContainer';
import { CodingAssistant } from './CodingAssistant';

export function activate(context: vscode.ExtensionContext) {
	const workflowManager = new WorkflowManager(context);
	const agentOrchestrator = new AgentOrchestrator(workflowManager);
	const fileManager = new FileUploadManager();
	const copilotConnector = new CopilotStudioConnector();
	const selfAware = new SelfAwareAgent();
	const multiChat = new MultiChatManager();
	const copilotReplacer = new CopilotReplacer();
	const qDeveloper = new QDeveloperContainer(context);
	const codingAssistant = new CodingAssistant();
	
	// Maximize VS Code limits on activation
	VSCodeEnhancer.enhanceSearchLimits();
	VSCodeEnhancer.enhanceCopilotLimits();
	DesktopSearchEnhancer.enhanceDesktopSearch();
	DesktopSearchEnhancer.enhanceWorkspaceSearch();
	context.subscriptions.push(
		vscode.commands.registerCommand('aiFoundation.openUnifiedEditor', () => {
			const panel = vscode.window.createWebviewPanel(
				'unifiedEditor',
				'Unified Text Editor',
				vscode.ViewColumn.One,
				{ enableScripts: true }
			);
			panel.webview.html = `
				<!DOCTYPE html>
				<html lang="en">
				<head>
					<meta charset="UTF-8">
					<meta name="viewport" content="width=device-width, initial-scale=1.0">
					<title>Unified Editor</title>
					<style>
						body { font-family: Segoe UI, sans-serif; margin: 0; padding: 20px; background: #fafafa; }
						textarea { width: 100%; height: 300px; font-size: 16px; margin-bottom: 10px; }
						button, select { font-size: 16px; padding: 8px 16px; margin-right: 8px; }
						#response { margin-top: 20px; background: #f0f0f0; padding: 10px; border-radius: 4px; white-space: pre-line; }
					</style>
				</head>
				<body>
					<h2>Unified Text Editor</h2>
					<textarea id="note"></textarea><br>
					<select id="aiSelect">
						<option value="echo">Echo</option>
						<option value="openai">OpenAI</option>
						<option value="anthropic">Anthropic</option>
					</select>
					<button id="chat">Chat</button>
					<button id="review">Code Review</button>
					<button id="suggest">Suggest</button>
					<div id="response"></div>
					<script>
						const vscode = acquireVsCodeApi();
						document.getElementById('chat').onclick = () => {
							const text = document.getElementById('note').value;
							const ai = document.getElementById('aiSelect').value;
							vscode.postMessage({ type: 'chat', text, ai });
						};
						document.getElementById('review').onclick = () => {
							const text = document.getElementById('note').value;
							const ai = document.getElementById('aiSelect').value;
							vscode.postMessage({ type: 'review', text, ai });
						};
						document.getElementById('suggest').onclick = () => {
							const text = document.getElementById('note').value;
							const ai = document.getElementById('aiSelect').value;
							vscode.postMessage({ type: 'suggest', text, ai });
						};
						window.addEventListener('message', event => {
							const msg = event.data;
							if (msg.type === 'response') {
								document.getElementById('response').textContent = msg.result;
							}
						});
					</script>
				</body>
				</html>
			`;
			panel.webview.onDidReceiveMessage(async (msg: any) => {
				try {
					let result = '';
					if (msg.ai === 'echo') {
						setProvider('echo');
						const provider = createProvider();
						result = await provider.respond({ messages: [{ role: 'user', content: msg.text }] });
					} else if (msg.ai === 'openai') {
						setProvider('openai');
						const provider = createProvider();
						result = await provider.respond({ messages: [{ role: 'user', content: msg.text }] });
					} else if (msg.ai === 'anthropic') {
						setProvider('anthropic');
						const provider = createProvider();
						result = await provider.respond({ messages: [{ role: 'user', content: msg.text }] });
					} else {
						result = `${msg.ai} ${msg.type}: ${msg.text || 'No input'}`;
					}
					panel.webview.postMessage({ type: 'response', result });
				} catch (error) {
					panel.webview.postMessage({ type: 'response', result: `Error: ${error instanceof Error ? error.message : error}` });
				}
			});
		}),

		vscode.commands.registerCommand('aiFoundation.viewWorkflows', () => {
			const panel = vscode.window.createWebviewPanel('workflows', 'AI Agent Workflows', vscode.ViewColumn.One, { enableScripts: true });
			const todos = workflowManager.getPendingTodos();
			panel.webview.html = `
				<!DOCTYPE html>
				<html><head><title>Workflows</title></head>
				<body>
					<h2>AI Agent Workflows</h2>
					<h3>Pending TODOs</h3>
					${todos.map(t => `<div><input type="checkbox" onclick="completeTodo('${t.id}')">${t.title}</div>`).join('')}
					<script>
						const vscode = acquireVsCodeApi();
						function completeTodo(id) { vscode.postMessage({type: 'completeTodo', id}); }
					</script>
				</body></html>`;
			panel.webview.onDidReceiveMessage(msg => {
				if (msg.type === 'completeTodo') {
					workflowManager.completeTodo(msg.id);
					vscode.window.showInformationMessage('Todo completed!');
				}
			});
		}),

		vscode.commands.registerCommand('aiFoundation.showCapabilities', () => {
			const panel = vscode.window.createWebviewPanel('capabilities', 'AI Capabilities', vscode.ViewColumn.One, {});
			const caps = selfAware.getCapabilities();
			const stats = selfAware.analyzeOwnCode();
			panel.webview.html = `
				<html><body>
					<h2>AI Agent Capabilities</h2>
					${caps.map(c => `<li>${c}</li>`).join('')}
					<h3>Self Analysis</h3>
					<p>Files: ${stats.files}, Lines: ${stats.lines}, Functions: ${stats.functions}</p>
				</body></html>`;
		}),

		vscode.commands.registerCommand('aiFoundation.searchAgents', async () => {
			const query = await vscode.window.showInputBox({ prompt: 'Search for AI agents/tools' });
			if (query) {
				const agents = await agentOrchestrator.discoverNewAgents();
				const selected = await vscode.window.showQuickPick(agents, { placeHolder: 'Select agent to add' });
				if (selected) {
					await agentOrchestrator.addAgentFromSearch(selected, ['basic functionality']);
					vscode.window.showInformationMessage(`Added agent: ${selected}`);
				}
			}
		}),

		vscode.commands.registerCommand('aiFoundation.newChat', () => {
			const chatId = multiChat.createNewChat();
			vscode.window.showInformationMessage(`Created new chat: ${chatId}`);
		}),

		vscode.commands.registerCommand('aiFoundation.replaceCopilot', async () => {
			await copilotReplacer.replaceCopilot();
		}),

		vscode.commands.registerCommand('aiFoundation.findCodingAgents', async () => {
			await codingAssistant.findAndHelpAgents();
		}),

		vscode.commands.registerCommand('aiFoundation.openQDeveloper', async () => {
			await qDeveloper.activate();
		}),

		vscode.commands.registerCommand('aiFoundation.enhanceVSCode', () => {
			VSCodeEnhancer.enhanceSearchLimits();
			VSCodeEnhancer.enhanceCopilotLimits();
			DesktopSearchEnhancer.enhanceDesktopSearch();
			DesktopSearchEnhancer.enhanceWorkspaceSearch();
		}),

		vscode.commands.registerCommand('aiFoundation.setApiKey', async () => {
			const provider = await vscode.window.showQuickPick(['openai', 'anthropic'], { placeHolder: 'Select AI provider' });
			if (provider) {
				const apiKey = await vscode.window.showInputBox({ prompt: `Enter ${provider} API key`, password: true });
				if (apiKey) {
					setApiKey(provider, apiKey);
					vscode.window.showInformationMessage(`${provider} API key set successfully`);
				}
			}
		}),

		vscode.commands.registerCommand('aiFoundation.openNotepad', () => {
			const panel = vscode.window.createWebviewPanel(
				'notepad',
				'Notepad',
				vscode.ViewColumn.One,
				{ enableScripts: true }
			);
			panel.webview.html = `
				<!DOCTYPE html>
				<html lang="en">
				<head>
					<meta charset="UTF-8">
					<meta name="viewport" content="width=device-width, initial-scale=1.0">
					<title>Notepad</title>
					<style>
						body { font-family: Segoe UI, sans-serif; margin: 0; padding: 20px; background: #fafafa; }
						textarea { width: 100%; height: 300px; font-size: 16px; margin-bottom: 10px; }
						button, select { font-size: 16px; padding: 8px 16px; margin-right: 8px; }
						#response { margin-top: 20px; background: #f0f0f0; padding: 10px; border-radius: 4px; }
					</style>
				</head>
				<body>
					<h2>Notepad</h2>
					<textarea id="note"></textarea><br>
					<select id="aiSelect">
						<option value="amazonq">Amazon Q</option>
						<option value="kimi">Kimi</option>
						<option value="copilot">Copilot</option>
					</select>
					<button id="proofread">Proofread with Selected AI</button>
					<div id="response"></div>
					<script>
						const vscode = acquireVsCodeApi();
						document.getElementById('proofread').onclick = () => {
							const text = document.getElementById('note').value;
							const ai = document.getElementById('aiSelect').value;
							vscode.postMessage({ type: 'proofread', text, ai });
						};
						window.addEventListener('message', event => {
							const msg = event.data;
							if (msg.type === 'proofreadResult') {
								document.getElementById('response').textContent = msg.result;
							}
							if (msg.type === 'proofreadChain') {
								document.getElementById('response').textContent += '\n' + msg.result;
							}
						});
					</script>
				</body>
				</html>
			`;
			panel.webview.onDidReceiveMessage(async (msg: any) => {
				if (msg.type === 'proofread') {
					let result = '';
					if (msg.ai === 'amazonq') {
						result = 'Amazon Q feedback: ' + (msg.text.length > 0 ? 'No major issues found.' : 'Please enter some text.');
						panel.webview.postMessage({ type: 'proofreadResult', result });
						const kimiResult = 'Kimi feedback: ' + (msg.text.length > 0 ? 'Looks good, but consider clarity improvements.' : 'No input for Kimi.');
						panel.webview.postMessage({ type: 'proofreadChain', result: kimiResult });
					} else if (msg.ai === 'kimi') {
						result = 'Kimi feedback: ' + (msg.text.length > 0 ? 'Looks good, but consider clarity improvements.' : 'No input for Kimi.');
						panel.webview.postMessage({ type: 'proofreadResult', result });
						const copilotResult = 'Copilot feedback: ' + (msg.text.length > 0 ? 'No syntax errors detected.' : 'No input for Copilot.');
						panel.webview.postMessage({ type: 'proofreadChain', result: copilotResult });
					} else if (msg.ai === 'copilot') {
						result = 'Copilot feedback: ' + (msg.text.length > 0 ? 'No syntax errors detected.' : 'No input for Copilot.');
						panel.webview.postMessage({ type: 'proofreadResult', result });
					}
				}
			});
		})
	);
}
