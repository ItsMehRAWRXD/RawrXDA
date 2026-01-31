// ai-cli-backend/src/main/java/com/aicli/plugins/PluginProcessExecutor.java
package com.aicli.plugins;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.ArrayList;

public class PluginProcessExecutor {
    private static final int DEFAULT_TIMEOUT_SECONDS = 60;
    private static final int MAX_OUTPUT_SIZE = 1024 * 1024; // 1MB limit
    
    public String execute(Path pluginJarPath, String className, String methodName, String input) throws IOException, InterruptedException {
        return execute(pluginJarPath, className, methodName, input, DEFAULT_TIMEOUT_SECONDS, null);
    }
    
    public String execute(Path pluginJarPath, String className, String methodName, String input, int ipcPort) throws IOException, InterruptedException {
        return execute(pluginJarPath, className, methodName, input, DEFAULT_TIMEOUT_SECONDS, ipcPort);
    }
    
    public String execute(Path pluginJarPath, String className, String methodName, String input, int timeoutSeconds, Integer ipcPort) throws IOException, InterruptedException {
        // Build command with security flags
        List<String> command = new ArrayList<>();
        command.add("java");
        
        // Security JVM flags
        command.add("-Djava.security.manager");
        command.add("-Djava.security.policy=plugin.policy");
        command.add("-Xmx256m"); // Memory limit
        command.add("-XX:+UseG1GC"); // Efficient garbage collection
        command.add("-XX:MaxGCPauseMillis=100"); // GC pause limit
        
        // Plugin execution
        command.add("-jar");
        command.add(pluginJarPath.toString());
        command.add(className);
        command.add(methodName);
        command.add(input);
        
        // Add IPC port if provided
        if (ipcPort != null) {
            command.add(String.valueOf(ipcPort));
        }

        ProcessBuilder processBuilder = new ProcessBuilder(command);
        
        // Security: redirect standard output and error to capture results
        processBuilder.redirectErrorStream(true);
        File tempOutput = File.createTempFile("plugin-output-", ".log");
        tempOutput.deleteOnExit();
        processBuilder.redirectOutput(tempOutput);
        
        // Security: set working directory to temp
        processBuilder.directory(File.createTempFile("plugin-work-", "").getParentFile());
        
        // Security: limit environment variables
        processBuilder.environment().clear();
        processBuilder.environment().put("PATH", System.getenv("PATH"));
        processBuilder.environment().put("JAVA_HOME", System.getenv("JAVA_HOME"));
        
        Process process = processBuilder.start();
        boolean finished = process.waitFor(timeoutSeconds, TimeUnit.SECONDS);

        if (!finished) {
            process.destroyForcibly();
            return "Execution timed out after " + timeoutSeconds + " seconds.";
        }

        if (process.exitValue() != 0) {
            return "Execution failed with exit code " + process.exitValue();
        }
        
        // Read output with size limit
        byte[] outputBytes = java.nio.file.Files.readAllBytes(tempOutput.toPath());
        if (outputBytes.length > MAX_OUTPUT_SIZE) {
            return "Output too large (" + outputBytes.length + " bytes). Maximum allowed: " + MAX_OUTPUT_SIZE + " bytes.";
        }
        
        return new String(outputBytes, java.nio.charset.StandardCharsets.UTF_8);
    }
    
    /**
     * Execute plugin with resource monitoring
     */
    public ExecutionResult executeWithMonitoring(Path pluginJarPath, String className, String methodName, String input) {
        long startTime = System.currentTimeMillis();
        long startMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        
        try {
            String output = execute(pluginJarPath, className, methodName, input);
            long endTime = System.currentTimeMillis();
            long endMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
            
            return new ExecutionResult(true, output, endTime - startTime, endMemory - startMemory, 0);
        } catch (Exception e) {
            long endTime = System.currentTimeMillis();
            return new ExecutionResult(false, "Error: " + e.getMessage(), endTime - startTime, 0, 1);
        }
    }
    
    public static class ExecutionResult {
        private final boolean success;
        private final String output;
        private final long executionTimeMs;
        private final long memoryUsedBytes;
        private final int exitCode;
        
        public ExecutionResult(boolean success, String output, long executionTimeMs, long memoryUsedBytes, int exitCode) {
            this.success = success;
            this.output = output;
            this.executionTimeMs = executionTimeMs;
            this.memoryUsedBytes = memoryUsedBytes;
            this.exitCode = exitCode;
        }
        
        public boolean isSuccess() { return success; }
        public String getOutput() { return output; }
        public long getExecutionTimeMs() { return executionTimeMs; }
        public long getMemoryUsedBytes() { return memoryUsedBytes; }
        public int getExitCode() { return exitCode; }
        
        @Override
        public String toString() {
            return String.format("ExecutionResult{success=%s, time=%dms, memory=%d bytes, exitCode=%d}", 
                               success, executionTimeMs, memoryUsedBytes, exitCode);
        }
    }
}
