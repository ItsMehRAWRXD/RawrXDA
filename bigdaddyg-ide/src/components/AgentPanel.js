import React, { useState, useEffect, useMemo, useCallback, useRef } from 'react';
import { useIdeFeatures } from '../contexts/IdeFeaturesContext';
import {
  focusVisibleRing,
  MinimalSurfaceM814Footer,
  CopySupportLineButton
} from '../utils/minimalisticM08M14';

/**
 * Autonomous agent panel: goals, step timeline, human-in-the-loop approval for mutating steps.
 */
const AgentPanel = ({
  onStartAgent,
  taskId,
  taskStatus,
  steps,
  onRefresh,
  onRefreshTaskList,
  taskSnapshots,
  onSelectTask,
  pendingApproval,
  onApproveStep,
  onCancelTask,
  taskGoal,
  taskLog,
  autoApproveWrites,
  onAutoApproveWritesChange,
  autoApproveReads,
  onAutoApproveReadsChange,
  requireAskAiApproval,
  onRequireAskAiApprovalChange,
  enableReflection,
  onEnableReflectionChange,
  requirePlanApproval,
  onRequirePlanApprovalChange,
  batchApproveMutations,
  onBatchApproveMutationsChange,
  enableRollbackSnapshots,
  onEnableRollbackSnapshotsChange,
  rollbackCount,
  onRollbackMutations
}) => {
  const { setStatusLine, noisyLog, pushToast, announceA11y } = useIdeFeatures();
  const [goal, setGoal] = useState('');
  const lastAnnouncedKeyRef = useRef('');
  const lastAgentTerminalKeyRef = useRef('');

  const pollMs = taskStatus === 'awaiting_approval' ? 400 : 2000;

  useEffect(() => {
    if (!taskId || !onRefresh) return undefined;
    const interval = setInterval(() => onRefresh(taskId), pollMs);
    return () => clearInterval(interval);
  }, [taskId, onRefresh, pollMs]);

  useEffect(() => {
    if (!taskId) return;
    const pend = pendingApproval;
    const key = `${taskId}|${taskStatus || ''}|${pend?.kind || ''}|${pend?.stepType || ''}`;
    if (key === lastAnnouncedKeyRef.current) return;
    lastAnnouncedKeyRef.current = key;
    let msg = `Autonomous agent, task ${(taskId || '').slice(0, 14)}: status ${taskStatus || 'unknown'}.`;
    if (taskStatus === 'awaiting_approval' && pend) {
      msg += ` Approval needed for ${pend.stepType || 'step'}.`;
    }
    const bad = taskStatus === 'failed' || taskStatus === 'cancelled';
    announceA11y(msg, { assertive: bad });
  }, [taskId, taskStatus, pendingApproval, announceA11y]);

  useEffect(() => {
    if (!taskId || !taskStatus) return;
    const terminal = new Set([
      'completed',
      'completed_with_errors',
      'completed_with_skips',
      'failed',
      'cancelled'
    ]);
    if (!terminal.has(taskStatus)) {
      lastAgentTerminalKeyRef.current = '';
      return;
    }
    const key = `${taskId}|${taskStatus}`;
    if (lastAgentTerminalKeyRef.current === key) return;
    lastAgentTerminalKeyRef.current = key;
    if (taskStatus === 'completed') {
      setStatusLine('[agent] task completed');
      noisyLog('[agent]', 'task completed', taskId);
    } else if (taskStatus === 'failed' || taskStatus === 'cancelled') {
      setStatusLine(`[agent] task ${taskStatus}`);
      noisyLog('[agent]', `task ${taskStatus}`, taskId);
    } else {
      setStatusLine(`[agent] task ${taskStatus.replace(/_/g, ' ')}`);
      noisyLog('[agent]', 'task finished', taskStatus, taskId);
    }
  }, [taskId, taskStatus, setStatusLine, noisyLog]);

  const taskOptions = useMemo(() => {
    const active = taskSnapshots?.active || [];
    const history = taskSnapshots?.history || [];
    const m = new Map();
    for (const t of active) {
      if (t?.id) m.set(t.id, t);
    }
    for (const t of history) {
      if (t?.id && !m.has(t.id)) m.set(t.id, t);
    }
    return Array.from(m.values()).slice(0, 48);
  }, [taskSnapshots]);

  const formatTaskLabel = useCallback((t) => {
    const g = (t.goal || '').replace(/\s+/g, ' ').trim();
    const head = g.length > 42 ? `${g.slice(0, 42)}…` : g || t.id;
    return `${head} · ${t.status || '?'}`;
  }, []);

  const handleStart = () => {
    const g = goal.trim();
    if (g && onStartAgent) {
      noisyLog('[agent]', 'start requested', g.slice(0, 80));
      setStatusLine('agent: starting task…');
      onStartAgent(g, {
        autoApproveWrites: Boolean(autoApproveWrites),
        autoApproveReads: Boolean(autoApproveReads),
        requireAskAiApproval: Boolean(requireAskAiApproval),
        enableReflection: Boolean(enableReflection),
        requirePlanApproval: Boolean(requirePlanApproval),
        batchApproveMutations: Boolean(batchApproveMutations),
        enableRollbackSnapshots: enableRollbackSnapshots !== false
      });
    }
    setGoal('');
  };

  const awaiting = taskStatus === 'awaiting_approval' && pendingApproval;

  const statusClass = useMemo(() => {
    if (taskStatus === 'completed') return 'text-ide-success';
    if (taskStatus === 'failed' || taskStatus === 'cancelled') return 'text-ide-error';
    if (taskStatus === 'awaiting_approval') return 'text-amber-400';
    return 'text-ide-warning';
  }, [taskStatus]);

  return (
    <div
      className="flex flex-col h-full bg-ide-sidebar text-white min-w-0"
      role="region"
      aria-label="Autonomous agent: goals, approvals, and task timeline"
    >
      <div className="p-3 border-b border-gray-700 shrink-0">
        <h3 className="font-semibold mb-1">Autonomous agent</h3>
        <p
          className="text-[10px] text-gray-500 mb-2"
          title="Main orchestrator; approval gates from Settings › Copilot and main policy."
        >
          Planning orchestrator · plan / mutation batch gates · rollback snapshots · risk tiers
        </p>
        <div className="flex flex-col gap-1.5 text-xs text-gray-400 mb-2">
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="Off by default: write/append steps wait for approval unless enabled."
          >
            <input
              type="checkbox"
              checked={Boolean(autoApproveWrites)}
              onChange={(e) => onAutoApproveWritesChange?.(e.target.checked)}
            />
            Auto-approve writes (write/append)
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="When on, read/list/search_repo steps run without pausing. Does not bypass approval_policy.json for mutating steps unless you also auto-approve writes."
          >
            <input
              type="checkbox"
              checked={Boolean(autoApproveReads)}
              onChange={(e) => onAutoApproveReadsChange?.(e.target.checked)}
            />
            Auto-approve reads (read/list/search_repo)
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="Forces a human gate on each ask_ai step. Does not disable the orchestrator."
          >
            <input
              type="checkbox"
              checked={Boolean(requireAskAiApproval)}
              onChange={(e) => onRequireAskAiApprovalChange?.(e.target.checked)}
            />
            Require approval for each ask_ai step
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="Runs an extra reflection pass after steps. Does not undo completed writes."
          >
            <input
              type="checkbox"
              checked={Boolean(enableReflection)}
              onChange={(e) => onEnableReflectionChange?.(e.target.checked)}
            />
            Reflection pass after steps
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="Requires you to approve the full plan before any step runs. Does not replace per-step gates you leave enabled."
          >
            <input
              type="checkbox"
              checked={Boolean(requirePlanApproval)}
              onChange={(e) => onRequirePlanApprovalChange?.(e.target.checked)}
            />
            Approve full plan before running steps
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="One approval covers all queued writes in the plan. Does not skip read or ask_ai gates you enabled separately."
          >
            <input
              type="checkbox"
              checked={Boolean(batchApproveMutations)}
              onChange={(e) => onBatchApproveMutationsChange?.(e.target.checked)}
            />
            One approval for all writes in plan (mutation queue)
          </label>
          <label
            className="flex items-center gap-2 cursor-pointer"
            title="Snapshots files before writes when the main process supports it. Does not replace backups outside the IDE."
          >
            <input
              type="checkbox"
              checked={enableRollbackSnapshots !== false}
              onChange={(e) => onEnableRollbackSnapshotsChange?.(e.target.checked)}
            />
            Rollback snapshots before writes
          </label>
        </div>
        {taskOptions.length > 0 && (
          <div className="flex gap-2 items-center mb-2">
            <span className="text-[10px] text-gray-500 shrink-0">Tasks</span>
            <select
              value={taskId || ''}
              onChange={(e) => onSelectTask?.(e.target.value || null)}
              className="flex-1 min-w-0 bg-ide-bg border border-gray-600 rounded px-2 py-1 text-xs"
              title="Switch active task view. Does not start a new task — use Run with a goal."
              aria-label="Select agent task to inspect"
            >
              <option value="">— New / none —</option>
              {taskOptions.map((t) => (
                <option key={t.id} value={t.id}>
                  {formatTaskLabel(t)}
                </option>
              ))}
            </select>
            <button
              type="button"
              className="text-[10px] px-2 py-1 rounded bg-gray-700 hover:bg-gray-600 shrink-0"
              title="agent:list-tasks IPC. No new rows: main-process log."
              onClick={() => {
                noisyLog('[agent]', 'refresh task list');
                setStatusLine('agent: refreshing tasks…');
                onRefreshTaskList?.();
              }}
            >
              ↻
            </button>
          </div>
        )}
        <div className="flex gap-2">
          <input
            id="rawrxd-agent-goal"
            type="text"
            value={goal}
            onChange={(e) => setGoal(e.target.value)}
            placeholder="Goal: e.g. add README section…"
            className={`flex-1 min-w-0 bg-ide-bg border border-gray-600 rounded px-2 py-1.5 text-sm ${focusVisibleRing}`}
            onKeyDown={(e) => e.key === 'Enter' && handleStart()}
            aria-label="Agent goal"
            title="Goal text is not persisted after Run — copy it elsewhere if you need to keep it."
          />
          <button
            type="button"
            onClick={handleStart}
            disabled={!goal.trim()}
            title="Starts a task via the main-process orchestrator. Mutating steps still honor approvals below."
            className={`px-3 py-1.5 rounded bg-ide-accent hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed text-sm shrink-0 ${focusVisibleRing}`}
          >
            Run
          </button>
        </div>
        {taskId && (
          <button
            type="button"
            onClick={() => {
              noisyLog('[agent]', 'cancel requested', taskId);
              setStatusLine('agent: cancel requested');
              onCancelTask?.(taskId);
            }}
            className="mt-2 text-xs text-red-400 hover:text-red-300"
            title="Asks the orchestrator to cancel this task. In-flight steps may still finish briefly."
          >
            Cancel task
          </button>
        )}
      </div>

      <div className="flex-1 overflow-auto p-3 text-sm min-h-0">
        {taskId && (
          <>
            {taskGoal && (
              <div className="mb-2 text-xs text-gray-400">
                <span className="text-gray-500">Goal: </span>
                {taskGoal}
              </div>
            )}
            <div className="mb-2 flex flex-wrap items-center gap-2">
              <span className="text-gray-400">Status:</span>
              <span className={statusClass}>{taskStatus || '—'}</span>
            </div>

            {awaiting && (
              <div
                className="mb-3 p-3 rounded-lg border border-amber-600/60 bg-amber-950/40"
                role="alert"
                aria-live="assertive"
              >
                <div className="text-amber-200 font-medium text-xs uppercase tracking-wide mb-1">
                  Approval required
                </div>
                <div className="text-xs text-gray-300 space-y-1">
                  <div>
                    <span className="text-gray-500">Action:</span> {pendingApproval.stepType}
                  </div>
                  {pendingApproval.path && (
                    <div>
                      <span className="text-gray-500">Path:</span> {pendingApproval.path}
                    </div>
                  )}
                  {pendingApproval.description && (
                    <div>
                      <span className="text-gray-500">Note:</span> {pendingApproval.description}
                    </div>
                  )}
                  {pendingApproval.summary && (
                    <pre className="mt-2 text-[10px] text-gray-400 whitespace-pre-wrap break-words max-h-32 overflow-y-auto bg-black/30 p-2 rounded">
                      {pendingApproval.summary}
                    </pre>
                  )}
                </div>
                <div className="flex gap-2 mt-3">
                  <button
                    type="button"
                    onClick={() => {
                      noisyLog('[agent]', 'approve', taskId);
                      setStatusLine('agent: step approved');
                      onApproveStep?.(taskId, true);
                    }}
                    className={`flex-1 py-1.5 rounded bg-emerald-700 hover:bg-emerald-600 text-white text-xs ${focusVisibleRing}`}
                    aria-label={`Approve agent step: ${pendingApproval.stepType || 'action'}`}
                  >
                    Approve
                  </button>
                  <button
                    type="button"
                    onClick={() => {
                      noisyLog('[agent]', 'deny', taskId);
                      setStatusLine('agent: step denied');
                      onApproveStep?.(taskId, false);
                    }}
                    className={`flex-1 py-1.5 rounded bg-gray-700 hover:bg-gray-600 text-white text-xs ${focusVisibleRing}`}
                    aria-label={`Deny or skip agent step: ${pendingApproval.stepType || 'action'}`}
                  >
                    Deny
                  </button>
                </div>
              </div>
            )}

            {steps && steps.length > 0 && (
              <ul className="space-y-2">
                {steps.map((step, i) => (
                  <li
                    key={i}
                    className={`border-l-2 pl-2 py-1 ${
                      step.risk === 'mutate'
                        ? 'border-rose-500/70'
                        : step.risk === 'read'
                        ? 'border-cyan-600/60'
                        : 'border-gray-600'
                    }`}
                  >
                    <div className="flex flex-wrap items-center gap-2">
                      <span className="text-gray-400">{step.type || 'step'}</span>
                      {step.risk && (
                        <span className="text-[9px] uppercase px-1 rounded bg-gray-800 text-gray-500">
                          {step.risk}
                        </span>
                      )}
                      {step.riskLevel && (
                        <span className="text-[9px] uppercase px-1 rounded bg-gray-800 text-amber-500/90">
                          {step.riskLevel}
                        </span>
                      )}
                      {step.status && (
                        <span className="text-xs text-gray-500">({step.status})</span>
                      )}
                    </div>
                    <div className="text-gray-200 mt-0.5">
                      {step.description || step.content || ''}
                    </div>
                    {step.error && (
                      <div className="mt-1 text-xs text-red-400 break-words">Error: {step.error}</div>
                    )}
                    {step.result != null && (
                      <pre className="mt-1 text-xs text-gray-400 whitespace-pre-wrap break-words max-h-40 overflow-y-auto">
                        {typeof step.result === 'string' ? step.result : JSON.stringify(step.result, null, 2)}
                      </pre>
                    )}
                  </li>
                ))}
              </ul>
            )}

            {taskLog && taskLog.length > 0 && (
              <details className="mt-4 text-xs">
                <summary className="cursor-pointer text-gray-500">Agent log</summary>
                <pre className="mt-2 text-[10px] text-gray-500 whitespace-pre-wrap break-words max-h-36 overflow-y-auto">
                  {taskLog.join('\n')}
                </pre>
              </details>
            )}
          </>
        )}
        {!taskId && (
          <div className="text-gray-500 text-sm space-y-2">
            <p>
              Idle: no task id. Run needs Electron + preload <code className="text-gray-400">startAgent</code>. Gates:
              Settings › Copilot (auto-approve writes / ask_ai approval).
            </p>
            {typeof window !== 'undefined' && typeof window.electronAPI?.startAgent !== 'function' ? (
              <p className="text-amber-500/95 text-xs border border-amber-700/40 rounded-lg px-2 py-1.5 bg-amber-950/25">
                Browser preview: no <code className="text-amber-200/90">startAgent</code> IPC. Agent runs in Electron with
                preload.
              </p>
            ) : null}
          </div>
        )}
        {taskId && (taskStatus === 'failed' || taskStatus === 'cancelled') && (
          <p className="text-gray-500 text-xs mt-2 border-t border-gray-800 pt-2">
            Task state: {taskStatus}. Step errors above; empty detail → main-process log.
          </p>
        )}
      </div>
      <MinimalSurfaceM814Footer
        surfaceId="agent"
        offlineHint="UI works offline; running tasks need the Electron main process."
        docPath="docs/AUTONOMOUS_AGENT_ELECTRON.md"
        m13Hint="Dev: main-process logs; Settings › Noise (verbose dev logs)."
        className="px-2 border-t border-gray-800/80"
      >
        <div className="flex flex-wrap gap-1 px-1 pb-1">
          <CopySupportLineButton
            getText={() =>
              `[agent] task=${taskId || 'none'} status=${taskStatus || 'idle'} auto_write=${Boolean(autoApproveWrites)}`
            }
            label="Copy support line"
            className={`text-[9px] px-1.5 py-0.5 rounded border border-gray-700 text-gray-400 hover:text-gray-200 ${focusVisibleRing}`}
            onCopied={() => {
              pushToast({ title: '[agent]', message: 'Support line copied.', variant: 'success', durationMs: 2000 });
              setStatusLine('[agent] support line copied');
            }}
            onFailed={() =>
              pushToast({ title: '[agent]', message: 'Clipboard unavailable.', variant: 'warn', durationMs: 2600 })
            }
          />
        </div>
      </MinimalSurfaceM814Footer>
    </div>
  );
};

export default AgentPanel;
