// BigDaddyGEngine/orchestration/SessionRunner.ts - Batch Orchestration Engine with Checkpointing
import { MultiAgentOrchestrator, OrchestrationGraph } from '../MultiAgentOrchestrator';
import { AgentMetricsLogger } from '../optimization/AgentMetricsLogger';
import { SelfOptimizingAgentMesh } from '../optimization/SelfOptimizingAgentMesh';
import { IndexedDBManager } from '../persistence/IndexedDBManager';

export interface SessionConfig {
  maxConcurrentSessions: number;
  checkpointInterval: number; // milliseconds
  retryAttempts: number;
  timeoutMs: number;
  enableRecovery: boolean;
  enableScaling: boolean;
  enableMetrics: boolean;
  enableLogging: boolean;
}

export interface SessionTask {
  id: string;
  type: 'refactor' | 'analyze' | 'optimize' | 'test' | 'deploy';
  priority: 'low' | 'medium' | 'high' | 'critical';
  payload: any;
  dependencies: string[];
  estimatedDuration: number;
  maxRetries: number;
  timeout: number;
  metadata: Record<string, any>;
}

export interface SessionCheckpoint {
  sessionId: string;
  timestamp: number;
  status: 'running' | 'paused' | 'completed' | 'failed' | 'cancelled';
  progress: number;
  currentTask: string | null;
  completedTasks: string[];
  failedTasks: string[];
  results: Record<string, any>;
  metrics: SessionMetrics;
  state: any;
}

export interface SessionMetrics {
  totalTasks: number;
  completedTasks: number;
  failedTasks: number;
  successRate: number;
  averageLatency: number;
  totalLatency: number;
  resourceUsage: number;
  throughput: number;
  errorRate: number;
}

export interface SessionResult {
  sessionId: string;
  status: 'completed' | 'failed' | 'cancelled';
  results: Record<string, any>;
  metrics: SessionMetrics;
  duration: number;
  errors: string[];
  warnings: string[];
  recommendations: string[];
}

export interface SessionEvent {
  type: 'started' | 'task_completed' | 'task_failed' | 'checkpoint' | 'completed' | 'failed' | 'cancelled';
  sessionId: string;
  timestamp: number;
  data: any;
}

export class SessionRunner {
  private orchestrator: MultiAgentOrchestrator;
  private metricsLogger: AgentMetricsLogger;
  private mesh: SelfOptimizingAgentMesh;
  private dbManager: IndexedDBManager;
  private config: SessionConfig;
  
  private activeSessions: Map<string, SessionCheckpoint> = new Map();
  private sessionQueue: SessionTask[] = [];
  private eventListeners: Map<string, ((event: SessionEvent) => void)[]> = new Map();
  private isRunning: boolean = false;
  private checkpointTimer: NodeJS.Timeout | null = null;

  constructor(
    orchestrator: MultiAgentOrchestrator,
    metricsLogger: AgentMetricsLogger,
    mesh: SelfOptimizingAgentMesh,
    dbManager: IndexedDBManager,
    config: Partial<SessionConfig> = {}
  ) {
    this.orchestrator = orchestrator;
    this.metricsLogger = metricsLogger;
    this.mesh = mesh;
    this.dbManager = dbManager;
    this.config = {
      maxConcurrentSessions: 5,
      checkpointInterval: 30000, // 30 seconds
      retryAttempts: 3,
      timeoutMs: 300000, // 5 minutes
      enableRecovery: true,
      enableScaling: true,
      enableMetrics: true,
      enableLogging: true,
      ...config
    };

    this.initializeSessionRunner();
  }

  /**
   * Start a new session with tasks
   */
  async startSession(
    sessionId: string,
    tasks: SessionTask[],
    options: {
      priority?: 'low' | 'medium' | 'high' | 'critical';
      dependencies?: string[];
      metadata?: Record<string, any>;
    } = {}
  ): Promise<SessionResult> {
    if (this.activeSessions.has(sessionId)) {
      throw new Error(`Session ${sessionId} already exists`);
    }

    if (this.activeSessions.size >= this.config.maxConcurrentSessions) {
      // Queue the session
      this.sessionQueue.push(...tasks.map(task => ({ ...task, sessionId })));
      throw new Error('Maximum concurrent sessions reached, queued for later execution');
    }

    try {
      // Create initial checkpoint
      const checkpoint: SessionCheckpoint = {
        sessionId,
        timestamp: Date.now(),
        status: 'running',
        progress: 0,
        currentTask: null,
        completedTasks: [],
        failedTasks: [],
        results: {},
        metrics: {
          totalTasks: tasks.length,
          completedTasks: 0,
          failedTasks: 0,
          successRate: 0,
          averageLatency: 0,
          totalLatency: 0,
          resourceUsage: 0,
          throughput: 0,
          errorRate: 0
        },
        state: {
          tasks,
          options,
          startTime: Date.now()
        }
      };

      this.activeSessions.set(sessionId, checkpoint);
      this.emitEvent('started', sessionId, { tasks, options });

      // Start session execution
      const result = await this.executeSession(sessionId, tasks, options);
      
      // Clean up
      this.activeSessions.delete(sessionId);
      
      return result;
    } catch (error) {
      this.activeSessions.delete(sessionId);
      throw error;
    }
  }

  /**
   * Pause a running session
   */
  async pauseSession(sessionId: string): Promise<void> {
    const checkpoint = this.activeSessions.get(sessionId);
    if (!checkpoint) {
      throw new Error(`Session ${sessionId} not found`);
    }

    checkpoint.status = 'paused';
    await this.saveCheckpoint(checkpoint);
    this.emitEvent('checkpoint', sessionId, { status: 'paused' });
  }

  /**
   * Resume a paused session
   */
  async resumeSession(sessionId: string): Promise<void> {
    const checkpoint = this.activeSessions.get(sessionId);
    if (!checkpoint) {
      throw new Error(`Session ${sessionId} not found`);
    }

    checkpoint.status = 'running';
    await this.saveCheckpoint(checkpoint);
    this.emitEvent('checkpoint', sessionId, { status: 'resumed' });
  }

  /**
   * Cancel a session
   */
  async cancelSession(sessionId: string): Promise<void> {
    const checkpoint = this.activeSessions.get(sessionId);
    if (!checkpoint) {
      throw new Error(`Session ${sessionId} not found`);
    }

    checkpoint.status = 'cancelled';
    await this.saveCheckpoint(checkpoint);
    this.activeSessions.delete(sessionId);
    this.emitEvent('cancelled', sessionId, {});
  }

  /**
   * Get session status
   */
  getSessionStatus(sessionId: string): SessionCheckpoint | null {
    return this.activeSessions.get(sessionId) || null;
  }

  /**
   * Get all active sessions
   */
  getActiveSessions(): SessionCheckpoint[] {
    return Array.from(this.activeSessions.values());
  }

  /**
   * Get session metrics
   */
  getSessionMetrics(sessionId: string): SessionMetrics | null {
    const checkpoint = this.activeSessions.get(sessionId);
    return checkpoint ? checkpoint.metrics : null;
  }

  /**
   * Add event listener
   */
  on(eventType: string, listener: (event: SessionEvent) => void): void {
    if (!this.eventListeners.has(eventType)) {
      this.eventListeners.set(eventType, []);
    }
    this.eventListeners.get(eventType)!.push(listener);
  }

  /**
   * Remove event listener
   */
  off(eventType: string, listener: (event: SessionEvent) => void): void {
    const listeners = this.eventListeners.get(eventType);
    if (listeners) {
      const index = listeners.indexOf(listener);
      if (index > -1) {
        listeners.splice(index, 1);
      }
    }
  }

  /**
   * Start the session runner
   */
  async start(): Promise<void> {
    if (this.isRunning) return;

    this.isRunning = true;
    
    // Start checkpoint timer
    this.checkpointTimer = setInterval(() => {
      this.saveAllCheckpoints();
    }, this.config.checkpointInterval);

    // Start queue processor
    this.processQueue();

    console.log('SessionRunner started');
  }

  /**
   * Stop the session runner
   */
  async stop(): Promise<void> {
    if (!this.isRunning) return;

    this.isRunning = false;
    
    // Clear checkpoint timer
    if (this.checkpointTimer) {
      clearInterval(this.checkpointTimer);
      this.checkpointTimer = null;
    }

    // Save all checkpoints
    await this.saveAllCheckpoints();

    console.log('SessionRunner stopped');
  }

  /**
   * Recover sessions from storage
   */
  async recoverSessions(): Promise<void> {
    if (!this.config.enableRecovery) return;

    try {
      const checkpoints = await this.dbManager.getSessionCheckpoints();
      
      for (const checkpoint of checkpoints) {
        if (checkpoint.status === 'running' || checkpoint.status === 'paused') {
          this.activeSessions.set(checkpoint.sessionId, checkpoint);
          this.emitEvent('checkpoint', checkpoint.sessionId, { status: 'recovered' });
        }
      }
    } catch (error) {
      console.error('Failed to recover sessions:', error);
    }
  }

  // Private methods

  private async executeSession(
    sessionId: string,
    tasks: SessionTask[],
    options: any
  ): Promise<SessionResult> {
    const startTime = Date.now();
    const results: Record<string, any> = {};
    const errors: string[] = [];
    const warnings: string[] = [];
    const recommendations: string[] = [];

    try {
      // Execute tasks in order
      for (const task of tasks) {
        const checkpoint = this.activeSessions.get(sessionId);
        if (!checkpoint || checkpoint.status !== 'running') {
          break;
        }

        checkpoint.currentTask = task.id;
        await this.saveCheckpoint(checkpoint);

        try {
          // Execute task
          const result = await this.executeTask(task);
          results[task.id] = result;
          
          checkpoint.completedTasks.push(task.id);
          checkpoint.progress = (checkpoint.completedTasks.length / tasks.length) * 100;
          checkpoint.metrics.completedTasks = checkpoint.completedTasks.length;
          checkpoint.metrics.successRate = checkpoint.completedTasks.length / tasks.length;
          
          this.emitEvent('task_completed', sessionId, { task, result });
        } catch (error) {
          const errorMessage = error instanceof Error ? error.message : 'Unknown error';
          errors.push(`Task ${task.id} failed: ${errorMessage}`);
          
          checkpoint.failedTasks.push(task.id);
          checkpoint.metrics.failedTasks = checkpoint.failedTasks.length;
          checkpoint.metrics.errorRate = checkpoint.failedTasks.length / tasks.length;
          
          this.emitEvent('task_failed', sessionId, { task, error: errorMessage });
          
          // Retry logic
          if (task.maxRetries > 0) {
            task.maxRetries--;
            await this.executeTask(task);
          }
        }
      }

      // Calculate final metrics
      const duration = Date.now() - startTime;
      const checkpoint = this.activeSessions.get(sessionId);
      const finalMetrics = checkpoint ? checkpoint.metrics : this.calculateMetrics(tasks, results, duration);

      const result: SessionResult = {
        sessionId,
        status: errors.length === 0 ? 'completed' : 'failed',
        results,
        metrics: finalMetrics,
        duration,
        errors,
        warnings,
        recommendations
      };

      this.emitEvent('completed', sessionId, result);
      return result;
    } catch (error) {
      const result: SessionResult = {
        sessionId,
        status: 'failed',
        results,
        metrics: this.calculateMetrics(tasks, results, Date.now() - startTime),
        duration: Date.now() - startTime,
        errors: [...errors, error instanceof Error ? error.message : 'Unknown error'],
        warnings,
        recommendations
      };

      this.emitEvent('failed', sessionId, result);
      return result;
    }
  }

  private async executeTask(task: SessionTask): Promise<any> {
    const startTime = Date.now();
    
    try {
      // Route task through mesh
      const route = await this.mesh.routeTask(task.payload, task.dependencies);
      
      // Execute through orchestrator
      const result = await this.orchestrator.execute(route, {
        timeout: task.timeout,
        retries: task.maxRetries,
        metadata: task.metadata
      });

      const latency = Date.now() - startTime;
      
      // Log metrics
      if (this.config.enableMetrics) {
        this.metricsLogger.logTaskExecution(task.id, {
          latency,
          success: true,
          resourceUsage: this.calculateResourceUsage(),
          throughput: 1 / (latency / 1000)
        });
      }

      return result;
    } catch (error) {
      const latency = Date.now() - startTime;
      
      // Log metrics
      if (this.config.enableMetrics) {
        this.metricsLogger.logTaskExecution(task.id, {
          latency,
          success: false,
          resourceUsage: this.calculateResourceUsage(),
          throughput: 0
        });
      }

      throw error;
    }
  }

  private async saveCheckpoint(checkpoint: SessionCheckpoint): Promise<void> {
    if (this.config.enableLogging) {
      await this.dbManager.saveSessionCheckpoint(checkpoint);
    }
  }

  private async saveAllCheckpoints(): Promise<void> {
    for (const checkpoint of this.activeSessions.values()) {
      await this.saveCheckpoint(checkpoint);
    }
  }

  private calculateMetrics(tasks: SessionTask[], results: Record<string, any>, duration: number): SessionMetrics {
    const completedTasks = Object.keys(results).length;
    const failedTasks = tasks.length - completedTasks;
    const successRate = completedTasks / tasks.length;
    const averageLatency = duration / tasks.length;
    const throughput = completedTasks / (duration / 1000);
    const errorRate = failedTasks / tasks.length;

    return {
      totalTasks: tasks.length,
      completedTasks,
      failedTasks,
      successRate,
      averageLatency,
      totalLatency: duration,
      resourceUsage: this.calculateResourceUsage(),
      throughput,
      errorRate
    };
  }

  private calculateResourceUsage(): number {
    // Calculate resource usage based on active sessions and mesh load
    const activeSessions = this.activeSessions.size;
    const meshLoad = this.mesh.getHealth().overallHealth === 'excellent' ? 0.1 : 0.5;
    return (activeSessions / this.config.maxConcurrentSessions) * 0.5 + meshLoad;
  }

  private emitEvent(type: string, sessionId: string, data: any): void {
    const event: SessionEvent = {
      type: type as any,
      sessionId,
      timestamp: Date.now(),
      data
    };

    const listeners = this.eventListeners.get(type);
    if (listeners) {
      listeners.forEach(listener => listener(event));
    }
  }

  private async processQueue(): Promise<void> {
    while (this.isRunning) {
      if (this.sessionQueue.length > 0 && this.activeSessions.size < this.config.maxConcurrentSessions) {
        const task = this.sessionQueue.shift();
        if (task) {
          try {
            await this.startSession(task.sessionId, [task], {});
          } catch (error) {
            console.error('Failed to process queued task:', error);
          }
        }
      }
      
      // Wait before checking queue again
      await new Promise(resolve => setTimeout(resolve, 1000));
    }
  }

  private initializeSessionRunner(): void {
    // Initialize event listeners
    this.on('task_completed', (event) => {
      if (this.config.enableLogging) {
        console.log(`Task completed in session ${event.sessionId}:`, event.data);
      }
    });

    this.on('task_failed', (event) => {
      if (this.config.enableLogging) {
        console.error(`Task failed in session ${event.sessionId}:`, event.data);
      }
    });

    this.on('session_completed', (event) => {
      if (this.config.enableLogging) {
        console.log(`Session completed: ${event.sessionId}`, event.data);
      }
    });
  }
}
