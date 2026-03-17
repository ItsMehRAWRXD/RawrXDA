import * as vscode from 'vscode';
import axios, { AxiosInstance } from 'axios';
import { Logger } from '../utils/Logger';
import { WebAuthManager } from './WebAuthManager';

export class AICopilotProvider {
  private logger: Logger;
  private context: vscode.ExtensionContext;
  private webAuthManager: WebAuthManager;
  private apiClient: AxiosInstance;
  private requestTimeout = 30000; // 30 seconds
  private maxRetries = 3;
  private baseRetryDelay = 1000; // 1 second

  constructor(context: vscode.ExtensionContext, webAuthManager: WebAuthManager) {
    this.logger = new Logger('AICopilotProvider');
    this.context = context;
    this.webAuthManager = webAuthManager;
    this.apiClient = axios.create();
  }

  private async retryWithBackoff<T>(
    operation: () => Promise<T>,
    operationName: string
  ): Promise<T> {
    let lastError: any;
    
    for (let attempt = 0; attempt <= this.maxRetries; attempt++) {
      try {
        return await operation();
      } catch (error: any) {
        lastError = error;
        const is429 = error.response?.status === 429;
        const is5xx = error.response?.status >= 500;
        
        if ((is429 || is5xx) && attempt < this.maxRetries) {
          const delay = this.baseRetryDelay * Math.pow(2, attempt);
          this.logger.warn(
            `${operationName} failed (${error.response?.status}), retrying in ${delay}ms (attempt ${attempt + 1}/${this.maxRetries})`
          );
          await new Promise(resolve => setTimeout(resolve, delay));
        } else {
          throw error;
        }
      }
    }
    
    throw lastError;
  }

  private async getApiConfig() {
    const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
    // Use embedded API key for Win32 agentic IDE (production)
    const defaultApiKey = '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';
    const token = await this.webAuthManager.getToken() || defaultApiKey;

    return {
      apiEndpoint: config.get<string>('apiEndpoint', 'https://api.openai.com/v1'),
      token: token,
      model: config.get<string>('model', 'gpt-5.2-pro'),
      temperature: config.get<number>('temperature', 0.7),
      maxTokens: config.get<number>('maxTokens', 4096),
      timeout: this.requestTimeout
    };
  }

  async getAvailableModels(): Promise<string[]> {
    try {
      this.logger.debug('Fetching available models from OpenAI API');
      const apiConfig = await this.getApiConfig();

      const response = await this.apiClient.get(
        `${apiConfig.apiEndpoint}/models`,
        {
          headers: {
            'Authorization': `Bearer ${apiConfig.token}`,
            'Content-Type': 'application/json'
          },
          timeout: this.requestTimeout
        }
      );

      const models = response.data.data
        .map((m: any) => m.id)
        .filter((id: string) => id.includes('gpt') || id.includes('text-embedding') || id.includes('tts') || id.includes('whisper'))
        .sort();

      this.logger.info(`Retrieved ${models.length} available models`);
      return models;
    } catch (error) {
      this.logger.error('Error fetching models:', error);
      // Return fallback model list if fetch fails
      return [
        'gpt-5.2-pro',
        'gpt-5.2',
        'gpt-5-pro',
        'gpt-5',
        'gpt-4o',
        'gpt-4o-mini',
        'gpt-3.5-turbo',
        'o1',
        'o3-mini'
      ];
    }
  }

  async explainCode(code: string, languageId: string): Promise<string> {
    return this.retryWithBackoff(async () => {
      try {
        this.logger.debug(`Explaining code (${languageId}): ${code.substring(0, 50)}...`);
        const startTime = Date.now();

        const apiConfig = await this.getApiConfig();
        if (!apiConfig.token) {
          throw new Error('Not authenticated. Please authenticate first.');
        }

        const response = await this.apiClient.post(
          `${apiConfig.apiEndpoint}/chat/completions`,
          {
            model: apiConfig.model,
            messages: [
              {
                role: 'system',
                content: 'You are an expert code analyst. Provide clear, concise explanations of code.'
              },
              {
                role: 'user',
                content: `Explain the following ${languageId} code:\n\`\`\`\n${code}\n\`\`\``
              }
            ],
            temperature: apiConfig.temperature,
            max_tokens: apiConfig.maxTokens
          },
          {
            headers: {
              'Authorization': `Bearer ${apiConfig.token}`,
              'Content-Type': 'application/json'
            },
            timeout: apiConfig.timeout
          }
        );

        const explanation = response.data.choices[0].message.content;
        const duration = Date.now() - startTime;
        this.logger.info(`Code explanation completed in ${duration}ms`);

        return explanation;
      } catch (error) {
        this.logger.error('Error explaining code:', error);
        throw error;
      }
    }, 'explainCode');
  }

  async refactorCode(code: string, languageId: string): Promise<string> {
    return this.retryWithBackoff(async () => {
      try {
        this.logger.debug(`Refactoring code (${languageId}): ${code.substring(0, 50)}...`);
        const startTime = Date.now();

        const apiConfig = await this.getApiConfig();
        if (!apiConfig.token) {
          throw new Error('Not authenticated. Please authenticate first.');
        }

        const response = await this.apiClient.post(
          `${apiConfig.apiEndpoint}/chat/completions`,
          {
            model: apiConfig.model,
            messages: [
              {
                role: 'system',
                content: 'You are an expert code refactorer. Improve code quality, readability, and performance. Return only the refactored code.'
              },
              {
                role: 'user',
                content: `Refactor the following ${languageId} code:\n\`\`\`\n${code}\n\`\`\``
              }
            ],
            temperature: apiConfig.temperature,
            max_tokens: apiConfig.maxTokens
          },
          {
            headers: {
              'Authorization': `Bearer ${apiConfig.token}`,
              'Content-Type': 'application/json'
            },
            timeout: apiConfig.timeout
          }
        );

        const refactoredCode = response.data.choices[0].message.content;
        const duration = Date.now() - startTime;
        this.logger.info(`Code refactoring completed in ${duration}ms`);

        return refactoredCode;
      } catch (error) {
        this.logger.error('Error refactoring code:', error);
        throw error;
      }
    }, 'refactorCode');
  }

  async generateCode(prompt: string, languageId: string): Promise<string> {
    try {
      this.logger.debug(`Generating code (${languageId}): ${prompt}`);
      const startTime = Date.now();

      const apiConfig = await this.getApiConfig();
      if (!apiConfig.token) {
        throw new Error('Not authenticated. Please authenticate first.');
      }

      const response = await this.apiClient.post(
        `${apiConfig.apiEndpoint}/chat/completions`,
        {
          model: apiConfig.model,
          messages: [
            {
              role: 'system',
              content: `You are an expert ${languageId} developer. Generate clean, well-structured code. Return only the code without explanations.`
            },
            {
              role: 'user',
              content: `Generate ${languageId} code for: ${prompt}`
            }
          ],
          temperature: apiConfig.temperature,
          max_tokens: apiConfig.maxTokens
        },
        {
          headers: {
            'Authorization': `Bearer ${apiConfig.token}`,
            'Content-Type': 'application/json'
          },
          timeout: apiConfig.timeout
        }
      );

      const generatedCode = response.data.choices[0].message.content;
      const duration = Date.now() - startTime;
      this.logger.info(`Code generation completed in ${duration}ms`);

      return generatedCode;
    } catch (error) {
      this.logger.error('Error generating code:', error);
      throw error;
    }
  }

  async optimizeCode(code: string, languageId: string): Promise<string> {
    try {
      this.logger.debug(`Optimizing code (${languageId}): ${code.substring(0, 50)}...`);
      const startTime = Date.now();

      const apiConfig = await this.getApiConfig();
      if (!apiConfig.token) {
        throw new Error('Not authenticated. Please authenticate first.');
      }

      const response = await this.apiClient.post(
        `${apiConfig.apiEndpoint}/chat/completions`,
        {
          model: apiConfig.model,
          messages: [
            {
              role: 'system',
              content: 'You are an expert code optimizer. Improve performance and efficiency. Return only the optimized code.'
            },
            {
              role: 'user',
              content: `Optimize the following ${languageId} code for performance:\n\`\`\`\n${code}\n\`\`\``
            }
          ],
          temperature: apiConfig.temperature,
          max_tokens: apiConfig.maxTokens
        },
        {
          headers: {
            'Authorization': `Bearer ${apiConfig.token}`,
            'Content-Type': 'application/json'
          },
          timeout: apiConfig.timeout
        }
      );

      const optimizedCode = response.data.choices[0].message.content;
      const duration = Date.now() - startTime;
      this.logger.info(`Code optimization completed in ${duration}ms`);

      return optimizedCode;
    } catch (error) {
      this.logger.error('Error optimizing code:', error);
      throw error;
    }
  }

  async chat(messages: any[]): Promise<string> {
    try {
      this.logger.debug(`Chat with ${messages.length} messages`);
      const startTime = Date.now();

      const apiConfig = await this.getApiConfig();
      if (!apiConfig.token) {
        throw new Error('Authentication required. Please set your OpenAI API key in settings.');
      }

      const response = await this.apiClient.post(
        `${apiConfig.apiEndpoint}/chat/completions`,
        {
          model: apiConfig.model,
          messages: messages,
          temperature: apiConfig.temperature,
          max_tokens: apiConfig.maxTokens
        },
        {
          headers: {
            'Authorization': `Bearer ${apiConfig.token}`,
            'Content-Type': 'application/json'
          },
          timeout: apiConfig.timeout
        }
      );

      const reply = response.data.choices[0].message.content;
      const duration = Date.now() - startTime;
      this.logger.info(`Chat response received in ${duration}ms`);

      return reply;
    } catch (error: any) {
      this.logger.error('Error in chat:', error);
      // Provide more helpful error messages
      if (error.response?.status === 404) {
        throw new Error('API endpoint not found. Check your API configuration.');
      } else if (error.response?.status === 401) {
        throw new Error('Invalid API key. Please authenticate with a valid OpenAI API key.');
      } else if (error.message?.includes('Not authenticated') || error.message?.includes('Authentication required')) {
        throw error; // Re-throw authentication errors
      }
      throw error;
    }
  }
}
