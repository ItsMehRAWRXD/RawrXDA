// InteractiveAgentGraph.java - LangGraph4j-based interactive workflow management
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.time.Instant;

/**
 * Interactive agent graph implementation using LangGraph4j patterns.
 * This class manages complex workflows with human-in-the-loop checkpoints,
 * tool execution, and state management for multi-step AI tasks.
 */
public class InteractiveAgentGraph {
    private final ChatLanguageModel model;
    private final Map<String, AgentState> activeGraphs;
    private final ExecutorService executorService;
    private final Dispatcher dispatcher;
    private final ApprovalNode approvalNode;
    private final ToolExecutor toolExecutor;

    public InteractiveAgentGraph(ChatLanguageModel model) {
        this.model = model;
        this.activeGraphs = new ConcurrentHashMap<>();
        this.executorService = Executors.newFixedThreadPool(10);
        this.dispatcher = new Dispatcher();
        this.approvalNode = new ApprovalNode();
        this.toolExecutor = new ToolExecutor();
    }

    /**
     * Create and start a new interactive workflow
     */
    public String startWorkflow(String userId, String sessionId, String initialPrompt) {
        String graphId = UUID.randomUUID().toString();
        AgentState initialState = new AgentState(userId, sessionId);
        
        // Add initial user message
        initialState.addMessage("User: " + initialPrompt);
        initialState.setWorkflowStatus("INITIALIZED");
        initialState.setCurrentNode("llm_node");
        
        activeGraphs.put(graphId, initialState);
        
        // Start workflow execution asynchronously
        CompletableFuture.runAsync(() -> {
            try {
                executeWorkflow(graphId);
            } catch (Exception e) {
                AgentState state = activeGraphs.get(graphId);
                if (state != null) {
                    state.setErrorMessage("Workflow execution failed: " + e.getMessage());
                    state.setWorkflowStatus("FAILED");
                }
            }
        }, executorService);
        
        return graphId;
    }

    /**
     * Resume workflow after human input
     */
    public AgentState resumeWorkflow(String graphId, String humanInput) {
        AgentState state = activeGraphs.get(graphId);
        if (state == null) {
            throw new IllegalArgumentException("Workflow not found: " + graphId);
        }
        
        // Add human feedback to state
        state.setHumanFeedback(humanInput);
        state.clearApprovalRequest();
        state.setRequiresHumanInput(false);
        state.setWorkflowStatus("IN_PROGRESS");
        
        // Continue workflow execution
        CompletableFuture.runAsync(() -> {
            try {
                executeWorkflow(graphId);
            } catch (Exception e) {
                state.setErrorMessage("Workflow execution failed: " + e.getMessage());
                state.setWorkflowStatus("FAILED");
            }
        }, executorService);
        
        return state;
    }

    /**
     * Execute the workflow graph
     */
    private void executeWorkflow(String graphId) {
        AgentState state = activeGraphs.get(graphId);
        if (state == null) {
            return;
        }
        
        try {
            while (!state.isCompleted() && !state.isFailed() && !state.isWaitingForHuman()) {
                String currentNode = state.getCurrentNode();
                
                switch (currentNode) {
                    case "llm_node":
                        executeLlmNode(state);
                        break;
                    case "dispatcher_node":
                        executeDispatcherNode(state);
                        break;
                    case "approval_node":
                        executeApprovalNode(state);
                        break;
                    case "tool_execution_node":
                        executeToolExecutionNode(state);
                        break;
                    case "final_node":
                        executeFinalNode(state);
                        break;
                    default:
                        state.setErrorMessage("Unknown node: " + currentNode);
                        state.setWorkflowStatus("FAILED");
                        return;
                }
                
                // Check for workflow completion
                if (state.isWaitingForHuman()) {
                    return; // Wait for human input
                }
            }
        } catch (Exception e) {
            state.setErrorMessage("Workflow execution error: " + e.getMessage());
            state.setWorkflowStatus("FAILED");
        }
    }

    /**
     * Execute LLM reasoning node
     */
    private void executeLlmNode(AgentState state) {
        try {
            // Build context from chat history
            StringBuilder context = new StringBuilder();
            for (String message : state.getChatHistory()) {
                context.append(message).append("\n");
            }
            
            // Add human feedback if available
            if (state.hasHumanFeedback()) {
                context.append("Human feedback: ").append(state.getHumanFeedback()).append("\n");
                state.clearHumanFeedback();
            }
            
            // Call LLM
            String response = callLlm(context.toString());
            state.addMessage("AI: " + response);
            
            // Check if response contains tool calls
            if (response.contains("TOOL_CALL:") || response.contains("EXECUTE:")) {
                state.setCurrentNode("dispatcher_node");
            } else {
                state.setCurrentNode("final_node");
            }
            
        } catch (Exception e) {
            state.setErrorMessage("LLM node execution failed: " + e.getMessage());
            state.setWorkflowStatus("FAILED");
        }
    }

    /**
     * Execute dispatcher node
     */
    private void executeDispatcherNode(AgentState state) {
        try {
            String lastMessage = state.getChatHistory().get(state.getChatHistory().size() - 1);
            String toolCall = dispatcher.extractToolCall(lastMessage);
            
            if (toolCall != null) {
                // Check if tool call requires approval
                if (dispatcher.requiresApproval(toolCall)) {
                    state.setCurrentNode("approval_node");
                    state.setApprovalRequest("Tool call requires approval: " + toolCall);
                } else {
                    state.setCurrentNode("tool_execution_node");
                    state.addToContext("pending_tool_call", toolCall);
                }
            } else {
                state.setCurrentNode("final_node");
            }
            
        } catch (Exception e) {
            state.setErrorMessage("Dispatcher node execution failed: " + e.getMessage());
            state.setWorkflowStatus("FAILED");
        }
    }

    /**
     * Execute approval node
     */
    private void executeApprovalNode(AgentState state) {
        // This node waits for human input
        state.setRequiresHumanInput(true);
        state.setWorkflowStatus("WAITING_FOR_HUMAN");
        state.setCurrentNode("approval_node");
    }

    /**
     * Execute tool execution node
     */
    private void executeToolExecutionNode(AgentState state) {
        try {
            String toolCall = (String) state.getFromContext("pending_tool_call");
            if (toolCall != null) {
                String result = toolExecutor.executeTool(toolCall);
                state.addToolResult("last_tool", result);
                state.addMessage("AI: Tool execution result: " + result);
                state.removeFromContext("pending_tool_call");
            }
            
            state.setCurrentNode("llm_node");
            
        } catch (Exception e) {
            state.setErrorMessage("Tool execution failed: " + e.getMessage());
            state.setWorkflowStatus("FAILED");
        }
    }

    /**
     * Execute final node
     */
    private void executeFinalNode(AgentState state) {
        state.setWorkflowStatus("COMPLETED");
        state.setCurrentNode("final_node");
    }

    /**
     * Call LLM with context
     */
    private String callLlm(String context) {
        // This is a simplified LLM call - in practice, you'd use the actual model
        return "LLM Response for: " + context.substring(0, Math.min(100, context.length()));
    }

    /**
     * Get workflow state
     */
    public AgentState getWorkflowState(String graphId) {
        return activeGraphs.get(graphId);
    }

    /**
     * List all active workflows
     */
    public List<AgentState> getActiveWorkflows() {
        return new ArrayList<>(activeGraphs.values());
    }

    /**
     * Cancel a workflow
     */
    public void cancelWorkflow(String graphId) {
        AgentState state = activeGraphs.get(graphId);
        if (state != null) {
            state.setWorkflowStatus("CANCELLED");
        }
    }

    /**
     * Clean up completed workflows
     */
    public void cleanupWorkflows() {
        activeGraphs.entrySet().removeIf(entry -> {
            AgentState state = entry.getValue();
            return state.isCompleted() || state.isFailed() || "CANCELLED".equals(state.getWorkflowStatus());
        });
    }

    /**
     * Shutdown the graph executor
     */
    public void shutdown() {
        executorService.shutdown();
        cleanupWorkflows();
    }

    /**
     * Dispatcher for tool call routing
     */
    public static class Dispatcher {
        public String extractToolCall(String message) {
            // Extract tool calls from LLM response
            if (message.contains("TOOL_CALL:")) {
                int start = message.indexOf("TOOL_CALL:") + 10;
                int end = message.indexOf("\n", start);
                if (end == -1) end = message.length();
                return message.substring(start, end).trim();
            }
            return null;
        }

        public boolean requiresApproval(String toolCall) {
            // Determine if tool call requires human approval
            String lowerCall = toolCall.toLowerCase();
            return lowerCall.contains("delete") || lowerCall.contains("remove") || 
                   lowerCall.contains("modify") || lowerCall.contains("replace") ||
                   lowerCall.contains("execute") || lowerCall.contains("run");
        }

        public String routeToolCall(AgentState state) {
            String lastMessage = state.getChatHistory().get(state.getChatHistory().size() - 1);
            String toolCall = extractToolCall(lastMessage);
            
            if (toolCall != null) {
                if (requiresApproval(toolCall)) {
                    return "requires_approval";
                } else {
                    return "execute_tool";
                }
            }
            
            return "continue";
        }
    }

    /**
     * Approval node for human-in-the-loop processes
     */
    public static class ApprovalNode {
        public AgentState interruptForApproval(AgentState state) {
            state.setRequiresHumanInput(true);
            state.setWorkflowStatus("WAITING_FOR_HUMAN");
            return state;
        }
    }

    /**
     * Tool executor for running tools
     */
    public static class ToolExecutor {
        public String executeTool(String toolCall) {
            // Simplified tool execution - in practice, you'd have a registry of tools
            return "Tool executed: " + toolCall;
        }
    }
}
