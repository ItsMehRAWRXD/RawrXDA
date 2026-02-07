// DeploymentManager.java - Sandboxed execution environment management

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import java.time.Duration;

/**
 * Deployment manager that provides sandboxed execution environments for tasks
 * with comprehensive resource limits and security controls.
 */
public class DeploymentManager {
    private static final Logger logger = Logger.getLogger(DeploymentManager.class.getName());
    
    // Configuration
    private boolean sandboxEnabled = true;
    private boolean resourceLimitsEnabled = true;
    private Duration defaultTimeout = Duration.ofMinutes(5);
    private long defaultMemoryLimit = 512 * 1024 * 1024; // 512MB
    private int defaultCpuLimit = 50; // 50% CPU
    
    // Active deployments
    private final Map<String, Deployment> activeDeployments = new ConcurrentHashMap<>();
    private final ExecutorService deploymentExecutor = Executors.newCachedThreadPool();
    
    // Resource monitoring
    private final ScheduledExecutorService monitoringExecutor = Executors.newScheduledThreadPool(2);
    private final Map<String, ResourceUsage> resourceUsage = new ConcurrentHashMap<>();
    
    public DeploymentManager() {
        startResourceMonitoring();
    }
    
    /**
     * Deploy a task in a sandboxed environment
     */
    public String deployTask(AgenticOrchestrator.Task task) {
        String deploymentId = UUID.randomUUID().toString();
        
        try {
            // Create deployment directory
            Path deploymentDir = createDeploymentDirectory(deploymentId);
            
            // Create deployment configuration
            DeploymentConfig config = createDeploymentConfig(task, deploymentDir);
            
            // Create deployment instance
            Deployment deployment = new Deployment(deploymentId, task, config, deploymentDir);
            
            // Initialize sandbox
            if (sandboxEnabled) {
                initializeSandbox(deployment);
            }
            
            // Set resource limits
            if (resourceLimitsEnabled) {
                setResourceLimits(deployment);
            }
            
            // Start deployment
            deployment.start();
            activeDeployments.put(deploymentId, deployment);
            
            logger.info("Deployed task " + task.getType() + " with ID: " + deploymentId);
            return deploymentId;
            
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to deploy task", e);
            throw new RuntimeException("Deployment failed", e);
        }
    }
    
    /**
     * Cleanup a deployment
     */
    public void cleanupDeployment(String deploymentId) {
        Deployment deployment = activeDeployments.remove(deploymentId);
        if (deployment != null) {
            try {
                deployment.stop();
                cleanupDeploymentDirectory(deployment.getDeploymentDir());
                resourceUsage.remove(deploymentId);
                logger.info("Cleaned up deployment: " + deploymentId);
            } catch (Exception e) {
                logger.log(Level.WARNING, "Failed to cleanup deployment: " + deploymentId, e);
            }
        }
    }
    
    /**
     * Get deployment status
     */
    public DeploymentStatus getDeploymentStatus(String deploymentId) {
        Deployment deployment = activeDeployments.get(deploymentId);
        if (deployment != null) {
            return deployment.getStatus();
        }
        return DeploymentStatus.NOT_FOUND;
    }
    
    /**
     * Get resource usage for a deployment
     */
    public ResourceUsage getResourceUsage(String deploymentId) {
        return resourceUsage.get(deploymentId);
    }
    
    /**
     * Create deployment directory
     */
    private Path createDeploymentDirectory(String deploymentId) throws IOException {
        Path baseDir = Paths.get("deployments");
        Path deploymentDir = baseDir.resolve(deploymentId);
        
        Files.createDirectories(deploymentDir);
        
        // Create subdirectories
        Files.createDirectories(deploymentDir.resolve("input"));
        Files.createDirectories(deploymentDir.resolve("output"));
        Files.createDirectories(deploymentDir.resolve("logs"));
        Files.createDirectories(deploymentDir.resolve("temp"));
        
        return deploymentDir;
    }
    
    /**
     * Create deployment configuration
     */
    private DeploymentConfig createDeploymentConfig(AgenticOrchestrator.Task task, Path deploymentDir) {
        DeploymentConfig config = new DeploymentConfig();
        config.setDeploymentDir(deploymentDir);
        config.setTimeout(defaultTimeout);
        config.setMemoryLimit(defaultMemoryLimit);
        config.setCpuLimit(defaultCpuLimit);
        config.setSandboxEnabled(sandboxEnabled);
        config.setResourceLimitsEnabled(resourceLimitsEnabled);
        
        // Task-specific configuration
        switch (task.getType()) {
            case "ai_query":
                config.setMemoryLimit(256 * 1024 * 1024); // 256MB for AI queries
                config.setTimeout(Duration.ofMinutes(2));
                break;
            case "plugin_execution":
                config.setMemoryLimit(512 * 1024 * 1024); // 512MB for plugin execution
                config.setTimeout(Duration.ofMinutes(5));
                break;
            case "tool_usage":
                config.setMemoryLimit(128 * 1024 * 1024); // 128MB for tool usage
                config.setTimeout(Duration.ofMinutes(1));
                break;
            case "file_operation":
                config.setMemoryLimit(64 * 1024 * 1024); // 64MB for file operations
                config.setTimeout(Duration.ofSeconds(30));
                break;
        }
        
        return config;
    }
    
    /**
     * Initialize sandbox for deployment
     */
    private void initializeSandbox(Deployment deployment) throws IOException {
        Path deploymentDir = deployment.getDeploymentDir();
        Path sandboxDir = deploymentDir.resolve("sandbox");
        
        // Create sandbox directory
        Files.createDirectories(sandboxDir);
        
        // Create sandbox configuration
        Path sandboxConfig = sandboxDir.resolve("sandbox.conf");
        List<String> configLines = Arrays.asList(
            "sandbox.enabled=true",
            "sandbox.readonly=true",
            "sandbox.network=false",
            "sandbox.filesystem=" + sandboxDir.toString(),
            "sandbox.memory=" + deployment.getConfig().getMemoryLimit(),
            "sandbox.cpu=" + deployment.getConfig().getCpuLimit()
        );
        Files.write(sandboxConfig, configLines);
        
        // Set sandbox permissions
        if (Files.getFileAttributeView(sandboxDir, PosixFileAttributeView.class) != null) {
            Files.setPosixFilePermissions(sandboxDir, 
                EnumSet.of(PosixFilePermission.OWNER_READ, PosixFilePermission.OWNER_WRITE, PosixFilePermission.OWNER_EXECUTE));
        }
        
        deployment.setSandboxDir(sandboxDir);
    }
    
    /**
     * Set resource limits for deployment
     */
    private void setResourceLimits(Deployment deployment) {
        DeploymentConfig config = deployment.getConfig();
        
        // Set memory limit
        if (config.getMemoryLimit() > 0) {
            // In a real implementation, this would set JVM memory limits
            logger.info("Setting memory limit: " + config.getMemoryLimit() + " bytes");
        }
        
        // Set CPU limit
        if (config.getCpuLimit() > 0) {
            // In a real implementation, this would set CPU limits
            logger.info("Setting CPU limit: " + config.getCpuLimit() + "%");
        }
        
        // Set timeout
        if (config.getTimeout() != null) {
            logger.info("Setting timeout: " + config.getTimeout());
        }
    }
    
    /**
     * Start resource monitoring
     */
    private void startResourceMonitoring() {
        monitoringExecutor.scheduleAtFixedRate(() -> {
            try {
                monitorResourceUsage();
            } catch (Exception e) {
                logger.log(Level.WARNING, "Resource monitoring failed", e);
            }
        }, 0, 5, TimeUnit.SECONDS);
        
        monitoringExecutor.scheduleAtFixedRate(() -> {
            try {
                cleanupExpiredDeployments();
            } catch (Exception e) {
                logger.log(Level.WARNING, "Deployment cleanup failed", e);
            }
        }, 0, 1, TimeUnit.MINUTES);
    }
    
    /**
     * Monitor resource usage for all active deployments
     */
    private void monitorResourceUsage() {
        for (Map.Entry<String, Deployment> entry : activeDeployments.entrySet()) {
            String deploymentId = entry.getKey();
            Deployment deployment = entry.getValue();
            
            try {
                ResourceUsage usage = calculateResourceUsage(deployment);
                resourceUsage.put(deploymentId, usage);
                
                // Check for resource violations
                if (usage.getMemoryUsage() > deployment.getConfig().getMemoryLimit()) {
                    logger.warning("Memory limit exceeded for deployment: " + deploymentId);
                    deployment.stop();
                }
                
                if (usage.getCpuUsage() > deployment.getConfig().getCpuLimit()) {
                    logger.warning("CPU limit exceeded for deployment: " + deploymentId);
                    deployment.stop();
                }
                
            } catch (Exception e) {
                logger.log(Level.WARNING, "Failed to monitor resources for deployment: " + deploymentId, e);
            }
        }
    }
    
    /**
     * Calculate resource usage for a deployment
     */
    private ResourceUsage calculateResourceUsage(Deployment deployment) {
        // In a real implementation, this would query the system for actual resource usage
        ResourceUsage usage = new ResourceUsage();
        usage.setDeploymentId(deployment.getDeploymentId());
        usage.setTimestamp(Instant.now());
        usage.setMemoryUsage(64 * 1024 * 1024); // Simulated 64MB usage
        usage.setCpuUsage(10); // Simulated 10% CPU usage
        usage.setDiskUsage(1024 * 1024); // Simulated 1MB disk usage
        return usage;
    }
    
    /**
     * Cleanup expired deployments
     */
    private void cleanupExpiredDeployments() {
        Instant now = Instant.now();
        List<String> expiredDeployments = new ArrayList<>();
        
        for (Map.Entry<String, Deployment> entry : activeDeployments.entrySet()) {
            Deployment deployment = entry.getValue();
            if (deployment.isExpired(now)) {
                expiredDeployments.add(entry.getKey());
            }
        }
        
        for (String deploymentId : expiredDeployments) {
            logger.info("Cleaning up expired deployment: " + deploymentId);
            cleanupDeployment(deploymentId);
        }
    }
    
    /**
     * Cleanup deployment directory
     */
    private void cleanupDeploymentDirectory(Path deploymentDir) {
        try {
            if (Files.exists(deploymentDir)) {
                Files.walk(deploymentDir)
                    .sorted(Comparator.reverseOrder())
                    .forEach(path -> {
                        try {
                            Files.deleteIfExists(path);
                        } catch (IOException e) {
                            logger.log(Level.WARNING, "Failed to delete: " + path, e);
                        }
                    });
            }
        } catch (IOException e) {
            logger.log(Level.WARNING, "Failed to cleanup deployment directory: " + deploymentDir, e);
        }
    }
    
    /**
     * Cleanup all resources
     */
    public void cleanup() {
        logger.info("Cleaning up DeploymentManager...");
        
        // Stop all active deployments
        for (String deploymentId : new ArrayList<>(activeDeployments.keySet())) {
            cleanupDeployment(deploymentId);
        }
        
        // Shutdown executors
        deploymentExecutor.shutdown();
        monitoringExecutor.shutdown();
        
        try {
            if (!deploymentExecutor.awaitTermination(30, TimeUnit.SECONDS)) {
                deploymentExecutor.shutdownNow();
            }
            if (!monitoringExecutor.awaitTermination(30, TimeUnit.SECONDS)) {
                monitoringExecutor.shutdownNow();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        
        logger.info("DeploymentManager cleanup complete");
    }
    
    // Configuration methods
    
    public void setSandboxEnabled(boolean enabled) {
        this.sandboxEnabled = enabled;
    }
    
    public void setResourceLimitsEnabled(boolean enabled) {
        this.resourceLimitsEnabled = enabled;
    }
    
    public void setDefaultTimeout(Duration timeout) {
        this.defaultTimeout = timeout;
    }
    
    public void setDefaultMemoryLimit(long memoryLimit) {
        this.defaultMemoryLimit = memoryLimit;
    }
    
    public void setDefaultCpuLimit(int cpuLimit) {
        this.defaultCpuLimit = cpuLimit;
    }
    
    /**
     * Deployment status enumeration
     */
    public enum DeploymentStatus {
        CREATED, RUNNING, COMPLETED, FAILED, EXPIRED, NOT_FOUND
    }
    
    /**
     * Deployment configuration
     */
    public static class DeploymentConfig {
        private Path deploymentDir;
        private Duration timeout;
        private long memoryLimit;
        private int cpuLimit;
        private boolean sandboxEnabled;
        private boolean resourceLimitsEnabled;
        
        // Getters and setters
        public Path getDeploymentDir() { return deploymentDir; }
        public void setDeploymentDir(Path deploymentDir) { this.deploymentDir = deploymentDir; }
        public Duration getTimeout() { return timeout; }
        public void setTimeout(Duration timeout) { this.timeout = timeout; }
        public long getMemoryLimit() { return memoryLimit; }
        public void setMemoryLimit(long memoryLimit) { this.memoryLimit = memoryLimit; }
        public int getCpuLimit() { return cpuLimit; }
        public void setCpuLimit(int cpuLimit) { this.cpuLimit = cpuLimit; }
        public boolean isSandboxEnabled() { return sandboxEnabled; }
        public void setSandboxEnabled(boolean sandboxEnabled) { this.sandboxEnabled = sandboxEnabled; }
        public boolean isResourceLimitsEnabled() { return resourceLimitsEnabled; }
        public void setResourceLimitsEnabled(boolean resourceLimitsEnabled) { this.resourceLimitsEnabled = resourceLimitsEnabled; }
    }
    
    /**
     * Deployment instance
     */
    public static class Deployment {
        private final String deploymentId;
        private final AgenticOrchestrator.Task task;
        private final DeploymentConfig config;
        private final Path deploymentDir;
        private Path sandboxDir;
        private DeploymentStatus status;
        private Instant startTime;
        private Instant endTime;
        private String error;
        
        public Deployment(String deploymentId, AgenticOrchestrator.Task task, DeploymentConfig config, Path deploymentDir) {
            this.deploymentId = deploymentId;
            this.task = task;
            this.config = config;
            this.deploymentDir = deploymentDir;
            this.status = DeploymentStatus.CREATED;
        }
        
        public void start() {
            this.status = DeploymentStatus.RUNNING;
            this.startTime = Instant.now();
        }
        
        public void stop() {
            this.status = DeploymentStatus.COMPLETED;
            this.endTime = Instant.now();
        }
        
        public void fail(String error) {
            this.status = DeploymentStatus.FAILED;
            this.endTime = Instant.now();
            this.error = error;
        }
        
        public boolean isExpired(Instant now) {
            if (startTime == null) return false;
            return now.isAfter(startTime.plus(config.getTimeout()));
        }
        
        // Getters and setters
        public String getDeploymentId() { return deploymentId; }
        public AgenticOrchestrator.Task getTask() { return task; }
        public DeploymentConfig getConfig() { return config; }
        public Path getDeploymentDir() { return deploymentDir; }
        public Path getSandboxDir() { return sandboxDir; }
        public void setSandboxDir(Path sandboxDir) { this.sandboxDir = sandboxDir; }
        public DeploymentStatus getStatus() { return status; }
        public void setStatus(DeploymentStatus status) { this.status = status; }
        public Instant getStartTime() { return startTime; }
        public Instant getEndTime() { return endTime; }
        public String getError() { return error; }
    }
    
    /**
     * Resource usage tracking
     */
    public static class ResourceUsage {
        private String deploymentId;
        private Instant timestamp;
        private long memoryUsage;
        private int cpuUsage;
        private long diskUsage;
        
        // Getters and setters
        public String getDeploymentId() { return deploymentId; }
        public void setDeploymentId(String deploymentId) { this.deploymentId = deploymentId; }
        public Instant getTimestamp() { return timestamp; }
        public void setTimestamp(Instant timestamp) { this.timestamp = timestamp; }
        public long getMemoryUsage() { return memoryUsage; }
        public void setMemoryUsage(long memoryUsage) { this.memoryUsage = memoryUsage; }
        public int getCpuUsage() { return cpuUsage; }
        public void setCpuUsage(int cpuUsage) { this.cpuUsage = cpuUsage; }
        public long getDiskUsage() { return diskUsage; }
        public void setDiskUsage(long diskUsage) { this.diskUsage = diskUsage; }
    }
}
