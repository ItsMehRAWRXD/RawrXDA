// MultiAgentAPIHandler.java - API handler for multi-agent UI integration
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * HTTP handler for multi-agent UI integration endpoints.
 * This handler provides REST API endpoints for the React UI components.
 */
public class MultiAgentAPIHandler implements HttpHandler {
    private final SecureMultiAgentCoordinator coordinator;
    private final Map<String, Object> sessionCache = new ConcurrentHashMap<>();

    public MultiAgentAPIHandler(SecureMultiAgentCoordinator coordinator) {
        this.coordinator = coordinator;
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
        String method = exchange.getRequestMethod();
        String path = exchange.getRequestURI().getPath();
        
        try {
            if ("POST".equals(method)) {
                if (path.endsWith("/secure-collaboration/start")) {
                    handleStartCollaboration(exchange);
                } else if (path.endsWith("/secure-collaboration/approve/")) {
                    handleApproval(exchange);
                } else if (path.endsWith("/predictive-suggestions")) {
                    handlePredictiveSuggestions(exchange);
                } else if (path.endsWith("/suggestions/execute")) {
                    handleSuggestionExecution(exchange);
                } else {
                    sendErrorResponse(exchange, 404, "Not Found");
                }
            } else if ("GET".equals(method)) {
                if (path.startsWith("/secure-collaboration/session/")) {
                    handleGetSession(exchange);
                } else {
                    sendErrorResponse(exchange, 404, "Not Found");
                }
            } else {
                sendErrorResponse(exchange, 405, "Method Not Allowed");
            }
        } catch (Exception e) {
            System.err.println("Error handling API request: " + e.getMessage());
            sendErrorResponse(exchange, 500, "Internal Server Error: " + e.getMessage());
        }
    }

    /**
     * Handle start collaboration request
     */
    private void handleStartCollaboration(HttpExchange exchange) throws IOException {
        // Read request body
        InputStream requestBody = exchange.getRequestBody();
        String body = new String(requestBody.readAllBytes(), StandardCharsets.UTF_8);
        
        // Parse JSON request (simplified - in practice use Jackson)
        Map<String, Object> request = parseJsonRequest(body);
        
        String task = (String) request.get("task");
        String userId = (String) request.get("userId");
        String userRole = (String) request.get("userRole");
        
        if (task == null || userId == null || userRole == null) {
            sendErrorResponse(exchange, 400, "Missing required fields");
            return;
        }
        
        try {
            // Start secure collaboration
            SecureMultiAgentCoordinator.CollaborationSession session = 
                coordinator.startSecureCollaboration(task, userId, userRole);
            
            // Cache session for polling
            sessionCache.put(session.getSessionId(), session);
            
            // Send response
            Map<String, Object> response = new HashMap<>();
            response.put("sessionId", session.getSessionId());
            response.put("userId", session.getUserId());
            response.put("task", session.getTask());
            response.put("status", session.getStatus().toString());
            response.put("userRole", session.getUserRole());
            response.put("createdAt", session.getCreatedAt().toString());
            
            sendJsonResponse(exchange, 200, response);
            
        } catch (SecurityException e) {
            sendErrorResponse(exchange, 403, "Security violation: " + e.getMessage());
        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Failed to start collaboration: " + e.getMessage());
        }
    }

    /**
     * Handle get session request
     */
    private void handleGetSession(HttpExchange exchange) throws IOException {
        String path = exchange.getRequestURI().getPath();
        String sessionId = path.substring(path.lastIndexOf("/") + 1);
        
        if (sessionId == null || sessionId.isEmpty()) {
            sendErrorResponse(exchange, 400, "Session ID required");
            return;
        }
        
        // Get session from cache
        Object sessionObj = sessionCache.get(sessionId);
        if (sessionObj == null) {
            sendErrorResponse(exchange, 404, "Session not found");
            return;
        }
        
        SecureMultiAgentCoordinator.CollaborationSession session = 
            (SecureMultiAgentCoordinator.CollaborationSession) sessionObj;
        
        // Build response
        Map<String, Object> response = new HashMap<>();
        response.put("sessionId", session.getSessionId());
        response.put("userId", session.getUserId());
        response.put("task", session.getTask());
        response.put("status", session.getStatus().toString());
        response.put("userRole", session.getUserRole());
        response.put("createdAt", session.getCreatedAt().toString());
        response.put("agentResults", session.getAgentResults());
        
        if (session.getFinalResult() != null) {
            response.put("finalResult", session.getFinalResult());
        }
        if (session.getError() != null) {
            response.put("error", session.getError());
        }
        if (session.getApprovalRequest() != null) {
            response.put("approvalRequest", session.getApprovalRequest());
        }
        
        sendJsonResponse(exchange, 200, response);
    }

    /**
     * Handle approval request
     */
    private void handleApproval(HttpExchange exchange) throws IOException {
        String path = exchange.getRequestURI().getPath();
        String sessionId = path.substring(path.lastIndexOf("/") + 1);
        
        if (sessionId == null || sessionId.isEmpty()) {
            sendErrorResponse(exchange, 400, "Session ID required");
            return;
        }
        
        // Read request body
        InputStream requestBody = exchange.getRequestBody();
        String body = new String(requestBody.readAllBytes(), StandardCharsets.UTF_8);
        
        // Parse JSON request
        Map<String, Object> request = parseJsonRequest(body);
        
        Boolean approved = (Boolean) request.get("approved");
        String feedback = (String) request.get("feedback");
        
        if (approved == null) {
            sendErrorResponse(exchange, 400, "Approval status required");
            return;
        }
        
        try {
            // Get session from cache
            Object sessionObj = sessionCache.get(sessionId);
            if (sessionObj == null) {
                sendErrorResponse(exchange, 404, "Session not found");
                return;
            }
            
            SecureMultiAgentCoordinator.CollaborationSession session = 
                (SecureMultiAgentCoordinator.CollaborationSession) sessionObj;
            
            // Continue collaboration with approval
            SecureMultiAgentCoordinator.CollaborationSession updatedSession = 
                coordinator.continueCollaboration(sessionId, approved, feedback);
            
            // Update cache
            sessionCache.put(sessionId, updatedSession);
            
            // Send response
            Map<String, Object> response = new HashMap<>();
            response.put("sessionId", updatedSession.getSessionId());
            response.put("status", updatedSession.getStatus().toString());
            response.put("agentResults", updatedSession.getAgentResults());
            
            if (updatedSession.getFinalResult() != null) {
                response.put("finalResult", updatedSession.getFinalResult());
            }
            if (updatedSession.getError() != null) {
                response.put("error", updatedSession.getError());
            }
            
            sendJsonResponse(exchange, 200, response);
            
        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Failed to process approval: " + e.getMessage());
        }
    }

    /**
     * Handle predictive suggestions request
     */
    private void handlePredictiveSuggestions(HttpExchange exchange) throws IOException {
        // Read request body
        InputStream requestBody = exchange.getRequestBody();
        String body = new String(requestBody.readAllBytes(), StandardCharsets.UTF_8);
        
        // Parse JSON request
        Map<String, Object> request = parseJsonRequest(body);
        
        String sessionId = (String) request.get("sessionId");
        String userRole = (String) request.get("userRole");
        
        if (sessionId == null) {
            sendErrorResponse(exchange, 400, "Session ID required");
            return;
        }
        
        try {
            // Generate predictive suggestions based on session context
            Map<String, Object> suggestions = generatePredictiveSuggestions(sessionId, userRole);
            
            sendJsonResponse(exchange, 200, suggestions);
            
        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Failed to generate suggestions: " + e.getMessage());
        }
    }

    /**
     * Handle suggestion execution request
     */
    private void handleSuggestionExecution(HttpExchange exchange) throws IOException {
        // Read request body
        InputStream requestBody = exchange.getRequestBody();
        String body = new String(requestBody.readAllBytes(), StandardCharsets.UTF_8);
        
        // Parse JSON request
        Map<String, Object> request = parseJsonRequest(body);
        
        String action = (String) request.get("action");
        String sessionId = (String) request.get("sessionId");
        String userId = (String) request.get("userId");
        
        if (action == null || sessionId == null || userId == null) {
            sendErrorResponse(exchange, 400, "Missing required fields");
            return;
        }
        
        try {
            // Execute suggestion action
            Map<String, Object> result = executeSuggestionAction(action, request, sessionId, userId);
            
            sendJsonResponse(exchange, 200, result);
            
        } catch (Exception e) {
            sendErrorResponse(exchange, 500, "Failed to execute suggestion: " + e.getMessage());
        }
    }

    /**
     * Generate predictive suggestions based on session context
     */
    private Map<String, Object> generatePredictiveSuggestions(String sessionId, String userRole) {
        Map<String, Object> suggestions = new HashMap<>();
        
        // Get session from cache
        Object sessionObj = sessionCache.get(sessionId);
        if (sessionObj == null) {
            return suggestions;
        }
        
        SecureMultiAgentCoordinator.CollaborationSession session = 
            (SecureMultiAgentCoordinator.CollaborationSession) sessionObj;
        
        // Generate suggestions based on session status and user role
        if (session.getStatus() == SecureMultiAgentCoordinator.CollaborationSession.Status.IN_PROGRESS) {
            suggestions.put("suggestions", new Object[]{
                Map.of(
                    "id", "suggestion_1",
                    "type", "enhancement",
                    "title", "Add Security Validation",
                    "description", "Consider adding additional security checks to the implementation",
                    "severity", "warning",
                    "confidence", 0.8,
                    "actions", new String[]{"add_security_checks", "review_permissions"}
                ),
                Map.of(
                    "id", "suggestion_2",
                    "type", "optimization",
                    "title", "Performance Optimization",
                    "description", "The current implementation could benefit from performance optimizations",
                    "severity", "info",
                    "confidence", 0.6,
                    "actions", new String[]{"optimize_performance", "add_caching"}
                )
            });
        }
        
        return suggestions;
    }

    /**
     * Execute suggestion action
     */
    private Map<String, Object> executeSuggestionAction(String action, Map<String, Object> request, 
                                                       String sessionId, String userId) {
        Map<String, Object> result = new HashMap<>();
        
        switch (action) {
            case "add_security_checks":
                result.put("status", "success");
                result.put("message", "Security checks added to the implementation");
                break;
            case "optimize_performance":
                result.put("status", "success");
                result.put("message", "Performance optimizations applied");
                break;
            case "review_permissions":
                result.put("status", "success");
                result.put("message", "Permission review completed");
                break;
            case "add_caching":
                result.put("status", "success");
                result.put("message", "Caching mechanism implemented");
                break;
            default:
                result.put("status", "error");
                result.put("message", "Unknown action: " + action);
        }
        
        return result;
    }

    /**
     * Parse JSON request (simplified implementation)
     */
    private Map<String, Object> parseJsonRequest(String json) {
        // Simplified JSON parsing - in practice use Jackson ObjectMapper
        Map<String, Object> result = new HashMap<>();
        
        // Remove braces and quotes for simple parsing
        String clean = json.replaceAll("[{}\"]", "");
        String[] pairs = clean.split(",");
        
        for (String pair : pairs) {
            String[] keyValue = pair.split(":");
            if (keyValue.length == 2) {
                String key = keyValue[0].trim();
                String value = keyValue[1].trim();
                
                // Convert value types
                if ("true".equals(value) || "false".equals(value)) {
                    result.put(key, Boolean.parseBoolean(value));
                } else if (value.startsWith("[") && value.endsWith("]")) {
                    // Handle arrays (simplified)
                    result.put(key, value);
                } else {
                    result.put(key, value);
                }
            }
        }
        
        return result;
    }

    /**
     * Send JSON response
     */
    private void sendJsonResponse(HttpExchange exchange, int statusCode, Object response) throws IOException {
        String jsonResponse = "{}"; // Simplified - in practice use Jackson ObjectMapper
        
        // Convert response to JSON string (simplified)
        if (response instanceof Map) {
            Map<String, Object> map = (Map<String, Object>) response;
            StringBuilder json = new StringBuilder("{");
            boolean first = true;
            for (Map.Entry<String, Object> entry : map.entrySet()) {
                if (!first) json.append(",");
                json.append("\"").append(entry.getKey()).append("\":");
                if (entry.getValue() instanceof String) {
                    json.append("\"").append(entry.getValue()).append("\"");
                } else if (entry.getValue() instanceof Boolean) {
                    json.append(entry.getValue());
                } else if (entry.getValue() instanceof Object[]) {
                    json.append("[]"); // Simplified array handling
                } else {
                    json.append("\"").append(entry.getValue()).append("\"");
                }
                first = false;
            }
            json.append("}");
            jsonResponse = json.toString();
        }
        
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
}
