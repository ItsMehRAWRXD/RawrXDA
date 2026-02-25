# Agent Orchestration System

## Overview

The Agent Orchestration System provides a powerful framework for coordinating multiple AI agents, managing complex workflows, and executing tool chains. It enables seamless multi-agent collaboration with context memory, tool registration, and workflow management.

## Features

- **Multi-Agent Coordination**: Coordinate multiple agents sequentially, in parallel, or with voting
- **Tool Chaining**: Chain multiple tools together in sophisticated workflows
- **Context Memory**: Short-term and long-term memory for agents
- **Workflow Management**: Define and execute complex multi-step workflows
- **Tool Registration**: Register custom tools and agents dynamically
- **Execution Planning**: Automatic dependency resolution and execution planning
- **Cost Tracking**: Monitor resource usage and execution costs
- **Statistics**: Track execution metrics and performance

## Architecture

```
AgentOrchestrator
├── Agent Management
│   ├── Register/Unregister Agents
│   ├── Execute Tasks
│   └── Coordinate Multiple Agents
├── Workflow Management
│   ├── Define Workflows
│   ├── Execute Workflows
│   └── Manage Workflow State
└── Memory Management
    ├── Short-Term Memory
    ├── Long-Term Memory
    └── Embeddings

ToolChainManager
├── Tool Registration
│   ├── Register/Unregister Tools
│   ├── Define Tool Interfaces
│   └── Validate Tool Schemas
├── Chain Management
│   ├── Create Chains
│   ├── Plan Execution
│   └── Execute Chains
└── Execution Tracking
    ├── Execution History
    ├── Statistics
    └── Cost Tracking
```

## Quick Start

### Basic Usage

```typescript
import { 
  initializeAgentOrchestrator, 
  getGlobalAgentOrchestrator 
} from './core/agent/AgentOrchestrator';

// Initialize the orchestrator
const orchestrator = await initializeAgentOrchestrator();

// Execute a simple task
const result = await orchestrator.executeTask({
  id: 'task1',
  agentId: 'analyzer',
  prompt: 'Analyze this code',
  context: {
    code: 'function example() { return 42; }'
  }
});

console.log('Result:', result);
```

### Tool Chain Execution

```typescript
import { 
  initializeToolChainManager,
  getGlobalToolChainManager 
} from './orchestration/ToolChainManager';

const toolChainManager = await initializeToolChainManager();

// Create a tool chain
const chain = toolChainManager.createChain(
  'file-analysis',
  'File Analysis',
  'Analyze and summarize a file',
  [
    {
      stepId: 'read',
      toolId: 'read_file',
      inputMapping: { path: 'example.ts' }
    },
    {
      stepId: 'analyze',
      toolId: 'analyze_code',
      inputMapping: { code: '${read.result.content}' }
    },
    {
      stepId: 'summarize',
      toolId: 'generate_summary',
      inputMapping: { text: '${analyze.result}' }
    }
  ]
);

// Execute the chain
const results = await toolChainManager.executeChain(chain.id);
console.log('Results:', results);
```

### Multi-Agent Coordination

```typescript
// Sequential coordination
const result = await orchestrator.coordinateAgents(
  'Analyze, refactor, and document this code',
  ['analyzer', 'refactorer', 'generator'],
  'sequential'
);

// Parallel coordination
const results = await orchestrator.coordinateAgents(
  'Analyze this from multiple perspectives',
  ['analyzer', 'refactorer'],
  'parallel'
);

// Voting coordination
const result = await orchestrator.coordinateAgents(
  'Generate the best solution',
  ['generator1', 'generator2', 'generator3'],
  'voting'
);
```

## API Reference

### AgentOrchestrator

#### Methods

- `initialize(): Promise<void>` - Initialize the orchestrator
- `registerAgent(agent: Agent): void` - Register an agent
- `unregisterAgent(agentId: string): void` - Unregister an agent
- `executeTask(task: AgentTask): Promise<any>` - Execute a task with an agent
- `executeWorkflow(workflowId: string, context?: AgentContext): Promise<any>` - Execute a workflow
- `createWorkflow(id: string, name: string, description: string, tasks: AgentTask[]): AgentWorkflow` - Create a workflow
- `coordinateAgents(task: string, agentIds: string[], strategy: 'sequential' | 'parallel' | 'voting'): Promise<any>` - Coordinate multiple agents
- `getAgentMemory(agentId: string): AgentMemory | undefined` - Get agent memory
- `updateAgentMemory(agentId: string, key: string, value: any, longTerm?: boolean): void` - Update agent memory
- `getStatus(): Status` - Get orchestrator status

### ToolChainManager

#### Methods

- `registerTool(tool: AgentTool): void` - Register a tool
- `unregisterTool(toolId: string): void` - Unregister a tool
- `createChain(id: string, name: string, description: string, steps: ChainStep[]): ToolChain` - Create a tool chain
- `planExecution(chainId: string): Promise<ExecutionPlan>` - Plan execution for a chain
- `executeChain(chainId: string, initialContext?: ToolChainContext): Promise<ToolExecutionResult[]>` - Execute a chain
- `getExecutionStats(): Stats` - Get execution statistics
- `getExecutionHistory(limit?: number): ToolExecutionResult[]` - Get execution history

## Built-in Tools

### File Operations
- `read_file` - Read content from a file
- `write_file` - Write content to a file

### Analysis Tools
- `analyze_code` - Perform static analysis on code
- `generate_summary` - Generate a summary from text

### LLM Integration
- `llm_generate` - Generate text using LLM

### Transformation Tools
- `refactor_code` - Refactor code to improve quality

## Built-in Agents

### Analyzer
- **Type**: Analysis
- **Capabilities**: Analyze, Summarize, Report
- **Use Case**: Code quality analysis, performance assessment

### Refactorer
- **Type**: Refactor
- **Capabilities**: Refactor, Optimize, Improve
- **Use Case**: Code refactoring, quality improvement

### Generator
- **Type**: Generation
- **Capabilities**: Generate, Create, Build
- **Use Case**: Code generation, content creation

## Workflows

### Analyze Workflow
Read a file, analyze the code, and generate a summary.

### Refactor Workflow
Read, analyze, refactor, and save code improvements.

### Generate Workflow
Generate new code based on a prompt and save it.

## Custom Tools

You can register custom tools:

```typescript
toolChainManager.registerTool({
  id: 'my_custom_tool',
  name: 'My Custom Tool',
  description: 'Does something custom',
  inputSchema: { type: 'object', properties: { input: { type: 'string' } } },
  outputSchema: { type: 'object', properties: { output: { type: 'string' } } },
  handler: async (args: any) => {
    // Your custom logic here
    return { output: 'Custom result' };
  },
  timeout: 5000,
  cost: 0.1
});
```

## Custom Agents

You can register custom agents:

```typescript
orchestrator.registerAgent({
  id: 'my_agent',
  name: 'My Custom Agent',
  type: 'custom',
  execute: async (prompt: string, context?: AgentContext) => {
    // Your custom logic here
    return 'Agent response';
  },
  getCapabilities: () => ['capability1', 'capability2']
});
```

## Context Variables

Use context variables in input mappings:

```typescript
{
  stepId: 'step2',
  toolId: 'analyze_code',
  inputMapping: { 
    code: '${step1.result.content}'  // Reference previous step result
  }
}
```

## Conditional Execution

Add conditions to chain steps:

```typescript
{
  stepId: 'conditional_step',
  toolId: 'some_tool',
  inputMapping: { /* ... */ },
  condition: (context: any) => {
    return context.someValue > 10;  // Only execute if condition is true
  }
}
```

## Error Handling

Failed steps can be configured to retry:

```typescript
{
  stepId: 'retry_step',
  toolId: 'some_tool',
  inputMapping: { /* ... */ },
  retryOnFailure: true  // Retry on failure
}
```

## Performance Monitoring

Track execution metrics:

```typescript
const stats = toolChainManager.getExecutionStats();
console.log('Success Rate:', stats.successRate);
console.log('Average Duration:', stats.averageDuration);
console.log('Total Cost:', stats.totalCost);
```

## Examples

See `examples/agent-orchestration-example.ts` for comprehensive examples covering:
- Simple agent execution
- Tool chain execution
- Multi-agent workflows
- Sequential coordination
- Parallel coordination
- Custom tool registration
- Execution statistics

## Future Enhancements

- Distributed agent execution
- Agent learning and adaptation
- Advanced workflow patterns
- Real-time monitoring dashboards
- Performance optimization strategies
