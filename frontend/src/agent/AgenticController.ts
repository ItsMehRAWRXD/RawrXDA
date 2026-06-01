import { TelemetrySink } from '../telemetry/TelemetrySink';
import { auditLogService } from '../telemetry/AuditLogService';
import { engineService } from '../engine/EngineService';
import { HitlGate } from './HitlGate';
import { DEFAULT_TOOL_REGISTRY, ToolDefinition, ToolExecutionResult } from './ToolRegistry';

export type AgentStatus =
  | 'IDLE'
  | 'THINKING'
  | 'TOOL_PROPOSAL'
  | 'TOOL_EXEC'
  | 'OBSERVING'
  | 'FAULT';

export interface ProposedToolCall {
  id: string;
  toolName: string;
  params: Record<string, unknown>;
  reason: string;
  riskLevel: ToolDefinition['riskLevel'];
  proposedAt: number;
}

export interface ProposalOutcome {
  status: AgentStatus;
  proposal: ProposedToolCall;
  result: ToolExecutionResult;
}

type StatusCallback = (status: AgentStatus) => void;
type ProposalCallback = (proposal: ProposedToolCall, result: ToolExecutionResult) => void;

export class AgenticController {
  private status: AgentStatus = 'IDLE';
  private registryByName: Record<string, ToolDefinition>;
  private hitlGate: HitlGate;
  private pendingApprovals: Map<string, { proposal: ProposedToolCall; tool: ToolDefinition }> = new Map();
  private approvalPauseActive = false;
  private statusCallbacks: Set<StatusCallback> = new Set();
  private proposalCallbacks: Set<ProposalCallback> = new Set();

  constructor(registry: ToolDefinition[] = DEFAULT_TOOL_REGISTRY, hitlGate = new HitlGate('PROPOSE_ONLY')) {
    this.registryByName = registry.reduce<Record<string, ToolDefinition>>((acc, tool) => {
      acc[tool.name] = tool;
      return acc;
    }, {});
    this.hitlGate = hitlGate;
  }

  public getStatus(): AgentStatus {
    return this.status;
  }

  public getHitlMode(): ReturnType<HitlGate['getMode']> {
    return this.hitlGate.getMode();
  }

  public getToolCount(): number {
    return Object.keys(this.registryByName).length;
  }

  public getToolNames(): string[] {
    return Object.keys(this.registryByName).sort();
  }

  public getPendingApprovalCount(): number {
    return this.pendingApprovals.size;
  }

  public onStatusChange(callback: StatusCallback): () => void {
    this.statusCallbacks.add(callback);
    return () => {
      this.statusCallbacks.delete(callback);
    };
  }

  public onProposal(callback: ProposalCallback): () => void {
    this.proposalCallbacks.add(callback);
    return () => {
      this.proposalCallbacks.delete(callback);
    };
  }

  public async proposeToolCall(
    toolName: string,
    params: Record<string, unknown>,
    reason: string
  ): Promise<ProposalOutcome> {
    const tool = this.registryByName[toolName];
    if (!tool) {
      this.setStatus('FAULT');
      const fallbackProposal: ProposedToolCall = {
        id: `proposal-${Date.now()}`,
        toolName,
        params,
        reason,
        riskLevel: 'HIGH',
        proposedAt: Date.now(),
      };
      const result: ToolExecutionResult = {
        status: 'FAILED',
        message: `Unknown tool: ${toolName}`,
      };
      auditLogService.record({
        kind: 'EXECUTION_RESULT',
        toolName,
        executionStatus: 'FAILED',
        message: result.message,
      });
      this.emitProposal(fallbackProposal, result);
      return {
        status: this.status,
        proposal: fallbackProposal,
        result,
      };
    }

    this.setStatus('THINKING');

    const proposal: ProposedToolCall = {
      id: `proposal-${Date.now()}`,
      toolName,
      params,
      reason,
      riskLevel: tool.riskLevel,
      proposedAt: Date.now(),
    };

    auditLogService.record({
      kind: 'PROPOSAL',
      proposalId: proposal.id,
      toolName: proposal.toolName,
      riskLevel: proposal.riskLevel,
      reason: proposal.reason,
      paramsPreview: this.buildParamsPreview(proposal.params),
      timestamp: proposal.proposedAt,
    });

    this.setStatus('TOOL_PROPOSAL');

    const decision = await this.hitlGate.evaluate(tool.name, tool.riskLevel);
    if (!decision.allowExecution) {
      this.pendingApprovals.set(proposal.id, { proposal, tool });
      await this.updateApprovalPauseState(true);

      const blockedResult: ToolExecutionResult = {
        status: 'PENDING_APPROVAL',
        message: decision.reason,
      };

      auditLogService.record({
        kind: 'PENDING_APPROVAL',
        proposalId: proposal.id,
        toolName: proposal.toolName,
        riskLevel: proposal.riskLevel,
        reason: proposal.reason,
        paramsPreview: this.buildParamsPreview(proposal.params),
        executionStatus: blockedResult.status,
        message: blockedResult.message,
      });

      void TelemetrySink.log(
        {
          type: 'TOOL_PENDING_APPROVAL',
          severity: proposal.riskLevel === 'HIGH' ? 'CRITICAL' : 'HIGH',
        },
        `AGENT_PENDING_APPROVAL:${proposal.toolName}`
      );

      this.emitProposal(proposal, blockedResult);
      void TelemetrySink.log(
        {
          type: 'STREAM_BLOCK',
          severity: proposal.riskLevel === 'HIGH' ? 'CRITICAL' : proposal.riskLevel === 'MEDIUM' ? 'HIGH' : 'LOW',
        },
        `AGENT_PROPOSAL_BLOCK:${proposal.toolName}`
      );
      this.setStatus('OBSERVING');
      this.setStatus('IDLE');
      return {
        status: this.status,
        proposal,
        result: blockedResult,
      };
    }

    const execResult = await this.executeToolWithTelemetry(proposal, tool, params);

    this.emitProposal(proposal, execResult);
    this.setStatus('OBSERVING');
    this.setStatus('IDLE');

    return {
      status: this.status,
      proposal,
      result: execResult,
    };
  }

  public async resolvePendingProposal(proposalId: string, approve: boolean): Promise<ProposalOutcome | null> {
    const pending = this.pendingApprovals.get(proposalId);
    if (!pending) {
      return null;
    }

    this.pendingApprovals.delete(proposalId);

    if (!approve) {
      auditLogService.record({
        kind: 'USER_DENIED',
        proposalId: pending.proposal.id,
        toolName: pending.proposal.toolName,
        riskLevel: pending.proposal.riskLevel,
        reason: pending.proposal.reason,
        paramsPreview: this.buildParamsPreview(pending.proposal.params),
      });

      void TelemetrySink.log(
        {
          type: 'USER_DENIED',
          severity: pending.proposal.riskLevel === 'HIGH' ? 'CRITICAL' : 'HIGH',
        },
        `AGENT_DENIED:${pending.proposal.toolName}`
      );

      const deniedResult: ToolExecutionResult = {
        status: 'DENIED',
        message: 'Human reviewer denied this tool call.',
      };

      this.emitProposal(pending.proposal, deniedResult);
      await this.updateApprovalPauseState(this.pendingApprovals.size > 0);
      this.setStatus('OBSERVING');
      this.setStatus('IDLE');

      return {
        status: this.status,
        proposal: pending.proposal,
        result: deniedResult,
      };
    }

    void TelemetrySink.log(
      {
        type: 'USER_APPROVED',
        severity: pending.proposal.riskLevel === 'HIGH' ? 'CRITICAL' : 'HIGH',
      },
      `AGENT_APPROVED:${pending.proposal.toolName}`
    );

    auditLogService.record({
      kind: 'USER_APPROVED',
      proposalId: pending.proposal.id,
      toolName: pending.proposal.toolName,
      riskLevel: pending.proposal.riskLevel,
      reason: pending.proposal.reason,
      paramsPreview: this.buildParamsPreview(pending.proposal.params),
    });

    await this.updateApprovalPauseState(this.pendingApprovals.size > 0);

    const approvedResult = await this.executeToolWithTelemetry(
      pending.proposal,
      pending.tool,
      pending.proposal.params
    );

    this.emitProposal(pending.proposal, approvedResult);

    this.setStatus('OBSERVING');
    this.setStatus('IDLE');

    return {
      status: this.status,
      proposal: pending.proposal,
      result: approvedResult,
    };
  }

  private async updateApprovalPauseState(shouldPause: boolean): Promise<void> {
    if (shouldPause && !this.approvalPauseActive) {
      try {
        await engineService.pauseAgenticExecution();
        this.approvalPauseActive = true;
        void TelemetrySink.log(
          {
            type: 'ENGINE_PAUSED',
            severity: 'HIGH',
          },
          'HITL_ENGINE_PAUSED'
        );
        auditLogService.record({
          kind: 'ENGINE_PAUSED',
          message: 'Execution lane paused pending HITL decision.',
        });
      } catch {
        // Fail-safe: keep agent approval gate active even if backend pause endpoint is unavailable.
      }
      return;
    }

    if (!shouldPause && this.approvalPauseActive) {
      try {
        await engineService.resumeAgenticExecution();
        this.approvalPauseActive = false;
        void TelemetrySink.log(
          {
            type: 'ENGINE_RESUMED',
            severity: 'LOW',
          },
          'HITL_ENGINE_RESUMED'
        );
        auditLogService.record({
          kind: 'ENGINE_RESUMED',
          message: 'Execution lane resumed after HITL resolution.',
        });
      } catch {
        // Fail-safe: execution stays gated by HITL until explicit approvals are resolved.
      }
    }
  }

  private async executeToolWithTelemetry(
    proposal: ProposedToolCall,
    tool: ToolDefinition,
    params: Record<string, unknown>
  ): Promise<ToolExecutionResult> {
    this.setStatus('TOOL_EXEC');

    try {
      const execResult = await tool.execute(params);
      if (execResult.status === 'EXECUTED') {
        void TelemetrySink.log(
          {
            type: 'TOOL_EXECUTION_SUCCESS',
            severity: proposal.riskLevel === 'HIGH' ? 'CRITICAL' : proposal.riskLevel === 'MEDIUM' ? 'HIGH' : 'LOW',
          },
          `AGENT_EXEC_SUCCESS:${proposal.toolName}`
        );

        if (proposal.toolName === 'write_file') {
          void TelemetrySink.log(
            {
              type: 'TOOL_WRITE_FILE_SUCCESS',
              severity: 'HIGH',
            },
            `AGENT_WRITE_SUCCESS:${proposal.toolName}`
          );
        }
      }

      auditLogService.record({
        kind: 'EXECUTION_RESULT',
        proposalId: proposal.id,
        toolName: proposal.toolName,
        riskLevel: proposal.riskLevel,
        reason: proposal.reason,
        paramsPreview: this.buildParamsPreview(proposal.params),
        executionStatus: execResult.status,
        message: execResult.message,
      });
      return execResult;
    } catch (error) {
      this.setStatus('FAULT');
      const failed: ToolExecutionResult = {
        status: 'FAILED',
        message: error instanceof Error ? error.message : 'Tool execution failed',
      };
      auditLogService.record({
        kind: 'EXECUTION_RESULT',
        proposalId: proposal.id,
        toolName: proposal.toolName,
        riskLevel: proposal.riskLevel,
        reason: proposal.reason,
        paramsPreview: this.buildParamsPreview(proposal.params),
        executionStatus: failed.status,
        message: failed.message,
      });
      return failed;
    }
  }

  private buildParamsPreview(params: Record<string, unknown>): string {
    try {
      const serialized = JSON.stringify(params);
      if (!serialized) {
        return '{}';
      }
      return serialized.length > 280 ? `${serialized.slice(0, 280)}...` : serialized;
    } catch {
      return '[unserializable params]';
    }
  }

  private setStatus(next: AgentStatus): void {
    this.status = next;
    for (const callback of this.statusCallbacks) {
      callback(next);
    }
  }

  private emitProposal(proposal: ProposedToolCall, result: ToolExecutionResult): void {
    for (const callback of this.proposalCallbacks) {
      callback(proposal, result);
    }
  }
}

export const agenticController = new AgenticController();
