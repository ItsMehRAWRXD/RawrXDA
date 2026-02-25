// BigDaddyGEngine/agent.ts - Agent Bridge for BigDaddyG Orchestration
// Fixed: Removed React hook usage from class constructor
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

interface AgentState {
  status: 'idle' | 'loading' | 'running' | 'error';
  tokens: string[];
  trace: Array<{ token: string; index: number; timestamp: number; logits?: number[] }>;
  currentModel: string | null;
  memoryUsage: number;
  tokensGenerated: number;
  latency: number;
}

class BigDaddyGAgent {
  private config: AgentConfig;
  private state: AgentState;

  constructor(config: AgentConfig) {
    this.config = config;
    this.state = {
      status: 'idle',
      tokens: [],
      trace: [],
      currentModel: null,
      memoryUsage: 0,
      tokensGenerated: 0,
      latency: 0
    };
  }

  async run(task: string): Promise<AgentResult> {
    const startTime = Date.now();
    
    try {
      this.state.status = 'loading';
      
      // Load model if not already loaded
      if (this.state.currentModel !== this.config.model) {
        await bigDaddyGRuntime.loadModel(this.config.model);
        this.state.currentModel = this.config.model;
      }

      this.state.status = 'running';
      this.state.tokens = [];
      this.state.trace = [];
      
      // Execute task with streaming
      const tokens: string[] = [];
      const trace: Array<{ token: string; index: number; timestamp: number; logits?: number[] }> = [];
      
      for await (const streamingToken of bigDaddyGRuntime.streamTokens(task, this.config.maxTokens)) {
        tokens.push(streamingToken.token);
        trace.push({
          token: streamingToken.token,
          index: streamingToken.index,
          timestamp: streamingToken.timestamp,
          logits: streamingToken.logits
        });
        
        this.state.tokens = [...tokens];
        this.state.trace = [...trace];
        this.state.tokensGenerated = tokens.length;
        this.state.latency = streamingToken.timestamp - trace[0]?.timestamp || 0;
      }
      
      this.state.status = 'idle';
      const output = tokens.join('');
      const latency = Date.now() - startTime;
      
      return {
        output,
        trace: {
          tokens: this.state.tokensGenerated,
          latency,
          memoryUsage: this.state.memoryUsage
        },
        metadata: {
          agentId: this.config.id,
          timestamp: Date.now(),
          model: this.config.model
        }
      };
    } catch (error) {
      this.state.status = 'error';
      throw new Error(`Agent ${this.config.name} failed: ${error}`);
    }
  }

  async runWithLogits(task: string): Promise<AgentResult & { logits: number[][] }> {
    const startTime = Date.now();
    
    try {
      this.state.status = 'loading';
      
      if (this.state.currentModel !== this.config.model) {
        await bigDaddyGRuntime.loadModel(this.config.model);
        this.state.currentModel = this.config.model;
      }

      this.state.status = 'running';
      this.state.tokens = [];
      this.state.trace = [];
      
      const trace: Array<{ token: string; index: number; timestamp: number; logits?: number[] }> = [];
      let index = 0;
      
      for await (const { token, logits } of bigDaddyGRuntime.streamWithLogits(task, this.config.maxTokens)) {
        trace.push({
          token,
          index: index++,
          timestamp: Date.now(),
          logits
        });
        
        this.state.trace = [...trace];
        this.state.tokensGenerated = trace.length;
        this.state.latency = Date.now() - trace[0]?.timestamp || 0;
      }
      
      this.state.status = 'idle';
      const output = trace.map(t => t.token).join('');
      const latency = Date.now() - startTime;
      const logits = trace.map(t => t.logits || []);
      
      return {
        output,
        trace: {
          tokens: this.state.tokensGenerated,
          latency,
          memoryUsage: this.state.memoryUsage
        },
        metadata: {
          agentId: this.config.id,
          timestamp: Date.now(),
          model: this.config.model
        },
        logits
      };
    } catch (error) {
      this.state.status = 'error';
      throw new Error(`Agent ${this.config.name} failed: ${error}`);
    }
  }

  getStatus() {
    return {
      id: this.config.id,
      name: this.config.name,
      status: this.state.status,
      currentModel: this.state.currentModel,
      memoryUsage: this.state.memoryUsage,
      tokensGenerated: this.state.tokensGenerated,
      latency: this.state.latency
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
  return Object.entries(agentRegistry).map(([, agent]) => agent.getStatus());
}
