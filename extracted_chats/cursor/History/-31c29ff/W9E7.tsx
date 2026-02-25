// BigDaddyGEngine/marketplace/AgentMarketplace.tsx - Unified Agent Discovery and Management
import React, { useState, useEffect, useCallback } from 'react';
import { AgentForgeRegistry, AgentManifest } from '../registry/AgentForgeRegistry';

export interface AgentBenchmark {
  agentId: string;
  speed: number; // ms
  accuracy: number; // 0-1
  confidence: number; // 0-1
  reliability: number; // 0-1
  resourceUsage: number; // 0-1
  lastTested: number;
  testCount: number;
}

export interface AgentCategory {
  id: string;
  name: string;
  description: string;
  icon: string;
  color: string;
}

export interface AgentFilter {
  category?: string;
  minAccuracy?: number;
  maxResourceUsage?: number;
  status?: 'active' | 'inactive' | 'deprecated';
  tags?: string[];
}

export interface AgentInstallation {
  agentId: string;
  version: string;
  status: 'installing' | 'installed' | 'failed' | 'updating';
  progress: number;
  error?: string;
  installedAt: number;
}

export interface MarketplaceStats {
  totalAgents: number;
  activeAgents: number;
  categories: number;
  totalDownloads: number;
  averageRating: number;
  lastUpdated: number;
}

export interface AgentMarketplaceProps {
  registry: AgentForgeRegistry;
  onAgentInstall?: (agentId: string, version: string) => void;
  onAgentUninstall?: (agentId: string) => void;
  onAgentUpdate?: (agentId: string, version: string) => void;
  onBenchmarkRun?: (agentId: string) => void;
}

export function AgentMarketplace({
  registry,
  onAgentInstall,
  onAgentUninstall,
  onAgentUpdate,
  onBenchmarkRun
}: AgentMarketplaceProps) {
  const [agents, setAgents] = useState<AgentManifest[]>([]);
  const [benchmarks, setBenchmarks] = useState<Map<string, AgentBenchmark>>(new Map());
  const [installations, setInstallations] = useState<Map<string, AgentInstallation>>(new Map());
  const [filter, setFilter] = useState<AgentFilter>({});
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [sortBy, setSortBy] = useState<'name' | 'rating' | 'downloads' | 'recent'>('name');
  const [viewMode, setViewMode] = useState<'grid' | 'list'>('grid');
  const [stats, setStats] = useState<MarketplaceStats>({
    totalAgents: 0,
    activeAgents: 0,
    categories: 0,
    totalDownloads: 0,
    averageRating: 0,
    lastUpdated: Date.now()
  });

  const categories: AgentCategory[] = [
    { id: 'all', name: 'All', description: 'All agents', icon: '🔍', color: '#6366f1' },
    { id: 'code-generation', name: 'Code Generation', description: 'Agents for generating code', icon: '💻', color: '#10b981' },
    { id: 'analysis', name: 'Analysis', description: 'Code analysis and review agents', icon: '🔍', color: '#f59e0b' },
    { id: 'optimization', name: 'Optimization', description: 'Performance and code optimization', icon: '⚡', color: '#ef4444' },
    { id: 'testing', name: 'Testing', description: 'Test generation and execution', icon: '🧪', color: '#8b5cf6' },
    { id: 'documentation', name: 'Documentation', description: 'Documentation generation', icon: '📚', color: '#06b6d4' },
    { id: 'security', name: 'Security', description: 'Security analysis and scanning', icon: '🔒', color: '#84cc16' }
  ];

  // Load agents and benchmarks on mount
  useEffect(() => {
    loadAgents();
    loadBenchmarks();
    loadInstallations();
    loadStats();
  }, []);

  const loadAgents = useCallback(async () => {
    try {
      const allAgents = registry.getAllAgents();
      setAgents(allAgents);
      console.log(`📦 Loaded ${allAgents.length} agents from registry`);
    } catch (error) {
      console.error('❌ Failed to load agents:', error);
    }
  }, [registry]);

  const loadBenchmarks = useCallback(async () => {
    try {
      // Simulate loading benchmark data
      const benchmarkData = new Map<string, AgentBenchmark>();
      
      agents.forEach(agent => {
        benchmarkData.set(agent.id, {
          agentId: agent.id,
          speed: Math.random() * 1000 + 100, // 100-1100ms
          accuracy: Math.random() * 0.4 + 0.6, // 0.6-1.0
          confidence: Math.random() * 0.3 + 0.7, // 0.7-1.0
          reliability: Math.random() * 0.2 + 0.8, // 0.8-1.0
          resourceUsage: Math.random() * 0.5 + 0.2, // 0.2-0.7
          lastTested: Date.now() - Math.random() * 86400000, // Last 24 hours
          testCount: Math.floor(Math.random() * 100) + 10
        });
      });

      setBenchmarks(benchmarkData);
      console.log(`📊 Loaded benchmarks for ${benchmarkData.size} agents`);
    } catch (error) {
      console.error('❌ Failed to load benchmarks:', error);
    }
  }, [agents]);

  const loadInstallations = useCallback(async () => {
    try {
      // Simulate loading installation data
      const installationData = new Map<string, AgentInstallation>();
      
      agents.slice(0, 3).forEach(agent => {
        installationData.set(agent.id, {
          agentId: agent.id,
          version: agent.version,
          status: 'installed',
          progress: 100,
          installedAt: Date.now() - Math.random() * 604800000 // Last week
        });
      });

      setInstallations(installationData);
      console.log(`📥 Loaded installations for ${installationData.size} agents`);
    } catch (error) {
      console.error('❌ Failed to load installations:', error);
    }
  }, [agents]);

  const loadStats = useCallback(async () => {
    try {
      const newStats: MarketplaceStats = {
        totalAgents: agents.length,
        activeAgents: agents.filter(a => a.status === 'active').length,
        categories: categories.length - 1, // Exclude 'all'
        totalDownloads: agents.reduce((sum, a) => sum + (a.downloads || 0), 0),
        averageRating: agents.reduce((sum, a) => sum + (a.rating || 0), 0) / agents.length || 0,
        lastUpdated: Date.now()
      };
      
      setStats(newStats);
      console.log('📈 Loaded marketplace stats');
    } catch (error) {
      console.error('❌ Failed to load stats:', error);
    }
  }, [agents, categories]);

  const handleInstallAgent = useCallback(async (agentId: string, version: string) => {
    try {
      console.log(`📥 Installing agent ${agentId} version ${version}`);
      
      // Update installation status
      setInstallations(prev => {
        const newMap = new Map(prev);
        newMap.set(agentId, {
          agentId,
          version,
          status: 'installing',
          progress: 0,
          installedAt: Date.now()
        });
        return newMap;
      });

      // Simulate installation process
      const progressInterval = setInterval(() => {
        setInstallations(prev => {
          const newMap = new Map(prev);
          const installation = newMap.get(agentId);
          if (installation && installation.status === 'installing') {
            const newProgress = Math.min(installation.progress + 10, 100);
            newMap.set(agentId, {
              ...installation,
              progress: newProgress,
              status: newProgress === 100 ? 'installed' : 'installing'
            });
            return newMap;
          }
          return newMap;
        });
      }, 200);

      // Complete installation after 2 seconds
      setTimeout(() => {
        clearInterval(progressInterval);
        setInstallations(prev => {
          const newMap = new Map(prev);
          newMap.set(agentId, {
            agentId,
            version,
            status: 'installed',
            progress: 100,
            installedAt: Date.now()
          });
          return newMap;
        });
        
        if (onAgentInstall) {
          onAgentInstall(agentId, version);
        }
        console.log(`✅ Installed agent ${agentId}`);
      }, 2000);

    } catch (error) {
      console.error(`❌ Failed to install agent ${agentId}:`, error);
      setInstallations(prev => {
        const newMap = new Map(prev);
        newMap.set(agentId, {
          agentId,
          version,
          status: 'failed',
          progress: 0,
          error: error instanceof Error ? error.message : 'Unknown error',
          installedAt: Date.now()
        });
        return newMap;
      });
    }
  }, [onAgentInstall]);

  const handleUninstallAgent = useCallback(async (agentId: string) => {
    try {
      console.log(`🗑️ Uninstalling agent ${agentId}`);
      
      setInstallations(prev => {
        const newMap = new Map(prev);
        newMap.delete(agentId);
        return newMap;
      });

      if (onAgentUninstall) {
        onAgentUninstall(agentId);
      }
      console.log(`✅ Uninstalled agent ${agentId}`);
    } catch (error) {
      console.error(`❌ Failed to uninstall agent ${agentId}:`, error);
    }
  }, [onAgentUninstall]);

  const handleUpdateAgent = useCallback(async (agentId: string, version: string) => {
    try {
      console.log(`🔄 Updating agent ${agentId} to version ${version}`);
      
      setInstallations(prev => {
        const newMap = new Map(prev);
        newMap.set(agentId, {
          agentId,
          version,
          status: 'updating',
          progress: 0,
          installedAt: Date.now()
        });
        return newMap;
      });

      // Simulate update process
      setTimeout(() => {
        setInstallations(prev => {
          const newMap = new Map(prev);
          newMap.set(agentId, {
            agentId,
            version,
            status: 'installed',
            progress: 100,
            installedAt: Date.now()
          });
          return newMap;
        });
        
        if (onAgentUpdate) {
          onAgentUpdate(agentId, version);
        }
        console.log(`✅ Updated agent ${agentId}`);
      }, 1500);

    } catch (error) {
      console.error(`❌ Failed to update agent ${agentId}:`, error);
    }
  }, [onAgentUpdate]);

  const handleRunBenchmark = useCallback(async (agentId: string) => {
    try {
      console.log(`🏃 Running benchmark for agent ${agentId}`);
      
      if (onBenchmarkRun) {
        onBenchmarkRun(agentId);
      }

      // Simulate benchmark run
      setTimeout(() => {
        setBenchmarks(prev => {
          const newMap = new Map(prev);
          const benchmark = newMap.get(agentId);
          if (benchmark) {
            newMap.set(agentId, {
              ...benchmark,
              speed: Math.random() * 1000 + 100,
              accuracy: Math.random() * 0.4 + 0.6,
              confidence: Math.random() * 0.3 + 0.7,
              reliability: Math.random() * 0.2 + 0.8,
              resourceUsage: Math.random() * 0.5 + 0.2,
              lastTested: Date.now(),
              testCount: benchmark.testCount + 1
            });
          }
          return newMap;
        });
        console.log(`✅ Benchmark completed for agent ${agentId}`);
      }, 3000);

    } catch (error) {
      console.error(`❌ Failed to run benchmark for agent ${agentId}:`, error);
    }
  }, [onBenchmarkRun]);

  const filteredAgents = agents.filter(agent => {
    // Category filter
    if (selectedCategory !== 'all' && agent.category !== selectedCategory) {
      return false;
    }

    // Search query filter
    if (searchQuery && !agent.name.toLowerCase().includes(searchQuery.toLowerCase()) &&
        !agent.description.toLowerCase().includes(searchQuery.toLowerCase())) {
      return false;
    }

    // Additional filters
    if (filter.minAccuracy) {
      const benchmark = benchmarks.get(agent.id);
      if (!benchmark || benchmark.accuracy < filter.minAccuracy) {
        return false;
      }
    }

    if (filter.maxResourceUsage) {
      const benchmark = benchmarks.get(agent.id);
      if (!benchmark || benchmark.resourceUsage > filter.maxResourceUsage) {
        return false;
      }
    }

    return true;
  });

  const sortedAgents = [...filteredAgents].sort((a, b) => {
    switch (sortBy) {
      case 'name':
        return a.name.localeCompare(b.name);
      case 'rating':
        return (b.rating || 0) - (a.rating || 0);
      case 'downloads':
        return (b.downloads || 0) - (a.downloads || 0);
      case 'recent':
        return (b.updatedAt || 0) - (a.updatedAt || 0);
      default:
        return 0;
    }
  });

  return (
    <div className="agent-marketplace">
      {/* Header */}
      <div className="marketplace-header">
        <h2>🤖 Agent Marketplace</h2>
        <div className="marketplace-stats">
          <div className="stat">
            <span className="stat-value">{stats.totalAgents}</span>
            <span className="stat-label">Agents</span>
          </div>
          <div className="stat">
            <span className="stat-value">{stats.activeAgents}</span>
            <span className="stat-label">Active</span>
          </div>
          <div className="stat">
            <span className="stat-value">{stats.categories}</span>
            <span className="stat-label">Categories</span>
          </div>
          <div className="stat">
            <span className="stat-value">{stats.totalDownloads}</span>
            <span className="stat-label">Downloads</span>
          </div>
        </div>
      </div>

      {/* Filters and Search */}
      <div className="marketplace-controls">
        <div className="search-bar">
          <input
            type="text"
            placeholder="Search agents..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="search-input"
          />
        </div>

        <div className="filter-controls">
          <select
            value={selectedCategory}
            onChange={(e) => setSelectedCategory(e.target.value)}
            className="category-filter"
          >
            {categories.map(category => (
              <option key={category.id} value={category.id}>
                {category.icon} {category.name}
              </option>
            ))}
          </select>

          <select
            value={sortBy}
            onChange={(e) => setSortBy(e.target.value as any)}
            className="sort-filter"
          >
            <option value="name">Sort by Name</option>
            <option value="rating">Sort by Rating</option>
            <option value="downloads">Sort by Downloads</option>
            <option value="recent">Sort by Recent</option>
          </select>

          <div className="view-toggle">
            <button
              onClick={() => setViewMode('grid')}
              className={viewMode === 'grid' ? 'active' : ''}
            >
              ⊞ Grid
            </button>
            <button
              onClick={() => setViewMode('list')}
              className={viewMode === 'list' ? 'active' : ''}
            >
              ☰ List
            </button>
          </div>
        </div>
      </div>

      {/* Agent Grid/List */}
      <div className={`agent-container ${viewMode}`}>
        {sortedAgents.map(agent => {
          const benchmark = benchmarks.get(agent.id);
          const installation = installations.get(agent.id);
          const category = categories.find(c => c.id === agent.category);

          return (
            <div key={agent.id} className="agent-card">
              <div className="agent-header">
                <div className="agent-icon">
                  {category?.icon || '🤖'}
                </div>
                <div className="agent-info">
                  <h3 className="agent-name">{agent.name}</h3>
                  <p className="agent-description">{agent.description}</p>
                  <div className="agent-meta">
                    <span className="agent-version">v{agent.version}</span>
                    <span className="agent-category">{category?.name || 'Unknown'}</span>
                    <span className="agent-rating">
                      ⭐ {agent.rating?.toFixed(1) || 'N/A'}
                    </span>
                  </div>
                </div>
              </div>

              {benchmark && (
                <div className="agent-benchmarks">
                  <div className="benchmark-metric">
                    <span className="metric-label">Speed:</span>
                    <span className="metric-value">{benchmark.speed.toFixed(0)}ms</span>
                  </div>
                  <div className="benchmark-metric">
                    <span className="metric-label">Accuracy:</span>
                    <span className="metric-value">{(benchmark.accuracy * 100).toFixed(1)}%</span>
                  </div>
                  <div className="benchmark-metric">
                    <span className="metric-label">Confidence:</span>
                    <span className="metric-value">{(benchmark.confidence * 100).toFixed(1)}%</span>
                  </div>
                  <div className="benchmark-metric">
                    <span className="metric-label">Reliability:</span>
                    <span className="metric-value">{(benchmark.reliability * 100).toFixed(1)}%</span>
                  </div>
                </div>
              )}

              <div className="agent-actions">
                {installation ? (
                  <div className="installation-status">
                    {installation.status === 'installing' && (
                      <div className="progress-bar">
                        <div 
                          className="progress-fill" 
                          style={{ width: `${installation.progress}%` }}
                        />
                        <span className="progress-text">{installation.progress}%</span>
                      </div>
                    )}
                    
                    {installation.status === 'installed' && (
                      <div className="installed-actions">
                        <button
                          onClick={() => handleUpdateAgent(agent.id, agent.version)}
                          className="update-button"
                        >
                          🔄 Update
                        </button>
                        <button
                          onClick={() => handleUninstallAgent(agent.id)}
                          className="uninstall-button"
                        >
                          🗑️ Uninstall
                        </button>
                      </div>
                    )}
                    
                    {installation.status === 'failed' && (
                      <div className="error-status">
                        ❌ Installation failed
                        {installation.error && (
                          <div className="error-message">{installation.error}</div>
                        )}
                      </div>
                    )}
                  </div>
                ) : (
                  <button
                    onClick={() => handleInstallAgent(agent.id, agent.version)}
                    className="install-button"
                  >
                    📥 Install
                  </button>
                )}

                <button
                  onClick={() => handleRunBenchmark(agent.id)}
                  className="benchmark-button"
                  disabled={!benchmark}
                >
                  🏃 Benchmark
                </button>
              </div>

              <div className="agent-tags">
                {agent.capabilities.map(capability => (
                  <span key={capability} className="agent-tag">
                    {capability}
                  </span>
                ))}
              </div>
            </div>
          );
        })}
      </div>

      {sortedAgents.length === 0 && (
        <div className="no-agents">
          <p>No agents found matching your criteria.</p>
          <button onClick={() => {
            setSearchQuery('');
            setSelectedCategory('all');
            setFilter({});
          }}>
            Clear Filters
          </button>
        </div>
      )}
    </div>
  );
}

// Export factory function
export function createAgentMarketplace(registry: AgentForgeRegistry): React.ComponentType<AgentMarketplaceProps> {
  return (props: AgentMarketplaceProps) => <AgentMarketplace {...props} registry={registry} />;
}
