import React, { useState, useRef, useEffect, useCallback } from 'react';
import { contextWindowManager } from '../memory/ContextWindowManager';
import { configManager } from '../BigDaddyGEngine.config';

interface Message {
  id: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: number;
  tokens?: number;
  confidence?: number;
  agents?: string[];
  metadata?: {
    contextWindow: number;
    memoryUsage: number;
    processingTime: number;
  };
}

interface ChatInstantProps {
  engine: any;
  className?: string;
  maxMessages?: number;
  enableMemoryOptimization?: boolean;
  enableAgentSelection?: boolean;
}

export function ChatInstant({ 
  engine, 
  className = '',
  maxMessages = 50,
  enableMemoryOptimization = true,
  enableAgentSelection = true
}: ChatInstantProps) {
  const [messages, setMessages] = useState<Message[]>([]);
  const [input, setInput] = useState('');
  const [isStreaming, setIsStreaming] = useState(false);
  const [selectedAgents, setSelectedAgents] = useState<string[]>(['rawr', 'browser']);
  const [streamingMessage, setStreamingMessage] = useState<string>('');
  const [contextHistory, setContextHistory] = useState<string[]>([]);
  
  const endRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);
  const streamingRef = useRef<boolean>(false);

  // Auto-scroll to bottom when new messages arrive
  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, streamingMessage]);

  // Focus input on mount
  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  // Memory optimization: trim old messages when context gets too large
  useEffect(() => {
    if (!enableMemoryOptimization || messages.length <= maxMessages) return;

    const totalTokens = messages.reduce((sum, msg) => sum + (msg.tokens || 0), 0);
    const contextState = contextWindowManager.getCurrentState();
    
    if (totalTokens > contextState.currentTokens * 0.8) {
      // Keep only the most recent messages
      const keepCount = Math.floor(maxMessages * 0.7);
      setMessages(prev => prev.slice(-keepCount));
      console.log('🧠 Memory optimization: trimmed old messages');
    }
  }, [messages, maxMessages, enableMemoryOptimization]);

  // Generate unique message ID
  const generateMessageId = (): string => {
    return `msg_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  };

  // Estimate token count for a message
  const estimateTokens = (content: string): number => {
    // Rough estimation: ~4 characters per token
    return Math.ceil(content.length / 4);
  };

  // Handle sending a message
  const handleSend = useCallback(async () => {
    if (!input.trim() || isStreaming) return;

    const userMessage: Message = {
      id: generateMessageId(),
      role: 'user',
      content: input.trim(),
      timestamp: Date.now(),
      tokens: estimateTokens(input.trim())
    };

    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setIsStreaming(true);
    streamingRef.current = true;

    try {
      // Update context history for memory optimization
      setContextHistory(prev => [...prev, input.trim()].slice(-10)); // Keep last 10 messages

      // Start streaming response
      const startTime = performance.now();
      let buffer = '';
      let tokenCount = 0;
      let confidence = 0;

      // Create initial assistant message
      const assistantMessage: Message = {
        id: generateMessageId(),
        role: 'assistant',
        content: '',
        timestamp: Date.now(),
        agents: selectedAgents,
        metadata: {
          contextWindow: contextWindowManager.getCurrentState().currentTokens,
          memoryUsage: contextWindowManager.getCurrentState().memoryUsageMB,
          processingTime: 0
        }
      };

      setMessages(prev => [...prev, assistantMessage]);

      // Stream from engine
      const stream = engine.streamChat({ 
        prompt: input.trim(),
        agents: selectedAgents,
        context: contextHistory.slice(-5) // Last 5 messages for context
      });

      for await (const chunk of stream) {
        if (!streamingRef.current) break; // Handle abort

        buffer += chunk;
        tokenCount += estimateTokens(chunk);
        confidence = Math.min(0.95, confidence + 0.01); // Simulate increasing confidence

        setStreamingMessage(buffer);
        
        // Update the assistant message in real-time
        setMessages(prev => prev.map(msg => 
          msg.id === assistantMessage.id 
            ? { 
                ...msg, 
                content: buffer, 
                tokens: tokenCount,
                confidence,
                metadata: {
                  ...msg.metadata!,
                  processingTime: performance.now() - startTime
                }
              }
            : msg
        ));

        // Check memory pressure during streaming
        if (enableMemoryOptimization) {
          const currentMemory = contextWindowManager.getCurrentState().memoryUsageMB;
          if (currentMemory > 400) { // 400MB threshold
            console.log('⚠️ Memory pressure detected during streaming');
            // Could implement chunking or compression here
          }
        }
      }

      // Finalize the message
      setMessages(prev => prev.map(msg => 
        msg.id === assistantMessage.id 
          ? { ...msg, content: buffer, tokens: tokenCount, confidence }
          : msg
      ));

      setStreamingMessage('');

    } catch (error) {
      console.error('❌ Chat streaming failed:', error);
      
      // Add error message
      const errorMessage: Message = {
        id: generateMessageId(),
        role: 'system',
        content: `Error: ${error instanceof Error ? error.message : 'Unknown error'}`,
        timestamp: Date.now()
      };
      
      setMessages(prev => [...prev, errorMessage]);
      setStreamingMessage('');
    } finally {
      setIsStreaming(false);
      streamingRef.current = false;
    }
  }, [input, isStreaming, selectedAgents, contextHistory, enableMemoryOptimization, engine]);

  // Handle key press
  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  // Handle abort streaming
  const handleAbort = () => {
    streamingRef.current = false;
    setIsStreaming(false);
    setStreamingMessage('');
    console.log('🛑 Chat streaming aborted');
  };

  // Handle agent selection
  const handleAgentToggle = (agentId: string) => {
    setSelectedAgents(prev => 
      prev.includes(agentId) 
        ? prev.filter(id => id !== agentId)
        : [...prev, agentId]
    );
  };

  // Get available agents from config
  const availableAgents = configManager.getConfig().agents.enabled;

  return (
    <div className={`chat-instant ${className}`}>
      {/* Header */}
      <div className="chat-header">
        <div className="chat-title">
          <span className="chat-icon">💬</span>
          <span className="chat-name">BigDaddyG Chat</span>
        </div>
        <div className="chat-status">
          {isStreaming ? (
            <div className="streaming-indicator">
              <span className="streaming-dot"></span>
              <span>Streaming...</span>
            </div>
          ) : (
            <div className="ready-indicator">
              <span className="ready-dot"></span>
              <span>Ready</span>
            </div>
          )}
        </div>
      </div>

      {/* Agent Selection */}
      {enableAgentSelection && (
        <div className="agent-selection">
          <div className="agent-selection-label">Active Agents:</div>
          <div className="agent-buttons">
            {availableAgents.map(agentId => (
              <button
                key={agentId}
                className={`agent-btn ${selectedAgents.includes(agentId) ? 'active' : ''}`}
                onClick={() => handleAgentToggle(agentId)}
                disabled={isStreaming}
              >
                {agentId}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Messages */}
      <div className="chat-window">
        {messages.length === 0 && (
          <div className="welcome-message">
            <div className="welcome-icon">🧠</div>
            <div className="welcome-text">
              <h3>Welcome to BigDaddyG Chat</h3>
              <p>Ask me to analyze code, optimize performance, generate tests, or help with any development task.</p>
              <div className="suggestion-chips">
                <button 
                  className="suggestion-chip"
                  onClick={() => setInput('Analyze this code for potential issues')}
                >
                  🔍 Analyze Code
                </button>
                <button 
                  className="suggestion-chip"
                  onClick={() => setInput('Optimize this function for better performance')}
                >
                  ⚡ Optimize Performance
                </button>
                <button 
                  className="suggestion-chip"
                  onClick={() => setInput('Generate unit tests for this component')}
                >
                  🧪 Generate Tests
                </button>
                <button 
                  className="suggestion-chip"
                  onClick={() => setInput('Explain this algorithm step by step')}
                >
                  📚 Explain Algorithm
                </button>
              </div>
            </div>
          </div>
        )}

        {messages.map((message) => (
          <div key={message.id} className={`message ${message.role}`}>
            <div className="message-header">
              <span className="message-role">
                {message.role === 'user' ? '👤 You' : 
                 message.role === 'assistant' ? '🧠 BigDaddyG' : 
                 '⚙️ System'}
              </span>
              <span className="message-time">
                {new Date(message.timestamp).toLocaleTimeString()}
              </span>
              {message.tokens && (
                <span className="message-tokens">
                  {message.tokens} tokens
                </span>
              )}
              {message.confidence && (
                <span className="message-confidence">
                  {(message.confidence * 100).toFixed(1)}% confidence
                </span>
              )}
            </div>
            <div className="message-content">
              {message.content}
            </div>
            {message.agents && (
              <div className="message-agents">
                Agents: {message.agents.join(', ')}
              </div>
            )}
            {message.metadata && (
              <div className="message-metadata">
                <span>Context: {message.metadata.contextWindow.toLocaleString()} tokens</span>
                <span>Memory: {message.metadata.memoryUsage.toFixed(1)}MB</span>
                <span>Time: {message.metadata.processingTime.toFixed(0)}ms</span>
              </div>
            )}
          </div>
        ))}

        {/* Streaming message */}
        {streamingMessage && (
          <div className="message assistant streaming">
            <div className="message-header">
              <span className="message-role">🧠 BigDaddyG</span>
              <span className="streaming-indicator">🔄 Streaming...</span>
            </div>
            <div className="message-content">
              {streamingMessage}
              <span className="streaming-cursor">|</span>
            </div>
          </div>
        )}

        <div ref={endRef} />
      </div>

      {/* Input */}
      <div className="chat-input">
        <div className="input-container">
          <input
            ref={inputRef}
            type="text"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={handleKeyPress}
            placeholder="Ask BigDaddyG anything..."
            disabled={isStreaming}
            className="chat-input-field"
          />
          <div className="input-actions">
            {isStreaming ? (
              <button 
                className="action-btn abort-btn"
                onClick={handleAbort}
                title="Stop streaming"
              >
                ⏹️
              </button>
            ) : (
              <button 
                className="action-btn send-btn"
                onClick={handleSend}
                disabled={!input.trim()}
                title="Send message"
              >
                ➤
              </button>
            )}
          </div>
        </div>
        
        <div className="input-footer">
          <div className="input-hint">
            Press Enter to send, Shift+Enter for new line
          </div>
          <div className="context-info">
            Context: {contextWindowManager.getCurrentState().currentTokens.toLocaleString()} tokens
          </div>
        </div>
      </div>
    </div>
  );
}
