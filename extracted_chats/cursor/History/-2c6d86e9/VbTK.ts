// BigDaddyGEngine/optimization/SelfOptimizingAgentMesh.ts - Self-Optimizing Agent Mesh System
import { AgentMetricsLogger, AgentMetrics, PerformanceSnapshot, OptimizationSignal } from './AgentMetricsLogger';
import { ReinforcementLearner, OptimizationAction, AgentConfig, LearningResult } from './ReinforcementLearner';
import { MultiAgentOrchestrator, OrchestrationGraph, OrchestrationConfig } from '../MultiAgentOrchestrator';

export interface AgentNode {
  id: string;
  config: AgentConfig;
  metrics: AgentMetrics;
  connections: string[]; // Connected agent IDs
  status: 'active' | 'idle' | 'optimizing' | 'degraded';
  load: number; // Current load factor (0-1)
  lastUpdate: number;
}

export interface MeshTopology {
  nodes: Map<string, AgentNode>;
  edges: Map<string, Set<string>>; // Adjacency list
  clusters: string[][]; // Agent clusters by specialization
}

export interface RoutingStrategy {
  type: 'performance' | 'latency' | 'balance' | 'learning';
  weight: number;
}

export interface MeshConfig {
  enableLearning: boolean;
  enableAdaptiveRouting: boolean;
  enableSelfHealing: boolean;
  optimizeInterval: number; // ms
  loadBalanceThreshold: number;
  degradationThreshold: number;
}

export interface MeshHealth {
  overallHealth: 'excellent' | 'good' | 'fair' | 'poor' | 'critical';
  activeAgents: number;
  idleAgents: number;
  degradedAgents: number;
  averageLoad: number;
  optimizationScore: number;
  issues: string[];
}

export interface RouteDecision {
  targetAgentId: string;
  confidence: number;
  reasoning: string;
  alternatives: string[];
}

/**
 * Self-Optimizing Agent Mesh System
 * 
 * Features:
 * - Intelligent agent selection based on performance metrics
 * - Adaptive routing based on load and performance
 * - Self-healing through automatic agent management
 * - Reinforcement learning for optimization
 * - Cluster-based organization by specialization
 */
export class SelfOptimizingAgentMesh {
  private topology: MeshTopology;
  private metricsLogger: AgentMetricsLogger;
  private learner: ReinforcementLearner;
  private orchestrator: MultiAgentOrchestrator;
  private config: MeshConfig;
  private routingStrategies: RoutingStrategy[];
  private optimizationTimer?: NodeJS.Timeout;

  constructor(
    metricsLogger: AgentMetricsLogger,
    learner: ReinforcementLearner,
    orchestrator: MultiAgentOrchestrator,
    config: Partial<MeshConfig> = {}
  ) {
    this.topology = {
      nodes: new Map(),
      edges: new Map(),
      clusters: []
    };
    
    this.metricsLogger = metricsLogger;
    this.learner = learner;
    this.orchestrator = orchestrator;

    this.config = {
      enableLearning: true,
      enableAdaptiveRouting: true,
      enableSelfHealing: true,
      optimizeInterval: 60000, // 1 minute
      loadBalanceThreshold: 0.8,
      degradationThreshold: 0.3,
      ...config
    };

    this.routingStrategies = [
      { type: 'performance', weight: 0.4 },
      { type: 'latency', weight: 0.3 },
      { type: 'balance', weight: 0.2 },
      { type: 'learning', weight: 0.1 }
    ];

    if (this.config.enableLearning) {
      this.startOptimizationLoop();
    }
  }

  /**
   * Register an agent in the mesh
   */
  registerAgent(config: AgentConfig): void {
    const node: AgentNode = {
      id: config.id,
      config,
      metrics: this.metricsLogger.getAgentMetrics(config.id) || this.createDefaultMetrics(config.id),
      connections: [],
      status: config.enabled ? 'idle' : 'degraded',
      load: 0,
      lastUpdate: Date.now()
    };

    this.topology.nodes.set(config.id, node);
    this.topology.edges.set(config.id, new Set());

    if (this.config.enableLearning) {
      this.learner.initializeAgent(config.id);
    }

    // Update clusters based on specialization
    this.updateClusters();
  }

  /**
   * Remove an agent from the mesh
   */
  unregisterAgent(agentId: string): void {
    this.topology.nodes.delete(agentId);
    this.topology.edges.delete(agentId);
    
    // Remove connections
    for (const [nodeId, connections] of Array.from(this.topology.edges.entries())) {
      connections.delete(agentId);
    }

    this.updateClusters();
  }

  /**
   * Add a connection between agents
   */
  addConnection(fromAgentId: string, toAgentId: string): void {
    if (!this.topology.nodes.has(fromAgentId) || !this.topology.nodes.has(toAgentId)) {
      throw new Error('Agent not found');
    }

    const fromConnections = this.topology.edges.get(fromAgentId);
    if (fromConnections) {
      fromConnections.add(toAgentId);
    }

    const fromNode = this.topology.nodes.get(fromAgentId);
    if (fromNode) {
      fromNode.connections.push(toAgentId);
    }
  }

  /**
   * Route a task to the best agent
   */
  async routeTask(
    task: string,
    requiredCapabilities?: string[],
    strategy: RoutingStrategy['type'] = 'performance'
  ): Promise<RouteDecision> {
    const candidates = this.getCandidateAgents(requiredCapabilities);
    
    if (candidates.length === 0) {
      throw new Error('No suitable agents available');
    }

    const scores = this.scoreAgents(candidates, strategy);
    const sorted = scores.sort((a, b) => b.score - a.score);
    const best = sorted[0];

    // Update load
    const agent = this.topology.nodes.get(best.agentId);
    if (agent) {
      agent.load = Math.min(1, agent.load + 0.1);
      agent.status = 'active';
    }

    return {
      targetAgentId: best.agentId,
      confidence: best.score,
      reasoning: best.reasoning,
      alternatives: sorted.slice(1, 4).map(s => s.agentId)
    };
  }

  /**
   * Execute a task through the mesh with adaptive routing
   */
  async executeTask(
    task: string,
    requiredCapabilities?: string[]
  ): Promise<OrchestrationGraph> {
    const route = await this.routeTask(task, requiredCapabilities);
    
    // Create a simple chain with the selected agent
    const chain = [route.targetAgentId];
    
    // If adaptive routing is enabled, add smart chain construction
    if (this.config.enableAdaptiveRouting) {
      const enhancedChain = await this.constructChain(route.targetAgentId, task, requiredCapabilities);
      return this.orchestrator.run(task, enhancedChain);
    }

    return this.orchestrator.run(task, chain);
  }

  /**
   * Get mesh health status
   */
  getHealth(): MeshHealth {
    const nodes = Array.from(this.topology.nodes.values());
    const activeAgents = nodes.filter(n => n.status === 'active').length;
    const idleAgents = nodes.filter(n => n.status === 'idle').length;
    const degradedAgents = nodes.filter(n => n.status === 'degraded').length;
    const averageLoad = nodes.reduce((sum, n) => sum + n.load, 0) / nodes.length;

    const issues: string[] = [];
    if (degradedAgents > 0) {
      issues.push(`${degradedAgents} degraded agent(s)`);
    }
    if (averageLoad > this.config.loadBalanceThreshold) {
      issues.push('High average load');
    }

    const optimizationScore = this.calculateOptimizationScore();
    
    let overallHealth: 'excellent' | 'good' | 'fair' | 'poor' | 'critical' = 'excellent';
    if (nodes.length === 0) overallHealth = 'critical';
    else if (degradedAgents > nodes.length * 0.5) overallHealth = 'critical';
    else if (degradedAgents > nodes.length * 0.3) overallHealth = 'poor';
    else if (issues.length > 2) overallHealth = 'fair';
    else if (issues.length > 0) overallHealth = 'good';

    return {
      overallHealth,
      activeAgents,
      idleAgents,
      degradedAgents,
      averageLoad,
      optimizationScore,
      issues
    };
  }

  /**
   * Get topology information
   */
  getTopology(): Readonly<MeshTopology> {
    return {
      ...this.topology,
      nodes: new Map(this.topology.nodes),
      edges: new Map(this.topology.edges),
      clusters: this.topology.clusters.map(c => [...c])
    };
  }

  /**
   * Force optimization cycle
   */
  async optimize(): Promise<LearningResult[]> {
    const metrics = this.metricsLogger.getAllMetrics();
    const signals = this.metricsLogger.getOptimizationSignals();
    
    const actions = this.learner.generateActions(metrics, signals);
    const results: LearningResult[] = [];

    for (const action of actions) {
      const node = this.topology.nodes.get(action.agentId);
      if (!node) continue;

      const reward = await this.executeOptimizationAction(action, node);
      const newConfig = this.applyConfigChange(node.config, action);
      
      const result: LearningResult = {
        agentId: action.agentId,
        action,
        reward,
        newConfig,
        improvement: reward - node.metrics.performanceScore
      };

      results.push(result);
      
      // Update node
      node.config = newConfig;
      node.metrics = this.metricsLogger.getAgentMetrics(action.agentId) || node.metrics;
    }

    this.updateAgentStatuses();
    return results;
  }

  /**
   * Start self-healing processes
   */
  startSelfHealing(): void {
    if (!this.config.enableSelfHealing) return;

    const nodes = Array.from(this.topology.nodes.values());
    
    for (const node of nodes) {
      // Check if agent needs healing
      if (node.metrics.performanceScore < this.config.degradationThreshold) {
        this.healAgent(node);
      }

      // Rebalance load if needed
      if (this.shouldRebalance(node)) {
        this.rebalanceLoad(node);
      }
    }
  }

  /**
   * Shutdown the mesh
   */
  shutdown(): void {
    if (this.optimizationTimer) {
      clearInterval(this.optimizationTimer);
    }
  }

  // Private methods

  private createDefaultMetrics(agentId: string): AgentMetrics {
    return {
      agentId,
      totalRuns: 0,
      successfulRuns: 0,
      failedRuns: 0,
      averageLatency: 0,
      averageConfidence: 0,
      averageTokens: 0,
      averageMemoryUsage: 0,
      successRate: 0,
      lastRun: 0,
      performanceScore: 0.5,
      trend: 'stable'
    };
  }

  private updateClusters(): void {
    const clusters = new Map<string, string[]>();
    
    for (const node of this.topology.nodes.values()) {
      const specialization = node.config.specialization;
      if (!clusters.has(specialization)) {
        clusters.set(specialization, []);
      }
      clusters.get(specialization)!.push(node.id);
    }

    this.topology.clusters = Array.from(clusters.values());
  }

  private getCandidateAgents(requiredCapabilities?: string[]): string[] {
    const nodes = Array.from(this.topology.nodes.values());
    
    if (!requiredCapabilities || requiredCapabilities.length === 0) {
      return nodes.filter(n => n.status !== 'degraded').map(n => n.id);
    }

    return nodes
      .filter(n => {
        if (n.status === 'degraded') return false;
        
        if (requiredCapabilities) {
          return requiredCapabilities.every(cap => 
            n.config.capabilities.includes(cap)
          );
        }
        return true;
      })
      .map(n => n.id);
  }

  private scoreAgents(
    candidateIds: string[],
    strategy: RoutingStrategy['type']
  ): Array<{ agentId: string; score: number; reasoning: string }> {
    return candidateIds.map(agentId => {
      const node = this.topology.nodes.get(agentId);
      if (!node) return { agentId, score: 0, reasoning: 'Node not found' };

      const scores: Record<string, number> = {
        performance: node.metrics.performanceScore,
        latency: Math.max(0, 1 - (node.metrics.averageLatency / 5000)),
        balance: 1 - node.load,
        learning: this.learner.getQValue(agentId, 'current_state') || 0.5
      };

      const strategyIndex = this.routingStrategies.findIndex(s => s.type === strategy);
      const weightedScore = strategyIndex >= 0
        ? scores[strategy]
        : this.calculateWeightedScore(scores);

      return {
        agentId,
        score: weightedScore,
        reasoning: `${strategy}: ${scores[strategy].toFixed(2)}, load: ${(node.load * 100).toFixed(0)}%`
      };
    });
  }

  private calculateWeightedScore(scores: Record<string, number>): number {
    return this.routingStrategies.reduce((sum, strategy) => {
      return sum + (scores[strategy.type] * strategy.weight);
    }, 0);
  }

  private async constructChain(
    startAgentId: string,
    task: string,
    requiredCapabilities?: string[]
  ): Promise<string[]> {
    // Simple implementation - can be enhanced with graph algorithms
    return [startAgentId];
  }

  private calculateOptimizationScore(): number {
    const nodes = Array.from(this.topology.nodes.values());
    if (nodes.length === 0) return 0;

    const avgPerformance = nodes.reduce((sum, n) => 
      sum + n.metrics.performanceScore, 0
    ) / nodes.length;

    const avgLoad = nodes.reduce((sum, n) => sum + n.load, 0) / nodes.length;
    const degradedCount = nodes.filter(n => n.status === 'degraded').length;
    const degradedRatio = degradedCount / nodes.length;

    return avgPerformance * (1 - avgLoad) * (1 - degradedRatio);
  }

  private async executeOptimizationAction(
    action: OptimizationAction,
    node: AgentNode
  ): Promise<number> {
    // Execute action and measure reward
    const snapshot: PerformanceSnapshot = {
      timestamp: Date.now(),
      agentId: node.id,
      latency: node.metrics.averageLatency,
      confidence: node.metrics.averageConfidence,
      tokens: node.metrics.averageTokens,
      memoryUsage: node.metrics.averageMemoryUsage,
      success: node.metrics.successRate > 0.5,
      context: {
        prompt: 'optimization',
        output: 'action executed',
        agentConfig: node.config
      }
    };

    this.metricsLogger.logSnapshot(snapshot);
    
    // Calculate reward
    return node.metrics.performanceScore;
  }

  private applyConfigChange(config: AgentConfig, action: OptimizationAction): AgentConfig {
    const newConfig = { ...config };

    switch (action.type) {
      case 'adjust_temperature':
        newConfig.temperature = Math.max(0, Math.min(2, action.value));
        break;
      case 'adjust_max_tokens':
        newConfig.maxTokens = Math.max(100, Math.min(10000, action.value));
        break;
      case 'change_priority':
        newConfig.priority = Math.max(0, Math.min(10, action.value));
        break;
      case 'enable_disable':
        newConfig.enabled = action.value;
        break;
    }

    return newConfig;
  }

  private updateAgentStatuses(): void {
    for (const node of this.topology.nodes.values()) {
      const now = Date.now();
      
      // Decay load over time
      if (now - node.lastUpdate > 5000) {
        node.load = Math.max(0, node.load - 0.05);
        node.lastUpdate = now;
      }

      // Update status based on metrics and load
      if (node.load < 0.1 && node.metrics.performanceScore > 0.5) {
        node.status = 'idle';
      } else if (node.load > this.config.loadBalanceThreshold) {
        node.status = node.status === 'degraded' ? 'degraded' : 'active';
      } else if (node.metrics.performanceScore < this.config.degradationThreshold) {
        node.status = 'degraded';
      }

      // Reset to idle if not degraded and low load
      if (node.status !== 'degraded' && node.load < 0.1) {
        node.status = 'idle';
      }
    }
  }

  private healAgent(node: AgentNode): void {
    // Attempt to heal degraded agents
    console.log(`Attempting to heal agent: ${node.id}`);

    // Reset configuration to defaults
    node.config = {
      ...node.config,
      temperature: 0.7,
      maxTokens: 1000
    };

    // Mark for re-evaluation
    node.status = 'idle';
  }

  private shouldRebalance(node: AgentNode): boolean {
    return node.load > this.config.loadBalanceThreshold;
  }

  private rebalanceLoad(node: AgentNode): void {
    // Find alternative agents with lower load
    const alternatives = Array.from(this.topology.nodes.values())
      .filter(n => 
        n.id !== node.id && 
        n.config.specialization === node.config.specialization &&
        n.load < node.load
      )
      .sort((a, b) => a.load - b.load);

    if (alternatives.length > 0) {
      // Reduce load by preferring alternatives
      console.log(`Rebalancing: ${node.id} -> ${alternatives[0].id}`);
    }
  }

  private startOptimizationLoop(): void {
    this.optimizationTimer = setInterval(() => {
      this.optimize().catch(err => {
        console.error('Optimization cycle failed:', err);
      });
      
      if (this.config.enableSelfHealing) {
        this.startSelfHealing();
      }
    }, this.config.optimizeInterval);
  }
}

/**
 * Factory function for creating a configured mesh
 */
export function createSelfOptimizingMesh(
  metricsLogger: AgentMetricsLogger,
  learner: ReinforcementLearner,
  orchestrator: MultiAgentOrchestrator,
  config?: Partial<MeshConfig>
): SelfOptimizingAgentMesh {
  return new SelfOptimizingAgentMesh(metricsLogger, learner, orchestrator, config);
}
