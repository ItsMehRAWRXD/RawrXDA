// ============================================================================
// agent_puppeteer_bridge.ts — TypeScript Bridge for Agent Puppeteer
// ============================================================================
// Connects the C++ AgentPuppeteer backend to the TypeScript UI via ACP/JSON-RPC.
//
// Provides:
//   - Agent selection and configuration
//   - Task execution with progress tracking
//   - Result aggregation display
//   - VRAM status monitoring
//
// Integrates with existing:
//   - ACPClient (JSON-RPC bridge)
//   - Win32IDE streaming output
//   - ChatPane message display
// ============================================================================

import { ACPClient } from './acp_client';

// ============================================================================
// Types
// ============================================================================

export enum AgentRole {
    Planner = 'planner',
    Coder = 'coder',
    Reviewer = 'reviewer',
    Analyst = 'analyst',
    Tester = 'tester',
    Architect = 'architect',
    Security = 'security',
    Optimizer = 'optimizer',
    Documenter = 'documenter',
    Custom = 'custom'
}

export enum AggregationMode {
    Union = 'union',
    Consensus = 'consensus',
    Synthesis = 'synthesis',
    BestMatch = 'best_match',
    Refinement = 'refinement'
}

export enum ExecutionMode {
    Sequential = 'sequential',
    Parallel = 'parallel',
    Pipeline = 'pipeline'
}

export interface AgentConfig {
    id: string;
    name: string;
    role: AgentRole;
    systemPrompt: string;
    dependencies: AgentRole[];
    priority: number;
    enabled: boolean;
    maxRetries: number;
    timeoutSeconds: number;
    shareContext: boolean;
    accumulateContext: boolean;
}

export interface AgentResult {
    agentId: string;
    agentRole: string;
    output: string;
    artifacts: string[];
    contextUpdates: Record<string, string>;
    confidence: number;
    tokensUsed: number;
    durationMs: number;
    success: boolean;
    errorMessage?: string;
}

export interface AggregatedResult {
    mode: AggregationMode;
    synthesis: string;
    consensus: string;
    allResults: AgentResult[];
    actionItems: string[];
    conflicts: string[];
    overallConfidence: number;
}

export interface VRAMStatus {
    usedBytes: number;
    maxBytes: number;
    utilizationPercent: number;
    isThrottled: boolean;
}

export interface ExecutionProgress {
    agentId: string;
    status: 'pending' | 'running' | 'completed' | 'failed';
    progress: number; // 0-100
    message: string;
}

// ============================================================================
// Agent Puppeteer Bridge
// ============================================================================

export class AgentPuppeteerBridge {
    private acpClient: ACPClient;
    private progressCallbacks: Set<(progress: ExecutionProgress) => void> = new Set();
    private resultCallbacks: Set<(result: AgentResult) => void> = new Set();
    private errorCallbacks: Set<(agentId: string, error: string) => void> = new Set();

    constructor(acpClient: ACPClient) {
        this.acpClient = acpClient;
        this.setupEventHandlers();
    }

    // ============================================================================
    // Agent Management
    // ============================================================================

    async registerAgent(config: AgentConfig): Promise<boolean> {
        const response = await this.acpClient.request('agent_puppeteer.register_agent', {
            id: config.id,
            name: config.name,
            role: config.role,
            system_prompt: config.systemPrompt,
            dependencies: config.dependencies,
            priority: config.priority,
            enabled: config.enabled,
            max_retries: config.maxRetries,
            timeout_seconds: config.timeoutSeconds,
            share_context: config.shareContext,
            accumulate_context: config.accumulateContext
        });
        return response.success;
    }

    async registerDefaultAgents(): Promise<boolean> {
        const response = await this.acpClient.request('agent_puppeteer.register_default_agents', {});
        return response.success;
    }

    async getRegisteredAgents(): Promise<AgentConfig[]> {
        const response = await this.acpClient.request('agent_puppeteer.get_registered_agents', {});
        return response.agents || [];
    }

    async unregisterAgent(agentId: string): Promise<boolean> {
        const response = await this.acpClient.request('agent_puppeteer.unregister_agent', {
            agent_id: agentId
        });
        return response.success;
    }

    // ============================================================================
    // Task Execution
    // ============================================================================

    async executeTask(
        input: string,
        agentIds: string[],
        options: {
            aggregation?: AggregationMode;
            executionMode?: ExecutionMode;
        } = {}
    ): Promise<AgentResult[]> {
        const response = await this.acpClient.request('agent_puppeteer.execute_task', {
            input,
            agent_ids: agentIds,
            aggregation: options.aggregation || AggregationMode.Union,
            execution_mode: options.executionMode || ExecutionMode.Sequential
        });

        if (response.error) {
            throw new Error(`Task execution failed: ${response.error}`);
        }

        return response.results || [];
    }

    async executeTaskAsync(
        input: string,
        agentIds: string[],
        options: {
            aggregation?: AggregationMode;
            executionMode?: ExecutionMode;
        } = {}
    ): Promise<string> {
        const response = await this.acpClient.request('agent_puppeteer.execute_task_async', {
            input,
            agent_ids: agentIds,
            aggregation: options.aggregation || AggregationMode.Union,
            execution_mode: options.executionMode || ExecutionMode.Sequential
        });

        if (response.error) {
            throw new Error(`Async task execution failed: ${response.error}`);
        }

        return response.task_id;
    }

    async getTaskResults(taskId: string): Promise<AgentResult[]> {
        const response = await this.acpClient.request('agent_puppeteer.get_task_results', {
            task_id: taskId
        });
        return response.results || [];
    }

    // ============================================================================
    // Pipeline Builder (Fluent API)
    // ============================================================================

    pipeline(): PipelineBuilder {
        return new PipelineBuilder(this);
    }

    // ============================================================================
    // Aggregation
    // ============================================================================

    async aggregateResults(
        results: AgentResult[],
        mode: AggregationMode
    ): Promise<AggregatedResult> {
        const response = await this.acpClient.request('agent_puppeteer.aggregate_results', {
            results,
            mode
        });

        if (response.error) {
            throw new Error(`Aggregation failed: ${response.error}`);
        }

        return response.aggregated;
    }

    // ============================================================================
    // VRAM Status
    // ============================================================================

    async getVRAMStatus(): Promise<VRAMStatus> {
        const response = await this.acpClient.request('agent_puppeteer.get_vram_status', {});
        return {
            usedBytes: response.vram_used || 0,
            maxBytes: response.vram_max || 0,
            utilizationPercent: response.utilization_percent || 0,
            isThrottled: response.is_throttled || false
        };
    }

    // ============================================================================
    // Context Management
    // ============================================================================

    async setGlobalContext(key: string, value: string): Promise<boolean> {
        const response = await this.acpClient.request('agent_puppeteer.set_global_context', {
            key,
            value
        });
        return response.success;
    }

    async getGlobalContext(key: string): Promise<string | null> {
        const response = await this.acpClient.request('agent_puppeteer.get_global_context', {
            key
        });
        return response.value || null;
    }

    async clearGlobalContext(): Promise<boolean> {
        const response = await this.acpClient.request('agent_puppeteer.clear_global_context', {});
        return response.success;
    }

    // ============================================================================
    // Callbacks
    // ============================================================================

    onProgress(callback: (progress: ExecutionProgress) => void): () => void {
        this.progressCallbacks.add(callback);
        return () => this.progressCallbacks.delete(callback);
    }

    onResult(callback: (result: AgentResult) => void): () => void {
        this.resultCallbacks.add(callback);
        return () => this.resultCallbacks.delete(callback);
    }

    onError(callback: (agentId: string, error: string) => void): () => void {
        this.errorCallbacks.add(callback);
        return () => this.errorCallbacks.delete(callback);
    }

    // ============================================================================
    // Preset Workflows
    // ============================================================================

    async runCodeReview(code: string): Promise<AgentResult[]> {
        return this.pipeline()
            .coder()
            .reviewer()
            .execute(code);
    }

    async runSecurityAudit(code: string): Promise<AgentResult[]> {
        return this.pipeline()
            .architect()
            .coder()
            .security()
            .reviewer()
            .execute(code);
    }

    async runPerformanceOptimization(code: string): Promise<AgentResult[]> {
        return this.pipeline()
            .analyst()
            .coder()
            .optimizer()
            .tester()
            .execute(code);
    }

    async runFullDevelopmentCycle(requirements: string): Promise<AgentResult[]> {
        return this.pipeline()
            .planner()
            .architect()
            .coder()
            .tester()
            .reviewer()
            .security()
            .documenter()
            .execute(requirements);
    }

    // ============================================================================
    // Private
    // ============================================================================

    private setupEventHandlers(): void {
        // Listen for progress events from C++ backend
        this.acpClient.onNotification('agent_puppeteer.progress', (params) => {
            const progress: ExecutionProgress = {
                agentId: params.agent_id,
                status: params.status,
                progress: params.progress,
                message: params.message
            };
            this.progressCallbacks.forEach(cb => cb(progress));
        });

        // Listen for result events
        this.acpClient.onNotification('agent_puppeteer.result', (params) => {
            const result: AgentResult = {
                agentId: params.agent_id,
                agentRole: params.agent_role,
                output: params.output,
                artifacts: params.artifacts || [],
                contextUpdates: params.context_updates || {},
                confidence: params.confidence,
                tokensUsed: params.tokens_used,
                durationMs: params.duration_ms,
                success: params.success,
                errorMessage: params.error_message
            };
            this.resultCallbacks.forEach(cb => cb(result));
        });

        // Listen for error events
        this.acpClient.onNotification('agent_puppeteer.error', (params) => {
            this.errorCallbacks.forEach(cb => cb(params.agent_id, params.error));
        });
    }
}

// ============================================================================
// Pipeline Builder
// ============================================================================

export class PipelineBuilder {
    private bridge: AgentPuppeteerBridge;
    private agentIds: string[] = [];

    constructor(bridge: AgentPuppeteerBridge) {
        this.bridge = bridge;
    }

    add(agentId: string): PipelineBuilder {
        this.agentIds.push_back(agentId);
        return this;
    }

    planner(): PipelineBuilder { return this.add('planner'); }
    coder(): PipelineBuilder { return this.add('coder'); }
    reviewer(): PipelineBuilder { return this.add('reviewer'); }
    analyst(): PipelineBuilder { return this.add('analyst'); }
    tester(): PipelineBuilder { return this.add('tester'); }
    architect(): PipelineBuilder { return this.add('architect'); }
    security(): PipelineBuilder { return this.add('security'); }
    optimizer(): PipelineBuilder { return this.add('optimizer'); }
    documenter(): PipelineBuilder { return this.add('documenter'); }

    async execute(input: string): Promise<AgentResult[]> {
        return this.bridge.executeTask(input, this.agentIds);
    }

    async executeAsync(input: string): Promise<string> {
        return this.bridge.executeTaskAsync(input, this.agentIds);
    }
}

// ============================================================================
// React Hook (for UI integration)
// ============================================================================

import { useState, useEffect, useCallback } from 'react';

export function useAgentPuppeteer(acpClient: ACPClient) {
    const [bridge] = useState(() => new AgentPuppeteerBridge(acpClient));
    const [agents, setAgents] = useState<AgentConfig[]>([]);
    const [progress, setProgress] = useState<ExecutionProgress | null>(null);
    const [results, setResults] = useState<AgentResult[]>([]);
    const [vramStatus, setVramStatus] = useState<VRAMStatus | null>(null);
    const [isLoading, setIsLoading] = useState(false);

    useEffect(() => {
        // Load registered agents
        bridge.getRegisteredAgents().then(setAgents);

        // Setup progress listener
        const unsubscribeProgress = bridge.onProgress((p) => {
            setProgress(p);
        });

        // Setup result listener
        const unsubscribeResult = bridge.onResult((r) => {
            setResults(prev => [...prev, r]);
        });

        // Poll VRAM status
        const vramInterval = setInterval(() => {
            bridge.getVRAMStatus().then(setVramStatus);
        }, 5000);

        return () => {
            unsubscribeProgress();
            unsubscribeResult();
            clearInterval(vramInterval);
        };
    }, [bridge]);

    const executeTask = useCallback(async (
        input: string,
        agentIds: string[],
        options?: { aggregation?: AggregationMode; executionMode?: ExecutionMode }
    ) => {
        setIsLoading(true);
        setResults([]);
        try {
            const taskResults = await bridge.executeTask(input, agentIds, options);
            setResults(taskResults);
            return taskResults;
        } finally {
            setIsLoading(false);
        }
    }, [bridge]);

    const runPreset = useCallback(async (preset: string, input: string) => {
        setIsLoading(true);
        setResults([]);
        try {
            let taskResults: AgentResult[];
            switch (preset) {
                case 'code_review':
                    taskResults = await bridge.runCodeReview(input);
                    break;
                case 'security_audit':
                    taskResults = await bridge.runSecurityAudit(input);
                    break;
                case 'performance':
                    taskResults = await bridge.runPerformanceOptimization(input);
                    break;
                case 'full_cycle':
                    taskResults = await bridge.runFullDevelopmentCycle(input);
                    break;
                default:
                    throw new Error(`Unknown preset: ${preset}`);
            }
            setResults(taskResults);
            return taskResults;
        } finally {
            setIsLoading(false);
        }
    }, [bridge]);

    return {
        bridge,
        agents,
        progress,
        results,
        vramStatus,
        isLoading,
        executeTask,
        runPreset,
        pipeline: () => bridge.pipeline()
    };
}
