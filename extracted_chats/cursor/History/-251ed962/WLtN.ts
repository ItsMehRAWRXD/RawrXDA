// BigDaddyGEngine/analytics/PerformanceMetrics.ts

export class PerformanceMetrics {
  private data: Record<string, number[]> = {};

  record(metric: string, value: number): void {
    if (!this.data[metric]) this.data[metric] = [];
    this.data[metric].push(value);
  }

  getSummary(): Record<string, number> {
    const summary: Record<string, number> = {};
    for (const [key, values] of Object.entries(this.data)) {
      summary[key] = values.reduce((a, b) => a + b, 0) / values.length;
    }
    return summary;
  }
}
