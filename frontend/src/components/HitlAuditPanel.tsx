import React, { useEffect, useState } from 'react';
import { auditLogService, AuditEvent } from '../telemetry/AuditLogService';
import { governanceEnforcer } from '../telemetry/GovernanceEnforcer';

const formatTime = (epochMs: number): string => {
  const d = new Date(epochMs);
  return d.toLocaleTimeString();
};

const renderSummary = (event: AuditEvent): string => {
  if (event.kind === 'PROPOSAL') {
    return `Proposed ${event.toolName ?? 'tool'} (${event.riskLevel ?? 'UNKNOWN'})`;
  }

  if (event.kind === 'PENDING_APPROVAL') {
    return `Pending approval for ${event.toolName ?? 'tool'}`;
  }

  if (event.kind === 'USER_APPROVED') {
    return `User approved ${event.toolName ?? 'tool'}`;
  }

  if (event.kind === 'USER_DENIED') {
    return `User denied ${event.toolName ?? 'tool'}`;
  }

  if (event.kind === 'ENGINE_PAUSED') {
    return 'Execution lane paused for HITL review';
  }

  if (event.kind === 'ENGINE_RESUMED') {
    return 'Execution lane resumed';
  }

  if (event.kind === 'EXECUTION_RESULT') {
    return `${event.toolName ?? 'tool'} -> ${event.executionStatus ?? 'UNKNOWN'}`;
  }

  if (event.kind === 'GOVERNANCE_ENFORCED') {
    return `Governance enforced: ${event.toolName ?? 'tool'} ${event.message ?? ''}`;
  }

  return `${event.telemetryType ?? 'TELEMETRY'} ${event.telemetrySeverity ?? ''}`.trim();
};

const renderEnforcementBadge = (event: AuditEvent): React.ReactNode | null => {
  if (event.kind !== 'GOVERNANCE_ENFORCED') {
    return null;
  }
  return (
    <span className="hitl-audit-governance-badge">
      AUTO
    </span>
  );
};

export const HitlAuditPanel: React.FC = () => {
  const [events, setEvents] = useState<AuditEvent[]>(() => auditLogService.getEvents());
  const [enforcementCount, setEnforcementCount] = useState(0);

  const complianceSnapshot = auditLogService.exportComplianceReport();

  useEffect(() => {
    const unsubscribe = auditLogService.onChange((next) => {
      setEvents(next);
    });
    const unsubEnforcement = governanceEnforcer.onEnforcement(() => {
      setEnforcementCount(governanceEnforcer.getOverrides().length);
    });
    return () => {
      unsubscribe();
      unsubEnforcement();
    };
  }, []);

  const exportReport = () => {
    const payload = auditLogService.exportReport();
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `hitl-audit-${payload.generatedAtEpochMs}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const exportComplianceReport = () => {
    const payload = auditLogService.exportComplianceReport();
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `compliance-report-${payload.generatedAtEpochMs}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="hitl-audit-panel">
      <div className="hitl-audit-header">
        <span className="hitl-audit-title">HITL Audit Timeline</span>
        <span className="hitl-audit-count">{events.length}</span>
      </div>

      <div className="hitl-audit-actions">
        <button className="hitl-audit-btn" onClick={exportReport} disabled={events.length === 0}>
          Export Report
        </button>
        <button className="hitl-audit-btn hitl-audit-btn-compliance" onClick={exportComplianceReport} disabled={events.length === 0}>
          Export Compliance
        </button>
        <button className="hitl-audit-btn hitl-audit-btn-clear" onClick={() => auditLogService.clear()} disabled={events.length === 0}>
          Clear View
        </button>
      </div>

      <div className="hitl-audit-summary-strip">
        <span>Writes: {complianceSnapshot.analyzedWriteOperations}</span>
        <span className="hitl-compliant">Compliant: {complianceSnapshot.compliantWriteOperations}</span>
        <span className="hitl-suspicious">Suspicious: {complianceSnapshot.suspiciousWriteOperations}</span>
        {enforcementCount > 0 && (
          <span className="hitl-enforcement">Enforced: {enforcementCount}</span>
        )}
      </div>

      {events.length === 0 && <div className="hitl-audit-empty">No HITL events yet.</div>}

      {events.length > 0 && (
        <div className="hitl-audit-log">
          {events.map((event) => (
            <div key={event.id} className={`hitl-audit-entry ${event.kind === 'GOVERNANCE_ENFORCED' ? 'hitl-audit-entry-governance' : ''}`}>
              <div className="hitl-audit-entry-top">
                <span className="hitl-audit-kind">{event.kind}</span>
                {renderEnforcementBadge(event)}
                <span className="hitl-audit-time">{formatTime(event.timestamp)}</span>
              </div>
              <div className="hitl-audit-summary">{renderSummary(event)}</div>
              {event.reason && <div className="hitl-audit-detail">Intent: {event.reason}</div>}
              {event.paramsPreview && <div className="hitl-audit-detail">Params: {event.paramsPreview}</div>}
              {event.message && <div className="hitl-audit-detail">Message: {event.message}</div>}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};
