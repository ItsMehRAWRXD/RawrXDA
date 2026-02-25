import React, { useState, useRef, useEffect } from 'react';

interface PromptInputProps {
  onSend: (prompt: string, agents: string[]) => void;
  disabled?: boolean;
  placeholder?: string;
  className?: string;
}

export function PromptInput({ 
  onSend, 
  disabled = false, 
  placeholder = "Ask BigDaddyG anything...",
  className = ''
}: PromptInputProps) {
  const [input, setInput] = useState('');
  const [selectedAgents, setSelectedAgents] = useState<string[]>(['rawr', 'browser']);
  const [showAgentSelector, setShowAgentSelector] = useState(false);
  const [isComposing, setIsComposing] = useState(false);
  
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Available agents
  const availableAgents = [
    'rawr', 'browser', 'parser', 'optimizer', 'codegen', 'debugger',
    'tester', 'analyzer', 'security', 'performance', 'memory', 'io',
    'network', 'database', 'ui', 'ai', 'ml', 'quantum', 'orchestrator',
    'git', 'collaboration', 'documentation', 'build', 'deployment',
    'devops', 'code-review', 'refactoring', 'compliance', 'accessibility',
    'ux', 'internationalization', 'data-engineer', 'analytics',
    'visualization', 'mobile', 'desktop', 'web', 'game', 'embedded',
    'blockchain', 'scientific', 'monitoring', 'backup', 'migration'
  ];

  // Auto-resize textarea
  useEffect(() => {
    if (textareaRef.current) {
      textareaRef.current.style.height = 'auto';
      textareaRef.current.style.height = `${textareaRef.current.scrollHeight}px`;
    }
  }, [input]);

  // Handle key press
  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey && !isComposing) {
      e.preventDefault();
      handleSend();
    }
  };

  // Handle send
  const handleSend = () => {
    if (!input.trim() || disabled) return;
    
    onSend(input.trim(), selectedAgents);
    setInput('');
  };

  // Handle agent toggle
  const handleAgentToggle = (agentId: string) => {
    setSelectedAgents(prev => 
      prev.includes(agentId) 
        ? prev.filter(id => id !== agentId)
        : [...prev, agentId]
    );
  };

  // Handle suggestion click
  const handleSuggestionClick = (suggestion: string) => {
    setInput(suggestion);
    if (textareaRef.current) {
      textareaRef.current.focus();
    }
  };

  // Quick suggestions
  const suggestions = [
    "Analyze this code for potential issues",
    "Optimize this function for better performance", 
    "Generate unit tests for this component",
    "Explain this algorithm step by step",
    "Refactor this code for better maintainability",
    "Create documentation for this API",
    "Review this code for security vulnerabilities",
    "Suggest improvements for this design pattern"
  ];

  return (
    <div className={`prompt-input ${className}`}>
      {/* Agent Selector */}
      {showAgentSelector && (
        <div className="agent-selector">
          <div className="agent-selector-header">
            <h4>Select Agents</h4>
            <button 
              className="close-btn"
              onClick={() => setShowAgentSelector(false)}
            >
              ×
            </button>
          </div>
          <div className="agent-grid">
            {availableAgents.map(agentId => (
              <button
                key={agentId}
                className={`agent-option ${selectedAgents.includes(agentId) ? 'selected' : ''}`}
                onClick={() => handleAgentToggle(agentId)}
              >
                {agentId}
              </button>
            ))}
          </div>
          <div className="agent-selector-footer">
            <span className="selected-count">
              {selectedAgents.length} agents selected
            </span>
            <button 
              className="apply-btn"
              onClick={() => setShowAgentSelector(false)}
            >
              Apply
            </button>
          </div>
        </div>
      )}

      {/* Quick Suggestions */}
      {!input && (
        <div className="suggestions">
          <div className="suggestions-header">
            <span className="suggestions-label">💡 Quick suggestions:</span>
          </div>
          <div className="suggestion-chips">
            {suggestions.slice(0, 4).map((suggestion, index) => (
              <button
                key={index}
                className="suggestion-chip"
                onClick={() => handleSuggestionClick(suggestion)}
                disabled={disabled}
              >
                {suggestion}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Input Container */}
      <div className="input-container">
        <div className="input-wrapper">
          <textarea
            ref={textareaRef}
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={handleKeyDown}
            onCompositionStart={() => setIsComposing(true)}
            onCompositionEnd={() => setIsComposing(false)}
            placeholder={placeholder}
            disabled={disabled}
            className="prompt-textarea"
            rows={1}
          />
          
          <div className="input-actions">
            <button
              className="agent-selector-btn"
              onClick={() => setShowAgentSelector(!showAgentSelector)}
              disabled={disabled}
              title="Select agents"
            >
              🤖 {selectedAgents.length}
            </button>
            
            <button
              className="send-btn"
              onClick={handleSend}
              disabled={!input.trim() || disabled}
              title="Send message"
            >
              ➤
            </button>
          </div>
        </div>
        
        <div className="input-footer">
          <div className="input-hints">
            <span className="hint">Press Enter to send</span>
            <span className="hint">Shift+Enter for new line</span>
          </div>
          <div className="selected-agents">
            {selectedAgents.length > 0 && (
              <div className="agent-tags">
                {selectedAgents.slice(0, 3).map(agent => (
                  <span key={agent} className="agent-tag">
                    {agent}
                  </span>
                ))}
                {selectedAgents.length > 3 && (
                  <span className="agent-tag more">
                    +{selectedAgents.length - 3}
                  </span>
                )}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
