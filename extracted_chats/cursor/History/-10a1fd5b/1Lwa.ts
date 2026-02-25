// BigDaddyGEngine/core/agent/AgentOrchestrator.ts
// Agent Orchestration Layer for Cooperative Multi-Agent Workflows

export interface Agent {
  name: string;
  description: string;
  tools: string[];
  execute: (task: any, context: AgentContext) => Promise<any>;
  state: Record<string, any>;
}

export interface AgentContext {
  sessionId: string;
  agents: Map<string, Agent>;
  sharedMemory: Map<string, any>;
  toolChain: Tool[];
}

export interface Tool {
  name: string;
  description: string;
  execute: (input: any, context: AgentContext) => Promise<any>;
  dependencies: string[];
}

export interface Task {
  id: string;
  type: string;
  goal: string;
  parameters: Record<string, any>;
  priority: number;
  deadline?: number;
}

export class AgentOrchestrator {
  private agents: Map<string, Agent>;
  private tools: Map<string, Tool>;
  private tasks: Task[];
  private sharedMemory: Map<string, any>;
  private executionHistory: Array<{
    taskId: string;
    agent: string;
    result: any;
    timestamp: number;
  }>;

  constructor() {
    this.agents = new Map();
    this.tools = new Map();
    this.tasks = [];
    this.sharedMemory = new Map();
    this.executionHistory = [];
    
    // Register default tools
    this.registerDefaultTools();
  }

  /**
   * Register a new agent
   */
  async registerAgent(agent: Agent): Promise<void> {
    if (this.agents.has(agent.name)) {
      console.warn(`Agent ${agent.name} already registered, updating...`);
    }
    
    this.agents.set(agent.name, agent);
    console.log(`🤖 Registered agent: ${agent.name}`);
    
    // Validate agent has required tools
    for (const toolName of agent.tools) {
      if (!this.tools.has(toolName)) {
        console.warn(`⚠️ Agent ${agent.name} requires unknown tool: ${toolName}`);
      }
    }
  }

  /**
   * Unregister an agent
   */
  unregisterAgent(agentName: string): void {
    if (this.agents.delete(agentName)) {
      console.log(`🗑️ Unregistered agent: ${agentName}`);
    }
  }

  /**
   * Register a new tool
   */
  registerTool(tool: Tool): void {
    if (this.tools.has(tool.name)) {
      console.warn(`Tool ${tool.name} already registered, updating...`);
    }
    
    this.tools.set(tool.name, tool);
    console.log(`🔧 Registered tool: ${tool.name}`);
  }

  /**
   * Execute a task with the most suitable agent
   */
  async executeTask(task: Task): Promise<any> {
    console.log(`📋 Executing task: ${task.goal}`);
    
    // Find the best agent for this task
    const agent = this.selectAgent(task);
    if (!agent) {
      throw new Error(`No suitable agent found for task: ${task.type}`);
    }

    try {
      const context: AgentContext = {
        sessionId: task.id,
        agents: this.agents,
        sharedMemory: this.sharedMemory,
        toolChain: task.type === 'chain' ? 
          this.buildToolChain(task.parameters.chain) : []
      };

      console.log(`🚀 Executing with agent: ${agent.name}`);
      const result = await agent.execute(task, context);
      
      // Record execution
      this.executionHistory.push({
        taskId: task.id,
        agent: agent.name,
        result,
        timestamp: Date.now()
      });

      console.log(`✅ Task completed: ${task.goal}`);
      return result;
      
    } catch (error) {
      console.error(`❌ Task execution failed:`, error);
      
      // Try fallback agent if available
      const fallback = this.selectFallbackAgent(task);
      if (fallback && fallback !== agent) {
        console.log(`🔄 Retrying with fallback agent: ${fallback.name}`);
        return await this.executeTaskWithAgent(task, fallback);
      }
      
      throw error;
    }
  }

  /**
   * Execute a task with a specific agent
   */
  async executeTaskWithAgent(task: Task, agent: Agent): Promise<any> {
    const context: AgentContext = {
      sessionId: task.id,
      agents: this.agents,
      sharedMemory: this.sharedMemory,
      toolChain: []
    };

    return await agent.execute(task, context);
  }

  /**
   * Execute a tool chain
   */
  async executeToolChain(tools: string[], input: any): Promise<any> {
    console.log(`🔗 Executing tool chain: ${tools.join(' -> ')}`);
    
    let result = input;
    
    for (const toolName of tools) {
      const tool = this.tools.get(toolName);
      if (!tool) {
        throw new Error(`Tool not found: ${toolName}`);
      }

      // Check dependencies
      for (const dep of tool.dependencies) {
        if (!this.tools.has(dep)) {
          throw new Error(`Missing dependency: ${dep} for tool ${toolName}`);
        }
      }

      const context: AgentContext = {
        sessionId: crypto.randomUUID(),
        agents: this.agents,
        sharedMemory: this.sharedMemory,
        toolChain: []
      };

      result = await tool.execute(result, context);
      console.log(`  ✓ ${toolName} completed`);
    }

    console.log(`✅ Tool chain completed`);
    return result;
  }

  /**
   * Select the best agent for a task
   */
  private selectAgent(task: Task): Agent | null {
    // Score agents based on task requirements
    const scoredAgents = Array.from(this.agents.values())
      .map(agent => ({
        agent,
        score: this.scoreAgent(agent, task)
      }))
      .filter(item => item.score > 0)
      .sort((a, b) => b.score - a.score);

    return scoredAgents.length > 0 ? scoredAgents[0].agent : null;
  }

  /**
   * Score an agent for a specific task
   */
  private scoreAgent(agent: Agent, task: Task): number {
    let score = 1.0;
    
    // Check if agent has required tools
    if (task.parameters.requiredTools) {
      const hasAllTools = task.parameters.requiredTools.every(
        tool => agent.tools.includes(tool)
      );
      if (!hasAllTools) return 0;
    }

    // Check description match
    const taskKeywords = task.goal.toLowerCase().split(/\s+/);
    const agentKeywords = agent.description.toLowerCase().split(/\s+/);
    const keywordMatch = taskKeywords.filter(k => agentKeywords.includes(k)).length;
    score += keywordMatch * 0.1;

    // Check historical performance
    const successfulExecutions = this.executionHistory.filter(
      e => e.agent === agent.name && e.taskId !== task.id
    ).length;
    score += Math.min(successfulExecutions * 0.1, 2.0);

    return score;
  }

  /**
   * Select a fallback agent
   */
  private selectFallbackAgent(task: Task): Agent | null {
    // Find any agent that can handle the task
    return Array.from(this.agents.values()).find(agent => {
      if (task.parameters.requiredTools) {
        return task.parameters.requiredTools.every(tool => agent.tools.includes(tool));
      }
      return true;
    }) || null;
  }

  /**
   * Build a tool chain from tool names
   */
  private buildToolChain(toolNames: string[]): Tool[] {
    return toolNames.map(name => this.tools.get(name)).filter(Boolean) as Tool[];
  }

  /**
   * Register default tools
   */
  private registerDefaultTools(): void {
    // Summarizer tool
    this.registerTool({
      name: 'summarize',
      description: 'Summarizes text or code',
      dependencies: [],
      execute: async (input, context) => {
        if (typeof input === 'string') {
          const words = input.split(/\s+/);
          return words.slice(0, Math.min(50, words.length)).join(' ') + '...';
        }
        return input;
      }
    });

    // Search tool
    this.registerTool({
      name: 'search',
      description: 'Searches for information',
      dependencies: [],
      execute: async (input, context) => {
        // Placeholder for search functionality
        return `Search results for: ${input}`;
      }
    });

    // Analyze tool
    this.registerTool({
      name: 'analyze',
      description: 'Analyzes code or data',
      dependencies: [],
      execute: async (input, context) => {
        if (typeof input === 'string') {
          return {
            length: input.length,
            lines: input.split('\n').length,
            words: input.split(/\s+/).length
          };
        }
        return input;
      }
    });
  }

  /**
   * Get agent by name
   */
  getAgent(name: string): Agent | undefined {
    return this.agents.get(name);
  }

  /**
   * List all registered agents
   */
  listAgents(): Agent[] {
    return Array.from(this.agents.values());
  }

  /**
   * List all registered tools
   */
  listTools(): Tool[] {
    return Array.from(this.tools.values());
  }

  /**
   * Get execution history
   */
  getExecutionHistory(): Array<{
    taskId: string;
    agent: string;
    result: any;
    timestamp: number;
  }> {
    return [...this.executionHistory];
  }

  /**
   * Share memory across agents
   */
  setSharedMemory(key: string, value: any): void {
    this.sharedMemory.set(key, value);
  }

  /**
   * Get shared memory
   */
  getSharedMemory(key: string): any {
    return this.sharedMemory.get(key);
  }

  /**
   * Clear execution history
   */
  clearHistory(): void {
    this.executionHistory = [];
    console.log('🧹 Execution history cleared');
  }

  /**
   * Get orchestrator statistics
   */
  getStats(): {
    agentCount: number;
    toolCount: number;
    taskCount: number;
    executionCount: number;
    recentExecutions: number;
  } {
    const oneHourAgo = Date.now() - 60 * 60 * 1000;
    const recentExecutions = this.executionHistory.filter(
      e => e.timestamp > oneHourAgo
    ).length;

    return {
      agentCount: this.agents.size,
      toolCount: this.tools.size,
      taskCount: this.tasks.length,
      executionCount: this.executionHistory.length,
      recentExecutions
    };
  }
}

// Global agent orchestrator instance
let globalAgentOrchestrator: AgentOrchestrator | null = null;

export function getGlobalAgentOrchestrator(): AgentOrchestrator {
  if (!globalAgentOrchestrator) {
    globalAgentOrchestrator = new AgentOrchestrator();
  }
  return globalAgentOrchestrator;
}

export async function initializeAgentOrchestrator(): Promise<void> {
  const orchestrator = getGlobalAgentOrchestrator();
  console.log('🚀 Agent orchestrator initialized');
  return Promise.resolve();
}

export async function registerAgent(agent: Agent): Promise<void> {
  const orchestrator = getGlobalAgentOrchestrator();
  return await orchestrator.registerAgent(agent);
}

export async function executeTask(task: Task): Promise<any> {
  const orchestrator = getGlobalAgentOrchestrator();
  return await orchestrator.executeTask(task);
}
