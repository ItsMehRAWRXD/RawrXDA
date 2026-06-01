/**
 * StatusDashboard.tsx
 * Day 1 Implementation: Status & Health Dashboard
 * 
 * Governance: UI purely visualizes engine state; does not implement any business logic.
 * Engine-as-Authority: All decisions (retry, fallback, abort) come from engine policy.
 */

import React, { useEffect, useState } from 'react';
import { engineService, EngineStatus } from '../engine/EngineService';
import { getStatusUI } from '../engine/StatusMapper';
import { HeapMonitor } from './HeapMonitor';

export const StatusDashboard: React.FC = () => {
  const [status, setStatus] = useState<EngineStatus | null>(null);
  const [isConnecting, setIsConnecting] = useState(true);

  useEffect(() => {
    // Subscribe to status updates
    const unsubscribe = engineService.onStatusChange((newStatus: EngineStatus) => {
      setStatus(newStatus);
      setIsConnecting(false);
    });

    // Cleanup on unmount
    return () => {
      unsubscribe();
    };
  }, []);

  if (isConnecting || !status) {
    return (
      <div className="dashboard-container connecting">
        <div className="status-bar">
          <span className="icon">⏳</span>
          <span className="label">Connecting to Engine...</span>
        </div>
      </div>
    );
  }

  const uiStyle = getStatusUI(status.loader_context.state);

  return (
    <div className="dashboard-container">
      {/* Status Bar Header */}
      <div className="status-bar" style={{ borderLeftColor: uiStyle.color }}>
        <span className="icon" style={{ color: uiStyle.color }}>
          {uiStyle.icon}
        </span>
        <div className="status-info">
          <span className="label">{uiStyle.label}</span>
          <span className="description">{uiStyle.description}</span>
        </div>
      </div>

      {/* Session & Identity Info */}
      <div className="metadata">
        <span className="meta-item">
          <strong>Session:</strong> {status.session_id.substring(0, 12)}...
        </span>
        <span className="meta-item">
          <strong>Seq:</strong> {status.status_seq}
        </span>
        <HeapMonitor />
      </div>

      {/* Fault Panel: Last Gasp Consumer (Engine-as-Authority) */}
      {status.loader_context.state === 3 && (
        <div className="fault-panel">
          <div className="fault-header">
            <span className="fault-icon">⚠️</span>
            <span className="fault-title">Fault Detected</span>
          </div>

          <div className="fault-details">
            <div className="fault-row">
              <span className="fault-label">Error Tag:</span>
              <code className="fault-value">{status.last_error_tag || 'UNKNOWN'}</code>
            </div>

            <div className="fault-row">
              <span className="fault-label">Fault Class:</span>
              <code className="fault-value">{status.loader_context.fault_class || 'UNKNOWN'}</code>
            </div>

            <div className="fault-row">
              <span className="fault-label">Suggested Action:</span>
              <strong className="fault-action">{status.loader_context.suggested_action || 'UNKNOWN'}</strong>
            </div>

            <div className="fault-row">
              <span className="fault-label">Can Retry:</span>
              <span className={`fault-flag ${status.loader_context.can_retry ? 'yes' : 'no'}`}>
                {status.loader_context.can_retry ? 'Yes' : 'No'}
              </span>
            </div>

            <div className="fault-row">
              <span className="fault-label">Retry Budget Remaining:</span>
              <span className="fault-value">{status.loader_context.retry_budget_rem}</span>
            </div>

            <div className="fault-row">
              <span className="fault-label">Terminal Fault:</span>
              <span className={`fault-flag ${status.loader_context.terminal_fault ? 'yes' : 'no'}`}>
                {status.loader_context.terminal_fault ? 'Yes' : 'No'}
              </span>
            </div>
          </div>

          <div className="fault-note">
            <small>
              The engine has computed the above recovery policy. Do not ignore or override the
              <strong> Suggested Action</strong>.
            </small>
          </div>
        </div>
      )}

      {/* Ready Panel */}
      {status.loader_context.state === 2 && (
        <div className="ready-panel">
          <div className="ready-message">✅ Engine Ready for Inference</div>
          {status.recommended_model && (
            <div className="recommended-model">
              <span className="model-label">Recommended Model:</span>
              <strong>{status.recommended_model}</strong>
            </div>
          )}
        </div>
      )}
    </div>
  );
};
