export enum LogLevel {
  TRACE = 'trace',
  DEBUG = 'debug',
  INFO = 'info',
  WARN = 'warn',
  ERROR = 'error',
  FATAL = 'fatal',
}

export interface LogEntry {
  timestamp: string;
  level: LogLevel;
  message: string;
  context?: Record<string, unknown> | undefined;
  error?: Error | undefined;
  traceId?: string | undefined;
}

export class Logger {
  private static instance: Logger;
  private logBuffer: LogEntry[] = [];
  private maxBufferSize: number = 1000;
  private level: LogLevel;

  private constructor(level: LogLevel = LogLevel.INFO) {
    this.level = level;
  }

  static getInstance(level?: LogLevel): Logger {
    if (!Logger.instance) {
      Logger.instance = new Logger(level);
    }
    return Logger.instance;
  }

  private shouldLog(level: LogLevel): boolean {
    const levels = [LogLevel.TRACE, LogLevel.DEBUG, LogLevel.INFO, LogLevel.WARN, LogLevel.ERROR, LogLevel.FATAL];
    return levels.indexOf(level) >= levels.indexOf(this.level);
  }

  private log(level: LogLevel, message: string, context?: Record<string, unknown>): void {
    if (!this.shouldLog(level)) return;

    const entry: LogEntry = {
      timestamp: new Date().toISOString(),
      level,
      message,
      context: context ?? undefined,
    };

    this.logBuffer.push(entry);
    if (this.logBuffer.length > this.maxBufferSize) {
      this.logBuffer.shift();
    }

    // Console output for now (pino would be used in production)
    const prefix = `[${entry.timestamp}] ${level.toUpperCase()}:`;
    if (context) {
      console.log(prefix, message, context);
    } else {
      console.log(prefix, message);
    }
  }

  trace(message: string, context?: Record<string, unknown>): void {
    this.log(LogLevel.TRACE, message, context);
  }

  debug(message: string, context?: Record<string, unknown>): void {
    this.log(LogLevel.DEBUG, message, context);
  }

  info(message: string, context?: Record<string, unknown>): void {
    this.log(LogLevel.INFO, message, context);
  }

  warn(message: string, context?: Record<string, unknown>): void {
    this.log(LogLevel.WARN, message, context);
  }

  error(message: string, error?: Error, context?: Record<string, unknown>): void {
    this.log(LogLevel.ERROR, message, { ...context, error: error?.message, stack: error?.stack });
  }

  fatal(message: string, error?: Error, context?: Record<string, unknown>): void {
    this.log(LogLevel.FATAL, message, { ...context, error: error?.message, stack: error?.stack });
  }

  getBuffer(): LogEntry[] {
    return [...this.logBuffer];
  }

  flushToFile(filename: string): void {
    const fs = require('fs');
    const path = require('path');
    const logDir = path.join('.', 'logs');
    const logPath = path.join(logDir, filename);
    fs.mkdirSync(logDir, { recursive: true });
    fs.writeFileSync(logPath, JSON.stringify(this.logBuffer, null, 2));
  }
}

declare const process: { env: Record<string, string | undefined> };
export const logger = Logger.getInstance(process.env['LOG_LEVEL'] as LogLevel || LogLevel.INFO);
