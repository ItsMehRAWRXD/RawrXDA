// BigDaddyGEngine/core/agent/AgentOrchestrator.ts
// Enhanced Agent Orchestrator with Tool Chaining and Context Memory

import { getGlobalToolChainManager } from '../../orchestration/ToolChainManager';
import type { AgentTool, ToolChain, ChainStep } from '../../orchestration/ToolChainManager';

export interface AgentContext {
  [key: string]: any;
}

export interface AgentTask {
  id: string;
  agentId: string;
  prompt: string;
  tools?: string[];
  context?: AgentContext;
  priority?: number;
}

export interface AgentWorkflow {
  id: string;
  name: string;
  description: string;
  tasks: AgentTask[];
  context?: AgentContext;
}

export interface Agent {
  id: string;
  name: string;
  type: string;
  execute: (prompt: string, context?: AgentContext) => Promise<string>;
  getCapabilities: () => string[];
}

export interface AgentMemory {
  shortTerm: Map<string, any>;
  longTerm: Map<string, any>;
  embeddings: Map<string, Float32Array>;
  lastAccess: Date;
}

export class AgentOrchestrator {
  private agents: Map<string, Agent> = new Map();
  private memory: Map<string, AgentMemory> = new Map();
  private toolChainManager = getGlobalToolChainManager();
  private workflows: Map<string, AgentWorkflow> = new Map();
  private isInitialized = false;

  constructor() {
    this.initializeDefaultAgents();
  }

  /**
   * Initialize with default agents
   */
  private initializeDefaultAgents(): void {
    // Create analysis agent
    this.registerAgent({
      id: 'analyzer',
      name: 'Code Analyzer',
      type: 'analysis',
      execute: async (prompt: string, context?: AgentContext) => {
        const result = await this.toolChainManager.executeChain('analyze-workflow', {
          prompt,
          ...context
        });
        return JSON.stringify(result);
      },
      getCapabilities: () => ['analyze', 'summarize', 'report']
    });

    // Create refactoring agent
    this.registerAgent({
      id: 'refactorer',
      name: 'Code Refactorer',
      type: 'refactor',
      execute: async (prompt: string, context?: AgentContext) => {
        const result = await this.toolChainManager.executeChain('refactor-workflow', {
          prompt,
          ...context
        });
        return JSON.stringify(result);
      },
      getCapabilities: () => ['refactor', 'optimize', 'improve']
    });

    // Create generation agent
    this.registerAgent({
      id: 'generator',
      name: 'Code Generator',
      type: 'generation',
      execute: async (prompt: string, context?: AgentContext) => {
        const result = await this.toolChainManager.executeChain('generate-workflow', {
          prompt,
          ...context
        });
        return JSON.stringify(result);
      },
      getCapabilities: () => ['generate', 'create', 'build']
    });

    // Initialize default workflows
    this.initializeDefaultWorkflows();
  }

  /**
   * Initialize default workflows
   */
  private initializeDefaultWorkflows(): void {
    // Analysis workflow
    this.createWorkflow('analyze-workflow', 'Analyze Code', 'Analyze code quality and structure', [
      {
        id: 'step1',
        agentId: 'analyzer',
        prompt: 'Analyze the code and generate a summary',
        tools: ['read_file', 'analyze_code', 'generate_summary']
      }
    ]);

    // Refactoring workflow
    this.createWorkflow('refactor-workflow', 'Refactor Code', 'Refactor code to improve quality', [
      {
        id: 'step1',
        agentId: 'refactorer',
        prompt: 'Read the code file',
        tools: ['read_file']
      },
      {
        id: 'step2',
        agentId: 'refactorer',
        prompt: 'Analyze code for refactoring opportunities',
        tools: ['analyze_code']
      },
      {
        id: 'step3',
        agentId: 'refactorer',
        prompt: 'Refactor the code',
        tools: ['refactor_code']
      },
      {
        id: 'step4',
        agentId: 'refactorer',
        prompt: 'Save the refactored code',
        tools: ['write_file']
      }
    ]);

    // Generation workflow
    this.createWorkflow('generate-workflow', 'Generate Code', 'Generate new code from prompt', [
      {
        id: 'step1',
        agentId: 'generator',
        prompt: 'Generate code based on the prompt',
        tools: ['llm_generate']
      },
      {
        id: 'step2',
        agentId: 'generator',
        prompt: 'Save the generated code',
        tools: ['write_file']
      }
    ]);
  }

  /**
   * Register an agent
   */
  registerAgent(agent: Agent): void {
    if (this.agents.has(agent.id)) {
      console.warn(`Agent ${agent.id} already registered, replacing...`);
    }
    
    this.agents.set(agent.id, agent);
    this.memory.set(agent.id, {
      shortTerm: new Map(),
      longTerm: new Map(),
      embeddings: new Map(),
      lastAccess: new Date()
    });
    
    console.log(`🤖 Registered agent: ${agent.name} (${agent.id})`);
  }

  /**
   * Unregister an agent
   */
  unregisterAgent(agentId: string): void {
    if (this.agents.has(agentId)) {
      this.agents.delete(agentId);
      this.memory.delete(agentId);
      console.log(`🗑️ Unregistered agent: ${agentId}`);
    }
  }

  /**
   * Get agent by ID
   */
  getAgent(agentId: string): Agent | undefined {
    return this.agents.get(agentId);
  }

  /**
   * List all agents
   */
  listAgents(): Agent[] {
    return Array.from(this.agents.values());
  }

  /**
   * Execute a task with an agent
   */
  async executeTask(task: AgentTask): Promise<any> {
    const agent = this.agents.get(task.agentId);
    if (!agent) {
      throw new Error(`Agent ${task.agentId} not found`);
    }

    console.log(`🎯 Executing task ${task.id} with agent ${agent.name}`);

    // Update memory
    const memory = this.memory.get(task.agentId)!;
    memory.lastAccess = new Date();
    memory.shortTerm.set(`task_${task.id}`, {
      prompt: task.prompt,
      context: task.context,
      timestamp: new Date()
    });

    try {
      // Build context
      const context = {
        ...task.context,
        memory: this.getAgentMemory(task.agentId),
        capabilities: agent.getCapabilities()
      };

      // Execute agent
      const result = await agent.execute(task.prompt, context);

      // Store result in memory
      memory.shortTerm.set(`result_${task.id}`, result);
      
      console.log(`✅ Task ${task.id} completed`);
      return result;

    } catch (error: any) {
      console.error(`❌ Task ${task.id} failed:`, error);
      throw error;
    }
  }

  /**
   * Execute a multi-agent workflow
   */
  async executeWorkflow(workflowId: string, context?: AgentContext): Promise<any> {
    const workflow = this.workflows.get(workflowId);
    if (!workflow) {
      throw new Error(`Workflow ${workflowId} not found`);
    }

    console.log(`🔄 Executing workflow: ${workflow.name}`);

    const results: any[] = [];
    const workflowContext = { ...workflow.context, ...context };

    try {
      for (const task of workflow.tasks) {
        const taskContext = {
          ...workflowContext,
          previousResults: results,
          workflowId
        };

        const result = await this.executeTask({
          ...task,
          context: taskContext
        });

        results.push(result);

        // Update workflow context with result
        workflowContext[task.id] = result;
      }

      console.log(`✅ Workflow ${workflowId} completed`);
      return results;

    } catch (error: any) {
      console.error(`❌ Workflow ${workflowId} failed:`, error);
      throw error;
    }
  }

  /**
   * Create a workflow
   */
  createWorkflow(id: string, name: string, description: string, tasks: AgentTask[]): AgentWorkflow {
    const workflow: AgentWorkflow = {
      id,
      name,
      description,
      tasks,
      context: {}
    };

    this.workflows.set(id, workflow);
    console.log(`📋 Created workflow: ${name} with ${tasks.length} tasks`);
    
    return workflow;
  }

  /**
   * Get workflow by ID
   */
  getWorkflow(workflowId: string): AgentWorkflow | undefined {
    return this.workflows.get(workflowId);
  }

  /**
   * List all workflows
   */
  listWorkflows(): AgentWorkflow[] {
    return Array.from(this.workflows.values());
  }

  /**
   * Get agent memory
   */
  getAgentMemory(agentId: string): AgentMemory | undefined {
    return this.memory.get(agentId);
  }

  /**
   * Update agent memory
   */
  updateAgentMemory(agentId: string, key: string, value: any, longTerm: boolean = false): void {
    const memory = this.memory.get(agentId);
    if (!memory) {
      return;
    }

    const store = longTerm ? memory.longTerm : memory.shortTerm;
    store.set(key, value);
    memory.lastAccess = new Date();
  }

  /**
   * Clear agent short-term memory
   */
  clearAgentMemory(agentId: string, longTerm: boolean = false): void {
    const memory = this.memory.get(agentId);
    if (!memory) {
      return;
    }

    if (longTerm) {
      memory.longTerm.clear();
    } else {
      memory.shortTerm.clear();
    }
  }

  /**
   * Coordinate multiple agents for a complex task
   */
  async coordinateAgents(
    task: string,
    agentIds: string[],
    coordinationStrategy: 'sequential' | 'parallel' | 'voting' = 'sequential'
  ): Promise<any> {
    console.log(`🤝 Coordinating ${agentIds.length} agents with ${coordinationStrategy} strategy`);

    const agents = agentIds
      .map(id => this.agents.get(id))
      .filter((a): a is Agent => a !== undefined);

    if (agents.length === 0) {
      throw new Error('No valid agents found');
    }

    switch (coordinationStrategy) {
      case 'sequential':
        return await this.coordinateSequential(task, agents);
      
      case 'parallel':
        return await this.coordinateParallel(task, agents);
      
      case 'voting':
        return await this.coordinateVoting(task, agents);
      
      default:
        throw new Error(`Unknown coordination strategy: ${coordinationStrategy}`);
    }
  }

  /**
   * Coordinate agents sequentially
   */
  private async coordinateSequential(task: string, agents: Agent[]): Promise<any> {
    let result = task;

    for (const agent of agents) {
      console.log(`  → Processing with ${agent.name}...`);
      result = await agent.execute(result);
    }

    return result;
  }

  /**
   * Coordinate agents in parallel
   */
  private async coordinateParallel(task: string, agents: Agent[]): Promise<any> {
    const promises = agents.map(agent => 
      agent.execute(task).catch(error => ({ error: error.message, agent: agent.name }))
    );

    const results = await Promise.all(promises);
    return results;
  }

  /**
   * Coordinate agents with voting
   */
  private async coordinateVoting(task: string, agents: Agent[]): Promise<any> {
    const results = await Promise.all(
      agents.map(agent => agent.execute(task))
    );

    // Simple voting: return first result (could be improved with confidence scores)
    return results[0];
  }

  /**
   * Get orchestration status
   */
  getStatus(): {
    initialized: boolean;
    agentCount: number;
    workflowCount: number;
    memoryUsage: number;
  } {
    let memoryUsage = 0;
    for (const memory of this.memory.values()) {
      memoryUsage += memory.shortTerm.size + memory.longTerm.size;
    }

    return {
      initialized: this.isInitialized,
      agentCount: this.agents.size,
      workflowCount: this.workflows.size,
      memoryUsage
    };
  }

  /**
   * Initialize orchestrator
   */
  async initialize(): Promise<void> {
    if (this.isInitialized) {
      console.log('Orchestrator already initialized');
      return;
    }

    console.log('🚀 Initializing Agent Orchestrator...');
    this.isInitialized = true;
    console.log('✅ Agent Orchestrator initialized');
  }

  /**
   * Shutdown orchestrator
   */
  async shutdown(): Promise<void> {
    console.log('🛑 Shutting down Agent Orchestrator...');
    this.agents.clear();
    this.workflows.clear();
    this.memory.clear();
    this.isInitialized = false;
    console.log('✅ Agent Orchestrator shut down');
  }
}

/**
 * Global agent orchestrator instance
 */
let globalAgentOrchestrator: AgentOrchestrator | null = null;

export function getGlobalAgentOrchestrator(): AgentOrchestrator {
  if (!globalAgentOrchestrator) {
    globalAgentOrchestrator = new AgentOrchestrator();
  }
  return globalAgentOrchestrator;
}

/**
 * Initialize agent orchestrator
 */
export async function initializeAgentOrchestrator(): Promise<AgentOrchestrator> {
  const orchestrator = getGlobalAgentOrchestrator();
  await orchestrator.initialize();
  return orchestrator;
}
