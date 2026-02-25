import React, { useState, useRef, useEffect, useCallback } from 'react';
import { configManager } from '../BigDaddyGEngine.config';

interface Agent {
  id: string;
  name: string;
  description: string;
  capabilities: string[];
  isActive: boolean;
  tokenBudget?: number;
}

interface PromptInputProps {
  onSend: (prompt: string, agents: string[]) => Promise<void>;
  disabled?: boolean;
  placeholder?: string;
  enableAgentSelection?: boolean;
  enableTokenBudgeting?: boolean;
  maxTokens?: number;
  className?: string;
}

export function PromptInput({
  onSend,
  disabled = false,
  placeholder = "Ask BigDaddyG anything...",
  enableAgentSelection = true,
  enableTokenBudgeting = true,
  maxTokens = 4000,
  className = ''
}: PromptInputProps) {
  const [input, setInput] = useState('');
  const [selectedAgents, setSelectedAgents] = useState<string[]>(['rawr', 'browser']);
  const [tokenBudget, setTokenBudget] = useState(maxTokens);
  const [isComposing, setIsComposing] = useState(false);
  const [showAgentPanel, setShowAgentPanel] = useState(false);
  const [estimatedTokens, setEstimatedTokens] = useState(0);
  
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const compositionRef = useRef<boolean>(false);

  // Get available agents from config
  const availableAgents: Agent[] = configManager.getConfig().agents.enabled.map(agentId => ({
    id: agentId,
    name: agentId.charAt(0).toUpperCase() + agentId.slice(1),
    description: `AI agent specialized in ${agentId} tasks`,
    capabilities: getAgentCapabilities(agentId),
    isActive: selectedAgents.includes(agentId),
    tokenBudget: Math.floor(maxTokens / selectedAgents.length)
  }));

  // Get agent capabilities based on ID
  function getAgentCapabilities(agentId: string): string[] {
    const capabilities: { [key: string]: string[] } = {
      'rawr': ['Code Analysis', 'Pattern Recognition', 'Debugging'],
      'browser': ['Web Automation', 'DOM Manipulation', 'API Testing'],
      'analyzer': ['Code Quality', 'Performance Analysis', 'Security Audit'],
      'optimizer': ['Performance Optimization', 'Memory Management', 'Algorithm Improvement'],
      'tester': ['Test Generation', 'Coverage Analysis', 'Quality Assurance'],
      'security': ['Vulnerability Detection', 'Security Scanning', 'Compliance Check'],
      'codegen': ['Code Generation', 'Template Creation', 'Boilerplate'],
      'performance': ['Benchmarking', 'Profiling', 'Resource Monitoring']
    };
    return capabilities[agentId] || ['General AI Tasks'];
  }

  // Estimate token count for input
  const estimateTokens = useCallback((text: string): number => {
    // Rough estimation: ~4 characters per token
    return Math.ceil(text.length / 4);
  }, []);

  // Update estimated tokens when input changes
  useEffect(() => {
    const tokens = estimateTokens(input);
    setEstimatedTokens(tokens);
  }, [input, estimateTokens]);

  // Handle input change
  const handleInputChange = (e: React.ChangeEvent<HTMLTextAreaElement>) => {
    setInput(e.target.value);
  };

  // Handle composition start/end for IME
  const handleCompositionStart = () => {
    compositionRef.current = true;
    setIsComposing(true);
  };

  const handleCompositionEnd = () => {
    compositionRef.current = false;
    setIsComposing(false);
  };

  // Handle key press
  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey && !compositionRef.current) {
      e.preventDefault();
      handleSend();
    } else if (e.key === 'Enter' && e.shiftKey) {
      // Allow new line with Shift+Enter
      return;
    }
  };

  // Handle send
  const handleSend = useCallback(async () => {
    if (!input.trim() || disabled || isComposing) return;

    const prompt = input.trim();
    const agents = selectedAgents.length > 0 ? selectedAgents : ['rawr'];

    try {
      await onSend(prompt, agents);
      setInput('');
      setEstimatedTokens(0);
    } catch (error) {
      console.error('❌ Failed to send prompt:', error);
    }
  }, [input, disabled, isComposing, selectedAgents, onSend]);

  // Handle agent selection
  const handleAgentToggle = (agentId: string) => {
    setSelectedAgents(prev => {
      const newSelection = prev.includes(agentId)
        ? prev.filter(id => id !== agentId)
        : [...prev, agentId];
      
      // Update token budget per agent
      if (enableTokenBudgeting) {
        setTokenBudget(Math.floor(maxTokens / newSelection.length));
      }
      
      return newSelection;
    });
  };

  // Handle agent panel toggle
  const toggleAgentPanel = () => {
    setShowAgentPanel(prev => !prev);
  };

  // Auto-resize textarea
  useEffect(() => {
    if (inputRef.current) {
      inputRef.current.style.height = 'auto';
      inputRef.current.style.height = `${inputRef.current.scrollHeight}px`;
    }
  }, [input]);

  // Focus input on mount
  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  const canSend = input.trim().length > 0 && !disabled && !isComposing;
  const isOverTokenLimit = estimatedTokens > tokenBudget;

  return (
    <div className={`k15-prompt-input ${className}`}>
      {/* Agent Selection Panel */}
      {enableAgentSelection && (
        <div className="k15-agent-selection">
          <div className="k15-agent-selection-header">
            <div className="k15-agent-selection-label">
              Active Agents ({selectedAgents.length})
            </div>
            <button
              className="k15-agent-toggle-btn"
              onClick={toggleAgentPanel}
              title="Toggle agent selection"
            >
              {showAgentPanel ? '▼' : '▶'}
            </button>
          </div>
          
          {showAgentPanel && (
            <div className="k15-agent-panel k15-fade-in">
              <div className="k15-agent-buttons">
                {availableAgents.map(agent => (
                  <button
                    key={agent.id}
                    className={`k15-agent-btn ${agent.isActive ? 'active' : ''}`}
                    onClick={() => handleAgentToggle(agent.id)}
                    disabled={disabled}
                    title={agent.description}
                  >
                    <span className="k15-agent-name">{agent.name}</span>
                    <span className="k15-agent-capabilities">
                      {agent.capabilities.slice(0, 2).join(', ')}
                    </span>
                  </button>
                ))}
              </div>
              
              {enableTokenBudgeting && (
                <div className="k15-token-budget">
                  <div className="k15-token-budget-label">
                    Token Budget: {tokenBudget.toLocaleString()} per agent
                  </div>
                  <div className="k15-token-budget-bar">
                    <div 
                      className="k15-token-budget-fill"
                      style={{ 
                        width: `${Math.min(100, (estimatedTokens / tokenBudget) * 100)}%`,
                        backgroundColor: isOverTokenLimit ? 'var(--fluent-error)' : 'var(--fluent-primary)'
                      }}
                    />
                  </div>
                  <div className="k15-token-budget-info">
                    {estimatedTokens.toLocaleString()} / {tokenBudget.toLocaleString()} tokens
                    {isOverTokenLimit && (
                      <span className="k15-token-warning">⚠️ Over budget</span>
                    )}
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      )}

      {/* Input Container */}
      <div className="k15-input-container">
        <textarea
          ref={inputRef}
          value={input}
          onChange={handleInputChange}
          onKeyDown={handleKeyDown}
          onCompositionStart={handleCompositionStart}
          onCompositionEnd={handleCompositionEnd}
          placeholder={placeholder}
          disabled={disabled}
          className="k15-input-field"
          rows={1}
          style={{ minHeight: '20px', maxHeight: '120px' }}
        />
        
        <div className="k15-input-actions">
          {canSend ? (
            <button
              className="k15-action-btn k15-send-btn"
              onClick={handleSend}
              disabled={disabled}
              title="Send message (Enter)"
            >
              ➤
            </button>
          ) : (
            <button
              className="k15-action-btn k15-send-btn"
              disabled
              title="Enter a message to send"
            >
              ➤
            </button>
          )}
        </div>
      </div>

      {/* Input Footer */}
      <div className="k15-input-footer">
        <div className="k15-input-hint">
          Press Enter to send, Shift+Enter for new line
        </div>
        
        <div className="k15-input-stats">
          <span className="k15-token-count">
            {estimatedTokens.toLocaleString()} tokens
          </span>
          {isOverTokenLimit && (
            <span className="k15-token-warning">
              ⚠️ Over budget
            </span>
          )}
        </div>
      </div>

      {/* Quick Suggestions */}
      {input.length === 0 && (
        <div className="k15-suggestions">
          <div className="k15-suggestions-label">Quick suggestions:</div>
          <div className="k15-suggestion-chips">
            <button
              className="k15-suggestion-chip"
              onClick={() => setInput('Analyze this code for potential issues')}
              disabled={disabled}
            >
              🔍 Analyze Code
            </button>
            <button
              className="k15-suggestion-chip"
              onClick={() => setInput('Optimize this function for better performance')}
              disabled={disabled}
            >
              ⚡ Optimize Performance
            </button>
            <button
              className="k15-suggestion-chip"
              onClick={() => setInput('Generate unit tests for this component')}
              disabled={disabled}
            >
              🧪 Generate Tests
            </button>
            <button
              className="k15-suggestion-chip"
              onClick={() => setInput('Explain this algorithm step by step')}
              disabled={disabled}
            >
              📚 Explain Algorithm
            </button>
          </div>
        </div>
      )}
    </div>
  );
}