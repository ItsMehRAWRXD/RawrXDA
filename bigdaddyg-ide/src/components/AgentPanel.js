import React, { useState, useEffect } from 'react';

const AgentPanel = ({ onStartAgent, taskId, taskStatus, steps, onRefresh }) => {
  const [goal, setGoal] = useState('');

  useEffect(() => {
    if (!taskId || !onRefresh) return;
    const interval = setInterval(() => onRefresh(taskId), 2000);
    return () => clearInterval(interval);
  }, [taskId, onRefresh]);

  const handleStart = () => {
    const g = goal.trim();
    if (g && onStartAgent) onStartAgent(g);
    setGoal('');
  };

  return (
    <div className="flex flex-col h-full bg-ide-sidebar text-white">
      <div className="p-3 border-b border-gray-700">
        <h3 className="font-semibold mb-2">Agentic Task</h3>
        <div className="flex gap-2">
          <input
            type="text"
            value={goal}
            onChange={(e) => setGoal(e.target.value)}
            placeholder="Enter goal..."
            className="flex-1 bg-ide-bg border border-gray-600 rounded px-2 py-1.5 text-sm"
            onKeyDown={(e) => e.key === 'Enter' && handleStart()}
          />
          <button
            type="button"
            onClick={handleStart}
            disabled={!goal.trim()}
            className="px-3 py-1.5 rounded bg-ide-accent hover:bg-blue-600 disabled:opacity-50 disabled:cursor-not-allowed text-sm"
          >
            Start
          </button>
        </div>
      </div>
      <div className="flex-1 overflow-auto p-3 text-sm">
        {taskId && (
          <>
            <div className="mb-2">
              <span className="text-gray-400">Status: </span>
              <span
                className={
                  taskStatus === 'completed'
                    ? 'text-ide-success'
                    : taskStatus === 'failed'
                    ? 'text-ide-error'
                    : 'text-ide-warning'
                }
              >
                {taskStatus || '—'}
              </span>
            </div>
            {steps && steps.length > 0 && (
              <ul className="space-y-2">
                {steps.map((step, i) => (
                  <li key={i} className="border-l-2 border-gray-600 pl-2 py-1">
                    <span className="text-gray-400">{step.type || 'step'}: </span>
                    {step.description || step.content || JSON.stringify(step)}
                    {step.status && (
                      <span className="ml-2 text-xs text-gray-500">({step.status})</span>
                    )}
                  </li>
                ))}
              </ul>
            )}
          </>
        )}
        {!taskId && (
          <p className="text-gray-500">Start an agent task by entering a goal and clicking Start.</p>
        )}
      </div>
    </div>
  );
};

export default AgentPanel;
