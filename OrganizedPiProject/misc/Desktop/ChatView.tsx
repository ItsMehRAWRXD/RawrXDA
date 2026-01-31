// ChatView.tsx - Enhanced chat interface with streaming and markdown support
import React, { useState, useRef, useEffect } from 'react';
import ReactMarkdown from 'react-markdown';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus, vscLightPlus } from 'react-syntax-highlighter/dist/esm/styles/prism';

interface Message {
    id: string;
    type: 'user' | 'agent' | 'system';
    content: string;
    timestamp: Date;
    status?: 'sending' | 'sent' | 'error' | 'streaming';
    isMarkdown?: boolean;
}

interface ChatViewProps {
    messages: Message[];
    onSendMessage: (text: string) => void;
    isLoading: boolean;
    placeholder: string;
    isAskMode?: boolean;
}

const ChatView: React.FC<ChatViewProps> = ({ 
    messages, 
    onSendMessage, 
    isLoading, 
    placeholder,
    isAskMode = false 
}) => {
    const [input, setInput] = useState('');
    const [isStreaming, setIsStreaming] = useState(false);
    const messagesEndRef = useRef<HTMLDivElement>(null);
    const inputRef = useRef<HTMLInputElement>(null);

    // Auto-scroll to bottom when new messages arrive
    useEffect(() => {
        messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }, [messages]);

    // Focus input when component mounts or after sending message
    useEffect(() => {
        if (!isLoading && !isStreaming) {
            inputRef.current?.focus();
        }
    }, [isLoading, isStreaming]);

    const handleSend = async () => {
        if (input.trim() && !isLoading && !isStreaming) {
            const messageText = input.trim();
            setInput('');
            
            // Send message to parent component
            onSendMessage(messageText);
        }
    };

    const handleKeyPress = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter') {
            if (e.shiftKey) {
                // Shift+Enter for new line (if we had textarea)
                return;
            } else {
                e.preventDefault();
                handleSend();
            }
        }
    };

    const formatMessage = (message: Message) => {
        if (message.type === 'user') {
            return (
                <div className={`message user-message ${message.status}`}>
                    <div className="message-header">
                        <span className="message-type">You</span>
                        <span className="message-time">
                            {message.timestamp.toLocaleTimeString()}
                        </span>
                    </div>
                    <div className="message-content">
                        {message.content}
                    </div>
                </div>
            );
        } else if (message.type === 'agent') {
            return (
                <div className={`message agent-message ${message.status}`}>
                    <div className="message-header">
                        <span className="message-type">AI Agent</span>
                        <span className="message-time">
                            {message.timestamp.toLocaleTimeString()}
                        </span>
                        {message.status === 'streaming' && (
                            <span className="streaming-indicator">●</span>
                        )}
                    </div>
                    <div className="message-content">
                        {message.isMarkdown !== false ? (
                            <ReactMarkdown
                                components={{
                                    code({ node, inline, className, children, ...props }) {
                                        const match = /language-(\w+)/.exec(className || '');
                                        const language = match ? match[1] : '';
                                        
                                        if (!inline && language) {
                                            return (
                                                <SyntaxHighlighter
                                                    style={getCodeTheme()}
                                                    language={language}
                                                    PreTag="div"
                                                    customStyle={{
                                                        borderRadius: '6px',
                                                        fontSize: '0.9em',
                                                        margin: '0.5rem 0'
                                                    }}
                                                    {...props}
                                                >
                                                    {String(children).replace(/\n$/, '')}
                                                </SyntaxHighlighter>
                                            );
                                        } else {
                                            return (
                                                <code className={className} {...props}>
                                                    {children}
                                                </code>
                                            );
                                        }
                                    },
                                    p({ children }) {
                                        return <p className="markdown-paragraph">{children}</p>;
                                    },
                                    ul({ children }) {
                                        return <ul className="markdown-list">{children}</ul>;
                                    },
                                    ol({ children }) {
                                        return <ol className="markdown-list">{children}</ol>;
                                    },
                                    li({ children }) {
                                        return <li className="markdown-list-item">{children}</li>;
                                    },
                                    blockquote({ children }) {
                                        return (
                                            <blockquote className="markdown-blockquote">
                                                {children}
                                            </blockquote>
                                        );
                                    },
                                    table({ children }) {
                                        return (
                                            <div className="table-container">
                                                <table className="markdown-table">
                                                    {children}
                                                </table>
                                            </div>
                                        );
                                    },
                                    a({ href, children }) {
                                        return (
                                            <a 
                                                href={href} 
                                                target="_blank" 
                                                rel="noopener noreferrer"
                                                className="markdown-link"
                                            >
                                                {children}
                                            </a>
                                        );
                                    }
                                }}
                            >
                                {message.content}
                            </ReactMarkdown>
                        ) : (
                            <pre className="plain-text">{message.content}</pre>
                        )}
                    </div>
                </div>
            );
        } else if (message.type === 'system') {
            return (
                <div className={`message system-message ${message.status}`}>
                    <div className="message-header">
                        <span className="message-type">System</span>
                        <span className="message-time">
                            {message.timestamp.toLocaleTimeString()}
                        </span>
                    </div>
                    <div className="message-content">
                        {message.content}
                    </div>
                </div>
            );
        }
        return null;
    };

    const getCodeTheme = () => {
        // In a real implementation, you'd detect the VSCode theme
        // For now, we'll use a default that works well in most cases
        return vscDarkPlus;
    };

    const copyToClipboard = async (text: string) => {
        try {
            await navigator.clipboard.writeText(text);
            // Could show a toast notification here
        } catch (err) {
            console.error('Failed to copy text: ', err);
        }
    };

    return (
        <div className="chat-container">
            <div className="messages">
                {messages.length === 0 ? (
                    <div className="empty-state">
                        <div className="empty-icon">
                            {isAskMode ? '❓' : '💬'}
                        </div>
                        <h3>
                            {isAskMode ? 'Ask a Question' : 'Start a Conversation'}
                        </h3>
                        <p>
                            {isAskMode 
                                ? 'Ask the AI agent any coding question and get immediate help.'
                                : 'Chat with the AI agent about your code, get suggestions, or discuss ideas.'
                            }
                        </p>
                        <div className="suggestions">
                            <h4>Try asking:</h4>
                            <ul>
                                {isAskMode ? (
                                    <>
                                        <li>"How do I implement a binary search in Java?"</li>
                                        <li>"What's the difference between HashMap and TreeMap?"</li>
                                        <li>"How can I optimize this SQL query?"</li>
                                    </>
                                ) : (
                                    <>
                                        <li>"Help me understand this algorithm"</li>
                                        <li>"Suggest improvements for this code"</li>
                                        <li>"Explain the design pattern used here"</li>
                                    </>
                                )}
                            </ul>
                        </div>
                    </div>
                ) : (
                    <>
                        {messages.map((message) => (
                            <div key={message.id}>
                                {formatMessage(message)}
                            </div>
                        ))}
                        {(isLoading || isStreaming) && (
                            <div className="message agent-message streaming">
                                <div className="message-header">
                                    <span className="message-type">AI Agent</span>
                                    <span className="streaming-indicator">●</span>
                                </div>
                                <div className="message-content">
                                    <div className="typing-indicator">
                                        <span></span>
                                        <span></span>
                                        <span></span>
                                    </div>
                                </div>
                            </div>
                        )}
                    </>
                )}
                <div ref={messagesEndRef} />
            </div>
            
            <div className="input-area">
                <div className="input-container">
                    <input
                        ref={inputRef}
                        value={input}
                        onChange={(e) => setInput(e.target.value)}
                        onKeyDown={handleKeyPress}
                        placeholder={placeholder}
                        disabled={isLoading || isStreaming}
                        className="message-input"
                    />
                    <button 
                        onClick={handleSend} 
                        disabled={!input.trim() || isLoading || isStreaming}
                        className="send-button"
                        title="Send message (Enter)"
                    >
                        {isLoading || isStreaming ? (
                            <div className="button-spinner"></div>
                        ) : (
                            '➤'
                        )}
                    </button>
                </div>
                
                <div className="input-hints">
                    <span className="hint">Press Enter to send, Shift+Enter for new line</span>
                    {isAskMode && (
                        <span className="hint">• Quick questions mode</span>
                    )}
                </div>
            </div>
        </div>
    );
};

export default ChatView;
