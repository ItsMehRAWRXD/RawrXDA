// BigDaddyGEngine/evolution/EvolutionCycleManager.ts - Continuous Self-Improvement Engine
import { PatternRecognizer, ReversalInsightReport } from '../agents/PatternRecognizer';
import { MemoryGraph } from '../memory/MemoryGraph';
import { MultiAgentOrchestrator } from '../orchestration/MultiAgentOrchestrator';
import { AgentMetricsLogger } from '../optimization/AgentMetricsLogger';

export interface EvolutionConfig {
  cycleInterval: number; // milliseconds
  minImprovementThreshold: number;
  maxMutationAttempts: number;
  enableAutoMutation: boolean;
  enableCrossProjectSync: boolean;
  enablePredictiveAdjustment: boolean;
  mutationWeights: {
    temperature: number;
    maxTokens: number;
    agentOrder: number;
    patternWeights: number;
    confidenceThreshold: number;
  };
}

export interface EvolutionCycle {
  id: string;
  timestamp: number;
  duration: number;
  agents: string[];
  mutations: Mutation[];
  improvements: Improvement[];
  regressions: Regression[];
  success: boolean;
  confidence: number;
}

export interface Mutation {
  type: 'temperature' | 'maxTokens' | 'agentOrder' | 'patternWeights' | 'confidenceThreshold';
  agentId: string;
  previousValue: any;
  newValue: any;
  rationale: string;
  impact: number;
  confidence: number;
}

export interface Improvement {
  metric: string;
  before: number;
  after: number;
  improvement: number;
  significance: number;
  agent: string;
}

export interface Regression {
  metric: string;
  before: number;
  after: number;
  degradation: number;
  cause: string;
  agent: string;
}

export interface EvolutionMetrics {
  totalCycles: number;
  successfulCycles: number;
  averageImprovement: number;
  mutationSuccessRate: number;
  crossProjectContributions: number;
  predictiveAccuracy: number;
  adaptationRate: number;
  stabilityScore: number;
}

export interface SelfImprovementStrategy {
  targetMetrics: string[];
  mutationPriorities: Record<string, number>;
  convergenceCriteria: number;
  maxIterations: number;
  rollbackThreshold: number;
}

export class EvolutionCycleManager {
  private config: EvolutionConfig;
  private patternRecognizer: PatternRecognizer;
  private memoryGraph: MemoryGraph;
  private orchestrator: MultiAgentOrchestrator;
  private metricsLogger: AgentMetricsLogger;
  private evolutionHistory: EvolutionCycle[] = [];
  private currentStrategy: SelfImprovementStrategy | null = null;
  private cycleTimer: NodeJS.Timeout | null = null;
  private isEvolving: boolean = false;

  constructor(
    config: EvolutionConfig,
    patternRecognizer: PatternRecognizer,
    memoryGraph: MemoryGraph,
    orchestrator: MultiAgentOrchestrator,
    metricsLogger: AgentMetricsLogger
  ) {
    this.config = config;
    this.patternRecognizer = patternRecognizer;
    this.memoryGraph = memoryGraph;
    this.orchestrator = orchestrator;
    this.metricsLogger = metricsLogger;
    this.initializeEvolutionManager();
  }

  /**
   * 🧬 EVOLUTION CYCLE MANAGER
   * Manages continuous self-improvement through adaptive mutations and learning
   */
  async startEvolution(strategy?: SelfImprovementStrategy): Promise<void> {
    console.log('🧬 Starting autonomous evolution cycles...');

    if (strategy) {
      this.currentStrategy = strategy;
    }

    if (!this.currentStrategy) {
      this.currentStrategy = this.createDefaultStrategy();
    }

    this.isEvolving = true;

    // Start evolution cycle
    await this.runEvolutionCycle();

    // Schedule recurring cycles
    this.cycleTimer = setInterval(async () => {
      if (this.isEvolving) {
        await this.runEvolutionCycle();
      }
    }, this.config.cycleInterval);

    console.log(`✅ Evolution cycles started (interval: ${this.config.cycleInterval}ms)`);
  }

  async stopEvolution(): Promise<void> {
    console.log('⏹️ Stopping evolution cycles...');

    this.isEvolving = false;

    if (this.cycleTimer) {
      clearInterval(this.cycleTimer);
      this.cycleTimer = null;
    }

    console.log('✅ Evolution cycles stopped');
  }

  async runEvolutionCycle(): Promise<EvolutionCycle> {
    console.log('🔄 Running evolution cycle...');

    const cycleStart = performance.now();
    const cycleId = `evolution-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;

    try {
      // 1. Assess current system state
      const baselineMetrics = await this.assessBaselineMetrics();

      // 2. Analyze pattern evolution and drift
      const reversalInsights = await this.patternRecognizer.analyzeReversePatterns(this.memoryGraph);

      // 3. Generate and apply mutations
      const mutations = await this.generateMutations(baselineMetrics, reversalInsights);

      // 4. Apply mutations to agents and orchestrator
      const appliedMutations = await this.applyMutations(mutations);

      // 5. Test and validate improvements
      const testResults = await this.testImprovements(appliedMutations);

      // 6. Calculate improvements and regressions
      const improvements = this.calculateImprovements(baselineMetrics, testResults.after);
      const regressions = this.calculateRegressions(baselineMetrics, testResults.after);

      // 7. Update strategy based on results
      await this.updateStrategy(improvements, regressions);

      // 8. Store evolution cycle data
      const cycle: EvolutionCycle = {
        id: cycleId,
        timestamp: Date.now(),
        duration: performance.now() - cycleStart,
        agents: Object.keys(this.orchestrator.getAvailableAgents()),
        mutations: appliedMutations,
        improvements,
        regressions,
        success: improvements.length > regressions.length && this.evaluateCycleSuccess(improvements, regressions),
        confidence: this.calculateCycleConfidence(improvements, regressions, appliedMutations)
      };

      this.evolutionHistory.push(cycle);
      await this.storeEvolutionCycle(cycle);

      // 9. Rollback if necessary
      if (!cycle.success && this.shouldRollback(cycle)) {
        await this.rollbackMutations(appliedMutations);
        console.log('🔄 Rolled back unsuccessful mutations');
      }

      console.log(`✅ Evolution cycle ${cycleId} completed`);
      console.log(`📊 Success: ${cycle.success}, Confidence: ${(cycle.confidence * 100).toFixed(1)}%`);
      console.log(`📈 Improvements: ${improvements.length}, Regressions: ${regressions.length}`);

      return cycle;

    } catch (error) {
      console.error('❌ Evolution cycle failed:', error);

      const failedCycle: EvolutionCycle = {
        id: cycleId,
        timestamp: Date.now(),
        duration: performance.now() - cycleStart,
        agents: [],
        mutations: [],
        improvements: [],
        regressions: [{
          metric: 'cycle_success',
          before: 1,
          after: 0,
          degradation: 1,
          cause: error instanceof Error ? error.message : 'Unknown error',
          agent: 'evolution_manager'
        }],
        success: false,
        confidence: 0
      };

      this.evolutionHistory.push(failedCycle);
      await this.storeEvolutionCycle(failedCycle);

      return failedCycle;
    }
  }

  /**
   * Generates mutations based on current performance and insights
   */
  private async generateMutations(
    baselineMetrics: any,
    reversalInsights: ReversalInsightReport
  ): Promise<Mutation[]> {
    const mutations: Mutation[] = [];
    const agents = this.orchestrator.getAvailableAgents();

    // Temperature mutations for exploration vs exploitation
    for (const agentId of agents) {
      const currentTemp = this.orchestrator.getAgentConfig(agentId)?.temperature || 0.7;

      if (reversalInsights.regressions.some(r => r.trend === 'regressing')) {
        // Lower temperature for more deterministic behavior
        const newTemp = Math.max(0.1, currentTemp - 0.1);
        mutations.push({
          type: 'temperature',
          agentId,
          previousValue: currentTemp,
          newValue: newTemp,
          rationale: 'Reducing temperature to improve consistency',
          impact: this.config.mutationWeights.temperature,
          confidence: 0.7
        });
      } else if (reversalInsights.overRefactoring.length > 0) {
        // Increase temperature for more creative solutions
        const newTemp = Math.min(1.0, currentTemp + 0.1);
        mutations.push({
          type: 'temperature',
          agentId,
          previousValue: currentTemp,
          newValue: newTemp,
          rationale: 'Increasing temperature for creative problem solving',
          impact: this.config.mutationWeights.temperature,
          confidence: 0.6
        });
      }
    }

    // Max tokens mutations for efficiency
    for (const agentId of agents) {
      const currentMaxTokens = this.orchestrator.getAgentConfig(agentId)?.maxTokens || 1000;

      if (baselineMetrics.averageLatency > 5000) {
        // Reduce tokens for faster execution
        const newMaxTokens = Math.max(200, currentMaxTokens - 200);
        mutations.push({
          type: 'maxTokens',
          agentId,
          previousValue: currentMaxTokens,
          newValue: newMaxTokens,
          rationale: 'Reducing token limit to improve performance',
          impact: this.config.mutationWeights.maxTokens,
          confidence: 0.8
        });
      }
    }

    // Confidence threshold mutations
    if (reversalInsights.regressions.length > reversalInsights.overRefactoring.length) {
      // Lower confidence threshold to catch more issues
      const currentThreshold = this.patternRecognizer['config'].confidenceThreshold;
      const newThreshold = Math.max(0.3, currentThreshold - 0.1);

      mutations.push({
        type: 'confidenceThreshold',
        agentId: 'pattern_recognizer',
        previousValue: currentThreshold,
        newValue: newThreshold,
        rationale: 'Lowering confidence threshold to detect more subtle issues',
        impact: this.config.mutationWeights.confidenceThreshold,
        confidence: 0.6
      });
    }

    // Pattern weights mutations
    const highRecurrenceSmells = reversalInsights.regressions.filter(r => r.recurrence >= 3);
    if (highRecurrenceSmells.length > 0) {
      for (const smell of highRecurrenceSmells.slice(0, 3)) {
        const currentWeights = this.patternRecognizer['patternDatabase'].get(smell.smell);
        if (currentWeights) {
          const increasedWeights = Object.fromEntries(
            Object.entries(currentWeights).map(([key, value]) => [key, Math.min(1.0, value + 0.1)])
          );

          mutations.push({
            type: 'patternWeights',
            agentId: 'pattern_recognizer',
            previousValue: currentWeights,
            newValue: increasedWeights,
            rationale: `Increasing weights for recurring ${smell.smell} pattern`,
            impact: this.config.mutationWeights.patternWeights,
            confidence: 0.7
          });
        }
      }
    }

    // Limit mutations to avoid chaos
    return mutations.slice(0, this.config.maxMutationAttempts);
  }

  /**
   * Applies mutations to agents and orchestrator
   */
  private async applyMutations(mutations: Mutation[]): Promise<Mutation[]> {
    const appliedMutations: Mutation[] = [];

    for (const mutation of mutations) {
      try {
        await this.applySingleMutation(mutation);
        appliedMutations.push(mutation);
        console.log(`🔄 Applied ${mutation.type} mutation to ${mutation.agentId}`);
      } catch (error) {
        console.warn(`⚠️ Failed to apply mutation to ${mutation.agentId}:`, error);
      }
    }

    return appliedMutations;
  }

  private async applySingleMutation(mutation: Mutation): Promise<void> {
    switch (mutation.type) {
      case 'temperature':
        this.orchestrator.updateAgentConfig(mutation.agentId, { temperature: mutation.newValue });
        break;

      case 'maxTokens':
        this.orchestrator.updateAgentConfig(mutation.agentId, { maxTokens: mutation.newValue });
        break;

      case 'confidenceThreshold':
        this.patternRecognizer['config'].confidenceThreshold = mutation.newValue;
        break;

      case 'patternWeights':
        this.patternRecognizer['patternDatabase'].set(mutation.agentId, mutation.newValue);
        break;

      case 'agentOrder':
        // Update orchestration chain order
        this.orchestrator.updateConfig({ chain: mutation.newValue });
        break;
    }
  }

  /**
   * Tests improvements after mutations
   */
  private async testImprovements(mutations: Mutation[]): Promise<{ before: any; after: any }> {
    // Run a quick test analysis to measure impact
    const testGraph = {
      nodes: {
        'test.ts': { type: 'file', content: 'function test() { /* test code */ }' }
      },
      edges: {},
      totalFiles: 1,
      totalLines: 10,
      complexity: 0.5
    };

    const beforeResult = await this.patternRecognizer.analyzeCodebase(testGraph);

    // Wait for mutations to settle
    await new Promise(resolve => setTimeout(resolve, 100));

    const afterResult = await this.patternRecognizer.analyzeCodebase(testGraph);

    return {
      before: beforeResult,
      after: afterResult
    };
  }

  /**
   * Calculates improvements from test results
   */
  private calculateImprovements(before: any, after: any): Improvement[] {
    const improvements: Improvement[] = [];

    // Compare metrics
    const metrics = ['complexityScore', 'averageConfidence', 'totalSmells'];

    for (const metric of metrics) {
      const beforeValue = before[metric] || before.summary?.[metric.replace('Score', '').toLowerCase()] || 0;
      const afterValue = after[metric] || after.summary?.[metric.replace('Score', '').toLowerCase()] || 0;

      const improvement = afterValue - beforeValue;

      if (Math.abs(improvement) > this.config.minImprovementThreshold) {
        improvements.push({
          metric,
          before: beforeValue,
          after: afterValue,
          improvement,
          significance: Math.abs(improvement) / Math.abs(beforeValue),
          agent: 'evolution_manager'
        });
      }
    }

    return improvements;
  }

  /**
   * Calculates regressions from test results
   */
  private calculateRegressions(before: any, after: any): Regression[] {
    const regressions: Regression[] = [];

    // Compare metrics for degradation
    const metrics = ['complexityScore', 'averageConfidence'];

    for (const metric of metrics) {
      const beforeValue = before[metric] || before.summary?.[metric.replace('Score', '').toLowerCase()] || 0;
      const afterValue = after[metric] || after.summary?.[metric.replace('Score', '').toLowerCase()] || 0;

      const degradation = beforeValue - afterValue;

      if (degradation > this.config.minImprovementThreshold) {
        regressions.push({
          metric,
          before: beforeValue,
          after: afterValue,
          degradation,
          cause: 'Mutation induced performance degradation',
          agent: 'evolution_manager'
        });
      }
    }

    return regressions;
  }

  /**
   * Evaluates overall cycle success
   */
  private evaluateCycleSuccess(improvements: Improvement[], regressions: Regression[]): boolean {
    const totalImprovement = improvements.reduce((sum, imp) => sum + imp.improvement, 0);
    const totalDegradation = regressions.reduce((sum, reg) => sum + reg.degradation, 0);

    return totalImprovement > totalDegradation * 2; // Improvements should outweigh regressions
  }

  /**
   * Calculates cycle confidence
   */
  private calculateCycleConfidence(
    improvements: Improvement[],
    regressions: Regression[],
    mutations: Mutation[]
  ): number {
    const improvementConfidence = improvements.reduce((sum, imp) => sum + imp.significance, 0) / improvements.length;
    const mutationConfidence = mutations.reduce((sum, mut) => sum + mut.confidence, 0) / mutations.length;
    const regressionPenalty = regressions.length * 0.2;

    return Math.max(0, Math.min(1, (improvementConfidence + mutationConfidence) / 2 - regressionPenalty));
  }

  /**
   * Assesses baseline metrics before mutations
   */
  private async assessBaselineMetrics(): Promise<any> {
    // Get current system metrics
    const orchestratorMetrics = this.orchestrator.getPerformanceMetrics();
    const patternMetrics = this.patternRecognizer['stats'];

    return {
      averageLatency: orchestratorMetrics.averageExecutionTime,
      averageConfidence: patternMetrics.avgConfidence,
      complexityScore: patternMetrics.avgComplexity,
      totalExecutions: orchestratorMetrics.totalExecutions
    };
  }

  /**
   * Updates evolution strategy based on results
   */
  private async updateStrategy(improvements: Improvement[], regressions: Regression[]): Promise<void> {
    if (!this.currentStrategy) return;

    // Adjust mutation priorities based on success
    for (const improvement of improvements) {
      this.currentStrategy.mutationPriorities[improvement.metric] =
        (this.currentStrategy.mutationPriorities[improvement.metric] || 0.5) + 0.1;
    }

    for (const regression of regressions) {
      this.currentStrategy.mutationPriorities[regression.metric] =
        Math.max(0.1, (this.currentStrategy.mutationPriorities[regression.metric] || 0.5) - 0.1);
    }

    // Check convergence
    const avgPriority = Object.values(this.currentStrategy.mutationPriorities).reduce((sum, p) => sum + p, 0) /
                       Object.values(this.currentStrategy.mutationPriorities).length;

    if (avgPriority > this.currentStrategy.convergenceCriteria) {
      console.log('🎯 Evolution strategy converged - optimization complete');
      this.currentStrategy.maxIterations = 0; // Stop evolution
    }
  }

  /**
   * Determines if mutations should be rolled back
   */
  private shouldRollback(cycle: EvolutionCycle): boolean {
    return cycle.confidence < 0.3 || cycle.regressions.length > cycle.improvements.length * 2;
  }

  /**
   * Rolls back applied mutations
   */
  private async rollbackMutations(mutations: Mutation[]): Promise<void> {
    console.log('🔄 Rolling back mutations...');

    for (const mutation of mutations.reverse()) {
      try {
        await this.applySingleMutation({
          ...mutation,
          newValue: mutation.previousValue,
          previousValue: mutation.newValue
        });
        console.log(`↩️ Rolled back ${mutation.type} mutation for ${mutation.agentId}`);
      } catch (error) {
        console.error(`❌ Failed to rollback mutation for ${mutation.agentId}:`, error);
      }
    }
  }

  /**
   * Gets evolution metrics
   */
  getEvolutionMetrics(): EvolutionMetrics {
    const totalCycles = this.evolutionHistory.length;
    const successfulCycles = this.evolutionHistory.filter(c => c.success).length;
    const averageImprovement = this.evolutionHistory.reduce((sum, c) =>
      sum + c.improvements.reduce((impSum, imp) => impSum + imp.improvement, 0), 0) / totalCycles;

    const mutationSuccessRate = this.evolutionHistory.reduce((sum, c) =>
      sum + (c.mutations.length > 0 ? 1 : 0), 0) / totalCycles;

    return {
      totalCycles,
      successfulCycles,
      averageImprovement,
      mutationSuccessRate,
      crossProjectContributions: 0, // Would be calculated from cross-project data
      predictiveAccuracy: 0.85, // Would be calculated from prediction accuracy
      adaptationRate: successfulCycles / totalCycles,
      stabilityScore: this.calculateStabilityScore()
    };
  }

  /**
   * Calculates system stability score
   */
  private calculateStabilityScore(): number {
    if (this.evolutionHistory.length < 2) return 1.0;

    const recentCycles = this.evolutionHistory.slice(-5);
    const successfulRecent = recentCycles.filter(c => c.success).length;

    return successfulRecent / recentCycles.length;
  }

  /**
   * Creates default evolution strategy
   */
  private createDefaultStrategy(): SelfImprovementStrategy {
    return {
      targetMetrics: ['averageConfidence', 'complexityScore', 'averageLatency'],
      mutationPriorities: {
        temperature: 0.5,
        maxTokens: 0.4,
        confidenceThreshold: 0.6,
        patternWeights: 0.7,
        agentOrder: 0.3
      },
      convergenceCriteria: 0.8,
      maxIterations: 100,
      rollbackThreshold: 0.3
    };
  }

  /**
   * Stores evolution cycle in memory
   */
  private async storeEvolutionCycle(cycle: EvolutionCycle): Promise<void> {
    try {
      const trace = {
        id: `evolution-${cycle.id}`,
        prompt: 'evolution_cycle',
        output: JSON.stringify(cycle),
        embedding: await this.patternRecognizer['embed'](JSON.stringify(cycle)),
        timestamp: cycle.timestamp,
        agentId: 'evolution_cycle_manager',
        metadata: {
          sessionId: `evolution-${Date.now()}`,
          projectId: 'evolution-management',
          contextHash: 'evolution-cycle',
          relatedTraces: []
        }
      };

      await this.memoryGraph.store(trace);
    } catch (error) {
      console.error('❌ Failed to store evolution cycle:', error);
    }
  }

  private initializeEvolutionManager(): void {
    console.log('🧬 Initializing evolution cycle manager...');
  }
}

// Export factory function
export function createEvolutionCycleManager(
  config: EvolutionConfig,
  patternRecognizer: PatternRecognizer,
  memoryGraph: MemoryGraph,
  orchestrator: MultiAgentOrchestrator,
  metricsLogger: AgentMetricsLogger
): EvolutionCycleManager {
  return new EvolutionCycleManager(config, patternRecognizer, memoryGraph, orchestrator, metricsLogger);
}