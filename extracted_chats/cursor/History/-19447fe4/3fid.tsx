import React, { useState, useCallback } from 'react';

interface PromptComposerProps {
  onCompose: (prompt: string, agents: string[], tokenBudget: number) => void;
  availableAgents: string[];
  maxTokens?: number;
}

export function PromptComposer({ onCompose, availableAgents, maxTokens = 4000 }: PromptComposerProps) {
  const [prompt, setPrompt] = useState('');
  const [selectedAgents, setSelectedAgents] = useState<string[]>([]);
  const [tokenBudget, setTokenBudget] = useState(1000);
  const [isComposing, setIsComposing] = useState(false);

  const handleAgentToggle = useCallback((agent: string) => {
    setSelectedAgents(prev => 
      prev.includes(agent) 
        ? prev.filter(a => a !== agent)
        : [...prev, agent]
    );
  }, []);

  const handleCompose = useCallback(async () => {
    if (!prompt.trim() || selectedAgents.length === 0) return;

    setIsComposing(true);
    try {
      await onCompose(prompt, selectedAgents, tokenBudget);
      setPrompt('');
      setSelectedAgents([]);
    } catch (error) {
      console.error('Prompt composition error:', error);
    } finally {
      setIsComposing(false);
    }
  }, [prompt, selectedAgents, tokenBudget, onCompose]);

  const estimatedTokens = Math.ceil(prompt.length / 4); // Rough estimation
  const remainingTokens = tokenBudget - estimatedTokens;

  return (
    <div className="k15-prompt-composer">
      <div className="k15-composer-header">
        <h3>✍️ Prompt Composer</h3>
        <div className="k15-composer-status">
          {isComposing ? '🔄 Composing...' : '✅ Ready'}
        </div>
      </div>

      <div className="k15-composer-content">
        <div className="k15-prompt-section">
          <label htmlFor="prompt-input">Prompt:</label>
          <textarea
            id="prompt-input"
            value={prompt}
            onChange={(e) => setPrompt(e.target.value)}
            placeholder="Compose your prompt here..."
            className="k15-prompt-textarea"
            rows={4}
          />
          <div className="k15-token-estimate">
            Estimated tokens: {estimatedTokens} / {tokenBudget}
          </div>
        </div>

        <div className="k15-agents-section">
          <label>Select Agents:</label>
          <div className="k15-agents-grid">
            {availableAgents.map(agent => (
              <div
                key={agent}
                className={`k15-agent-option ${selectedAgents.includes(agent) ? 'selected' : ''}`}
                onClick={() => handleAgentToggle(agent)}
              >
                <input
                  type="checkbox"
                  checked={selectedAgents.includes(agent)}
                  readOnly
                />
                <span className="k15-agent-name">{agent}</span>
              </div>
            ))}
          </div>
        </div>

        <div className="k15-budget-section">
          <label htmlFor="token-budget">Token Budget:</label>
          <input
            id="token-budget"
            type="range"
            min="100"
            max={maxTokens}
            step="100"
            value={tokenBudget}
            onChange={(e) => setTokenBudget(Number(e.target.value))}
            className="k15-budget-slider"
          />
          <div className="k15-budget-display">
            <span>Budget: {tokenBudget} tokens</span>
            <span className={`k15-remaining ${remainingTokens < 0 ? 'over-budget' : ''}`}>
              Remaining: {remainingTokens}
            </span>
          </div>
        </div>

        <button
          onClick={handleCompose}
          disabled={isComposing || !prompt.trim() || selectedAgents.length === 0}
          className="k15-compose-btn"
        >
          {isComposing ? '⏳ Composing...' : '🚀 Compose Prompt'}
        </button>
      </div>
    </div>
  );
}