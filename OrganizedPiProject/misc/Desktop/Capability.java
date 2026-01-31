// ai-cli-backend/src/main/java/com/aicli/plugins/Capability.java
package com.aicli.plugins;

/**
 * Defines the granular capabilities that plugins can request.
 * Each capability represents a specific permission that must be explicitly granted.
 */
public enum Capability {
    // File system access capabilities
    FILE_READ_ACCESS("Read files from the filesystem"),
    FILE_WRITE_ACCESS("Write files to the filesystem"),
    FILE_DELETE_ACCESS("Delete files from the filesystem"),
    
    // Network access capabilities
    NETWORK_ACCESS("Make outbound network requests"),
    NETWORK_LISTEN("Listen for inbound network connections"),
    
    // System execution capabilities
    EXECUTE_SYSTEM_COMMAND("Execute system commands"),
    EXECUTE_NATIVE_CODE("Execute native code"),
    
    // IDE integration capabilities
    IDE_API_ACCESS("Access IDE APIs and services"),
    CODE_COMPLETION_ACCESS("Provide code completion suggestions"),
    REFACTORING_ACCESS("Perform code refactoring operations"),
    DEBUGGING_ACCESS("Access debugging information"),
    
    // Data access capabilities
    DATABASE_ACCESS("Access databases"),
    CACHE_ACCESS("Access caching systems"),
    CONFIG_ACCESS("Read configuration files"),
    
    // Advanced capabilities
    PLUGIN_MANAGEMENT("Manage other plugins"),
    SECURITY_CONTEXT_ACCESS("Access security contexts"),
    TRACING_ACCESS("Access distributed tracing");

    private final String description;

    Capability(String description) {
        this.description = description;
    }

    public String getDescription() {
        return description;
    }

    /**
     * Check if this capability is considered high-risk
     */
    public boolean isHighRisk() {
        return this == EXECUTE_SYSTEM_COMMAND ||
               this == EXECUTE_NATIVE_CODE ||
               this == NETWORK_ACCESS ||
               this == PLUGIN_MANAGEMENT ||
               this == SECURITY_CONTEXT_ACCESS;
    }

    /**
     * Check if this capability requires additional security validation
     */
    public boolean requiresSecurityValidation() {
        return isHighRisk() || this == DATABASE_ACCESS;
    }

    /**
     * Get the default timeout for operations requiring this capability
     */
    public int getDefaultTimeoutSeconds() {
        return switch (this) {
            case NETWORK_ACCESS -> 30;
            case EXECUTE_SYSTEM_COMMAND, DATABASE_ACCESS -> 60;
            default -> 10;
        };
    }
}
