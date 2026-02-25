// AutoOrchestratorAgent.ts - Autonomous orchestration agent for end-to-end execution
// Production-grade scaffold for planning → execution → result emission without interaction loops

import { BigDaddyGConfigManager } from '../BigDaddyGEngine.config';

export interface AutoOrchestratorOptions {
  prompt: string;
  agents?: string[];
  contextWindow?: number;
  maxSteps?: number;
  maxRetries?: number;
  enableStreaming?: boolean;
  priority?: 'low' | 'normal' | 'high';
  timeout?: number;
  fallbackAgents?: string[];
  confidenceThreshold?: number;
}

export interface AutoOrchestratorResult {
  success: boolean;
  output: string;
  steps: OrchestrationStep[];
  tokenCount: number;
  executionTimeMs: number;
  errors?: string[];
  metadata?: {
    sessionId: string;
    startTime: number;
    endTime: number;
    agentsUsed: string[];
    averageConfidence: number;
    memoryPeak: number;
    contextUtilization: number;
  };
}

export interface OrchestrationStep {
  step: number;
  agent: string;
  input: string;
  output: string;
  confidence?: number;
  durationMs?: number;
  tokens?: number;
  logs?: string[];
  metadata?: {
    modelUsed?: string;
    contextWindow?: number;
    memoryUsage?: number;
    cpuUsage?: number;
    errorCount?: number;
  };
}

export interface Agent {
  id: string;
  name: string;
  description: string;
  capabilities: string[];
  estimatedTokens: number;
  confidence: number;
  run: (params: AgentRunParams) => Promise<AgentRunResult>;
  fallback?: Agent;
  isActive: boolean;
}

export interface AgentRunParams {
  input: string;
  contextWindow: number;
  sessionId: string;
  step: number;
  previousOutput?: string;
  metadata?: Record<string, any>;
}

export interface AgentRunResult {
  output: string;
  tokens: string[];
  confidence: number;
  durationMs: number;
  logs: string[];
  metadata?: {
    modelUsed?: string;
    memoryUsage?: number;
    cpuUsage?: number;
    errorCount?: number;
    fallbackUsed?: boolean;
  };
}

export interface OrchestrationPlan {
  steps: PlannedStep[];
  estimatedDuration: number;
  estimatedTokens: number;
  confidence: number;
  fallbackStrategy?: string;
}

export interface PlannedStep {
  step: number;
  agentId: string;
  input: string;
  expectedOutput: string;
  confidence: number;
  estimatedDuration: number;
  estimatedTokens: number;
}

// Global agent registry
export const agentRegistry: Map<string, Agent> = new Map();

// Session tracking
const activeSessions: Map<string, AutoOrchestratorResult> = new Map();

/** Registers a runtime agent dynamically */
export function registerAgent(agent: Agent): void {
  agentRegistry.set(agent.id, agent);
  console.log(`🤖 Agent registered: ${agent.name} (${agent.id})`);
}

/** Unregisters an agent */
export function unregisterAgent(agentId: string): boolean {
  return agentRegistry.delete(agentId);
}

/** Gets all registered agents */
export function getRegisteredAgents(): Agent[] {
  return Array.from(agentRegistry.values());
}

/** Gets an agent by ID or throws if missing */
function getAgentById(id: string): Agent {
  const agent = agentRegistry.get(id);
  if (!agent) {
    throw new Error(`Agent not found: ${id}. Available agents: ${Array.from(agentRegistry.keys()).join(', ')}`);
  }
  return agent;
}

/** Plans the orchestration sequence */
export function planOrchestration(options: AutoOrchestratorOptions): OrchestrationPlan {
  const agents = options.agents || getDefaultAgents();
  const steps: PlannedStep[] = [];
  let estimatedDuration = 0;
  let estimatedTokens = 0;
  let totalConfidence = 0;

  for (let i = 0; i < (options.maxSteps || 5); i++) {
    const agentId = agents[i % agents.length];
    const agent = agentRegistry.get(agentId);
    
    if (!agent) {
      console.warn(`Agent ${agentId} not found, skipping step ${i}`);
      continue;
    }

    const step: PlannedStep = {
      step: i + 1,
      agentId,
      input: i === 0 ? options.prompt : `[Output from step ${i}]`,
      expectedOutput: `[Expected output from ${agent.name}]`,
      confidence: agent.confidence,
      estimatedDuration: agent.estimatedTokens * 10, // Rough estimate
      estimatedTokens: agent.estimatedTokens
    };

    steps.push(step);
    estimatedDuration += step.estimatedDuration;
    estimatedTokens += step.estimatedTokens;
    totalConfidence += step.confidence;
  }

  return {
    steps,
    estimatedDuration,
    estimatedTokens,
    confidence: totalConfidence / steps.length,
    fallbackStrategy: options.fallbackAgents ? `Use fallback agents: ${options.fallbackAgents.join(', ')}` : 'Retry with same agents'
  };
}

/** Core autonomous execution loop */
export async function runAutoOrchestrator(
  options: AutoOrchestratorOptions
): Promise<AutoOrchestratorResult> {
  const startTime = performance.now();
  const sessionId = crypto.randomUUID();
  const config = new BigDaddyGConfigManager().getConfig();
  
  const agents = options.agents || getDefaultAgents();
  const steps: OrchestrationStep[] = [];
  let currentInput = options.prompt;
  let tokenCount = 0;
  const errors: string[] = [];
  const agentsUsed: string[] = [];
  let totalConfidence = 0;
  let memoryPeak = 0;

  console.log(`🚀 Starting autonomous orchestration: "${options.prompt}"`);
  console.log(`📋 Using agents: ${agents.join(', ')}`);
  console.log(`🪟 Context window: ${options.contextWindow || config.performance.maxContextWindow} tokens`);

  // Plan the orchestration
  const plan = planOrchestration(options);
  console.log(`📊 Orchestration plan: ${plan.steps.length} steps, ~${plan.estimatedDuration}ms, ~${plan.estimatedTokens} tokens`);

  try {
    for (let step = 0; step < (options.maxSteps || 5); step++) {
      const agentId = agents[step % agents.length];
      const agent = getAgentById(agentId);
      
      console.log(`🔄 Step ${step + 1}: Running ${agent.name} (${agentId})`);
      
      const stepStartTime = performance.now();
      
      try {
        const result = await agent.run({
          input: currentInput,
          contextWindow: options.contextWindow || config.performance.maxContextWindow,
          sessionId,
          step: step + 1,
          previousOutput: step > 0 ? steps[step - 1].output : undefined,
          metadata: {
            prompt: options.prompt,
            maxSteps: options.maxSteps,
            priority: options.priority || 'normal'
          }
        });

        const stepDuration = performance.now() - stepStartTime;
        const stepTokens = result.tokens.length;
        tokenCount += stepTokens;
        totalConfidence += result.confidence;
        agentsUsed.push(agentId);

        // Track memory usage
        const memoryInfo = (performance as any).memory;
        if (memoryInfo) {
          const currentMemory = memoryInfo.usedJSHeapSize / 1024 / 1024; // MB
          memoryPeak = Math.max(memoryPeak, currentMemory);
        }

        const orchestrationStep: OrchestrationStep = {
          step: step + 1,
          agent: agentId,
          input: currentInput,
          output: result.output,
          confidence: result.confidence,
          durationMs: result.durationMs || stepDuration,
          tokens: stepTokens,
          logs: result.logs,
          metadata: {
            modelUsed: result.metadata?.modelUsed,
            contextWindow: options.contextWindow || config.performance.maxContextWindow,
            memoryUsage: result.metadata?.memoryUsage,
            cpuUsage: result.metadata?.cpuUsage,
            errorCount: result.metadata?.errorCount
          }
        };

        steps.push(orchestrationStep);
        currentInput = result.output;

        console.log(`✅ Step ${step + 1} completed: ${stepTokens} tokens, ${result.confidence.toFixed(2)} confidence, ${stepDuration.toFixed(2)}ms`);

        // Check confidence threshold
        if (options.confidenceThreshold && result.confidence < options.confidenceThreshold) {
          console.warn(`⚠️ Low confidence (${result.confidence.toFixed(2)}) for step ${step + 1}`);
          
          // Try fallback agent if available
          if (agent.fallback) {
            console.log(`🔄 Trying fallback agent: ${agent.fallback.name}`);
            try {
              const fallbackResult = await agent.fallback.run({
                input: currentInput,
                contextWindow: options.contextWindow || config.performance.maxContextWindow,
                sessionId,
                step: step + 1,
                previousOutput: step > 0 ? steps[step - 1].output : undefined,
                metadata: { isFallback: true }
              });
              
              orchestrationStep.output = fallbackResult.output;
              orchestrationStep.confidence = fallbackResult.confidence;
              orchestrationStep.logs?.push(`Fallback used: ${agent.fallback.name}`);
              currentInput = fallbackResult.output;
              
              console.log(`✅ Fallback successful: ${fallbackResult.confidence.toFixed(2)} confidence`);
            } catch (fallbackError: any) {
              console.error(`❌ Fallback failed: ${fallbackError.message}`);
              errors.push(`Fallback failed for step ${step + 1}: ${fallbackError.message}`);
            }
          }
        }

        // Check for early termination conditions
        if (result.output.toLowerCase().includes('error') || result.output.toLowerCase().includes('failed')) {
          console.warn(`⚠️ Potential error detected in step ${step + 1} output`);
        }

      } catch (error: any) {
        const errorMessage = `Step ${step + 1} (${agentId}): ${error.message}`;
        console.error(`❌ ${errorMessage}`);
        errors.push(errorMessage);
        
        // Retry logic
        if (errors.length <= (options.maxRetries || 2)) {
          console.log(`🔄 Retrying step ${step + 1} (attempt ${errors.length})`);
          step--; // Retry this step
          continue;
        } else {
          console.error(`❌ Max retries exceeded for step ${step + 1}`);
          break;
        }
      }
    }

    const executionTime = performance.now() - startTime;
    const averageConfidence = totalConfidence / steps.length;
    const contextUtilization = tokenCount / (options.contextWindow || config.performance.maxContextWindow);

    const result: AutoOrchestratorResult = {
      success: errors.length === 0,
      output: currentInput,
      steps,
      tokenCount,
      executionTimeMs: executionTime,
      errors: errors.length > 0 ? errors : undefined,
      metadata: {
        sessionId,
        startTime,
        endTime: performance.now(),
        agentsUsed: [...new Set(agentsUsed)],
        averageConfidence,
        memoryPeak,
        contextUtilization
      }
    };

    // Store session result
    activeSessions.set(sessionId, result);

    console.log(`🎉 Orchestration completed: ${result.success ? 'SUCCESS' : 'FAILED'}`);
    console.log(`📊 Final stats: ${tokenCount} tokens, ${averageConfidence.toFixed(2)} avg confidence, ${executionTime.toFixed(2)}ms`);
    console.log(`🤖 Agents used: ${result.metadata?.agentsUsed.join(', ')}`);

    return result;

  } catch (error: any) {
    const executionTime = performance.now() - startTime;
    console.error(`💥 Orchestration failed: ${error.message}`);
    
    return {
      success: false,
      output: '',
      steps,
      tokenCount,
      executionTimeMs: executionTime,
      errors: [error.message],
      metadata: {
        sessionId,
        startTime,
        endTime: performance.now(),
        agentsUsed,
        averageConfidence: totalConfidence / Math.max(steps.length, 1),
        memoryPeak,
        contextUtilization: 0
      }
    };
  }
}

/** Gets default agents from config */
function getDefaultAgents(): string[] {
  const config = new BigDaddyGConfigManager().getConfig();
  return config.agents.enabled;
}

/** Gets session result by ID */
export function getSessionResult(sessionId: string): AutoOrchestratorResult | undefined {
  return activeSessions.get(sessionId);
}

/** Clears completed sessions */
export function clearCompletedSessions(): void {
  const now = Date.now();
  for (const [sessionId, result] of activeSessions.entries()) {
    if (now - result.metadata!.endTime > 300000) { // 5 minutes
      activeSessions.delete(sessionId);
    }
  }
}

/** Gets orchestration statistics */
export function getOrchestrationStats(): {
  totalSessions: number;
  successfulSessions: number;
  averageExecutionTime: number;
  averageTokenCount: number;
  averageConfidence: number;
} {
  const sessions = Array.from(activeSessions.values());
  const successfulSessions = sessions.filter(s => s.success).length;
  const totalExecutionTime = sessions.reduce((sum, s) => sum + s.executionTimeMs, 0);
  const totalTokens = sessions.reduce((sum, s) => sum + s.tokenCount, 0);
  const totalConfidence = sessions.reduce((sum, s) => sum + (s.metadata?.averageConfidence || 0), 0);

  return {
    totalSessions: sessions.length,
    successfulSessions,
    averageExecutionTime: sessions.length > 0 ? totalExecutionTime / sessions.length : 0,
    averageTokenCount: sessions.length > 0 ? totalTokens / sessions.length : 0,
    averageConfidence: sessions.length > 0 ? totalConfidence / sessions.length : 0
  };
}

// Example agent implementations
export const exampleAgents = {
  rawr: {
    id: 'rawr',
    name: 'Rawr Agent',
    description: 'Basic text transformation agent',
    capabilities: ['text-transform', 'uppercase', 'lowercase'],
    estimatedTokens: 50,
    confidence: 0.95,
    isActive: true,
    async run({ input, contextWindow, sessionId, step }: AgentRunParams): Promise<AgentRunResult> {
      const output = input.toUpperCase();
      const tokens = output.split(' ');
      
      return {
        output,
        tokens,
        confidence: 0.95,
        durationMs: 30,
        logs: [`Rawr transformation completed for step ${step}`],
        metadata: {
          modelUsed: 'rawr-v1',
          memoryUsage: 10,
          cpuUsage: 5,
          errorCount: 0
        }
      };
    }
  } as Agent,

  analyzer: {
    id: 'analyzer',
    name: 'Code Analyzer',
    description: 'Analyzes code structure and patterns',
    capabilities: ['code-analysis', 'pattern-detection', 'structure-analysis'],
    estimatedTokens: 200,
    confidence: 0.88,
    isActive: true,
    async run({ input, contextWindow, sessionId, step }: AgentRunParams): Promise<AgentRunResult> {
      // Simulate code analysis
      const analysis = `Analysis of: ${input.substring(0, 100)}...\n- Lines: ${input.split('\n').length}\n- Complexity: Medium\n- Patterns: Found 3 design patterns`;
      const tokens = analysis.split(' ');
      
      return {
        output: analysis,
        tokens,
        confidence: 0.88,
        durationMs: 150,
        logs: [`Code analysis completed for step ${step}`, `Analyzed ${input.length} characters`],
        metadata: {
          modelUsed: 'analyzer-v2',
          memoryUsage: 25,
          cpuUsage: 15,
          errorCount: 0
        }
      };
    }
  } as Agent,

  optimizer: {
    id: 'optimizer',
    name: 'Performance Optimizer',
    description: 'Optimizes code for performance',
    capabilities: ['optimization', 'performance', 'efficiency'],
    estimatedTokens: 300,
    confidence: 0.92,
    isActive: true,
    async run({ input, contextWindow, sessionId, step }: AgentRunParams): Promise<AgentRunResult> {
      const optimizations = `Optimizations applied:\n1. Reduced complexity from O(n²) to O(n)\n2. Added caching for repeated operations\n3. Optimized memory usage\n4. Improved algorithm efficiency`;
      const tokens = optimizations.split(' ');
      
      return {
        output: optimizations,
        tokens,
        confidence: 0.92,
        durationMs: 200,
        logs: [`Optimization completed for step ${step}`, `Applied 4 optimizations`],
        metadata: {
          modelUsed: 'optimizer-v1',
          memoryUsage: 35,
          cpuUsage: 20,
          errorCount: 0
        }
      };
    }
  } as Agent
};

// Initialize with example agents
export function initializeExampleAgents(): void {
  Object.values(exampleAgents).forEach(agent => {
    registerAgent(agent);
  });
  console.log('🤖 Example agents initialized');
}
