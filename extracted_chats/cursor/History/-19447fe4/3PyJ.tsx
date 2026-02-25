// PromptComposer.tsx - Advanced prompt composition with agent selection and token budgeting
// Production-grade input panel for BigDaddyG-Engine orchestration

import React, { useState, useEffect, useRef, useCallback } from 'react';
import { ContextWindowManager } from '../memory/ContextWindowManager';
import { BigDaddyGConfigManager } from '../BigDaddyGEngine.config';
import { useIndexedDB } from '../hooks/useIndexedDB';
import './prompt-composer.css';

export interface AgentOption {
  id: string;
  name: string;
  description: string;
  category: 'core' | 'analysis' | 'generation' | 'optimization' | 'testing' | 'deployment';
  capabilities: string[];
  estimatedTokens: number;
  confidence: number;
  isActive: boolean;
}

export interface PromptHistory {
  id: string;
  prompt: string;
  agents: string[];
  timestamp: number;
  tokens: number;
  result?: string;
  success: boolean;
}

export interface TokenBudget {
  total: number;
  used: number;
  remaining: number;
  perAgent: { [agentId: string]: number };
  estimated: number;
}

export interface PromptComposerProps {
  engine?: any;
  contextWindowManager?: ContextWindowManager;
  configManager?: BigDaddyGConfigManager;
  onPromptSubmit: (prompt: string, agents: string[], options: PromptOptions) => void;
  onPromptHistory?: (history: PromptHistory[]) => void;
  enableAgentSelection?: boolean;
  enableTokenBudgeting?: boolean;
  enableContextWindow?: boolean;
  enableHistory?: boolean;
  maxAgents?: number;
  defaultContextWindow?: number;
}

export interface PromptOptions {
  contextWindow: number;
  maxTokens: number;
  enableWebGPU: boolean;
  enableTokenIntrospection: boolean;
  enableStreaming: boolean;
  priority: 'low' | 'normal' | 'high';
  timeout: number;
}

const DEFAULT_AGENTS: AgentOption[] = [
  {
    id: 'lexer',
    name: 'Lexer',
    description: 'Tokenizes and parses input text',
    category: 'core',
    capabilities: ['tokenization', 'parsing', 'syntax-analysis'],
    estimatedTokens: 100,
    confidence: 0.95,
    isActive: true
  },
  {
    id: 'parser',
    name: 'Parser',
    description: 'Analyzes syntax and structure',
    category: 'core',
    capabilities: ['syntax-analysis', 'ast-generation', 'error-detection'],
    estimatedTokens: 150,
    confidence: 0.92,
    isActive: true
  },
  {
    id: 'semantic',
    name: 'Semantic Analyzer',
    description: 'Performs semantic analysis and type checking',
    category: 'analysis',
    capabilities: ['type-checking', 'semantic-analysis', 'symbol-resolution'],
    estimatedTokens: 200,
    confidence: 0.88,
    isActive: true
  },
  {
    id: 'optimizer',
    name: 'Optimizer',
    description: 'Optimizes code and performance',
    category: 'optimization',
    capabilities: ['code-optimization', 'performance-analysis', 'dead-code-elimination'],
    estimatedTokens: 180,
    confidence: 0.90,
    isActive: true
  },
  {
    id: 'codegen',
    name: 'Code Generator',
    description: 'Generates executable code',
    category: 'generation',
    capabilities: ['code-generation', 'compilation', 'transpilation'],
    estimatedTokens: 300,
    confidence: 0.85,
    isActive: true
  },
  {
    id: 'debugger',
    name: 'Debugger',
    description: 'Identifies and fixes bugs',
    category: 'testing',
    capabilities: ['debugging', 'error-analysis', 'stack-trace-analysis'],
    estimatedTokens: 250,
    confidence: 0.87,
    isActive: true
  },
  {
    id: 'tester',
    name: 'Tester',
    description: 'Creates and runs tests',
    category: 'testing',
    capabilities: ['test-generation', 'test-execution', 'coverage-analysis'],
    estimatedTokens: 220,
    confidence: 0.89,
    isActive: true
  },
  {
    id: 'analyzer',
    name: 'Static Analyzer',
    description: 'Performs static code analysis',
    category: 'analysis',
    capabilities: ['static-analysis', 'vulnerability-detection', 'code-quality'],
    estimatedTokens: 280,
    confidence: 0.83,
    isActive: true
  }
];

export function PromptComposer({
  engine,
  contextWindowManager,
  configManager,
  onPromptSubmit,
  onPromptHistory,
  enableAgentSelection = true,
  enableTokenBudgeting = true,
  enableContextWindow = true,
  enableHistory = true,
  maxAgents = 5,
  defaultContextWindow = 4096
}: PromptComposerProps) {
  const [prompt, setPrompt] = useState('');
  const [selectedAgents, setSelectedAgents] = useState<string[]>([]);
  const [availableAgents, setAvailableAgents] = useState<AgentOption[]>(DEFAULT_AGENTS);
  const [promptHistory, setPromptHistory] = useState<PromptHistory[]>([]);
  const [tokenBudget, setTokenBudget] = useState<TokenBudget>({
    total: defaultContextWindow,
    used: 0,
    remaining: defaultContextWindow,
    perAgent: {},
    estimated: 0
  });
  const [contextWindow, setContextWindow] = useState(defaultContextWindow);
  const [promptOptions, setPromptOptions] = useState<PromptOptions>({
    contextWindow: defaultContextWindow,
    maxTokens: 2048,
    enableWebGPU: true,
    enableTokenIntrospection: true,
    enableStreaming: true,
    priority: 'normal',
    timeout: 30000
  });
  const [isComposing, setIsComposing] = useState(false);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [agentFilter, setAgentFilter] = useState<string>('all');
  const [searchQuery, setSearchQuery] = useState('');

  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const { addMessage, getMessages, createSession } = useIndexedDB();

  // Initialize with context window manager
  useEffect(() => {
    if (contextWindowManager) {
      const windowSize = contextWindowManager.getWindowSize();
      setContextWindow(windowSize);
      setTokenBudget(prev => ({
        ...prev,
        total: windowSize,
        remaining: windowSize - prev.used
      }));
    }
  }, [contextWindowManager]);

  // Load prompt history
  useEffect(() => {
    if (enableHistory) {
      loadPromptHistory();
    }
  }, [enableHistory]);

  // Auto-resize textarea
  useEffect(() => {
    if (textareaRef.current) {
      textareaRef.current.style.height = 'auto';
      textareaRef.current.style.height = textareaRef.current.scrollHeight + 'px';
    }
  }, [prompt]);

  // Calculate token budget
  useEffect(() => {
    calculateTokenBudget();
  }, [selectedAgents, prompt, contextWindow]);

  const loadPromptHistory = async () => {
    try {
      const messages = await getMessages('current-session');
      const history: PromptHistory[] = messages
        .filter(m => m.role === 'user')
        .map(m => ({
          id: m.id,
          prompt: m.content,
          agents: m.metadata?.agentId ? [m.metadata.agentId] : [],
          timestamp: m.timestamp,
          tokens: m.tokens || 0,
          success: true
        }));
      setPromptHistory(history);
      if (onPromptHistory) {
        onPromptHistory(history);
      }
    } catch (error) {
      console.warn('Failed to load prompt history:', error);
    }
  };

  const calculateTokenBudget = useCallback(() => {
    const promptTokens = prompt.split(' ').length;
    const agentTokens = selectedAgents.reduce((sum, agentId) => {
      const agent = availableAgents.find(a => a.id === agentId);
      return sum + (agent?.estimatedTokens || 0);
    }, 0);
    
    const estimated = promptTokens + agentTokens;
    const used = promptTokens;
    const remaining = contextWindow - used;
    
    const perAgent: { [agentId: string]: number } = {};
    selectedAgents.forEach(agentId => {
      const agent = availableAgents.find(a => a.id === agentId);
      perAgent[agentId] = agent?.estimatedTokens || 0;
    });

    setTokenBudget({
      total: contextWindow,
      used,
      remaining,
      perAgent,
      estimated
    });
  }, [prompt, selectedAgents, availableAgents, contextWindow]);

  const handleAgentToggle = (agentId: string) => {
    setSelectedAgents(prev => {
      if (prev.includes(agentId)) {
        return prev.filter(id => id !== agentId);
      } else if (prev.length < maxAgents) {
        return [...prev, agentId];
      }
      return prev;
    });
  };

  const handleContextWindowChange = (newWindow: number) => {
    setContextWindow(newWindow);
    setPromptOptions(prev => ({ ...prev, contextWindow: newWindow }));
    if (contextWindowManager) {
      contextWindowManager.setMaxTokens(newWindow);
    }
  };

  const handleSubmit = async () => {
    if (!prompt.trim() || isComposing) return;

    setIsComposing(true);
    
    try {
      // Create prompt history entry
      const historyEntry: PromptHistory = {
        id: crypto.randomUUID(),
        prompt: prompt.trim(),
        agents: selectedAgents,
        timestamp: Date.now(),
        tokens: tokenBudget.estimated,
        success: false
      };

      // Store in IndexedDB
      if (enableHistory) {
        const message: any = {
          id: historyEntry.id,
          role: 'user',
          content: prompt.trim(),
          timestamp: historyEntry.timestamp,
          tokens: historyEntry.tokens,
          sessionId: 'current-session',
          metadata: {
            agentId: selectedAgents.join(', '),
            contextWindow,
            estimatedTokens: tokenBudget.estimated
          }
        };
        await addMessage(message);
      }

      // Submit prompt
      onPromptSubmit(prompt.trim(), selectedAgents, promptOptions);

      // Update history
      setPromptHistory(prev => [historyEntry, ...prev.slice(0, 49)]); // Keep last 50
      
      // Clear form
      setPrompt('');
      setSelectedAgents([]);
      
    } catch (error) {
      console.error('Failed to submit prompt:', error);
    } finally {
      setIsComposing(false);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSubmit();
    }
  };

  const loadHistoryPrompt = (historyItem: PromptHistory) => {
    setPrompt(historyItem.prompt);
    setSelectedAgents(historyItem.agents);
  };

  const filteredAgents = availableAgents.filter(agent => {
    const matchesFilter = agentFilter === 'all' || agent.category === agentFilter;
    const matchesSearch = searchQuery === '' || 
      agent.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      agent.description.toLowerCase().includes(searchQuery.toLowerCase());
    return matchesFilter && matchesSearch;
  });

  const getAgentCategoryIcon = (category: string) => {
    const icons: { [key: string]: string } = {
      core: '🔧',
      analysis: '🔍',
      generation: '⚡',
      optimization: '🚀',
      testing: '🧪',
      deployment: '🚢'
    };
    return icons[category] || '🤖';
  };

  return (
    <div className="prompt-composer">
      <div className="composer-header">
        <h3>🎯 Prompt Composer</h3>
        <div className="header-controls">
          <button 
            className="toggle-advanced"
            onClick={() => setShowAdvanced(!showAdvanced)}
            title="Toggle advanced options"
          >
            {showAdvanced ? '▼' : '▶'} Advanced
          </button>
          <button 
            className="clear-button"
            onClick={() => {
              setPrompt('');
              setSelectedAgents([]);
            }}
            title="Clear form"
          >
            🗑️ Clear
          </button>
        </div>
      </div>

      <div className="composer-body">
        {/* Prompt Input */}
        <div className="prompt-section">
          <label className="section-label">
            💬 Prompt
            <span className="token-count">
              {prompt.split(' ').length} tokens
            </span>
          </label>
          <textarea
            ref={textareaRef}
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="Describe what you want BigDaddyG to do..."
            className="prompt-textarea"
            rows={3}
            disabled={isComposing}
          />
        </div>

        {/* Agent Selection */}
        {enableAgentSelection && (
          <div className="agent-section">
            <label className="section-label">
              🤖 Agents ({selectedAgents.length}/{maxAgents})
              <span className="agent-summary">
                {selectedAgents.length > 0 
                  ? selectedAgents.map(id => availableAgents.find(a => a.id === id)?.name).join(', ')
                  : 'None selected'
                }
              </span>
            </label>
            
            <div className="agent-controls">
              <div className="agent-filters">
                <select 
                  value={agentFilter}
                  onChange={(e) => setAgentFilter(e.target.value)}
                  className="category-filter"
                >
                  <option value="all">All Categories</option>
                  <option value="core">Core</option>
                  <option value="analysis">Analysis</option>
                  <option value="generation">Generation</option>
                  <option value="optimization">Optimization</option>
                  <option value="testing">Testing</option>
                  <option value="deployment">Deployment</option>
                </select>
                
                <input
                  type="text"
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  placeholder="Search agents..."
                  className="agent-search"
                />
              </div>
            </div>

            <div className="agent-grid">
              {filteredAgents.map(agent => (
                <div 
                  key={agent.id}
                  className={`agent-card ${selectedAgents.includes(agent.id) ? 'selected' : ''} ${!agent.isActive ? 'inactive' : ''}`}
                  onClick={() => agent.isActive && handleAgentToggle(agent.id)}
                >
                  <div className="agent-header">
                    <span className="agent-icon">
                      {getAgentCategoryIcon(agent.category)}
                    </span>
                    <span className="agent-name">{agent.name}</span>
                    <span className="agent-confidence">
                      {(agent.confidence * 100).toFixed(0)}%
                    </span>
                  </div>
                  <div className="agent-description">{agent.description}</div>
                  <div className="agent-meta">
                    <span className="agent-tokens">~{agent.estimatedTokens} tokens</span>
                    <span className="agent-category">{agent.category}</span>
                  </div>
                  {selectedAgents.includes(agent.id) && (
                    <div className="agent-selected">✓ Selected</div>
                  )}
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Token Budget */}
        {enableTokenBudgeting && (
          <div className="token-budget-section">
            <label className="section-label">
              📊 Token Budget
              <span className="budget-status">
                {tokenBudget.remaining < 0 ? '⚠️ Over budget' : '✅ Within budget'}
              </span>
            </label>
            
            <div className="budget-visualization">
              <div className="budget-bar">
                <div 
                  className="budget-used"
                  style={{ width: `${(tokenBudget.used / tokenBudget.total) * 100}%` }}
                />
                <div 
                  className="budget-estimated"
                  style={{ 
                    width: `${(tokenBudget.estimated / tokenBudget.total) * 100}%`,
                    left: `${(tokenBudget.used / tokenBudget.total) * 100}%`
                  }}
                />
              </div>
              <div className="budget-labels">
                <span>Used: {tokenBudget.used}</span>
                <span>Estimated: {tokenBudget.estimated}</span>
                <span>Total: {tokenBudget.total}</span>
                <span>Remaining: {tokenBudget.remaining}</span>
              </div>
            </div>
          </div>
        )}

        {/* Context Window */}
        {enableContextWindow && (
          <div className="context-window-section">
            <label className="section-label">
              🪟 Context Window
              <span className="window-size">{contextWindow.toLocaleString()} tokens</span>
            </label>
            
            <div className="window-controls">
              <input
                type="range"
                min="1024"
                max="512000"
                step="1024"
                value={contextWindow}
                onChange={(e) => handleContextWindowChange(parseInt(e.target.value))}
                className="window-slider"
              />
              <div className="window-presets">
                <button onClick={() => handleContextWindowChange(4096)}>4K</button>
                <button onClick={() => handleContextWindowChange(8192)}>8K</button>
                <button onClick={() => handleContextWindowChange(16384)}>16K</button>
                <button onClick={() => handleContextWindowChange(32768)}>32K</button>
                <button onClick={() => handleContextWindowChange(65536)}>64K</button>
              </div>
            </div>
          </div>
        )}

        {/* Advanced Options */}
        {showAdvanced && (
          <div className="advanced-section">
            <label className="section-label">⚙️ Advanced Options</label>
            
            <div className="advanced-grid">
              <div className="option-group">
                <label>Max Tokens</label>
                <input
                  type="number"
                  value={promptOptions.maxTokens}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    maxTokens: parseInt(e.target.value) 
                  }))}
                  min="512"
                  max="8192"
                />
              </div>
              
              <div className="option-group">
                <label>Priority</label>
                <select
                  value={promptOptions.priority}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    priority: e.target.value as 'low' | 'normal' | 'high'
                  }))}
                >
                  <option value="low">Low</option>
                  <option value="normal">Normal</option>
                  <option value="high">High</option>
                </select>
              </div>
              
              <div className="option-group">
                <label>Timeout (ms)</label>
                <input
                  type="number"
                  value={promptOptions.timeout}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    timeout: parseInt(e.target.value) 
                  }))}
                  min="5000"
                  max="120000"
                />
              </div>
            </div>
            
            <div className="feature-toggles">
              <label className="toggle">
                <input
                  type="checkbox"
                  checked={promptOptions.enableWebGPU}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    enableWebGPU: e.target.checked 
                  }))}
                />
                <span>WebGPU Acceleration</span>
              </label>
              
              <label className="toggle">
                <input
                  type="checkbox"
                  checked={promptOptions.enableTokenIntrospection}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    enableTokenIntrospection: e.target.checked 
                  }))}
                />
                <span>Token Introspection</span>
              </label>
              
              <label className="toggle">
                <input
                  type="checkbox"
                  checked={promptOptions.enableStreaming}
                  onChange={(e) => setPromptOptions(prev => ({ 
                    ...prev, 
                    enableStreaming: e.target.checked 
                  }))}
                />
                <span>Streaming Response</span>
              </label>
            </div>
          </div>
        )}

        {/* Prompt History */}
        {enableHistory && promptHistory.length > 0 && (
          <div className="history-section">
            <label className="section-label">
              📚 Recent Prompts ({promptHistory.length})
            </label>
            
            <div className="history-list">
              {promptHistory.slice(0, 5).map(item => (
                <div 
                  key={item.id}
                  className="history-item"
                  onClick={() => loadHistoryPrompt(item)}
                >
                  <div className="history-prompt">
                    {item.prompt.substring(0, 100)}
                    {item.prompt.length > 100 && '...'}
                  </div>
                  <div className="history-meta">
                    <span>{new Date(item.timestamp).toLocaleTimeString()}</span>
                    <span>{item.agents.length} agents</span>
                    <span>{item.tokens} tokens</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      <div className="composer-footer">
        <div className="submit-info">
          <span className="agent-count">
            {selectedAgents.length} agent{selectedAgents.length !== 1 ? 's' : ''} selected
          </span>
          <span className="token-estimate">
            ~{tokenBudget.estimated} tokens estimated
          </span>
        </div>
        
        <button 
          className="submit-button"
          onClick={handleSubmit}
          disabled={!prompt.trim() || isComposing || selectedAgents.length === 0}
        >
          {isComposing ? '⏳ Composing...' : '🚀 Submit Prompt'}
        </button>
      </div>
    </div>
  );
}
