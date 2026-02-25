// BigDaddyGEngine/agent.ts - Agent Bridge for BigDaddyG Orchestration
import { useBigDaddyGEngine } from './useEngine';
import { bigDaddyGRuntime } from './runtime';

export interface AgentResult {
  output: string;
  trace: {
    tokens: number;
    latency: number;
    memoryUsage: number;
  };
  metadata: {
    agentId: string;
    timestamp: number;
    model: string;
  };
}

export interface AgentConfig {
  id: string;
  name: string;
  model: string;
  specialization: string;
  capabilities: string[];
  maxTokens: number;
  temperature: number;
}

class BigDaddyGAgent {
  private config: AgentConfig;
  private engine: ReturnType<typeof useBigDaddyGEngine>;

  constructor(config: AgentConfig) {
    this.config = config;
    this.engine = useBigDaddyGEngine();
  }

  async run(task: string): Promise<AgentResult> {
    const startTime = Date.now();
    
    try {
      // Load model if not already loaded
      if (this.engine.currentModel !== this.config.model) {
        await this.engine.loadModel(this.config.model);
      }

      // Execute task with streaming
      await this.engine.stream(task, `/models/${this.config.model}.gguf`);
      
      const output = this.engine.tokens.join('');
      const latency = Date.now() - startTime;
      
      return {
        output,
        trace: {
          tokens: this.engine.tokensGenerated,
          latency,
          memoryUsage: this.engine.memoryUsage
        },
        metadata: {
          agentId: this.config.id,
          timestamp: Date.now(),
          model: this.config.model
        }
      };
    } catch (error) {
      throw new Error(`Agent ${this.config.name} failed: ${error}`);
    }
  }

  async runWithLogits(task: string): Promise<AgentResult & { logits: number[][] }> {
    const startTime = Date.now();
    
    try {
      if (this.engine.currentModel !== this.config.model) {
        await this.engine.loadModel(this.config.model);
      }

      await this.engine.streamWithLogits(task, `/models/${this.config.model}.gguf`);
      
      const output = this.engine.tokens.join('');
      const latency = Date.now() - startTime;
      const logits = this.engine.trace.map(t => t.logits || []);
      
      return {
        output,
        trace: {
          tokens: this.engine.tokensGenerated,
          latency,
          memoryUsage: this.engine.memoryUsage
        },
        metadata: {
          agentId: this.config.id,
          timestamp: Date.now(),
          model: this.config.model
        },
        logits
      };
    } catch (error) {
      throw new Error(`Agent ${this.config.name} failed: ${error}`);
    }
  }

  getStatus() {
    return {
      id: this.config.id,
      name: this.config.name,
      status: this.engine.status,
      currentModel: this.engine.currentModel,
      memoryUsage: this.engine.memoryUsage,
      tokensGenerated: this.engine.tokensGenerated,
      latency: this.engine.latency
    };
  }

  getConfig(): AgentConfig {
    return { ...this.config };
  }
}

// Predefined agent configurations
export const agentConfigs: AgentConfig[] = [
  {
    id: 'rawr',
    name: 'Rawr Agent',
    model: 'rawr',
    specialization: 'General purpose AI assistant',
    capabilities: ['text_generation', 'reasoning', 'analysis'],
    maxTokens: 1000,
    temperature: 0.7
  },
  {
    id: 'browser',
    name: 'Browser Agent',
    model: 'rawr',
    specialization: 'Web browsing and automation',
    capabilities: ['web_automation', 'dom_manipulation', 'scraping'],
    maxTokens: 500,
    temperature: 0.3
  },
  {
    id: 'parser',
    name: 'Parser Agent',
    model: 'rawr',
    specialization: 'Code parsing and analysis',
    capabilities: ['syntax_analysis', 'ast_construction', 'code_understanding'],
    maxTokens: 800,
    temperature: 0.2
  },
  {
    id: 'optimizer',
    name: 'Optimizer Agent',
    model: 'rawr',
    specialization: 'Code optimization and performance',
    capabilities: ['code_optimization', 'performance_analysis', 'refactoring'],
    maxTokens: 600,
    temperature: 0.4
  }
];

export function createAgent(config: AgentConfig): BigDaddyGAgent {
  return new BigDaddyGAgent(config);
}

export function getAgentForContext(context: string): BigDaddyGAgent {
  const config = agentConfigs.find(agent => 
    agent.specialization.toLowerCase().includes(context.toLowerCase()) ||
    agent.capabilities.some(cap => cap.includes(context.toLowerCase()))
  ) || agentConfigs[0]; // Default to rawr agent
  
  return createAgent(config);
}

export async function bigDaddyGEngineRun({ prompt, model }: { prompt: string; model: string }): Promise<AgentResult> {
  const agent = getAgentForContext('general');
  return await agent.run(prompt);
}

export async function orchestrateTask(task: string): Promise<string> {
  const result = await bigDaddyGEngineRun({ prompt: task, model: 'rawr' });
  return result.output;
}

// Agent registry for orchestration
export const agentRegistry = {
  rawr: createAgent(agentConfigs[0]),
  browser: createAgent(agentConfigs[1]),
  parser: createAgent(agentConfigs[2]),
  optimizer: createAgent(agentConfigs[3])
};

export function getAgentRegistry() {
  return agentRegistry;
}

export function getAgentStatuses() {
  return Object.entries(agentRegistry).map(([id, agent]) => ({
    id,
    ...agent.getStatus()
  }));
}
