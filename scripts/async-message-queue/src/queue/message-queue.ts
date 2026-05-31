import { v4 as uuidv4 } from 'uuid';
import { EventEmitter } from 'events';
import { ConfigManager } from '../config';
import { Logger, logger } from '../logger';
import { MetricsCollector, metrics } from '../metrics';

export enum MessagePriority {
  LOW = 0,
  NORMAL = 1,
  HIGH = 2,
  URGENT = 3,
}

export enum MessageStatus {
  PENDING = 'pending',
  QUEUED = 'queued',
  PROCESSING = 'processing',
  STREAMING = 'streaming',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled',
  TIMEOUT = 'timeout',
}

export interface MessageContext {
  fileId?: string;
  fileName?: string;
  language?: string;
  selection?: {
    startLine: number;
    endLine: number;
    text: string;
  };
  cursor?: {
    line: number;
    column: number;
  };
  recentFiles?: string[];
  recentEdits?: string[];
  metadata?: Record<string, unknown>;
}

export interface QueuedMessage {
  id: string;
  correlationId: string;
  content: string;
  priority: MessagePriority;
  status: MessageStatus;
  context?: MessageContext;
  response?: string;
  responseChunks?: string[];
  progress: number;
  error?: {
    code: string;
    message: string;
    stack?: string | undefined;
    retryable: boolean;
  };
  timestamps: {
    created: number;
    queued: number;
    processingStarted?: number;
    processingEnded?: number;
    completed?: number;
  };
  retryCount: number;
  metadata: Record<string, unknown>;
}

export interface MessageProcessor {
  process(message: QueuedMessage): AsyncGenerator<string, void, unknown>;
}

export class MessageQueue extends EventEmitter {
  private queue: QueuedMessage[] = [];
  private activeMessages: Map<string, QueuedMessage> = new Map();
  private messageStore: Map<string, QueuedMessage> = new Map();
  private processing: boolean = false;
  private paused: boolean = false;
  private shuttingDown: boolean = false;

  private config = ConfigManager.getInstance().get();
  private logger: Logger;
  private metrics: MetricsCollector;

  private processor: MessageProcessor | null = null;
  private abortControllers: Map<string, AbortController> = new Map();

  private persistenceTimer: NodeJS.Timeout | null = null;
  private healthCheckTimer: NodeJS.Timeout | null = null;

  constructor(processor?: MessageProcessor) {
    super();
    this.processor = processor || null;
    this.logger = logger;
    this.metrics = metrics;

    this.loadPersistedQueue();
    this.startPersistenceTimer();
    this.startHealthCheck();

    try {
      if (typeof process !== 'undefined' && process.versions?.node) {
        this.setupGracefulShutdown();
      }
    } catch {
      // Not in Node.js environment
    }
  }

  setProcessor(processor: MessageProcessor): void {
    this.processor = processor;
  }

  enqueue(
    content: string,
    priority: MessagePriority = MessagePriority.NORMAL,
    context?: MessageContext,
    metadata?: Record<string, unknown>
  ): QueuedMessage {
    if (this.shuttingDown) {
      throw new Error('Queue is shutting down');
    }

    if (this.queue.length >= this.config.queue.maxQueueSize) {
      this.logger.warn('Queue at capacity, rejecting message', {
        queueSize: this.queue.length,
        maxSize: this.config.queue.maxQueueSize
      });
      throw new Error('Queue is at maximum capacity');
    }

    const now = Date.now();
    const message: QueuedMessage = {
      id: uuidv4(),
      correlationId: uuidv4(),
      content,
      priority,
      status: MessageStatus.QUEUED,
      context: context || {},
      response: '',
      responseChunks: [],
      progress: 0,
      timestamps: {
        created: now,
        queued: now,
      },
      retryCount: 0,
      metadata: metadata || {},
    };

    this.insertByPriority(message);
    this.messageStore.set(message.id, message);

    this.metrics.increment('queue.total');
    this.metrics.gauge('queue.pending', this.queue.length);

    this.logger.info('Message enqueued', {
      messageId: message.id,
      priority: MessagePriority[priority],
      queuePosition: this.queue.length
    });

    this.emit('message:queued', message);
    this.processQueue();

    return message;
  }

  private insertByPriority(message: QueuedMessage): void {
    const index = this.queue.findIndex(m => m.priority < message.priority);
    if (index === -1) {
      this.queue.push(message);
    } else {
      this.queue.splice(index, 0, message);
    }
  }

  private async processQueue(): Promise<void> {
    if (this.processing || this.paused || this.shuttingDown) return;
    if (this.activeMessages.size >= this.config.queue.maxConcurrent) return;
    if (this.queue.length === 0) return;

    this.processing = true;

    try {
      while (
        this.queue.length > 0 &&
        this.activeMessages.size < this.config.queue.maxConcurrent &&
        !this.paused &&
        !this.shuttingDown
      ) {
        const message = this.queue.shift()!;

        this.metrics.gauge('queue.pending', this.queue.length);
        this.metrics.histogram('queue.wait_time', Date.now() - message.timestamps.queued);

        this.activeMessages.set(message.id, message);
        message.status = MessageStatus.PROCESSING;
        message.timestamps.processingStarted = Date.now();

        this.emit('message:started', message);

        this.processMessage(message).catch(error => {
          this.handleProcessingError(message, error);
        });
      }

      if (this.queue.length === 0 && this.activeMessages.size === 0) {
        this.emit('queue:drained');
      }
    } finally {
      this.processing = false;
    }
  }

  private async processMessage(message: QueuedMessage): Promise<void> {
    if (!this.processor) {
      throw new Error('No message processor configured');
    }

    const abortController = new AbortController();
    this.abortControllers.set(message.id, abortController);

    try {
      this.logger.debug('Processing message', { messageId: message.id });

      message.response = '';
      message.responseChunks = [];
      message.progress = 0;
      message.status = MessageStatus.STREAMING;

      const generator = this.processor.process(message);
      let chunkCount = 0;

      for await (const chunk of generator) {
        if (abortController.signal.aborted) {
          message.status = MessageStatus.CANCELLED;
          this.emit('message:cancelled', message.id);
          return;
        }

        message.responseChunks!.push(chunk);
        message.response = message.responseChunks!.join('');
        message.progress = Math.min(99, chunkCount * 2);
        chunkCount++;

        this.emit('message:chunk', message, chunk);
        this.emit('message:progress', message);
      }

      message.status = MessageStatus.COMPLETED;
      message.progress = 100;
      message.timestamps.processingEnded = Date.now();
      message.timestamps.completed = Date.now();

      const processingTime = message.timestamps.processingEnded - message.timestamps.processingStarted!;
      this.metrics.histogram('queue.processing_time', processingTime);
      this.metrics.increment('queue.completed');

      this.logger.info('Message completed', {
        messageId: message.id,
        processingTime,
        responseLength: message.response.length
      });

      this.emit('message:completed', message);

    } catch (error: any) {
      await this.handleProcessingError(message, error);
    } finally {
      this.abortControllers.delete(message.id);
      this.activeMessages.delete(message.id);
      this.processQueue();
    }
  }

  private async handleProcessingError(message: QueuedMessage, error: Error): Promise<void> {
    this.logger.error('Message processing failed', error, { messageId: message.id });

    const isRetryable = this.isRetryableError(error);

    if (isRetryable && message.retryCount < this.config.queue.retryAttempts) {
      message.retryCount++;
      message.status = MessageStatus.QUEUED;
      message.error = {
        code: 'PROCESSING_ERROR',
        message: error.message,
        stack: error.stack || undefined,
        retryable: true,
      };

      this.logger.info('Retrying message', {
        messageId: message.id,
        attempt: message.retryCount,
        maxAttempts: this.config.queue.retryAttempts
      });

      await this.delay(this.config.queue.retryDelay * message.retryCount);

      this.activeMessages.delete(message.id);
      this.insertByPriority(message);
      this.queue.push(message);
      this.processQueue();

    } else {
      message.status = MessageStatus.FAILED;
      message.error = {
        code: 'PROCESSING_ERROR',
        message: error.message,
        stack: error.stack || undefined,
        retryable: false,
      };
      message.timestamps.processingEnded = Date.now();

      this.metrics.increment('queue.failed');
      this.emit('message:failed', message, error);
    }
  }

  private isRetryableError(error: Error): boolean {
    const retryableCodes = ['ECONNRESET', 'ENOTFOUND', 'ETIMEDOUT', 'ECONNREFUSED'];
    const retryableMessages = ['rate limit', 'timeout', 'temporarily unavailable'];

    if (retryableCodes.includes((error as any).code)) return true;
    if (retryableMessages.some(msg => error.message.toLowerCase().includes(msg))) return true;

    return false;
  }

  private delay(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  getMessage(messageId: string): QueuedMessage | undefined {
    return this.messageStore.get(messageId);
  }

  getMessages(status?: MessageStatus): QueuedMessage[] {
    const messages = Array.from(this.messageStore.values());
    if (status) {
      return messages.filter(m => m.status === status);
    }
    return messages;
  }

  getQueue(): QueuedMessage[] {
    return [...this.queue];
  }

  getActive(): QueuedMessage[] {
    return Array.from(this.activeMessages.values());
  }

  getStats(): { pending: number; active: number; total: number } {
    return {
      pending: this.queue.length,
      active: this.activeMessages.size,
      total: this.messageStore.size,
    };
  }

  cancel(messageId: string): boolean {
    const abortController = this.abortControllers.get(messageId);
    if (abortController) {
      abortController.abort();
      return true;
    }

    const index = this.queue.findIndex(m => m.id === messageId);
    if (index !== -1) {
      const message = this.queue.splice(index, 1)[0];
      if (message) {
        message.status = MessageStatus.CANCELLED;
        this.emit('message:cancelled', message.id);
        this.metrics.increment('queue.cancelled');
      }
      return true;
    }

    return false;
  }

  cancelAll(): void {
    this.queue.forEach(message => {
      message.status = MessageStatus.CANCELLED;
      this.emit('message:cancelled', message.id);
    });
    this.queue = [];

    this.abortControllers.forEach((controller) => {
      controller.abort();
    });

    this.metrics.gauge('queue.pending', 0);
    this.logger.info('All messages cancelled');
  }

  prioritize(messageId: string, priority: MessagePriority): boolean {
    const message = this.messageStore.get(messageId);
    if (!message) return false;

    message.priority = priority;

    if (message.status === MessageStatus.QUEUED || message.status === MessageStatus.PENDING) {
      const index = this.queue.findIndex(m => m.id === messageId);
      if (index !== -1) {
        this.queue.splice(index, 1);
        this.insertByPriority(message);
      }
    }

    return true;
  }

  pause(): void {
    this.paused = true;
    this.logger.info('Queue paused');
    this.emit('queue:paused');
  }

  resume(): void {
    this.paused = false;
    this.logger.info('Queue resumed');
    this.emit('queue:resumed');
    this.processQueue();
  }

  private loadPersistedQueue(): void {
    if (!this.config.queue.persistenceEnabled) return;

    try {
      if (typeof localStorage === 'undefined') return;

      const stored = localStorage.getItem('messageQueue');
      if (!stored) return;

      const data = JSON.parse(stored);
      const messages: QueuedMessage[] = data.messages || [];

      messages.forEach((msg: QueuedMessage) => {
        if (msg.status === MessageStatus.QUEUED || msg.status === MessageStatus.PENDING) {
          msg.status = MessageStatus.QUEUED;
          this.insertByPriority(msg);
          this.messageStore.set(msg.id, msg);
        }
      });

      this.logger.info('Loaded persisted queue', { count: this.queue.length });

    } catch (error) {
      this.logger.error('Failed to load persisted queue', error as Error);
    }
  }

  private startPersistenceTimer(): void {
    if (!this.config.queue.persistenceEnabled) return;

    this.persistenceTimer = setInterval(() => {
      this.persistQueue();
    }, this.config.queue.persistenceInterval);
  }

  private persistQueue(): void {
    if (typeof localStorage === 'undefined') return;

    try {
      const data = {
        messages: this.queue,
        timestamp: Date.now(),
      };
      localStorage.setItem('messageQueue', JSON.stringify(data));
    } catch (error) {
      this.logger.error('Failed to persist queue', error as Error);
    }
  }

  private startHealthCheck(): void {
    this.healthCheckTimer = setInterval(() => {
      const stuckMessages = Array.from(this.activeMessages.values()).filter(msg => {
        const elapsed = Date.now() - (msg.timestamps.processingStarted || 0);
        return elapsed > this.config.queue.processingTimeout;
      });

      stuckMessages.forEach(msg => {
        this.logger.warn('Stuck message detected, cancelling', { messageId: msg.id });
        msg.status = MessageStatus.TIMEOUT;
        msg.error = {
          code: 'TIMEOUT',
          message: 'Message processing exceeded timeout',
          retryable: true,
        };
        this.cancel(msg.id);
      });

      this.metrics.gauge('queue.active', this.activeMessages.size);
      this.metrics.gauge('queue.pending', this.queue.length);

    }, 10000);
  }

  private setupGracefulShutdown(): void {
    const shutdown = async () => {
      await this.shutdown();
    };

    if (typeof process !== 'undefined' && process.on) {
      process.on('SIGTERM', shutdown);
      process.on('SIGINT', shutdown);
    }
  }

  async shutdown(): Promise<void> {
    this.logger.info('Shutting down message queue');
    this.shuttingDown = true;

    if (this.persistenceTimer) clearInterval(this.persistenceTimer);
    if (this.healthCheckTimer) clearInterval(this.healthCheckTimer);

    this.abortControllers.forEach((controller) => {
      controller.abort();
    });

    const timeout = 5000;
    const start = Date.now();
    while (this.activeMessages.size > 0 && Date.now() - start < timeout) {
      await this.delay(100);
    }

    this.persistQueue();

    this.logger.info('Message queue shutdown complete');
  }
}
