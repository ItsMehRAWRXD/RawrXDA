// FeedbackHandler.java - HTTP handler for processing user feedback
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.JsonNode;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/**
 * HTTP handler for processing user feedback requests.
 * This handler receives feedback from the VSCode extension and processes it
 * through the AgenticOrchestrator for continuous learning.
 */
public class FeedbackHandler implements HttpHandler {
    private final AgenticOrchestrator orchestrator;
    private final FeedbackStore feedbackStore;
    private final ObjectMapper objectMapper;

    public FeedbackHandler(AgenticOrchestrator orchestrator) {
        this.orchestrator = orchestrator;
        this.feedbackStore = new FeedbackStore();
        this.objectMapper = new ObjectMapper();
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
        String method = exchange.getRequestMethod();
        String path = exchange.getRequestURI().getPath();
        
        try {
            if ("POST".equals(method)) {
                if (path.endsWith("/feedback")) {
                    handleFeedbackSubmission(exchange);
                } else if (path.endsWith("/feedback/stats")) {
                    handleFeedbackStats(exchange);
                } else if (path.endsWith("/feedback/export")) {
                    handleFeedbackExport(exchange);
                } else {
                    sendErrorResponse(exchange, 404, "Not Found");
                }
            } else if ("GET".equals(method)) {
                if (path.endsWith("/feedback/stats")) {
                    handleFeedbackStats(exchange);
                } else if (path.endsWith("/feedback/export")) {
                    handleFeedbackExport(exchange);
                } else {
                    sendErrorResponse(exchange, 404, "Not Found");
                }
            } else {
                sendErrorResponse(exchange, 405, "Method Not Allowed");
            }
        } catch (Exception e) {
            System.err.println("Error handling feedback request: " + e.getMessage());
            sendErrorResponse(exchange, 500, "Internal Server Error: " + e.getMessage());
        }
    }

    /**
     * Handle feedback submission from VSCode extension
     */
    private void handleFeedbackSubmission(HttpExchange exchange) throws IOException {
        // Read request body
        InputStream requestBody = exchange.getRequestBody();
        String body = new String(requestBody.readAllBytes(), StandardCharsets.UTF_8);
        
        // Parse JSON request
        JsonNode jsonNode = objectMapper.readTree(body);
        
        // Extract feedback data
        String userId = jsonNode.get("userId").asText();
        String sessionId = jsonNode.get("sessionId").asText();
        String conversationId = jsonNode.get("conversationId").asText();
        String originalPrompt = jsonNode.get("originalPrompt").asText();
        String originalResponse = jsonNode.get("originalResponse").asText();
        String interactionType = jsonNode.get("interaction").asText();
        String editedResponse = jsonNode.has("editedResponse") ? jsonNode.get("editedResponse").asText() : null;
        Double rating = jsonNode.has("rating") ? jsonNode.get("rating").asDouble() : null;
        String context = jsonNode.has("context") ? jsonNode.get("context").asText() : null;
        String reasoning = jsonNode.has("reasoning") ? jsonNode.get("reasoning").asText() : null;
        
        // Parse interaction type
        FeedbackRecord.InteractionType interaction;
        try {
            interaction = FeedbackRecord.InteractionType.valueOf(interactionType.toUpperCase());
        } catch (IllegalArgumentException e) {
            sendErrorResponse(exchange, 400, "Invalid interaction type: " + interactionType);
            return;
        }
        
        // Create feedback record
        FeedbackRecord feedbackRecord = new FeedbackRecord.Builder()
                .userId(userId)
                .sessionId(sessionId)
                .conversationId(conversationId)
                .originalPrompt(originalPrompt)
                .originalResponse(originalResponse)
                .interaction(interaction)
                .editedResponse(editedResponse)
                .rating(rating)
                .context(context)
                .reasoning(reasoning)
                .build();
        
        // Store feedback
        feedbackStore.storeFeedback(feedbackRecord);
        
        // Process feedback through orchestrator
        try {
            orchestrator.processFeedback(userId, interaction, editedResponse);
        } catch (Exception e) {
            System.err.println("Error processing feedback through orchestrator: " + e.getMessage());
        }
        
        // Send success response
        Map<String, Object> response = new HashMap<>();
        response.put("status", "success");
        response.put("message", "Feedback recorded successfully");
        response.put("feedbackId", feedbackRecord.getTimestamp().toString());
        
        sendJsonResponse(exchange, 200, response);
    }

    /**
     * Handle feedback statistics request
     */
    private void handleFeedbackStats(HttpExchange exchange) throws IOException {
        FeedbackStore.FeedbackStats stats = feedbackStore.getStats();
        
        Map<String, Object> response = new HashMap<>();
        response.put("totalRecords", stats.getTotalRecords());
        response.put("uniqueUsers", stats.getUniqueUsers());
        response.put("averageRating", stats.getAverageRating());
        response.put("typeCounts", stats.getTypeCounts());
        response.put("userCounts", stats.getUserCounts());
        
        sendJsonResponse(exchange, 200, response);
    }

    /**
     * Handle feedback export request
     */
    private void handleFeedbackExport(HttpExchange exchange) throws IOException {
        // Get query parameters
        String format = getQueryParameter(exchange, "format", "json");
        String userId = getQueryParameter(exchange, "userId", null);
        
        if ("json".equals(format)) {
            // Export as JSON
            String jsonData;
            if (userId != null) {
                jsonData = objectMapper.writeValueAsString(feedbackStore.getFeedbackForUser(userId));
            } else {
                jsonData = objectMapper.writeValueAsString(feedbackStore.getAllFeedback());
            }
            
            // Set response headers for file download
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.getResponseHeaders().set("Content-Disposition", "attachment; filename=feedback_export.json");
            
            byte[] responseBytes = jsonData.getBytes(StandardCharsets.UTF_8);
            exchange.sendResponseHeaders(200, responseBytes.length);
            
            try (OutputStream os = exchange.getResponseBody()) {
                os.write(responseBytes);
            }
        } else {
            sendErrorResponse(exchange, 400, "Unsupported format: " + format);
        }
    }

    /**
     * Send JSON response
     */
    private void sendJsonResponse(HttpExchange exchange, int statusCode, Object response) throws IOException {
        String jsonResponse = objectMapper.writeValueAsString(response);
        byte[] responseBytes = jsonResponse.getBytes(StandardCharsets.UTF_8);
        
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
        
        exchange.sendResponseHeaders(statusCode, responseBytes.length);
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(responseBytes);
        }
    }

    /**
     * Send error response
     */
    private void sendErrorResponse(HttpExchange exchange, int statusCode, String message) throws IOException {
        Map<String, Object> errorResponse = new HashMap<>();
        errorResponse.put("status", "error");
        errorResponse.put("message", message);
        
        sendJsonResponse(exchange, statusCode, errorResponse);
    }

    /**
     * Get query parameter value
     */
    private String getQueryParameter(HttpExchange exchange, String name, String defaultValue) {
        String query = exchange.getRequestURI().getQuery();
        if (query == null) {
            return defaultValue;
        }
        
        String[] params = query.split("&");
        for (String param : params) {
            String[] keyValue = param.split("=");
            if (keyValue.length == 2 && name.equals(keyValue[0])) {
                return keyValue[1];
            }
        }
        
        return defaultValue;
    }
}
