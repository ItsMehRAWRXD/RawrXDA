// AgentHandler.java - Enhanced HTTP handler with streaming support
package com.aicli.server;

import com.aicli.AgenticOrchestrator;
import com.aicli.AgentState;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import javax.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;

/**
 * Enhanced HTTP handler that supports both regular and streaming AI responses.
 * Implements Server-Sent Events (SSE) for real-time streaming of AI generated content.
 */
public class AgentHandler implements HttpHandler {
    private final AgenticOrchestrator orchestrator;
    private final ExecutorService executorService;

    public AgentHandler(AgenticOrchestrator orchestrator) {
        this.orchestrator = orchestrator;
        this.executorService = Executors.newCachedThreadPool();
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
        String method = exchange.getRequestMethod();
        String path = exchange.getRequestURI().getPath();

        try {
            if ("POST".equals(method)) {
                if (path.endsWith("/stream")) {
                    handleStreamingRequest(exchange);
                } else {
                    handleRegularRequest(exchange);
                }
            } else if ("GET".equals(method) && path.endsWith("/stream")) {
                handleSSEConnection(exchange);
            } else {
                sendErrorResponse(exchange, 405, "Method not allowed");
            }
        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Internal server error: " + e.getMessage());
        }
    }

    /**
     * Handle regular (non-streaming) AI requests
     */
    private void handleRegularRequest(HttpExchange exchange) throws IOException {
        String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
        
        try {
            JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
            String prompt = request.getString("prompt", "");
            String userId = request.getString("userId", "anonymous");
            JsonObject context = request.getJsonObject("context");
            
            if (prompt.isEmpty()) {
                sendErrorResponse(exchange, 400, "Prompt is required");
                return;
            }

            // Execute AI request asynchronously
            CompletableFuture<String> future = CompletableFuture.supplyAsync(() -> {
                try {
                    return orchestrator.execute(userId, prompt);
                } catch (Exception e) {
                    throw new RuntimeException("AI execution failed", e);
                }
            }, executorService);

            String response = future.get();
            
            // Send response
            JsonObject responseJson = Json.createObjectBuilder()
                    .add("status", "success")
                    .add("message", response)
                    .add("timestamp", System.currentTimeMillis())
                    .build();

            sendJsonResponse(exchange, responseJson);

        } catch (Exception e) {
            JsonObject errorResponse = Json.createObjectBuilder()
                    .add("status", "error")
                    .add("message", e.getMessage())
                    .add("timestamp", System.currentTimeMillis())
                    .build();
            sendJsonResponse(exchange, errorResponse);
        }
    }

    /**
     * Handle streaming AI requests using Server-Sent Events
     */
    private void handleStreamingRequest(HttpExchange exchange) throws IOException {
        String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
        
        try {
            JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
            String prompt = request.getString("prompt", "");
            String userId = request.getString("userId", "anonymous");
            JsonObject context = request.getJsonObject("context");
            
            if (prompt.isEmpty()) {
                sendErrorResponse(exchange, 400, "Prompt is required");
                return;
            }

            // Set up SSE headers
            exchange.getResponseHeaders().set("Content-Type", "text/event-stream");
            exchange.getResponseHeaders().set("Cache-Control", "no-cache");
            exchange.getResponseHeaders().set("Connection", "keep-alive");
            exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
            exchange.sendResponseHeaders(200, 0);

            try (OutputStream os = exchange.getResponseBody()) {
                // Create streaming consumer
                Consumer<String> streamConsumer = token -> {
                    try {
                        String event = "data: " + escapeForSSE(token) + "\n\n";
                        os.write(event.getBytes(StandardCharsets.UTF_8));
                        os.flush();
                    } catch (IOException e) {
                        throw new RuntimeException("Failed to write stream token", e);
                    }
                };

                // Send initial connection event
                sendSSEEvent(os, "connected", "Streaming started");

                // Execute streaming AI request
                orchestrator.streamExecution(streamConsumer, userId, prompt, context);

                // Send completion event
                sendSSEEvent(os, "completed", "Stream completed");
                
            } catch (Exception e) {
                try (OutputStream os = exchange.getResponseBody()) {
                    sendSSEEvent(os, "error", "Stream failed: " + e.getMessage());
                }
            }

        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Streaming request failed: " + e.getMessage());
        }
    }

    /**
     * Handle persistent SSE connection for real-time updates
     */
    private void handleSSEConnection(HttpExchange exchange) throws IOException {
        // Set up SSE headers
        exchange.getResponseHeaders().set("Content-Type", "text/event-stream");
        exchange.getResponseHeaders().set("Cache-Control", "no-cache");
        exchange.getResponseHeaders().set("Connection", "keep-alive");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(200, 0);

        try (OutputStream os = exchange.getResponseBody()) {
            // Send keep-alive events
            while (!Thread.currentThread().isInterrupted()) {
                sendSSEEvent(os, "ping", "keep-alive");
                Thread.sleep(30000); // Send ping every 30 seconds
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } catch (Exception e) {
            // Connection closed
        }
    }

    /**
     * Send a Server-Sent Event
     */
    private void sendSSEEvent(OutputStream os, String eventType, String data) throws IOException {
        String event = String.format("event: %s\ndata: %s\n\n", eventType, escapeForSSE(data));
        os.write(event.getBytes(StandardCharsets.UTF_8));
        os.flush();
    }

    /**
     * Escape data for SSE format
     */
    private String escapeForSSE(String data) {
        return data.replace("\n", "\\n")
                  .replace("\r", "\\r")
                  .replace("\0", "");
    }

    /**
     * Send JSON response
     */
    private void sendJsonResponse(HttpExchange exchange, JsonObject json) throws IOException {
        String response = json.toString();
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(200, response.length());
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response.getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Send error response
     */
    private void sendErrorResponse(HttpExchange exchange, int statusCode, String message) throws IOException {
        JsonObject errorResponse = Json.createObjectBuilder()
                .add("status", "error")
                .add("message", message)
                .add("timestamp", System.currentTimeMillis())
                .build();

        String response = errorResponse.toString();
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(statusCode, response.length());
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response.getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Handle human approval workflow
     */
    public void handleApprovalRequest(HttpExchange exchange) throws IOException {
        String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
        
        try {
            JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
            String taskId = request.getString("taskId");
            String userId = request.getString("userId");
            boolean approved = request.getBoolean("approved");
            String feedback = request.getString("feedback", "");

            // Process approval through orchestrator
            AgentState.AgentTask task = orchestrator.continueTask(taskId, approved);
            
            JsonObject response = Json.createObjectBuilder()
                    .add("status", "success")
                    .add("taskStatus", task.getStatus().name())
                    .add("message", approved ? "Task approved and continued" : "Task rejected")
                    .build();

            sendJsonResponse(exchange, response);

        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Approval processing failed: " + e.getMessage());
        }
    }

    /**
     * Handle feedback submission for learning
     */
    public void handleFeedbackSubmission(HttpExchange exchange) throws IOException {
        String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
        
        try {
            JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
            String userId = request.getString("userId");
            String interaction = request.getString("interaction"); // ACCEPT, REJECT, EDIT
            String editedResponse = request.getString("editedResponse", "");

            // Process feedback through orchestrator
            orchestrator.processFeedback(userId, 
                com.aicli.feedback.FeedbackRecord.InteractionType.valueOf(interaction), 
                editedResponse);

            JsonObject response = Json.createObjectBuilder()
                    .add("status", "success")
                    .add("message", "Feedback recorded successfully")
                    .build();

            sendJsonResponse(exchange, response);

        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Feedback processing failed: " + e.getMessage());
        }
    }

    /**
     * Get current agent status and active tasks
     */
    public void handleStatusRequest(HttpExchange exchange) throws IOException {
        try {
            JsonObjectBuilder statusBuilder = Json.createObjectBuilder();
            
            // Add active tasks, memory stats, etc.
            statusBuilder.add("activeTasks", orchestrator.getActiveTaskCount());
            statusBuilder.add("status", "running");
            statusBuilder.add("timestamp", System.currentTimeMillis());

            sendJsonResponse(exchange, statusBuilder.build());

        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Status request failed: " + e.getMessage());
        }
    }
}
