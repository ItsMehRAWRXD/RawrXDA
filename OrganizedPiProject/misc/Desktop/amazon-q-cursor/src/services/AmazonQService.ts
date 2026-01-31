import { BedrockRuntimeClient, InvokeModelCommand } from '@aws-sdk/client-bedrock-runtime';
import { fromEnv, fromIni } from '@aws-sdk/credential-providers';
import { AmazonQConfig, ChatMessage, CodeAnalysis, AmazonQRequest, AmazonQResponse, FileContext } from '../types/index.js';

export class AmazonQService {
  private client: BedrockRuntimeClient;
  private config: AmazonQConfig;
  private conversationHistory: Map<string, ChatMessage[]> = new Map();

  constructor(config: AmazonQConfig) {
    this.config = config;
    this.initializeClient();
  }

  private initializeClient(): void {
    const credentials = this.config.profile 
      ? fromIni({ profile: this.config.profile })
      : fromEnv();

    this.client = new BedrockRuntimeClient({
      region: this.config.region,
      credentials: this.config.accessKeyId && this.config.secretAccessKey
        ? {
            accessKeyId: this.config.accessKeyId,
            secretAccessKey: this.config.secretAccessKey,
          }
        : credentials,
    });
  }

  async processRequest(request: AmazonQRequest): Promise<AmazonQResponse> {
    try {
      switch (request.type) {
        case 'chat':
          return await this.handleChat(request);
        case 'analyze':
          return await this.handleCodeAnalysis(request);
        case 'suggest':
          return await this.handleCodeSuggestions(request);
        case 'explain':
          return await this.handleCodeExplanation(request);
        case 'refactor':
          return await this.handleCodeRefactoring(request);
        default:
          throw new Error(`Unknown request type: ${request.type}`);
      }
    } catch (error) {
      console.error('Amazon Q Service Error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error occurred',
        conversationId: request.conversationId,
      };
    }
  }

  private async handleChat(request: AmazonQRequest): Promise<AmazonQResponse> {
    const conversationId = request.conversationId || this.generateConversationId();
    const history = this.conversationHistory.get(conversationId) || [];
    
    const messages = this.buildMessageHistory(history, request.content);
    const response = await this.invokeModel(messages);
    
    const assistantMessage: ChatMessage = {
      id: this.generateId(),
      role: 'assistant',
      content: response,
      timestamp: new Date(),
      metadata: {
        model: this.config.modelId,
      },
    };

    history.push({
      id: this.generateId(),
      role: 'user',
      content: request.content,
      timestamp: new Date(),
    });
    history.push(assistantMessage);
    this.conversationHistory.set(conversationId, history);

    return {
      success: true,
      data: assistantMessage,
      conversationId,
    };
  }

  private async handleCodeAnalysis(request: AmazonQRequest): Promise<AmazonQResponse> {
    const prompt = this.buildCodeAnalysisPrompt(request.content, request.context);
    const response = await this.invokeModel([{ role: 'user', content: prompt }]);
    
    const analysis: CodeAnalysis = this.parseCodeAnalysis(response);
    
    return {
      success: true,
      data: analysis,
      conversationId: request.conversationId,
    };
  }

  private async handleCodeSuggestions(request: AmazonQRequest): Promise<AmazonQResponse> {
    const prompt = this.buildCodeSuggestionsPrompt(request.content, request.context);
    const response = await this.invokeModel([{ role: 'user', content: prompt }]);
    
    const suggestions = this.parseCodeSuggestions(response);
    
    return {
      success: true,
      data: suggestions,
      conversationId: request.conversationId,
    };
  }

  private async handleCodeExplanation(request: AmazonQRequest): Promise<AmazonQResponse> {
    const prompt = this.buildCodeExplanationPrompt(request.content, request.context);
    const response = await this.invokeModel([{ role: 'user', content: prompt }]);
    
    return {
      success: true,
      data: { explanation: response },
      conversationId: request.conversationId,
    };
  }

  private async handleCodeRefactoring(request: AmazonQRequest): Promise<AmazonQResponse> {
    const prompt = this.buildCodeRefactoringPrompt(request.content, request.context);
    const response = await this.invokeModel([{ role: 'user', content: prompt }]);
    
    return {
      success: true,
      data: { refactoredCode: response },
      conversationId: request.conversationId,
    };
  }

  private async invokeModel(messages: Array<{ role: string; content: string }>): Promise<string> {
    const body = JSON.stringify({
      anthropic_version: "bedrock-2023-05-31",
      max_tokens: this.config.maxTokens,
      temperature: this.config.temperature,
      messages: messages,
    });

    const command = new InvokeModelCommand({
      modelId: this.config.modelId,
      body: body,
      contentType: 'application/json',
    });

    const response = await this.client.send(command);
    const responseBody = JSON.parse(new TextDecoder().decode(response.body));
    
    return responseBody.content[0].text;
  }

  private buildMessageHistory(history: ChatMessage[], newContent: string): Array<{ role: string; content: string }> {
    const messages = history.slice(-10).map(msg => ({
      role: msg.role,
      content: msg.content,
    }));
    
    messages.push({
      role: 'user',
      content: newContent,
    });
    
    return messages;
  }

  private buildCodeAnalysisPrompt(code: string, context?: FileContext): string {
    return `Analyze the following code and provide a comprehensive analysis including potential issues, improvements, and best practices:

${context ? `File: ${context.path}\nLanguage: ${context.language}\n` : ''}
Code:
\`\`\`${context?.language || 'javascript'}
${code}
\`\`\`

Please provide:
1. Code quality assessment
2. Potential bugs or issues
3. Performance optimizations
4. Security considerations
5. Best practice recommendations
6. Overall code rating (1-10)

Format your response as JSON with the following structure:
{
  "suggestions": [...],
  "issues": [...],
  "summary": "...",
  "confidence": 0.95
}`;
  }

  private buildCodeSuggestionsPrompt(code: string, context?: FileContext): string {
    return `Provide specific code improvement suggestions for the following code:

${context ? `File: ${context.path}\nLanguage: ${context.language}\n` : ''}
Code:
\`\`\`${context?.language || 'javascript'}
${code}
\`\`\`

Focus on:
1. Code readability and maintainability
2. Performance optimizations
3. Modern language features
4. Error handling improvements
5. Type safety enhancements

Provide specific, actionable suggestions with code examples.`;
  }

  private buildCodeExplanationPrompt(code: string, context?: FileContext): string {
    return `Explain the following code in detail:

${context ? `File: ${context.path}\nLanguage: ${context.language}\n` : ''}
Code:
\`\`\`${context?.language || 'javascript'}
${code}
\`\`\`

Provide:
1. What the code does
2. How it works (step by step)
3. Key concepts and patterns used
4. Potential use cases
5. Any important considerations or gotchas`;
  }

  private buildCodeRefactoringPrompt(code: string, context?: FileContext): string {
    return `Refactor the following code to improve its quality, readability, and maintainability:

${context ? `File: ${context.path}\nLanguage: ${context.language}\n` : ''}
Code:
\`\`\`${context?.language || 'javascript'}
${code}
\`\`\`

Please provide:
1. The refactored code
2. Explanation of changes made
3. Benefits of the refactoring
4. Any trade-offs or considerations`;
  }

  private parseCodeAnalysis(response: string): CodeAnalysis {
    try {
      const parsed = JSON.parse(response);
      return {
        suggestions: parsed.suggestions || [],
        issues: parsed.issues || [],
        summary: parsed.summary || '',
        confidence: parsed.confidence || 0.8,
      };
    } catch {
      return {
        suggestions: [],
        issues: [],
        summary: response,
        confidence: 0.5,
      };
    }
  }

  private parseCodeSuggestions(response: string): any[] {
    try {
      // Try to parse as JSON first
      const parsed = JSON.parse(response);
      if (Array.isArray(parsed)) {
        return parsed;
      }
      if (parsed.suggestions && Array.isArray(parsed.suggestions)) {
        return parsed.suggestions;
      }
    } catch {
      // If not JSON, try to extract suggestions from text
      const lines = response.split('\n');
      const suggestions = [];
      
      for (const line of lines) {
        if (line.trim().startsWith('-') || line.trim().startsWith('*')) {
          suggestions.push({
            type: 'improvement',
            title: line.trim().substring(1).trim(),
            description: line.trim(),
            severity: 'medium'
          });
        }
      }
      
      return suggestions;
    }
    
    return [];
  }

  private generateId(): string {
    return Math.random().toString(36).substr(2, 9);
  }

  private generateConversationId(): string {
    return `conv_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  getConversationHistory(conversationId: string): ChatMessage[] {
    return this.conversationHistory.get(conversationId) || [];
  }

  clearConversationHistory(conversationId?: string): void {
    if (conversationId) {
      this.conversationHistory.delete(conversationId);
    } else {
      this.conversationHistory.clear();
    }
  }
}
