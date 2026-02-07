// ai-cli-backend/src/main/java/com/aicli/plugins/SecureApiServer.java
package com.aicli.plugins;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.Files;
import java.io.IOException;
import java.util.logging.Logger;
import java.util.logging.Level;

public class SecureApiServer {
    private static final Logger logger = Logger.getLogger(SecureApiServer.class.getName());
    private final ExecutorService executor = Executors.newFixedThreadPool(10);
    private final Map<String, Integer> clientConnections = new ConcurrentHashMap<>();
    private final Set<String> allowedCommands = new HashSet<>();
    private final Path allowedDirectory;
    private volatile boolean running = false;
    private ServerSocket serverSocket;
    
    public SecureApiServer(Path allowedDirectory) {
        this.allowedDirectory = allowedDirectory;
        initializeAllowedCommands();
    }
    
    private void initializeAllowedCommands() {
        allowedCommands.add("getFileContent");
        allowedCommands.add("listFiles");
        allowedCommands.add("getFileInfo");
        allowedCommands.add("ping");
        allowedCommands.add("getSystemInfo");
    }
    
    public void start(int port) throws IOException {
        serverSocket = new ServerSocket(port);
        running = true;
        logger.info("Secure API Server listening on port " + port);
        
        while (running && !serverSocket.isClosed()) {
            try {
                Socket clientSocket = serverSocket.accept();
                String clientId = clientSocket.getInetAddress().getHostAddress() + ":" + clientSocket.getPort();
                clientConnections.put(clientId, clientConnections.getOrDefault(clientId, 0) + 1);
                
                executor.submit(() -> handleClient(clientSocket, clientId));
            } catch (IOException e) {
                if (running) {
                    logger.log(Level.WARNING, "Error accepting client connection", e);
                }
            }
        }
    }
    
    public void stop() {
        running = false;
        try {
            if (serverSocket != null && !serverSocket.isClosed()) {
                serverSocket.close();
            }
        } catch (IOException e) {
            logger.log(Level.WARNING, "Error closing server socket", e);
        }
        executor.shutdown();
    }
    
    private void handleClient(Socket clientSocket, String clientId) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));
             PrintWriter writer = new PrintWriter(clientSocket.getOutputStream(), true)) {
            
            // Set timeout for client operations
            clientSocket.setSoTimeout(30000); // 30 seconds
            
            String request = reader.readLine();
            if (request == null) {
                writer.println("ERROR: Empty request");
                return;
            }
            
            // Parse command and arguments
            String[] parts = request.split(" ", 2);
            String command = parts[0];
            String args = parts.length > 1 ? parts[1] : "";
            
            // Validate command
            if (!allowedCommands.contains(command)) {
                writer.println("ERROR: Invalid command: " + command);
                return;
            }
            
            // Execute command with security checks
            String result = executeSecureCommand(command, args, clientId);
            writer.println(result);
            
        } catch (IOException e) {
            logger.log(Level.WARNING, "Error handling client " + clientId, e);
        } finally {
            clientConnections.remove(clientId);
        }
    }
    
    private String executeSecureCommand(String command, String args, String clientId) {
        try {
            switch (command) {
                case "ping":
                    return "PONG";
                    
                case "getFileContent":
                    return getFileContentSecurely(args);
                    
                case "listFiles":
                    return listFilesSecurely(args);
                    
                case "getFileInfo":
                    return getFileInfoSecurely(args);
                    
                case "getSystemInfo":
                    return getSystemInfo();
                    
                default:
                    return "ERROR: Unknown command: " + command;
            }
        } catch (Exception e) {
            logger.log(Level.WARNING, "Error executing command " + command + " for client " + clientId, e);
            return "ERROR: " + e.getMessage();
        }
    }
    
    private String getFileContentSecurely(String filePath) {
        try {
            Path path = Paths.get(filePath).normalize();
            
            // Security check: ensure path is within allowed directory
            if (!path.startsWith(allowedDirectory)) {
                return "ERROR: Access denied - path outside allowed directory";
            }
            
            // Security check: file size limit (1MB)
            if (Files.size(path) > 1024 * 1024) {
                return "ERROR: File too large (max 1MB)";
            }
            
            // Security check: only allow text files
            String contentType = Files.probeContentType(path);
            if (contentType != null && !contentType.startsWith("text/")) {
                return "ERROR: Only text files are allowed";
            }
            
            return "CONTENT:" + new String(Files.readAllBytes(path));
            
        } catch (IOException e) {
            return "ERROR: Cannot read file: " + e.getMessage();
        }
    }
    
    private String listFilesSecurely(String directoryPath) {
        try {
            Path path = Paths.get(directoryPath).normalize();
            
            // Security check: ensure path is within allowed directory
            if (!path.startsWith(allowedDirectory)) {
                return "ERROR: Access denied - path outside allowed directory";
            }
            
            if (!Files.isDirectory(path)) {
                return "ERROR: Not a directory";
            }
            
            StringBuilder result = new StringBuilder("FILES:");
            Files.list(path)
                .limit(100) // Limit to 100 files
                .forEach(p -> {
                    try {
                        result.append(p.getFileName().toString())
                              .append(":")
                              .append(Files.isDirectory(p) ? "DIR" : "FILE")
                              .append(":");
                              .append(Files.size(p))
                              .append("\n");
                    } catch (IOException e) {
                        // Skip files that can't be accessed
                    }
                });
            
            return result.toString();
            
        } catch (IOException e) {
            return "ERROR: Cannot list directory: " + e.getMessage();
        }
    }
    
    private String getFileInfoSecurely(String filePath) {
        try {
            Path path = Paths.get(filePath).normalize();
            
            // Security check: ensure path is within allowed directory
            if (!path.startsWith(allowedDirectory)) {
                return "ERROR: Access denied - path outside allowed directory";
            }
            
            if (!Files.exists(path)) {
                return "ERROR: File does not exist";
            }
            
            return String.format("INFO:%s:%s:%d:%s", 
                path.getFileName().toString(),
                Files.isDirectory(path) ? "DIR" : "FILE",
                Files.size(path),
                Files.getLastModifiedTime(path).toString());
            
        } catch (IOException e) {
            return "ERROR: Cannot get file info: " + e.getMessage();
        }
    }
    
    private String getSystemInfo() {
        Runtime runtime = Runtime.getRuntime();
        return String.format("SYSTEM:Java:%s:Memory:%d:%d:Processors:%d", 
            System.getProperty("java.version"),
            runtime.totalMemory(),
            runtime.freeMemory(),
            runtime.availableProcessors());
    }
    
    public Map<String, Integer> getClientConnections() {
        return new ConcurrentHashMap<>(clientConnections);
    }
    
    public boolean isRunning() {
        return running;
    }
}
