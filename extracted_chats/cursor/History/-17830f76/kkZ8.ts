// BigDaddyGEngine/examples/agent-orchestration-example.ts
// Example demonstrating Agent Orchestration with Tool Chaining

import { getGlobalAgentOrchestrator, initializeAgentOrchestrator } from '../core/agent/AgentOrchestrator';
import { getGlobalToolChainManager, initializeToolChainManager } from '../orchestration/ToolChainManager';

/**
 * Example: Simple agent execution
 */
export async function exampleSimpleAgentExecution() {
  console.log('=== Example: Simple Agent Execution ===\n');

  const orchestrator = await initializeAgentOrchestrator();

  // Execute a simple task
  const result = await orchestrator.executeTask({
    id: 'task1',
    agentId: 'analyzer',
    prompt: 'Analyze this code snippet and provide insights',
    context: {
      code: 'function example() { return 42; }'
    }
  });

  console.log('Result:', result);
  console.log('');
}

/**
 * Example: Tool chain execution
 */
export async function exampleToolChainExecution() {
  console.log('=== Example: Tool Chain Execution ===\n');

  const toolChainManager = await initializeToolChainManager();

  // Create a simple chain
  const chain = toolChainManager.createChain(
    'file-analysis-chain',
    'File Analysis Chain',
    'Read, analyze, and summarize a file',
    [
      {
        stepId: 'step1',
        toolId: 'read_file',
        inputMapping: { path: 'example.ts' }
      },
      {
        stepId: 'step2',
        toolId: 'analyze_code',
        inputMapping: { code: '${step1.result.content}' }
      },
      {
        stepId: 'step3',
        toolId: 'generate_summary',
        inputMapping: { text: '${step2.result.analysis}' }
      }
    ]
  );

  // Execute the chain
  const results = await toolChainManager.executeChain(chain.id, {
    initialFile: 'example.ts'
  });

  console.log('Chain Results:', results);
  console.log('');
}

/**
 * Example: Multi-agent workflow
 */
export async function exampleMultiAgentWorkflow() {
  console.log('=== Example: Multi-Agent Workflow ===\n');

  const orchestrator = await initializeAgentOrchestrator();

  // Execute a refactoring workflow
  const results = await orchestrator.executeWorkflow('refactor-workflow', {
    filePath: 'src/example.ts',
    goal: 'Improve code quality'
  });

  console.log('Workflow Results:', results);
  console.log('');
}

/**
 * Example: Sequential agent coordination
 */
export async function exampleSequentialCoordination() {
  console.log('=== Example: Sequential Agent Coordination ===\n');

  const orchestrator = await initializeAgentOrchestrator();

  // Coordinate multiple agents sequentially
  const result = await orchestrator.coordinateAgents(
    'Analyze, refactor, and generate documentation for this code',
    ['analyzer', 'refactorer', 'generator'],
    'sequential'
  );

  console.log('Sequential Result:', result);
  console.log('');
}

/**
 * Example: Parallel agent coordination
 */
export async function exampleParallelCoordination() {
  console.log('=== Example: Parallel Agent Coordination ===\n');

  const orchestrator = await initializeAgentOrchestrator();

  // Coordinate multiple agents in parallel
  const results = await orchestrator.coordinateAgents(
    'Analyze this code from multiple perspectives',
    ['analyzer', 'refactorer'],
    'parallel'
  );

  console.log('Parallel Results:', results);
  console.log('');
}

/**
 * Example: Custom tool registration
 */
export async function exampleCustomToolRegistration() {
  console.log('=== Example: Custom Tool Registration ===\n');

  const toolChainManager = await initializeToolChainManager();

  // Register a custom tool
  toolChainManager.registerTool({
    id: 'custom_test_tool',
    name: 'Custom Test Tool',
    description: 'A custom tool for testing',
    inputSchema: { type: 'object', properties: { input: { type: 'string' } } },
    outputSchema: { type: 'object', properties: { output: { type: 'string' } } },
    handler: async (args: any) => {
      return { output: `Processed: ${args.input}` };
    },
    timeout: 1000
  });

  // Create and execute a chain with the custom tool
  const chain = toolChainManager.createChain(
    'custom-test-chain',
    'Custom Test Chain',
    'Test chain with custom tool',
    [
      {
        stepId: 'step1',
        toolId: 'custom_test_tool',
        inputMapping: { input: 'Hello, World!' }
      }
    ]
  );

  const results = await toolChainManager.executeChain(chain.id);

  console.log('Custom Tool Results:', results);
  console.log('');
}

/**
 * Example: Execution statistics
 */
export async function exampleExecutionStatistics() {
  console.log('=== Example: Execution Statistics ===\n');

  const toolChainManager = await initializeToolChainManager();
  const orchestrator = await initializeAgentOrchestrator();

  // Get statistics
  const toolStats = toolChainManager.getExecutionStats();
  const orchestratorStatus = orchestrator.getStatus();

  console.log('Tool Chain Stats:', toolStats);
  console.log('Orchestrator Status:', orchestratorStatus);
  console.log('');
}

/**
 * Run all examples
 */
export async function runAllExamples() {
  console.log('🚀 Starting Agent Orchestration Examples\n');
  console.log('='.repeat(60) + '\n');

  try {
    // Initialize systems
    await initializeToolChainManager();
    await initializeAgentOrchestrator();

    // Run examples
    await exampleSimpleAgentExecution();
    await exampleToolChainExecution();
    await exampleMultiAgentWorkflow();
    await exampleSequentialCoordination();
    await exampleParallelCoordination();
    await exampleCustomToolRegistration();
    await exampleExecutionStatistics();

    console.log('='.repeat(60));
    console.log('✅ All examples completed successfully!');
    
  } catch (error) {
    console.error('❌ Error running examples:', error);
  }
}

// Run examples if this file is executed directly
if (typeof window === 'undefined') {
  runAllExamples();
}

export default {
  exampleSimpleAgentExecution,
  exampleToolChainExecution,
  exampleMultiAgentWorkflow,
  exampleSequentialCoordination,
  exampleParallelCoordination,
  exampleCustomToolRegistration,
  exampleExecutionStatistics,
  runAllExamples
};
