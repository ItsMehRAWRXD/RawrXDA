// App.tsx - Main React component for AI Agent webview
import React, { useState, useEffect } from 'react';
import ChatView from './ChatView';
import FeaturesView from './FeaturesView';
import './App.css';

// Extend the global window object to include VSCode API
declare global {
    interface Window {
        vscode: any;
        initialMode: string;
    }
}

interface Message {
    id: string;
    type: 'user' | 'agent' | 'system';
    content: string;
    timestamp: Date;
    status?: 'sending' | 'sent' | 'error';
}

const App: React.FC = () => {
    const [mode, setMode] = useState<'chat' | 'ask' | 'features'>(() => {
        return (window.initialMode as any) || 'chat';
    });
    const [messages, setMessages] = useState<Message[]>([]);
    const [isLoading, setIsLoading] = useState(false);

    // Listen for messages from VSCode extension
    useEffect(() => {
        const handleMessage = (event: MessageEvent) => {
            const message = event.data;
            
            switch (message.command) {
                case 'response':
                    setIsLoading(false);
                    addMessage('agent', message.text);
                    break;
                    
                case 'error':
                    setIsLoading(false);
                    addMessage('system', `Error: ${message.text}`);
                    break;
                    
                case 'setMode':
                    setMode(message.mode);
                    break;
                    
                case 'humanApprovalRequired':
                    handleHumanApprovalRequired(message);
                    break;
                    
                default:
                    console.log('Unknown message:', message);
            }
        };

        window.addEventListener('message', handleMessage);
        return () => window.removeEventListener('message', handleMessage);
    }, []);

    const handleToggle = (newMode: 'chat' | 'ask' | 'features') => {
        setMode(newMode);
        
        // Clear messages when switching modes (except when going to features)
        if (newMode !== 'features') {
            setMessages([]);
        }
    };

    const addMessage = (type: 'user' | 'agent' | 'system', content: string) => {
        const newMessage: Message = {
            id: Date.now().toString(),
            type,
            content,
            timestamp: new Date(),
            status: 'sent'
        };
        
        setMessages(prev => [...prev, newMessage]);
    };

    const sendMessage = (text: string, isAskMode: boolean = false) => {
        if (!text.trim()) return;

        // Add user message
        const userMessage: Message = {
            id: Date.now().toString(),
            type: 'user',
            content: text,
            timestamp: new Date(),
            status: 'sending'
        };
        
        setMessages(prev => [...prev, userMessage]);
        setIsLoading(true);

        // Send to VSCode extension
        window.vscode.postMessage({
            command: isAskMode ? 'askAgent' : 'chatAgent',
            text: text,
            mode: mode
        });

        // Update message status to sent
        setTimeout(() => {
            setMessages(prev => 
                prev.map(msg => 
                    msg.id === userMessage.id 
                        ? { ...msg, status: 'sent' }
                        : msg
                )
            );
        }, 100);
    };

    const handleFeatureSelect = (featureName: string) => {
        setIsLoading(true);
        
        // Send feature execution request to VSCode extension
        window.vscode.postMessage({
            command: 'executeFeature',
            feature: featureName
        });
    };

    const handleHumanApprovalRequired = (message: any) => {
        // Show approval dialog
        const approval = confirm(`AI Agent requires approval for: ${message.action}\n\nDo you approve this action?`);
        
        window.vscode.postMessage({
            command: 'provideApproval',
            approved: approval,
            action: message.action,
            data: message.data
        });
    };

    const clearMessages = () => {
        setMessages([]);
    };

    return (
        <div className="app-container">
            <header className="toggle-header">
                <button
                    onClick={() => handleToggle('chat')}
                    className={`toggle-btn ${mode === 'chat' ? 'active' : ''}`}
                    title="Chat with the AI agent for extended conversations"
                >
                    💬 Chat
                </button>
                <button
                    onClick={() => handleToggle('ask')}
                    className={`toggle-btn ${mode === 'ask' ? 'active' : ''}`}
                    title="Ask quick questions and get immediate answers"
                >
                    ❓ Ask
                </button>
                <button
                    onClick={() => handleToggle('features')}
                    className={`toggle-btn ${mode === 'features' ? 'active' : ''}`}
                    title="Browse and execute AI-powered features"
                >
                    ⚡ Features
                </button>
                
                {(mode === 'chat' || mode === 'ask') && messages.length > 0 && (
                    <button
                        onClick={clearMessages}
                        className="clear-btn"
                        title="Clear conversation history"
                    >
                        🗑️ Clear
                    </button>
                )}
            </header>
            
            <main className="content-area">
                {mode === 'chat' && (
                    <ChatView 
                        messages={messages}
                        onSendMessage={(text) => sendMessage(text, false)}
                        isLoading={isLoading}
                        placeholder="Chat with the AI agent about your code, ask for help, or discuss ideas..."
                    />
                )}
                
                {mode === 'ask' && (
                    <ChatView 
                        messages={messages}
                        onSendMessage={(text) => sendMessage(text, true)}
                        isLoading={isLoading}
                        placeholder="Ask a quick question and get an immediate answer..."
                    />
                )}
                
                {mode === 'features' && (
                    <FeaturesView 
                        onFeatureSelect={handleFeatureSelect}
                        isLoading={isLoading}
                    />
                )}
            </main>
            
            {isLoading && (
                <div className="loading-overlay">
                    <div className="loading-spinner"></div>
                    <span>AI Agent is thinking...</span>
                </div>
            )}
        </div>
    );
};

export default App;
