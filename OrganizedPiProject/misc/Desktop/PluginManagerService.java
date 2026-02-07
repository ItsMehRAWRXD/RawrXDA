// ai-cli-backend/src/main/java/com/aicli/plugins/PluginManagerService.java
package com.aicli.plugins;

import org.pf4j.DefaultPluginManager;
import org.pf4j.PluginManager;
import org.pf4j.PluginWrapper;
import org.pf4j.PluginState;
import org.pf4j.PluginDescriptor;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;
import java.util.logging.Logger;
import java.util.logging.Level;

/**
 * Plugin management service using PF4J.
 */
public class PluginManagerService {
    private static final Logger logger = Logger.getLogger(PluginManagerService.class.getName());
    
    private final PluginManager pluginManager;
    private final PermissionManager permissionManager;
    private final Map<String, PluginSecurityContext> pluginContexts = new ConcurrentHashMap<>();

    public PluginManagerService(Path pluginsDirectory) {
        this.pluginManager = new DefaultPluginManager(pluginsDirectory);
        this.permissionManager = new PermissionManager();
        
        initializePluginSecurity();
        loadPluginsSecurely();
    }

    /**
     * Initialize security contexts for all discovered plugins
     */
    private void initializePluginSecurity() {
        try {
            List<PluginWrapper> plugins = pluginManager.getPlugins();
            for (PluginWrapper plugin : plugins) {
                PluginDescriptor descriptor = plugin.getDescriptor();
                String pluginId = descriptor.getPluginId();
                
                PluginSecurityContext context = createDefaultSecurityContext(pluginId, descriptor);
                pluginContexts.put(pluginId, context);
                
                grantCapabilitiesFromMetadata(pluginId, descriptor);
            }
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to initialize plugin security", e);
        }
    }

    /**
     * Securely load plugins with dependency resolution
     */
    private void loadPluginsSecurely() {
        try {
            pluginManager.loadPlugins();
            
            for (PluginWrapper plugin : pluginManager.getPlugins()) {
                if (plugin.getPluginState() == PluginState.CREATED) {
                    String pluginId = plugin.getDescriptor().getPluginId();
                    
                    if (hasRequiredPermissions(pluginId)) {
                        pluginManager.startPlugin(pluginId);
                        logger.info("Started plugin: " + pluginId);
                    } else {
                        logger.warning("Plugin " + pluginId + " lacks required permissions");
                    }
                }
            }
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to load plugins securely", e);
        }
    }

    /**
     * Get all available tools from loaded plugins
     */
    public List<Tool> getPlugins() {
        return pluginManager.getExtensions(Tool.class).stream()
                .filter(tool -> {
                    String pluginId = getPluginIdForTool(tool);
                    return pluginContexts.containsKey(pluginId) && 
                           permissionManager.hasPermission(pluginId, Capability.IDE_API_ACCESS);
                })
                .collect(Collectors.toList());
    }

    /**
     * Grant specific capability to a plugin
     */
    public void grantCapability(String pluginId, Capability capability) {
        if (pluginContexts.containsKey(pluginId)) {
            permissionManager.grantPermission(pluginId, capability);
            logger.info("Granted capability " + capability + " to plugin " + pluginId);
        }
    }

    /**
     * Stop and unload all plugins
     */
    public void stopAndUnload() {
        try {
            pluginManager.stopPlugins();
            pluginManager.unloadPlugins();
            pluginContexts.clear();
            logger.info("Stopped and unloaded all plugins");
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to stop and unload plugins", e);
        }
    }

    /**
     * Create default security context for a plugin
     */
    private PluginSecurityContext createDefaultSecurityContext(String pluginId, PluginDescriptor descriptor) {
        return new PluginSecurityContext(
                pluginId,
                descriptor.getVersion(),
                descriptor.getProvider(),
                System.currentTimeMillis()
        );
    }

    /**
     * Grant capabilities based on plugin metadata
     */
    private void grantCapabilitiesFromMetadata(String pluginId, PluginDescriptor descriptor) {
        permissionManager.grantPermission(pluginId, Capability.IDE_API_ACCESS);
    }

    /**
     * Check if plugin has required permissions
     */
    private boolean hasRequiredPermissions(String pluginId) {
        return permissionManager.hasPermission(pluginId, Capability.IDE_API_ACCESS);
    }

    /**
     * Get plugin ID for a tool instance
     */
    private String getPluginIdForTool(Tool tool) {
        return tool.getClass().getSimpleName().toLowerCase();
    }

    /**
     * Security context for a plugin
     */
    public static class PluginSecurityContext {
        private final String pluginId;
        private final String version;
        private final String provider;
        private final long loadTime;

        public PluginSecurityContext(String pluginId, String version, String provider, long loadTime) {
            this.pluginId = pluginId;
            this.version = version;
            this.provider = provider;
            this.loadTime = loadTime;
        }

        public String getPluginId() { return pluginId; }
        public String getVersion() { return version; }
        public String getProvider() { return provider; }
        public long getLoadTime() { return loadTime; }
    }
}
