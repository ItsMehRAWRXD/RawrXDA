// BigDaddyGEngine/core/agents.ts
// Agent Orchestration Framework for Autonomous Toolchains

export interface ToolHandler {
  (input: any, context?: AgentContext): Promise<any>;
}

export interface AgentContext {
  memory: Map<string, any>;
  callTool(name: string, args: any): Promise<any>;
  getMemory(key: string): any;
  setMemory(key: string, value: any): void;
  clearMemory(): void;
  getAvailableTools(): string[];
}

export interface AgentTool {
  name: string;
  description: string;
  handler: ToolHandler;
  inputSchema?: any;
  outputSchema?: any;
  dependencies?: string[];
  permissions?: string[];
}

export interface AgentWorkflow {
  id: string;
  name: string;
  description: string;
  steps: AgentWorkflowStep[];
  context?: Record<string, any>;
}

export interface AgentWorkflowStep {
  id: string;
  tool: string;
  input: any;
  condition?: (context: AgentContext) => boolean;
  onSuccess?: (result: any, context: AgentContext) => void;
  onError?: (error: Error, context: AgentContext) => void;
}

export interface AgentExecutionResult {
  success: boolean;
  result?: any;
  error?: Error;
  executionTime: number;
  stepsExecuted: number;
  context: AgentContext;
}

export class AgentRegistry {
  private tools = new Map<string, AgentTool>();
  private workflows = new Map<string, AgentWorkflow>();
  private executionHistory: AgentExecutionResult[] = [];
  private isInitialized = false;

  constructor() {
    this.initializeDefaultTools();
  }

  /**
   * Initialize agent registry
   */
  async init(): Promise<void> {
    if (this.isInitialized) {
      console.log('Agent registry already initialized');
      return;
    }

    try {
      console.log('🤖 Initializing agent orchestration framework...');
      
      // Register core tools
      await this.registerCoreTools();
      
      // Load saved workflows
      await this.loadWorkflows();
      
      this.isInitialized = true;
      console.log('✅ Agent orchestration framework initialized');
      
    } catch (error) {
      console.error('❌ Failed to initialize agent registry:', error);
      throw error;
    }
  }

  /**
   * Register a tool
   */
  registerTool(tool: AgentTool): void {
    try {
      console.log(`🔧 Registering tool: ${tool.name}`);
      
      // Validate tool
      this.validateTool(tool);
      
      // Check for conflicts
      if (this.tools.has(tool.name)) {
        console.warn(`Tool ${tool.name} already exists, overwriting...`);
      }
      
      // Register tool
      this.tools.set(tool.name, tool);
      
      console.log(`✅ Tool registered: ${tool.name}`);
      
    } catch (error) {
      console.error(`❌ Failed to register tool ${tool.name}:`, error);
      throw error;
    }
  }

  /**
   * Execute a tool
   */
  async executeTool(name: string, input: any, context?: AgentContext): Promise<any> {
    const tool = this.tools.get(name);
    if (!tool) {
      throw new Error(`Tool not found: ${name}`);
    }

    try {
      console.log(`🔧 Executing tool: ${name}`);
      
      // Create context if not provided
      const agentContext = context || this.createContext();
      
      // Check permissions
      await this.checkPermissions(tool, agentContext);
      
      // Execute tool
      const startTime = performance.now();
      const result = await tool.handler(input, agentContext);
      const executionTime = performance.now() - startTime;
      
      // Log execution
      this.logExecution(name, input, result, executionTime, true);
      
      console.log(`✅ Tool executed: ${name} (${executionTime.toFixed(2)}ms)`);
      return result;
      
    } catch (error) {
      console.error(`❌ Tool execution failed: ${name}`, error);
      this.logExecution(name, input, null, 0, false, error as Error);
      throw error;
    }
  }

  /**
   * Execute a workflow
   */
  async executeWorkflow(workflowId: string, initialInput?: any): Promise<AgentExecutionResult> {
    const workflow = this.workflows.get(workflowId);
    if (!workflow) {
      throw new Error(`Workflow not found: ${workflowId}`);
    }

    try {
      console.log(`🔄 Executing workflow: ${workflow.name}`);
      
      const startTime = performance.now();
      const context = this.createContext();
      let currentInput = initialInput;
      let stepsExecuted = 0;
      
      // Execute each step
      for (const step of workflow.steps) {
        try {
          // Check condition
          if (step.condition && !step.condition(context)) {
            console.log(`⏭️ Skipping step ${step.id} due to condition`);
            continue;
          }
          
          // Execute step
          console.log(`🔧 Executing step: ${step.id} (${step.tool})`);
          const result = await this.executeTool(step.tool, step.input || currentInput, context);
          
          // Update input for next step
          currentInput = result;
          stepsExecuted++;
          
          // Handle success callback
          if (step.onSuccess) {
            step.onSuccess(result, context);
          }
          
        } catch (error) {
          console.error(`❌ Step execution failed: ${step.id}`, error);
          
          // Handle error callback
          if (step.onError) {
            step.onError(error as Error, context);
          }
          
          // Decide whether to continue or stop
          if (step.onError) {
            // Let the error handler decide
            continue;
          } else {
            throw error;
          }
        }
      }
      
      const executionTime = performance.now() - startTime;
      const result: AgentExecutionResult = {
        success: true,
        result: currentInput,
        executionTime,
        stepsExecuted,
        context
      };
      
      // Store execution history
      this.executionHistory.push(result);
      
      console.log(`✅ Workflow completed: ${workflow.name} (${executionTime.toFixed(2)}ms, ${stepsExecuted} steps)`);
      return result;
      
    } catch (error) {
      console.error(`❌ Workflow execution failed: ${workflow.name}`, error);
      
      const executionTime = performance.now() - startTime;
      const result: AgentExecutionResult = {
        success: false,
        error: error as Error,
        executionTime,
        stepsExecuted,
        context: this.createContext()
      };
      
      this.executionHistory.push(result);
      return result;
    }
  }

  /**
   * Create agent context
   */
  createContext(): AgentContext {
    const memory = new Map<string, any>();
    
    return {
      memory,
      callTool: async (name: string, args: any) => {
        return await this.executeTool(name, args, { memory, callTool: async () => {}, getMemory: () => {}, setMemory: () => {}, clearMemory: () => {}, getAvailableTools: () => [] });
      },
      getMemory: (key: string) => memory.get(key),
      setMemory: (key: string, value: any) => memory.set(key, value),
      clearMemory: () => memory.clear(),
      getAvailableTools: () => Array.from(this.tools.keys())
    };
  }

  /**
   * List available tools
   */
  listTools(): AgentTool[] {
    return Array.from(this.tools.values());
  }

  /**
   * Get tool by name
   */
  getTool(name: string): AgentTool | undefined {
    return this.tools.get(name);
  }

  /**
   * Register a workflow
   */
  registerWorkflow(workflow: AgentWorkflow): void {
    try {
      console.log(`🔄 Registering workflow: ${workflow.name}`);
      
      // Validate workflow
      this.validateWorkflow(workflow);
      
      // Check for conflicts
      if (this.workflows.has(workflow.id)) {
        console.warn(`Workflow ${workflow.id} already exists, overwriting...`);
      }
      
      // Register workflow
      this.workflows.set(workflow.id, workflow);
      
      console.log(`✅ Workflow registered: ${workflow.name}`);
      
    } catch (error) {
      console.error(`❌ Failed to register workflow ${workflow.id}:`, error);
      throw error;
    }
  }

  /**
   * List workflows
   */
  listWorkflows(): AgentWorkflow[] {
    return Array.from(this.workflows.values());
  }

  /**
   * Get workflow by ID
   */
  getWorkflow(id: string): AgentWorkflow | undefined {
    return this.workflows.get(id);
  }

  /**
   * Get execution history
   */
  getExecutionHistory(): AgentExecutionResult[] {
    return [...this.executionHistory];
  }

  /**
   * Clear execution history
   */
  clearExecutionHistory(): void {
    this.executionHistory = [];
    console.log('🧹 Execution history cleared');
  }

  /**
   * Initialize default tools
   */
  private initializeDefaultTools(): void {
    // This will be called during construction
  }

  /**
   * Register core tools
   */
  private async registerCoreTools(): Promise<void> {
    // Register basic LLM tools
    this.registerTool({
      name: 'generateText',
      description: 'Generate text using local LLM',
      handler: async (input: { prompt: string; model?: string }, context) => {
        const response = await fetch('/api/generate', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 
            model: input.model || 'local-llm', 
            prompt: input.prompt 
          })
        });
        const result = await response.json();
        return result.response;
      }
    });

    this.registerTool({
      name: 'chat',
      description: 'Chat with local LLM',
      handler: async (input: { messages: any[]; model?: string }, context) => {
        const response = await fetch('/api/chat', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 
            model: input.model || 'local-llm', 
            messages: input.messages 
          })
        });
        const result = await response.json();
        return result.message.content;
      }
    });

    this.registerTool({
      name: 'analyzeCode',
      description: 'Analyze code for issues and improvements',
      handler: async (input: { code: string; language?: string }, context) => {
        const prompt = `Analyze the following ${input.language || 'code'} for issues, improvements, and best practices:\n\n${input.code}`;
        return await this.executeTool('generateText', { prompt }, context);
      }
    });

    this.registerTool({
      name: 'refactorCode',
      description: 'Refactor code according to best practices',
      handler: async (input: { code: string; instructions: string; language?: string }, context) => {
        const prompt = `Refactor the following ${input.language || 'code'} according to these instructions: ${input.instructions}\n\nCode:\n${input.code}\n\nRefactored code:`;
        return await this.executeTool('generateText', { prompt }, context);
      }
    });

    this.registerTool({
      name: 'summarizeText',
      description: 'Summarize text content',
      handler: async (input: { text: string; maxLength?: number }, context) => {
        const prompt = `Summarize the following text in ${input.maxLength || 200} characters or less:\n\n${input.text}`;
        return await this.executeTool('generateText', { prompt }, context);
      }
    });

    console.log('✅ Core tools registered');
  }

  /**
   * Load workflows from storage
   */
  private async loadWorkflows(): Promise<void> {
    try {
      const stored = localStorage.getItem('agent-workflows');
      if (stored) {
        const workflows = JSON.parse(stored);
        for (const workflow of workflows) {
          this.workflows.set(workflow.id, workflow);
        }
        console.log(`📦 Loaded ${workflows.length} workflows from storage`);
      }
    } catch (error) {
      console.warn('Failed to load workflows from storage:', error);
    }
  }

  /**
   * Save workflows to storage
   */
  private async saveWorkflows(): Promise<void> {
    try {
      const workflows = Array.from(this.workflows.values());
      localStorage.setItem('agent-workflows', JSON.stringify(workflows));
      console.log('💾 Workflows saved to storage');
    } catch (error) {
      console.error('Failed to save workflows to storage:', error);
    }
  }

  /**
   * Validate tool
   */
  private validateTool(tool: AgentTool): void {
    if (!tool.name) throw new Error('Tool name is required');
    if (!tool.handler || typeof tool.handler !== 'function') {
      throw new Error('Tool handler must be a function');
    }
  }

  /**
   * Validate workflow
   */
  private validateWorkflow(workflow: AgentWorkflow): void {
    if (!workflow.id) throw new Error('Workflow ID is required');
    if (!workflow.name) throw new Error('Workflow name is required');
    if (!workflow.steps || !Array.isArray(workflow.steps)) {
      throw new Error('Workflow steps must be an array');
    }
    
    for (const step of workflow.steps) {
      if (!step.tool) throw new Error('Workflow step must specify a tool');
      if (!this.tools.has(step.tool)) {
        throw new Error(`Workflow step references unknown tool: ${step.tool}`);
      }
    }
  }

  /**
   * Check permissions
   */
  private async checkPermissions(tool: AgentTool, context: AgentContext): Promise<void> {
    if (tool.permissions && tool.permissions.length > 0) {
      // In a real implementation, check if the context has the required permissions
      console.log(`🔐 Checking permissions for tool: ${tool.name}`, tool.permissions);
    }
  }

  /**
   * Log execution
   */
  private logExecution(toolName: string, input: any, result: any, executionTime: number, success: boolean, error?: Error): void {
    const logEntry = {
      timestamp: new Date().toISOString(),
      tool: toolName,
      input,
      result,
      executionTime,
      success,
      error: error?.message
    };
    
    // Store in execution history
    this.executionHistory.push({
      success,
      result,
      error,
      executionTime,
      stepsExecuted: 1,
      context: this.createContext()
    });
    
    console.log(`📊 Tool execution logged: ${toolName} (${executionTime.toFixed(2)}ms)`);
  }
}

/**
 * Global agent registry instance
 */
let globalAgentRegistry: AgentRegistry | null = null;

export function getGlobalAgentRegistry(): AgentRegistry {
  if (!globalAgentRegistry) {
    globalAgentRegistry = new AgentRegistry();
  }
  return globalAgentRegistry;
}

/**
 * Initialize agent system
 */
export async function initializeAgentSystem(): Promise<void> {
  const registry = getGlobalAgentRegistry();
  await registry.init();
}

/**
 * Register a tool
 */
export function registerTool(tool: AgentTool): void {
  const registry = getGlobalAgentRegistry();
  registry.registerTool(tool);
}

/**
 * Execute a tool
 */
export async function executeTool(name: string, input: any, context?: AgentContext): Promise<any> {
  const registry = getGlobalAgentRegistry();
  return await registry.executeTool(name, input, context);
}

/**
 * Execute a workflow
 */
export async function executeWorkflow(workflowId: string, initialInput?: any): Promise<AgentExecutionResult> {
  const registry = getGlobalAgentRegistry();
  return await registry.executeWorkflow(workflowId, initialInput);
}

/**
 * Register a workflow
 */
export function registerWorkflow(workflow: AgentWorkflow): void {
  const registry = getGlobalAgentRegistry();
  registry.registerWorkflow(workflow);
}

/**
 * List available tools
 */
export function listTools(): AgentTool[] {
  const registry = getGlobalAgentRegistry();
  return registry.listTools();
}

/**
 * List workflows
 */
export function listWorkflows(): AgentWorkflow[] {
  const registry = getGlobalAgentRegistry();
  return registry.listWorkflows();
}
