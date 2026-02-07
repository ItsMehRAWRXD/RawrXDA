// ai-cli-backend/src/main/java/com/aicli/plugins/PermissionManager.java
package com.aicli.plugins;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Collections;

/**
 * Permission management system for plugin capabilities.
 */
public class PermissionManager {
    private static final Logger logger = Logger.getLogger(PermissionManager.class.getName());
    
    private final Map<String, Set<Capability>> pluginPermissions = new ConcurrentHashMap<>();
    private final ReadWriteLock lock = new ReentrantReadWriteLock();
    
    /**
     * Grant a capability to a plugin
     */
    public boolean grantPermission(String pluginId, Capability capability) {
        lock.writeLock().lock();
        try {
            if (pluginId == null || capability == null) {
                return false;
            }
            
            pluginPermissions.computeIfAbsent(pluginId, k -> ConcurrentHashMap.newKeySet()).add(capability);
            logger.info("Granted capability " + capability + " to plugin " + pluginId);
            return true;
            
        } finally {
            lock.writeLock().unlock();
        }
    }
    
    /**
     * Revoke a capability from a plugin
     */
    public boolean revokePermission(String pluginId, Capability capability) {
        lock.writeLock().lock();
        try {
            Set<Capability> permissions = pluginPermissions.get(pluginId);
            if (permissions != null) {
                permissions.remove(capability);
                logger.info("Revoked capability " + capability + " from plugin " + pluginId);
                return true;
            }
            return false;
            
        } finally {
            lock.writeLock().unlock();
        }
    }
    
    /**
     * Check if a plugin has a specific capability
     */
    public boolean hasPermission(String pluginId, Capability capability) {
        lock.readLock().lock();
        try {
            Set<Capability> permissions = pluginPermissions.get(pluginId);
            return permissions != null && permissions.contains(capability);
            
        } finally {
            lock.readLock().unlock();
        }
    }
    
    /**
     * Get all capabilities for a plugin
     */
    public Set<Capability> getPermissions(String pluginId) {
        lock.readLock().lock();
        try {
            Set<Capability> permissions = pluginPermissions.get(pluginId);
            return permissions != null ? Set.copyOf(permissions) : Collections.emptySet();
        } finally {
            lock.readLock().unlock();
        }
    }
}
