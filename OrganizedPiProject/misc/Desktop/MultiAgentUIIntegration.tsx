// MultiAgentUIIntegration.tsx - Integration between UI components and multi-agent backend
import React, { useState, useEffect, useCallback } from 'react';
import PersonaSelector from './components/PersonaSelector';
import PredictiveSuggestions from './components/PredictiveSuggestions';
import './MultiAgentUIIntegration.css';

interface AgentPersona {
    id: string;
    name: string;
    description: string;
    capabilities: string[];
    systemPrompt: string;
    isActive: boolean;
}

interface CollaborationSession {
    sessionId: string;
    userId: string;
    task: string;
    status: 'INITIALIZED' | 'IN_PROGRESS' | 'REQUIRES_APPROVAL' | 'COMPLETED' | 'FAILED' | 'CANCELLED';
    agentResults: Record<string, string>;
    finalResult?: string;
    error?: string;
    approvalRequest?: string;
    userRole: string;
}

interface MultiAgentUIIntegrationProps {
    userId: string;
    userRole: string;
    onSessionUpdate: (session: CollaborationSession) => void;
    onError: (error: string) => void;
}

const MultiAgentUIIntegration: React.FC<MultiAgentUIIntegrationProps> = ({
    userId,
    userRole,
    onSessionUpdate,
    onError
}) => {
    const [selectedPersonas, setSelectedPersonas] = useState<AgentPersona[]>([]);
    const [activeSession, setActiveSession] = useState<CollaborationSession | null>(null);
    const [suggestions, setSuggestions] = useState<any[]>([]);
    const [isLoading, setIsLoading] = useState(false);
    const [taskInput, setTaskInput] = useState('');

    // Available agent personas
    const availablePersonas: AgentPersona[] = [
        {
            id: 'planner',
            name: 'Task Planner',
            description: 'Breaks down complex tasks into manageable steps',
            capabilities: ['planning', 'decomposition', 'risk_assessment', 'security_analysis'],
            systemPrompt: 'You are a task planning specialist with security awareness.',
            isActive: false
        },
        {
            id: 'executor',
            name: 'Code Executor',
            description: 'Implements code safely within sandboxed environments',
            capabilities: ['coding', 'testing', 'debugging', 'secure_execution'],
            systemPrompt: 'You are a code execution specialist with security constraints.',
            isActive: false
        },
        {
            id: 'researcher',
            name: 'Research Agent',
            description: 'Gathers information from approved sources',
            capabilities: ['research', 'documentation', 'analysis', 'secure_data_access'],
            systemPrompt: 'You are a research specialist with controlled information access.',
            isActive: false
        },
        {
            id: 'reviewer',
            name: 'Security Reviewer',
            description: 'Reviews work for quality and security compliance',
            capabilities: ['review', 'quality_assurance', 'security_audit', 'compliance_check'],
            systemPrompt: 'You are a quality assurance specialist with security auditing capabilities.',
            isActive: false
        }
    ];

    // Initialize personas
    useEffect(() => {
        setSelectedPersonas(availablePersonas);
    }, []);

    // Handle persona selection
    const handlePersonaToggle = useCallback((personaId: string) => {
        setSelectedPersonas(prev => 
            prev.map(persona => 
                persona.id === personaId 
                    ? { ...persona, isActive: !persona.isActive }
                    : persona
            )
        );
    }, []);

    // Start collaborative task
    const startCollaboration = useCallback(async (task: string) => {
        if (!task.trim()) {
            onError('Please enter a task description');
            return;
        }

        const activePersonas = selectedPersonas.filter(p => p.isActive);
        if (activePersonas.length === 0) {
            onError('Please select at least one agent persona');
            return;
        }

        setIsLoading(true);
        try {
            const response = await fetch('/api/secure-collaboration/start', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    task,
                    userId,
                    userRole,
                    activePersonas: activePersonas.map(p => p.id)
                })
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const session: CollaborationSession = await response.json();
            setActiveSession(session);
            onSessionUpdate(session);

            // Start polling for updates
            pollSessionUpdates(session.sessionId);

        } catch (error) {
            onError(`Failed to start collaboration: ${error}`);
        } finally {
            setIsLoading(false);
        }
    }, [selectedPersonas, userId, userRole, onError, onSessionUpdate]);

    // Poll for session updates
    const pollSessionUpdates = useCallback(async (sessionId: string) => {
        const pollInterval = setInterval(async () => {
            try {
                const response = await fetch(`/api/secure-collaboration/session/${sessionId}`);
                if (response.ok) {
                    const session: CollaborationSession = await response.json();
                    setActiveSession(session);
                    onSessionUpdate(session);

                    // Stop polling if session is completed or failed
                    if (session.status === 'COMPLETED' || session.status === 'FAILED' || session.status === 'CANCELLED') {
                        clearInterval(pollInterval);
                    }
                }
            } catch (error) {
                console.error('Failed to poll session updates:', error);
            }
        }, 2000); // Poll every 2 seconds

        // Cleanup after 5 minutes
        setTimeout(() => clearInterval(pollInterval), 300000);
    }, [onSessionUpdate]);

    // Handle approval response
    const handleApprovalResponse = useCallback(async (approved: boolean, feedback?: string) => {
        if (!activeSession) return;

        try {
            const response = await fetch(`/api/secure-collaboration/approve/${activeSession.sessionId}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    approved,
                    feedback
                })
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const session: CollaborationSession = await response.json();
            setActiveSession(session);
            onSessionUpdate(session);

        } catch (error) {
            onError(`Failed to submit approval: ${error}`);
        }
    }, [activeSession, onError, onSessionUpdate]);

    // Generate predictive suggestions
    const generateSuggestions = useCallback(async () => {
        if (!activeSession) return;

        try {
            const response = await fetch('/api/predictive-suggestions', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    sessionId: activeSession.sessionId,
                    context: activeSession.agentResults,
                    userRole
                })
            });

            if (response.ok) {
                const suggestions = await response.json();
                setSuggestions(suggestions);
            }
        } catch (error) {
            console.error('Failed to generate suggestions:', error);
        }
    }, [activeSession, userRole]);

    // Handle suggestion action
    const handleSuggestionAction = useCallback(async (action: string, payload: any) => {
        try {
            const response = await fetch('/api/suggestions/execute', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    action,
                    payload,
                    sessionId: activeSession?.sessionId,
                    userId
                })
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            // Refresh suggestions after action
            generateSuggestions();

        } catch (error) {
            onError(`Failed to execute suggestion: ${error}`);
        }
    }, [activeSession, userId, generateSuggestions, onError]);

    // Generate suggestions when session updates
    useEffect(() => {
        if (activeSession && activeSession.status === 'IN_PROGRESS') {
            generateSuggestions();
        }
    }, [activeSession, generateSuggestions]);

    return (
        <div className="multi-agent-ui-integration">
            <div className="integration-header">
                <h2>Multi-Agent Collaboration</h2>
                <p>Select agent personas and start a collaborative task</p>
            </div>

            <div className="integration-content">
                {/* Persona Selection */}
                <div className="persona-section">
                    <h3>Select Agent Personas</h3>
                    <div className="persona-grid">
                        {selectedPersonas.map(persona => (
                            <PersonaSelector
                                key={persona.id}
                                persona={persona}
                                onToggle={() => handlePersonaToggle(persona.id)}
                            />
                        ))}
                    </div>
                </div>

                {/* Task Input */}
                <div className="task-section">
                    <h3>Task Description</h3>
                    <textarea
                        value={taskInput}
                        onChange={(e) => setTaskInput(e.target.value)}
                        placeholder="Describe the task you want the agents to collaborate on..."
                        rows={4}
                        className="task-input"
                    />
                    <button
                        onClick={() => startCollaboration(taskInput)}
                        disabled={isLoading || !taskInput.trim()}
                        className="start-collaboration-btn"
                    >
                        {isLoading ? 'Starting...' : 'Start Collaboration'}
                    </button>
                </div>

                {/* Active Session */}
                {activeSession && (
                    <div className="session-section">
                        <h3>Collaboration Session</h3>
                        <div className="session-info">
                            <p><strong>Status:</strong> {activeSession.status}</p>
                            <p><strong>Task:</strong> {activeSession.task}</p>
                            <p><strong>User Role:</strong> {activeSession.userRole}</p>
                        </div>

                        {/* Agent Results */}
                        {Object.entries(activeSession.agentResults).length > 0 && (
                            <div className="agent-results">
                                <h4>Agent Results</h4>
                                {Object.entries(activeSession.agentResults).map(([agent, result]) => (
                                    <div key={agent} className="agent-result">
                                        <h5>{agent}</h5>
                                        <pre>{result}</pre>
                                    </div>
                                ))}
                            </div>
                        )}

                        {/* Approval Request */}
                        {activeSession.status === 'REQUIRES_APPROVAL' && activeSession.approvalRequest && (
                            <div className="approval-section">
                                <h4>Approval Required</h4>
                                <p>{activeSession.approvalRequest}</p>
                                <div className="approval-buttons">
                                    <button
                                        onClick={() => handleApprovalResponse(true)}
                                        className="approve-btn"
                                    >
                                        Approve
                                    </button>
                                    <button
                                        onClick={() => handleApprovalResponse(false)}
                                        className="reject-btn"
                                    >
                                        Reject
                                    </button>
                                </div>
                            </div>
                        )}

                        {/* Final Result */}
                        {activeSession.finalResult && (
                            <div className="final-result">
                                <h4>Final Result</h4>
                                <pre>{activeSession.finalResult}</pre>
                            </div>
                        )}

                        {/* Error */}
                        {activeSession.error && (
                            <div className="error-section">
                                <h4>Error</h4>
                                <p className="error-message">{activeSession.error}</p>
                            </div>
                        )}
                    </div>
                )}

                {/* Predictive Suggestions */}
                {suggestions.length > 0 && (
                    <div className="suggestions-section">
                        <h3>Predictive Suggestions</h3>
                        <PredictiveSuggestions
                            suggestions={suggestions}
                            onSuggestionAction={handleSuggestionAction}
                        />
                    </div>
                )}
            </div>
        </div>
    );
};

export default MultiAgentUIIntegration;
