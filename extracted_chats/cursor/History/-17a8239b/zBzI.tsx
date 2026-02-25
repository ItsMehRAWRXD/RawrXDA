// BigDaddyGEngine/components/MeshDashboard.tsx - Dashboard for Self-Optimizing Agent Mesh
import React, { useState } from 'react';
import { MeshVisualization } from './MeshVisualization';
import { useSelfOptimizingMesh } from '../hooks/useSelfOptimizingMesh';
import { AgentMetricsLogger } from '../optimization/AgentMetricsLogger';
import { ReinforcementLearner } from '../optimization/ReinforcementLearner';
import { MultiAgentOrchestrator } from '../MultiAgentOrchestrator';

export function MeshDashboard() {
  const [metricsLogger] = useState(() => new AgentMetricsLogger());
  const [learner] = useState(() => new ReinforcementLearner());
  const [orchestrator] = useState(() => new MultiAgentOrchestrator({}));

  const {
    health,
    topology,
    isInitialized,
    isLoading,
    error,
    optimize,
    refreshHealth,
    startSelfHealing,
    optimizationScore,
    activeAgentCount,
    degradedAgentCount
  } = useSelfOptimizingMesh(metricsLogger, learner, orchestrator, {
    enableLearning: true,
    enableAdaptiveRouting: true,
    enableSelfHealing: true
  }, {
    enableAutoRefresh: true,
    refreshInterval: 3000
  });

  return (
    <div style={{ padding: '24px' }}>
      <style>{`
        .mesh-dashboard {
          max-width: 1400px;
          margin: 0 auto;
        }
        .dashboard-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 24px;
          padding: 20px;
          background: linear-gradient(135deg, #1e1e1e 0%, #2d2d2d 100%);
          border-radius: 12px;
        }
        .header-title {
          font-size: 28px;
          font-weight: bold;
          color: #fff;
        }
        .header-subtitle {
          font-size: 14px;
          color: #a0a0a0;
          margin-top: 4px;
        }
        .control-panel {
          display: flex;
          gap: 12px;
          flex-wrap: wrap;
        }
        .control-button {
          padding: 10px 20px;
          background: linear-gradient(135deg, #3b82f6 0%, #2563eb 100%);
          border: none;
          border-radius: 8px;
          color: #fff;
          font-weight: 600;
          cursor: pointer;
          transition: all 0.3s ease;
          box-shadow: 0 2px 8px rgba(59, 130, 246, 0.3);
        }
        .control-button:hover {
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(59, 130, 246, 0.4);
        }
        .control-button:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
        .control-button.secondary {
          background: linear-gradient(135deg, #6b7280 0%, #4b5563 100%);
        }
        .control-button.success {
          background: linear-gradient(135deg, #10b981 0%, #059669 100%);
        }
        .control-button.danger {
          background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
        }
        .status-indicator {
          display: flex;
          align-items: center;
          gap: 8px;
          padding: 8px 16px;
          background: rgba(255, 255, 255, 0.1);
          border-radius: 20px;
        }
        .status-dot {
          width: 10px;
          height: 10px;
          border-radius: 50%;
        }
        .status-dot.initialized {
          background: #10b981;
          box-shadow: 0 0 10px #10b981;
        }
        .status-dot.loading {
          background: #f59e0b;
          animation: pulse 2s infinite;
        }
        .status-dot.error {
          background: #ef4444;
          box-shadow: 0 0 10px #ef4444;
        }
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
        .quick-stats {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 16px;
          margin-bottom: 24px;
        }
        .quick-stat {
          background: rgba(255, 255, 255, 0.05);
          padding: 16px;
          border-radius: 8px;
          border: 1px solid rgba(255, 255, 255, 0.1);
        }
        .quick-stat-label {
          font-size: 12px;
          color: #a0a0a0;
          text-transform: uppercase;
          letter-spacing: 1px;
        }
        .quick-stat-value {
          font-size: 24px;
          font-weight: bold;
          color: #fff;
          margin-top: 8px;
        }
        .error-message {
          padding: 16px;
          background: rgba(239, 68, 68, 0.1);
          border-left: 4px solid #ef4444;
          border-radius: 8px;
          color: #ffc9c9;
          margin-bottom: 24px;
        }
      `}</style>

      <div className="mesh-dashboard">
        <div className="dashboard-header">
          <div>
            <div className="header-title">Self-Optimizing Agent Mesh</div>
            <div className="header-subtitle">Real-time orchestration and adaptive intelligence</div>
          </div>
          <div className="status-indicator">
            <div className={`status-dot ${isInitialized ? 'initialized' : isLoading ? 'loading' : error ? 'error' : ''}`} />
            <span style={{ color: '#fff', fontSize: '14px' }}>
              {isInitialized ? 'Active' : isLoading ? 'Loading' : error ? 'Error' : 'Inactive'}
            </span>
          </div>
        </div>

        {error && (
          <div className="error-message">
            <strong>Error:</strong> {error.message}
          </div>
        )}

        <div className="quick-stats">
          <div className="quick-stat">
            <div className="quick-stat-label">Optimization Score</div>
            <div className="quick-stat-value" style={{ color: '#10b981' }}>
              {(optimizationScore * 100).toFixed(1)}%
            </div>
          </div>
          <div className="quick-stat">
            <div className="quick-stat-label">Active Agents</div>
            <div className="quick-stat-value" style={{ color: '#3b82f6' }}>
              {activeAgentCount}
            </div>
          </div>
          <div className="quick-stat">
            <div className="quick-stat-label">Degraded Agents</div>
            <div className="quick-stat-value" style={{ color: degradedAgentCount > 0 ? '#ef4444' : '#10b981' }}>
              {degradedAgentCount}
            </div>
          </div>
        </div>

        <div className="control-panel">
          <button 
            className="control-button" 
            onClick={() => optimize()}
            disabled={!isInitialized || isLoading}
          >
            🔄 Run Optimization
          </button>
          <button 
            className="control-button secondary" 
            onClick={() => refreshHealth()}
            disabled={!isInitialized}
          >
            🔍 Refresh Health
          </button>
          <button 
            className="control-button success" 
            onClick={() => startSelfHealing()}
            disabled={!isInitialized}
          >
            🏥 Start Self-Healing
          </button>
        </div>

        <MeshVisualization 
          health={health}
          topology={topology}
          isLoading={isLoading}
        />
      </div>
    </div>
  );
}

export default MeshDashboard;
