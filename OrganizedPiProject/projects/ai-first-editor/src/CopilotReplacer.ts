import * as vscode from 'vscode';
import { MultiChatManager } from './MultiChatManager';
import { createProvider, setProvider } from './ai/provider';

export class CopilotReplacer {
  private multiChat: MultiChatManager;
  
  constructor() {
    this.multiChat = new MultiChatManager();
  }

  async replaceCopilot(): Promise<void> {
    // Disable existing Copilot
    await this.disableExistingCopilot();
    
    // Start our enhanced AI system
    const chatId = this.multiChat.createNewChat();
    
    // Register as the new AI assistant
    this.registerAsDefaultAI();
    
    vscode.window.showInformationMessage('Enhanced AI Copilot activated! Old Copilot disabled.');
  }

  private async disableExistingCopilot(): Promise<void> {
    const config = vscode.workspace.getConfiguration();
    
    // Disable GitHub Copilot
    await config.update('github.copilot.enable', false, vscode.ConfigurationTarget.Global);
    
    // Disable other AI extensions
    const extensions = vscode.extensions.all;
    for (const ext of extensions) {
      if (ext.id.includes('copilot') || ext.id.includes('ai-assistant')) {
        try {
          await vscode.commands.executeCommand('workbench.extensions.action.disableGlobally', ext.id);
        } catch (e) {
          // Continue if can't disable
        }
      }
    }
  }

  private registerAsDefaultAI(): void {
    // Register completion provider
    vscode.languages.registerCompletionItemProvider('*', {
      provideCompletionItems: async (document, position) => {
        const line = document.lineAt(position).text;
        const prefix = line.substring(0, position.character);
        
        if (prefix.trim().length < 2) return [];
        
        try {
          setProvider('openai');
          const provider = createProvider();
          const completion = await provider.respond({
            messages: [{ role: 'user', content: `Complete this code: ${prefix}` }]
          });
          
          const item = new vscode.CompletionItem(completion, vscode.CompletionItemKind.Text);
          item.insertText = completion;
          return [item];
        } catch (e) {
          return [];
        }
      }
    });

    // Register hover provider
    vscode.languages.registerHoverProvider('*', {
      provideHover: async (document, position) => {
        const range = document.getWordRangeAtPosition(position);
        if (!range) return;
        
        const word = document.getText(range);
        return new vscode.Hover(`Enhanced AI: ${word}`);
      }
    });
  }
}