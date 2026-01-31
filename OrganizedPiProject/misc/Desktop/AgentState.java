// AgentState.java - State management for LangGraph4j workflows
package com.aicli;

import dev.langchain4j.agent.tool.ToolExecutionRequest;
import dev.langchain4j.graph.State;
import java.util.*;
import java.time.Instant;

/**
 * Comprehensive state object for LangGraph4j agent workflows.
 * This class manages conversation history, tool execution requests, human feedback,
 * and workflow state transitions for human-in-the-loop processes.
 */
@State
public class AgentState {
    private List<String> chatHistory;
    private ToolExecutionRequest proposedToolCall;
    private String humanFeedback;
    private String workflowStatus;
    private Instant lastUpdated;
    private Map<String, Object> context;
    private String currentNode;
    private String nextNode;
    private boolean requiresHumanInput;
    private String approvalRequest;
    private Map<String, String> toolResults;
    private String errorMessage;
    private int retryCount;
    private String userId;
    private String sessionId;

    public AgentState() {
        this.chatHistory = new ArrayList<>();
        this.context = new HashMap<>();
        this.toolResults = new HashMap<>();
        this.workflowStatus = "INITIALIZED";
        this.lastUpdated = Instant.now();
        this.requiresHumanInput = false;
        this.retryCount = 0;
    }

    public AgentState(List<String> chatHistory) {
        this();
        this.chatHistory = chatHistory != null ? new ArrayList<>(chatHistory) : new ArrayList<>();
    }

    public AgentState(String userId, String sessionId) {
        this();
        this.userId = userId;
        this.sessionId = sessionId;
    }

    // Core state management
    public List<String> getChatHistory() {
        return chatHistory;
    }

    public void setChatHistory(List<String> chatHistory) {
        this.chatHistory = chatHistory != null ? new ArrayList<>(chatHistory) : new ArrayList<>();
        this.lastUpdated = Instant.now();
    }

    public void addMessage(String message) {
        this.chatHistory.add(message);
        this.lastUpdated = Instant.now();
    }

    // Tool execution management
    public ToolExecutionRequest getProposedToolCall() {
        return proposedToolCall;
    }

    public void setProposedToolCall(ToolExecutionRequest proposedToolCall) {
        this.proposedToolCall = proposedToolCall;
        this.lastUpdated = Instant.now();
    }

    public boolean hasProposedToolCall() {
        return proposedToolCall != null;
    }

    public void clearProposedToolCall() {
        this.proposedToolCall = null;
        this.lastUpdated = Instant.now();
    }

    // Human feedback management
    public String getHumanFeedback() {
        return humanFeedback;
    }

    public void setHumanFeedback(String humanFeedback) {
        this.humanFeedback = humanFeedback;
        this.lastUpdated = Instant.now();
    }

    public boolean hasHumanFeedback() {
        return humanFeedback != null && !humanFeedback.trim().isEmpty();
    }

    public void clearHumanFeedback() {
        this.humanFeedback = null;
        this.lastUpdated = Instant.now();
    }

    // Workflow status management
    public String getWorkflowStatus() {
        return workflowStatus;
    }

    public void setWorkflowStatus(String workflowStatus) {
        this.workflowStatus = workflowStatus;
        this.lastUpdated = Instant.now();
    }

    public boolean isCompleted() {
        return "COMPLETED".equals(workflowStatus);
    }

    public boolean isFailed() {
        return "FAILED".equals(workflowStatus);
    }

    public boolean isWaitingForHuman() {
        return "WAITING_FOR_HUMAN".equals(workflowStatus);
    }

    // Node management for graph traversal
    public String getCurrentNode() {
        return currentNode;
    }

    public void setCurrentNode(String currentNode) {
        this.currentNode = currentNode;
        this.lastUpdated = Instant.now();
    }

    public String getNextNode() {
        return nextNode;
    }

    public void setNextNode(String nextNode) {
        this.nextNode = nextNode;
        this.lastUpdated = Instant.now();
    }

    // Human input requirements
    public boolean isRequiresHumanInput() {
        return requiresHumanInput;
    }

    public void setRequiresHumanInput(boolean requiresHumanInput) {
        this.requiresHumanInput = requiresHumanInput;
        this.lastUpdated = Instant.now();
    }

    public String getApprovalRequest() {
        return approvalRequest;
    }

    public void setApprovalRequest(String approvalRequest) {
        this.approvalRequest = approvalRequest;
        this.requiresHumanInput = true;
        this.workflowStatus = "WAITING_FOR_HUMAN";
        this.lastUpdated = Instant.now();
    }

    public void clearApprovalRequest() {
        this.approvalRequest = null;
        this.requiresHumanInput = false;
        this.lastUpdated = Instant.now();
    }

    // Context management
    public Map<String, Object> getContext() {
        return context;
    }

    public void setContext(Map<String, Object> context) {
        this.context = context != null ? new HashMap<>(context) : new HashMap<>();
        this.lastUpdated = Instant.now();
    }

    public void addToContext(String key, Object value) {
        if (this.context == null) {
            this.context = new HashMap<>();
        }
        this.context.put(key, value);
        this.lastUpdated = Instant.now();
    }

        public Object getFromContext(String key) {
            return this.context != null ? this.context.get(key) : null;
        }

        public void removeFromContext(String key) {
            if (this.context != null) {
                this.context.remove(key);
                this.lastUpdated = Instant.now();
            }
        }

    // Tool results management
    public Map<String, String> getToolResults() {
        return toolResults;
    }

    public void setToolResults(Map<String, String> toolResults) {
        this.toolResults = toolResults != null ? new HashMap<>(toolResults) : new HashMap<>();
        this.lastUpdated = Instant.now();
    }

    public void addToolResult(String toolName, String result) {
        if (this.toolResults == null) {
            this.toolResults = new HashMap<>();
        }
        this.toolResults.put(toolName, result);
        this.lastUpdated = Instant.now();
    }

    public String getToolResult(String toolName) {
        return this.toolResults != null ? this.toolResults.get(toolName) : null;
    }

    // Error management
    public String getErrorMessage() {
        return errorMessage;
    }

    public void setErrorMessage(String errorMessage) {
        this.errorMessage = errorMessage;
        this.workflowStatus = "FAILED";
        this.lastUpdated = Instant.now();
    }

    public boolean hasError() {
        return errorMessage != null && !errorMessage.trim().isEmpty();
    }

    public void clearError() {
        this.errorMessage = null;
        this.lastUpdated = Instant.now();
    }

    // Retry management
    public int getRetryCount() {
        return retryCount;
    }

    public void setRetryCount(int retryCount) {
        this.retryCount = retryCount;
        this.lastUpdated = Instant.now();
    }

    public void incrementRetryCount() {
        this.retryCount++;
        this.lastUpdated = Instant.now();
    }

    public boolean shouldRetry(int maxRetries) {
        return this.retryCount < maxRetries;
    }

    // User and session management
    public String getUserId() {
        return userId;
    }

    public void setUserId(String userId) {
        this.userId = userId;
        this.lastUpdated = Instant.now();
    }

    public String getSessionId() {
        return sessionId;
    }

    public void setSessionId(String sessionId) {
        this.sessionId = sessionId;
        this.lastUpdated = Instant.now();
    }

    // Timestamp management
    public Instant getLastUpdated() {
        return lastUpdated;
    }

    public void updateTimestamp() {
        this.lastUpdated = Instant.now();
    }

    // Utility methods
    public boolean isStale(long timeoutSeconds) {
        return Instant.now().isAfter(this.lastUpdated.plusSeconds(timeoutSeconds));
    }

    public void resetForNewTask() {
        this.proposedToolCall = null;
        this.humanFeedback = null;
        this.workflowStatus = "READY";
        this.currentNode = null;
        this.nextNode = null;
        this.requiresHumanInput = false;
        this.approvalRequest = null;
        this.errorMessage = null;
        this.retryCount = 0;
        this.toolResults.clear();
        this.lastUpdated = Instant.now();
    }

    public AgentState createSnapshot() {
        AgentState snapshot = new AgentState();
        snapshot.chatHistory = new ArrayList<>(this.chatHistory);
        snapshot.proposedToolCall = this.proposedToolCall;
        snapshot.humanFeedback = this.humanFeedback;
        snapshot.workflowStatus = this.workflowStatus;
        snapshot.lastUpdated = this.lastUpdated;
        snapshot.context = new HashMap<>(this.context);
        snapshot.currentNode = this.currentNode;
        snapshot.nextNode = this.nextNode;
        snapshot.requiresHumanInput = this.requiresHumanInput;
        snapshot.approvalRequest = this.approvalRequest;
        snapshot.toolResults = new HashMap<>(this.toolResults);
        snapshot.errorMessage = this.errorMessage;
        snapshot.retryCount = this.retryCount;
        snapshot.userId = this.userId;
        snapshot.sessionId = this.sessionId;
        return snapshot;
    }

    @Override
    public String toString() {
        return String.format("AgentState{status='%s', node='%s', humanInput=%s, updated=%s}", 
                workflowStatus, currentNode, requiresHumanInput, lastUpdated);
    }
}
