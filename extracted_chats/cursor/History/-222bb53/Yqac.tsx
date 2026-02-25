// BigDaddyGEngine/BigDaddyGIntegratedDashboard.tsx - Complete Multi-Agent Trace Analysis Dashboard
import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { TokenHeatmap } from './components/TokenHeatmap';
import { TraceComparisonPanel } from './components/TraceComparisonPanel';
import { OrchestrationGraph } from './components/OrchestrationGraph';
import { BigDaddyGBrowserPanel } from './components/BigDaddyGBrowserPanel';
import { useBigDaddyGEngine } from './useEngine';
import { useAnalytics, usePerformanceMonitor } from './hooks';
import { getAgentRegistry, getAgentStatuses } from './agent';
import { bigElderOrchestrate } from './orchestrate';

interface TraceToken {
  token: string;
  index: number;
  timestamp: number;
  logits?: number[];
  agentId?: string;
  version?: string;
  confidence?: number;
  entropy?: number;
}

interface DashboardState {
  activeTab: 'inference' | 'orchestration' | 'introspection' | 'comparison';
  selectedVersions: string[];
  visibleAgents: string[];
  tracesByVersion: Record<string, TraceToken[]>;
  selectedToken: { token: TraceToken; version: string } | null;
  orchestrationResult: any | null;
}

export function BigDaddyGIntegratedDashboard() {
  const [state, setState] = useState<DashboardState>({
    activeTab: 'inference',
    selectedVersions: ['v1'],
    visibleAgents: [],
    tracesByVersion: {},
    selectedToken: null,
    orchestrationResult: null
  });

  const { execute, stream, streamWithLogits, status, tokens, trace, currentModel, memoryUsage, latency } = useBigDaddyGEngine();
  const { logEvent, getEvents } = useAnalytics();
  const { metrics } = usePerformanceMonitor();

  // Initialize with sample traces
  useEffect(() => {
    const sampleTraces: Record<string, TraceToken[]> = {
      'v1': [
        { token: 'Hello', index: 0, timestamp: Date.now() - 1000, agentId: 'rawr', version: 'v1', confidence: 0.8, entropy: 0.2 },
        { token: ' world', index: 1, timestamp: Date.now() - 800, agentId: 'rawr', version: 'v1', confidence: 0.9, entropy: 0.1 },
        { token: '!', index: 2, timestamp: Date.now() - 600, agentId: 'rawr', version: 'v1', confidence: 0.95, entropy: 0.05 }
      ],
      'v2': [
        { token: 'Hi', index: 0, timestamp: Date.now() - 1000, agentId: 'browser', version: 'v2', confidence: 0.7, entropy: 0.3 },
        { token: ' there', index: 1, timestamp: Date.now() - 800, agentId: 'browser', version: 'v2', confidence: 0.85, entropy: 0.15 },
        { token: '!', index: 2, timestamp: Date.now() - 600, agentId: 'browser', version: 'v2', confidence: 0.9, entropy: 0.1 }
      ]
    };
    setState(prev => ({ ...prev, tracesByVersion: sampleTraces }));
  }, []);

  // Live trace streaming simulation
  useEffect(() => {
    const interval = setInterval(() => {
      if (status === 'running') {
        const newToken: TraceToken = {
          token: String.fromCharCode(65 + Math.floor(Math.random() * 26)),
          index: trace.length,
          timestamp: Date.now(),
          agentId: ['rawr', 'browser', 'parser', 'optimizer'][Math.floor(Math.random() * 4)],
          version: 'live',
          confidence: Math.random(),
          entropy: Math.random() * 0.5
        };
        
        setState(prev => ({
          ...prev,
          tracesByVersion: {
            ...prev.tracesByVersion,
            live: [...(prev.tracesByVersion.live || []), newToken]
          }
        }));
      }
    }, 1000);

    return () => clearInterval(interval);
  }, [status, trace.length]);

  const handleTabChange = useCallback((tab: DashboardState['activeTab']) => {
    setState(prev => ({ ...prev, activeTab: tab }));
    logEvent('tab_switch', { tab });
  }, [logEvent]);

  const handleVersionToggle = useCallback((version: string) => {
    setState(prev => ({
      ...prev,
      selectedVersions: prev.selectedVersions.includes(version)
        ? prev.selectedVersions.filter(v => v !== version)
        : [...prev.selectedVersions, version]
    }));
    logEvent('version_toggle', { version });
  }, [logEvent]);

  const handleAgentToggle = useCallback((agentId: string) => {
    setState(prev => ({
      ...prev,
      visibleAgents: prev.visibleAgents.includes(agentId)
        ? prev.visibleAgents.filter(a => a !== agentId)
        : [...prev.visibleAgents, agentId]
    }));
    logEvent('agent_toggle', { agentId });
  }, [logEvent]);

  const handleTokenSelect = useCallback((token: TraceToken, version: string) => {
    setState(prev => ({ ...prev, selectedToken: { token, version } }));
    logEvent('token_select', { token: token.token, version, agentId: token.agentId });
  }, [logEvent]);

  const handleRunOrchestration = useCallback(async () => {
    const task = prompt('Enter orchestration task:') || 'Analyze and optimize code';
    if (task) {
      try {
        const result = await bigElderOrchestrate(task);
        setState(prev => ({ ...prev, orchestrationResult: result }));
        logEvent('orchestration_complete', { task, result });
      } catch (error) {
        console.error('Orchestration failed:', error);
        logEvent('orchestration_error', { task, error: error.message });
      }
    }
  }, [logEvent]);

  const filteredTraces = useMemo(() => {
    const filtered: Record<string, TraceToken[]> = {};
    Object.entries(state.tracesByVersion).forEach(([version, tokens]) => {
      if (state.visibleAgents.length === 0) {
        filtered[version] = tokens;
      } else {
        filtered[version] = tokens.filter(token => 
          state.visibleAgents.includes(token.agentId || '')
        );
      }
    });
    return filtered;
  }, [state.tracesByVersion, state.visibleAgents]);

  const agentStatuses = useMemo(() => {
    return getAgentStatuses();
  }, []);

  const uniqueAgents = useMemo(() => {
    const agents = new Set<string>();
    Object.values(state.tracesByVersion).forEach(tokens => {
      tokens.forEach(token => {
        if (token.agentId) agents.add(token.agentId);
      });
    });
    return Array.from(agents);
  }, [state.tracesByVersion]);

  return (
    <div className="bigdaddyg-integrated-dashboard">
      <header className="dashboard-header">
        <div className="header-content">
          <h1>BigDaddyG Integrated Dashboard</h1>
          <div className="header-subtitle">
            Multi-Agent Trace Analysis & Orchestration
          </div>
        </div>
        
        <div className="header-controls">
          <div className="status-indicators">
            <span className={`status ${status}`}>
              {status === 'idle' ? '🟢 Ready' : 
               status === 'loading' ? '🟡 Loading' : 
               status === 'running' ? '🔵 Running' : 
               '🔴 Error'}
            </span>
            <span className="model-status">
              Model: {currentModel || 'None'}
            </span>
            <span className="memory-status">
              Memory: {Math.round(memoryUsage / 1024 / 1024)}MB
            </span>
          </div>
        </div>
      </header>

      <nav className="dashboard-nav">
        <div className="nav-tabs">
          <button 
            className={`nav-tab ${state.activeTab === 'inference' ? 'active' : ''}`}
            onClick={() => handleTabChange('inference')}
          >
            🧠 Inference Engine
          </button>
          <button 
            className={`nav-tab ${state.activeTab === 'orchestration' ? 'active' : ''}`}
            onClick={() => handleTabChange('orchestration')}
          >
            🎭 Agent Orchestration
          </button>
          <button 
            className={`nav-tab ${state.activeTab === 'introspection' ? 'active' : ''}`}
            onClick={() => handleTabChange('introspection')}
          >
            🔍 Token Introspection
          </button>
          <button 
            className={`nav-tab ${state.activeTab === 'comparison' ? 'active' : ''}`}
            onClick={() => handleTabChange('comparison')}
          >
            📊 Trace Comparison
          </button>
        </div>
        
        <div className="nav-actions">
          <div className="agent-filters">
            <span className="filter-label">Agents:</span>
            {uniqueAgents.map(agentId => (
              <button
                key={agentId}
                className={`agent-filter ${state.visibleAgents.includes(agentId) ? 'active' : ''}`}
                onClick={() => handleAgentToggle(agentId)}
              >
                {agentId}
              </button>
            ))}
          </div>
          
          <div className="version-selector">
            <span className="filter-label">Versions:</span>
            {Object.keys(state.tracesByVersion).map(version => (
              <button
                key={version}
                className={`version-filter ${state.selectedVersions.includes(version) ? 'active' : ''}`}
                onClick={() => handleVersionToggle(version)}
              >
                {version}
              </button>
            ))}
          </div>
        </div>
      </nav>

      <main className="dashboard-main">
        {state.activeTab === 'inference' && (
          <div className="inference-tab">
            <BigDaddyGBrowserPanel />
          </div>
        )}
        
        {state.activeTab === 'orchestration' && (
          <div className="orchestration-tab">
            <div className="orchestration-controls">
              <button 
                onClick={handleRunOrchestration}
                className="btn btn-primary"
              >
                Run Orchestration
              </button>
              {state.orchestrationResult && (
                <div className="orchestration-result">
                  <h3>Result:</h3>
                  <p>{state.orchestrationResult.output}</p>
                  <div className="result-metrics">
                    <span>Agents: {state.orchestrationResult.agentChain.join(' → ')}</span>
                    <span>Latency: {state.orchestrationResult.totalLatency.toFixed(2)}ms</span>
                  </div>
                </div>
              )}
            </div>
            <OrchestrationGraph />
          </div>
        )}
        
        {state.activeTab === 'introspection' && (
          <div className="introspection-tab">
            <div className="introspection-controls">
              <div className="trace-selector">
                <label>Active Trace:</label>
                <select 
                  value={state.selectedVersions[0] || 'v1'} 
                  onChange={(e) => setState(prev => ({ ...prev, selectedVersions: [e.target.value] }))}
                >
                  {Object.keys(state.tracesByVersion).map(version => (
                    <option key={version} value={version}>
                      Version {version} ({state.tracesByVersion[version]?.length || 0} tokens)
                    </option>
                  ))}
                </select>
              </div>
              
              <div className="introspection-metrics">
                <span>Tokens: {filteredTraces[state.selectedVersions[0]]?.length || 0}</span>
                <span>Agents: {uniqueAgents.length}</span>
                <span>Memory: {Math.round(memoryUsage / 1024 / 1024)}MB</span>
              </div>
            </div>
            
            <TokenHeatmap 
              trace={filteredTraces[state.selectedVersions[0]] || []}
              showLogits={true}
              colorScheme="rainbow"
              size="medium"
            />
          </div>
        )}
        
        {state.activeTab === 'comparison' && (
          <div className="comparison-tab">
            <TraceComparisonPanel
              tracesByVersion={filteredTraces}
              selectedVersions={state.selectedVersions}
              onVersionToggle={handleVersionToggle}
              onTokenSelect={handleTokenSelect}
            />
          </div>
        )}
      </main>

      <footer className="dashboard-footer">
        <div className="footer-content">
          <div className="footer-section">
            <strong>BigDaddyG Integrated Dashboard v2.0</strong>
            <span>• Multi-Agent Trace Analysis • Real-time Introspection • Version Comparison</span>
          </div>
          
          <div className="footer-section">
            <span>Events: {getEvents().length}</span>
            <span>• Agents: {agentStatuses.length}</span>
            <span>• Traces: {Object.keys(state.tracesByVersion).length}</span>
            <span>• Uptime: {Math.round(performance.now() / 1000)}s</span>
          </div>
        </div>
      </footer>

      <style jsx>{`
        .bigdaddyg-integrated-dashboard {
          display: flex;
          flex-direction: column;
          height: 100vh;
          background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%);
          color: #00ff00;
          font-family: 'Courier New', monospace;
          overflow: hidden;
        }

        .dashboard-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px 30px;
          background: rgba(0, 255, 0, 0.05);
          border-bottom: 2px solid #00ff00;
        }

        .header-content h1 {
          margin: 0;
          font-size: 28px;
          text-shadow: 0 0 15px #00ff00;
        }

        .header-subtitle {
          font-size: 14px;
          color: #00aa00;
          margin-top: 5px;
        }

        .header-controls {
          display: flex;
          gap: 20px;
          align-items: center;
        }

        .status-indicators {
          display: flex;
          gap: 15px;
          align-items: center;
        }

        .status {
          padding: 5px 10px;
          border-radius: 5px;
          font-size: 12px;
          font-weight: bold;
        }

        .status.idle { background: rgba(0, 255, 0, 0.2); }
        .status.loading { background: rgba(255, 255, 0, 0.2); }
        .status.running { background: rgba(0, 150, 255, 0.2); }
        .status.error { background: rgba(255, 0, 0, 0.2); }

        .model-status, .memory-status {
          font-size: 12px;
          color: #00aa00;
        }

        .dashboard-nav {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 10px 30px;
          background: rgba(0, 0, 0, 0.3);
          border-bottom: 1px solid #00ff00;
        }

        .nav-tabs {
          display: flex;
          gap: 5px;
        }

        .nav-tab {
          padding: 10px 20px;
          background: transparent;
          border: 1px solid #00ff00;
          border-radius: 5px 5px 0 0;
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-weight: bold;
          transition: all 0.3s;
        }

        .nav-tab:hover {
          background: rgba(0, 255, 0, 0.1);
        }

        .nav-tab.active {
          background: rgba(0, 255, 0, 0.2);
          border-bottom-color: transparent;
        }

        .nav-actions {
          display: flex;
          gap: 20px;
          align-items: center;
        }

        .agent-filters, .version-selector {
          display: flex;
          gap: 5px;
          align-items: center;
        }

        .filter-label {
          font-size: 12px;
          color: #00aa00;
          margin-right: 5px;
        }

        .agent-filter, .version-filter {
          padding: 3px 8px;
          background: rgba(0, 255, 0, 0.1);
          border: 1px solid #00ff00;
          border-radius: 3px;
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-size: 11px;
          transition: all 0.3s;
        }

        .agent-filter:hover, .version-filter:hover {
          background: rgba(0, 255, 0, 0.2);
        }

        .agent-filter.active, .version-filter.active {
          background: rgba(0, 255, 0, 0.3);
          box-shadow: 0 0 5px rgba(0, 255, 0, 0.5);
        }

        .dashboard-main {
          flex: 1;
          display: flex;
          flex-direction: column;
          overflow: hidden;
        }

        .inference-tab, .orchestration-tab, .introspection-tab, .comparison-tab {
          flex: 1;
          display: flex;
          flex-direction: column;
          overflow: hidden;
        }

        .orchestration-controls {
          padding: 15px 20px;
          background: rgba(0, 0, 0, 0.2);
          border-bottom: 1px solid #00ff00;
        }

        .btn {
          padding: 8px 16px;
          background: rgba(0, 255, 0, 0.1);
          border: 1px solid #00ff00;
          border-radius: 5px;
          color: #00ff00;
          cursor: pointer;
          font-family: inherit;
          font-weight: bold;
          transition: all 0.3s;
        }

        .btn:hover {
          background: rgba(0, 255, 0, 0.2);
          box-shadow: 0 0 10px rgba(0, 255, 0, 0.5);
        }

        .btn-primary {
          background: rgba(0, 255, 0, 0.2);
        }

        .orchestration-result {
          margin-top: 15px;
          padding: 10px;
          background: rgba(0, 0, 0, 0.3);
          border: 1px solid #00ff00;
          border-radius: 5px;
        }

        .orchestration-result h3 {
          margin: 0 0 10px 0;
          color: #00ff00;
        }

        .result-metrics {
          display: flex;
          gap: 15px;
          font-size: 12px;
          color: #00aa00;
        }

        .introspection-controls {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 15px 20px;
          background: rgba(0, 0, 0, 0.2);
          border-bottom: 1px solid #00ff00;
        }

        .trace-selector {
          display: flex;
          align-items: center;
          gap: 10px;
        }

        .trace-selector label {
          font-size: 12px;
          color: #00aa00;
        }

        .trace-selector select {
          background: rgba(0, 255, 0, 0.05);
          border: 1px solid #00ff00;
          border-radius: 3px;
          padding: 5px 8px;
          color: #00ff00;
          font-family: inherit;
          font-size: 12px;
        }

        .introspection-metrics {
          display: flex;
          gap: 15px;
          font-size: 12px;
          color: #00aa00;
        }

        .dashboard-footer {
          padding: 10px 30px;
          background: rgba(0, 0, 0, 0.5);
          border-top: 1px solid #00ff00;
        }

        .footer-content {
          display: flex;
          justify-content: space-between;
          align-items: center;
          font-size: 12px;
        }

        .footer-section {
          display: flex;
          gap: 15px;
          align-items: center;
        }

        .footer-section span {
          color: #00aa00;
        }

        /* Responsive design */
        @media (max-width: 768px) {
          .dashboard-header {
            flex-direction: column;
            gap: 15px;
            text-align: center;
          }

          .dashboard-nav {
            flex-direction: column;
            gap: 10px;
          }

          .nav-tabs {
            width: 100%;
            justify-content: center;
          }

          .nav-actions {
            width: 100%;
            justify-content: center;
            flex-wrap: wrap;
          }

          .footer-content {
            flex-direction: column;
            gap: 10px;
            text-align: center;
          }
        }
      `}</style>
    </div>
  );
}
