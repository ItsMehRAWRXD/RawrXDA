import { TelemetrySink } from '../telemetry/TelemetrySink';
import type { RiskLevel } from './ToolRegistry';

export type HitlMode = 'PROPOSE_ONLY' | 'ENFORCING';

export interface GateDecision {
  state: 'PENDING_APPROVAL' | 'APPROVED' | 'DENIED';
  reason: string;
  allowExecution: boolean;
}

export class HitlGate {
  private mode: HitlMode;

  constructor(mode: HitlMode = 'PROPOSE_ONLY') {
    this.mode = mode;
  }

  public getMode(): HitlMode {
    return this.mode;
  }

  public setMode(mode: HitlMode): void {
    this.mode = mode;
    void TelemetrySink.log(
      {
        type: 'MODE_CHANGE',
        severity: 'LOW',
      },
      `HITL:${mode}`
    );
  }

  public async evaluate(toolName: string, riskLevel: RiskLevel): Promise<GateDecision> {
    if (riskLevel === 'LOW') {
      return {
        state: 'APPROVED',
        reason: 'LOW risk tool auto-approved for Day 12 read-only lane.',
        allowExecution: true,
      };
    }

    if (this.mode === 'PROPOSE_ONLY') {
      void TelemetrySink.log(
        {
          type: 'STREAM_BLOCK',
          severity: riskLevel === 'HIGH' ? 'CRITICAL' : riskLevel === 'MEDIUM' ? 'HIGH' : 'LOW',
        },
        `HITL_BLOCK:${toolName}`
      );

      return {
        state: 'PENDING_APPROVAL',
        reason: 'Propose-only mode: execution blocked pending human approval.',
        allowExecution: false,
      };
    }

    return {
      state: 'PENDING_APPROVAL',
      reason: `${riskLevel} risk tool requires explicit approval token/UI gesture.`,
      allowExecution: false,
    };
  }
}
