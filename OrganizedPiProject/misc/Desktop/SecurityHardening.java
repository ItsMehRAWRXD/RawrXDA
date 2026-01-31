// SecurityHardening.java - Comprehensive security hardening and monitoring

import java.util.*;
import java.util.concurrent.*;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import java.time.Duration;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Security hardening system that provides comprehensive security controls,
 * monitoring, and incident response capabilities.
 */
public class SecurityHardening {
    private static final Logger logger = Logger.getLogger(SecurityHardening.class.getName());
    
    // Configuration
    private boolean intrusionDetectionEnabled = true;
    private boolean rateLimitingEnabled = true;
    private boolean anomalyDetectionEnabled = true;
    private boolean incidentResponseEnabled = true;
    private boolean securityLoggingEnabled = true;
    
    // Rate limiting
    private final Map<String, RateLimit> rateLimits = new ConcurrentHashMap<>();
    private final int defaultRateLimit = 100; // requests per minute
    private final Duration rateLimitWindow = Duration.ofMinutes(1);
    
    // Intrusion detection
    private final Map<String, List<SecurityEvent>> securityEvents = new ConcurrentHashMap<>();
    private final Map<String, IntrusionAttempt> intrusionAttempts = new ConcurrentHashMap<>();
    private final Set<String> blockedIPs = ConcurrentHashMap.newKeySet();
    private final Set<String> blockedUsers = ConcurrentHashMap.newKeySet();
    
    // Anomaly detection
    private final Map<String, UserBehavior> userBehaviors = new ConcurrentHashMap<>();
    private final Map<String, SystemMetrics> systemMetrics = new ConcurrentHashMap<>();
    private final List<Anomaly> detectedAnomalies = Collections.synchronizedList(new ArrayList<>());
    
    // Incident response
    private final Map<String, SecurityIncident> activeIncidents = new ConcurrentHashMap<>();
    private final List<SecurityIncident> resolvedIncidents = Collections.synchronizedList(new ArrayList<>());
    private final IncidentResponsePlan incidentResponsePlan = new IncidentResponsePlan();
    
    // Security monitoring
    private final ScheduledExecutorService monitoringExecutor = Executors.newScheduledThreadPool(4);
    private final SecurityMetrics securityMetrics = new SecurityMetrics();
    
    public SecurityHardening() {
        initializeSecurityHardening();
        startSecurityMonitoring();
    }
    
    /**
     * Initialize security hardening
     */
    private void initializeSecurityHardening() {
        // Initialize rate limits
        initializeRateLimits();
        
        // Initialize intrusion detection
        initializeIntrusionDetection();
        
        // Initialize anomaly detection
        initializeAnomalyDetection();
        
        // Initialize incident response
        initializeIncidentResponse();
        
        logger.info("Security hardening initialized");
    }
    
    /**
     * Start security monitoring
     */
    private void startSecurityMonitoring() {
        // Rate limit monitoring
        monitoringExecutor.scheduleAtFixedRate(this::monitorRateLimits, 0, 1, TimeUnit.MINUTES);
        
        // Intrusion detection monitoring
        monitoringExecutor.scheduleAtFixedRate(this::monitorIntrusionDetection, 0, 30, TimeUnit.SECONDS);
        
        // Anomaly detection monitoring
        monitoringExecutor.scheduleAtFixedRate(this::monitorAnomalyDetection, 0, 2, TimeUnit.MINUTES);
        
        // Incident response monitoring
        monitoringExecutor.scheduleAtFixedRate(this::monitorIncidentResponse, 0, 1, TimeUnit.MINUTES);
        
        // Security metrics collection
        monitoringExecutor.scheduleAtFixedRate(this::collectSecurityMetrics, 0, 30, TimeUnit.SECONDS);
        
        // Cleanup old data
        monitoringExecutor.scheduleAtFixedRate(this::cleanupOldData, 0, 1, TimeUnit.HOURS);
        
        logger.info("Security monitoring started");
    }
    
    /**
     * Check rate limit for a user/IP
     */
    public boolean checkRateLimit(String identifier, String operation) {
        if (!rateLimitingEnabled) {
            return true;
        }
        
        try {
            String key = identifier + ":" + operation;
            RateLimit rateLimit = rateLimits.computeIfAbsent(key, k -> new RateLimit(defaultRateLimit, rateLimitWindow));
            
            boolean allowed = rateLimit.isAllowed();
            if (!allowed) {
                recordSecurityEvent(identifier, "RATE_LIMIT_EXCEEDED", "Rate limit exceeded for operation: " + operation);
                securityMetrics.incrementRateLimitViolations();
            }
            
            return allowed;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Rate limit check failed", e);
            return false; // Fail secure
        }
    }
    
    /**
     * Record security event
     */
    public void recordSecurityEvent(String identifier, String eventType, String description) {
        if (!securityLoggingEnabled) {
            return;
        }
        
        try {
            SecurityEvent event = new SecurityEvent(
                UUID.randomUUID().toString(), identifier, eventType, description, Instant.now());
            
            securityEvents.computeIfAbsent(identifier, k -> new ArrayList<>()).add(event);
            securityMetrics.incrementSecurityEvents();
            
            // Check for intrusion patterns
            if (intrusionDetectionEnabled) {
                checkIntrusionPatterns(identifier, event);
            }
            
            // Update user behavior
            if (anomalyDetectionEnabled) {
                updateUserBehavior(identifier, event);
            }
            
            logger.info("Security event recorded: " + eventType + " for " + identifier);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to record security event", e);
        }
    }
    
    /**
     * Check for intrusion patterns
     */
    private void checkIntrusionPatterns(String identifier, SecurityEvent event) {
        try {
            List<SecurityEvent> userEvents = securityEvents.get(identifier);
            if (userEvents == null || userEvents.size() < 5) {
                return;
            }
            
            // Check for rapid successive events
            Instant now = Instant.now();
            long recentEvents = userEvents.stream()
                .filter(e -> e.getTimestamp().isAfter(now.minus(Duration.ofMinutes(5))))
                .count();
            
            if (recentEvents > 20) {
                handleIntrusionAttempt(identifier, "RAPID_EVENTS", "Too many events in short time");
                return;
            }
            
            // Check for suspicious event patterns
            Map<String, Long> eventCounts = new HashMap<>();
            userEvents.stream()
                .filter(e -> e.getTimestamp().isAfter(now.minus(Duration.ofHours(1))))
                .forEach(e -> eventCounts.merge(e.getEventType(), 1L, Long::sum));
            
            // Check for high-risk event combinations
            if (eventCounts.getOrDefault("AUTHENTICATION_FAILURE", 0L) > 10 &&
                eventCounts.getOrDefault("PRIVILEGE_ESCALATION", 0L) > 0) {
                handleIntrusionAttempt(identifier, "AUTH_PRIVILEGE_ESCALATION", "Authentication failure with privilege escalation");
            }
            
            if (eventCounts.getOrDefault("RATE_LIMIT_EXCEEDED", 0L) > 5 &&
                eventCounts.getOrDefault("MALICIOUS_REQUEST", 0L) > 0) {
                handleIntrusionAttempt(identifier, "RATE_LIMIT_MALICIOUS", "Rate limit exceeded with malicious requests");
            }
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Intrusion pattern check failed", e);
        }
    }
    
    /**
     * Handle intrusion attempt
     */
    private void handleIntrusionAttempt(String identifier, String attackType, String description) {
        try {
            IntrusionAttempt attempt = new IntrusionAttempt(
                UUID.randomUUID().toString(), identifier, attackType, description, Instant.now());
            
            intrusionAttempts.put(identifier, attempt);
            securityMetrics.incrementIntrusionAttempts();
            
            // Block identifier if threshold exceeded
            if (intrusionAttempts.size() > 3) {
                blockIdentifier(identifier, "Multiple intrusion attempts");
            }
            
            // Create security incident
            if (incidentResponseEnabled) {
                createSecurityIncident("INTRUSION_ATTEMPT", "Intrusion attempt detected: " + description, identifier);
            }
            
            logger.warning("Intrusion attempt detected: " + attackType + " for " + identifier);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to handle intrusion attempt", e);
        }
    }
    
    /**
     * Block identifier
     */
    private void blockIdentifier(String identifier, String reason) {
        try {
            // Determine if it's an IP or user
            if (identifier.matches("\\d+\\.\\d+\\.\\d+\\.\\d+")) {
                blockedIPs.add(identifier);
            } else {
                blockedUsers.add(identifier);
            }
            
            recordSecurityEvent(identifier, "BLOCKED", "Identifier blocked: " + reason);
            securityMetrics.incrementBlockedIdentifiers();
            
            logger.warning("Identifier blocked: " + identifier + " - " + reason);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to block identifier", e);
        }
    }
    
    /**
     * Check if identifier is blocked
     */
    public boolean isBlocked(String identifier) {
        return blockedIPs.contains(identifier) || blockedUsers.contains(identifier);
    }
    
    /**
     * Update user behavior
     */
    private void updateUserBehavior(String identifier, SecurityEvent event) {
        try {
            UserBehavior behavior = userBehaviors.computeIfAbsent(identifier, k -> new UserBehavior(identifier));
            behavior.addEvent(event);
            
            // Check for anomalies
            if (behavior.isAnomalous()) {
                Anomaly anomaly = new Anomaly(
                    UUID.randomUUID().toString(), identifier, "USER_BEHAVIOR", 
                    "Anomalous user behavior detected", Instant.now());
                
                detectedAnomalies.add(anomaly);
                securityMetrics.incrementAnomalies();
                
                logger.warning("Anomaly detected for user: " + identifier);
            }
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to update user behavior", e);
        }
    }
    
    /**
     * Create security incident
     */
    private void createSecurityIncident(String incidentType, String description, String affectedResource) {
        try {
            SecurityIncident incident = new SecurityIncident(
                UUID.randomUUID().toString(), incidentType, description, affectedResource, Instant.now());
            
            activeIncidents.put(incident.getId(), incident);
            securityMetrics.incrementSecurityIncidents();
            
            // Execute incident response plan
            incidentResponsePlan.executeResponse(incident);
            
            logger.warning("Security incident created: " + incidentType + " - " + description);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to create security incident", e);
        }
    }
    
    /**
     * Resolve security incident
     */
    public void resolveSecurityIncident(String incidentId, String resolution) {
        try {
            SecurityIncident incident = activeIncidents.remove(incidentId);
            if (incident != null) {
                incident.setResolved(true);
                incident.setResolution(resolution);
                incident.setResolvedAt(Instant.now());
                resolvedIncidents.add(incident);
                
                logger.info("Security incident resolved: " + incidentId);
            }
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Failed to resolve security incident", e);
        }
    }
    
    /**
     * Get security status
     */
    public SecurityStatus getSecurityStatus() {
        SecurityStatus status = new SecurityStatus();
        status.setTimestamp(Instant.now());
        status.setActiveIncidents(activeIncidents.size());
        status.setResolvedIncidents(resolvedIncidents.size());
        status.setBlockedIPs(blockedIPs.size());
        status.setBlockedUsers(blockedUsers.size());
        status.setDetectedAnomalies(detectedAnomalies.size());
        status.setSecurityMetrics(securityMetrics.getMetrics());
        return status;
    }
    
    /**
     * Get security events for identifier
     */
    public List<SecurityEvent> getSecurityEvents(String identifier) {
        return securityEvents.getOrDefault(identifier, new ArrayList<>());
    }
    
    /**
     * Get active incidents
     */
    public List<SecurityIncident> getActiveIncidents() {
        return new ArrayList<>(activeIncidents.values());
    }
    
    /**
     * Get detected anomalies
     */
    public List<Anomaly> getDetectedAnomalies() {
        return new ArrayList<>(detectedAnomalies);
    }
    
    /**
     * Initialize rate limits
     */
    private void initializeRateLimits() {
        // Default rate limits for different operations
        rateLimits.put("compile:default", new RateLimit(50, Duration.ofMinutes(1)));
        rateLimits.put("security:analyze", new RateLimit(100, Duration.ofMinutes(1)));
        rateLimits.put("admin:default", new RateLimit(10, Duration.ofMinutes(1)));
    }
    
    /**
     * Initialize intrusion detection
     */
    private void initializeIntrusionDetection() {
        // Intrusion detection patterns and thresholds
        logger.info("Intrusion detection initialized");
    }
    
    /**
     * Initialize anomaly detection
     */
    private void initializeAnomalyDetection() {
        // Anomaly detection algorithms and baselines
        logger.info("Anomaly detection initialized");
    }
    
    /**
     * Initialize incident response
     */
    private void initializeIncidentResponse() {
        // Incident response procedures and automation
        logger.info("Incident response initialized");
    }
    
    /**
     * Monitor rate limits
     */
    private void monitorRateLimits() {
        try {
            // Clean up expired rate limits
            rateLimits.entrySet().removeIf(entry -> entry.getValue().isExpired());
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Rate limit monitoring failed", e);
        }
    }
    
    /**
     * Monitor intrusion detection
     */
    private void monitorIntrusionDetection() {
        try {
            // Check for expired intrusion attempts
            intrusionAttempts.entrySet().removeIf(entry -> 
                entry.getValue().getTimestamp().isBefore(Instant.now().minus(Duration.ofHours(24))));
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Intrusion detection monitoring failed", e);
        }
    }
    
    /**
     * Monitor anomaly detection
     */
    private void monitorAnomalyDetection() {
        try {
            // Analyze system metrics for anomalies
            // This would implement sophisticated anomaly detection algorithms
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Anomaly detection monitoring failed", e);
        }
    }
    
    /**
     * Monitor incident response
     */
    private void monitorIncidentResponse() {
        try {
            // Check for incidents that need escalation
            for (SecurityIncident incident : activeIncidents.values()) {
                if (incident.needsEscalation()) {
                    incidentResponsePlan.escalateIncident(incident);
                }
            }
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Incident response monitoring failed", e);
        }
    }
    
    /**
     * Collect security metrics
     */
    private void collectSecurityMetrics() {
        try {
            // Update system metrics
            SystemMetrics metrics = new SystemMetrics();
            metrics.setTimestamp(Instant.now());
            metrics.setActiveConnections(Thread.activeCount());
            metrics.setMemoryUsage(Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory());
            metrics.setCpuUsage(0); // Would need system-specific implementation
            
            systemMetrics.put("current", metrics);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Security metrics collection failed", e);
        }
    }
    
    /**
     * Cleanup old data
     */
    private void cleanupOldData() {
        try {
            Instant cutoff = Instant.now().minus(Duration.ofDays(30));
            
            // Clean up old security events
            for (List<SecurityEvent> events : securityEvents.values()) {
                events.removeIf(event -> event.getTimestamp().isBefore(cutoff));
            }
            
            // Clean up old anomalies
            detectedAnomalies.removeIf(anomaly -> anomaly.getTimestamp().isBefore(cutoff));
            
            // Clean up old resolved incidents
            resolvedIncidents.removeIf(incident -> incident.getResolvedAt().isBefore(cutoff));
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Data cleanup failed", e);
        }
    }
    
    /**
     * Shutdown security hardening
     */
    public void shutdown() {
        logger.info("Shutting down security hardening...");
        
        monitoringExecutor.shutdown();
        try {
            if (!monitoringExecutor.awaitTermination(30, TimeUnit.SECONDS)) {
                monitoringExecutor.shutdownNow();
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            monitoringExecutor.shutdownNow();
        }
        
        logger.info("Security hardening shutdown complete");
    }
    
    // Configuration methods
    
    public void setIntrusionDetectionEnabled(boolean enabled) {
        this.intrusionDetectionEnabled = enabled;
    }
    
    public void setRateLimitingEnabled(boolean enabled) {
        this.rateLimitingEnabled = enabled;
    }
    
    public void setAnomalyDetectionEnabled(boolean enabled) {
        this.anomalyDetectionEnabled = enabled;
    }
    
    public void setIncidentResponseEnabled(boolean enabled) {
        this.incidentResponseEnabled = enabled;
    }
    
    public void setSecurityLoggingEnabled(boolean enabled) {
        this.securityLoggingEnabled = enabled;
    }
    
    /**
     * Rate limit class
     */
    public static class RateLimit {
        private final int limit;
        private final Duration window;
        private final Queue<Instant> requests = new ConcurrentLinkedQueue<>();
        
        public RateLimit(int limit, Duration window) {
            this.limit = limit;
            this.window = window;
        }
        
        public boolean isAllowed() {
            Instant now = Instant.now();
            Instant cutoff = now.minus(window);
            
            // Remove old requests
            requests.removeIf(timestamp -> timestamp.isBefore(cutoff));
            
            // Check if under limit
            if (requests.size() >= limit) {
                return false;
            }
            
            // Add current request
            requests.offer(now);
            return true;
        }
        
        public boolean isExpired() {
            return requests.isEmpty() || 
                   requests.peek().isBefore(Instant.now().minus(window));
        }
    }
    
    /**
     * Security event class
     */
    public static class SecurityEvent {
        private final String id;
        private final String identifier;
        private final String eventType;
        private final String description;
        private final Instant timestamp;
        
        public SecurityEvent(String id, String identifier, String eventType, String description, Instant timestamp) {
            this.id = id;
            this.identifier = identifier;
            this.eventType = eventType;
            this.description = description;
            this.timestamp = timestamp;
        }
        
        // Getters
        public String getId() { return id; }
        public String getIdentifier() { return identifier; }
        public String getEventType() { return eventType; }
        public String getDescription() { return description; }
        public Instant getTimestamp() { return timestamp; }
    }
    
    /**
     * Intrusion attempt class
     */
    public static class IntrusionAttempt {
        private final String id;
        private final String identifier;
        private final String attackType;
        private final String description;
        private final Instant timestamp;
        
        public IntrusionAttempt(String id, String identifier, String attackType, String description, Instant timestamp) {
            this.id = id;
            this.identifier = identifier;
            this.attackType = attackType;
            this.description = description;
            this.timestamp = timestamp;
        }
        
        // Getters
        public String getId() { return id; }
        public String getIdentifier() { return identifier; }
        public String getAttackType() { return attackType; }
        public String getDescription() { return description; }
        public Instant getTimestamp() { return timestamp; }
    }
    
    /**
     * User behavior class
     */
    public static class UserBehavior {
        private final String identifier;
        private final List<SecurityEvent> events = new ArrayList<>();
        private final Map<String, Integer> eventCounts = new HashMap<>();
        
        public UserBehavior(String identifier) {
            this.identifier = identifier;
        }
        
        public void addEvent(SecurityEvent event) {
            events.add(event);
            eventCounts.merge(event.getEventType(), 1, Integer::sum);
        }
        
        public boolean isAnomalous() {
            // Simple anomaly detection - in practice, this would be more sophisticated
            return eventCounts.getOrDefault("AUTHENTICATION_FAILURE", 0) > 5 ||
                   eventCounts.getOrDefault("PRIVILEGE_ESCALATION", 0) > 0 ||
                   eventCounts.getOrDefault("MALICIOUS_REQUEST", 0) > 3;
        }
        
        // Getters
        public String getIdentifier() { return identifier; }
        public List<SecurityEvent> getEvents() { return events; }
        public Map<String, Integer> getEventCounts() { return eventCounts; }
    }
    
    /**
     * System metrics class
     */
    public static class SystemMetrics {
        private Instant timestamp;
        private int activeConnections;
        private long memoryUsage;
        private int cpuUsage;
        
        // Getters and setters
        public Instant getTimestamp() { return timestamp; }
        public void setTimestamp(Instant timestamp) { this.timestamp = timestamp; }
        public int getActiveConnections() { return activeConnections; }
        public void setActiveConnections(int activeConnections) { this.activeConnections = activeConnections; }
        public long getMemoryUsage() { return memoryUsage; }
        public void setMemoryUsage(long memoryUsage) { this.memoryUsage = memoryUsage; }
        public int getCpuUsage() { return cpuUsage; }
        public void setCpuUsage(int cpuUsage) { this.cpuUsage = cpuUsage; }
    }
    
    /**
     * Anomaly class
     */
    public static class Anomaly {
        private final String id;
        private final String identifier;
        private final String anomalyType;
        private final String description;
        private final Instant timestamp;
        
        public Anomaly(String id, String identifier, String anomalyType, String description, Instant timestamp) {
            this.id = id;
            this.identifier = identifier;
            this.anomalyType = anomalyType;
            this.description = description;
            this.timestamp = timestamp;
        }
        
        // Getters
        public String getId() { return id; }
        public String getIdentifier() { return identifier; }
        public String getAnomalyType() { return anomalyType; }
        public String getDescription() { return description; }
        public Instant getTimestamp() { return timestamp; }
    }
    
    /**
     * Security incident class
     */
    public static class SecurityIncident {
        private final String id;
        private final String incidentType;
        private final String description;
        private final String affectedResource;
        private final Instant createdAt;
        private boolean resolved;
        private String resolution;
        private Instant resolvedAt;
        
        public SecurityIncident(String id, String incidentType, String description, String affectedResource, Instant createdAt) {
            this.id = id;
            this.incidentType = incidentType;
            this.description = description;
            this.affectedResource = affectedResource;
            this.createdAt = createdAt;
            this.resolved = false;
        }
        
        public boolean needsEscalation() {
            return !resolved && createdAt.isBefore(Instant.now().minus(Duration.ofHours(1)));
        }
        
        // Getters and setters
        public String getId() { return id; }
        public String getIncidentType() { return incidentType; }
        public String getDescription() { return description; }
        public String getAffectedResource() { return affectedResource; }
        public Instant getCreatedAt() { return createdAt; }
        public boolean isResolved() { return resolved; }
        public void setResolved(boolean resolved) { this.resolved = resolved; }
        public String getResolution() { return resolution; }
        public void setResolution(String resolution) { this.resolution = resolution; }
        public Instant getResolvedAt() { return resolvedAt; }
        public void setResolvedAt(Instant resolvedAt) { this.resolvedAt = resolvedAt; }
    }
    
    /**
     * Incident response plan class
     */
    public static class IncidentResponsePlan {
        public void executeResponse(SecurityIncident incident) {
            // Implement incident response procedures
            logger.info("Executing incident response for: " + incident.getIncidentType());
        }
        
        public void escalateIncident(SecurityIncident incident) {
            // Implement incident escalation procedures
            logger.warning("Escalating incident: " + incident.getId());
        }
    }
    
    /**
     * Security metrics class
     */
    public static class SecurityMetrics {
        private final AtomicLong securityEvents = new AtomicLong(0);
        private final AtomicLong intrusionAttempts = new AtomicLong(0);
        private final AtomicLong anomalies = new AtomicLong(0);
        private final AtomicLong securityIncidents = new AtomicLong(0);
        private final AtomicLong rateLimitViolations = new AtomicLong(0);
        private final AtomicLong blockedIdentifiers = new AtomicLong(0);
        
        public void incrementSecurityEvents() { securityEvents.incrementAndGet(); }
        public void incrementIntrusionAttempts() { intrusionAttempts.incrementAndGet(); }
        public void incrementAnomalies() { anomalies.incrementAndGet(); }
        public void incrementSecurityIncidents() { securityIncidents.incrementAndGet(); }
        public void incrementRateLimitViolations() { rateLimitViolations.incrementAndGet(); }
        public void incrementBlockedIdentifiers() { blockedIdentifiers.incrementAndGet(); }
        
        public Map<String, Object> getMetrics() {
            Map<String, Object> metrics = new HashMap<>();
            metrics.put("securityEvents", securityEvents.get());
            metrics.put("intrusionAttempts", intrusionAttempts.get());
            metrics.put("anomalies", anomalies.get());
            metrics.put("securityIncidents", securityIncidents.get());
            metrics.put("rateLimitViolations", rateLimitViolations.get());
            metrics.put("blockedIdentifiers", blockedIdentifiers.get());
            return metrics;
        }
    }
    
    /**
     * Security status class
     */
    public static class SecurityStatus {
        private Instant timestamp;
        private int activeIncidents;
        private int resolvedIncidents;
        private int blockedIPs;
        private int blockedUsers;
        private int detectedAnomalies;
        private Map<String, Object> securityMetrics;
        
        // Getters and setters
        public Instant getTimestamp() { return timestamp; }
        public void setTimestamp(Instant timestamp) { this.timestamp = timestamp; }
        public int getActiveIncidents() { return activeIncidents; }
        public void setActiveIncidents(int activeIncidents) { this.activeIncidents = activeIncidents; }
        public int getResolvedIncidents() { return resolvedIncidents; }
        public void setResolvedIncidents(int resolvedIncidents) { this.resolvedIncidents = resolvedIncidents; }
        public int getBlockedIPs() { return blockedIPs; }
        public void setBlockedIPs(int blockedIPs) { this.blockedIPs = blockedIPs; }
        public int getBlockedUsers() { return blockedUsers; }
        public void setBlockedUsers(int blockedUsers) { this.blockedUsers = blockedUsers; }
        public int getDetectedAnomalies() { return detectedAnomalies; }
        public void setDetectedAnomalies(int detectedAnomalies) { this.detectedAnomalies = detectedAnomalies; }
        public Map<String, Object> getSecurityMetrics() { return securityMetrics; }
        public void setSecurityMetrics(Map<String, Object> securityMetrics) { this.securityMetrics = securityMetrics; }
    }
}
