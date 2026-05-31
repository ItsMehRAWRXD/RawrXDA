import { MessageQueue, MessagePriority } from './queue/message-queue';
import { AIStreamProcessor } from './processor/ai-processor';
import { ConfigManager } from './config';
import { Logger } from './logger';
import { MetricsCollector } from './metrics';

export class ChatSystem {
  private queue: MessageQueue;
  private processor: AIStreamProcessor;
  private config = ConfigManager.getInstance();
  private logger = Logger.getInstance();
  private metrics = MetricsCollector.getInstance();

  constructor(apiKey?: string) {
    this.processor = new AIStreamProcessor(apiKey);
    this.queue = new MessageQueue(this.processor);

    this.setupErrorHandling();

    this.logger.info('Chat system initialized', {
      config: this.config.get(),
    });
  }

  private setupErrorHandling(): void {
    this.queue.on('queue:error', (error: Error) => {
      this.logger.error('Queue error', error);
    });

    if (typeof process !== 'undefined' && process.on) {
      process.on('unhandledRejection', (reason, _promise) => {
        this.logger.error('Unhandled rejection', reason as Error);
      });

      process.on('uncaughtException', (error) => {
        this.logger.fatal('Uncaught exception', error);
      });
    }
  }

  sendMessage(content: string, priority: MessagePriority = MessagePriority.NORMAL, context?: any): string {
    const message = this.queue.enqueue(content, priority, context);
    return message.id;
  }

  getQueue(): MessageQueue {
    return this.queue;
  }

  getMetrics() {
    return this.metrics.getQueueMetrics();
  }

  getLogs() {
    return this.logger.getBuffer();
  }

  async shutdown(): Promise<void> {
    await this.queue.shutdown();
    this.logger.flushToFile(`shutdown-${Date.now()}.log`);
  }
}

export * from './queue/message-queue';
export * from './processor/ai-processor';
export * from './config';
export * from './logger';
export * from './metrics';
export * from './persistence/repository';

let instance: ChatSystem | null = null;

export function initialize(apiKey?: string): ChatSystem {
  if (!instance) {
    instance = new ChatSystem(apiKey);
  }
  return instance;
}

export function getInstance(): ChatSystem {
  if (!instance) {
    throw new Error('Chat system not initialized. Call initialize() first.');
  }
  return instance;
}
