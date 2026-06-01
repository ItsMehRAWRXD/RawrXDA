import React, { useEffect, useMemo, useState } from 'react';
import { agenticController, AgentStatus, ProposedToolCall } from '../agent/AgenticController';
import { ToolExecutionResult } from '../agent/ToolRegistry';

const TOOL_DEFAULT = 'read_file';

interface ProposalRecord {
  proposal: ProposedToolCall;
  result: ToolExecutionResult;
}

export const AgentPanel: React.FC = () => {
  const [status, setStatus] = useState<AgentStatus>(agenticController.getStatus());
  const [selectedTool, setSelectedTool] = useState<string>(TOOL_DEFAULT);
  const [proposalReason, setProposalReason] = useState<string>('Static analysis pass over project files');
  const [lastProposal, setLastProposal] = useState<ProposalRecord | null>(null);
  const [pendingApproval, setPendingApproval] = useState<ProposalRecord | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  const tools = useMemo(() => agenticController.getToolNames(), []);

  useEffect(() => {
    const unsubStatus = agenticController.onStatusChange((next) => {
      setStatus(next);
    });

    const unsubProposal = agenticController.onProposal((proposal, result) => {
      const nextRecord = { proposal, result };
      setLastProposal(nextRecord);
      if (result.status === 'PENDING_APPROVAL' && proposal.riskLevel !== 'LOW') {
        setPendingApproval(nextRecord);
      } else {
        setPendingApproval((current) => (current?.proposal.id === proposal.id ? null : current));
      }
      setIsSubmitting(false);
    });

    return () => {
      unsubStatus();
      unsubProposal();
    };
  }, []);

  const handlePropose = async () => {
    if (isSubmitting) {
      return;
    }

    setIsSubmitting(true);

    let params: Record<string, unknown> = { source: 'AgentPanel', dryRun: true };
    if (selectedTool === 'read_file') {
      params = { path: 'frontend/src/App.tsx', startLine: 1, endLine: 120 };
    } else if (selectedTool === 'list_dir') {
      params = { path: 'frontend/src' };
    } else if (selectedTool === 'search_code') {
      params = { query: 'engineService' };
    } else if (selectedTool === 'write_file') {
      params = {
        path: 'tmp/day15_write_smoke.txt',
        content: 'day15 smoke write\n',
        append: false,
        createDirs: true,
      };
    }

    await agenticController.proposeToolCall(
      selectedTool,
      params,
      proposalReason
    );
  };

  const handleApprovalDecision = async (approve: boolean) => {
    if (!pendingApproval || isSubmitting) {
      return;
    }

    setIsSubmitting(true);
    await agenticController.resolvePendingProposal(pendingApproval.proposal.id, approve);
  };

  return (
    <div className="agent-panel">
      <div className="agent-panel-header">
        <span className="agent-panel-title">Agent (Restricted)</span>
        <span className={`agent-status agent-status-${status.toLowerCase()}`}>{status}</span>
      </div>

      <div className="agent-meta">
        <span>HITL: {agenticController.getHitlMode()}</span>
        <span>Tools: {agenticController.getToolCount()}</span>
      </div>

      <label className="agent-label" htmlFor="agent-tool-select">Tool Proposal</label>
      <select
        id="agent-tool-select"
        className="agent-select"
        value={selectedTool}
        onChange={(e) => setSelectedTool(e.target.value)}
      >
        {tools.map((toolName) => (
          <option key={toolName} value={toolName}>{toolName}</option>
        ))}
      </select>

      <label className="agent-label" htmlFor="agent-reason-input">Reason</label>
      <input
        id="agent-reason-input"
        className="agent-input"
        value={proposalReason}
        onChange={(e) => setProposalReason(e.target.value)}
      />

      <button className="agent-button" onClick={() => void handlePropose()} disabled={isSubmitting}>
        {isSubmitting ? 'Proposing...' : 'Propose Tool Call'}
      </button>

      {pendingApproval && (
        <div className="agent-approval-overlay" role="dialog" aria-modal="true" aria-label="Tool approval required">
          <div className="agent-approval-modal">
            <div className="agent-approval-header">
              <div className="agent-approval-title">Approval Required</div>
              <span className={`agent-approval-risk risk-${pendingApproval.proposal.riskLevel.toLowerCase()}`}>
                {pendingApproval.proposal.riskLevel}
              </span>
            </div>

            <div className="agent-approval-row">
              <strong>Tool:</strong> {pendingApproval.proposal.toolName}
            </div>
            <div className="agent-approval-row">
              <strong>Reason:</strong> {pendingApproval.proposal.reason}
            </div>
            <div className="agent-approval-row">
              <strong>Pending Queue:</strong> {agenticController.getPendingApprovalCount()}
            </div>

            <div className="agent-approval-params">
              <div className="agent-approval-params-title">Parameters</div>
              <pre>{JSON.stringify(pendingApproval.proposal.params, null, 2)}</pre>
            </div>

            <div className="agent-approval-actions">
              <button
                className="agent-button agent-button-deny"
                onClick={() => void handleApprovalDecision(false)}
                disabled={isSubmitting}
              >
                Deny
              </button>
              <button
                className="agent-button agent-button-approve"
                onClick={() => void handleApprovalDecision(true)}
                disabled={isSubmitting}
              >
                Approve
              </button>
            </div>
          </div>
        </div>
      )}

      {lastProposal && (
        <div className="agent-last-proposal">
          <div className="agent-last-proposal-title">Last Proposal</div>
          <div>Tool: {lastProposal.proposal.toolName}</div>
          <div>Risk: {lastProposal.proposal.riskLevel}</div>
          <div>Outcome: {lastProposal.result.status}</div>
          <div className="agent-last-proposal-message">{lastProposal.result.message}</div>
        </div>
      )}
    </div>
  );
};
