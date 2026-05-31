import { EventEmitter } from 'events';

export interface MetricValue {
  name: string;
  value: number;
  timestamp: number;
  tags?: Record<string, string> | undefined;
}

export interface QueueMetrics {
  totalMessages: number;
  pendingMessages: number;
  processingMessages: number;
  completedMessages: number;
  failedMessages: number;
  averageProcessingTime: number;
  queueWaitTime: number;
  throughputPerMinute: number;
}

export class MetricsCollector extends EventEmitter {
  private static instance: MetricsCollector;
  private metrics: Map<string, MetricValue[]> = new Map();
  private counters: Map<string, number> = new Map();
  private gauges: Map<string, number> = new Map();
  private histograms: Map<string, number[]> = new Map();
  private maxRetention: number = 1000;

  private constructor() {
    super();
    this.startCollection();
  }

  static getInstance(): MetricsCollector {
    if (!MetricsCollector.instance) {
      MetricsCollector.instance = new MetricsCollector();
    }
    return MetricsCollector.instance;
  }

  increment(name: string, value: number = 1, tags?: Record<string, string>): void {
    const current = this.counters.get(name) || 0;
    this.counters.set(name, current + value);
    this.recordMetric(name, current + value, 'counter', tags);
  }

  decrement(name: string, value: number = 1, tags?: Record<string, string>): void {
    const current = this.counters.get(name) || 0;
    this.counters.set(name, Math.max(0, current - value));
    this.recordMetric(name, this.counters.get(name)!, 'counter', tags);
  }

  gauge(name: string, value: number, tags?: Record<string, string>): void {
    this.gauges.set(name, value);
    this.recordMetric(name, value, 'gauge', tags);
  }

  histogram(name: string, value: number, tags?: Record<string, string>): void {
    if (!this.histograms.has(name)) {
      this.histograms.set(name, []);
    }
    const values = this.histograms.get(name)!;
    values.push(value);
    if (values.length > this.maxRetention) {
      values.shift();
    }
    this.recordMetric(name, value, 'histogram', tags);
  }

  timing(name: string, durationMs: number, tags?: Record<string, string>): void {
    this.histogram(name, durationMs, tags);
  }

  private recordMetric(name: string, value: number, _type: string, tags?: Record<string, string>): void {
    const metric: MetricValue = {
      name,
      value,
      timestamp: Date.now(),
      tags: tags ?? undefined,
    };

    if (!this.metrics.has(name)) {
      this.metrics.set(name, []);
    }

    const values = this.metrics.get(name)!;
    values.push(metric);

    if (values.length > this.maxRetention) {
      values.shift();
    }

    this.emit('metric', metric);
  }

  getCounter(name: string): number {
    return this.counters.get(name) || 0;
  }

  getGauge(name: string): number {
    return this.gauges.get(name) || 0;
  }

  getHistogramStats(name: string): { min: number; max: number; avg: number; p50: number; p95: number; p99: number } {
    const values = this.histograms.get(name) || [];
    if (values.length === 0) {
      return { min: 0, max: 0, avg: 0, p50: 0, p95: 0, p99: 0 };
    }

    const sorted = [...values].sort((a, b) => a - b);
    return {
      min: sorted[0] ?? 0,
      max: sorted[sorted.length - 1] ?? 0,
      avg: values.reduce((a, b) => a + b, 0) / values.length,
      p50: sorted[Math.floor(sorted.length * 0.5)] ?? 0,
      p95: sorted[Math.floor(sorted.length * 0.95)] ?? 0,
      p99: sorted[Math.floor(sorted.length * 0.99)] ?? 0,
    };
  }

  private startCollection(): void {
    setInterval(() => {
      this.emit('snapshot', this.getSnapshot());
    }, 60000);
  }

  getSnapshot(): Map<string, MetricValue[]> {
    return new Map(this.metrics);
  }

  getQueueMetrics(): QueueMetrics {
    return {
      totalMessages: this.getCounter('queue.total'),
      pendingMessages: this.getGauge('queue.pending'),
      processingMessages: this.getGauge('queue.processing'),
      completedMessages: this.getCounter('queue.completed'),
      failedMessages: this.getCounter('queue.failed'),
      averageProcessingTime: this.getHistogramStats('queue.processing_time').avg,
      queueWaitTime: this.getHistogramStats('queue.wait_time').avg,
      throughputPerMinute: this.calculateThroughput(),
    };
  }

  private calculateThroughput(): number {
    const values = this.metrics.get('queue.completed') || [];
    const oneMinuteAgo = Date.now() - 60000;
    const recent = values.filter(v => v.timestamp > oneMinuteAgo);
    return recent.length;
  }

  reset(): void {
    this.metrics.clear();
    this.counters.clear();
    this.gauges.clear();
    this.histograms.clear();
  }
}

export const metrics = MetricsCollector.getInstance();
