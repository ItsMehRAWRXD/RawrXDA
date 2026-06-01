export type AuditEventKind =
  | 'PROPOSAL'
  | 'PENDING_APPROVAL'
  | 'USER_APPROVED'
  | 'USER_DENIED'
  | 'ENGINE_PAUSED'
  | 'ENGINE_RESUMED'
  | 'EXECUTION_RESULT'
  | 'TELEMETRY';

export interface AuditEvent {
  id: string;
  timestamp: number;
  kind: AuditEventKind;
  proposalId?: string;
  toolName?: string;
  riskLevel?: 'LOW' | 'MEDIUM' | 'HIGH';
  reason?: string;
  paramsPreview?: string;
  executionStatus?: 'PENDING_APPROVAL' | 'APPROVED' | 'DENIED' | 'EXECUTED' | 'FAILED';
  message?: string;
  telemetryType?: string;
  telemetrySeverity?: 'LOW' | 'HIGH' | 'CRITICAL';
  hashedPrompt?: string;
}

type AuditChangeCallback = (events: AuditEvent[]) => void;

interface ReportPayload {
  schemaVersion: 'day16.hitl-audit.v1';
  generatedAtEpochMs: number;
  eventCount: number;
  events: AuditEvent[];
}

export type WriteOperationClassification = 'COMPLIANT' | 'SUSPICIOUS';

export interface WriteOperationReport {
  proposalId: string;
  toolName: 'write_file';
  riskLevel: 'LOW' | 'MEDIUM' | 'HIGH' | 'UNKNOWN';
  proposedAt?: number;
  approvedAt?: number;
  deniedAt?: number;
  executedAt?: number;
  executionStatus?: 'PENDING_APPROVAL' | 'APPROVED' | 'DENIED' | 'EXECUTED' | 'FAILED';
  reason?: string;
  paramsPreview?: string;
  executionMessage?: string;
  classification: WriteOperationClassification;
  rationale: string[];
}

export interface ComplianceReportPayload {
  schemaVersion: 'day17.compliance-report.v1';
  generatedAtEpochMs: number;
  totalAuditEvents: number;
  analyzedWriteOperations: number;
  compliantWriteOperations: number;
  suspiciousWriteOperations: number;
  writeOperations: WriteOperationReport[];
}

const MAX_EVENTS = 1500;

class AuditLogService {
  private events: AuditEvent[] = [];
  private callbacks: Set<AuditChangeCallback> = new Set();

  public record(event: Omit<AuditEvent, 'id' | 'timestamp'> & { timestamp?: number }): void {
    const normalized: AuditEvent = {
      ...event,
      id: `audit-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      timestamp: event.timestamp ?? Date.now(),
    };

    this.events.unshift(normalized);
    if (this.events.length > MAX_EVENTS) {
      this.events = this.events.slice(0, MAX_EVENTS);
    }

    this.emit();
  }

  public onChange(callback: AuditChangeCallback): () => void {
    this.callbacks.add(callback);
    return () => {
      this.callbacks.delete(callback);
    };
  }

  public getEvents(limit = 120): AuditEvent[] {
    return this.events.slice(0, Math.max(1, limit)).map((event) => ({ ...event }));
  }

  public clear(): void {
    this.events = [];
    this.emit();
  }

  public exportReport(limit = 600): ReportPayload {
    const events = this.getEvents(limit);
    return {
      schemaVersion: 'day16.hitl-audit.v1',
      generatedAtEpochMs: Date.now(),
      eventCount: events.length,
      events,
    };
  }

  public exportComplianceReport(limit = 1200): ComplianceReportPayload {
    const events = this.getEvents(limit);
    const chronological = [...events].sort((a, b) => a.timestamp - b.timestamp);

    const proposalsById: Record<string, WriteOperationReport> = {};
    const proposalFingerprintCounts: Record<string, number> = {};

    for (const event of chronological) {
      if (!event.proposalId) {
        continue;
      }

      const isWriteToolEvent = event.toolName === 'write_file';
      const knownWriteOperation = Boolean(proposalsById[event.proposalId]);
      if (!isWriteToolEvent && !knownWriteOperation) {
        continue;
      }

      if (!proposalsById[event.proposalId]) {
        proposalsById[event.proposalId] = {
          proposalId: event.proposalId,
          toolName: 'write_file',
          riskLevel: event.riskLevel ?? 'UNKNOWN',
          reason: event.reason,
          paramsPreview: event.paramsPreview,
          classification: 'SUSPICIOUS',
          rationale: [],
        };
      }

      const op = proposalsById[event.proposalId];

      if (event.kind === 'PROPOSAL') {
        op.proposedAt = event.timestamp;
        op.reason = event.reason ?? op.reason;
        op.paramsPreview = event.paramsPreview ?? op.paramsPreview;

        const fingerprint = `${op.reason ?? ''}|${op.paramsPreview ?? ''}`;
        proposalFingerprintCounts[fingerprint] = (proposalFingerprintCounts[fingerprint] ?? 0) + 1;
      }

      if (event.kind === 'USER_APPROVED') {
        op.approvedAt = event.timestamp;
      }

      if (event.kind === 'USER_DENIED') {
        op.deniedAt = event.timestamp;
      }

      if (event.kind === 'EXECUTION_RESULT') {
        op.executedAt = event.timestamp;
        op.executionStatus = event.executionStatus;
        op.executionMessage = event.message;
      }
    }

    const writeOperations = Object.values(proposalsById).map((op) => {
      const rationale: string[] = [];
      const fingerprint = `${op.reason ?? ''}|${op.paramsPreview ?? ''}`;
      const fingerprintCount = proposalFingerprintCounts[fingerprint] ?? 0;

      if (fingerprintCount > 1) {
        rationale.push(`Retried proposal signature detected (${fingerprintCount} attempts).`);
      }

      if (op.deniedAt) {
        rationale.push('Write operation was denied by human reviewer.');
      }

      if (!op.approvedAt && op.executionStatus === 'EXECUTED') {
        rationale.push('Write executed without an explicit approval event.');
      }

      if (op.approvedAt && op.executionStatus && op.executionStatus !== 'EXECUTED') {
        rationale.push(`Approved write ended with non-success status ${op.executionStatus}.`);
      }

      if (!op.executionStatus) {
        rationale.push('Write proposal has no terminal execution result.');
      }

      if (op.executionStatus === 'FAILED') {
        rationale.push('Write execution failed.');
      }

      const compliant = Boolean(op.approvedAt) && op.executionStatus === 'EXECUTED' && !op.deniedAt && fingerprintCount <= 1;
      if (compliant) {
        return {
          ...op,
          classification: 'COMPLIANT' as const,
          rationale: ['Approved by human reviewer and executed successfully.'],
        };
      }

      return {
        ...op,
        classification: 'SUSPICIOUS' as const,
        rationale: rationale.length > 0 ? rationale : ['Write operation requires manual review.'],
      };
    });

    const compliantWriteOperations = writeOperations.filter((op) => op.classification === 'COMPLIANT').length;
    const suspiciousWriteOperations = writeOperations.length - compliantWriteOperations;

    return {
      schemaVersion: 'day17.compliance-report.v1',
      generatedAtEpochMs: Date.now(),
      totalAuditEvents: events.length,
      analyzedWriteOperations: writeOperations.length,
      compliantWriteOperations,
      suspiciousWriteOperations,
      writeOperations,
    };
  }

  private emit(): void {
    const snapshot = this.getEvents();
    for (const callback of this.callbacks) {
      callback(snapshot);
    }
  }
}

export const auditLogService = new AuditLogService();
