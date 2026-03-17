export class Logger {
  private context: string;

  constructor(context: string) {
    this.context = context;
  }

  private formatMessage(level: string, message: string, data?: any): string {
    const timestamp = new Date().toISOString();
    let formatted = `[${timestamp}] [${this.context}] [${level}] ${message}`;

    if (data) {
      try {
        formatted += ` ${JSON.stringify(data)}`;
      } catch {
        formatted += ` ${String(data)}`;
      }
    }

    return formatted;
  }

  debug(message: string, data?: any): void {
    console.debug(this.formatMessage('DEBUG', message, data));
  }

  info(message: string, data?: any): void {
    console.log(this.formatMessage('INFO', message, data));
  }

  warn(message: string, data?: any): void {
    console.warn(this.formatMessage('WARN', message, data));
  }

  error(message: string, error?: any): void {
    const errorMessage = error instanceof Error ? error.message : String(error);
    console.error(this.formatMessage('ERROR', message, errorMessage));
  }
}
