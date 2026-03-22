/**
 * Renderer mirror of autonomous agent tasks (IPC to main).
 *
 * M01 — Does not: parse plans or execute steps — main-process orchestrator owns that; this context only reflects state + invokes IPC.
 * M03 — When IPC is missing, methods no-op; dev logs explain the next step (reload app / check preload).
 * M05 — Dev diagnostics use `[agent]` prefix.
 */
import React, { createContext, useContext, useState, useCallback, useEffect } from 'react';

const AgentContext = createContext(null);

function devAgentWarn(...args) {
  try {
    if (process.env.NODE_ENV === 'development' || window.electronAPI?.isDev) {
      // eslint-disable-next-line no-console
      console.warn('[agent]', ...args);
    }
  } catch {
    /* ignore */
  }
}

let warnedMissingGetAgentStatus = false;

/**
 * Holds active task id, steps snapshot, approvals UI state.
 * Does not: modify `approval_policy.json` or bypass gates — policy is enforced in main/orchestrator.
 */
export function AgentProvider({ children }) {
  const [activeTaskId, setActiveTaskId] = useState(null);
  const [taskStatus, setTaskStatus] = useState(null);
  const [steps, setSteps] = useState([]);
  const [pendingApproval, setPendingApproval] = useState(null);
  const [taskGoal, setTaskGoal] = useState(null);
  const [taskLog, setTaskLog] = useState([]);
  /** @type {{ active: object[], history: object[] }} */
  const [taskSnapshots, setTaskSnapshots] = useState({ active: [], history: [] });
  const [rollbackCount, setRollbackCount] = useState(0);

  const applyTaskSnapshot = useCallback((task) => {
    if (!task) return;
    setTaskStatus(task.status);
    setSteps(task.steps || []);
    setPendingApproval(task.pendingApproval || null);
    setTaskGoal(task.goal || null);
    setTaskLog(Array.isArray(task.log) ? task.log.slice(-80) : []);
    setRollbackCount(typeof task.rollbackCount === 'number' ? task.rollbackCount : 0);
  }, []);

  const refreshTaskList = useCallback(async () => {
    const api = window.electronAPI;
    if (!api?.listAgentTasks) {
      devAgentWarn('listAgentTasks unavailable — agent panel needs preload wiring.');
      return;
    }
    const result = await api.listAgentTasks();
    if (result?.success && result.data) {
      setTaskSnapshots({
        active: Array.isArray(result.data.active) ? result.data.active : [],
        history: Array.isArray(result.data.history) ? result.data.history : []
      });
    } else if (result && !result.success) {
      devAgentWarn('listAgentTasks failed:', result.error || 'unknown (main log).');
    }
  }, []);

  useEffect(() => {
    refreshTaskList();
    const t = setInterval(refreshTaskList, 5000);
    return () => clearInterval(t);
  }, [refreshTaskList]);

  /**
   * Starts a new agent task in main. Does not: queue chat messages or change active AI provider.
   */
  const startAgent = useCallback(
    async (goal, policy = {}) => {
      const api = window.electronAPI;
      if (!api?.startAgent) {
        devAgentWarn('startAgent unavailable (preload IPC).');
        return;
      }
      const result = await api.startAgent(goal, policy);
      if (result?.success && result.data) {
        setActiveTaskId(result.data);
        setTaskStatus('planning');
        setSteps([]);
        setPendingApproval(null);
        setTaskGoal(typeof goal === 'string' ? goal.trim() : '');
        setTaskLog([]);
        setRollbackCount(0);
        await refreshTaskList();
      } else if (result && !result.success) {
        devAgentWarn('startAgent failed:', result.error || 'unknown (main log).');
      }
    },
    [refreshTaskList]
  );

  /**
   * Pulls one task snapshot from main. Does not: approve or cancel — use `approveAgentStep` / `cancelAgentTask`.
   */
  const refreshStatus = useCallback(
    async (taskId) => {
      const api = window.electronAPI;
      if (!taskId) return;
      if (!api?.getAgentStatus) {
        if (!warnedMissingGetAgentStatus) {
          warnedMissingGetAgentStatus = true;
          devAgentWarn('getAgentStatus unavailable (preload); further polls silent.');
        }
        return;
      }
      const result = await api.getAgentStatus(taskId);
      if (result?.success && result.data) {
        applyTaskSnapshot(result.data);
      } else if (result && !result.success) {
        devAgentWarn('getAgentStatus failed:', taskId, result.error || 'unknown');
      }
    },
    [applyTaskSnapshot]
  );

  const approveAgentStep = useCallback(
    async (taskId, approved) => {
      const api = window.electronAPI;
      if (!api?.approveAgentStep || !taskId) return;
      await api.approveAgentStep(taskId, approved);
      await refreshStatus(taskId);
      await refreshTaskList();
    },
    [refreshStatus, refreshTaskList]
  );

  const cancelAgentTask = useCallback(
    async (taskId) => {
      const api = window.electronAPI;
      if (!api?.cancelAgentTask || !taskId) return;
      await api.cancelAgentTask(taskId);
      await refreshStatus(taskId);
      await refreshTaskList();
    },
    [refreshStatus, refreshTaskList]
  );

  const rollbackAgentMutations = useCallback(
    async (taskId) => {
      const api = window.electronAPI;
      if (!api?.rollbackAgentMutations || !taskId) return null;
      const result = await api.rollbackAgentMutations(taskId);
      await refreshStatus(taskId);
      await refreshTaskList();
      return result;
    },
    [refreshStatus, refreshTaskList]
  );

  const selectAgentTask = useCallback(
    async (taskId) => {
      if (!taskId) {
        setActiveTaskId(null);
        setTaskStatus(null);
        setSteps([]);
        setPendingApproval(null);
        setTaskGoal(null);
        setTaskLog([]);
        setRollbackCount(0);
        return;
      }
      setActiveTaskId(taskId);
      await refreshStatus(taskId);
    },
    [refreshStatus]
  );

  const value = {
    activeTaskId,
    taskStatus,
    steps,
    pendingApproval,
    taskGoal,
    taskLog,
    taskSnapshots,
    rollbackCount,
    startAgent,
    refreshStatus,
    refreshTaskList,
    approveAgentStep,
    cancelAgentTask,
    rollbackAgentMutations,
    selectAgentTask
  };

  return <AgentContext.Provider value={value}>{children}</AgentContext.Provider>;
}

/**
 * Agent task + approval UI hook.
 * Does not: apply IdeFeaturesContext toggles to the running task — main merges policy at `startAgent` time.
 */
export function useAgent() {
  const ctx = useContext(AgentContext);
  if (!ctx) throw new Error('useAgent must be used within AgentProvider');
  return ctx;
}

export default AgentContext;
