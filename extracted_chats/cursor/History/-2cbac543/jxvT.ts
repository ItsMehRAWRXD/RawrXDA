/**
 * Dynamic Context Window Manager
 * Adaptive context window scaling based on memory availability and performance
 */

import { EventEmitter } from 'events';
import { configManager } from '../BigDaddyGEngine.config';

export interface ContextWindowConfig {
  minTokens: number;
  maxTokens: number;
  defaultTokens: number;
  memoryThresholds: MemoryThreshold[];
  compressionEnabled: boolean;
  streamingEnabled: boolean;
  adaptiveScaling: boolean;
}

export interface MemoryThreshold {
  memoryMB: number;
  contextTokens: number;
  compressionLevel: number;
  streamingChunkSize: number;
}

export interface ContextWindowState {
  currentTokens: number;
  maxTokens: number;
  memoryUsageMB: number;
  compressionRatio: number;
  streamingActive: boolean;
  performanceScore: number;
}

export interface TokenChunk {
  id: string;
  tokens: string[];
  startIndex: number;
  endIndex: number;
  compressed: boolean;
  memoryFootprint: number;
  priority: number;
}

export class ContextWindowManager extends EventEmitter {
  private config: ContextWindowConfig;
  private state: ContextWindowState;
  private memoryMonitor: MemoryMonitor;
  private compressionEngine: CompressionEngine;
  private streamingEngine: StreamingEngine;
  private performanceTracker: PerformanceTracker;

  constructor() {
    super();
    this.config = this.initializeConfig();
    this.state = this.initializeState();
    this.memoryMonitor = new MemoryMonitor();
    this.compressionEngine = new CompressionEngine();
    this.streamingEngine = new StreamingEngine();
    this.performanceTracker = new PerformanceTracker();
    
    this.setupEventHandlers();
    this.startAdaptiveScaling();
  }

  private initializeConfig(): ContextWindowConfig {
    return {
      minTokens: 1024,
      maxTokens: 512_000,
      defaultTokens: 8192,
      memoryThresholds: [
        { memoryMB: 256, contextTokens: 1024, compressionLevel: 3, streamingChunkSize: 256 },
        { memoryMB: 512, contextTokens: 4096, compressionLevel: 2, streamingChunkSize: 512 },
        { memoryMB: 1024, contextTokens: 8192, compressionLevel: 1, streamingChunkSize: 1024 },
        { memoryMB: 2048, contextTokens: 32_000, compressionLevel: 0, streamingChunkSize: 2048 },
        { memoryMB: 4096, contextTokens: 128_000, compressionLevel: 0, streamingChunkSize: 4096 },
        { memoryMB: 8192, contextTokens: 512_000, compressionLevel: 0, streamingChunkSize: 8192 }
      ],
      compressionEnabled: true,
      streamingEnabled: true,
      adaptiveScaling: true
    };
  }

  private initializeState(): ContextWindowState {
    return {
      currentTokens: this.config.defaultTokens,
      maxTokens: this.config.maxTokens,
      memoryUsageMB: 0,
      compressionRatio: 1.0,
      streamingActive: false,
      performanceScore: 1.0
    };
  }

  private setupEventHandlers(): void {
    this.memoryMonitor.on('memoryUpdate', (usage: number) => {
      this.updateMemoryUsage(usage);
    });

    this.performanceTracker.on('performanceUpdate', (score: number) => {
      this.updatePerformanceScore(score);
    });

    this.compressionEngine.on('compressionComplete', (ratio: number) => {
      this.updateCompressionRatio(ratio);
    });
  }

  private startAdaptiveScaling(): void {
    if (!this.config.adaptiveScaling) return;

    setInterval(() => {
      this.performAdaptiveScaling();
    }, 1000); // Check every second
  }

  // Core Methods
  public async adjustContextWindow(memoryUsageMB: number, performanceScore: number): Promise<number> {
    const threshold = this.findOptimalThreshold(memoryUsageMB, performanceScore);
    
    if (threshold) {
      const newTokens = threshold.contextTokens;
      const oldTokens = this.state.currentTokens;
      
      if (newTokens !== oldTokens) {
        this.state.currentTokens = newTokens;
        this.state.memoryUsageMB = memoryUsageMB;
        this.state.performanceScore = performanceScore;
        
        this.emit('contextWindowAdjusted', {
          oldTokens,
          newTokens,
          memoryUsageMB,
          performanceScore
        });
        
        console.log(`🧠 Context window adjusted: ${oldTokens} → ${newTokens} tokens (${memoryUsageMB}MB RAM)`);
      }
    }
    
    return this.state.currentTokens;
  }

  public async processPrompt(prompt: string): Promise<TokenChunk[]> {
    const tokens = this.tokenizePrompt(prompt);
    const chunks = this.createTokenChunks(tokens);
    
    if (this.config.compressionEnabled) {
      return await this.compressChunks(chunks);
    }
    
    return chunks;
  }

  public async *streamInference(
    prompt: string,
    model: any,
    options: StreamingOptions = {}
  ): AsyncGenerator<TokenChunk, void, unknown> {
    const chunks = await this.processPrompt(prompt);
    this.state.streamingActive = true;
    
    try {
      for (const chunk of chunks) {
        const result = await this.processChunk(chunk, model, options);
        yield result;
        
        // Check memory pressure during streaming
        const currentMemory = await this.memoryMonitor.getCurrentUsage();
        if (currentMemory > this.getMemoryThreshold() * 0.9) {
          await this.handleMemoryPressure();
        }
      }
    } finally {
      this.state.streamingActive = false;
    }
  }

  public getOptimalChunkSize(): number {
    const threshold = this.findOptimalThreshold(this.state.memoryUsageMB, this.state.performanceScore);
    return threshold?.streamingChunkSize || 1024;
  }

  public getCompressionLevel(): number {
    const threshold = this.findOptimalThreshold(this.state.memoryUsageMB, this.state.performanceScore);
    return threshold?.compressionLevel || 0;
  }

  public getCurrentState(): ContextWindowState {
    return { ...this.state };
  }

  public getMemoryEfficiency(): number {
    const maxPossibleTokens = this.state.maxTokens;
    const currentTokens = this.state.currentTokens;
    const compressionRatio = this.state.compressionRatio;
    
    return (currentTokens * compressionRatio) / maxPossibleTokens;
  }

  // Private Helper Methods
  private findOptimalThreshold(memoryMB: number, performanceScore: number): MemoryThreshold | null {
    const sortedThresholds = [...this.config.memoryThresholds].sort((a, b) => a.memoryMB - b.memoryMB);
    
    for (let i = sortedThresholds.length - 1; i >= 0; i--) {
      const threshold = sortedThresholds[i];
      if (memoryMB >= threshold.memoryMB && performanceScore >= 0.5) {
        return threshold;
      }
    }
    
    return sortedThresholds[0]; // Fallback to minimum
  }

  private tokenizePrompt(prompt: string): string[] {
    // Implement tokenization logic
    // This is a simplified version - in production, use proper tokenizer
    return prompt.split(/\s+/).filter(token => token.length > 0);
  }

  private createTokenChunks(tokens: string[]): TokenChunk[] {
    const chunkSize = this.getOptimalChunkSize();
    const chunks: TokenChunk[] = [];
    
    for (let i = 0; i < tokens.length; i += chunkSize) {
      const chunkTokens = tokens.slice(i, i + chunkSize);
      chunks.push({
        id: `chunk_${i}_${Date.now()}`,
        tokens: chunkTokens,
        startIndex: i,
        endIndex: i + chunkTokens.length,
        compressed: false,
        memoryFootprint: this.calculateMemoryFootprint(chunkTokens),
        priority: this.calculateChunkPriority(chunkTokens, i)
      });
    }
    
    return chunks;
  }

  private async compressChunks(chunks: TokenChunk[]): Promise<TokenChunk[]> {
    const compressionLevel = this.getCompressionLevel();
    
    for (const chunk of chunks) {
      if (chunk.tokens.length > 100) { // Only compress larger chunks
        const compressed = await this.compressionEngine.compress(chunk.tokens, compressionLevel);
        chunk.tokens = compressed;
        chunk.compressed = true;
        chunk.memoryFootprint = this.calculateMemoryFootprint(compressed);
      }
    }
    
    return chunks;
  }

  private async processChunk(chunk: TokenChunk, model: any, options: StreamingOptions): Promise<TokenChunk> {
    const startTime = performance.now();
    
    try {
      // Process chunk through model
      const result = await model.infer(chunk.tokens, {
        contextWindow: this.state.currentTokens,
        compressionLevel: this.getCompressionLevel(),
        streaming: options.streaming || false
      });
      
      chunk.tokens = result.tokens;
      chunk.memoryFootprint = this.calculateMemoryFootprint(result.tokens);
      
      // Update performance metrics
      const processingTime = performance.now() - startTime;
      this.performanceTracker.recordProcessingTime(processingTime);
      
      return chunk;
      
    } catch (error) {
      console.error('❌ Chunk processing failed:', error);
      throw error;
    }
  }

  private calculateMemoryFootprint(tokens: string[]): number {
    // Estimate memory footprint in bytes
    const textSize = tokens.join(' ').length * 2; // UTF-16 encoding
    const overhead = tokens.length * 8; // Per-token overhead
    return textSize + overhead;
  }

  private calculateChunkPriority(tokens: string[], index: number): number {
    // Higher priority for chunks at the beginning and end
    const totalChunks = Math.ceil(tokens.length / this.getOptimalChunkSize());
    const position = index / tokens.length;
    
    if (position < 0.1 || position > 0.9) return 1.0; // High priority
    if (position < 0.3 || position > 0.7) return 0.7; // Medium priority
    return 0.4; // Low priority
  }

  private async handleMemoryPressure(): Promise<void> {
    console.log('⚠️ Memory pressure detected, reducing context window');
    
    // Reduce context window by 25%
    const newTokens = Math.floor(this.state.currentTokens * 0.75);
    this.state.currentTokens = Math.max(newTokens, this.config.minTokens);
    
    // Enable more aggressive compression
    this.state.compressionRatio = 0.5;
    
    this.emit('memoryPressure', {
      newTokens: this.state.currentTokens,
      compressionRatio: this.state.compressionRatio
    });
  }

  private updateMemoryUsage(usage: number): void {
    this.state.memoryUsageMB = usage;
    this.performAdaptiveScaling();
  }

  private updatePerformanceScore(score: number): void {
    this.state.performanceScore = score;
    this.performAdaptiveScaling();
  }

  private updateCompressionRatio(ratio: number): void {
    this.state.compressionRatio = ratio;
  }

  private async performAdaptiveScaling(): Promise<void> {
    const currentMemory = await this.memoryMonitor.getCurrentUsage();
    const performanceScore = this.performanceTracker.getCurrentScore();
    
    await this.adjustContextWindow(currentMemory, performanceScore);
  }

  private getMemoryThreshold(): number {
    const threshold = this.findOptimalThreshold(this.state.memoryUsageMB, this.state.performanceScore);
    return threshold?.memoryMB || 512;
  }
}

// Supporting Classes
class MemoryMonitor extends EventEmitter {
  private currentUsage: number = 0;
  private monitoringInterval: NodeJS.Timeout | null = null;

  constructor() {
    super();
    this.startMonitoring();
  }

  private startMonitoring(): void {
    this.monitoringInterval = setInterval(async () => {
      const usage = await this.getCurrentUsage();
      if (usage !== this.currentUsage) {
        this.currentUsage = usage;
        this.emit('memoryUpdate', usage);
      }
    }, 500);
  }

  public async getCurrentUsage(): Promise<number> {
    if (typeof performance !== 'undefined' && 'memory' in performance) {
      const memInfo = (performance as any).memory;
      return memInfo.usedJSHeapSize / (1024 * 1024); // Convert to MB
    }
    return 0;
  }

  public destroy(): void {
    if (this.monitoringInterval) {
      clearInterval(this.monitoringInterval);
    }
  }
}

class CompressionEngine extends EventEmitter {
  public async compress(tokens: string[], level: number): Promise<string[]> {
    // Implement compression logic (Brotli, LZ4, etc.)
    // For now, return simplified compression
    if (level === 0) return tokens;
    
    const compressionRatio = 1 - (level * 0.2);
    const compressedCount = Math.floor(tokens.length * compressionRatio);
    return tokens.slice(0, compressedCount);
  }
}

class StreamingEngine {
  public async *createStream(chunks: TokenChunk[]): AsyncGenerator<TokenChunk> {
    for (const chunk of chunks) {
      yield chunk;
    }
  }
}

class PerformanceTracker extends EventEmitter {
  private processingTimes: number[] = [];
  private currentScore: number = 1.0;

  public recordProcessingTime(time: number): void {
    this.processingTimes.push(time);
    
    // Keep only last 100 measurements
    if (this.processingTimes.length > 100) {
      this.processingTimes.shift();
    }
    
    this.updateScore();
  }

  public getCurrentScore(): number {
    return this.currentScore;
  }

  private updateScore(): void {
    if (this.processingTimes.length === 0) return;
    
    const avgTime = this.processingTimes.reduce((sum, time) => sum + time, 0) / this.processingTimes.length;
    const maxTime = Math.max(...this.processingTimes);
    
    // Score based on consistency and speed
    this.currentScore = Math.max(0, 1 - (avgTime / maxTime));
    this.emit('performanceUpdate', this.currentScore);
  }
}

// Supporting Interfaces
export interface StreamingOptions {
  streaming?: boolean;
  compressionLevel?: number;
  priority?: 'high' | 'medium' | 'low';
  timeout?: number;
}

// Export singleton instance
export const contextWindowManager = new ContextWindowManager();
