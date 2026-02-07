// RequiresHumanApprovalTool.java - LangChain4j tool for human-in-the-loop workflows
package com.aicli.plugins;

import dev.langchain4j.agent.tool.Tool;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Custom LangChain4j tool that enables human-in-the-loop workflows.
 * When called by the LLM, this tool pauses execution and waits for human feedback.
 * This is typically used with LangGraph4j's interrupt() mechanism for proper state management.
 */
public class RequiresHumanApprovalTool implements Tool {
    private final AtomicReference<String> humanFeedback = new AtomicReference<>();
    private final BlockingQueue<String> feedbackQueue = new LinkedBlockingQueue<>();
    private final AtomicReference<String> pendingAction = new AtomicReference<>();
    private final long timeoutSeconds;

    public RequiresHumanApprovalTool() {
        this(300); // Default 5 minute timeout
    }

    public RequiresHumanApprovalTool(long timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
    }

    @Override
    public String name() {
        return "request_human_approval";
    }

    @Override
    public String description() {
        return "Requests human approval for a critical action before proceeding. " +
               "Input should describe the action requiring approval. The tool " +
               "will pause execution until human approval is received. " +
               "Use this for actions like: file modifications, API calls, " +
               "database changes, or any potentially destructive operations. " +
               "Format: 'ACTION_DESCRIPTION|DETAILED_CONTEXT|RISK_LEVEL'";
    }
    
    @Override
    public String execute(String actionToApprove) {
        System.err.println("--- HUMAN APPROVAL REQUIRED ---");
        System.err.println("Action: " + actionToApprove);
        System.err.println("Timestamp: " + java.time.Instant.now());
        
        // Parse the action description
        String[] parts = actionToApprove.split("\\|");
        String action = parts.length > 0 ? parts[0] : actionToApprove;
        String context = parts.length > 1 ? parts[1] : "";
        String riskLevel = parts.length > 2 ? parts[2] : "MEDIUM";
        
        // Store the pending action
        pendingAction.set(actionToApprove);
        
        // Signal that we need human input (for LangGraph interrupt)
        signalHumanInputRequired(action, context, riskLevel);
        
        // Wait for human feedback with timeout
        try {
            String feedback = feedbackQueue.poll(timeoutSeconds, TimeUnit.SECONDS);
            if (feedback == null) {
                return "TIMEOUT: No human feedback received within " + timeoutSeconds + " seconds. " +
                       "Action cancelled for safety.";
            }
            
            // Process the feedback
            return processFeedback(feedback);
            
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return "INTERRUPTED: Execution was interrupted while waiting for human feedback.";
        } finally {
            // Clear the pending action
            pendingAction.set(null);
        }
    }

    /**
     * Signal that human input is required (used by LangGraph interrupt mechanism)
     */
    private void signalHumanInputRequired(String action, String context, String riskLevel) {
        System.err.println("Risk Level: " + riskLevel);
        System.err.println("Context: " + context);
        System.err.println("Waiting for feedback...");
        
        // In a real implementation, this would trigger LangGraph's interrupt()
        // and return a structured response to the client
    }

    /**
     * Process human feedback and return appropriate response
     */
    private String processFeedback(String feedback) {
        System.err.println("Human feedback received: " + feedback);
        
        // Parse feedback type
        if (feedback.startsWith("APPROVE:")) {
            String approvalDetails = feedback.substring(8).trim();
            return "APPROVED: " + approvalDetails + ". Proceeding with the action.";
        } else if (feedback.startsWith("REJECT:")) {
            String rejectionReason = feedback.substring(7).trim();
            return "REJECTED: " + rejectionReason + ". Action cancelled.";
        } else if (feedback.startsWith("MODIFY:")) {
            String modifications = feedback.substring(7).trim();
            return "MODIFIED: " + modifications + ". Please incorporate these changes and re-approve.";
        } else if (feedback.startsWith("ASK:")) {
            String question = feedback.substring(4).trim();
            return "QUESTION: " + question + ". Please provide clarification.";
        } else {
            // Default to approval for simple responses
            return "APPROVED: " + feedback + ". Proceeding with the action.";
        }
    }

    /**
     * Provide human feedback (called by external systems)
     */
    public void provideFeedback(String feedback) {
        feedbackQueue.offer(feedback);
    }

    /**
     * Get the currently pending action
     */
    public String getPendingAction() {
        return pendingAction.get();
    }

    /**
     * Check if waiting for human input
     */
    public boolean isWaitingForInput() {
        return pendingAction.get() != null;
    }

    /**
     * Cancel pending approval request
     */
    public void cancelPending() {
        provideFeedback("REJECT: Request cancelled by user");
    }

    /**
     * Timeout pending approval request
     */
    public void timeoutPending() {
        provideFeedback("TIMEOUT: Request timed out");
    }

    /**
     * Helper method to create approval request strings
     */
    public static String createApprovalRequest(String action, String context, RiskLevel riskLevel) {
        return action + "|" + context + "|" + riskLevel.name();
    }

    /**
     * Helper method to create approval request strings with default context
     */
    public static String createApprovalRequest(String action, RiskLevel riskLevel) {
        return createApprovalRequest(action, "No additional context provided", riskLevel);
    }

    /**
     * Risk levels for approval requests
     */
    public enum RiskLevel {
        LOW("Low risk - Safe to proceed"),
        MEDIUM("Medium risk - Review recommended"),
        HIGH("High risk - Careful review required"),
        CRITICAL("Critical risk - Detailed review mandatory");

        private final String description;

        RiskLevel(String description) {
            this.description = description;
        }

        public String getDescription() {
            return description;
        }
    }
}
