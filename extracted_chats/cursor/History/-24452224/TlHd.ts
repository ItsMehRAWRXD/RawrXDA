// SessionRunner.ts - Batch orchestration session management with parallel monitoring
// Production-grade scaffold for queuing and executing multiple autonomous orchestration jobs

import { 
  runAutoOrchestrator, 
  AutoOrchestratorOptions, 
  AutoOrchestratorResult,
  registerAgent,
  getOrchestrationStats,
  clearCompletedSessions
} from './AutoOrchestratorAgent';
import { BigDaddyGConfigManager } from '../BigDaddyGEngine.config';

export interface Session {
  id: string;
  prompt: string;
  agents?: string[];
  contextWindow?: number;
  maxSteps?: number;
  maxRetries?: number;
  priority?: 'low' | 'normal' | 'high';
  timeout?: number;
  result?: AutoOrchestratorResult;
  status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  createdAt: number;
  startedAt?: number;
  completedAt?: number;
  progress: number;
  metadata?: {
    sessionId?: string;
    estimatedDuration?: number;
    estimatedTokens?: number;
    retryCount?: number;
    errorHistory?: string[];
  };
}

export interface SessionRunnerOptions {
  maxConcurrentSessions?: number;
  maxQueueSize?: number;
  autoRetry?: boolean;
  retryDelay?: number;
  enableProgressTracking?: boolean;
  enableMetrics?: boolean;
  cleanupInterval?: number;
}

export interface SessionMetrics {
  totalSessions: number;
  pendingSessions: number;
  runningSessions: number;
  completedSessions: number;
  failedSessions: number;
  averageExecutionTime: number;
  averageTokenCount: number;
  averageConfidence: number;
  successRate: number;
  throughput: number; // sessions per minute
}

export interface SessionEvent {
  type: 'session_added' | 'session_started' | 'session_completed' | 'session_failed' | 'session_cancelled' | 'queue_updated';
  sessionId: string;
  timestamp: number;
  data?: any;
}

export class SessionRunner {
  private sessions: Session[] = [];
  private runningSessions: Set<string> = new Set();
  private eventListeners: Map<string, (event: SessionEvent) => void> = new Map();
  private cleanupTimer?: NodeJS.Timeout;
  private metricsTimer?: NodeJS.Timeout;
  
  private readonly maxConcurrentSessions: number;
  private readonly maxQueueSize: number;
  private readonly autoRetry: boolean;
  private readonly retryDelay: number;
  private readonly enableProgressTracking: boolean;
  private readonly enableMetrics: boolean;
  private readonly cleanupInterval: number;

  constructor(options: SessionRunnerOptions = {}) {
    this.maxConcurrentSessions = options.maxConcurrentSessions || 5;
    this.maxQueueSize = options.maxQueueSize || 100;
    this.autoRetry = options.autoRetry || true;
    this.retryDelay = options.retryDelay || 1000;
    this.enableProgressTracking = options.enableProgressTracking || true;
    this.enableMetrics = options.enableMetrics || true;
    this.cleanupInterval = options.cleanupInterval || 300000; // 5 minutes

    // Start cleanup timer
    if (this.cleanupInterval > 0) {
      this.cleanupTimer = setInterval(() => {
        this.cleanupCompletedSessions();
      }, this.cleanupInterval);
    }

    // Start metrics timer
    if (this.enableMetrics) {
      this.metricsTimer = setInterval(() => {
        this.updateMetrics();
      }, 10000); // Every 10 seconds
    }

    console.log(`🚀 SessionRunner initialized: max concurrent=${this.maxConcurrentSessions}, queue size=${this.maxQueueSize}`);
  }

  /** Adds a new session to the queue */
  addSession(sessionData: Omit<Session, 'id' | 'status' | 'createdAt' | 'progress'>): string {
    if (this.sessions.length >= this.maxQueueSize) {
      throw new Error(`Queue is full (${this.maxQueueSize} sessions). Cannot add more sessions.`);
    }

    const session: Session = {
      id: crypto.randomUUID(),
      status: 'pending',
      createdAt: Date.now(),
      progress: 0,
      ...sessionData
    };

    this.sessions.push(session);
    this.emitEvent({
      type: 'session_added',
      sessionId: session.id,
      timestamp: Date.now(),
      data: { prompt: session.prompt, agents: session.agents }
    });

    console.log(`📝 Session added: ${session.id} - "${session.prompt.substring(0, 50)}..."`);
    this.processQueue();
    return session.id;
  }

  /** Removes a session from the queue */
  removeSession(sessionId: string): boolean {
    const sessionIndex = this.sessions.findIndex(s => s.id === sessionId);
    if (sessionIndex === -1) return false;

    const session = this.sessions[sessionIndex];
    
    if (session.status === 'running') {
      this.cancelSession(sessionId);
    }

    this.sessions.splice(sessionIndex, 1);
    this.emitEvent({
      type: 'session_cancelled',
      sessionId,
      timestamp: Date.now(),
      data: { reason: 'removed_by_user' }
    });

    console.log(`🗑️ Session removed: ${sessionId}`);
    return true;
  }

  /** Cancels a running session */
  cancelSession(sessionId: string): boolean {
    const session = this.sessions.find(s => s.id === sessionId);
    if (!session || session.status !== 'running') return false;

    session.status = 'cancelled';
    session.completedAt = Date.now();
    this.runningSessions.delete(sessionId);

    this.emitEvent({
      type: 'session_cancelled',
      sessionId,
      timestamp: Date.now(),
      data: { reason: 'cancelled_by_user' }
    });

    console.log(`⏹️ Session cancelled: ${sessionId}`);
    this.processQueue();
    return true;
  }

  /** Gets session by ID */
  getSession(sessionId: string): Session | undefined {
    return this.sessions.find(s => s.id === sessionId);
  }

  /** Gets all sessions */
  getSessions(): Session[] {
    return [...this.sessions];
  }

  /** Gets sessions by status */
  getSessionsByStatus(status: Session['status']): Session[] {
    return this.sessions.filter(s => s.status === status);
  }

  /** Gets current queue status */
  getQueueStatus(): {
    total: number;
    pending: number;
    running: number;
    completed: number;
    failed: number;
    cancelled: number;
  } {
    const statusCounts = this.sessions.reduce((acc, session) => {
      acc[session.status] = (acc[session.status] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);

    return {
      total: this.sessions.length,
      pending: statusCounts.pending || 0,
      running: statusCounts.running || 0,
      completed: statusCounts.completed || 0,
      failed: statusCounts.failed || 0,
      cancelled: statusCounts.cancelled || 0
    };
  }

  /** Gets session metrics */
  getMetrics(): SessionMetrics {
    const completedSessions = this.sessions.filter(s => s.status === 'completed');
    const failedSessions = this.sessions.filter(s => s.status === 'failed');
    const totalProcessed = completedSessions.length + failedSessions.length;
    
    const totalExecutionTime = completedSessions.reduce((sum, s) => 
      sum + (s.result?.executionTimeMs || 0), 0);
    const totalTokens = completedSessions.reduce((sum, s) => 
      sum + (s.result?.tokenCount || 0), 0);
    const totalConfidence = completedSessions.reduce((sum, s) => 
      sum + (s.result?.metadata?.averageConfidence || 0), 0);

    const now = Date.now();
    const oneHourAgo = now - 3600000;
    const recentSessions = this.sessions.filter(s => s.createdAt > oneHourAgo);
    const throughput = recentSessions.length / 60; // sessions per minute

    return {
      totalSessions: this.sessions.length,
      pendingSessions: this.sessions.filter(s => s.status === 'pending').length,
      runningSessions: this.sessions.filter(s => s.status === 'running').length,
      completedSessions: completedSessions.length,
      failedSessions: failedSessions.length,
      averageExecutionTime: completedSessions.length > 0 ? totalExecutionTime / completedSessions.length : 0,
      averageTokenCount: completedSessions.length > 0 ? totalTokens / completedSessions.length : 0,
      averageConfidence: completedSessions.length > 0 ? totalConfidence / completedSessions.length : 0,
      successRate: totalProcessed > 0 ? completedSessions.length / totalProcessed : 0,
      throughput
    };
  }

  /** Processes the queue */
  private async processQueue(): Promise<void> {
    while (this.sessions.length > 0 && this.runningSessions.size < this.maxConcurrentSessions) {
      const session = this.sessions.find(s => s.status === 'pending');
      if (!session) break;

      await this.executeSession(session);
    }
  }

  /** Executes a single session */
  private async executeSession(session: Session): Promise<void> {
    session.status = 'running';
    session.startedAt = Date.now();
    session.progress = 0;
    this.runningSessions.add(session.id);

    this.emitEvent({
      type: 'session_started',
      sessionId: session.id,
      timestamp: Date.now(),
      data: { prompt: session.prompt, agents: session.agents }
    });

    console.log(`▶️ Executing session: ${session.id} - "${session.prompt.substring(0, 50)}..."`);

    try {
      const options: AutoOrchestratorOptions = {
        prompt: session.prompt,
        agents: session.agents,
        contextWindow: session.contextWindow,
        maxSteps: session.maxSteps,
        maxRetries: session.maxRetries,
        priority: session.priority,
        timeout: session.timeout
      };

      // Update progress during execution
      if (this.enableProgressTracking) {
        const progressInterval = setInterval(() => {
          if (session.status === 'running') {
            const elapsed = Date.now() - (session.startedAt || 0);
            const estimatedDuration = session.metadata?.estimatedDuration || 30000; // 30s default
            session.progress = Math.min(95, (elapsed / estimatedDuration) * 100);
          }
        }, 1000);

        session.result = await runAutoOrchestrator(options);
        clearInterval(progressInterval);
      } else {
        session.result = await runAutoOrchestrator(options);
      }

      session.status = session.result.success ? 'completed' : 'failed';
      session.progress = 100;
      session.completedAt = Date.now();

      this.emitEvent({
        type: session.result.success ? 'session_completed' : 'session_failed',
        sessionId: session.id,
        timestamp: Date.now(),
        data: {
          success: session.result.success,
          executionTime: session.result.executionTimeMs,
          tokenCount: session.result.tokenCount,
          errors: session.result.errors
        }
      });

      console.log(`${session.result.success ? '✅' : '❌'} Session ${session.status}: ${session.id} (${session.result.executionTimeMs.toFixed(2)}ms)`);

    } catch (error: any) {
      session.status = 'failed';
      session.progress = 100;
      session.completedAt = Date.now();
      session.result = {
        success: false,
        output: '',
        steps: [],
        tokenCount: 0,
        executionTimeMs: Date.now() - (session.startedAt || 0),
        errors: [error.message]
      };

      this.emitEvent({
        type: 'session_failed',
        sessionId: session.id,
        timestamp: Date.now(),
        data: { error: error.message }
      });

      console.error(`💥 Session failed: ${session.id} - ${error.message}`);

      // Auto-retry logic
      if (this.autoRetry && (session.metadata?.retryCount || 0) < 3) {
        session.metadata = {
          ...session.metadata,
          retryCount: (session.metadata?.retryCount || 0) + 1,
          errorHistory: [...(session.metadata?.errorHistory || []), error.message]
        };

        console.log(`🔄 Auto-retrying session: ${session.id} (attempt ${session.metadata.retryCount})`);
        
        setTimeout(() => {
          session.status = 'pending';
          session.progress = 0;
          session.startedAt = undefined;
          session.completedAt = undefined;
          session.result = undefined;
          this.processQueue();
        }, this.retryDelay);
      }
    } finally {
      this.runningSessions.delete(session.id);
      this.processQueue();
    }
  }

  /** Cleans up completed sessions */
  private cleanupCompletedSessions(): void {
    const cutoffTime = Date.now() - 300000; // 5 minutes ago
    const initialCount = this.sessions.length;
    
    this.sessions = this.sessions.filter(session => {
      if (session.status === 'completed' || session.status === 'failed') {
        return session.completedAt && session.completedAt > cutoffTime;
      }
      return true;
    });

    const removedCount = initialCount - this.sessions.length;
    if (removedCount > 0) {
      console.log(`🧹 Cleaned up ${removedCount} completed sessions`);
    }
  }

  /** Updates metrics */
  private updateMetrics(): void {
    const metrics = this.getMetrics();
    console.log(`📊 SessionRunner metrics: ${metrics.totalSessions} total, ${metrics.runningSessions} running, ${metrics.successRate.toFixed(2)} success rate`);
  }

  /** Emits an event to all listeners */
  private emitEvent(event: SessionEvent): void {
    this.eventListeners.forEach(listener => {
      try {
        listener(event);
      } catch (error) {
        console.error('Error in event listener:', error);
      }
    });
  }

  /** Adds an event listener */
  addEventListener(eventType: string, listener: (event: SessionEvent) => void): void {
    this.eventListeners.set(eventType, listener);
  }

  /** Removes an event listener */
  removeEventListener(eventType: string): void {
    this.eventListeners.delete(eventType);
  }

  /** Destroys the session runner and cleans up resources */
  destroy(): void {
    if (this.cleanupTimer) {
      clearInterval(this.cleanupTimer);
    }
    if (this.metricsTimer) {
      clearInterval(this.metricsTimer);
    }
    
    // Cancel all running sessions
    this.sessions.forEach(session => {
      if (session.status === 'running') {
        session.status = 'cancelled';
        session.completedAt = Date.now();
      }
    });

    this.sessions = [];
    this.runningSessions.clear();
    this.eventListeners.clear();
    
    console.log('🛑 SessionRunner destroyed');
  }
}

// Utility functions
export function createSessionRunner(options?: SessionRunnerOptions): SessionRunner {
  return new SessionRunner(options);
}

export function initializeDefaultAgents(): void {
  // Register default agents
  const { exampleAgents } = require('./AutoOrchestratorAgent');
  Object.values(exampleAgents).forEach((agent: any) => {
    registerAgent(agent as Agent);
  });
  console.log('🤖 Default agents registered');
}

// Example usage
export async function exampleUsage(): Promise<void> {
  console.log('🚀 Starting SessionRunner example...');

  // Initialize agents
  initializeDefaultAgents();

  // Create session runner
  const runner = createSessionRunner({
    maxConcurrentSessions: 3,
    maxQueueSize: 50,
    autoRetry: true,
    retryDelay: 2000,
    enableProgressTracking: true,
    enableMetrics: true
  });

  // Add event listener
  runner.addEventListener('session_completed', (event) => {
    console.log(`🎉 Session completed: ${event.sessionId}`);
  });

  runner.addEventListener('session_failed', (event) => {
    console.log(`❌ Session failed: ${event.sessionId} - ${event.data?.error}`);
  });

  // Add some sessions
  const sessionIds = [
    runner.addSession({
      prompt: 'Optimize the browser inference loop for better performance',
      agents: ['rawr', 'analyzer', 'optimizer'],
      maxSteps: 3,
      priority: 'high'
    }),
    runner.addSession({
      prompt: 'Analyze the current codebase structure',
      agents: ['analyzer'],
      maxSteps: 2,
      priority: 'normal'
    }),
    runner.addSession({
      prompt: 'Transform text to uppercase format',
      agents: ['rawr'],
      maxSteps: 1,
      priority: 'low'
    })
  ];

  console.log(`📝 Added ${sessionIds.length} sessions to queue`);

  // Monitor progress
  const monitorInterval = setInterval(() => {
    const status = runner.getQueueStatus();
    const metrics = runner.getMetrics();
    
    console.log(`📊 Queue status: ${status.pending} pending, ${status.running} running, ${status.completed} completed, ${status.failed} failed`);
    console.log(`📈 Metrics: ${metrics.successRate.toFixed(2)} success rate, ${metrics.throughput.toFixed(2)} sessions/min`);
    
    if (status.pending === 0 && status.running === 0) {
      clearInterval(monitorInterval);
      console.log('✅ All sessions completed');
      
      // Show final results
      const sessions = runner.getSessions();
      sessions.forEach(session => {
        console.log(`📋 Session ${session.id}: ${session.status} - ${session.result?.executionTimeMs.toFixed(2)}ms`);
      });
      
      runner.destroy();
    }
  }, 2000);

  // Wait for completion
  await new Promise(resolve => {
    const checkInterval = setInterval(() => {
      const status = runner.getQueueStatus();
      if (status.pending === 0 && status.running === 0) {
        clearInterval(checkInterval);
        resolve(undefined);
      }
    }, 1000);
  });
}

// Re-export for use in other modules (types only to avoid redeclaration)
export type { Session, SessionRunnerOptions, SessionMetrics, SessionEvent };
