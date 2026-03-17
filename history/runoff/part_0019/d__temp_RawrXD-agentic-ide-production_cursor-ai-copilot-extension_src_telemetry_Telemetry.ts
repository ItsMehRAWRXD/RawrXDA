import * as vscode from 'vscode';
import { Logger } from '../utils/Logger';

export interface TelemetryEvent {
  name: string;
  properties?: { [key: string]: string };
  measurements?: { [key: string]: number };
}

export class Telemetry implements vscode.Disposable {
  private logger: Logger;
  private context: vscode.ExtensionContext;
  private enabledTelemetry: boolean;
  private events: TelemetryEvent[] = [];
  private readonly maxEvents = 100;

  constructor(context: vscode.ExtensionContext) {
    this.logger = new Logger('Telemetry');
    this.context = context;

    const config = vscode.workspace.getConfiguration('cursor-ai-copilot');
    this.enabledTelemetry = config.get<boolean>('enableTelemetry', true);

    this.logger.info(`Telemetry ${this.enabledTelemetry ? 'enabled' : 'disabled'}`);
  }

  trackCommand(commandName: string): void {
    this.trackEvent({
      name: 'command',
      properties: { command: commandName },
      measurements: { timestamp: Date.now() }
    });
  }

  trackFeatureUsage(feature: string, dataSize: number = 0): void {
    this.trackEvent({
      name: 'feature_usage',
      properties: { feature },
      measurements: { dataSize, timestamp: Date.now() }
    });
  }

  trackError(error: Error, context: string): void {
    this.trackEvent({
      name: 'error',
      properties: {
        context,
        message: error.message,
        stack: error.stack || 'no stack'
      },
      measurements: { timestamp: Date.now() }
    });
  }

  private trackEvent(event: TelemetryEvent): void {
    if (!this.enabledTelemetry) {
      return;
    }

    this.events.push(event);
    this.logger.debug(`Event tracked: ${event.name}`, event.properties);

    // Keep events list manageable
    if (this.events.length > this.maxEvents) {
      this.events = this.events.slice(-this.maxEvents);
    }
  }

  getEvents(): TelemetryEvent[] {
    return [...this.events];
  }

  clearEvents(): void {
    this.events = [];
    this.logger.debug('Telemetry events cleared');
  }

  dispose(): void {
    // Could send remaining events to analytics service here
    this.clearEvents();
    this.logger.info('Telemetry disposed');
  }
}
