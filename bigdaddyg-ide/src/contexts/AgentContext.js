import React, { createContext, useContext, useState, useCallback } from 'react';

const AgentContext = createContext(null);

export function AgentProvider({ children }) {
  const [activeTaskId, setActiveTaskId] = useState(null);
  const [taskStatus, setTaskStatus] = useState(null);
  const [steps, setSteps] = useState([]);

  const startAgent = useCallback(async (goal) => {
    const api = window.electronAPI;
    if (!api?.startAgent) return;
    const result = await api.startAgent(goal);
    if (result?.success && result.data) {
      setActiveTaskId(result.data);
      setTaskStatus('planning');
      setSteps([]);
    }
  }, []);

  const refreshStatus = useCallback(async (taskId) => {
    const api = window.electronAPI;
    if (!api?.getAgentStatus || !taskId) return;
    const result = await api.getAgentStatus(taskId);
    if (result?.success && result.data) {
      const task = result.data;
      setTaskStatus(task.status);
      setSteps(task.steps || []);
    }
  }, []);

  const value = {
    activeTaskId,
    taskStatus,
    steps,
    startAgent,
    refreshStatus
  };

  return <AgentContext.Provider value={value}>{children}</AgentContext.Provider>;
}

export function useAgent() {
  const ctx = useContext(AgentContext);
  if (!ctx) throw new Error('useAgent must be used within AgentProvider');
  return ctx;
}

export default AgentContext;
