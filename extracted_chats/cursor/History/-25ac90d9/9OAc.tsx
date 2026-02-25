// InsightsPanel.tsx - Live analytics dashboard for BigDaddyG orchestration
// Real-time performance monitoring with token velocity, memory usage, and agent metrics

import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useIndexedDB } from '../hooks/useIndexedDB';
import { ContextWindowManager } from '../memory/ContextWindowManager';
import { BigDaddyGConfigManager } from '../BigDaddyGEngine.config';
import './insights-panel.css';

export interface AgentMetrics {
  agentId: string;
  name: string;
  status: 'idle' | 'processing' | 'completed' | 'error';
  startTime: number;
  endTime?: number;
  duration: number;
  tokensProcessed: number;
  tokensPerSecond: number;
  confidence: number;
  memoryUsage: number;
  cpuUsage: number;
  errorCount: number;
  successRate: number;
}

export interface SystemMetrics {
  totalTokens: number;
  tokensPerSecond: number;
  memoryUsage: number;
  memoryPeak: number;
  contextWindow: number;
  contextUtilization: number;
  activeAgents: number;
  totalAgents: number;
  orchestrationLatency: number;
  throughput: number;
}

export interface TokenStream {
  timestamp: number;
  tokens: string[];
  confidence: number[];
  entropy: number[];
  agentId: string;
  modelUsed: string;
}

export interface PerformanceInsight {
  type: 'optimization' | 'warning' | 'error' | 'info';
  title: string;
  description: string;
  impact: 'low' | 'medium' | 'high';
  recommendation?: string;
  timestamp: number;
}

export interface InsightsPanelProps {
  engine?: any;
  contextWindowManager?: ContextWindowManager;
  configManager?: BigDaddyGConfigManager;
  orchestrationResult?: any;
  enableRealTimeMetrics?: boolean;
  enablePerformanceInsights?: boolean;
  enableTokenAnalysis?: boolean;
  refreshInterval?: number;
}

export function InsightsPanel({
  engine,
  contextWindowManager,
  configManager,
  orchestrationResult,
  enableRealTimeMetrics = true,
  enablePerformanceInsights = true,
  enableTokenAnalysis = true,
  refreshInterval = 1000
}: InsightsPanelProps) {
  const [agentMetrics, setAgentMetrics] = useState<AgentMetrics[]>([]);
  const [systemMetrics, setSystemMetrics] = useState<SystemMetrics>({
    totalTokens: 0,
    tokensPerSecond: 0,
    memoryUsage: 0,
    memoryPeak: 0,
    contextWindow: 4096,
    contextUtilization: 0,
    activeAgents: 0,
    totalAgents: 0,
    orchestrationLatency: 0,
    throughput: 0
  });
  const [tokenStreams, setTokenStreams] = useState<TokenStream[]>([]);
  const [insights, setInsights] = useState<PerformanceInsight[]>([]);
  const [isMonitoring, setIsMonitoring] = useState(false);
  const [selectedTimeRange, setSelectedTimeRange] = useState<'1m' | '5m' | '15m' | '1h'>('5m');
  const [selectedAgent, setSelectedAgent] = useState<string>('all');
  const [showAdvanced, setShowAdvanced] = useState(false);

  const metricsRef = useRef<HTMLDivElement>(null);
  const { getSessionStats } = useIndexedDB();

  // Real-time metrics collection
  useEffect(() => {
    if (!enableRealTimeMetrics || !isMonitoring) return;

    const interval = setInterval(() => {
      collectSystemMetrics();
      collectAgentMetrics();
      analyzePerformance();
    }, refreshInterval);

    return () => clearInterval(interval);
  }, [enableRealTimeMetrics, isMonitoring, refreshInterval]);

  // Initialize monitoring
  useEffect(() => {
    if (engine && enableRealTimeMetrics) {
      setIsMonitoring(true);
      collectSystemMetrics();
    }
  }, [engine, enableRealTimeMetrics]);

  const collectSystemMetrics = useCallback(() => {
    if (!engine) return;

    const memoryInfo = (performance as any).memory;
    const currentMemory = memoryInfo ? Math.round(memoryInfo.usedJSHeapSize / 1024 / 1024) : 0;
    const peakMemory = memoryInfo ? Math.round(memoryInfo.jsHeapSizeLimit / 1024 / 1024) : 0;
    
    const contextWindow = contextWindowManager?.getWindowSize() || 4096;
    const contextUtilization = systemMetrics.totalTokens / contextWindow;

    setSystemMetrics(prev => ({
      ...prev,
      memoryUsage: currentMemory,
      memoryPeak: peakMemory,
      contextWindow,
      contextUtilization: Math.min(contextUtilization, 1),
      activeAgents: agentMetrics.filter(a => a.status === 'processing').length,
      totalAgents: agentMetrics.length
    }));
  }, [engine, contextWindowManager, agentMetrics, systemMetrics.totalTokens]);

  const collectAgentMetrics = useCallback(async () => {
    if (!engine || !engine.agentRegistry) return;

    const agents = engine.agentRegistry.getActiveAgents();
    const metrics: AgentMetrics[] = [];

    for (const agent of agents) {
      const agentData = await engine.getAgentMetrics(agent.id);
      if (agentData) {
        metrics.push({
          agentId: agent.id,
          name: agent.name || agent.id,
          status: agentData.status || 'idle',
          startTime: agentData.startTime || Date.now(),
          endTime: agentData.endTime,
          duration: agentData.duration || 0,
          tokensProcessed: agentData.tokensProcessed || 0,
          tokensPerSecond: agentData.tokensPerSecond || 0,
          confidence: agentData.confidence || 0,
          memoryUsage: agentData.memoryUsage || 0,
          cpuUsage: agentData.cpuUsage || 0,
          errorCount: agentData.errorCount || 0,
          successRate: agentData.successRate || 1
        });
      }
    }

    setAgentMetrics(metrics);
  }, [engine]);

  const analyzePerformance = useCallback(() => {
    if (!enablePerformanceInsights) return;

    const newInsights: PerformanceInsight[] = [];

    // Memory usage insights
    if (systemMetrics.memoryUsage > systemMetrics.memoryPeak * 0.8) {
      newInsights.push({
        type: 'warning',
        title: 'High Memory Usage',
        description: `Memory usage is at ${Math.round((systemMetrics.memoryUsage / systemMetrics.memoryPeak) * 100)}% of peak`,
        impact: 'high',
        recommendation: 'Consider reducing context window or clearing cache',
        timestamp: Date.now()
      });
    }

    // Context window insights
    if (systemMetrics.contextUtilization > 0.9) {
      newInsights.push({
        type: 'warning',
        title: 'Context Window Near Limit',
        description: `Context utilization is at ${Math.round(systemMetrics.contextUtilization * 100)}%`,
        impact: 'medium',
        recommendation: 'Consider increasing context window or compressing history',
        timestamp: Date.now()
      });
    }

    // Agent performance insights
    const slowAgents = agentMetrics.filter(a => a.tokensPerSecond < 10 && a.status === 'processing');
    if (slowAgents.length > 0) {
      newInsights.push({
        type: 'optimization',
        title: 'Slow Agent Performance',
        description: `${slowAgents.length} agent(s) processing slowly`,
        impact: 'medium',
        recommendation: 'Consider agent optimization or fallback strategies',
        timestamp: Date.now()
      });
    }

    // Error rate insights
    const errorAgents = agentMetrics.filter(a => a.successRate < 0.8);
    if (errorAgents.length > 0) {
      newInsights.push({
        type: 'error',
        title: 'High Error Rate',
        description: `${errorAgents.length} agent(s) with low success rate`,
        impact: 'high',
        recommendation: 'Review agent configuration and error handling',
        timestamp: Date.now()
      });
    }

    setInsights(prev => [...newInsights, ...prev.slice(0, 19)]); // Keep last 20 insights
  }, [systemMetrics, agentMetrics, enablePerformanceInsights]);

  const getInsightIcon = (type: string) => {
    const icons: { [key: string]: string } = {
      optimization: '⚡',
      warning: '⚠️',
      error: '❌',
      info: 'ℹ️'
    };
    return icons[type] || 'ℹ️';
  };

  const getInsightColor = (type: string) => {
    const colors: { [key: string]: string } = {
      optimization: 'var(--success-color)',
      warning: 'var(--warning-color)',
      error: 'var(--error-color)',
      info: 'var(--info-color)'
    };
    return colors[type] || 'var(--info-color)';
  };

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  };

  const formatDuration = (ms: number) => {
    if (ms < 1000) return `${ms}ms`;
    if (ms < 60000) return `${(ms / 1000).toFixed(1)}s`;
    return `${(ms / 60000).toFixed(1)}m`;
  };

  const filteredAgentMetrics = selectedAgent === 'all' 
    ? agentMetrics 
    : agentMetrics.filter(a => a.agentId === selectedAgent);

  const filteredInsights = insights.filter(insight => {
    const age = Date.now() - insight.timestamp;
    const timeRanges = {
      '1m': 60000,
      '5m': 300000,
      '15m': 900000,
      '1h': 3600000
    };
    return age <= timeRanges[selectedTimeRange];
  });

  return (
    <div className="insights-panel">
      <div className="insights-header">
        <h3>📊 Performance Insights</h3>
        <div className="header-controls">
          <div className="time-range-selector">
            <label>Time Range:</label>
            <select 
              value={selectedTimeRange}
              onChange={(e) => setSelectedTimeRange(e.target.value as any)}
            >
              <option value="1m">1 Minute</option>
              <option value="5m">5 Minutes</option>
              <option value="15m">15 Minutes</option>
              <option value="1h">1 Hour</option>
            </select>
          </div>
          
          <div className="agent-filter">
            <label>Agent:</label>
            <select 
              value={selectedAgent}
              onChange={(e) => setSelectedAgent(e.target.value)}
            >
              <option value="all">All Agents</option>
              {agentMetrics.map(agent => (
                <option key={agent.agentId} value={agent.agentId}>
                  {agent.name}
                </option>
              ))}
            </select>
          </div>
          
          <button 
            className="toggle-advanced"
            onClick={() => setShowAdvanced(!showAdvanced)}
          >
            {showAdvanced ? '▼' : '▶'} Advanced
          </button>
        </div>
      </div>

      <div className="insights-body">
        {/* System Metrics */}
        <div className="metrics-section">
          <h4>🖥️ System Metrics</h4>
          <div className="metrics-grid">
            <div className="metric-card">
              <div className="metric-label">Memory Usage</div>
              <div className="metric-value">
                {systemMetrics.memoryUsage} MB / {systemMetrics.memoryPeak} MB
              </div>
              <div className="metric-bar">
                <div 
                  className="metric-fill"
                  style={{ 
                    width: `${(systemMetrics.memoryUsage / systemMetrics.memoryPeak) * 100}%`,
                    backgroundColor: systemMetrics.memoryUsage > systemMetrics.memoryPeak * 0.8 
                      ? 'var(--warning-color)' 
                      : 'var(--success-color)'
                  }}
                />
              </div>
            </div>

            <div className="metric-card">
              <div className="metric-label">Context Window</div>
              <div className="metric-value">
                {systemMetrics.contextWindow.toLocaleString()} tokens
              </div>
              <div className="metric-bar">
                <div 
                  className="metric-fill"
                  style={{ 
                    width: `${systemMetrics.contextUtilization * 100}%`,
                    backgroundColor: systemMetrics.contextUtilization > 0.9 
                      ? 'var(--warning-color)' 
                      : 'var(--success-color)'
                  }}
                />
              </div>
            </div>

            <div className="metric-card">
              <div className="metric-label">Active Agents</div>
              <div className="metric-value">
                {systemMetrics.activeAgents} / {systemMetrics.totalAgents}
              </div>
            </div>

            <div className="metric-card">
              <div className="metric-label">Tokens/Second</div>
              <div className="metric-value">
                {systemMetrics.tokensPerSecond.toFixed(1)}
              </div>
            </div>
          </div>
        </div>

        {/* Agent Performance */}
        <div className="agents-section">
          <h4>🤖 Agent Performance</h4>
          <div className="agents-grid">
            {filteredAgentMetrics.map(agent => (
              <div key={agent.agentId} className="agent-card">
                <div className="agent-header">
                  <span className="agent-name">{agent.name}</span>
                  <span className={`agent-status ${agent.status}`}>
                    {agent.status}
                  </span>
                </div>
                
                <div className="agent-metrics">
                  <div className="metric-row">
                    <span>Duration:</span>
                    <span>{formatDuration(agent.duration)}</span>
                  </div>
                  <div className="metric-row">
                    <span>Tokens/sec:</span>
                    <span>{agent.tokensPerSecond.toFixed(1)}</span>
                  </div>
                  <div className="metric-row">
                    <span>Confidence:</span>
                    <span>{(agent.confidence * 100).toFixed(1)}%</span>
                  </div>
                  <div className="metric-row">
                    <span>Success Rate:</span>
                    <span>{(agent.successRate * 100).toFixed(1)}%</span>
                  </div>
                </div>

                <div className="agent-progress">
                  <div className="progress-bar">
                    <div 
                      className="progress-fill"
                      style={{ 
                        width: `${agent.confidence * 100}%`,
                        backgroundColor: agent.confidence > 0.8 
                          ? 'var(--success-color)' 
                          : agent.confidence > 0.6 
                          ? 'var(--warning-color)' 
                          : 'var(--error-color)'
                      }}
                    />
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Performance Insights */}
        {enablePerformanceInsights && (
          <div className="insights-section">
            <h4>💡 Performance Insights</h4>
            <div className="insights-list">
              {filteredInsights.map((insight, index) => (
                <div key={index} className="insight-item">
                  <div className="insight-header">
                    <span className="insight-icon">
                      {getInsightIcon(insight.type)}
                    </span>
                    <span className="insight-title">{insight.title}</span>
                    <span className={`insight-impact ${insight.impact}`}>
                      {insight.impact}
                    </span>
                  </div>
                  <div className="insight-description">
                    {insight.description}
                  </div>
                  {insight.recommendation && (
                    <div className="insight-recommendation">
                      <strong>Recommendation:</strong> {insight.recommendation}
                    </div>
                  )}
                  <div className="insight-timestamp">
                    {new Date(insight.timestamp).toLocaleTimeString()}
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Advanced Metrics */}
        {showAdvanced && (
          <div className="advanced-section">
            <h4>🔬 Advanced Metrics</h4>
            <div className="advanced-grid">
              <div className="advanced-card">
                <h5>Token Stream Analysis</h5>
                <div className="stream-stats">
                  <div>Total Streams: {tokenStreams.length}</div>
                  <div>Average Confidence: {
                    tokenStreams.length > 0 
                      ? (tokenStreams.reduce((sum, stream) => sum + stream.confidence.reduce((a, b) => a + b, 0) / stream.confidence.length, 0) / tokenStreams.length * 100).toFixed(1)
                      : 0
                  }%</div>
                  <div>Average Entropy: {
                    tokenStreams.length > 0 
                      ? (tokenStreams.reduce((sum, stream) => sum + stream.entropy.reduce((a, b) => a + b, 0) / stream.entropy.length, 0) / tokenStreams.length).toFixed(3)
                      : 0
                  }</div>
                </div>
              </div>

              <div className="advanced-card">
                <h5>Orchestration Metrics</h5>
                <div className="orchestration-stats">
                  <div>Latency: {formatDuration(systemMetrics.orchestrationLatency)}</div>
                  <div>Throughput: {systemMetrics.throughput.toFixed(2)} ops/sec</div>
                  <div>Total Tokens: {systemMetrics.totalTokens.toLocaleString()}</div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}