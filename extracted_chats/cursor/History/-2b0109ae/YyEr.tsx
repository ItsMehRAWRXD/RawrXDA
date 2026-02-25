// BigDaddyGEngine/components/CognitiveDashboard.tsx - Unified Dashboard for Cognitive Development Environment
import React, { useState, useEffect, useCallback } from 'react';
import { useOptimizedSelfOptimizingMesh } from '../hooks/useOptimizedSelfOptimizingMesh';
import { useMultiAgentOrchestrator } from '../hooks/useMultiAgentOrchestrator';
import { MemoryGraph } from '../memory/MemoryGraph';
import { SemanticEmbedder } from '../memory/SemanticEmbedder';
import { ContextFetcher } from '../memory/ContextFetcher';
import { AgentMetricsLogger } from '../optimization/AgentMetricsLogger';
import { ReinforcementLearner } from '../optimization/ReinforcementLearner';
import { MetaOrchestrator } from '../orchestration/MetaOrchestrator';
import { AgentForgeRegistry } from '../registry/AgentForgeRegistry';
import { LiveCollaborationLayer } from '../collaboration/LiveCollaborationLayer';
import { FigmaAgent } from '../adapters/FigmaAgent';
import { JiraAgent } from '../adapters/JiraAgent';
import { GitHubAgent } from '../adapters/GitHubAgent';

export interface CognitiveDashboardProps {
  projectId?: string;
  sessionId?: string;
  userId?: string;
  onHealthChange?: (health: any) => void;
  onOptimization?: (result: any) => void;
}

export function CognitiveDashboard({
  projectId = 'default-project',
  sessionId = 'default-session',
  userId = 'user-1',
  onHealthChange,
  onOptimization
}: CognitiveDashboardProps) {
  // Initialize core components
  const [memoryGraph] = useState(() => new MemoryGraph());
  const [embedder] = useState(() => new SemanticEmbedder());
  const [contextFetcher] = useState(() => new ContextFetcher(memoryGraph, embedder));
  const [metricsLogger] = useState(() => new AgentMetricsLogger());
  const [learner] = useState(() => new ReinforcementLearner());
  const [agentRegistry] = useState(() => new AgentForgeRegistry());
  
  // Initialize orchestrator
  const { orchestratorInstance } = useMultiAgentOrchestrator();
  
  // Initialize self-optimizing mesh
  const {
    isLoading: meshLoading,
    isInitialized: meshInitialized,
    error: meshError,
    health: meshHealth,
    topology: meshTopology,
    registerAgent,
    unregisterAgent,
    routeTask,
    executeTask,
    optimize: optimizeMesh,
    shutdown: shutdownMesh
  } = useOptimizedSelfOptimizingMesh(
    metricsLogger,
    learner,
    orchestratorInstance,
    {
      enableLearning: true,
      enableAdaptiveRouting: true,
      enableSelfHealing: true,
      optimizeInterval: 30000,
      loadBalanceThreshold: 0.8,
      degradationThreshold: 0.3
    },
    {
      enableAutoRefresh: true,
      refreshInterval: 5000,
      onHealthChange,
      onOptimization
    }
  );

  // Initialize meta-orchestrator
  const [metaOrchestrator] = useState(() => 
    orchestratorInstance ? new MetaOrchestrator(orchestratorInstance, metricsLogger, learner) : null
  );

  // Initialize cross-domain agents
  const [figmaAgent] = useState(() => new FigmaAgent('figma-key', 'figma-file-id'));
  const [jiraAgent] = useState(() => new JiraAgent('jira-key', 'jira-base-url'));
  const [githubAgent] = useState(() => new GitHubAgent('github-key', 'owner', 'repo'));

  // State management
  const [activeTab, setActiveTab] = useState<'overview' | 'orchestration' | 'memory' | 'optimization' | 'collaboration' | 'adapters'>('overview');
  const [isRunning, setIsRunning] = useState(false);
  const [currentTask, setCurrentTask] = useState('');
  const [results, setResults] = useState<any[]>([]);
  const [insights, setInsights] = useState<any[]>([]);

  // Register cross-domain agents
  useEffect(() => {
    if (meshInitialized) {
      registerAgent(figmaAgent);
      registerAgent(jiraAgent);
      registerAgent(githubAgent);
    }
  }, [meshInitialized, registerAgent, figmaAgent, jiraAgent, githubAgent]);

  // Handle task execution
  const handleExecuteTask = useCallback(async (task: string, chain?: string[]) => {
    if (!meshInitialized) return;
    
    setIsRunning(true);
    setCurrentTask(task);
    
    try {
      const result = await executeTask(task, chain);
      setResults(prev => [...prev, { task, result, timestamp: Date.now() }]);
      
      // Store in memory graph
      await memoryGraph.storeOrchestrationGraph(result, projectId, sessionId);
      
      // Analyze with meta-orchestrator
      if (metaOrchestrator) {
        const newInsights = metaOrchestrator.analyzeOrchestration(result);
        setInsights(prev => [...prev, ...newInsights]);
      }
    } catch (error) {
      console.error('Task execution failed:', error);
    } finally {
      setIsRunning(false);
      setCurrentTask('');
    }
  }, [meshInitialized, executeTask, memoryGraph, projectId, sessionId, metaOrchestrator]);

  // Handle optimization
  const handleOptimize = useCallback(async () => {
    if (!meshInitialized) return;
    
    try {
      await optimizeMesh();
      onOptimization?.('Mesh optimization completed');
    } catch (error) {
      console.error('Optimization failed:', error);
    }
  }, [meshInitialized, optimizeMesh, onOptimization]);

  // Handle self-mutation
  const handleSelfMutation = useCallback(async () => {
    if (!metaOrchestrator) return;
    
    try {
      const mutationResult = await metaOrchestrator.applySelfMutation();
      console.log('Self-mutation result:', mutationResult);
    } catch (error) {
      console.error('Self-mutation failed:', error);
    }
  }, [metaOrchestrator]);

  // Get context suggestions
  const getContextSuggestions = useCallback(async (prompt: string) => {
    try {
      const suggestions = await contextFetcher.getContextSuggestions(prompt, projectId);
      return suggestions;
    } catch (error) {
      console.error('Context suggestions failed:', error);
      return { suggestions: [], relatedPrompts: [], agentRecommendations: [] };
    }
  }, [contextFetcher, projectId]);

  return (
    <div className="cognitive-dashboard">
      <div className="dashboard-header">
        <h1>🧠 BigDaddyG Cognitive Development Environment</h1>
        <div className="status-indicators">
          <span className={`mesh-status ${meshInitialized ? 'active' : 'inactive'}`}>
            {meshInitialized ? '🟢 Mesh Active' : '🔴 Mesh Inactive'}
          </span>
          <span className="health-status">
            {meshHealth ? `Health: ${meshHealth.overallHealth}` : 'Health: Unknown'}
          </span>
          <span className="agents-count">
            Agents: {meshTopology?.nodes.size || 0}
          </span>
        </div>
      </div>

      <div className="dashboard-tabs">
        <button 
          className={activeTab === 'overview' ? 'active' : ''} 
          onClick={() => setActiveTab('overview')}
        >
          📊 Overview
        </button>
        <button 
          className={activeTab === 'orchestration' ? 'active' : ''} 
          onClick={() => setActiveTab('orchestration')}
        >
          🎯 Orchestration
        </button>
        <button 
          className={activeTab === 'memory' ? 'active' : ''} 
          onClick={() => setActiveTab('memory')}
        >
          🧠 Memory
        </button>
        <button 
          className={activeTab === 'optimization' ? 'active' : ''} 
          onClick={() => setActiveTab('optimization')}
        >
          ⚡ Optimization
        </button>
        <button 
          className={activeTab === 'collaboration' ? 'active' : ''} 
          onClick={() => setActiveTab('collaboration')}
        >
          🤝 Collaboration
        </button>
        <button 
          className={activeTab === 'adapters' ? 'active' : ''} 
          onClick={() => setActiveTab('adapters')}
        >
          🔌 Adapters
        </button>
      </div>

      <div className="dashboard-content">
        {activeTab === 'overview' && (
          <div className="overview-panel">
            <div className="metrics-grid">
              <div className="metric-card">
                <h3>System Health</h3>
                <div className="metric-value">
                  {meshHealth?.overallHealth || 'Unknown'}
                </div>
                <div className="metric-details">
                  Active: {meshHealth?.activeAgents || 0} | 
                  Idle: {meshHealth?.idleAgents || 0} | 
                  Degraded: {meshHealth?.degradedAgents || 0}
                </div>
              </div>
              
              <div className="metric-card">
                <h3>Performance</h3>
                <div className="metric-value">
                  {meshHealth?.optimizationScore ? `${(meshHealth.optimizationScore * 100).toFixed(1)}%` : 'N/A'}
                </div>
                <div className="metric-details">
                  Avg Load: {meshHealth?.averageLoad ? `${(meshHealth.averageLoad * 100).toFixed(1)}%` : 'N/A'}
                </div>
              </div>
              
              <div className="metric-card">
                <h3>Memory</h3>
                <div className="metric-value">
                  {memoryGraph.getStats().totalTraces}
                </div>
                <div className="metric-details">
                  Projects: {memoryGraph.getStats().uniqueProjects} | 
                  Sessions: {memoryGraph.getStats().uniqueSessions}
                </div>
              </div>
              
              <div className="metric-card">
                <h3>Insights</h3>
                <div className="metric-value">
                  {insights.length}
                </div>
                <div className="metric-details">
                  Patterns: {insights.filter(i => i.type === 'pattern').length} | 
                  Bottlenecks: {insights.filter(i => i.type === 'bottleneck').length}
                </div>
              </div>
            </div>

            <div className="task-execution">
              <h3>Execute Task</h3>
              <div className="task-input">
                <input
                  type="text"
                  placeholder="Enter your task..."
                  value={currentTask}
                  onChange={(e) => setCurrentTask(e.target.value)}
                  disabled={isRunning}
                />
                <button 
                  onClick={() => handleExecuteTask(currentTask)}
                  disabled={isRunning || !currentTask}
                >
                  {isRunning ? 'Running...' : 'Execute'}
                </button>
              </div>
              
              {results.length > 0 && (
                <div className="recent-results">
                  <h4>Recent Results</h4>
                  {results.slice(-3).map((result, index) => (
                    <div key={index} className="result-item">
                      <div className="result-task">{result.task}</div>
                      <div className="result-status">
                        {result.result.success ? '✅ Success' : '❌ Failed'}
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        )}

        {activeTab === 'orchestration' && (
          <div className="orchestration-panel">
            <div className="orchestration-controls">
              <button onClick={handleOptimize} disabled={!meshInitialized}>
                Optimize Mesh
              </button>
              <button onClick={handleSelfMutation} disabled={!metaOrchestrator}>
                Apply Self-Mutation
              </button>
            </div>
            
            {meshTopology && (
              <div className="topology-visualization">
                <h3>Agent Topology</h3>
                <div className="topology-graph">
                  {Array.from(meshTopology.nodes.values()).map(node => (
                    <div key={node.id} className={`topology-node ${node.status}`}>
                      <div className="node-id">{node.id}</div>
                      <div className="node-status">{node.status}</div>
                      <div className="node-load">{Math.round(node.load * 100)}%</div>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        )}

        {activeTab === 'memory' && (
          <div className="memory-panel">
            <div className="memory-stats">
              <h3>Memory Statistics</h3>
              <div className="stats-grid">
                <div className="stat-item">
                  <span className="stat-label">Total Traces:</span>
                  <span className="stat-value">{memoryGraph.getStats().totalTraces}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Projects:</span>
                  <span className="stat-value">{memoryGraph.getStats().uniqueProjects}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Sessions:</span>
                  <span className="stat-value">{memoryGraph.getStats().uniqueSessions}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Memory Usage:</span>
                  <span className="stat-value">{Math.round(memoryGraph.getStats().memoryUsage / 1024)} KB</span>
                </div>
              </div>
            </div>
            
            <div className="context-search">
              <h3>Context Search</h3>
              <input
                type="text"
                placeholder="Search for similar contexts..."
                onChange={async (e) => {
                  if (e.target.value.length > 3) {
                    const suggestions = await getContextSuggestions(e.target.value);
                    console.log('Context suggestions:', suggestions);
                  }
                }}
              />
            </div>
          </div>
        )}

        {activeTab === 'optimization' && (
          <div className="optimization-panel">
            <div className="optimization-metrics">
              <h3>Optimization Metrics</h3>
              <div className="metrics-grid">
                <div className="metric-item">
                  <span className="metric-label">Total Executions:</span>
                  <span className="metric-value">{metricsLogger.getSystemStats().totalExecutions}</span>
                </div>
                <div className="metric-item">
                  <span className="metric-label">Success Rate:</span>
                  <span className="metric-value">{Math.round(metricsLogger.getSystemStats().successRate * 100)}%</span>
                </div>
                <div className="metric-item">
                  <span className="metric-label">Avg Execution Time:</span>
                  <span className="metric-value">{Math.round(metricsLogger.getSystemStats().averageExecutionTime)}ms</span>
                </div>
              </div>
            </div>
            
            <div className="learning-stats">
              <h3>Learning Statistics</h3>
              <div className="stats-grid">
                <div className="stat-item">
                  <span className="stat-label">Total Agents:</span>
                  <span className="stat-value">{learner.getLearningStats().totalAgents}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Avg Reward:</span>
                  <span className="stat-value">{learner.getLearningStats().averageReward.toFixed(2)}</span>
                </div>
                <div className="stat-item">
                  <span className="stat-label">Total Actions:</span>
                  <span className="stat-value">{learner.getLearningStats().totalActions}</span>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'collaboration' && (
          <div className="collaboration-panel">
            <LiveCollaborationLayer
              sessionId={sessionId}
              userId={userId}
              traces={results.map(r => r.result)}
              steps={results.flatMap(r => r.result.steps || [])}
            />
          </div>
        )}

        {activeTab === 'adapters' && (
          <div className="adapters-panel">
            <div className="adapter-grid">
              <div className="adapter-card">
                <h3>🎨 Figma Agent</h3>
                <div className="adapter-status">Active</div>
                <div className="adapter-capabilities">
                  Design automation, Component creation, Style application
                </div>
              </div>
              
              <div className="adapter-card">
                <h3>📋 Jira Agent</h3>
                <div className="adapter-status">Active</div>
                <div className="adapter-capabilities">
                  Issue management, Project tracking, Workflow automation
                </div>
              </div>
              
              <div className="adapter-card">
                <h3>🐙 GitHub Agent</h3>
                <div className="adapter-status">Active</div>
                <div className="adapter-capabilities">
                  Repository management, Pull request automation, Code review
                </div>
              </div>
            </div>
          </div>
        )}
      </div>

      <style jsx>{`
        .cognitive-dashboard {
          display: flex;
          flex-direction: column;
          height: 100vh;
          background: #f8f9fa;
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        }

        .dashboard-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px;
          background: #fff;
          border-bottom: 1px solid #e9ecef;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .dashboard-header h1 {
          margin: 0;
          color: #2c3e50;
          font-size: 24px;
        }

        .status-indicators {
          display: flex;
          gap: 20px;
          font-size: 14px;
        }

        .mesh-status.active {
          color: #28a745;
        }

        .mesh-status.inactive {
          color: #dc3545;
        }

        .dashboard-tabs {
          display: flex;
          background: #fff;
          border-bottom: 1px solid #e9ecef;
        }

        .dashboard-tabs button {
          padding: 12px 20px;
          border: none;
          background: transparent;
          cursor: pointer;
          font-size: 14px;
          color: #6c757d;
          border-bottom: 2px solid transparent;
          transition: all 0.2s;
        }

        .dashboard-tabs button.active {
          color: #007bff;
          border-bottom-color: #007bff;
        }

        .dashboard-tabs button:hover {
          color: #007bff;
        }

        .dashboard-content {
          flex: 1;
          padding: 20px;
          overflow-y: auto;
        }

        .metrics-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
          gap: 20px;
          margin-bottom: 30px;
        }

        .metric-card {
          background: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .metric-card h3 {
          margin: 0 0 10px 0;
          color: #2c3e50;
          font-size: 16px;
        }

        .metric-value {
          font-size: 24px;
          font-weight: bold;
          color: #007bff;
          margin-bottom: 5px;
        }

        .metric-details {
          font-size: 12px;
          color: #6c757d;
        }

        .task-execution {
          background: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .task-input {
          display: flex;
          gap: 10px;
          margin-bottom: 20px;
        }

        .task-input input {
          flex: 1;
          padding: 10px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 14px;
        }

        .task-input button {
          padding: 10px 20px;
          background: #007bff;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-size: 14px;
        }

        .task-input button:disabled {
          background: #6c757d;
          cursor: not-allowed;
        }

        .recent-results {
          margin-top: 20px;
        }

        .result-item {
          display: flex;
          justify-content: space-between;
          padding: 10px;
          margin-bottom: 5px;
          background: #f8f9fa;
          border-radius: 4px;
        }

        .orchestration-controls {
          display: flex;
          gap: 10px;
          margin-bottom: 20px;
        }

        .orchestration-controls button {
          padding: 10px 20px;
          background: #28a745;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
        }

        .orchestration-controls button:disabled {
          background: #6c757d;
          cursor: not-allowed;
        }

        .topology-graph {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
          gap: 15px;
          margin-top: 20px;
        }

        .topology-node {
          background: #fff;
          padding: 15px;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
          text-align: center;
        }

        .topology-node.active {
          border-left: 4px solid #28a745;
        }

        .topology-node.idle {
          border-left: 4px solid #ffc107;
        }

        .topology-node.degraded {
          border-left: 4px solid #dc3545;
        }

        .node-id {
          font-weight: bold;
          margin-bottom: 5px;
        }

        .node-status {
          font-size: 12px;
          color: #6c757d;
          margin-bottom: 5px;
        }

        .node-load {
          font-size: 14px;
          color: #007bff;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 15px;
        }

        .stat-item {
          display: flex;
          justify-content: space-between;
          padding: 10px;
          background: #f8f9fa;
          border-radius: 4px;
        }

        .stat-label {
          color: #6c757d;
        }

        .stat-value {
          font-weight: bold;
          color: #2c3e50;
        }

        .adapter-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
          gap: 20px;
        }

        .adapter-card {
          background: #fff;
          padding: 20px;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .adapter-card h3 {
          margin: 0 0 10px 0;
          color: #2c3e50;
        }

        .adapter-status {
          color: #28a745;
          font-weight: bold;
          margin-bottom: 10px;
        }

        .adapter-capabilities {
          font-size: 14px;
          color: #6c757d;
        }
      `}</style>
    </div>
  );
}
