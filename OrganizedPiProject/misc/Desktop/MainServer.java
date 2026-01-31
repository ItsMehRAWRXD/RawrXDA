// ai-cli-backend/src/main/java/com/aicli/server/MainServer.java
package com.aicli.server;

import com.aicli.AiLspServer;
import com.aicli.AgenticOrchestrator;
import com.aicli.plugins.PluginManagerService;
import com.aicli.plugins.PermissionManager;
import com.aicli.plugins.GraalVmSecureExecutor;
import com.aicli.plugins.Capability;
import com.aicli.monitoring.TracerService;
import com.aicli.AgentHandler;
import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import org.eclipse.lsp4j.jsonrpc.Launcher;
import org.eclipse.lsp4j.services.LanguageServer;

import java.io.*;
import java.net.InetSocketAddress;
import java.nio.file.*;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.Executors;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;

/**
 * Main server that combines HTTP API server and LSP server functionality.
 * Provides a unified entry point for the AI development environment with
 * comprehensive security, monitoring, and plugin management.
 * 
 * Features:
 * - HTTP API for web-based interactions
 * - LSP server for IDE integration
 * - Plugin management with security
 * - Distributed tracing and monitoring
 * - Capability-based access control
 */
public class MainServer {
    private static final Logger logger = Logger.getLogger(MainServer.class.getName());
    
    // Server configuration
    private static final int HTTP_PORT = 8080;
    private static final String PLUGINS_DIR = "plugins";
    private static final String CONFIG_FILE = "config.json";
    
    // Core components
    private HttpServer httpServer;
    private AiLspServer lspServer;
    private AgenticOrchestrator orchestrator;
    private PluginManagerService pluginManager;
    private PermissionManager permissionManager;
    private GraalVmSecureExecutor secureExecutor;
    private TracerService tracerService;
    
    // Server state
    private boolean isRunning = false;
    private Future<?> lspServerFuture;
    
    public static void main(String[] args) {
        try {
            MainServer server = new MainServer();
            server.start();
            
            // Add shutdown hook for graceful cleanup
            Runtime.getRuntime().addShutdownHook(new Thread(server::stop));
            
            // Keep main thread alive
            Thread.currentThread().join();
            
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to start MainServer", e);
            System.exit(1);
        }
    }
    
    /**
     * Start all server components
     */
    public void start() throws Exception {
        logger.info("Starting AI CLI MainServer...");
        
        // Initialize core components
        initializeComponents();
        
        // Start HTTP server
        startHttpServer();
        
        // Start LSP server
        startLspServer();
        
        isRunning = true;
        logger.info("MainServer started successfully");
        logger.info("HTTP server running on port " + HTTP_PORT);
        logger.info("LSP server ready for IDE connections");
    }
    
    /**
     * Initialize all core components
     */
    private void initializeComponents() throws Exception {
        // Initialize tracing service
        tracerService = new TracerService(
            Boolean.parseBoolean(System.getenv().getOrDefault("ENABLE_JAEGER", "false")),
            System.getenv().getOrDefault("JAEGER_ENDPOINT", "http://localhost:14250"),
            "ai-cli-main-server"
        );
        
        // Initialize permission manager
        permissionManager = new PermissionManager();
        setupDefaultPermissions();
        
        // Initialize plugin manager
        Path pluginsPath = Paths.get(PLUGINS_DIR);
        Files.createDirectories(pluginsPath);
        pluginManager = new PluginManagerService(pluginsPath);
        
        // Initialize secure executor
        secureExecutor = new GraalVmSecureExecutor(permissionManager);
        
        // Initialize orchestrator
        String apiKey = System.getenv("OPENAI_API_KEY");
        if (apiKey == null || apiKey.isEmpty()) {
            throw new IllegalStateException("OPENAI_API_KEY environment variable is required");
        }
        orchestrator = new AgenticOrchestrator(apiKey, pluginsPath);
        
        // Initialize LSP server
        lspServer = new AiLspServer();
        
        logger.info("All components initialized successfully");
    }
    
    /**
     * Setup default permissions for different plugin types
     */
    private void setupDefaultPermissions() {
        // Basic IDE access for all plugins
        permissionManager.setDefaultPermissions("basic", Set.of(
            Capability.IDE_API_ACCESS
        ));
        
        // File operations for file-related plugins
        permissionManager.setDefaultPermissions("file", Set.of(
            Capability.IDE_API_ACCESS,
            Capability.FILE_READ_ACCESS,
            Capability.FILE_WRITE_ACCESS
        ));
        
        // Network operations for web-related plugins
        permissionManager.setDefaultPermissions("network", Set.of(
            Capability.IDE_API_ACCESS,
            Capability.NETWORK_ACCESS
        ));
        
        // System operations for trusted plugins only
        permissionManager.setDefaultPermissions("system", Set.of(
            Capability.IDE_API_ACCESS,
            Capability.EXECUTE_SYSTEM_COMMAND,
            Capability.FILE_READ_ACCESS,
            Capability.FILE_WRITE_ACCESS
        ));
        
        logger.info("Default permissions configured");
    }
    
    /**
     * Start HTTP server with all endpoints
     */
    private void startHttpServer() throws IOException {
        httpServer = HttpServer.create(new InetSocketAddress(HTTP_PORT), 0);
        
        // Create context handlers
        httpServer.createContext("/agent-stream", new AgentHandler(orchestrator));
        httpServer.createContext("/api/plugins", new PluginApiHandler());
        httpServer.createContext("/api/permissions", new PermissionApiHandler());
        httpServer.createContext("/api/health", new HealthHandler());
        httpServer.createContext("/api/metrics", new MetricsHandler());
        
        // Set executor for handling requests
        httpServer.setExecutor(Executors.newCachedThreadPool());
        httpServer.start();
        
        logger.info("HTTP server started on port " + HTTP_PORT);
    }
    
    /**
     * Start LSP server for IDE integration
     */
    private void startLspServer() throws ExecutionException, InterruptedException {
        InputStream in = System.in;
        OutputStream out = System.out;
        
        Launcher<LanguageServer> lspLauncher = new Launcher.Builder<LanguageServer>()
                .setRemoteInterface(LanguageServer.class)
                .setLocalService(lspServer)
                .setInput(in)
                .setOutput(out)
                .create();
        
        lspServer.connect(lspLauncher.getRemoteProxy());
        lspServerFuture = lspLauncher.startListening();
        
        logger.info("LSP server started and listening");
    }
    
    /**
     * Stop all server components gracefully
     */
    public void stop() {
        logger.info("Stopping MainServer...");
        
        isRunning = false;
        
        try {
            // Stop HTTP server
            if (httpServer != null) {
                httpServer.stop(5);
                logger.info("HTTP server stopped");
            }
            
            // Stop LSP server
            if (lspServerFuture != null) {
                lspServerFuture.cancel(true);
                logger.info("LSP server stopped");
            }
            
            // Cleanup components
            if (pluginManager != null) {
                pluginManager.stopAndUnload();
                logger.info("Plugin manager stopped");
            }
            
            if (secureExecutor != null) {
                secureExecutor.shutdown();
                logger.info("Secure executor stopped");
            }
            
            if (tracerService != null) {
                tracerService.cleanup();
                logger.info("Tracer service stopped");
            }
            
            logger.info("MainServer stopped successfully");
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Error during shutdown", e);
        }
    }
    
    /**
     * Check if server is running
     */
    public boolean isRunning() {
        return isRunning;
    }
    
    // HTTP Handler implementations
    
    /**
     * Plugin API handler for managing plugins
     */
    private class PluginApiHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String method = exchange.getRequestMethod();
            String path = exchange.getRequestURI().getPath();
            
            try {
                switch (method) {
                    case "GET":
                        handleGetPlugins(exchange);
                        break;
                    case "POST":
                        handleInstallPlugin(exchange);
                        break;
                    case "DELETE":
                        handleUninstallPlugin(exchange);
                        break;
                    default:
                        sendErrorResponse(exchange, 405, "Method not allowed");
                }
            } catch (Exception e) {
                logger.log(Level.WARNING, "Error handling plugin API request", e);
                sendErrorResponse(exchange, 500, "Internal server error");
            }
        }
        
        private void handleGetPlugins(HttpExchange exchange) throws IOException {
            // Implementation for listing plugins
            String response = "{\"plugins\":[]}";
            sendJsonResponse(exchange, response);
        }
        
        private void handleInstallPlugin(HttpExchange exchange) throws IOException {
            // Implementation for installing plugins
            sendJsonResponse(exchange, "{\"status\":\"success\"}");
        }
        
        private void handleUninstallPlugin(HttpExchange exchange) throws IOException {
            // Implementation for uninstalling plugins
            sendJsonResponse(exchange, "{\"status\":\"success\"}");
        }
    }
    
    /**
     * Permission API handler for managing plugin permissions
     */
    private class PermissionApiHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String method = exchange.getRequestMethod();
            
            try {
                switch (method) {
                    case "GET":
                        handleGetPermissions(exchange);
                        break;
                    case "POST":
                        handleGrantPermission(exchange);
                        break;
                    case "DELETE":
                        handleRevokePermission(exchange);
                        break;
                    default:
                        sendErrorResponse(exchange, 405, "Method not allowed");
                }
            } catch (Exception e) {
                logger.log(Level.WARNING, "Error handling permission API request", e);
                sendErrorResponse(exchange, 500, "Internal server error");
            }
        }
        
        private void handleGetPermissions(HttpExchange exchange) throws IOException {
            // Implementation for getting plugin permissions
            String response = "{\"permissions\":{}}";
            sendJsonResponse(exchange, response);
        }
        
        private void handleGrantPermission(HttpExchange exchange) throws IOException {
            // Implementation for granting permissions
            sendJsonResponse(exchange, "{\"status\":\"success\"}");
        }
        
        private void handleRevokePermission(HttpExchange exchange) throws IOException {
            // Implementation for revoking permissions
            sendJsonResponse(exchange, "{\"status\":\"success\"}");
        }
    }
    
    /**
     * Health check handler
     */
    private class HealthHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            Map<String, Object> health = new HashMap<>();
            health.put("status", "healthy");
            health.put("timestamp", System.currentTimeMillis());
            health.put("uptime", System.currentTimeMillis() - System.currentTimeMillis()); // Placeholder
            
            String response = "{\"status\":\"healthy\",\"timestamp\":" + System.currentTimeMillis() + "}";
            sendJsonResponse(exchange, response);
        }
    }
    
    /**
     * Metrics handler for monitoring
     */
    private class MetricsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            Map<String, Object> metrics = new HashMap<>();
            metrics.put("activeSpans", tracerService.getActiveSpanCount());
            metrics.put("activeScopes", tracerService.getActiveScopeCount());
            
            String response = "{\"metrics\":" + metrics.toString() + "}";
            sendJsonResponse(exchange, response);
        }
    }
    
    // Utility methods for HTTP responses
    
    private void sendJsonResponse(HttpExchange exchange, String json) throws IOException {
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(200, json.getBytes().length);
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(json.getBytes());
        }
    }
    
    private void sendErrorResponse(HttpExchange exchange, int code, String message) throws IOException {
        String response = "{\"error\":\"" + message + "\"}";
        exchange.getResponseHeaders().set("Content-Type", "application/json");
        exchange.sendResponseHeaders(code, response.getBytes().length);
        
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(response.getBytes());
        }
    }
}
