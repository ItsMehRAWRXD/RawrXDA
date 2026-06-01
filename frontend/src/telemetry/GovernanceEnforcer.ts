import { auditLogService, AuditEvent } from './AuditLogService';
import type { RiskLevel } from '../agent/ToolRegistry';

export type EnforcementAction = 'DEMOTE' | 'HARD_LOCK' | 'NONE';

export interface GovernanceThreshold {
  maxSuspiciousEventsPerWindow: number;
  windowDurationMs: number;
  consecutiveDenialsThreshold: number;
  consecutiveDenialsWindowMs: number;
}

export interface ToolOverride {
  toolName: string;
  enforcedRiskLevel: RiskLevel;
  reason: string;
  enforcedAt: number;
  expiresAt: number | null;
}

export interface EnforcementEvent {
  kind: 'GOVERNANCE_ENFORCED';
  id: string;
  timestamp: number;
  action: EnforcementAction;
  toolName: string;
  previousRiskLevel: RiskLevel;
  enforcedRiskLevel: RiskLevel;
  reason: string;
  thresholdSnapshot: GovernanceThreshold;
  suspiciousCountInWindow: number;
}

type EnforcementCallback = (event: EnforcementEvent) => void;

const DEFAULT_THRESHOLDS: GovernanceThreshold = {
  maxSuspiciousEventsPerWindow: 3,
  windowDurationMs: 60 * 60 * 1000, // 1 hour
  consecutiveDenialsThreshold: 2,
  consecutiveDenialsWindowMs: 10 * 60 * 1000, // 10 minutes
};

/**
 * GovernanceEnforcer
 * Day 18: Active Policy Layer — Self-Healing Security Posture
 *
 * Architecture:
 * 1. Subscribes to AuditLogService as a sensor input.
 * 2. Maintains a sliding-window counter of suspicious events.
 * 3. When thresholds are breached, ratchets tool permissions upward
 *    (LOW → MEDIUM → HARD_LOCK).
 * 4. Emits EnforcementEvents into the audit timeline for forensic traceability.
 * 5. Provides a human-operated reset ("Break Glass") to clear all overrides.
 */
export class GovernanceEnforcer {
  private thresholds: GovernanceThreshold;
  private overrides: Map<string, ToolOverride> = new Map();
  private callbacks: Set<EnforcementCallback> = new Set();
  private unsubscribeAudit: (() => void) | null = null;
  private isRunning = false;

  constructor(thresholds: Partial<GovernanceThreshold> = {}) {
    this.thresholds = { ...DEFAULT_THRESHOLDS, ...thresholds };
  }

  public start(): void {
    if (this.isRunning) {
      return;
    }
    this.isRunning = true;
    this.unsubscribeAudit = auditLogService.onChange((events) => {
      this.evaluate(events);
    });
  }

  public stop(): void {
    if (this.unsubscribeAudit) {
      this.unsubscribeAudit();
      this.unsubscribeAudit = null;
    }
    this.isRunning = false;
  }

  public onEnforcement(callback: EnforcementCallback): () => void {
    this.callbacks.add(callback);
    return () => {
      this.callbacks.delete(callback);
    };
  }

  /**
   * Human-operated "Break Glass" reset.
   * Clears all dynamic overrides and restores the static risk definitions.
   */
  public reset(): void {
    const cleared = Array.from(this.overrides.values());
    this.overrides.clear();

    for (const override of cleared) {
      const event: EnforcementEvent = {
        kind: 'GOVERNANCE_ENFORCED',
        id: `enforcement-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
        timestamp: Date.now(),
        action: 'NONE',
        toolName: override.toolName,
        previousRiskLevel: override.enforcedRiskLevel,
        enforcedRiskLevel: override.enforcedRiskLevel,
        reason: 'Human operator initiated Break Glass — override cleared.',
        thresholdSnapshot: { ...this.thresholds },
        suspiciousCountInWindow: 0,
      };
      this.emit(event);
      auditLogService.record({
        kind: 'GOVERNANCE_ENFORCED',
        proposalId: event.id,
        toolName: override.toolName,
        riskLevel: override.enforcedRiskLevel,
        reason: event.reason,
        message: `Override cleared for ${override.toolName}`,
      });
    }
  }

  public getOverrides(): ToolOverride[] {
    return Array.from(this.overrides.values()).map((o) => ({ ...o }));
  }

  public getOverride(toolName: string): ToolOverride | undefined {
    const o = this.overrides.get(toolName);
    return o ? { ...o } : undefined;
  }

  public getEffectiveRiskLevel(toolName: string, baselineRisk: RiskLevel): RiskLevel {
    const override = this.overrides.get(toolName);
    if (!override) {
      return baselineRisk;
    }
    if (override.expiresAt !== null && Date.now() > override.expiresAt) {
      this.overrides.delete(toolName);
      return baselineRisk;
    }
    return override.enforcedRiskLevel;
  }

  public isEnforcementActive(): boolean {
    return this.overrides.size > 0;
  }

  private evaluate(events: AuditEvent[]): void {
    const now = Date.now();
    const windowStart = now - this.thresholds.windowDurationMs;

    const suspiciousInWindow = events.filter(
      (e) =>
        e.timestamp >= windowStart &&
        (e.kind === 'EXECUTION_RESULT' && e.executionStatus === 'FAILED') ||
        this.isSuspiciousEvent(e)
    );

    const suspiciousCount = suspiciousInWindow.length;

    if (suspiciousCount >= this.thresholds.maxSuspiciousEventsPerWindow) {
      this.applyRatchet(suspiciousCount);
    }

    // Also check consecutive denials in a shorter window
    const denialWindowStart = now - this.thresholds.consecutiveDenialsWindowMs;
    const denialEvents = events.filter(
      (e) => e.timestamp >= denialWindowStart && e.kind === 'USER_DENIED'
    );

    if (denialEvents.length >= this.thresholds.consecutiveDenialsThreshold) {
      this.applyDenialRatchet(denialEvents.length);
    }
  }

  private isSuspiciousEvent(event: AuditEvent): boolean {
    if (event.kind === 'EXECUTION_RESULT' && event.executionStatus === 'FAILED') {
      return true;
    }
    // A write_file that was denied is also suspicious
    if (event.kind === 'USER_DENIED' && event.toolName === 'write_file') {
      return true;
    }
    return false;
  }

  private applyRatchet(suspiciousCount: number): void {
    // Ratchet all MEDIUM and LOW write tools upward
    const toolsToRatchet = ['write_file', 'edit_file', 'create_file', 'rename_file', 'move_file'];

    for (const toolName of toolsToRatchet) {
      this.demoteTool(toolName, suspiciousCount, 'Sliding-window threshold breached');
    }
  }

  private applyDenialRatchet(denialCount: number): void {
    // Consecutive denials ratchet the specific tool that was denied
    // We don't know which tool from the count alone, so we ratchet all write tools
    const toolsToRatchet = ['write_file', 'edit_file', 'create_file', 'rename_file', 'move_file'];

    for (const toolName of toolsToRatchet) {
      this.demoteTool(toolName, denialCount, 'Consecutive human denial threshold breached');
    }
  }

  private demoteTool(
    toolName: string,
    triggerCount: number,
    triggerReason: string
  ): void {
    const existing = this.overrides.get(toolName);
    const currentEffective = existing ? existing.enforcedRiskLevel : this.inferBaselineRisk(toolName);

    let nextRisk: RiskLevel;
    let action: EnforcementAction;

    if (currentEffective === 'LOW') {
      nextRisk = 'MEDIUM';
      action = 'DEMOTE';
    } else if (currentEffective === 'MEDIUM') {
      nextRisk = 'HIGH';
      action = 'DEMOTE';
    } else {
      nextRisk = 'HIGH';
      action = 'HARD_LOCK';
    }

    // If already at HARD_LOCK, don't re-emit
    if (existing && existing.enforcedRiskLevel === 'HIGH' && action === 'HARD_LOCK') {
      return;
    }

    const override: ToolOverride = {
      toolName,
      enforcedRiskLevel: nextRisk,
      reason: `${triggerReason} (${triggerCount} events). Auto-demoted by GovernanceEnforcer.`,
      enforcedAt: Date.now(),
      expiresAt: null, // Permanent until human reset
    };

    this.overrides.set(toolName, override);

    const event: EnforcementEvent = {
      kind: 'GOVERNANCE_ENFORCED',
      id: `enforcement-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      timestamp: Date.now(),
      action,
      toolName,
      previousRiskLevel: currentEffective,
      enforcedRiskLevel: nextRisk,
      reason: override.reason,
      thresholdSnapshot: { ...this.thresholds },
      suspiciousCountInWindow: triggerCount,
    };

    this.emit(event);

    auditLogService.record({
      kind: 'GOVERNANCE_ENFORCED',
      proposalId: event.id,
      toolName,
      riskLevel: nextRisk,
      reason: override.reason,
      message: `GovernanceEnforcer ${action}: ${toolName} ${currentEffective} → ${nextRisk}`,
    });
  }

  private inferBaselineRisk(toolName: string): RiskLevel {
    // Mirror of static manifest for tools that may not have been overridden yet
    const baselineMap: Record<string, RiskLevel> = {
      read_file: 'LOW',
      list_dir: 'LOW',
      search_code: 'LOW',
      get_symbol_refs: 'LOW',
      write_file: 'MEDIUM',
      edit_file: 'MEDIUM',
      create_file: 'MEDIUM',
      delete_file: 'HIGH',
      rename_file: 'MEDIUM',
      move_file: 'MEDIUM',
      open_terminal: 'MEDIUM',
      execute_shell: 'HIGH',
      install_dependency: 'HIGH',
      run_tests: 'LOW',
      run_lint: 'LOW',
      run_format: 'LOW',
      git_status: 'LOW',
      git_diff: 'LOW',
      git_add: 'MEDIUM',
      git_commit: 'HIGH',
      git_branch: 'MEDIUM',
      git_checkout: 'MEDIUM',
      git_merge: 'HIGH',
      git_rebase: 'HIGH',
      create_pr: 'MEDIUM',
      fetch_http: 'LOW',
      post_http: 'MEDIUM',
      parse_json: 'LOW',
      parse_yaml: 'LOW',
      parse_markdown: 'LOW',
      run_sql_query: 'HIGH',
      migrate_db: 'HIGH',
      seed_db: 'HIGH',
      start_dev_server: 'MEDIUM',
      stop_dev_server: 'MEDIUM',
      run_benchmark: 'MEDIUM',
      collect_telemetry: 'MEDIUM',
      inspect_logs: 'LOW',
      restart_engine: 'HIGH',
      deploy_staging: 'HIGH',
      deploy_production: 'HIGH',
      rollback_release: 'HIGH',
      manage_secrets: 'HIGH',
      update_ci_workflow: 'HIGH',
    };
    return baselineMap[toolName] ?? 'HIGH';
  }

  private emit(event: EnforcementEvent): void {
    for (const callback of this.callbacks) {
      callback(event);
    }
  }
}

export const governanceEnforcer = new GovernanceEnforcer();
