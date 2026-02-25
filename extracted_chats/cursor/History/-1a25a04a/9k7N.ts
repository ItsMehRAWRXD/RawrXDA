// BigDaddyGEngine/agents/Agent.ts

export interface AgentResult {
  output: string;
  trace: {
    tokens: number;
    memoryUsage: number;
  };
}

export interface Agent {
  id: string;
  name: string;
  capabilities: string[];
  specialization: string;
  run(input: string): Promise<AgentResult>;
  getConfig(): { 
    id: string; 
    name: string; 
    model: string; 
    specialization: string; 
    capabilities: string[]; 
    maxTokens: number; 
    temperature: number 
  };
}
