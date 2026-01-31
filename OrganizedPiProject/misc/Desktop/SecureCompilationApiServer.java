// SecureCompilationApiServer.java - Secure compilation API with comprehensive security

import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import java.io.*;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import javax.json.*;

/**
 * Secure compilation API server that provides safe code compilation services
 * with comprehensive security controls and sandboxed execution.
 */
public class SecureCompilationApiServer {
    private static final Logger logger = Logger.getLogger(SecureCompilationApiServer.class.getName());
    
    // Server configuration
    private final int port;
    private final String host;
    private final int maxConcurrentRequests;
    private final Duration requestTimeout;
    
    // Security components
    private final SecurityAnalyzer securityAnalyzer;
    private final SafetyMechanisms safetyMechanisms;
    private final DeploymentManager deploymentManager;
    private final MonitoringSystem monitoringSystem;
    
    // Server state
    private HttpServer server;
    private volatile boolean running = false;
    
    // Request tracking
    private final Map<String, CompilationRequest> activeRequests = new HashMap<>();
    private final Map<String, CompilationResult> compilationCache = new HashMap<>();
    
    public SecureCompilationApiServer(int port, String host) {
        this.port = port;
        this.host = host;
        this.maxConcurrentRequests = 100;
        this.requestTimeout = Duration.ofMinutes(5);
        
        // Initialize security components
        this.securityAnalyzer = new SecurityAnalyzer();
        this.safetyMechanisms = new SafetyMechanisms(true);
        this.deploymentManager = new DeploymentManager();
        this.monitoringSystem = new MonitoringSystem();
    }
    
    /**
     * Start the secure compilation API server
     */
    public void start() throws IOException {
        if (running) {
            logger.warning("Server is already running");
            return;
        }
        
        logger.info("Starting Secure Compilation API Server on " + host + ":" + port);
        
        // Create HTTP server
        server = HttpServer.create(new InetSocketAddress(host, port), 0);
        
        // Set up routes
        setupRoutes();
        
        // Configure server
        server.setExecutor(Executors.newFixedThreadPool(maxConcurrentRequests));
        
        // Start monitoring
        monitoringSystem.start();
        
        // Start server
        server.start();
        running = true;
        
        logger.info("Secure Compilation API Server started successfully");
    }
    
    /**
     * Stop the server
     */
    public void stop() {
        if (!running) {
            logger.warning("Server is not running");
            return;
        }
        
        logger.info("Stopping Secure Compilation API Server...");
        
        running = false;
        
        if (server != null) {
            server.stop(30); // 30 second grace period
        }
        
        // Stop monitoring
        monitoringSystem.stop();
        
        // Cleanup deployment manager
        deploymentManager.cleanup();
        
        logger.info("Secure Compilation API Server stopped");
    }
    
    /**
     * Setup API routes
     */
    private void setupRoutes() {
        // Health check endpoint
        server.createContext("/health", new HealthCheckHandler());
        
        // Compilation endpoints
        server.createContext("/compile", new CompilationHandler());
        server.createContext("/compile/status", new CompilationStatusHandler());
        server.createContext("/compile/result", new CompilationResultHandler());
        
        // Security endpoints
        server.createContext("/security/analyze", new SecurityAnalysisHandler());
        server.createContext("/security/validate", new SecurityValidationHandler());
        
        // Monitoring endpoints
        server.createContext("/metrics", new MetricsHandler());
        server.createContext("/status", new StatusHandler());
        
        // Admin endpoints
        server.createContext("/admin/requests", new AdminRequestsHandler());
        server.createContext("/admin/cache", new AdminCacheHandler());
    }
    
    /**
     * Health check handler
     */
    private class HealthCheckHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                JsonObject response = Json.createObjectBuilder()
                    .add("status", "healthy")
                    .add("timestamp", Instant.now().toString())
                    .add("uptime", getUptime())
                    .add("activeRequests", activeRequests.size())
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Health check failed", e);
                sendErrorResponse(exchange, 500, "Health check failed");
            }
        }
    }
    
    /**
     * Compilation handler
     */
    private class CompilationHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                // Parse request
                String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
                JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
                
                // Extract parameters
                String code = request.getString("code");
                String language = request.getString("language", "java");
                String userId = request.getString("userId", "anonymous");
                String sessionId = request.getString("sessionId", UUID.randomUUID().toString());
                
                // Security validation
                if (!validateCompilationRequest(code, language, userId)) {
                    sendErrorResponse(exchange, 400, "Security validation failed");
                    return;
                }
                
                // Create compilation request
                CompilationRequest compilationRequest = new CompilationRequest(
                    UUID.randomUUID().toString(), code, language, userId, sessionId, Instant.now());
                
                // Check cache first
                String cacheKey = generateCacheKey(code, language);
                CompilationResult cachedResult = compilationCache.get(cacheKey);
                if (cachedResult != null && !cachedResult.isExpired()) {
                    sendJsonResponse(exchange, 200, createCompilationResponse(cachedResult));
                    return;
                }
                
                // Start compilation
                activeRequests.put(compilationRequest.getId(), compilationRequest);
                
                // Execute compilation in background
                Executors.newSingleThreadExecutor().submit(() -> {
                    try {
                        CompilationResult result = executeCompilation(compilationRequest);
                        compilationCache.put(cacheKey, result);
                        activeRequests.remove(compilationRequest.getId());
                    } catch (Exception e) {
                        logger.log(Level.WARNING, "Compilation failed", e);
                        activeRequests.remove(compilationRequest.getId());
                    }
                });
                
                // Return request ID
                JsonObject response = Json.createObjectBuilder()
                    .add("requestId", compilationRequest.getId())
                    .add("status", "accepted")
                    .add("message", "Compilation request accepted")
                    .build();
                
                sendJsonResponse(exchange, 202, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Compilation request failed", e);
                sendErrorResponse(exchange, 500, "Compilation request failed");
            }
        }
    }
    
    /**
     * Compilation status handler
     */
    private class CompilationStatusHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                String requestId = getQueryParameter(exchange, "requestId");
                if (requestId == null) {
                    sendErrorResponse(exchange, 400, "Missing requestId parameter");
                    return;
                }
                
                CompilationRequest request = activeRequests.get(requestId);
                if (request == null) {
                    sendErrorResponse(exchange, 404, "Request not found");
                    return;
                }
                
                JsonObject response = Json.createObjectBuilder()
                    .add("requestId", requestId)
                    .add("status", request.getStatus().toString())
                    .add("progress", request.getProgress())
                    .add("message", request.getMessage())
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Status check failed", e);
                sendErrorResponse(exchange, 500, "Status check failed");
            }
        }
    }
    
    /**
     * Compilation result handler
     */
    private class CompilationResultHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                String requestId = getQueryParameter(exchange, "requestId");
                if (requestId == null) {
                    sendErrorResponse(exchange, 400, "Missing requestId parameter");
                    return;
                }
                
                // Check active requests first
                CompilationRequest request = activeRequests.get(requestId);
                if (request != null) {
                    if (request.getStatus() == CompilationRequest.Status.COMPLETED) {
                        CompilationResult result = request.getResult();
                        if (result != null) {
                            sendJsonResponse(exchange, 200, createCompilationResponse(result));
                            return;
                        }
                    } else {
                        sendErrorResponse(exchange, 202, "Compilation still in progress");
                        return;
                    }
                }
                
                // Check cache
                String cacheKey = getQueryParameter(exchange, "cacheKey");
                if (cacheKey != null) {
                    CompilationResult cachedResult = compilationCache.get(cacheKey);
                    if (cachedResult != null && !cachedResult.isExpired()) {
                        sendJsonResponse(exchange, 200, createCompilationResponse(cachedResult));
                        return;
                    }
                }
                
                sendErrorResponse(exchange, 404, "Result not found");
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Result retrieval failed", e);
                sendErrorResponse(exchange, 500, "Result retrieval failed");
            }
        }
    }
    
    /**
     * Security analysis handler
     */
    private class SecurityAnalysisHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
                JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
                
                String code = request.getString("code");
                String userId = request.getString("userId", "anonymous");
                
                SecurityAnalyzer.SecurityAnalysisResult result = securityAnalyzer.analyzeCode(code);
                
                JsonObject response = Json.createObjectBuilder()
                    .add("safe", result.isSafe())
                    .add("threats", Json.createArrayBuilder(
                        result.getThreats().stream()
                            .map(threat -> Json.createObjectBuilder()
                                .add("type", threat.getType())
                                .add("description", threat.getDescription())
                                .add("severity", threat.getSeverity())
                                .build())
                            .toArray(JsonObject[]::new)))
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Security analysis failed", e);
                sendErrorResponse(exchange, 500, "Security analysis failed");
            }
        }
    }
    
    /**
     * Security validation handler
     */
    private class SecurityValidationHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8);
                JsonObject request = Json.createReader(new StringReader(requestBody)).readObject();
                
                String code = request.getString("code");
                String userId = request.getString("userId", "anonymous");
                
                boolean isValid = safetyMechanisms.validateUserRequest(code, userId);
                
                JsonObject response = Json.createObjectBuilder()
                    .add("valid", isValid)
                    .add("message", isValid ? "Code is safe" : "Code contains security violations")
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Security validation failed", e);
                sendErrorResponse(exchange, 500, "Security validation failed");
            }
        }
    }
    
    /**
     * Metrics handler
     */
    private class MetricsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                MonitoringSystem.PerformanceMetrics metrics = monitoringSystem.getPerformanceMetrics();
                
                JsonObject response = Json.createObjectBuilder()
                    .add("timestamp", metrics.getTimestamp().toString())
                    .add("totalRequests", metrics.getTotalRequests())
                    .add("successfulRequests", metrics.getSuccessfulRequests())
                    .add("failedRequests", metrics.getFailedRequests())
                    .add("activeConnections", metrics.getActiveConnections())
                    .add("averageExecutionTime", metrics.getAverageExecutionTime())
                    .add("successRate", metrics.getSuccessRate())
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Metrics retrieval failed", e);
                sendErrorResponse(exchange, 500, "Metrics retrieval failed");
            }
        }
    }
    
    /**
     * Status handler
     */
    private class StatusHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                MonitoringSystem.SystemHealthStatus status = monitoringSystem.getSystemHealthStatus();
                
                JsonObject response = Json.createObjectBuilder()
                    .add("timestamp", status.getTimestamp().toString())
                    .add("overallStatus", status.getOverallStatus().toString())
                    .add("activeRequests", activeRequests.size())
                    .add("cacheSize", compilationCache.size())
                    .add("uptime", getUptime())
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Status retrieval failed", e);
                sendErrorResponse(exchange, 500, "Status retrieval failed");
            }
        }
    }
    
    /**
     * Admin requests handler
     */
    private class AdminRequestsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                JsonArray requests = Json.createArrayBuilder(
                    activeRequests.values().stream()
                        .map(request -> Json.createObjectBuilder()
                            .add("id", request.getId())
                            .add("userId", request.getUserId())
                            .add("language", request.getLanguage())
                            .add("status", request.getStatus().toString())
                            .add("createdAt", request.getCreatedAt().toString())
                            .build())
                        .toArray(JsonObject[]::new))
                    .build();
                
                JsonObject response = Json.createObjectBuilder()
                    .add("requests", requests)
                    .add("count", activeRequests.size())
                    .build();
                
                sendJsonResponse(exchange, 200, response);
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Admin requests retrieval failed", e);
                sendErrorResponse(exchange, 500, "Admin requests retrieval failed");
            }
        }
    }
    
    /**
     * Admin cache handler
     */
    private class AdminCacheHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            try {
                String action = getQueryParameter(exchange, "action");
                
                if ("clear".equals(action)) {
                    compilationCache.clear();
                    JsonObject response = Json.createObjectBuilder()
                        .add("message", "Cache cleared successfully")
                        .build();
                    sendJsonResponse(exchange, 200, response);
                } else {
                    JsonObject response = Json.createObjectBuilder()
                        .add("cacheSize", compilationCache.size())
                        .add("actions", Json.createArrayBuilder()
                            .add("clear")
                            .build())
                        .build();
                    sendJsonResponse(exchange, 200, response);
                }
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Admin cache operation failed", e);
                sendErrorResponse(exchange, 500, "Admin cache operation failed");
            }
        }
    }
    
    /**
     * Validate compilation request
     */
    private boolean validateCompilationRequest(String code, String language, String userId) {
        try {
            // Security analysis
            SecurityAnalyzer.SecurityAnalysisResult securityResult = securityAnalyzer.analyzeCode(code);
            if (!securityResult.isSafe()) {
                logger.warning("Security analysis failed for user: " + userId);
                return false;
            }
            
            // Safety validation
            if (!safetyMechanisms.validateUserRequest(code, userId)) {
                logger.warning("Safety validation failed for user: " + userId);
                return false;
            }
            
            // Language validation
            if (!isSupportedLanguage(language)) {
                logger.warning("Unsupported language: " + language);
                return false;
            }
            
            return true;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Request validation failed", e);
            return false;
        }
    }
    
    /**
     * Execute compilation
     */
    private CompilationResult executeCompilation(CompilationRequest request) {
        try {
            // Update request status
            request.setStatus(CompilationRequest.Status.RUNNING);
            request.setProgress(10);
            request.setMessage("Starting compilation...");
            
            // Deploy in sandbox
            String deploymentId = deploymentManager.deployTask(createTaskFromRequest(request));
            
            // Update progress
            request.setProgress(50);
            request.setMessage("Compiling code...");
            
            // Simulate compilation (in real implementation, this would compile the code)
            Thread.sleep(2000); // Simulate compilation time
            
            // Update progress
            request.setProgress(90);
            request.setMessage("Finalizing compilation...");
            
            // Create result
            CompilationResult result = new CompilationResult(
                request.getId(), true, "Compilation successful", 
                "Compiled output", Instant.now());
            
            // Update request
            request.setStatus(CompilationRequest.Status.COMPLETED);
            request.setProgress(100);
            request.setMessage("Compilation completed");
            request.setResult(result);
            
            // Cleanup deployment
            deploymentManager.cleanupDeployment(deploymentId);
            
            return result;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Compilation failed", e);
            
            CompilationResult result = new CompilationResult(
                request.getId(), false, "Compilation failed: " + e.getMessage(), 
                null, Instant.now());
            
            request.setStatus(CompilationRequest.Status.FAILED);
            request.setMessage("Compilation failed");
            request.setResult(result);
            
            return result;
        }
    }
    
    /**
     * Create task from compilation request
     */
    private AgenticOrchestrator.Task createTaskFromRequest(CompilationRequest request) {
        AgenticOrchestrator.Task task = new AgenticOrchestrator.Task("plugin_execution", "Compile " + request.getLanguage() + " code");
        task.setCode(request.getCode());
        task.setClassName("CompiledCode");
        return task;
    }
    
    /**
     * Generate cache key
     */
    private String generateCacheKey(String code, String language) {
        return language + ":" + code.hashCode();
    }
    
    /**
     * Check if language is supported
     */
    private boolean isSupportedLanguage(String language) {
        String[] supportedLanguages = {"java", "javascript", "python", "cpp", "c"};
        return Arrays.asList(supportedLanguages).contains(language.toLowerCase());
    }
    
    /**
     * Get query parameter
     */
    private String getQueryParameter(HttpExchange exchange, String name) {
        String query = exchange.getRequestURI().getQuery();
        if (query == null) return null;
        
        String[] params = query.split("&");
        for (String param : params) {
            String[] keyValue = param.split("=");
            if (keyValue.length == 2 && name.equals(keyValue[0])) {
                return keyValue[1];
            }
        }
        return null;
    }
    
    /**
     * Send JSON response
     */
    private void sendJsonResponse(HttpExchange exchange, int statusCode, JsonObject response) throws IOException {
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.sendResponseHeaders(statusCode, 0);
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response.toString().getBytes(StandardCharsets.UTF_8));
        }
    }
    
    /**
     * Send error response
     */
    private void sendErrorResponse(HttpExchange exchange, int statusCode, String message) throws IOException {
        JsonObject error = Json.createObjectBuilder()
            .add("error", message)
            .add("status", statusCode)
            .add("timestamp", Instant.now().toString())
            .build();
        
        sendJsonResponse(exchange, statusCode, error);
    }
    
    /**
     * Create compilation response
     */
    private JsonObject createCompilationResponse(CompilationResult result) {
        return Json.createObjectBuilder()
            .add("requestId", result.getRequestId())
            .add("success", result.isSuccess())
            .add("message", result.getMessage())
            .add("output", result.getOutput() != null ? result.getOutput() : "")
            .add("timestamp", result.getTimestamp().toString())
            .build();
    }
    
    /**
     * Get server uptime
     */
    private String getUptime() {
        // In a real implementation, this would calculate actual uptime
        return "PT1H30M"; // Placeholder
    }
    
    /**
     * Compilation request class
     */
    public static class CompilationRequest {
        public enum Status { PENDING, RUNNING, COMPLETED, FAILED }
        
        private final String id;
        private final String code;
        private final String language;
        private final String userId;
        private final String sessionId;
        private final Instant createdAt;
        private Status status;
        private int progress;
        private String message;
        private CompilationResult result;
        
        public CompilationRequest(String id, String code, String language, String userId, String sessionId, Instant createdAt) {
            this.id = id;
            this.code = code;
            this.language = language;
            this.userId = userId;
            this.sessionId = sessionId;
            this.createdAt = createdAt;
            this.status = Status.PENDING;
            this.progress = 0;
            this.message = "Request created";
        }
        
        // Getters and setters
        public String getId() { return id; }
        public String getCode() { return code; }
        public String getLanguage() { return language; }
        public String getUserId() { return userId; }
        public String getSessionId() { return sessionId; }
        public Instant getCreatedAt() { return createdAt; }
        public Status getStatus() { return status; }
        public void setStatus(Status status) { this.status = status; }
        public int getProgress() { return progress; }
        public void setProgress(int progress) { this.progress = progress; }
        public String getMessage() { return message; }
        public void setMessage(String message) { this.message = message; }
        public CompilationResult getResult() { return result; }
        public void setResult(CompilationResult result) { this.result = result; }
    }
    
    /**
     * Compilation result class
     */
    public static class CompilationResult {
        private final String requestId;
        private final boolean success;
        private final String message;
        private final String output;
        private final Instant timestamp;
        
        public CompilationResult(String requestId, boolean success, String message, String output, Instant timestamp) {
            this.requestId = requestId;
            this.success = success;
            this.message = message;
            this.output = output;
            this.timestamp = timestamp;
        }
        
        public boolean isExpired() {
            return timestamp.isBefore(Instant.now().minusSeconds(3600)); // 1 hour
        }
        
        // Getters
        public String getRequestId() { return requestId; }
        public boolean isSuccess() { return success; }
        public String getMessage() { return message; }
        public String getOutput() { return output; }
        public Instant getTimestamp() { return timestamp; }
    }
}
