import React, { useState, useEffect } from 'react';

interface InsightsPanelProps {
  orchestrationResult?: any;
  systemStatus?: any;
  className?: string;
}

interface InsightMetric {
  label: string;
  value: string | number;
  trend?: 'up' | 'down' | 'stable';
  color?: string;
  icon?: string;
}

export function InsightsPanel({ 
  orchestrationResult, 
  systemStatus,
  className = '' 
}: InsightsPanelProps) {
  const [insights, setInsights] = useState<InsightMetric[]>([]);
  const [performanceHistory, setPerformanceHistory] = useState<number[]>([]);
  const [agentActivity, setAgentActivity] = useState<Record<string, number>>({});

  // Generate insights from orchestration result
  useEffect(() => {
    if (!orchestrationResult) {
      setInsights([]);
      return;
    }

    const newInsights: InsightMetric[] = [
      {
        label: 'Total Tokens',
        value: orchestrationResult.tokenCount?.toLocaleString() || '0',
        trend: 'up',
        color: '#3b82f6',
        icon: '📊'
      },
      {
        label: 'Processing Time',
        value: orchestrationResult.processingTime ? `${orchestrationResult.processingTime.toFixed(0)}ms` : 'N/A',
        trend: 'stable',
        color: '#10b981',
        icon: '⏱️'
      },
      {
        label: 'Agent Count',
        value: orchestrationResult.agents?.length || 0,
        trend: 'stable',
        color: '#8b5cf6',
        icon: '🤖'
      },
      {
        label: 'Success Rate',
        value: '95%',
        trend: 'up',
        color: '#059669',
        icon: '✅'
      },
      {
        label: 'Memory Usage',
        value: systemStatus?.memoryUsage ? `${systemStatus.memoryUsage.toFixed(1)}MB` : 'N/A',
        trend: 'stable',
        color: '#dc2626',
        icon: '💾'
      },
      {
        label: 'Context Window',
        value: systemStatus?.contextWindow ? `${systemStatus.contextWindow.toLocaleString()}` : 'N/A',
        trend: 'stable',
        color: '#ea580c',
        icon: '🪟'
      }
    ];

    setInsights(newInsights);
  }, [orchestrationResult, systemStatus]);

  // Simulate performance history
  useEffect(() => {
    const interval = setInterval(() => {
      setPerformanceHistory(prev => {
        const newValue = Math.random() * 100;
        return [...prev.slice(-19), newValue];
      });
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  // Simulate agent activity
  useEffect(() => {
    const agents = ['rawr', 'browser', 'parser', 'optimizer', 'codegen', 'debugger'];
    const activity: Record<string, number> = {};
    
    agents.forEach(agent => {
      activity[agent] = Math.random() * 100;
    });
    
    setAgentActivity(activity);
  }, []);

  const getTrendIcon = (trend?: string) => {
    switch (trend) {
      case 'up': return '📈';
      case 'down': return '📉';
      case 'stable': return '➡️';
      default: return '📊';
    }
  };

  const getTrendColor = (trend?: string) => {
    switch (trend) {
      case 'up': return '#10b981';
      case 'down': return '#ef4444';
      case 'stable': return '#6b7280';
      default: return '#6b7280';
    }
  };

  return (
    <div className={`insights-panel ${className}`}>
      <div className="insights-header">
        <h2>📊 System Insights</h2>
        <div className="insights-controls">
          <button className="insight-btn">🔄 Refresh</button>
          <button className="insight-btn">📈 Export</button>
        </div>
      </div>

      {/* Key Metrics */}
      <div className="insights-section">
        <h3>🎯 Key Metrics</h3>
        <div className="metrics-grid">
          {insights.map((insight, index) => (
            <div key={index} className="metric-card">
              <div className="metric-header">
                <span className="metric-icon">{insight.icon}</span>
                <span className="metric-label">{insight.label}</span>
                <span 
                  className="metric-trend"
                  style={{ color: getTrendColor(insight.trend) }}
                >
                  {getTrendIcon(insight.trend)}
                </span>
              </div>
              <div className="metric-value" style={{ color: insight.color }}>
                {insight.value}
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Performance Chart */}
      <div className="insights-section">
        <h3>📈 Performance Trends</h3>
        <div className="performance-chart">
          <div className="chart-container">
            <div className="chart-header">
              <span>System Performance (%)</span>
              <span className="current-value">
                {performanceHistory[performanceHistory.length - 1]?.toFixed(1) || '0'}%
              </span>
            </div>
            <div className="chart-area">
              <svg width="100%" height="120" className="performance-svg">
                <polyline
                  points={performanceHistory.map((value, index) => 
                    `${(index / (performanceHistory.length - 1)) * 100},${100 - value}`
                  ).join(' ')}
                  fill="none"
                  stroke="#3b82f6"
                  strokeWidth="2"
                />
                {performanceHistory.map((value, index) => (
                  <circle
                    key={index}
                    cx={`${(index / (performanceHistory.length - 1)) * 100}%`}
                    cy={`${100 - value}%`}
                    r="3"
                    fill="#3b82f6"
                  />
                ))}
              </svg>
            </div>
          </div>
        </div>
      </div>

      {/* Agent Activity */}
      <div className="insights-section">
        <h3>🤖 Agent Activity</h3>
        <div className="agent-activity">
          {Object.entries(agentActivity).map(([agent, activity]) => (
            <div key={agent} className="agent-activity-item">
              <div className="agent-info">
                <span className="agent-name">{agent}</span>
                <span className="agent-activity-value">{activity.toFixed(1)}%</span>
              </div>
              <div className="activity-bar">
                <div 
                  className="activity-fill"
                  style={{ 
                    width: `${activity}%`,
                    backgroundColor: activity > 80 ? '#10b981' : 
                                   activity > 50 ? '#f59e0b' : '#ef4444'
                  }}
                ></div>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* System Health */}
      <div className="insights-section">
        <h3>🏥 System Health</h3>
        <div className="health-metrics">
          <div className="health-item">
            <div className="health-label">
              <span className="health-icon">💾</span>
              <span>Memory Usage</span>
            </div>
            <div className="health-value">
              <div className="health-bar">
                <div 
                  className="health-fill"
                  style={{ 
                    width: `${(systemStatus?.memoryUsage || 0) / 512 * 100}%`,
                    backgroundColor: (systemStatus?.memoryUsage || 0) > 400 ? '#ef4444' : '#10b981'
                  }}
                ></div>
              </div>
              <span className="health-text">
                {systemStatus?.memoryUsage?.toFixed(1) || '0'}MB / 512MB
              </span>
            </div>
          </div>

          <div className="health-item">
            <div className="health-label">
              <span className="health-icon">🪟</span>
              <span>Context Window</span>
            </div>
            <div className="health-value">
              <div className="health-bar">
                <div 
                  className="health-fill"
                  style={{ 
                    width: `${(systemStatus?.contextWindow || 0) / 512000 * 100}%`,
                    backgroundColor: '#3b82f6'
                  }}
                ></div>
              </div>
              <span className="health-text">
                {systemStatus?.contextWindow?.toLocaleString() || '0'} / 512k tokens
              </span>
            </div>
          </div>

          <div className="health-item">
            <div className="health-label">
              <span className="health-icon">⚡</span>
              <span>Performance Score</span>
            </div>
            <div className="health-value">
              <div className="health-bar">
                <div 
                  className="health-fill"
                  style={{ 
                    width: `${(systemStatus?.performance || 0) * 100}%`,
                    backgroundColor: (systemStatus?.performance || 0) > 0.8 ? '#10b981' : 
                                   (systemStatus?.performance || 0) > 0.6 ? '#f59e0b' : '#ef4444'
                  }}
                ></div>
              </div>
              <span className="health-text">
                {((systemStatus?.performance || 0) * 100).toFixed(1)}%
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* Recommendations */}
      <div className="insights-section">
        <h3>💡 Recommendations</h3>
        <div className="recommendations">
          {systemStatus?.memoryUsage > 400 && (
            <div className="recommendation warning">
              <span className="rec-icon">⚠️</span>
              <span>High memory usage detected. Consider reducing context window or enabling compression.</span>
            </div>
          )}
          
          {systemStatus?.performance < 0.6 && (
            <div className="recommendation warning">
              <span className="rec-icon">🐌</span>
              <span>Performance is below optimal. Check for resource constraints or agent conflicts.</span>
            </div>
          )}
          
          {systemStatus?.contextWindow > 128000 && (
            <div className="recommendation info">
              <span className="rec-icon">💡</span>
              <span>Large context window active. Monitor memory usage for optimal performance.</span>
            </div>
          )}
          
          {(!systemStatus?.memoryUsage || systemStatus.memoryUsage < 200) && 
           (!systemStatus?.performance || systemStatus.performance > 0.8) && (
            <div className="recommendation success">
              <span className="rec-icon">✅</span>
              <span>System is running optimally. All metrics are within healthy ranges.</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
