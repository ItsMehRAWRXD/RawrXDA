/**
 * OpenAI API Client Module
 * Handles API communication with OpenAI's latest models
 */

import axios, { AxiosInstance } from 'axios';
import * as vscode from 'vscode';

export interface OpenAICompletionRequest {
  model: string;
  messages: Array<{ role: string; content: string }>;
  temperature?: number;
  max_tokens?: number;
  top_p?: number;
  frequency_penalty?: number;
  presence_penalty?: number;
}

export interface OpenAICompletionResponse {
  id: string;
  object: string;
  created: number;
  model: string;
  choices: Array<{
    index: number;
    message: { role: string; content: string };
    finish_reason: string;
  }>;
  usage: {
    prompt_tokens: number;
    completion_tokens: number;
    total_tokens: number;
  };
}

export class OpenAIAPIClient {
  private client: AxiosInstance;
  private apiKey: string = '';
  private endpoint: string;
  private maxRetries: number = 3;
  private retryDelay: number = 1000; // Start with 1 second

  constructor(endpoint: string = 'https://api.openai.com/v1') {
    this.endpoint = endpoint;
    this.client = axios.create({
      baseURL: endpoint,
      timeout: 30000,
      headers: {
        'Content-Type': 'application/json',
      },
    });
  }

  /**
   * Sleep utility for retry delays
   */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Retry wrapper with exponential backoff
   */
  private async retryWithBackoff<T>(
    operation: () => Promise<T>,
    operationName: string = 'API call'
  ): Promise<T> {
    let lastError: any;

    for (let attempt = 0; attempt <= this.maxRetries; attempt++) {
      try {
        return await operation();
      } catch (error) {
        lastError = error;

        if (axios.isAxiosError(error)) {
          const status = error.response?.status;

          // Don't retry on authentication errors
          if (status === 401 || status === 403) {
            throw error;
          }

          // Retry on rate limits and server errors
          if (status === 429 || status === 500 || status === 503) {
            if (attempt < this.maxRetries) {
              const delay = this.retryDelay * Math.pow(2, attempt);
              const retryAfter = error.response?.headers['retry-after'];
              const waitTime = retryAfter ? parseInt(retryAfter) * 1000 : delay;

              console.log(`${operationName} failed with ${status}, retrying in ${waitTime}ms (attempt ${attempt + 1}/${this.maxRetries})`);

              await vscode.window.withProgress(
                {
                  location: vscode.ProgressLocation.Notification,
                  title: `Rate limited. Retrying in ${Math.round(waitTime / 1000)}s...`,
                  cancellable: false
                },
                async () => {
                  await this.sleep(waitTime);
                }
              );

              continue;
            }
          }
        }

        // Don't retry other errors
        throw error;
      }
    }

    throw lastError;
  }

  /**
   * Initialize the API client with an API key
   */
  async initialize(apiKey?: string): Promise<boolean> {
    try {
      if (!apiKey) {
        apiKey = await this.loadAPIKey();
      }

      if (!apiKey) {
        vscode.window.showErrorMessage('OpenAI API key not found. Please set it in extension settings.');
        return false;
      }

      this.apiKey = apiKey;
      this.client.defaults.headers.common['Authorization'] = `Bearer ${this.apiKey}`;

      // Verify the API key by fetching models list
      return await this.verifyAPIKey();
    } catch (error) {
      vscode.window.showErrorMessage(`Failed to initialize OpenAI API: ${error}`);
      return false;
    }
  }

  /**
   * Load API key from configured source
   */
  private async loadAPIKey(): Promise<string | undefined> {
    const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
    const source = config.get<string>('apiKeySource', 'vscodeSecrets');

    switch (source) {
      case 'vscodeSecrets':
        return await this.loadFromSecrets();
      case 'environment':
        return process.env.OPENAI_API_KEY;
      case 'file':
        return await this.loadFromEnvFile();
      default:
        return await this.loadFromSecrets();
    }
  }

  /**
   * Load API key from VS Code secrets
   */
  private async loadFromSecrets(): Promise<string | undefined> {
    const secretStorage = vscode.workspace.workspaceFolders?.[0]?.uri as any;
    if (!secretStorage) {
      // Fallback to configuration
      const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
      return config.get<string>('apiKey');
    }
    return undefined;
  }

  /**
   * Load API key from .env file
   */
  private async loadFromEnvFile(): Promise<string | undefined> {
    try {
      const workspaceFolders = vscode.workspace.workspaceFolders;
      if (!workspaceFolders || workspaceFolders.length === 0) {
        return undefined;
      }

      const envPath = vscode.Uri.joinPath(workspaceFolders[0].uri, '.env');
      const envContent = await vscode.workspace.fs.readFile(envPath);
      const envText = new TextDecoder().decode(envContent);

      const match = envText.match(/OPENAI_API_KEY\s*=\s*([^\n\r]+)/);
      return match ? match[1].trim() : undefined;
    } catch {
      return undefined;
    }
  }

  /**
   * Verify the API key is valid
   */
  private async verifyAPIKey(): Promise<boolean> {
    try {
      const response = await this.client.get('/models', {
        headers: { 'Authorization': `Bearer ${this.apiKey}` },
      });
      return response.status === 200 && Array.isArray(response.data.data);
    } catch (error) {
      console.error('API key verification failed:', error);
      return false;
    }
  }

  /**
   * Get list of available models
   */
  async listModels(): Promise<any[]> {
    try {
      return await this.retryWithBackoff(
        async () => {
          const response = await this.client.get('/models');
          return response.data.data || [];
        },
        'List models'
      );
    } catch (error) {
      console.error('Failed to list models:', error);
      if (axios.isAxiosError(error) && error.response?.status === 429) {
        vscode.window.showWarningMessage(
          'Rate limit reached while fetching models. Using cached model list.'
        );
      }
      return [];
    }
  }

  /**
   * Create a completion using the specified model
   */
  async createCompletion(request: OpenAICompletionRequest): Promise<OpenAICompletionResponse | null> {
    try {
      return await this.retryWithBackoff(
        async () => {
          const response = await this.client.post('/chat/completions', request);
          return response.data;
        },
        'Completion request'
      );
    } catch (error) {
      if (axios.isAxiosError(error)) {
        const status = error.response?.status;
        const message = error.response?.data?.error?.message || error.message;
        const errorType = error.response?.data?.error?.type;

        if (status === 401) {
          vscode.window.showErrorMessage('OpenAI API: Invalid or expired API key. Please update your API key in settings.');
        } else if (status === 429) {
          const retryAfter = error.response?.headers['retry-after'];
          const waitTime = retryAfter ? `${retryAfter} seconds` : 'a moment';
          vscode.window.showErrorMessage(
            `OpenAI API: Rate limit exceeded after retries. Please wait ${waitTime} before trying again. ` +
            `Consider upgrading your OpenAI plan for higher limits.`,
            'Check Usage',
            'Upgrade Plan'
          ).then(selection => {
            if (selection === 'Check Usage') {
              vscode.env.openExternal(vscode.Uri.parse('https://platform.openai.com/account/usage'));
            } else if (selection === 'Upgrade Plan') {
              vscode.env.openExternal(vscode.Uri.parse('https://platform.openai.com/account/billing/overview'));
            }
          });
        } else if (status === 500 || status === 503) {
          vscode.window.showErrorMessage(
            `OpenAI API: Service temporarily unavailable. Please try again in a few moments. ` +
            `If the issue persists, check OpenAI status.`,
            'Check Status'
          ).then(selection => {
            if (selection === 'Check Status') {
              vscode.env.openExternal(vscode.Uri.parse('https://status.openai.com'));
            }
          });
        } else if (status === 400) {
          vscode.window.showErrorMessage(
            `OpenAI API: Invalid request - ${message}. ` +
            `This may be due to an unsupported model or invalid parameters.`
          );
        } else if (errorType === 'insufficient_quota') {
          vscode.window.showErrorMessage(
            `OpenAI API: Insufficient quota. Please check your plan and billing details.`,
            'Check Billing'
          ).then(selection => {
            if (selection === 'Check Billing') {
              vscode.env.openExternal(vscode.Uri.parse('https://platform.openai.com/account/billing/overview'));
            }
          });
        } else {
          vscode.window.showErrorMessage(`OpenAI API error (${status}): ${message}`);
        }
      } else {
        vscode.window.showErrorMessage(`Failed to complete request: ${error}`);
      }
      return null;
    }
  }

  /**
   * Create a streaming completion (returns an async iterator)
   */
  async *createStreamingCompletion(request: OpenAICompletionRequest): AsyncGenerator<string> {
    try {
      const response = await this.client.post('/chat/completions',
        { ...request, stream: true },
        { responseType: 'stream' }
      );

      for await (const chunk of response.data) {
        const line = chunk.toString('utf8').trim();
        if (line.startsWith('data: ')) {
          const data = line.slice(6);
          if (data === '[DONE]') break;

          try {
            const json = JSON.parse(data);
            const content = json.choices?.[0]?.delta?.content;
            if (content) {
              yield content;
            }
          } catch {
            // Ignore parse errors
          }
        }
      }
    } catch (error) {
      console.error('Streaming completion error:', error);
      throw error;
    }
  }

  /**
   * Moderate text content
   */
  async moderateContent(text: string): Promise<boolean> {
    try {
      const response = await this.client.post('/moderations', { input: text });
      return !response.data.results?.[0]?.flagged;
    } catch (error) {
      console.error('Moderation error:', error);
      return true; // Allow by default on error
    }
  }

  /**
   * Get embedding for text
   */
  async getEmbedding(text: string, model: string = 'text-embedding-3-small'): Promise<number[] | null> {
    try {
      const response = await this.client.post('/embeddings', {
        model,
        input: text,
      });
      return response.data.data?.[0]?.embedding || null;
    } catch (error) {
      console.error('Embedding error:', error);
      return null;
    }
  }

  /**
   * Set API endpoint
   */
  setEndpoint(endpoint: string): void {
    this.endpoint = endpoint;
    this.client.defaults.baseURL = endpoint;
  }

  /**
   * Get current endpoint
   */
  getEndpoint(): string {
    return this.endpoint;
  }

  /**
   * Check if API key is set
   */
  isAuthenticated(): boolean {
    return !!this.apiKey;
  }

  /**
   * Clear API key
   */
  clearAuthentication(): void {
    this.apiKey = '';
    delete this.client.defaults.headers.common['Authorization'];
  }
}
