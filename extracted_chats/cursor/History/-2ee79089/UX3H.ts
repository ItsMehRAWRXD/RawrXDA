// BigDaddyGEngine/ui/metricsUtils.ts
// Metrics Calculation Utilities for Insight Visualization

export interface MetricData {
  timestamp: number;
  value: number;
  label?: string;
}

export interface ROIData {
  revenue: number[];
  cost: number[];
  timestamp: number[];
}

export interface DriftData {
  metrics: number[];
  timestamp: number[];
  baseline?: number;
}

export interface TrendData {
  values: number[];
  timestamp: number[];
  period: 'hour' | 'day' | 'week' | 'month';
}

// ROI Calculations
export function calculateROI(revenue: number[], cost: number[]): number {
  if (revenue.length !== cost.length) {
    throw new Error('Revenue and cost arrays must have the same length');
  }
  
  const totalRevenue = revenue.reduce((sum, val) => sum + val, 0);
  const totalCost = cost.reduce((sum, val) => sum + val, 0);
  
  if (totalCost === 0) return 0;
  
  return ((totalRevenue - totalCost) / totalCost) * 100;
}

export function calculateROIOverTime(data: ROIData): number[] {
  const rois: number[] = [];
  
  for (let i = 0; i < data.revenue.length; i++) {
    const revenue = data.revenue[i];
    const cost = data.cost[i];
    const roi = cost === 0 ? 0 : ((revenue - cost) / cost) * 100;
    rois.push(roi);
  }
  
  return rois;
}

export function calculateROITrend(data: ROIData): {
  current: number;
  previous: number;
  change: number;
  trend: 'increasing' | 'decreasing' | 'stable';
} {
  const rois = calculateROIOverTime(data);
  
  if (rois.length < 2) {
    return {
      current: rois[0] || 0,
      previous: 0,
      change: 0,
      trend: 'stable'
    };
  }
  
  const current = rois[rois.length - 1];
  const previous = rois[rois.length - 2];
  const change = current - previous;
  
  let trend: 'increasing' | 'decreasing' | 'stable' = 'stable';
  if (Math.abs(change) > 0.01) {
    trend = change > 0 ? 'increasing' : 'decreasing';
  }
  
  return { current, previous, change, trend };
}

// Drift Calculations
export function calculateDrift(metrics: number[]): number {
  if (metrics.length === 0) return 0;
  
  const mean = metrics.reduce((sum, val) => sum + val, 0) / metrics.length;
  const variance = metrics.reduce((sum, val) => sum + Math.pow(val - mean, 2), 0) / metrics.length;
  
  return Math.sqrt(variance);
}

export function calculateDriftOverTime(data: DriftData): number[] {
  const drifts: number[] = [];
  const windowSize = Math.min(10, Math.floor(data.metrics.length / 4));
  
  for (let i = windowSize; i < data.metrics.length; i++) {
    const window = data.metrics.slice(i - windowSize, i);
    const drift = calculateDrift(window);
    drifts.push(drift);
  }
  
  return drifts;
}

export function calculateDriftTrend(data: DriftData): {
  current: number;
  average: number;
  trend: 'increasing' | 'decreasing' | 'stable';
  severity: 'low' | 'medium' | 'high' | 'critical';
} {
  const drifts = calculateDriftOverTime(data);
  
  if (drifts.length === 0) {
    return {
      current: 0,
      average: 0,
      trend: 'stable',
      severity: 'low'
    };
  
  const current = drifts[drifts.length - 1] || 0;
  const average = drifts.reduce((sum, val) => sum + val, 0) / drifts.length;
  const trend = current > average ? 'increasing' : current < average ? 'decreasing' : 'stable';
  
  let severity: 'low' | 'medium' | 'high' | 'critical' = 'low';
  if (current > 0.8) severity = 'critical';
  else if (current > 0.6) severity = 'high';
  else if (current > 0.3) severity = 'medium';
  
  return { current, average, trend, severity };
}

// Trend Analysis
export function calculateTrend(data: TrendData): {
  slope: number;
  correlation: number;
  trend: 'increasing' | 'decreasing' | 'stable';
  confidence: number;
} {
  if (data.values.length < 2) {
    return { slope: 0, correlation: 0, trend: 'stable', confidence: 0 };
  }
  
  const n = data.values.length;
  const x = data.timestamp.map((t, i) => i);
  const y = data.values;
  
  // Calculate means
  const xMean = x.reduce((sum, val) => sum + val, 0) / n;
  const yMean = y.reduce((sum, val) => sum + val, 0) / n;
  
  // Calculate slope (m) and correlation (r)
  let numerator = 0;
  let xSumSquares = 0;
  let ySumSquares = 0;
  
  for (let i = 0; i < n; i++) {
    const xDiff = x[i] - xMean;
    const yDiff = y[i] - yMean;
    
    numerator += xDiff * yDiff;
    xSumSquares += xDiff * xDiff;
    ySumSquares += yDiff * yDiff;
  }
  
  const slope = xSumSquares === 0 ? 0 : numerator / xSumSquares;
  const correlation = Math.sqrt(xSumSquares * ySumSquares) === 0 ? 0 : 
    numerator / Math.sqrt(xSumSquares * ySumSquares);
  
  let trend: 'increasing' | 'decreasing' | 'stable' = 'stable';
  if (Math.abs(slope) > 0.01) {
    trend = slope > 0 ? 'increasing' : 'decreasing';
  }
  
  const confidence = Math.abs(correlation);
  
  return { slope, correlation, trend, confidence };
}

// Performance Metrics
export function calculatePerformanceMetrics(data: MetricData[]): {
  average: number;
  median: number;
  min: number;
  max: number;
  standardDeviation: number;
  percentile95: number;
  percentile99: number;
} {
  if (data.length === 0) {
    return {
      average: 0,
      median: 0,
      min: 0,
      max: 0,
      standardDeviation: 0,
      percentile95: 0,
      percentile99: 0
    };
  }
  
  const values = data.map(d => d.value).sort((a, b) => a - b);
  const n = values.length;
  
  const average = values.reduce((sum, val) => sum + val, 0) / n;
  const median = n % 2 === 0 ? 
    (values[n / 2 - 1] + values[n / 2]) / 2 : 
    values[Math.floor(n / 2)];
  
  const min = values[0];
  const max = values[n - 1];
  
  const variance = values.reduce((sum, val) => sum + Math.pow(val - average, 2), 0) / n;
  const standardDeviation = Math.sqrt(variance);
  
  const percentile95 = values[Math.floor(n * 0.95)];
  const percentile99 = values[Math.floor(n * 0.99)];
  
  return {
    average,
    median,
    min,
    max,
    standardDeviation,
    percentile95,
    percentile99
  };
}

// Anomaly Detection
export function detectAnomalies(data: MetricData[], threshold: number = 2): MetricData[] {
  if (data.length < 3) return [];
  
  const values = data.map(d => d.value);
  const average = values.reduce((sum, val) => sum + val, 0) / values.length;
  const standardDeviation = Math.sqrt(
    values.reduce((sum, val) => sum + Math.pow(val - average, 2), 0) / values.length
  );
  
  return data.filter(d => Math.abs(d.value - average) > threshold * standardDeviation);
}

// Forecasting
export function forecastTrend(data: TrendData, periods: number = 7): number[] {
  if (data.values.length < 2) return [];
  
  const trend = calculateTrend(data);
  const lastValue = data.values[data.values.length - 1];
  const lastTimestamp = data.timestamp[data.timestamp.length - 1];
  
  const forecast: number[] = [];
  const timeStep = data.timestamp.length > 1 ? 
    data.timestamp[1] - data.timestamp[0] : 
    24 * 60 * 60 * 1000; // Default to 1 day
  
  for (let i = 1; i <= periods; i++) {
    const predictedValue = lastValue + (trend.slope * i);
    forecast.push(Math.max(0, predictedValue)); // Ensure non-negative
  }
  
  return forecast;
}

// Utility functions
export function formatNumber(value: number, decimals: number = 2): string {
  return value.toFixed(decimals);
}

export function formatPercentage(value: number, decimals: number = 1): string {
  return `${(value * 100).toFixed(decimals)}%`;
}

export function formatCurrency(value: number, currency: string = '$'): string {
  return `${currency}${value.toLocaleString()}`;
}

export function formatDuration(ms: number): string {
  const seconds = Math.floor(ms / 1000);
  const minutes = Math.floor(seconds / 60);
  const hours = Math.floor(minutes / 60);
  const days = Math.floor(hours / 24);
  
  if (days > 0) return `${days}d ${hours % 24}h`;
  if (hours > 0) return `${hours}h ${minutes % 60}m`;
  if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
  return `${seconds}s`;
}

export function getColorForValue(value: number, thresholds: { low: number; medium: number; high: number }): string {
  if (value <= thresholds.low) return '#10b981'; // green
  if (value <= thresholds.medium) return '#f59e0b'; // yellow
  if (value <= thresholds.high) return '#f97316'; // orange
  return '#ef4444'; // red
}

export function getTrendIcon(trend: 'increasing' | 'decreasing' | 'stable'): string {
  switch (trend) {
    case 'increasing': return '↗️';
    case 'decreasing': return '↘️';
    case 'stable': return '→';
    default: return '→';
  }
}
