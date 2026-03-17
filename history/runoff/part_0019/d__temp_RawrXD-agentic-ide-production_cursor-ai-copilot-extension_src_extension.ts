import * as vscode from 'vscode';
import { AICopilotProvider } from './ai/AICopilotProvider';
import { WebAuthManager } from './auth/WebAuthManager';
import { ChatPanel } from './ui/ChatPanel';
import { CodeAnalyzer } from './analysis/CodeAnalyzer';
import { CompletionProvider } from './completion/CompletionProvider';
import { Telemetry } from './telemetry/Telemetry';
import { Logger } from './utils/Logger';
import { OpenAIModelsManager } from './ai/openai-models';
import { OpenAIAPIClient } from './ai/openai-client';

let logger: Logger;
let telemetry: Telemetry;

export async function activate(context: vscode.ExtensionContext) {
  logger = new Logger('Cursor AI Copilot');
  logger.info('Extension activation started');

  try {
    // Initialize telemetry first
    telemetry = new Telemetry(context);
    context.subscriptions.push(telemetry);

    // Initialize components
    const webAuthManager = new WebAuthManager(context);
    const aiProvider = new AICopilotProvider(context, webAuthManager);
    const chatPanel = new ChatPanel(context, aiProvider);
    const codeAnalyzer = new CodeAnalyzer(context, aiProvider);
    const completionProvider = new CompletionProvider(context, aiProvider);

    // Register commands
    const commands = [
      vscode.commands.registerCommand('cursor-ai-copilot.activate', () => {
        telemetry.trackCommand('activate');
        logger.info('AI Copilot activated');
        vscode.window.showInformationMessage('Cursor AI Copilot activated! Use Ctrl+Shift+A to open chat.');
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.chat', () => {
        telemetry.trackCommand('chat');
        logger.debug('Opening chat panel');
        chatPanel.show();
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.explain', async () => {
        telemetry.trackCommand('explain');
        logger.debug('Explain command triggered');

        const editor = vscode.window.activeTextEditor;
        if (!editor) {
          vscode.window.showWarningMessage('No active editor found.');
          return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);

        if (!selectedText.trim()) {
          vscode.window.showWarningMessage('Please select some code to explain.');
          return;
        }

        try {
          await vscode.window.withProgress(
            {
              location: vscode.ProgressLocation.Notification,
              title: 'Explaining code...',
              cancellable: true
            },
            async (progress, token) => {
              const explanation = await aiProvider.explainCode(selectedText, editor.document.languageId);
              telemetry.trackFeatureUsage('explain', selectedText.length);
              chatPanel.showWithMessage(`Explain this code:\n\n\`\`\`${editor.document.languageId}\n${selectedText}\n\`\`\`\n\n${explanation}`);
            }
          );
        } catch (error) {
          logger.error('Explain failed:', error);
          vscode.window.showErrorMessage(`Failed to explain code: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.refactor', async () => {
        telemetry.trackCommand('refactor');
        logger.debug('Refactor command triggered');

        const editor = vscode.window.activeTextEditor;
        if (!editor) {
          vscode.window.showWarningMessage('No active editor found.');
          return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);

        if (!selectedText.trim()) {
          vscode.window.showWarningMessage('Please select some code to refactor.');
          return;
        }

        try {
          await vscode.window.withProgress(
            {
              location: vscode.ProgressLocation.Notification,
              title: 'Refactoring code...',
              cancellable: true
            },
            async (progress, token) => {
              const refactoredCode = await aiProvider.refactorCode(selectedText, editor.document.languageId);
              telemetry.trackFeatureUsage('refactor', selectedText.length);
              chatPanel.showWithMessage(`Refactor this code:\n\n\`\`\`${editor.document.languageId}\n${selectedText}\n\`\`\`\n\nRefactored:\n\n\`\`\`${editor.document.languageId}\n${refactoredCode}\n\`\`\``);
            }
          );
        } catch (error) {
          logger.error('Refactor failed:', error);
          vscode.window.showErrorMessage(`Failed to refactor code: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.generate', async () => {
        telemetry.trackCommand('generate');
        logger.debug('Generate command triggered');

        const editor = vscode.window.activeTextEditor;
        if (!editor) {
          vscode.window.showWarningMessage('No active editor found.');
          return;
        }

        const prompt = await vscode.window.showInputBox({
          prompt: 'What code would you like to generate?',
          placeHolder: 'e.g., "Create a React component for a todo list"'
        });

        if (!prompt) return;

        try {
          await vscode.window.withProgress(
            {
              location: vscode.ProgressLocation.Notification,
              title: 'Generating code...',
              cancellable: true
            },
            async (progress, token) => {
              const generatedCode = await aiProvider.generateCode(prompt, editor.document.languageId);
              telemetry.trackFeatureUsage('generate', prompt.length);
              chatPanel.showWithMessage(`Generate: ${prompt}\n\n\`\`\`${editor.document.languageId}\n${generatedCode}\n\`\`\``);
            }
          );
        } catch (error) {
          logger.error('Generate failed:', error);
          vscode.window.showErrorMessage(`Failed to generate code: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.optimize', async () => {
        telemetry.trackCommand('optimize');
        logger.debug('Optimize command triggered');

        const editor = vscode.window.activeTextEditor;
        if (!editor) {
          vscode.window.showWarningMessage('No active editor found.');
          return;
        }

        const selection = editor.selection;
        const selectedText = editor.document.getText(selection);

        if (!selectedText.trim()) {
          vscode.window.showWarningMessage('Please select some code to optimize.');
          return;
        }

        try {
          await vscode.window.withProgress(
            {
              location: vscode.ProgressLocation.Notification,
              title: 'Optimizing code...',
              cancellable: true
            },
            async (progress, token) => {
              const optimizedCode = await aiProvider.optimizeCode(selectedText, editor.document.languageId);
              telemetry.trackFeatureUsage('optimize', selectedText.length);
              chatPanel.showWithMessage(`Optimize this code:\n\n\`\`\`${editor.document.languageId}\n${selectedText}\n\`\`\`\n\nOptimized:\n\n\`\`\`${editor.document.languageId}\n${optimizedCode}\n\`\`\``);
            }
          );
        } catch (error) {
          logger.error('Optimize failed:', error);
          vscode.window.showErrorMessage(`Failed to optimize code: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.authenticate', async () => {
        telemetry.trackCommand('authenticate');
        logger.debug('Authentication command triggered');

        try {
          await webAuthManager.authenticate();
          vscode.window.showInformationMessage('Successfully authenticated with ChatGPT!');
          logger.info('Authentication successful');
        } catch (error) {
          logger.error('Authentication failed:', error);
          vscode.window.showErrorMessage(`Authentication failed: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.logout', async () => {
        telemetry.trackCommand('logout');
        logger.debug('Logout command triggered');

        try {
          await webAuthManager.logout();
          vscode.window.showInformationMessage('Logged out successfully.');
          logger.info('User logged out');
        } catch (error) {
          logger.error('Logout failed:', error);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.selectModel', async () => {
        telemetry.trackCommand('selectModel');
        logger.debug('Model selection command triggered');

        try {
          const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
          const selectionMode = config.get<string>('modelSelectionMode', 'recommended');

          let availableModels: any[] = [];

          switch (selectionMode) {
            case 'all':
              availableModels = OpenAIModelsManager.getAllModels();
              break;
            case 'chat':
              availableModels = OpenAIModelsManager.getChatModels();
              break;
            case 'audio':
              availableModels = OpenAIModelsManager.getAudioModels();
              break;
            case 'vision':
              availableModels = OpenAIModelsManager.getVisionModels();
              break;
            case 'embedding':
              availableModels = OpenAIModelsManager.getEmbeddingModels();
              break;
            default:
              availableModels = OpenAIModelsManager.getRecommendedModels();
          }

          const items = availableModels.map(model => ({
            label: model.id,
            description: OpenAIModelsManager.getModelCapabilities(model.id).join(', '),
            detail: `Owner: ${model.owned_by} | Created: ${new Date(model.created * 1000).toLocaleDateString()}`,
            model
          }));

          const selected = await vscode.window.showQuickPick(items, {
            placeHolder: 'Select an OpenAI model',
            matchOnDescription: true,
            matchOnDetail: true
          });

          if (selected) {
            await config.update('model', selected.model.id, vscode.ConfigurationTarget.Global);
            vscode.window.showInformationMessage(`Model switched to: ${selected.model.id}`);
            telemetry.trackFeatureUsage('model-selection', selected.model.id);
            logger.info(`Model changed to: ${selected.model.id}`);
          }
        } catch (error) {
          logger.error('Model selection failed:', error);
          vscode.window.showErrorMessage(`Failed to select model: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.listModels', async () => {
        telemetry.trackCommand('listModels');
        logger.debug('List models command triggered');

        try {
          const apiClient = new OpenAIAPIClient();
          const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
          const apiKey = config.get<string>('apiKey');

          if (!apiKey) {
            vscode.window.showErrorMessage('OpenAI API key not configured. Please set it in extension settings.');
            return;
          }

          await apiClient.initialize(apiKey);
          const models = await apiClient.listModels();

          if (models.length === 0) {
            vscode.window.showInformationMessage('No models available from OpenAI API.');
            return;
          }

          const modelList = models
            .map(m => `• ${m.id} (Owner: ${m.owned_by})`)
            .join('\n');

          const document = await vscode.workspace.openTextDocument({
            content: `# Available OpenAI Models (${models.length} total)\n\n${modelList}`,
            language: 'markdown'
          });

          await vscode.window.showTextDocument(document);
          telemetry.trackFeatureUsage('list-models', models.length);
          logger.info(`Listed ${models.length} available models`);
        } catch (error) {
          logger.error('List models failed:', error);
          vscode.window.showErrorMessage(`Failed to list models: ${error}`);
        }
      }),

      vscode.commands.registerCommand('cursor-ai-copilot.verifyAPIKey', async () => {
        telemetry.trackCommand('verifyAPIKey');
        logger.debug('API key verification triggered');

        try {
          const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
          const apiKey = config.get<string>('apiKey');

          if (!apiKey) {
            vscode.window.showWarningMessage('No OpenAI API key configured.');
            return;
          }

          const apiClient = new OpenAIAPIClient();
          const isValid = await apiClient.initialize(apiKey);

          if (isValid) {
            vscode.window.showInformationMessage('✓ OpenAI API key is valid!');
            logger.info('API key verified successfully');
          } else {
            vscode.window.showErrorMessage('✗ OpenAI API key is invalid or expired.');
            logger.warn('API key verification failed');
          }

          telemetry.trackCommand('apiKeyVerified');
        } catch (error) {
          logger.error('API key verification failed:', error);
          vscode.window.showErrorMessage(`Failed to verify API key: ${error}`);
        }
      })

    ];

    // Register completion provider
    const completionDisposable = vscode.languages.registerCompletionItemProvider(
      { scheme: 'file' },
      completionProvider,
      '.', ' ', '\n', '\t'
    );

    // Register hover provider for code analysis
    const hoverDisposable = vscode.languages.registerHoverProvider(
      { scheme: 'file' },
      {
        async provideHover(document, position, token) {
          try {
            const range = document.getWordRangeAtPosition(position);
            if (!range) return null;

            const word = document.getText(range);
            const analysis = await codeAnalyzer.analyzeWord(word, document, position);

            if (analysis) {
              telemetry.trackFeatureUsage('hover-analysis', word.length);
              return new vscode.Hover(analysis);
            }
            return null;
          } catch (error) {
            logger.error('Hover analysis failed:', error);
            return null;
          }
        }
      }
    );

    // Create status bar item
    const statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.text = '$(comment-discussion) AI Copilot';
    statusBarItem.command = 'cursor-ai-copilot.chat';
    statusBarItem.tooltip = 'Click to open AI Chat (Ctrl+Shift+A)';
    statusBarItem.show();

    // Add all disposables to context
    context.subscriptions.push(...commands, completionDisposable, hoverDisposable, statusBarItem);

    // Show welcome message on first activation
    const isFirstActivation = !context.globalState.get('cursor-ai-copilot.activated');
    if (isFirstActivation) {
      context.globalState.update('cursor-ai-copilot.activated', true);

      const selection = await vscode.window.showInformationMessage(
        'Cursor AI Copilot is ready! Use Ctrl+Shift+A to open the AI chat.',
        'Open Chat',
        'Authenticate',
        'Learn More'
      );

      if (selection === 'Open Chat') {
        chatPanel.show();
      } else if (selection === 'Authenticate') {
        vscode.commands.executeCommand('cursor-ai-copilot.authenticate');
      } else if (selection === 'Learn More') {
        vscode.env.openExternal(vscode.Uri.parse('https://github.com/RawrXD/cursor-ai-copilot'));
      }

      logger.info('First activation welcome message displayed');
    }

    logger.info('Extension activated successfully');
    telemetry.trackCommand('extension-activated');
  } catch (error) {
    logger.error('Extension activation failed:', error);
    vscode.window.showErrorMessage(`Failed to activate Cursor AI Copilot: ${error}`);
  }
}

export function deactivate() {
  logger.info('Extension deactivated');
  telemetry.trackCommand('extension-deactivated');
}
