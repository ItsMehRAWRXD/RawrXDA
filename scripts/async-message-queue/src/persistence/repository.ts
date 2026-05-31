import { promises as fs } from 'fs';
import * as path from 'path';
import { QueuedMessage, MessageStatus } from '../queue/message-queue';
import { Logger, logger } from '../logger';

export interface PersistenceAdapter {
  save(message: QueuedMessage): Promise<void>;
  load(id: string): Promise<QueuedMessage | null>;
  loadAll(status?: MessageStatus): Promise<QueuedMessage[]>;
  delete(id: string): Promise<void>;
  clear(): Promise<void>;
}

export class FileSystemPersistence implements PersistenceAdapter {
  private basePath: string;
  private logger: Logger = logger;

  constructor(basePath: string = './data/messages') {
    this.basePath = basePath;
    this.ensureDirectory();
  }

  private async ensureDirectory(): Promise<void> {
    try {
      await fs.mkdir(this.basePath, { recursive: true });
    } catch (error) {
      this.logger.error('Failed to create persistence directory', error as Error);
    }
  }

  async save(message: QueuedMessage): Promise<void> {
    const filePath = this.getFilePath(message.id);
    try {
      await fs.writeFile(filePath, JSON.stringify(message, null, 2));
      this.logger.trace('Message saved', { messageId: message.id });
    } catch (error) {
      this.logger.error('Failed to save message', error as Error, { messageId: message.id });
      throw error;
    }
  }

  async load(id: string): Promise<QueuedMessage | null> {
    const filePath = this.getFilePath(id);
    try {
      const data = await fs.readFile(filePath, 'utf-8');
      return JSON.parse(data);
    } catch (error: any) {
      if (error.code === 'ENOENT') {
        return null;
      }
      this.logger.error('Failed to load message', error, { messageId: id });
      throw error;
    }
  }

  async loadAll(status?: MessageStatus): Promise<QueuedMessage[]> {
    try {
      const files = await fs.readdir(this.basePath);
      const messages: QueuedMessage[] = [];

      for (const file of files) {
        if (!file.endsWith('.json')) continue;

        const id = file.replace('.json', '');
        const message = await this.load(id);

        if (message && (!status || message.status === status)) {
          messages.push(message);
        }
      }

      return messages;
    } catch (error: any) {
      if (error.code === 'ENOENT') {
        return [];
      }
      throw error;
    }
  }

  async delete(id: string): Promise<void> {
    const filePath = this.getFilePath(id);
    try {
      await fs.unlink(filePath);
      this.logger.trace('Message deleted', { messageId: id });
    } catch (error: any) {
      if (error.code !== 'ENOENT') {
        throw error;
      }
    }
  }

  async clear(): Promise<void> {
    try {
      const files = await fs.readdir(this.basePath);
      await Promise.all(
        files
          .filter(f => f.endsWith('.json'))
          .map(f => fs.unlink(path.join(this.basePath, f)))
      );
      this.logger.info('Persistence cleared');
    } catch (error: any) {
      if (error.code !== 'ENOENT') {
        throw error;
      }
    }
  }

  private getFilePath(id: string): string {
    return path.join(this.basePath, `${id}.json`);
  }
}

export class IndexedDBPersistence implements PersistenceAdapter {
  private dbName: string = 'message-queue-db';
  private storeName: string = 'messages';
  private db: IDBDatabase | null = null;
  private logger: Logger = logger;

  async init(): Promise<void> {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, 1);

      request.onerror = () => {
        this.logger.error('Failed to open IndexedDB', new Error(request.error?.message || 'Unknown error'));
        reject(request.error);
      };

      request.onsuccess = () => {
        this.db = request.result;
        this.logger.info('IndexedDB initialized');
        resolve();
      };

      request.onupgradeneeded = (event: any) => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains(this.storeName)) {
          const store = db.createObjectStore(this.storeName, { keyPath: 'id' });
          store.createIndex('status', 'status', { unique: false });
          store.createIndex('priority', 'priority', { unique: false });
          store.createIndex('timestamp', 'timestamps.created', { unique: false });
        }
      };
    });
  }

  async save(message: QueuedMessage): Promise<void> {
    if (!this.db) await this.init();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.put(message);

      request.onsuccess = () => resolve();
      request.onerror = () => {
        this.logger.error('Failed to save to IndexedDB', new Error(request.error?.message || 'Unknown error'));
        reject(request.error);
      };
    });
  }

  async load(id: string): Promise<QueuedMessage | null> {
    if (!this.db) await this.init();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readonly');
      const store = transaction.objectStore(this.storeName);
      const request = store.get(id);

      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  }

  async loadAll(status?: MessageStatus): Promise<QueuedMessage[]> {
    if (!this.db) await this.init();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readonly');
      const store = transaction.objectStore(this.storeName);
      const request = status
        ? store.index('status').getAll(status)
        : store.getAll();

      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }

  async delete(id: string): Promise<void> {
    if (!this.db) await this.init();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.delete(id);

      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }

  async clear(): Promise<void> {
    if (!this.db) await this.init();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction([this.storeName], 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.clear();

      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }
}
