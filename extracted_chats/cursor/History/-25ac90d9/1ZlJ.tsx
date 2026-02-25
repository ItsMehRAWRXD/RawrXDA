import React, { useState, useEffect, useMemo } from 'react';

interface OrchestrationResult {
  id: string;
  timestamp: number;
  agents: string[];
  tokens: number;
  confidence: number;
  processingTime: number;
  contextWindow: number;
  memoryUsage: number;
  performance: number;
  success: boolean;
  error?: string;
}

interface SystemStatus {
  connected: boolean;
  streaming: boolean;
  contextWindow: number;
  memoryUsage: number;
  performance: number;
}

interface InsightsPanelProps {
  orchestrationResult?: OrchestrationResult | null;
  systemStatus: SystemStatus;
  className?: string;
}

interface MetricCard {
  title: string;
  value: string | number;
  trend?: 'up' | 'down' | 'stable';
  icon: string;
  color: string;
  description?: string;
}

interface AgentPerformance {
  id: string;
  name: string;
  successRate: number;
  avgProcessingTime: number;
  avgTokens: number;
  avgConfidence: number;
  totalExecutions: number;
}

export function InsightsPanel({
  orchestrationResult,
  systemStatus, 
  className = '' 
}: InsightsPanelProps) {
  const [activeTab, setActiveTab] = useState<'overview' | 'agents' | 'performance' | 'analytics'>('overview');
  const [timeRange, setTimeRange] = useState<'1h' | '24h' | '7d' | '30d'>('24h');
  const [showAdvanced, setShowAdvanced] = useState(false);

  // Mock data for demonstration - in real implementation, this would come from a data service
  const mockAgentPerformance: AgentPerformance[] = [
    {
      id: 'rawr',
      name: 'Rawr',
      successRate: 0.94,
      avgProcessingTime: 1250,
      avgTokens: 850,
      avgConfidence: 0.87,
      totalExecutions: 1247
    },
    {
      id: 'browser',
      name: 'Browser',
      successRate: 0.89,
      avgProcessingTime: 2100,
      avgTokens: 1200,
      avgConfidence: 0.82,
      totalExecutions: 892
    },
    {
      id: 'analyzer',
      name: 'Analyzer',
      successRate: 0.96,
      avgProcessingTime: 1800,
      avgTokens: 1100,
      avgConfidence: 0.91,
      totalExecutions: 1563
    },
    {
      id: 'optimizer',
      name: 'Optimizer',
      successRate: 0.92,
      avgProcessingTime: 2200,
      avgTokens: 1350,
      avgConfidence: 0.88,
      totalExecutions: 743
    }
  ];

  // Calculate metrics based on current data
  const metrics: MetricCard[] = useMemo(() => {
    const totalExecutions = mockAgentPerformance.reduce((sum, agent) => sum + agent.totalExecutions, 0);
    const avgSuccessRate = mockAgentPerformance.reduce((sum, agent) => sum + agent.successRate, 0) / mockAgentPerformance.length;
    const avgProcessingTime = mockAgentPerformance.reduce((sum, agent) => sum + agent.avgProcessingTime, 0) / mockAgentPerformance.length;
    const totalTokens = mockAgentPerformance.reduce((sum, agent) => sum + (agent.avgTokens * agent.totalExecutions), 0);

    return [
      {
        title: 'Total Executions',
        value: totalExecutions.toLocaleString(),
        trend: 'up',
        icon: '🚀',
        color: 'var(--fluent-primary)',
        description: 'Total agent executions in selected time range'
      },
      {
        title: 'Success Rate',
        value: `${(avgSuccessRate * 100).toFixed(1)}%`,
        trend: avgSuccessRate > 0.9 ? 'up' : 'stable',
        icon: '✅',
        color: 'var(--fluent-success)',
        description: 'Average success rate across all agents'
      },
      {
        title: 'Avg Processing Time',
        value: `${avgProcessingTime.toFixed(0)}ms`,
        trend: avgProcessingTime < 2000 ? 'down' : 'stable',
        icon: '⚡',
        color: 'var(--fluent-warning)',
        description: 'Average processing time per execution'
      },
      {
        title: 'Total Tokens',
        value: `${(totalTokens / 1000).toFixed(1)}K`,
        trend: 'up',
        icon: '🧠',
        color: 'var(--fluent-secondary)',
        description: 'Total tokens processed across all agents'
      },
      {
        title: 'System Performance',
        value: `${(systemStatus.performance * 100).toFixed(1)}%`,
        trend: systemStatus.performance > 0.8 ? 'up' : 'down',
        icon: '📊',
        color: systemStatus.performance > 0.8 ? 'var(--fluent-success)' : 'var(--fluent-error)',
        description: 'Current system performance score'
      },
      {
        title: 'Memory Usage',
        value: `${systemStatus.memoryUsage.toFixed(1)}MB`,
        trend: systemStatus.memoryUsage < 500 ? 'down' : 'up',
        icon: '💾',
        color: systemStatus.memoryUsage < 500 ? 'var(--fluent-success)' : 'var(--fluent-warning)',
        description: 'Current memory usage'
      }
    ];
  }, [systemStatus, mockAgentPerformance]);

  // Get trend icon
  const getTrendIcon = (trend?: 'up' | 'down' | 'stable') => {
    switch (trend) {
      case 'up': return '📈';
      case 'down': return '📉';
      case 'stable': return '➡️';
      default: return '';
    }
  };

  // Get trend color
  const getTrendColor = (trend?: 'up' | 'down' | 'stable') => {
    switch (trend) {
      case 'up': return 'var(--fluent-success)';
      case 'down': return 'var(--fluent-error)';
      case 'stable': return 'var(--fluent-neutral-70)';
      default: return 'var(--fluent-neutral-70)';
    }
  };

  return (
    <div className={`k15-insights-panel ${className}`}>
      {/* Header */}
      <div className="k15-insights-header">
        <div className="k15-insights-title">
          <span className="k15-insights-icon">📊</span>
          <h2>Orchestration Analytics</h2>
          </div>
          
        <div className="k15-insights-controls">
          <div className="k15-time-range-selector">
            <label htmlFor="timeRange">Time Range:</label>
            <select 
              id="timeRange"
              value={timeRange}
              onChange={(e) => setTimeRange(e.target.value as any)}
              className="k15-time-range-select"
            >
              <option value="1h">Last Hour</option>
              <option value="24h">Last 24 Hours</option>
              <option value="7d">Last 7 Days</option>
              <option value="30d">Last 30 Days</option>
            </select>
          </div>
          
          <button 
            className="k15-advanced-toggle"
            onClick={() => setShowAdvanced(!showAdvanced)}
            title="Toggle advanced analytics"
          >
            {showAdvanced ? '🔽' : '🔼'} Advanced
          </button>
        </div>
      </div>

      {/* Tab Navigation */}
      <div className="k15-insights-tabs">
        <button
          className={`k15-insights-tab ${activeTab === 'overview' ? 'active' : ''}`}
          onClick={() => setActiveTab('overview')}
        >
          📈 Overview
        </button>
        <button
          className={`k15-insights-tab ${activeTab === 'agents' ? 'active' : ''}`}
          onClick={() => setActiveTab('agents')}
        >
          🤖 Agents
        </button>
        <button
          className={`k15-insights-tab ${activeTab === 'performance' ? 'active' : ''}`}
          onClick={() => setActiveTab('performance')}
        >
          ⚡ Performance
        </button>
        <button
          className={`k15-insights-tab ${activeTab === 'analytics' ? 'active' : ''}`}
          onClick={() => setActiveTab('analytics')}
        >
          📊 Analytics
        </button>
            </div>

      {/* Content */}
      <div className="k15-insights-content">
        {activeTab === 'overview' && (
          <div className="k15-overview-tab">
            {/* Metrics Grid */}
            <div className="k15-metrics-grid">
              {metrics.map((metric, index) => (
                <div key={index} className="k15-metric-card">
                  <div className="k15-metric-header">
                    <span className="k15-metric-icon">{metric.icon}</span>
                    <span className="k15-metric-title">{metric.title}</span>
                    <span 
                      className="k15-metric-trend"
                      style={{ color: getTrendColor(metric.trend) }}
                    >
                      {getTrendIcon(metric.trend)}
                    </span>
              </div>
                  <div 
                    className="k15-metric-value"
                    style={{ color: metric.color }}
                  >
                    {metric.value}
              </div>
                  {metric.description && (
                    <div className="k15-metric-description">
                      {metric.description}
            </div>
                  )}
              </div>
              ))}
            </div>

            {/* System Status */}
            <div className="k15-system-status">
              <h3>System Status</h3>
              <div className="k15-status-grid">
                <div className="k15-status-item">
                  <span className="k15-status-label">Connection:</span>
                  <span className={`k15-status-value ${systemStatus.connected ? 'connected' : 'disconnected'}`}>
                    {systemStatus.connected ? '🟢 Connected' : '🔴 Disconnected'}
                  </span>
                </div>
                <div className="k15-status-item">
                  <span className="k15-status-label">Streaming:</span>
                  <span className={`k15-status-value ${systemStatus.streaming ? 'streaming' : 'idle'}`}>
                    {systemStatus.streaming ? '🔄 Streaming' : '⏸️ Idle'}
                  </span>
                </div>
                <div className="k15-status-item">
                  <span className="k15-status-label">Context Window:</span>
                  <span className="k15-status-value">
                    {systemStatus.contextWindow.toLocaleString()} tokens
                  </span>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'agents' && (
          <div className="k15-agents-tab">
            <div className="k15-agents-grid">
              {mockAgentPerformance.map((agent) => (
                <div key={agent.id} className="k15-agent-card">
                  <div className="k15-agent-header">
                    <span className="k15-agent-name">{agent.name}</span>
                    <span className="k15-agent-id">({agent.id})</span>
        </div>

                  <div className="k15-agent-metrics">
                    <div className="k15-agent-metric">
                      <span className="k15-agent-metric-label">Success Rate:</span>
                      <span className="k15-agent-metric-value">
                        {(agent.successRate * 100).toFixed(1)}%
                      </span>
                    </div>
                    <div className="k15-agent-metric">
                      <span className="k15-agent-metric-label">Avg Processing:</span>
                      <span className="k15-agent-metric-value">
                        {agent.avgProcessingTime.toFixed(0)}ms
                      </span>
                    </div>
                    <div className="k15-agent-metric">
                      <span className="k15-agent-metric-label">Avg Tokens:</span>
                      <span className="k15-agent-metric-value">
                        {agent.avgTokens.toLocaleString()}
                      </span>
                    </div>
                    <div className="k15-agent-metric">
                      <span className="k15-agent-metric-label">Avg Confidence:</span>
                      <span className="k15-agent-metric-value">
                        {(agent.avgConfidence * 100).toFixed(1)}%
                    </span>
                    </div>
                    <div className="k15-agent-metric">
                      <span className="k15-agent-metric-label">Total Executions:</span>
                      <span className="k15-agent-metric-value">
                        {agent.totalExecutions.toLocaleString()}
                    </span>
                  </div>
                  </div>

                  <div className="k15-agent-performance-bar">
                    <div className="k15-performance-label">Performance Score</div>
                    <div className="k15-performance-bar">
                      <div 
                        className="k15-performance-fill"
                        style={{ 
                          width: `${agent.successRate * 100}%`,
                          backgroundColor: agent.successRate > 0.9 ? 'var(--fluent-success)' : 
                                         agent.successRate > 0.8 ? 'var(--fluent-warning)' : 'var(--fluent-error)'
                        }}
                      />
                    </div>
                    <div className="k15-performance-value">
                      {(agent.successRate * 100).toFixed(1)}%
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'performance' && (
          <div className="k15-performance-tab">
            <div className="k15-performance-charts">
              <div className="k15-chart-container">
                <h3>Processing Time Trends</h3>
                <div className="k15-chart-placeholder">
                  📈 Processing time chart would be rendered here
                </div>
              </div>
              
              <div className="k15-chart-container">
                <h3>Token Usage Distribution</h3>
                <div className="k15-chart-placeholder">
                  📊 Token distribution chart would be rendered here
                </div>
              </div>

              <div className="k15-chart-container">
                <h3>Success Rate Over Time</h3>
                <div className="k15-chart-placeholder">
                  📈 Success rate trend chart would be rendered here
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'analytics' && (
          <div className="k15-analytics-tab">
            <div className="k15-analytics-content">
              <div className="k15-analytics-section">
                <h3>Usage Patterns</h3>
                <div className="k15-pattern-grid">
                  <div className="k15-pattern-card">
                    <span className="k15-pattern-icon">🕐</span>
                    <span className="k15-pattern-label">Peak Usage:</span>
                    <span className="k15-pattern-value">2:00 PM - 4:00 PM</span>
                  </div>
                  <div className="k15-pattern-card">
                    <span className="k15-pattern-icon">🔥</span>
                    <span className="k15-pattern-label">Most Active Agent:</span>
                    <span className="k15-pattern-value">Rawr (34% of executions)</span>
                  </div>
                  <div className="k15-pattern-card">
                    <span className="k15-pattern-icon">⚡</span>
                    <span className="k15-pattern-label">Fastest Agent:</span>
                    <span className="k15-pattern-value">Analyzer (1.2s avg)</span>
                  </div>
                </div>
              </div>

              {showAdvanced && (
                <div className="k15-analytics-section">
                  <h3>Advanced Analytics</h3>
                  <div className="k15-advanced-metrics">
                    <div className="k15-advanced-metric">
                      <span className="k15-advanced-label">Token Efficiency:</span>
                      <span className="k15-advanced-value">87.3%</span>
                    </div>
                    <div className="k15-advanced-metric">
                      <span className="k15-advanced-label">Memory Optimization:</span>
                      <span className="k15-advanced-value">92.1%</span>
                    </div>
                    <div className="k15-advanced-metric">
                      <span className="k15-advanced-label">Context Utilization:</span>
                      <span className="k15-advanced-value">78.5%</span>
                    </div>
                  </div>
                </div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}