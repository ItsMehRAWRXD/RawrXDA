import * as vscode from 'vscode';
import { AICopilotProvider } from '../ai/AICopilotProvider';
import { Logger } from '../utils/Logger';

export class CodeAnalyzer {
  private logger: Logger;
  private context: vscode.ExtensionContext;
  private aiProvider: AICopilotProvider;
  private cache: Map<string, string> = new Map();
  private cacheTimeout = 3600000; // 1 hour
  private cacheTimestamps: Map<string, number> = new Map();

  constructor(context: vscode.ExtensionContext, aiProvider: AICopilotProvider) {
    this.logger = new Logger('CodeAnalyzer');
    this.context = context;
    this.aiProvider = aiProvider;
  }

  async analyzeWord(word: string, document: vscode.TextDocument, position: vscode.Position): Promise<string | null> {
    try {
      // Check cache first
      if (this.cache.has(word)) {
        const timestamp = this.cacheTimestamps.get(word) || 0;
        if (Date.now() - timestamp < this.cacheTimeout) {
          this.logger.debug(`Using cached analysis for: ${word}`);
          return this.cache.get(word) || null;
        } else {
          this.cache.delete(word);
          this.cacheTimestamps.delete(word);
        }
      }

      this.logger.debug(`Analyzing word: ${word}`);

      // Get context around the word
      const line = document.lineAt(position.line);
      const context = line.text;

      // Determine language and provide relevant analysis
      const languageId = document.languageId;

      let analysis: string;

      // Built-in analysis for common patterns
      if (this.isBuiltinKeyword(word, languageId)) {
        analysis = this.getBuiltinAnalysis(word, languageId);
      } else {
        // Use AI for custom analysis
        analysis = await this.getAIAnalysis(word, context, languageId);
      }

      // Cache the result
      this.cache.set(word, analysis);
      this.cacheTimestamps.set(word, Date.now());

      return analysis;
    } catch (error) {
      this.logger.error('Error analyzing word:', error);
      return null;
    }
  }

  private isBuiltinKeyword(word: string, languageId: string): boolean {
    const builtins: { [key: string]: string[] } = {
      'javascript': ['function', 'const', 'let', 'var', 'async', 'await', 'return', 'if', 'else', 'for', 'while'],
      'typescript': ['function', 'const', 'let', 'var', 'async', 'await', 'return', 'if', 'else', 'for', 'while', 'interface', 'type'],
      'python': ['def', 'class', 'return', 'if', 'else', 'for', 'while', 'try', 'except', 'import', 'from'],
      'cpp': ['void', 'int', 'class', 'struct', 'if', 'else', 'for', 'while', 'return', 'new', 'delete']
    };

    const keywords = builtins[languageId] || [];
    return keywords.includes(word.toLowerCase());
  }

  private getBuiltinAnalysis(word: string, languageId: string): string {
    const analyses: { [key: string]: { [key: string]: string } } = {
      'javascript': {
        'async': 'Makes a function return a Promise and allows using `await` inside',
        'await': 'Pauses execution until a Promise resolves',
        'const': 'Declares a block-scoped variable that cannot be reassigned',
        'let': 'Declares a block-scoped variable that can be reassigned',
        'var': 'Declares a function-scoped variable (avoid in modern JS)',
        'function': 'Declares a function that can be hoisted'
      },
      'typescript': {
        'interface': 'Defines a contract/shape for objects',
        'type': 'Creates a type alias for any type',
        'async': 'Makes a function return a Promise and allows using `await` inside',
        'await': 'Pauses execution until a Promise resolves'
      },
      'python': {
        'def': 'Defines a function',
        'class': 'Defines a class for object-oriented programming',
        'import': 'Imports a module',
        'from': 'Imports specific items from a module'
      }
    };

    return analyses[languageId]?.[word.toLowerCase()] || `Keyword: ${word}`;
  }

  private async getAIAnalysis(word: string, context: string, languageId: string): Promise<string> {
    try {
      // Request AI analysis
      const messages = [
        {
          role: 'system',
          content: 'You are a code analyst. Provide a brief (1-2 sentences) explanation of what the given word/identifier does in code context.'
        },
        {
          role: 'user',
          content: `In ${languageId} code, what does "${word}" do in this context?\n${context}`
        }
      ];

      const analysis = await this.aiProvider.chat(messages);
      return analysis;
    } catch (error) {
      this.logger.error('Error getting AI analysis:', error);
      return `Analysis for: ${word}`;
    }
  }

  async analyzeCode(code: string, languageId: string): Promise<{ issues: string[]; suggestions: string[] }> {
    try {
      this.logger.debug(`Analyzing code block (${languageId})`);

      const messages = [
        {
          role: 'system',
          content: 'You are a code quality analyzer. Identify issues and suggest improvements. Return JSON with "issues" and "suggestions" arrays.'
        },
        {
          role: 'user',
          content: `Analyze this ${languageId} code:\n\`\`\`\n${code}\n\`\`\`\n\nReturn JSON: {"issues": [], "suggestions": []}`
        }
      ];

      const response = await this.aiProvider.chat(messages);

      try {
        const parsed = JSON.parse(response);
        return parsed;
      } catch {
        return { issues: [response], suggestions: [] };
      }
    } catch (error) {
      this.logger.error('Error analyzing code:', error);
      return { issues: ['Analysis failed'], suggestions: [] };
    }
  }

  clearCache(): void {
    this.cache.clear();
    this.cacheTimestamps.clear();
    this.logger.debug('Analysis cache cleared');
  }
}
