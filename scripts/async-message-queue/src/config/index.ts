import { z } from 'zod';

declare const process: {
  env: Record<string, string | undefined>;
};

const ConfigSchema = z.object({
  queue: z.object({
    maxConcurrent: z.number().min(1).max(10).default(3),
    maxQueueSize: z.number().min(1).max(1000).default(100),
    processingTimeout: z.number().min(1000).max(300000).default(60000),
    retryAttempts: z.number().min(0).max(5).default(3),
    retryDelay: z.number().min(100).max(10000).default(1000),
    persistenceEnabled: z.boolean().default(true),
    persistenceInterval: z.number().min(1000).max(60000).default(5000),
  }),
  api: z.object({
    baseUrl: z.string().url(),
    apiKey: z.string().min(1),
    streamEndpoint: z.string().default('/v1/chat/completions'),
    timeout: z.number().default(120000),
  }),
  ui: z.object({
    maxVisibleMessages: z.number().default(50),
    notificationSound: z.boolean().default(true),
    autoFocusEditor: z.boolean().default(true),
    markdownRendering: z.boolean().default(true),
  }),
});

export type Config = z.infer<typeof ConfigSchema>;

export class ConfigManager {
  private static instance: ConfigManager;
  private config: Config;

  private constructor() {
    this.config = this.loadConfig();
  }

  static getInstance(): ConfigManager {
    if (!ConfigManager.instance) {
      ConfigManager.instance = new ConfigManager();
    }
    return ConfigManager.instance;
  }

  private loadConfig(): Config {
    const env = process.env;
    const envConfig = {
      queue: {
        maxConcurrent: parseInt(env['QUEUE_MAX_CONCURRENT'] || '3'),
        maxQueueSize: parseInt(env['QUEUE_MAX_SIZE'] || '100'),
        processingTimeout: parseInt(env['QUEUE_TIMEOUT'] || '60000'),
        retryAttempts: parseInt(env['QUEUE_RETRY_ATTEMPTS'] || '3'),
        retryDelay: parseInt(env['QUEUE_RETRY_DELAY'] || '1000'),
        persistenceEnabled: env['QUEUE_PERSISTENCE'] !== 'false',
        persistenceInterval: parseInt(env['QUEUE_PERSIST_INTERVAL'] || '5000'),
      },
      api: {
        baseUrl: env['API_BASE_URL'] || 'https://api.openai.com',
        apiKey: env['API_KEY'] || '',
        streamEndpoint: env['API_STREAM_ENDPOINT'] || '/v1/chat/completions',
        timeout: parseInt(env['API_TIMEOUT'] || '120000'),
      },
      ui: {
        maxVisibleMessages: parseInt(env['UI_MAX_MESSAGES'] || '50'),
        notificationSound: env['UI_SOUND'] !== 'false',
        autoFocusEditor: env['UI_AUTO_FOCUS'] !== 'false',
        markdownRendering: env['UI_MARKDOWN'] !== 'false',
      },
    };

    return ConfigSchema.parse(envConfig);
  }

  get(): Readonly<Config> {
    return Object.freeze({ ...this.config });
  }

  update(partial: Partial<Config>): void {
    this.config = ConfigSchema.parse({ ...this.config, ...partial });
    this.persist();
  }

  private persist(): void {
    if (typeof localStorage !== 'undefined') {
      localStorage.setItem('app:config', JSON.stringify(this.config));
    }
  }
}
