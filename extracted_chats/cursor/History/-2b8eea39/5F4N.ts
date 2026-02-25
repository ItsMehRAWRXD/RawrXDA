// BigDaddyGEngine/utils/MetricsTracker.ts

export class MetricsTracker {
  static record(metric: string, value: number): void {
    console.log(`[METRIC] ${metric}: ${value}`);
  }
}
