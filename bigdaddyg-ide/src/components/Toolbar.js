import React, { useState, useEffect } from 'react';

const Toolbar = ({ onOpenProject, onToggleAgentPanel, title = 'BigDaddyG IDE' }) => {
  const [providers, setProviders] = useState([]);
  const [activeProvider, setActiveProvider] = useState('bigdaddyg');

  useEffect(() => {
    if (window.electronAPI?.listAIProviders) {
      window.electronAPI.listAIProviders().then((list) => {
        if (list && Array.isArray(list)) {
          setProviders(list);
          const active = list.find((p) => p.active && p.enabled) || list.find((p) => p.enabled);
          if (active) {
            setActiveProvider(active.id);
          }
        }
      });
    }
  }, []);

  const handleProviderChange = (e) => {
    const id = e.target.value;
    setActiveProvider(id);
    if (window.electronAPI?.setActiveAIProvider) {
      window.electronAPI.setActiveAIProvider(id);
    }
  };

  return (
    <div className="flex items-center justify-between px-3 py-2 bg-ide-toolbar border-b border-gray-700 text-sm">
      <div className="flex items-center gap-4">
        <span className="font-semibold text-white">{title}</span>
        <button
          type="button"
          onClick={onOpenProject}
          className="px-3 py-1 rounded bg-ide-accent hover:bg-blue-600 text-white"
        >
          Open project
        </button>
        <label className="flex items-center gap-2 text-gray-300">
          AI provider:
          <select
            value={activeProvider}
            onChange={handleProviderChange}
            className="bg-ide-sidebar text-white border border-gray-600 rounded px-2 py-1"
          >
            {providers.map((p) => (
              <option key={p.id} value={p.id} disabled={!p.enabled}>
                {p.name}{p.enabled ? '' : ' (disabled)'}
              </option>
            ))}
          </select>
        </label>
      </div>
      <button
        type="button"
        onClick={onToggleAgentPanel}
        className="px-3 py-1 rounded bg-ide-sidebar hover:bg-gray-600 text-white"
      >
        Toggle Agent
      </button>
    </div>
  );
};

export default Toolbar;
