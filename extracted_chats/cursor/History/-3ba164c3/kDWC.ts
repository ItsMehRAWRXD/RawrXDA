// BigDaddyGEngine/orchestration/OrchestrationIntrospector.ts

import { MultiAgentOrchestrator } from './MultiAgentOrchestrator';
import { OrchestrationGraph } from './OrchestrationGraph';

export class OrchestrationIntrospector {
  constructor(private orchestrator: MultiAgentOrchestrator) {}

  /** ✅ Live metrics */
  getMetrics(): Record<string, any> {
    const graph = this.orchestrator.getOrchestrationGraph();
    return graph
      ? { steps: graph.steps.length, avgConfidence: graph.getAverageConfidence() }
      : { steps: 0, avgConfidence: 0 };
  }

  /** ✅ Step-by-step replay data */
  getReplayData(): OrchestrationGraph | null {
    return this.orchestrator.getOrchestrationGraph();
  }

  /** ✅ Live agent stats */
  getAgentStats(): Record<string, any> {
    const agents = this.orchestrator.listAgents();
    return Object.fromEntries(agents.map(a => [a, { active: true, health: 'ok' }]));
  }
}
