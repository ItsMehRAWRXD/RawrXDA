// ai-cli-backend/src/main/java/com/aicli/plugins/GraalVmSecureExecutor.java
package com.aicli.plugins;

import org.graalvm.polyglot.Context;
import org.graalvm.polyglot.HostAccess;
import org.graalvm.polyglot.SandboxPolicy;
import org.graalvm.polyglot.Value;
import org.graalvm.polyglot.Source;
import org.graalvm.polyglot.Engine;
import java.io.*;
import java.nio.file.*;
import java.time.Duration;
import java.time.Instant;
import java.util.concurrent.*;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Secure GraalVM executor with capability enforcement.
 */
public class GraalVmSecureExecutor {
    private static final Logger logger = Logger.getLogger(GraalVmSecureExecutor.class.getName());
    
    private final PermissionManager permissionManager;
    private final ExecutorService executorService;
    private final Map<String, Engine> enginePool = new ConcurrentHashMap<>();
    
    public GraalVmSecureExecutor(PermissionManager permissionManager) {
        this.permissionManager = permissionManager;
        this.executorService = Executors.newCachedThreadPool();
        initializeEnginePool();
    }
    
    /**
     * Simple execute method for compatibility with orchestrator
     */
    public String execute(String pluginId, String code, String className) {
        ExecutionResult result = execute(pluginId, code, className, null);
        if (result.isSuccess()) {
            return result.getResult();
        } else {
            return "GraalVM execution failed: " + result.getError();
        }
    }
    
    /**
     * Execute plugin code with comprehensive security enforcement and capability-based restrictions
     */
    public ExecutionResult execute(String pluginId, String code, String className, Map<String, Object> context) {
        // Pre-execution security checks - enforce capabilities before creating the context
        if (!permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND)) {
            return ExecutionResult.failure("Execution not permitted for plugin: " + pluginId);
        }
        
        // Capability-based security validation
        SecurityValidationResult validation = validateCapabilities(pluginId, code, context);
        if (!validation.isValid()) {
            return ExecutionResult.failure("Capability validation failed: " + validation.getReason());
        }
        
        // Get isolated engine for this plugin
        Engine engine = getOrCreateEngine(pluginId);
        
        try (Context polyglotContext = createSecureContext(engine, pluginId)) {
            // Enhanced code validation with capability awareness
            CodeValidationResult codeValidation = validateCodeWithCapabilities(code, pluginId);
            if (!codeValidation.isValid()) {
                return ExecutionResult.failure("Code validation failed: " + codeValidation.getReason());
            }
            
            // Create source with security metadata
            Source source = Source.newBuilder("js", code, "plugin_" + pluginId)
                    .mimeType("application/javascript")
                    .build();
            
            // Execute with capability-aware timeout
            int timeoutSeconds = getCapabilityBasedTimeout(pluginId);
            CompletableFuture<Value> executionFuture = CompletableFuture.supplyAsync(() -> {
                try {
                    return polyglotContext.eval(source);
                } catch (Exception e) {
                    throw new RuntimeException("Execution failed: " + e.getMessage(), e);
                }
            }, executorService);
            
            Value result = executionFuture.get(timeoutSeconds, TimeUnit.SECONDS);
            
            // Post-execution security validation
            if (!validateExecutionResult(result, pluginId)) {
                return ExecutionResult.failure("Execution result validation failed");
            }
            
            return ExecutionResult.success(result.toString());
            
        } catch (TimeoutException e) {
            return ExecutionResult.failure("Execution timeout after " + timeoutSeconds + " seconds");
        } catch (Exception e) {
            logger.log(Level.WARNING, "Execution error for plugin " + pluginId, e);
            return ExecutionResult.failure("Execution error: " + e.getMessage());
        }
    }
    
    /**
     * Execute with monitoring for use by tool wrappers
     */
    public ExecutionResult executeWithMonitoring(String code, String className) {
        String pluginId = "monitor_" + className;
        return execute(pluginId, code, className, null);
    }
    
    /**
     * Create secure GraalVM context with comprehensive capability-based restrictions
     */
    private Context createSecureContext(Engine engine, String pluginId) {
        Context.Builder contextBuilder = Context.newBuilder()
                .engine(engine)
                .sandbox(SandboxPolicy.UNTRUSTED)
                .allowHostAccess(HostAccess.NONE)
                .allowNativeAccess(false)
                .allowHostClassLoading(false)
                .allowCreateProcess(false)
                .allowCreateThread(false)
                .allowIO(false);
        
        // Configure based on plugin capabilities with granular control
        if (permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS)) {
            contextBuilder.allowIO(true);
            // Additional network-specific restrictions
            contextBuilder.option("sandbox.AllowNetworkAccess", "true");
        }
        
        if (permissionManager.hasPermission(pluginId, Capability.FILE_READ_ACCESS)) {
            contextBuilder.allowIO(true);
            // File access restrictions
            contextBuilder.option("sandbox.AllowFileRead", "true");
        }
        
        if (permissionManager.hasPermission(pluginId, Capability.FILE_WRITE_ACCESS)) {
            contextBuilder.allowIO(true);
            contextBuilder.option("sandbox.AllowFileWrite", "true");
        }
        
        // Set capability-aware resource limits
        setCapabilityBasedResourceLimits(contextBuilder, pluginId);
        
        // Additional security options
        contextBuilder.option("sandbox.EnableSecurityWarnings", "true");
        contextBuilder.option("sandbox.EnableResourceLimits", "true");
        
        return contextBuilder.build();
    }
    
    /**
     * Set resource limits based on plugin capabilities
     */
    private void setCapabilityBasedResourceLimits(Context.Builder contextBuilder, String pluginId) {
        // Base limits for all plugins
        contextBuilder.option("sandbox.MaxCPUTime", "10s");
        contextBuilder.option("sandbox.MaxHeapMemory", "128MB");
        
        // Enhanced limits for trusted plugins
        if (permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND)) {
            contextBuilder.option("sandbox.MaxCPUTime", "30s");
            contextBuilder.option("sandbox.MaxHeapMemory", "256MB");
        }
        
        // Network access plugins get additional memory for buffers
        if (permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS)) {
            contextBuilder.option("sandbox.MaxHeapMemory", "512MB");
        }
    }
    
    /**
     * Validate code for security issues with capability awareness
     */
    private CodeValidationResult validateCodeWithCapabilities(String code, String pluginId) {
        if (code == null || code.trim().isEmpty()) {
            return CodeValidationResult.invalid("Code is null or empty");
        }
        
        // Check for dangerous patterns
        String[] dangerousPatterns = {
            "eval(", "Function(", "setTimeout", "setInterval",
            "require(", "import(", "process.", "global.",
            "document.", "window.", "XMLHttpRequest", "fetch("
        };
        
        for (String pattern : dangerousPatterns) {
            if (code.contains(pattern)) {
                // Check if plugin has permission for this pattern
                if (!hasPermissionForPattern(pluginId, pattern)) {
                    logger.warning("Dangerous pattern detected without permission: " + pattern);
                    return CodeValidationResult.invalid("Dangerous pattern detected: " + pattern);
                }
            }
        }
        
        // Check for capability-specific patterns
        if (code.contains("fs.") && !permissionManager.hasPermission(pluginId, Capability.FILE_READ_ACCESS)) {
            return CodeValidationResult.invalid("File system access without permission");
        }
        
        if (code.contains("http") && !permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS)) {
            return CodeValidationResult.invalid("Network access without permission");
        }
        
        return CodeValidationResult.valid();
    }
    
    /**
     * Check if plugin has permission for a specific pattern
     */
    private boolean hasPermissionForPattern(String pluginId, String pattern) {
        return switch (pattern) {
            case "eval(", "Function(" -> permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
            case "setTimeout", "setInterval" -> permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
            case "require(", "import(" -> permissionManager.hasPermission(pluginId, Capability.PLUGIN_MANAGEMENT);
            case "process.", "global." -> permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
            case "document.", "window." -> permissionManager.hasPermission(pluginId, Capability.IDE_API_ACCESS);
            case "XMLHttpRequest", "fetch(" -> permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS);
            default -> false;
        };
    }
    
    /**
     * Validate capabilities for execution
     */
    private SecurityValidationResult validateCapabilities(String pluginId, String code, Map<String, Object> context) {
        // Check if plugin has basic execution permission
        if (!permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND)) {
            return SecurityValidationResult.invalid("No execution permission");
        }
        
        // Check for high-risk capabilities
        if (code.contains("system") && !permissionManager.hasPermission(pluginId, Capability.EXECUTE_NATIVE_CODE)) {
            return SecurityValidationResult.invalid("Native code execution without permission");
        }
        
        // Check context permissions
        if (context != null && context.containsKey("network") && 
            !permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS)) {
            return SecurityValidationResult.invalid("Network access without permission");
        }
        
        return SecurityValidationResult.valid();
    }
    
    /**
     * Get timeout based on plugin capabilities
     */
    private int getCapabilityBasedTimeout(String pluginId) {
        if (permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND)) {
            return 60; // 60 seconds for system commands
        }
        if (permissionManager.hasPermission(pluginId, Capability.NETWORK_ACCESS)) {
            return 30; // 30 seconds for network operations
        }
        return 10; // 10 seconds for basic operations
    }
    
    /**
     * Validate execution result for security issues
     */
    private boolean validateExecutionResult(Value result, String pluginId) {
        if (result == null) {
            return true; // Null results are acceptable
        }
        
        String resultString = result.toString();
        
        // Check for suspicious output patterns
        String[] suspiciousPatterns = {
            "password", "token", "key", "secret", "private"
        };
        
        for (String pattern : suspiciousPatterns) {
            if (resultString.toLowerCase().contains(pattern)) {
                logger.warning("Suspicious output pattern detected: " + pattern);
                // Don't block, but log for security monitoring
            }
        }
        
        return true;
    }
    
    /**
     * Initialize engine pool for better isolation
     */
    private void initializeEnginePool() {
        for (int i = 0; i < 5; i++) {
            String engineId = "engine_" + i;
            Engine engine = Engine.newBuilder()
                    .option("engine.WarnInterpreterOnly", "false")
                    .build();
            enginePool.put(engineId, engine);
        }
    }
    
    /**
     * Get or create isolated engine for plugin
     */
    private Engine getOrCreateEngine(String pluginId) {
        String engineId = "plugin_" + (pluginId.hashCode() % 5);
        return enginePool.computeIfAbsent(engineId, k -> 
            Engine.newBuilder()
                .option("engine.WarnInterpreterOnly", "false")
                .build()
        );
    }
    
    /**
     * Cleanup resources
     */
    public void shutdown() {
        executorService.shutdown();
        for (Engine engine : enginePool.values()) {
            engine.close();
        }
        enginePool.clear();
    }
    
    /**
     * Execution result wrapper
     */
    public static class ExecutionResult {
        private final boolean success;
        private final String result;
        private final String error;
        
        private ExecutionResult(boolean success, String result, String error) {
            this.success = success;
            this.result = result;
            this.error = error;
        }
        
        public static ExecutionResult success(String result) {
            return new ExecutionResult(true, result, null);
        }
        
        public static ExecutionResult failure(String error) {
            return new ExecutionResult(false, null, error);
        }
        
        public boolean isSuccess() { return success; }
        public String getResult() { return result; }
        public String getError() { return error; }
    }
    
    /**
     * Code validation result
     */
    public static class CodeValidationResult {
        private final boolean valid;
        private final String reason;
        
        private CodeValidationResult(boolean valid, String reason) {
            this.valid = valid;
            this.reason = reason;
        }
        
        public static CodeValidationResult valid() {
            return new CodeValidationResult(true, null);
        }
        
        public static CodeValidationResult invalid(String reason) {
            return new CodeValidationResult(false, reason);
        }
        
        public boolean isValid() { return valid; }
        public String getReason() { return reason; }
    }
    
    /**
     * Security validation result
     */
    public static class SecurityValidationResult {
        private final boolean valid;
        private final String reason;
        
        private SecurityValidationResult(boolean valid, String reason) {
            this.valid = valid;
            this.reason = reason;
        }
        
        public static SecurityValidationResult valid() {
            return new SecurityValidationResult(true, null);
        }
        
        public static SecurityValidationResult invalid(String reason) {
            return new SecurityValidationResult(false, reason);
        }
        
        public boolean isValid() { return valid; }
        public String getReason() { return reason; }
    }
}
