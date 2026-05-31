import { MessageProcessor, QueuedMessage } from '../queue/message-queue';
import { ConfigManager } from '../config';
import { Logger, logger } from '../logger';
import { MetricsCollector, metrics } from '../metrics';

export interface AIStreamConfig {
  model: string;
  temperature?: number;
  maxTokens?: number;
  topP?: number;
  frequencyPenalty?: number;
  presencePenalty?: number;
  systemPrompt?: string;
}

export interface ChatCompletionChunk {
  id: string;
  object: string;
  created: number;
  model: string;
  choices: Array<{
    index: number;
    delta: {
      role?: string;
      content?: string;
      function_call?: any;
    };
    finish_reason: string | null;
  }>;
}

export class AIStreamProcessor implements MessageProcessor {
  private config = ConfigManager.getInstance().get();
  private logger: Logger = logger;
  private metrics: MetricsCollector = metrics;

  private defaultModel: string = 'gpt-4-turbo-preview';
  private defaultConfig: AIStreamConfig = {
    model: this.defaultModel,
    temperature: 0.7,
    maxTokens: 4096,
    topP: 1,
    frequencyPenalty: 0,
    presencePenalty: 0,
  };

  constructor(private apiKey?: string) {
    this.apiKey = apiKey || this.config.api.apiKey;
  }

  async *process(message: QueuedMessage): AsyncGenerator<string, void, unknown> {
    const requestConfig = this.buildRequestConfig(message);
    const requestBody = this.buildRequestBody(message, requestConfig);

    this.logger.debug('Starting AI stream', {
      messageId: message.id,
      model: requestConfig.model
    });

    try {
      const response = await fetch(this.getEndpoint(), {
        method: 'POST',
        headers: this.getHeaders(),
        body: JSON.stringify(requestBody),
        signal: AbortSignal.timeout(this.config.api.timeout),
      });

      if (!response.ok) {
        const error = await response.text();
        throw new Error(`AI API error: ${response.status} - ${error}`);
      }

      if (!response.body) {
        throw new Error('No response body');
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let totalTokens = 0;
      const startTime = Date.now();

      while (true) {
        const { done, value } = await reader.read();

        if (done) {
          this.logger.debug('Stream completed', {
            messageId: message.id,
            tokens: totalTokens,
            duration: Date.now() - startTime
          });
          break;
        }

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop() || '';

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed || trimmed === 'data: [DONE]') continue;

          if (trimmed.startsWith('data: ')) {
            const data = trimmed.slice(6);
            try {
              const chunk: ChatCompletionChunk = JSON.parse(data);
              const content = chunk.choices[0]?.delta?.content;

              if (content) {
                totalTokens++;
                this.metrics.increment('ai.tokens_generated');
                yield content;
              }

              if (chunk.choices[0]?.finish_reason === 'stop') {
                this.metrics.histogram('ai.response_time', Date.now() - startTime);
                this.metrics.histogram('ai.tokens_per_request', totalTokens);
              }

            } catch (parseError) {
              this.logger.warn('Failed to parse chunk', {
                messageId: message.id,
                data: data.substring(0, 100)
              });
            }
          }
        }
      }

    } catch (error: any) {
      this.metrics.increment('ai.errors');

      if (error.name === 'AbortError' || error.name === 'TimeoutError') {
        throw new Error('AI request timeout');
      }

      throw error;
    }
  }

  private buildRequestConfig(message: QueuedMessage): AIStreamConfig {
    const metadata = message.metadata as any;

    return {
      ...this.defaultConfig,
      model: metadata?.model || this.defaultModel,
      temperature: metadata?.temperature ?? this.defaultConfig.temperature,
      maxTokens: metadata?.maxTokens ?? this.defaultConfig.maxTokens,
      systemPrompt: metadata?.systemPrompt,
    };
  }

  private buildRequestBody(message: QueuedMessage, config: AIStreamConfig): any {
    const messages: any[] = [];

    if (config.systemPrompt) {
      messages.push({
        role: 'system',
        content: config.systemPrompt,
      });
    }

    if (message.context) {
      const contextPrompt = this.buildContextPrompt(message.context);
      messages.push({
        role: 'system',
        content: contextPrompt,
      });
    }

    messages.push({
      role: 'user',
      content: message.content,
    });

    return {
      model: config.model,
      messages,
      temperature: config.temperature,
      max_tokens: config.maxTokens,
      top_p: config.topP,
      frequency_penalty: config.frequencyPenalty,
      presence_penalty: config.presencePenalty,
      stream: true,
    };
  }

  private buildContextPrompt(context: any): string {
    const parts: string[] = [];

    if (context.fileName) {
      parts.push(`Current file: ${context.fileName}`);
    }

    if (context.language) {
      parts.push(`Language: ${context.language}`);
    }

    if (context.selection) {
      parts.push(`Selected code (lines ${context.selection.startLine}-${context.selection.endLine}):\n\`\`\`\n${context.selection.text}\n\`\`\``);
    }

    if (context.cursor) {
      parts.push(`Cursor position: line ${context.cursor.line}, column ${context.cursor.column}`);
    }

    if (context.recentFiles && context.recentFiles.length > 0) {
      parts.push(`Recent files: ${context.recentFiles.join(', ')}`);
    }

    return parts.join('\n\n');
  }

  private getEndpoint(): string {
    const baseUrl = this.config.api.baseUrl.replace(/\/$/, '');
    return `${baseUrl}${this.config.api.streamEndpoint}`;
  }

  private getHeaders(): Record<string, string> {
    return {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${this.apiKey}`,
      'Accept': 'text/event-stream',
    };
  }
}
