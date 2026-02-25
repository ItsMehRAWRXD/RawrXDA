// BigDaddyGEngine/orchestration/SessionRunner.ts - Batch Orchestration and Recovery
import { MultiAgentOrchestrator, OrchestrationGraph, OrchestratorStep } from '../MultiAgentOrchestrator';
import { AgentFeedbackLoop } from '../feedback/AgentFeedbackLoop';
import { MemoryGraph } from '../memory/MemoryGraph';
import { AgentMetricsLogger } from '../optimization/AgentMetricsLogger';

export interface SessionConfig {
  sessionId: string;
  name: string;
  description: string;
  repositoryPath: string;
  agentChain: string[];
  batchSize: number;
  maxConcurrentSessions: number;
  timeout: number;
  retryAttempts: number;
  enableRecovery: boolean;
  enableFeedback: boolean;
}

export interface SessionTask {
  id: string;
  prompt: string;
  priority: number;
  metadata: Record<string, any>;
  dependencies?: string[];
  expectedDuration?: number;
}

export interface SessionResult {
  sessionId: string;
  taskId: string;
  success: boolean;
  result: any;
  duration: number;
  error?: string;
  feedback?: {
    score: number;
    comment?: string;
  };
}

export interface SessionProgress {
  sessionId: string;
  totalTasks: number;
  completedTasks: number;
  failedTasks: number;
  currentTask?: string;
  startTime: number;
  estimatedCompletion?: number;
  status: 'pending' | 'running' | 'paused' | 'completed' | 'failed' | 'cancelled';
}

export interface SessionRecovery {
  sessionId: string;
  checkpoint: number;
  completedTasks: string[];
  failedTasks: string[];
  pendingTasks: string[];
  lastCheckpoint: number;
  recoveryData: Record<string, any>;
}

export class SessionRunner {
  private config: SessionConfig;
  private orchestrator: MultiAgentOrchestrator;
  private feedbackLoop: AgentFeedbackLoop;
  private memoryGraph: MemoryGraph;
  private metricsLogger: AgentMetricsLogger;
  
  private activeSessions: Map<string, SessionProgress>;
  private sessionQueue: SessionTask[];
  private recoveryData: Map<string, SessionRecovery>;
  private sessionResults: Map<string, SessionResult[]>;
  
  private isRunning: boolean = false;
  private processingInterval: NodeJS.Timeout | null = null;

  constructor(
    config: SessionConfig,
    orchestrator: MultiAgentOrchestrator,
    feedbackLoop: AgentFeedbackLoop,
    memoryGraph: MemoryGraph,
    metricsLogger: AgentMetricsLogger
  ) {
    this.config = config;
    this.orchestrator = orchestrator;
    this.feedbackLoop = feedbackLoop;
    this.memoryGraph = memoryGraph;
    this.metricsLogger = metricsLogger;
    
    this.activeSessions = new Map();
    this.sessionQueue = [];
    this.recoveryData = new Map();
    this.sessionResults = new Map();
  }

  /**
   * Starts the session runner
   */
  start(): void {
    if (this.isRunning) {
      console.warn('⚠️ Session runner is already running');
      return;
    }

    this.isRunning = true;
    this.startProcessingLoop();
    console.log(`🚀 Session runner started for session ${this.config.sessionId}`);
  }

  /**
   * Stops the session runner
   */
  stop(): void {
    this.isRunning = false;
    if (this.processingInterval) {
      clearInterval(this.processingInterval);
      this.processingInterval = null;
    }
    console.log(`🛑 Session runner stopped for session ${this.config.sessionId}`);
  }

  /**
   * Adds tasks to the session queue
   */
  addTasks(tasks: SessionTask[]): void {
    this.sessionQueue.push(...tasks);
    console.log(`📝 Added ${tasks.length} tasks to session ${this.config.sessionId}`);
  }

  /**
   * Adds a single task to the session queue
   */
  addTask(task: SessionTask): void {
    this.sessionQueue.push(task);
    console.log(`📝 Added task ${task.id} to session ${this.config.sessionId}`);
  }

  /**
   * Runs a batch of tasks
   */
  async runBatch(tasks: SessionTask[]): Promise<SessionResult[]> {
    console.log(`🔄 Running batch of ${tasks.length} tasks`);
    
    const results: SessionResult[] = [];
    const batchPromises: Promise<SessionResult>[] = [];
    
    // Process tasks in parallel (up to batchSize)
    const concurrentTasks = tasks.slice(0, this.config.batchSize);
    
    for (const task of concurrentTasks) {
      batchPromises.push(this.runTask(task));
    }
    
    // Wait for all tasks to complete
    const batchResults = await Promise.allSettled(batchPromises);
    
    for (const result of batchResults) {
      if (result.status === 'fulfilled') {
        results.push(result.value);
      } else {
        console.error('❌ Task failed:', result.reason);
        results.push({
          sessionId: this.config.sessionId,
          taskId: 'unknown',
          success: false,
          result: null,
          duration: 0,
          error: result.reason?.message || 'Unknown error'
        });
      }
    }
    
    console.log(`✅ Batch completed: ${results.filter(r => r.success).length}/${results.length} successful`);
    return results;
  }

  /**
   * Runs a single task
   */
  async runTask(task: SessionTask): Promise<SessionResult> {
    const startTime = Date.now();
    console.log(`🎯 Running task ${task.id}: ${task.prompt.substring(0, 50)}...`);
    
    try {
      // Check dependencies
      if (task.dependencies) {
        const dependencyResults = await this.checkDependencies(task.dependencies);
        if (!dependencyResults.allSatisfied) {
          throw new Error(`Dependencies not satisfied: ${dependencyResults.missing.join(', ')}`);
        }
      }
      
      // Run orchestration
      const graph = await this.orchestrator.run(task.prompt, this.config.agentChain);
      
      // Process feedback if enabled
      if (this.config.enableFeedback) {
        await this.feedbackLoop.processOrchestrationGraph(graph);
      }
      
      // Store result in memory
      await this.storeTaskResult(task, graph);
      
      const duration = Date.now() - startTime;
      const result: SessionResult = {
        sessionId: this.config.sessionId,
        taskId: task.id,
        success: graph.success,
        result: graph,
        duration,
        feedback: {
          score: graph.success ? 0.8 : 0.2,
          comment: graph.success ? 'Task completed successfully' : 'Task failed'
        }
      };
      
      // Store result
      this.storeSessionResult(result);
      
      console.log(`✅ Task ${task.id} completed in ${duration}ms`);
      return result;
      
    } catch (error) {
      const duration = Date.now() - startTime;
      const result: SessionResult = {
        sessionId: this.config.sessionId,
        taskId: task.id,
        success: false,
        result: null,
        duration,
        error: error instanceof Error ? error.message : 'Unknown error'
      };
      
      this.storeSessionResult(result);
      
      console.error(`❌ Task ${task.id} failed:`, error);
      return result;
    }
  }

  /**
   * Pauses the session
   */
  pause(): void {
    const progress = this.activeSessions.get(this.config.sessionId);
    if (progress) {
      progress.status = 'paused';
      this.createRecoveryCheckpoint();
      console.log(`⏸️ Session ${this.config.sessionId} paused`);
    }
  }

  /**
   * Resumes the session
   */
  resume(): void {
    const progress = this.activeSessions.get(this.config.sessionId);
    if (progress && progress.status === 'paused') {
      progress.status = 'running';
      this.loadRecoveryCheckpoint();
      console.log(`▶️ Session ${this.config.sessionId} resumed`);
    }
  }

  /**
   * Cancels the session
   */
  cancel(): void {
    const progress = this.activeSessions.get(this.config.sessionId);
    if (progress) {
      progress.status = 'cancelled';
      this.stop();
      console.log(`❌ Session ${this.config.sessionId} cancelled`);
    }
  }

  /**
   * Gets session progress
   */
  getSessionProgress(sessionId: string): SessionProgress | null {
    return this.activeSessions.get(sessionId) || null;
  }

  /**
   * Gets session results
   */
  getSessionResults(sessionId: string): SessionResult[] {
    return this.sessionResults.get(sessionId) || [];
  }

  /**
   * Gets recovery data for a session
   */
  getRecoveryData(sessionId: string): SessionRecovery | null {
    return this.recoveryData.get(sessionId) || null;
  }

  /**
   * Recovers a session from checkpoint
   */
  async recoverSession(sessionId: string): Promise<boolean> {
    const recovery = this.recoveryData.get(sessionId);
    if (!recovery) {
      console.error(`❌ No recovery data found for session ${sessionId}`);
      return false;
    }
    
    try {
      console.log(`🔄 Recovering session ${sessionId} from checkpoint ${recovery.checkpoint}`);
      
      // Restore session state
      const progress: SessionProgress = {
        sessionId,
        totalTasks: recovery.completedTasks.length + recovery.failedTasks.length + recovery.pendingTasks.length,
        completedTasks: recovery.completedTasks.length,
        failedTasks: recovery.failedTasks.length,
        startTime: recovery.lastCheckpoint,
        status: 'running'
      };
      
      this.activeSessions.set(sessionId, progress);
      
      // Resume processing pending tasks
      const pendingTasks = this.sessionQueue.filter(task => 
        recovery.pendingTasks.includes(task.id)
      );
      
      if (pendingTasks.length > 0) {
        await this.runBatch(pendingTasks);
      }
      
      console.log(`✅ Session ${sessionId} recovered successfully`);
      return true;
      
    } catch (error) {
      console.error(`❌ Failed to recover session ${sessionId}:`, error);
      return false;
    }
  }

  /**
   * Exports session data
   */
  exportSessionData(sessionId: string): {
    config: SessionConfig;
    progress: SessionProgress | null;
    results: SessionResult[];
    recovery: SessionRecovery | null;
  } {
    return {
      config: this.config,
      progress: this.activeSessions.get(sessionId) || null,
      results: this.sessionResults.get(sessionId) || [],
      recovery: this.recoveryData.get(sessionId) || null
    };
  }

  // Private methods

  private startProcessingLoop(): void {
    if (this.processingInterval) {
      clearInterval(this.processingInterval);
    }
    
    this.processingInterval = setInterval(async () => {
      if (!this.isRunning) return;
      
      try {
        await this.processNextBatch();
      } catch (error) {
        console.error('❌ Processing loop error:', error);
      }
    }, 1000); // Check every second
  }

  private async processNextBatch(): Promise<void> {
    if (this.sessionQueue.length === 0) return;
    
    const activeCount = this.activeSessions.size;
    if (activeCount >= this.config.maxConcurrentSessions) return;
    
    // Get next batch of tasks
    const batch = this.sessionQueue.splice(0, this.config.batchSize);
    if (batch.length === 0) return;
    
    // Create session progress
    const progress: SessionProgress = {
      sessionId: this.config.sessionId,
      totalTasks: batch.length,
      completedTasks: 0,
      failedTasks: 0,
      startTime: Date.now(),
      status: 'running'
    };
    
    this.activeSessions.set(this.config.sessionId, progress);
    
    // Run batch
    try {
      const results = await this.runBatch(batch);
      
      // Update progress
      progress.completedTasks = results.filter(r => r.success).length;
      progress.failedTasks = results.filter(r => !r.success).length;
      progress.status = 'completed';
      
      console.log(`✅ Batch completed: ${progress.completedTasks}/${progress.totalTasks} successful`);
      
    } catch (error) {
      progress.status = 'failed';
      console.error('❌ Batch processing failed:', error);
    }
  }

  private async checkDependencies(dependencies: string[]): Promise<{
    allSatisfied: boolean;
    missing: string[];
  }> {
    const missing: string[] = [];
    
    for (const dep of dependencies) {
      const results = this.sessionResults.get(this.config.sessionId) || [];
      const hasResult = results.some(r => r.taskId === dep && r.success);
      
      if (!hasResult) {
        missing.push(dep);
      }
    }
    
    return {
      allSatisfied: missing.length === 0,
      missing
    };
  }

  private async storeTaskResult(task: SessionTask, graph: OrchestrationGraph): Promise<void> {
    try {
      // Store in memory graph
      await this.memoryGraph.addTrace({
        id: `task-${task.id}`,
        prompt: task.prompt,
        output: graph.steps.map(s => s.output).join('\n'),
        agentId: this.config.agentChain[0],
        timestamp: Date.now(),
        embedding: new Array(384).fill(0), // Mock embedding
        metadata: {
          sessionId: this.config.sessionId,
          taskId: task.id,
          priority: task.priority,
          ...task.metadata
        }
      });
      
    } catch (error) {
      console.error('❌ Failed to store task result:', error);
    }
  }

  private storeSessionResult(result: SessionResult): void {
    const results = this.sessionResults.get(result.sessionId) || [];
    results.push(result);
    this.sessionResults.set(result.sessionId, results);
  }

  private createRecoveryCheckpoint(): void {
    const progress = this.activeSessions.get(this.config.sessionId);
    if (!progress) return;
    
    const results = this.sessionResults.get(this.config.sessionId) || [];
    const completedTasks = results.filter(r => r.success).map(r => r.taskId);
    const failedTasks = results.filter(r => !r.success).map(r => r.taskId);
    const pendingTasks = this.sessionQueue.map(t => t.id);
    
    const recovery: SessionRecovery = {
      sessionId: this.config.sessionId,
      checkpoint: Date.now(),
      completedTasks,
      failedTasks,
      pendingTasks,
      lastCheckpoint: progress.startTime,
      recoveryData: {
        config: this.config,
        queueLength: this.sessionQueue.length
      }
    };
    
    this.recoveryData.set(this.config.sessionId, recovery);
    console.log(`💾 Created recovery checkpoint for session ${this.config.sessionId}`);
  }

  private loadRecoveryCheckpoint(): void {
    const recovery = this.recoveryData.get(this.config.sessionId);
    if (!recovery) return;
    
    console.log(`📂 Loading recovery checkpoint for session ${this.config.sessionId}`);
    // Recovery logic would be implemented here
  }
}

// Export factory function
export function createSessionRunner(
  config: SessionConfig,
  orchestrator: MultiAgentOrchestrator,
  feedbackLoop: AgentFeedbackLoop,
  memoryGraph: MemoryGraph,
  metricsLogger: AgentMetricsLogger
): SessionRunner {
  return new SessionRunner(config, orchestrator, feedbackLoop, memoryGraph, metricsLogger);
}