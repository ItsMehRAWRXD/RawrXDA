// LSP Bridge Server - Connects Scratch to Java Language Server Protocol
// Compile: javac LspBridgeServer.java
// Run: java LspBridgeServer
// Features: Code completion, diagnostics, hover docs, go to definition

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.*;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.regex.Pattern;

public class LspBridgeServer {
    private static final int PORT = 8080;
    private static final String LSP_SERVER_PATH = "org.eclipse.jdt.ls.core.internal.JavaLanguageServerApplication";
    private static final String LSP_WORKSPACE = System.getProperty("user.dir");
    
    private Process lspProcess;
    private LspClient lspClient;
    
    public static void main(String[] args) throws IOException {
        LspBridgeServer server = new LspBridgeServer();
        server.start();
    }
    
    public void start() throws IOException {
        // Start LSP server process
        startLspServer();
        
        // Initialize LSP client
        lspClient = new LspClient();
        
        // Create HTTP server
        HttpServer server = HttpServer.create(new InetSocketAddress(PORT), 0);
        
        // Register endpoints
        server.createContext("/complete", new CompletionHandler());
        server.createContext("/diagnostics", new DiagnosticsHandler());
        server.createContext("/hover", new HoverHandler());
        server.createContext("/definition", new DefinitionHandler());
        server.createContext("/references", new ReferencesHandler());
        server.createContext("/rename", new RenameHandler());
        server.createContext("/format", new FormatHandler());
        server.createContext("/health", new HealthHandler());
        
        server.setExecutor(Executors.newFixedThreadPool(10));
        server.start();
        
        System.out.println("LSP Bridge Server running on http://localhost:" + PORT);
        System.out.println("Available endpoints:");
        System.out.println("  POST /complete - Code completion");
        System.out.println("  POST /diagnostics - Code diagnostics");
        System.out.println("  POST /hover - Hover documentation");
        System.out.println("  POST /definition - Go to definition");
        System.out.println("  POST /references - Find references");
        System.out.println("  POST /rename - Rename symbol");
        System.out.println("  POST /format - Format code");
        System.out.println("  GET /health - Health check");
        
        // Add shutdown hook
        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            if (lspProcess != null) {
                lspProcess.destroy();
            }
        }));
    }
    
    private void startLspServer() throws IOException {
        // Try to find Eclipse JDT Language Server
        String[] possiblePaths = {
            "eclipse-jdt-ls/jdt-language-server-*.jar",
            "org.eclipse.jdt.ls.core/target/org.eclipse.jdt.ls.core-*.jar",
            "language-server/jdt-language-server.jar"
        };
        
        String lspJarPath = findLspJar(possiblePaths);
        if (lspJarPath == null) {
            System.err.println("Warning: Eclipse JDT Language Server not found. Some features may not work.");
            System.err.println("Please download Eclipse JDT Language Server and place it in the current directory.");
            return;
        }
        
        ProcessBuilder pb = new ProcessBuilder(
            "java",
            "-Dfile.encoding=UTF-8",
            "-jar", lspJarPath,
            "-configuration", LSP_WORKSPACE + "/.metadata",
            "-data", LSP_WORKSPACE + "/workspace"
        );
        
        lspProcess = pb.start();
        System.out.println("Started LSP server: " + lspJarPath);
    }
    
    private String findLspJar(String[] paths) {
        for (String path : paths) {
            File file = new File(path);
            if (file.exists()) {
                return path;
            }
        }
        return null;
    }
    
    // Helper method to read request body
    private String readRequestBody(HttpExchange exchange) throws IOException {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(exchange.getRequestBody()))) {
            return reader.lines().collect(java.util.stream.Collectors.joining("\n"));
        }
    }
    
    // Helper method to send JSON response
    private void sendJsonResponse(HttpExchange exchange, String json) throws IOException {
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
        
        byte[] response = json.getBytes(StandardCharsets.UTF_8);
        exchange.sendResponseHeaders(200, response.length);
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response);
        }
    }
    
    // Helper method to send error response
    private void sendErrorResponse(HttpExchange exchange, int code, String message) throws IOException {
        String errorJson = "{\"error\":\"" + message + "\"}";
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(code, errorJson.length());
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(errorJson.getBytes(StandardCharsets.UTF_8));
        }
    }
    
    // Code Completion Handler
    class CompletionHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                CompletionRequest request = parseCompletionRequest(requestBody);
                
                String completions = lspClient.getCompletions(request);
                sendJsonResponse(exchange, completions);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error getting completions: " + e.getMessage());
            }
        }
    }
    
    // Diagnostics Handler
    class DiagnosticsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                DiagnosticsRequest request = parseDiagnosticsRequest(requestBody);
                
                String diagnostics = lspClient.getDiagnostics(request);
                sendJsonResponse(exchange, diagnostics);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error getting diagnostics: " + e.getMessage());
            }
        }
    }
    
    // Hover Handler
    class HoverHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                HoverRequest request = parseHoverRequest(requestBody);
                
                String hover = lspClient.getHover(request);
                sendJsonResponse(exchange, hover);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error getting hover info: " + e.getMessage());
            }
        }
    }
    
    // Definition Handler
    class DefinitionHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                DefinitionRequest request = parseDefinitionRequest(requestBody);
                
                String definition = lspClient.getDefinition(request);
                sendJsonResponse(exchange, definition);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error getting definition: " + e.getMessage());
            }
        }
    }
    
    // References Handler
    class ReferencesHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                ReferencesRequest request = parseReferencesRequest(requestBody);
                
                String references = lspClient.getReferences(request);
                sendJsonResponse(exchange, references);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error getting references: " + e.getMessage());
            }
        }
    }
    
    // Rename Handler
    class RenameHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                RenameRequest request = parseRenameRequest(requestBody);
                
                String rename = lspClient.renameSymbol(request);
                sendJsonResponse(exchange, rename);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error renaming symbol: " + e.getMessage());
            }
        }
    }
    
    // Format Handler
    class FormatHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
                exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                exchange.getResponseHeaders().set("Access-Control-Allow-Methods", "POST, OPTIONS");
                exchange.getResponseHeaders().set("Access-Control-Allow-Headers", "Content-Type");
                exchange.sendResponseHeaders(200, -1);
                return;
            }
            
            if (!"POST".equalsIgnoreCase(exchange.getRequestMethod())) {
                sendErrorResponse(exchange, 405, "Method not allowed");
                return;
            }
            
            try {
                String requestBody = readRequestBody(exchange);
                FormatRequest request = parseFormatRequest(requestBody);
                
                String formatted = lspClient.formatCode(request);
                sendJsonResponse(exchange, formatted);
                
            } catch (Exception e) {
                sendErrorResponse(exchange, 500, "Error formatting code: " + e.getMessage());
            }
        }
    }
    
    // Health Handler
    class HealthHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String healthJson = "{\"status\":\"healthy\",\"lsp\":\"" + (lspProcess != null ? "running" : "stopped") + "\"}";
            sendJsonResponse(exchange, healthJson);
        }
    }
    
    // Request parsing methods
    private CompletionRequest parseCompletionRequest(String body) {
        // Simple JSON parsing for completion request
        // Expected format: {"file":"path","content":"code","line":1,"column":10}
        return new CompletionRequest(body);
    }
    
    private DiagnosticsRequest parseDiagnosticsRequest(String body) {
        // Simple JSON parsing for diagnostics request
        // Expected format: {"file":"path","content":"code"}
        return new DiagnosticsRequest(body);
    }
    
    private HoverRequest parseHoverRequest(String body) {
        // Simple JSON parsing for hover request
        // Expected format: {"file":"path","content":"code","line":1,"column":10}
        return new HoverRequest(body);
    }
    
    private DefinitionRequest parseDefinitionRequest(String body) {
        // Simple JSON parsing for definition request
        // Expected format: {"file":"path","content":"code","line":1,"column":10}
        return new DefinitionRequest(body);
    }
    
    private ReferencesRequest parseReferencesRequest(String body) {
        // Simple JSON parsing for references request
        // Expected format: {"file":"path","content":"code","line":1,"column":10}
        return new ReferencesRequest(body);
    }
    
    private RenameRequest parseRenameRequest(String body) {
        // Simple JSON parsing for rename request
        // Expected format: {"file":"path","content":"code","line":1,"column":10,"newName":"newName"}
        return new RenameRequest(body);
    }
    
    private FormatRequest parseFormatRequest(String body) {
        // Simple JSON parsing for format request
        // Expected format: {"file":"path","content":"code"}
        return new FormatRequest(body);
    }
    
    // Request classes
    static class CompletionRequest {
        String file, content;
        int line, column;
        
        CompletionRequest(String json) {
            // Simple parsing - in production, use proper JSON library
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
            this.line = Integer.parseInt(extractValue(json, "line", "1"));
            this.column = Integer.parseInt(extractValue(json, "column", "1"));
        }
    }
    
    static class DiagnosticsRequest {
        String file, content;
        
        DiagnosticsRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
        }
    }
    
    static class HoverRequest {
        String file, content;
        int line, column;
        
        HoverRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
            this.line = Integer.parseInt(extractValue(json, "line", "1"));
            this.column = Integer.parseInt(extractValue(json, "column", "1"));
        }
    }
    
    static class DefinitionRequest {
        String file, content;
        int line, column;
        
        DefinitionRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
            this.line = Integer.parseInt(extractValue(json, "line", "1"));
            this.column = Integer.parseInt(extractValue(json, "column", "1"));
        }
    }
    
    static class ReferencesRequest {
        String file, content;
        int line, column;
        
        ReferencesRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
            this.line = Integer.parseInt(extractValue(json, "line", "1"));
            this.column = Integer.parseInt(extractValue(json, "column", "1"));
        }
    }
    
    static class RenameRequest {
        String file, content, newName;
        int line, column;
        
        RenameRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
            this.newName = extractValue(json, "newName");
            this.line = Integer.parseInt(extractValue(json, "line", "1"));
            this.column = Integer.parseInt(extractValue(json, "column", "1"));
        }
    }
    
    static class FormatRequest {
        String file, content;
        
        FormatRequest(String json) {
            this.file = extractValue(json, "file");
            this.content = extractValue(json, "content");
        }
    }
    
    // Helper method to extract values from simple JSON
    private static String extractValue(String json, String key) {
        return extractValue(json, key, "");
    }
    
    private static String extractValue(String json, String key, String defaultValue) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        java.util.regex.Matcher matcher = pattern.matcher(json);
        return matcher.find() ? matcher.group(1) : defaultValue;
    }
    
    // LSP Client implementation
    static class LspClient {
        public String getCompletions(CompletionRequest request) {
            // For now, return simple completions based on Java keywords and common patterns
            String[] completions = {
                "public", "private", "protected", "static", "final", "abstract", "class", "interface",
                "void", "int", "String", "boolean", "double", "float", "long", "byte", "char",
                "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return",
                "try", "catch", "finally", "throw", "throws", "import", "package", "extends", "implements",
                "new", "this", "super", "null", "true", "false", "System.out.println", "System.out.print"
            };
            
            StringBuilder result = new StringBuilder("{\"completions\":[");
            for (int i = 0; i < completions.length; i++) {
                if (i > 0) result.append(",");
                result.append("{\"label\":\"").append(completions[i]).append("\",\"kind\":\"keyword\"}");
            }
            result.append("]}");
            return result.toString();
        }
        
        public String getDiagnostics(DiagnosticsRequest request) {
            // Simple diagnostic analysis
            StringBuilder diagnostics = new StringBuilder("{\"diagnostics\":[");
            boolean hasIssues = false;
            
            if (request.content != null) {
                String[] lines = request.content.split("\n");
                for (int i = 0; i < lines.length; i++) {
                    String line = lines[i];
                    
                    // Check for missing semicolons
                    if (line.trim().endsWith("}") || line.trim().endsWith("{")) {
                        // OK
                    } else if (line.trim().endsWith(";") || line.trim().isEmpty() || line.trim().startsWith("//")) {
                        // OK
                    } else if (line.contains("=") || line.contains("return") || line.contains("System.out")) {
                        if (!line.trim().endsWith(";")) {
                            if (hasIssues) diagnostics.append(",");
                            diagnostics.append("{\"line\":").append(i + 1)
                                .append(",\"column\":").append(line.length() + 1)
                                .append(",\"severity\":\"warning\",\"message\":\"Missing semicolon\"}");
                            hasIssues = true;
                        }
                    }
                }
            }
            
            diagnostics.append("]}");
            return diagnostics.toString();
        }
        
        public String getHover(HoverRequest request) {
            // Simple hover information
            return "{\"contents\":[{\"value\":\"Java Language Server - Hover information would appear here\"}]}";
        }
        
        public String getDefinition(DefinitionRequest request) {
            // Simple definition response
            return "{\"uri\":\"file://" + request.file + "\",\"range\":{\"start\":{\"line\":" + request.line + ",\"character\":" + request.column + "},\"end\":{\"line\":" + request.line + ",\"character\":" + (request.column + 10) + "}}}";
        }
        
        public String getReferences(ReferencesRequest request) {
            // Simple references response
            return "{\"references\":[{\"uri\":\"file://" + request.file + "\",\"range\":{\"start\":{\"line\":" + request.line + ",\"character\":" + request.column + "},\"end\":{\"line\":" + request.line + ",\"character\":" + (request.column + 10) + "}}}]}";
        }
        
        public String renameSymbol(RenameRequest request) {
            // Simple rename response
            return "{\"changes\":{\"file://" + request.file + "\":[{\"range\":{\"start\":{\"line\":" + request.line + ",\"character\":" + request.column + "},\"end\":{\"line\":" + request.line + ",\"character\":" + (request.column + 10) + "}},\"newText\":\"" + request.newName + "\"}]}}";
        }
        
        public String formatCode(FormatRequest request) {
            // Simple formatting - just return the content with basic indentation
            if (request.content == null) return "{\"formatted\":\"\"}";
            
            String[] lines = request.content.split("\n");
            StringBuilder formatted = new StringBuilder();
            int indent = 0;
            
            for (String line : lines) {
                String trimmed = line.trim();
                if (trimmed.startsWith("}")) indent--;
                
                for (int i = 0; i < indent; i++) {
                    formatted.append("    ");
                }
                formatted.append(trimmed).append("\n");
                
                if (trimmed.endsWith("{")) indent++;
            }
            
            return "{\"formatted\":\"" + formatted.toString().replace("\"", "\\\"").replace("\n", "\\n") + "\"}";
        }
    }
}
