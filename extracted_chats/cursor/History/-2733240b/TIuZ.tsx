// BigDaddyGEngine/components/MeshVisualization.tsx - Visualization for Self-Optimizing Agent Mesh
import React, { useMemo } from 'react';
import { MeshHealth, MeshTopology } from '../optimization/SelfOptimizingAgentMesh';

export interface MeshVisualizationProps {
  health: MeshHealth | null;
  topology: MeshTopology | null;
  isLoading?: boolean;
}

export function MeshVisualization({ health, topology, isLoading }: MeshVisualizationProps) {
  const getHealthColor = (healthStatus: string) => {
    switch (healthStatus) {
      case 'excellent': return '#10b981'; // green
      case 'good': return '#3b82f6'; // blue
      case 'fair': return '#f59e0b'; // yellow
      case 'poor': return '#ef4444'; // red
      case 'critical': return '#dc2626'; // dark red
      default: return '#6b7280'; // gray
    }
  };

  const stats = useMemo(() => {
    if (!health) return null;

    const totalAgents = health.activeAgents + health.idleAgents + health.degradedAgents;
    return {
      totalAgents,
      activePercentage: totalAgents > 0 ? (health.activeAgents / totalAgents) * 100 : 0,
      degradedPercentage: totalAgents > 0 ? (health.degradedAgents / totalAgents) * 100 : 0,
      optimizationPercentage: health.optimizationScore * 100,
      averageLoad: health.averageLoad * 100
    };
  }, [health]);

  if (isLoading) {
    return (
      <div style={{ padding: '20px', textAlign: 'center' }}>
        <div className="loading-spinner">Loading mesh visualization...</div>
      </div>
    );
  }

  return (
    <div className="mesh-visualization" style={{ padding: '20px' }}>
      <style>{`
        .mesh-visualization {
          background: linear-gradient(135deg, #1e1e1e 0%, #2d2d2d 100%);
          border-radius: 12px;
          color: #e0e0e0;
        }
        .health-header {
          display: flex;
          align-items: center;
          gap: 15px;
          margin-bottom: 24px;
          padding-bottom: 16px;
          border-bottom: 2px solid rgba(255, 255, 255, 0.1);
        }
        .health-indicator {
          width: 24px;
          height: 24px;
          border-radius: 50%;
          display: inline-block;
          box-shadow: 0 0 10px currentColor;
        }
        .health-status {
          font-size: 24px;
          font-weight: bold;
          text-transform: capitalize;
        }
        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 16px;
          margin-bottom: 24px;
        }
        .stat-card {
          background: rgba(255, 255, 255, 0.05);
          border-radius: 8px;
          padding: 16px;
          border: 1px solid rgba(255, 255, 255, 0.1);
          transition: all 0.3s ease;
        }
        .stat-card:hover {
          background: rgba(255, 255, 255, 0.08);
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
        }
        .stat-label {
          font-size: 12px;
          color: #a0a0a0;
          text-transform: uppercase;
          letter-spacing: 1px;
          margin-bottom: 8px;
        }
        .stat-value {
          font-size: 28px;
          font-weight: bold;
          color: #fff;
        }
        .stat-unit {
          font-size: 14px;
          color: #a0a0a0;
          margin-left: 4px;
        }
        .progress-bar {
          height: 8px;
          background: rgba(255, 255, 255, 0.1);
          border-radius: 4px;
          overflow: hidden;
          margin-top: 8px;
        }
        .progress-fill {
          height: 100%;
          background: linear-gradient(90deg, #3b82f6, #10b981);
          transition: width 0.3s ease;
        }
        .issues-list {
          margin-top: 20px;
        }
        .issue-item {
          padding: 12px;
          background: rgba(239, 68, 68, 0.1);
          border-left: 3px solid #ef4444;
          margin-bottom: 8px;
          border-radius: 4px;
          color: #ffc9c9;
        }
        .topology-preview {
          margin-top: 24px;
          padding: 16px;
          background: rgba(0, 0, 0, 0.2);
          border-radius: 8px;
        }
        .topology-title {
          font-size: 16px;
          font-weight: bold;
          margin-bottom: 12px;
          color: #fff;
        }
        .topology-grid {
          display: grid;
          grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
          gap: 12px;
        }
        .agent-node {
          padding: 12px;
          background: rgba(255, 255, 255, 0.05);
          border-radius: 6px;
          text-align: center;
          border: 2px solid transparent;
          transition: all 0.2s ease;
        }
        .agent-node:hover {
          border-color: #3b82f6;
          transform: scale(1.05);
        }
        .agent-node.active {
          border-color: #10b981;
          background: rgba(16, 185, 129, 0.1);
        }
        .agent-node.degraded {
          border-color: #ef4444;
          background: rgba(239, 68, 68, 0.1);
        }
        .agent-node.idle {
          border-color: #6b7280;
          background: rgba(107, 114, 128, 0.1);
        }
        .loading-spinner {
          color: #a0a0a0;
        }
      `}</style>

      {health && (
        <>
          <div className="health-header">
            <span 
              className="health-indicator" 
              style={{ 
                backgroundColor: getHealthColor(health.overallHealth),
                color: getHealthColor(health.overallHealth)
              }}
            />
            <div>
              <div className="health-status" style={{ color: getHealthColor(health.overallHealth) }}>
                {health.overallHealth}
              </div>
              <div style={{ fontSize: '12px', color: '#a0a0a0' }}>
                Mesh Health Status
              </div>
            </div>
          </div>

          {stats && (
            <div className="stats-grid">
              <div className="stat-card">
                <div className="stat-label">Optimization Score</div>
                <div className="stat-value">
                  {stats.optimizationPercentage.toFixed(1)}
                  <span className="stat-unit">%</span>
                </div>
                <div className="progress-bar">
                  <div 
                    className="progress-fill" 
                    style={{ width: `${stats.optimizationPercentage}%` }}
                  />
                </div>
              </div>

              <div className="stat-card">
                <div className="stat-label">Active Agents</div>
                <div className="stat-value">
                  {health.activeAgents}
                  <span className="stat-unit">/{stats.totalAgents}</span>
                </div>
                <div className="progress-bar">
                  <div 
                    className="progress-fill" 
                    style={{ width: `${stats.activePercentage}%` }}
                  />
                </div>
              </div>

              <div className="stat-card">
                <div className="stat-label">Average Load</div>
                <div className="stat-value">
                  {stats.averageLoad.toFixed(1)}
                  <span className="stat-unit">%</span>
                </div>
                <div className="progress-bar">
                  <div 
                    className="progress-fill" 
                    style={{ 
                      width: `${stats.averageLoad}%`,
                      background: stats.averageLoad > 80 
                        ? 'linear-gradient(90deg, #ef4444, #dc2626)' 
                        : 'linear-gradient(90deg, #3b82f6, #10b981)'
                    }}
                  />
                </div>
              </div>

              <div className="stat-card">
                <div className="stat-label">Idle Agents</div>
                <div className="stat-value">
                  {health.idleAgents}
                  <span className="stat-unit">agents</span>
                </div>
              </div>

              <div className="stat-card">
                <div className="stat-label">Degraded Agents</div>
                <div className="stat-value" style={{ color: health.degradedAgents > 0 ? '#ef4444' : '#10b981' }}>
                  {health.degradedAgents}
                  <span className="stat-unit">agents</span>
                </div>
                {health.degradedAgents > 0 && (
                  <div className="progress-bar">
                    <div 
                      className="progress-fill" 
                      style={{ 
                        width: `${stats.degradedPercentage}%`,
                        background: 'linear-gradient(90deg, #ef4444, #dc2626)'
                      }}
                    />
                  </div>
                )}
              </div>
            </div>
          )}

          {health.issues.length > 0 && (
            <div className="issues-list">
              <div style={{ fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', color: '#ef4444' }}>
                ⚠️ Issues Detected:
              </div>
              {health.issues.map((issue, idx) => (
                <div key={idx} className="issue-item">
                  {issue}
                </div>
              ))}
            </div>
          )}

          {topology && topology.nodes.size > 0 && (
            <div className="topology-preview">
              <div className="topology-title">Agent Topology</div>
              <div className="topology-grid">
                {Array.from(topology.nodes.values()).map((node) => (
                  <div 
                    key={node.id} 
                    className={`agent-node ${node.status}`}
                    title={`${node.id}: ${node.status}`}
                  >
                    <div style={{ fontSize: '12px', color: '#a0a0a0' }}>{node.config.specialization}</div>
                    <div style={{ fontSize: '14px', fontWeight: 'bold', marginTop: '4px' }}>
                      {node.id}
                    </div>
                    <div style={{ fontSize: '10px', marginTop: '4px', color: '#666' }}>
                      Load: {(node.load * 100).toFixed(0)}%
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </>
      )}

      {!health && !isLoading && (
        <div style={{ textAlign: 'center', padding: '40px', color: '#666' }}>
          No mesh data available. Initialize the mesh to view topology and health metrics.
        </div>
      )}
    </div>
  );
}

export default MeshVisualization;
