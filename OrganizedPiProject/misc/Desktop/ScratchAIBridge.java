import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.*;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;
import java.util.concurrent.Executors;

/**
 * Scratch AI Bridge Server
 * 
 * Provides HTTP endpoints for Scratch extensions to communicate with local AI tools.
 * Integrates with existing Java and PHP AI clients for seamless AI assistance.
 * 
 * Features:
 * - RESTful API for AI queries
 * - Support for both Java and PHP AI clients
 * - Compilation service integration
 * - CORS support for Scratch extensions
 * - Error handling and logging
 * - Configurable via environment variables
 */
public class ScratchAIBridge {
    
    private static final ObjectMapper objectMapper = new ObjectMapper();
    private static final int DEFAULT_PORT = 8001;
    private static final String DEFAULT_AI_CLIENT = "java"; // or "php"
    
    // Configuration from environment
    private static final String PORT_ENV = "SCRATCH_BRIDGE_PORT";
    private static final String AI_CLIENT_ENV = "SCRATCH_AI_CLIENT";
    private static final String GEMINI_API_KEY_ENV = "GEMINI_API_KEY";
    private static final String MODEL_ENV = "SCRATCH_AI_MODEL";
    private static final String SYSTEM_ENV = "SCRATCH_AI_SYSTEM";
    
    // Default configuration
    private static final Map<String, String> config = new HashMap<>();
    
    static {
        config.put("port", System.getenv().getOrDefault(PORT_ENV, String.valueOf(DEFAULT_PORT)));
        config.put("ai_client", System.getenv().getOrDefault(AI_CLIENT_ENV, DEFAULT_AI_CLIENT));
        config.put("model", System.getenv().getOrDefault(MODEL_ENV, "gemini-1.5-flash"));
        config.put("system", System.getenv().getOrDefault(SYSTEM_ENV, 
            "You are an AI assistant helping users with programming in Scratch. " +
            "Provide clear, helpful explanations and code examples when appropriate."));
    }

    public static void main(String[] args) throws IOException {
        int port = Integer.parseInt(config.get("port"));
        HttpServer server = HttpServer.create(new InetSocketAddress(port), 0);
        
        // Create contexts
        server.createContext("/ask", new AskHandler());
        server.createContext("/search", new SearchHandler());
        server.createContext("/compile", new CompileHandler());
        server.createContext("/health", new HealthHandler());
        server.createContext("/config", new ConfigHandler());
        
        // Enable CORS
        server.setExecutor(Executors.newCachedThreadPool());
        server.start();
        
        System.out.println("=== Scratch AI Bridge Server ===");
        System.out.println("Listening on: http://localhost:" + port);
        System.out.println("AI Client: " + config.get("ai_client"));
        System.out.println("Model: " + config.get("model"));
        System.out.println("Endpoints:");
        System.out.println("  POST /ask - Ask AI directly");
        System.out.println("  POST /search - Search with AI");
        System.out.println("  POST /compile - Compile code");
        System.out.println("  GET /health - Health check");
        System.out.println("  GET /config - Get configuration");
        System.out.println("================================");
    }

    /**
     * Base handler with CORS support and common functionality
     */
    abstract static class BaseHandler implements HttpHandler {
        
        protected void setCorsHeaders(HttpExchange exchange) {
            exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
            exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type, Authorization");
            exchange.getResponseHeaders().set("Access-Control-Max-Age", "86400");
        }
        
        protected void handleOptions(HttpExchange exchange) throws IOException {
            setCorsHeaders(exchange);
            exchange.sendResponseHeaders(204, -1);
            exchange.close();
        }
        
        protected String readRequestBody(HttpExchange exchange) throws IOException {
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(exchange.getRequestBody(), StandardCharsets.UTF_8))) {
                return reader.lines().collect(Collectors.joining("\n"));
            }
        }
        
        protected void sendJsonResponse(HttpExchange exchange, int statusCode, Object data) throws IOException {
            setCorsHeaders(exchange);
            String jsonResponse = objectMapper.writeValueAsString(data);
            exchange.getResponseHeaders().set("Content-Type", "application/json");
            exchange.sendResponseHeaders(statusCode, jsonResponse.getBytes(StandardCharsets.UTF_8).length);
            try (OutputStream os = exchange.getResponseBody()) {
                os.write(jsonResponse.getBytes(StandardCharsets.UTF_8));
            }
        }
        
        protected void sendTextResponse(HttpExchange exchange, int statusCode, String text) throws IOException {
            setCorsHeaders(exchange);
            exchange.getResponseHeaders().set("Content-Type", "text/plain; charset=utf-8");
            byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
            exchange.sendResponseHeaders(statusCode, bytes.length);
            try (OutputStream os = exchange.getResponseBody()) {
                os.write(bytes);
            }
        }
        
        protected ObjectNode createErrorResponse(String error, String details) {
            ObjectNode errorNode = objectMapper.createObjectNode();
            errorNode.put("success", false);
            errorNode.put("error", error);
            if (details != null) {
                errorNode.put("details", details);
            }
            return errorNode;
        }
        
        protected ObjectNode createSuccessResponse(Object data) {
            ObjectNode successNode = objectMapper.createObjectNode();
            successNode.put("success", true);
            if (data instanceof String) {
                successNode.put("data", (String) data);
            } else {
                successNode.set("data", objectMapper.valueToTree(data));
            }
            return successNode;
        }
    }

    /**
     * Handle direct AI questions
     */
    static class AskHandler extends BaseHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equals(exchange.getRequestMethod())) {
                handleOptions(exchange);
                return;
            }
            
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendJsonResponse(exchange, 405, createErrorResponse("Method not allowed", "Only POST is supported"));
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                JsonNode request = objectMapper.readTree(requestBody);
                
                String prompt = request.has("prompt") ? request.get("prompt").asText() : null;
                if (prompt == null || prompt.trim().isEmpty()) {
                    sendJsonResponse(exchange, 400, createErrorResponse("Missing prompt", "Please provide a prompt"));
                    return;
                }
                
                String aiResponse = callAI(prompt, "ask");
                sendJsonResponse(exchange, 200, createSuccessResponse(aiResponse));
                
            } catch (Exception e) {
                sendJsonResponse(exchange, 500, createErrorResponse("AI call failed", e.getMessage()));
            }
        }
    }

    /**
     * Handle AI search queries
     */
    static class SearchHandler extends BaseHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equals(exchange.getRequestMethod())) {
                handleOptions(exchange);
                return;
            }
            
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendJsonResponse(exchange, 405, createErrorResponse("Method not allowed", "Only POST is supported"));
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                JsonNode request = objectMapper.readTree(requestBody);
                
                String query = request.has("query") ? request.get("query").asText() : null;
                if (query == null || query.trim().isEmpty()) {
                    sendJsonResponse(exchange, 400, createErrorResponse("Missing query", "Please provide a search query"));
                    return;
                }
                
                String searchPrompt = "Using your knowledge, synthesize a concise answer for: " + query;
                String aiResponse = callAI(searchPrompt, "search");
                sendJsonResponse(exchange, 200, createSuccessResponse(aiResponse));
                
            } catch (Exception e) {
                sendJsonResponse(exchange, 500, createErrorResponse("AI search failed", e.getMessage()));
            }
        }
    }

    /**
     * Handle code compilation requests
     */
    static class CompileHandler extends BaseHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equals(exchange.getRequestMethod())) {
                handleOptions(exchange);
                return;
            }
            
            if (!"POST".equals(exchange.getRequestMethod())) {
                sendJsonResponse(exchange, 405, createErrorResponse("Method not allowed", "Only POST is supported"));
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                JsonNode request = objectMapper.readTree(requestBody);
                
                String language = request.has("language") ? request.get("language").asText() : null;
                String filename = request.has("filename") ? request.get("filename").asText() : null;
                String source = request.has("source") ? request.get("source").asText() : null;
                
                if (language == null || filename == null || source == null) {
                    sendJsonResponse(exchange, 400, createErrorResponse("Missing parameters", 
                        "Please provide language, filename, and source"));
                    return;
                }
                
                // Call the secure compilation service
                String compileResponse = callCompileService(language, filename, source);
                sendJsonResponse(exchange, 200, createSuccessResponse(compileResponse));
                
            } catch (Exception e) {
                sendJsonResponse(exchange, 500, createErrorResponse("Compilation failed", e.getMessage()));
            }
        }
    }

    /**
     * Health check endpoint
     */
    static class HealthHandler extends BaseHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            ObjectNode health = objectMapper.createObjectNode();
            health.put("status", "healthy");
            health.put("timestamp", System.currentTimeMillis());
            health.put("ai_client", config.get("ai_client"));
            health.put("model", config.get("model"));
            
            sendJsonResponse(exchange, 200, health);
        }
    }

    /**
     * Configuration endpoint
     */
    static class ConfigHandler extends BaseHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            ObjectNode configNode = objectMapper.createObjectNode();
            configNode.put("port", config.get("port"));
            configNode.put("ai_client", config.get("ai_client"));
            configNode.put("model", config.get("model"));
            configNode.put("system_instruction", config.get("system"));
            
            sendJsonResponse(exchange, 200, configNode);
        }
    }

    /**
     * Call the appropriate AI client based on configuration
     */
    private static String callAI(String prompt, String mode) throws IOException, InterruptedException {
        String aiClient = config.get("ai_client");
        String model = config.get("model");
        String system = config.get("system");
        
        ProcessBuilder builder;
        String[] command;
        
        if ("php".equals(aiClient)) {
            // Use PHP AI client
            command = new String[]{
                "php", "ai_cli.php",
                "--ask", prompt,
                "--model", model,
                "--system", system,
                "--env"  // Use GEMINI_API_KEY from environment
            };
            builder = new ProcessBuilder(command);
        } else {
            // Use Java AI client (default)
            command = new String[]{
                "java", "AIChatClient",
                "--ask", prompt,
                "--model", model,
                "--system", system,
                "--env"  // Use GEMINI_API_KEY from environment
            };
            builder = new ProcessBuilder(command);
        }
        
        Process process = builder.start();
        
        // Wait for completion with timeout
        boolean finished = process.waitFor(30, java.util.concurrent.TimeUnit.SECONDS);
        if (!finished) {
            process.destroyForcibly();
            throw new RuntimeException("AI client timeout");
        }
        
        int exitCode = process.exitValue();
        if (exitCode != 0) {
            // Read error output
            try (BufferedReader errorReader = new BufferedReader(
                    new InputStreamReader(process.getErrorStream()))) {
                String errorOutput = errorReader.lines().collect(Collectors.joining("\n"));
                throw new RuntimeException("AI client failed with exit code " + exitCode + ": " + errorOutput);
            }
        }
        
        // Read successful output
        try (BufferedReader outputReader = new BufferedReader(
                new InputStreamReader(process.getInputStream()))) {
            return outputReader.lines().collect(Collectors.joining("\n"));
        }
    }

    /**
     * Call the secure compilation service
     */
    private static String callCompileService(String language, String filename, String source) throws IOException, InterruptedException {
        // Create JSON payload for the compilation service
        ObjectNode payload = objectMapper.createObjectNode();
        payload.put("language", language);
        payload.put("filename", filename);
        payload.put("source", source);
        payload.put("timeout", 10);
        
        String jsonPayload = objectMapper.writeValueAsString(payload);
        
        ProcessBuilder builder = new ProcessBuilder(
            "curl", "-X", "POST",
            "http://localhost:4040/compile",
            "-H", "Content-Type: application/json",
            "-d", jsonPayload
        );
        
        Process process = builder.start();
        
        boolean finished = process.waitFor(15, java.util.concurrent.TimeUnit.SECONDS);
        if (!finished) {
            process.destroyForcibly();
            throw new RuntimeException("Compilation service timeout");
        }
        
        int exitCode = process.exitValue();
        if (exitCode != 0) {
            try (BufferedReader errorReader = new BufferedReader(
                    new InputStreamReader(process.getErrorStream()))) {
                String errorOutput = errorReader.lines().collect(Collectors.joining("\n"));
                throw new RuntimeException("Compilation failed: " + errorOutput);
            }
        }
        
        try (BufferedReader outputReader = new BufferedReader(
                new InputStreamReader(process.getInputStream()))) {
            return outputReader.lines().collect(Collectors.joining("\n"));
        }
    }
}
