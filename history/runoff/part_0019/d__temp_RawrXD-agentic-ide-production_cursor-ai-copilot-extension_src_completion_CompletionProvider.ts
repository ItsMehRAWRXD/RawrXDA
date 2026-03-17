import * as vscode from 'vscode';
import { AICopilotProvider } from '../ai/AICopilotProvider';
import { Logger } from '../utils/Logger';

export class CompletionProvider implements vscode.CompletionItemProvider {
  private logger: Logger;
  private context: vscode.ExtensionContext;
  private aiProvider: AICopilotProvider;
  private lastCompletionTime = 0;
  private completionThrottle = 500; // ms
  private availableModels: string[] = [];

  constructor(context: vscode.ExtensionContext, aiProvider: AICopilotProvider) {
    this.logger = new Logger('CompletionProvider');
    this.context = context;
    this.aiProvider = aiProvider;
    this.initializeModels();
  }

  private async initializeModels(): Promise<void> {
    try {
      this.availableModels = await this.aiProvider.getAvailableModels();
      this.logger.info(`Initialized with ${this.availableModels.length} available models`);
    } catch (error) {
      this.logger.error('Failed to initialize models:', error);
    }
  }

  async provideCompletionItems(
    document: vscode.TextDocument,
    position: vscode.Position,
    token: vscode.CancellationToken,
    context: vscode.CompletionContext
  ): Promise<vscode.CompletionItem[] | vscode.CompletionList | null> {
    try {
      // Throttle completions
      const now = Date.now();
      if (now - this.lastCompletionTime < this.completionThrottle) {
        return null;
      }
      this.lastCompletionTime = now;

      // Get the current line and word
      const line = document.lineAt(position.line);
      const currentLine = line.text.substring(0, position.character);
      const wordMatch = currentLine.match(/\w+$/);
      const word = wordMatch ? wordMatch[0] : '';

      // Don't complete if word is too short
      if (word.length < 2) {
        return null;
      }

      this.logger.debug(`Generating completions for: ${word}`);

      // Get context for better completions
      const contextStart = Math.max(0, position.line - 5);
      const contextLines = document.getText(
        new vscode.Range(contextStart, 0, position.line, position.character)
      );

      // Request completions from AI
      const completions = await this.getAICompletions(
        word,
        contextLines,
        document.languageId,
        token
      );

      return completions;
    } catch (error) {
      this.logger.error('Error providing completions:', error);
      return null;
    }
  }

  private async getAICompletions(
    word: string,
    context: string,
    languageId: string,
    token: vscode.CancellationToken
  ): Promise<vscode.CompletionItem[]> {
    try {
      const messages = [
        {
          role: 'system',
          content: `You are a code completion expert. Suggest 5-10 relevant completions for the partial word in ${languageId} code. Return JSON array: [{"label": "completion", "detail": "description"}, ...]`
        },
        {
          role: 'user',
          content: `Complete "${word}" in this context:\n\`\`\`${languageId}\n${context}\n\`\`\`\nReturn JSON array of completions.`
        }
      ];

      const response = await this.aiProvider.chat(messages);

      try {
        const parsed = JSON.parse(response);

        return parsed.map((item: any, index: number) => {
          const completion = new vscode.CompletionItem(
            item.label,
            vscode.CompletionItemKind.Variable
          );
          completion.detail = item.detail;
          completion.sortText = String(index).padStart(2, '0');
          return completion;
        });
      } catch {
        // Fallback: return basic completions
        return [
          new vscode.CompletionItem(`${word}_1`, vscode.CompletionItemKind.Variable),
          new vscode.CompletionItem(`${word}_2`, vscode.CompletionItemKind.Variable)
        ];
      }
    } catch (error) {
      this.logger.error('Error getting AI completions:', error);
      return [];
    }
  }

  resolveCompletionItem?(
    item: vscode.CompletionItem,
    token: vscode.CancellationToken
  ): vscode.ProviderResult<vscode.CompletionItem> {
    // Could enhance with more details if needed
    return item;
  }
}
