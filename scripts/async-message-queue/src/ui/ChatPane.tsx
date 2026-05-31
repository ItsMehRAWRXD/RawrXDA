import React, { useEffect, useState, useRef, useCallback, useMemo } from 'react';
import { MessageQueue, QueuedMessage, MessagePriority, MessageStatus } from '../queue/message-queue';
import { ConfigManager } from '../config';
import './ChatPane.css';

interface ChatPaneProps {
  queue: MessageQueue;
  onMessageSelect?: (message: QueuedMessage) => void;
  autoFocus?: boolean;
  maxHeight?: string;
}

export const ChatPane: React.FC<ChatPaneProps> = ({
  queue,
  onMessageSelect,
  autoFocus = true,
  maxHeight = '60vh',
}) => {
  const [messages, setMessages] = useState<QueuedMessage[]>([]);
  const [inputValue, setInputValue] = useState('');
  const [isPaused, setIsPaused] = useState(false);
  const [priority, setPriority] = useState<MessagePriority>(MessagePriority.NORMAL);
  const [selectedContext, setSelectedContext] = useState<any>(null);

  const inputRef = useRef<HTMLInputElement>(null);
  const listRef = useRef<HTMLDivElement>(null);
  const config = ConfigManager.getInstance().get();

  const handleQueued = useCallback((msg: QueuedMessage) => {
    setMessages(prev => [...prev, msg]);
  }, []);

  const handleProgress = useCallback((msg: QueuedMessage) => {
    setMessages(prev => prev.map(m => m.id === msg.id ? { ...msg } : m));
  }, []);

  const handleChunk = useCallback((msg: QueuedMessage, chunk: string) => {
    setMessages(prev => prev.map(m => {
      if (m.id === msg.id) {
        return {
          ...msg,
          response: (m.response || '') + chunk,
          progress: Math.min(99, msg.progress + 1),
        };
      }
      return m;
    }));
  }, []);

  const handleCompleted = useCallback((msg: QueuedMessage) => {
    setMessages(prev => prev.map(m =>
      m.id === msg.id ? { ...msg, status: MessageStatus.COMPLETED, progress: 100 } : m
    ));
    playNotification();
  }, []);

  const handleFailed = useCallback((msg: QueuedMessage, error: Error) => {
    setMessages(prev => prev.map(m =>
      m.id === msg.id ? { ...msg, status: MessageStatus.FAILED, error: {
        code: 'ERROR',
        message: error.message,
        retryable: false
      }} : m
    ));
  }, []);

  useEffect(() => {
    queue.on('message:queued', handleQueued);
    queue.on('message:progress', handleProgress);
    queue.on('message:chunk', handleChunk);
    queue.on('message:completed', handleCompleted);
    queue.on('message:failed', handleFailed);

    return () => {
      queue.off('message:queued', handleQueued);
      queue.off('message:progress', handleProgress);
      queue.off('message:chunk', handleChunk);
      queue.off('message:completed', handleCompleted);
      queue.off('message:failed', handleFailed);
    };
  }, [queue, handleQueued, handleProgress, handleChunk, handleCompleted, handleFailed]);

  useEffect(() => {
    if (listRef.current) {
      listRef.current.scrollTop = listRef.current.scrollHeight;
    }
  }, [messages]);

  const handleSend = useCallback(() => {
    const content = inputValue.trim();
    if (!content) return;

    const context = selectedContext || getCurrentContext();

    queue.enqueue(content, priority, context);
    setInputValue('');

    if (autoFocus && config.ui.autoFocusEditor) {
      returnFocusToEditor();
    }
  }, [inputValue, priority, selectedContext, queue, autoFocus, config]);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.shiftKey && e.key === 'C') {
        e.preventDefault();
        inputRef.current?.focus();
      }

      if ((e.metaKey || e.ctrlKey) && e.shiftKey && e.key === 'Enter') {
        e.preventDefault();
        const selection = window.getSelection()?.toString();
        if (selection) {
          queue.enqueue(selection, MessagePriority.URGENT, getCurrentContext());
        }
      }

      if (e.key === 'Escape') {
        inputRef.current?.blur();
        returnFocusToEditor();
      }
    };

    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [queue]);

  const backlog = useMemo(() =>
    messages.filter(m =>
      m.status === MessageStatus.QUEUED || m.status === MessageStatus.PENDING
    ),
    [messages]
  );

  const active = useMemo(() =>
    messages.filter(m =>
      m.status === MessageStatus.PROCESSING || m.status === MessageStatus.STREAMING
    ),
    [messages]
  );

  const completed = useMemo(() =>
    messages.filter(m => m.status === MessageStatus.COMPLETED),
    [messages]
  );

  return (
    <div className="chat-pane" data-testid="chat-pane">
      <div className="chat-header">
        <div className="queue-stats">
          <span className="stat pending" title="Pending">
            ⏳ {backlog.length}
          </span>
          <span className="stat active" title="Active">
            ⚡ {active.length}
          </span>
          <span className="stat completed" title="Completed">
            ✓ {completed.length}
          </span>
        </div>

        <div className="controls">
          <button
            className={`pause-btn ${isPaused ? 'paused' : ''}`}
            onClick={() => {
              if (isPaused) queue.resume();
              else queue.pause();
              setIsPaused(!isPaused);
            }}
            title={isPaused ? 'Resume' : 'Pause'}
          >
            {isPaused ? '▶️' : '⏸️'}
          </button>

          <button
            className="clear-btn"
            onClick={() => setMessages([])}
            title="Clear completed"
          >
            🗑️
          </button>
        </div>
      </div>

      {backlog.length > 0 && (
        <div className="backlog-indicator" data-testid="backlog-indicator">
          <span className="backlog-count">{backlog.length}</span>
          <span> messages queued</span>
          {backlog.length > 5 && (
            <button
              className="prioritize-all-btn"
              onClick={() => backlog.forEach(m => queue.prioritize(m.id, MessagePriority.URGENT))}
            >
              ⚡ Prioritize all
            </button>
          )}
        </div>
      )}

      <div
        className="message-list"
        ref={listRef}
        style={{ maxHeight }}
        data-testid="message-list"
      >
        {messages.slice(-config.ui.maxVisibleMessages).map(message => (
          <MessageItem
            key={message.id}
            message={message}
            onClick={() => onMessageSelect?.(message)}
            onCancel={() => queue.cancel(message.id)}
            onRetry={() => {
              queue.prioritize(message.id, MessagePriority.HIGH);
              queue.resume();
            }}
          />
        ))}
      </div>

      <div className="input-area">
        <div className="input-row">
          <select
            className="priority-select"
            value={priority}
            onChange={(e) => setPriority(Number(e.target.value))}
            data-testid="priority-select"
          >
            <option value={MessagePriority.LOW}>Low</option>
            <option value={MessagePriority.NORMAL}>Normal</option>
            <option value={MessagePriority.HIGH}>High</option>
            <option value={MessagePriority.URGENT}>⚡ Urgent</option>
          </select>

          <input
            ref={inputRef}
            type="text"
            className="chat-input"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                handleSend();
              }
            }}
            placeholder="Type message... (Cmd+Shift+Enter for urgent)"
            data-testid="chat-input"
          />

          <button
            className="send-btn"
            onClick={handleSend}
            disabled={!inputValue.trim() || isPaused}
            data-testid="send-btn"
          >
            Send
          </button>
        </div>

        {selectedContext && (
          <div className="context-preview">
            <span className="context-label">Context:</span>
            <span className="context-text">{selectedContext.fileName}</span>
            <button onClick={() => setSelectedContext(null)}>✕</button>
          </div>
        )}
      </div>
    </div>
  );
};

interface MessageItemProps {
  message: QueuedMessage;
  onClick: () => void;
  onCancel: () => void;
  onRetry: () => void;
}

const MessageItem: React.FC<MessageItemProps> = React.memo(({ message, onClick, onCancel, onRetry }) => {
  const statusClass = message.status.toLowerCase();
  const priorityClass = MessagePriority[message.priority].toLowerCase();

  return (
    <div
      className={`message ${statusClass} ${priorityClass}`}
      onClick={onClick}
      data-testid={`message-${message.id}`}
    >
      <div className="message-user">
        <div className="message-header">
          <span className="priority-indicator">
            {message.priority >= MessagePriority.URGENT && '⚡'}
            {message.priority === MessagePriority.HIGH && '↑'}
          </span>

          <span className="message-time">
            {new Date(message.timestamps.created).toLocaleTimeString()}
          </span>

          {message.status === MessageStatus.QUEUED && (
            <button className="cancel-btn" onClick={(e) => { e.stopPropagation(); onCancel(); }}>
              ✕
            </button>
          )}
        </div>

        <div className="message-content">
          {escapeHtml(message.content)}
        </div>
      </div>

      {(message.response || message.status === MessageStatus.STREAMING) && (
        <div className="message-response">
          {message.status === MessageStatus.STREAMING && message.progress < 100 && (
            <div className="progress-bar">
              <div
                className="progress-fill"
                style={{ width: `${message.progress}%` }}
              />
            </div>
          )}

          <div className="response-content">
            {message.response || ''}
          </div>
        </div>
      )}

      {message.status === MessageStatus.FAILED && message.error && (
        <div className="message-error">
          <span className="error-message">{message.error.message}</span>
          {message.error.retryable && (
            <button className="retry-btn" onClick={(e) => { e.stopPropagation(); onRetry(); }}>
              Retry
            </button>
          )}
        </div>
      )}

      {message.context?.fileName && (
        <div className="message-context">
          <span className="context-file">{message.context.fileName}</span>
          {message.context.selection && (
            <span className="context-lines">
              L{message.context.selection.startLine}-{message.context.selection.endLine}
            </span>
          )}
        </div>
      )}
    </div>
  );
});

function escapeHtml(text: string): string {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function getCurrentContext(): any {
  const selection = window.getSelection();
  const activeElement = document.activeElement;

  return {
    selection: selection?.toString() ? {
      text: selection.toString(),
      startLine: 0,
      endLine: 0,
    } : undefined,
    fileName: (activeElement as any)?.dataset?.file || undefined,
    language: (activeElement as any)?.dataset?.language || undefined,
  };
}

function returnFocusToEditor(): void {
  const editor = document.querySelector('[data-editor="true"]') as HTMLElement;
  editor?.focus();
}

function playNotification(): void {
  const config = ConfigManager.getInstance().get();
  if (!config.ui.notificationSound) return;

  try {
    const audio = new Audio('/sounds/notification.mp3');
    audio.volume = 0.1;
    audio.play().catch(() => {});
  } catch (error) {
    // Ignore
  }
}

export default ChatPane;
