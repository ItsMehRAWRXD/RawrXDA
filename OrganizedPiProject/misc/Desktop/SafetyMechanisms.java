// SafetyMechanisms.java - Comprehensive safety mechanisms and ethical constraints

import java.util.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Safety mechanisms that enforce ethical constraints and prevent harmful operations
 * with comprehensive content filtering and safety validation.
 */
public class SafetyMechanisms {
    private static final Logger logger = Logger.getLogger(SafetyMechanisms.class.getName());
    
    // Configuration
    private boolean ethicalConstraintsEnabled = true;
    private boolean contentFilteringEnabled = true;
    private boolean safetyValidationEnabled = true;
    private boolean humanApprovalRequired = false;
    
    // Safety patterns and rules
    private final Map<String, List<Pattern>> harmfulPatterns = new ConcurrentHashMap<>();
    private final Map<String, List<Pattern>> ethicalViolationPatterns = new ConcurrentHashMap<>();
    private final Map<String, SafetyRule> safetyRules = new ConcurrentHashMap<>();
    
    // Safety history and tracking
    private final Map<String, List<SafetyViolation>> violationHistory = new ConcurrentHashMap<>();
    private final Map<String, SafetyScore> userSafetyScores = new ConcurrentHashMap<>();
    
    // Approval system
    private final Map<String, ApprovalRequest> pendingApprovals = new ConcurrentHashMap<>();
    
    public SafetyMechanisms(boolean ethicalConstraintsEnabled) {
        this.ethicalConstraintsEnabled = ethicalConstraintsEnabled;
        initializeHarmfulPatterns();
        initializeEthicalViolationPatterns();
        initializeSafetyRules();
    }
    
    /**
     * Validate a user request for safety and ethical compliance
     */
    public boolean validateUserRequest(String request, String userId) {
        if (!safetyValidationEnabled) {
            return true;
        }
        
        try {
            // Check for harmful content
            if (contentFilteringEnabled && containsHarmfulContent(request)) {
                recordSafetyViolation(userId, "HARMFUL_CONTENT", "Request contains harmful content", request);
                return false;
            }
            
            // Check for ethical violations
            if (ethicalConstraintsEnabled && violatesEthicalConstraints(request)) {
                recordSafetyViolation(userId, "ETHICAL_VIOLATION", "Request violates ethical constraints", request);
                return false;
            }
            
            // Check safety rules
            if (!compliesWithSafetyRules(request, userId)) {
                recordSafetyViolation(userId, "SAFETY_RULE_VIOLATION", "Request violates safety rules", request);
                return false;
            }
            
            // Update user safety score
            updateUserSafetyScore(userId, true);
            
            return true;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Safety validation failed for user: " + userId, e);
            return false;
        }
    }
    
    /**
     * Check if a request violates ethical constraints
     */
    public boolean checkEthicalConstraints(String request) {
        if (!ethicalConstraintsEnabled) {
            return true;
        }
        
        return !violatesEthicalConstraints(request);
    }
    
    /**
     * Validate task execution for safety
     */
    public boolean validateTaskExecution(AgenticOrchestrator.Task task, String userId) {
        if (!safetyValidationEnabled) {
            return true;
        }
        
        try {
            // Check task type safety
            if (!isTaskTypeSafe(task.getType())) {
                recordSafetyViolation(userId, "UNSAFE_TASK_TYPE", "Task type is not safe", task.getType());
                return false;
            }
            
            // Check task content safety
            if (task.getDescription() != null && containsHarmfulContent(task.getDescription())) {
                recordSafetyViolation(userId, "HARMFUL_TASK_CONTENT", "Task contains harmful content", task.getDescription());
                return false;
            }
            
            // Check for dangerous operations
            if (containsDangerousOperations(task)) {
                recordSafetyViolation(userId, "DANGEROUS_OPERATION", "Task contains dangerous operations", task.getType());
                return false;
            }
            
            // Check user safety score
            SafetyScore score = getUserSafetyScore(userId);
            if (score != null && score.getScore() < 0.5) {
                recordSafetyViolation(userId, "LOW_SAFETY_SCORE", "User has low safety score", String.valueOf(score.getScore()));
                return false;
            }
            
            return true;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Task safety validation failed", e);
            return false;
        }
    }
    
    /**
     * Validate file path for safety
     */
    public boolean validateFilePath(String filePath, String operation) {
        if (!safetyValidationEnabled) {
            return true;
        }
        
        try {
            // Check for path traversal
            if (containsPathTraversal(filePath)) {
                logger.warning("Path traversal detected: " + filePath);
                return false;
            }
            
            // Check for system file access
            if (isSystemFile(filePath)) {
                logger.warning("System file access attempt: " + filePath);
                return false;
            }
            
            // Check for dangerous file extensions
            if (hasDangerousExtension(filePath)) {
                logger.warning("Dangerous file extension: " + filePath);
                return false;
            }
            
            // Check operation safety
            if (!isOperationSafe(operation, filePath)) {
                logger.warning("Unsafe operation: " + operation + " on " + filePath);
                return false;
            }
            
            return true;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "File path validation failed", e);
            return false;
        }
    }
    
    /**
     * Validate permission grant for safety
     */
    public boolean validatePermissionGrant(String toolName, Capability capability, String grantedBy) {
        if (!safetyValidationEnabled) {
            return true;
        }
        
        try {
            // Check if capability is safe to grant
            if (!isCapabilitySafe(capability)) {
                logger.warning("Unsafe capability grant attempt: " + capability);
                return false;
            }
            
            // Check if tool is safe
            if (!isToolSafe(toolName)) {
                logger.warning("Unsafe tool permission grant: " + toolName);
                return false;
            }
            
            // Check if granter is authorized
            if (!isAuthorizedGranter(grantedBy, capability)) {
                logger.warning("Unauthorized permission grant attempt by: " + grantedBy);
                return false;
            }
            
            return true;
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Permission grant validation failed", e);
            return false;
        }
    }
    
    /**
     * Request human approval for sensitive operations
     */
    public String requestHumanApproval(String operation, String reason, String userId) {
        if (!humanApprovalRequired) {
            return "APPROVED"; // Auto-approve if not required
        }
        
        String approvalId = UUID.randomUUID().toString();
        ApprovalRequest request = new ApprovalRequest(approvalId, operation, reason, userId, Instant.now());
        pendingApprovals.put(approvalId, request);
        
        logger.info("Human approval requested: " + operation + " for user: " + userId);
        return approvalId;
    }
    
    /**
     * Process human approval
     */
    public boolean processHumanApproval(String approvalId, boolean approved, String approver) {
        ApprovalRequest request = pendingApprovals.remove(approvalId);
        if (request != null) {
            request.setApproved(approved);
            request.setApprover(approver);
            request.setApprovedAt(Instant.now());
            
            logger.info("Human approval processed: " + approvalId + " - " + (approved ? "APPROVED" : "DENIED"));
            return approved;
        }
        
        return false;
    }
    
    /**
     * Get user safety score
     */
    public SafetyScore getUserSafetyScore(String userId) {
        return userSafetyScores.get(userId);
    }
    
    /**
     * Get safety violation history
     */
    public List<SafetyViolation> getSafetyViolationHistory(String userId) {
        return violationHistory.getOrDefault(userId, new ArrayList<>());
    }
    
    /**
     * Get pending approvals
     */
    public List<ApprovalRequest> getPendingApprovals() {
        return new ArrayList<>(pendingApprovals.values());
    }
    
    /**
     * Initialize harmful content patterns
     */
    private void initializeHarmfulPatterns() {
        // Violence patterns
        harmfulPatterns.put("VIOLENCE", Arrays.asList(
            Pattern.compile("(?i).*(kill|murder|assassinate|destroy|harm|hurt|violence|weapon|gun|knife|bomb).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(attack|assault|fight|war|battle|combat).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Hate speech patterns
        harmfulPatterns.put("HATE_SPEECH", Arrays.asList(
            Pattern.compile("(?i).*(hate|racist|sexist|discriminat|prejudice|bigot).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(nazi|fascist|supremacist|extremist).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Self-harm patterns
        harmfulPatterns.put("SELF_HARM", Arrays.asList(
            Pattern.compile("(?i).*(suicide|self-harm|cutting|overdose|poison).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(end my life|kill myself|hurt myself).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Illegal activities
        harmfulPatterns.put("ILLEGAL", Arrays.asList(
            Pattern.compile("(?i).*(illegal|crime|criminal|fraud|scam|theft|robbery).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(drug|narcotic|traffic|smuggle|counterfeit).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Malware patterns
        harmfulPatterns.put("MALWARE", Arrays.asList(
            Pattern.compile("(?i).*(virus|malware|trojan|worm|backdoor|keylogger).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(exploit|hack|breach|intrusion|unauthorized).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Privacy violations
        harmfulPatterns.put("PRIVACY", Arrays.asList(
            Pattern.compile("(?i).*(spy|surveillance|track|monitor|eavesdrop|stalking).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(personal data|private information|confidential).*", Pattern.CASE_INSENSITIVE)
        ));
    }
    
    /**
     * Initialize ethical violation patterns
     */
    private void initializeEthicalViolationPatterns() {
        // Deception patterns
        ethicalViolationPatterns.put("DECEPTION", Arrays.asList(
            Pattern.compile("(?i).*(lie|deceive|mislead|false|fake|fraudulent).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(manipulate|trick|cheat|scam|con).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Bias patterns
        ethicalViolationPatterns.put("BIAS", Arrays.asList(
            Pattern.compile("(?i).*(bias|prejudice|discriminat|unfair|unequal).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(stereotype|generalize|assume|judge).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Manipulation patterns
        ethicalViolationPatterns.put("MANIPULATION", Arrays.asList(
            Pattern.compile("(?i).*(manipulate|influence|persuade|convince|coerce).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(brainwash|indoctrinate|propaganda).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Exploitation patterns
        ethicalViolationPatterns.put("EXPLOITATION", Arrays.asList(
            Pattern.compile("(?i).*(exploit|abuse|misuse|take advantage).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(vulnerable|weak|helpless|defenseless).*", Pattern.CASE_INSENSITIVE)
        ));
    }
    
    /**
     * Initialize safety rules
     */
    private void initializeSafetyRules() {
        // Rule 1: No system file access
        safetyRules.put("NO_SYSTEM_FILES", new SafetyRule("NO_SYSTEM_FILES", 
            "Prevent access to system files", SafetyRule.Severity.HIGH));
        
        // Rule 2: No network access to untrusted domains
        safetyRules.put("NO_UNTRUSTED_NETWORK", new SafetyRule("NO_UNTRUSTED_NETWORK", 
            "Prevent network access to untrusted domains", SafetyRule.Severity.MEDIUM));
        
        // Rule 3: No execution of unknown code
        safetyRules.put("NO_UNKNOWN_CODE", new SafetyRule("NO_UNKNOWN_CODE", 
            "Prevent execution of unknown or untrusted code", SafetyRule.Severity.HIGH));
        
        // Rule 4: No privilege escalation
        safetyRules.put("NO_PRIVILEGE_ESCALATION", new SafetyRule("NO_PRIVILEGE_ESCALATION", 
            "Prevent privilege escalation attempts", SafetyRule.Severity.CRITICAL));
        
        // Rule 5: No data exfiltration
        safetyRules.put("NO_DATA_EXFILTRATION", new SafetyRule("NO_DATA_EXFILTRATION", 
            "Prevent unauthorized data exfiltration", SafetyRule.Severity.HIGH));
    }
    
    /**
     * Check if content contains harmful patterns
     */
    private boolean containsHarmfulContent(String content) {
        for (Map.Entry<String, List<Pattern>> entry : harmfulPatterns.entrySet()) {
            List<Pattern> patterns = entry.getValue();
            for (Pattern pattern : patterns) {
                if (pattern.matcher(content).matches()) {
                    return true;
                }
            }
        }
        return false;
    }
    
    /**
     * Check if content violates ethical constraints
     */
    private boolean violatesEthicalConstraints(String content) {
        for (Map.Entry<String, List<Pattern>> entry : ethicalViolationPatterns.entrySet()) {
            List<Pattern> patterns = entry.getValue();
            for (Pattern pattern : patterns) {
                if (pattern.matcher(content).matches()) {
                    return true;
                }
            }
        }
        return false;
    }
    
    /**
     * Check if request complies with safety rules
     */
    private boolean compliesWithSafetyRules(String request, String userId) {
        for (SafetyRule rule : safetyRules.values()) {
            if (!rule.isCompliant(request, userId)) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * Check if task type is safe
     */
    private boolean isTaskTypeSafe(String taskType) {
        String[] unsafeTaskTypes = {"system_command", "privilege_escalation", "data_exfiltration"};
        return !Arrays.asList(unsafeTaskTypes).contains(taskType);
    }
    
    /**
     * Check if task contains dangerous operations
     */
    private boolean containsDangerousOperations(AgenticOrchestrator.Task task) {
        String[] dangerousOperations = {"rm -rf", "format", "delete all", "wipe", "destroy"};
        String description = task.getDescription().toLowerCase();
        
        for (String operation : dangerousOperations) {
            if (description.contains(operation)) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Check if file path contains path traversal
     */
    private boolean containsPathTraversal(String filePath) {
        return filePath.contains("../") || filePath.contains("..\\") || 
               filePath.contains("%2e%2e%2f") || filePath.contains("%2e%2e%5c");
    }
    
    /**
     * Check if file is a system file
     */
    private boolean isSystemFile(String filePath) {
        String[] systemPaths = {"/etc/", "/sys/", "/proc/", "/dev/", "C:\\Windows\\", "C:\\System32\\"};
        for (String path : systemPaths) {
            if (filePath.startsWith(path)) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Check if file has dangerous extension
     */
    private boolean hasDangerousExtension(String filePath) {
        String[] dangerousExtensions = {".exe", ".bat", ".cmd", ".scr", ".pif", ".com"};
        String lowerPath = filePath.toLowerCase();
        for (String ext : dangerousExtensions) {
            if (lowerPath.endsWith(ext)) {
                return true;
            }
        }
        return false;
    }
    
    /**
     * Check if operation is safe
     */
    private boolean isOperationSafe(String operation, String filePath) {
        if ("delete".equals(operation) && isSystemFile(filePath)) {
            return false;
        }
        if ("write".equals(operation) && isSystemFile(filePath)) {
            return false;
        }
        return true;
    }
    
    /**
     * Check if capability is safe
     */
    private boolean isCapabilitySafe(Capability capability) {
        return !capability.isHighRisk();
    }
    
    /**
     * Check if tool is safe
     */
    private boolean isToolSafe(String toolName) {
        String[] unsafeTools = {"system", "admin", "root", "privilege", "escalate"};
        String lowerName = toolName.toLowerCase();
        for (String tool : unsafeTools) {
            if (lowerName.contains(tool)) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * Check if granter is authorized
     */
    private boolean isAuthorizedGranter(String granter, Capability capability) {
        // In a real implementation, this would check user roles and permissions
        return "admin".equals(granter) || "system".equals(granter);
    }
    
    /**
     * Record a safety violation
     */
    private void recordSafetyViolation(String userId, String violationType, String reason, String context) {
        SafetyViolation violation = new SafetyViolation(
            UUID.randomUUID().toString(), userId, violationType, reason, context, Instant.now());
        
        violationHistory.computeIfAbsent(userId, k -> new ArrayList<>()).add(violation);
        
        // Update user safety score
        updateUserSafetyScore(userId, false);
        
        logger.warning("Safety violation recorded: " + violationType + " for user: " + userId);
    }
    
    /**
     * Update user safety score
     */
    private void updateUserSafetyScore(String userId, boolean positive) {
        SafetyScore score = userSafetyScores.computeIfAbsent(userId, k -> new SafetyScore(userId, 1.0));
        
        if (positive) {
            score.increaseScore(0.01); // Small positive increment
        } else {
            score.decreaseScore(0.1); // Larger negative decrement
        }
        
        // Ensure score stays within bounds
        score.setScore(Math.max(0.0, Math.min(1.0, score.getScore())));
    }
    
    // Configuration methods
    
    public void setEthicalConstraintsEnabled(boolean enabled) {
        this.ethicalConstraintsEnabled = enabled;
    }
    
    public void setContentFilteringEnabled(boolean enabled) {
        this.contentFilteringEnabled = enabled;
    }
    
    public void setSafetyValidationEnabled(boolean enabled) {
        this.safetyValidationEnabled = enabled;
    }
    
    public void setHumanApprovalRequired(boolean required) {
        this.humanApprovalRequired = required;
    }
    
    /**
     * Safety rule class
     */
    public static class SafetyRule {
        public enum Severity { LOW, MEDIUM, HIGH, CRITICAL }
        
        private final String id;
        private final String description;
        private final Severity severity;
        
        public SafetyRule(String id, String description, Severity severity) {
            this.id = id;
            this.description = description;
            this.severity = severity;
        }
        
        public boolean isCompliant(String request, String userId) {
            // In a real implementation, this would contain specific compliance logic
            return true;
        }
        
        // Getters
        public String getId() { return id; }
        public String getDescription() { return description; }
        public Severity getSeverity() { return severity; }
    }
    
    /**
     * Safety violation class
     */
    public static class SafetyViolation {
        private final String id;
        private final String userId;
        private final String violationType;
        private final String reason;
        private final String context;
        private final Instant timestamp;
        
        public SafetyViolation(String id, String userId, String violationType, String reason, String context, Instant timestamp) {
            this.id = id;
            this.userId = userId;
            this.violationType = violationType;
            this.reason = reason;
            this.context = context;
            this.timestamp = timestamp;
        }
        
        // Getters
        public String getId() { return id; }
        public String getUserId() { return userId; }
        public String getViolationType() { return violationType; }
        public String getReason() { return reason; }
        public String getContext() { return context; }
        public Instant getTimestamp() { return timestamp; }
    }
    
    /**
     * Safety score class
     */
    public static class SafetyScore {
        private final String userId;
        private double score;
        private Instant lastUpdated;
        
        public SafetyScore(String userId, double initialScore) {
            this.userId = userId;
            this.score = initialScore;
            this.lastUpdated = Instant.now();
        }
        
        public void increaseScore(double amount) {
            this.score += amount;
            this.lastUpdated = Instant.now();
        }
        
        public void decreaseScore(double amount) {
            this.score -= amount;
            this.lastUpdated = Instant.now();
        }
        
        // Getters and setters
        public String getUserId() { return userId; }
        public double getScore() { return score; }
        public void setScore(double score) { this.score = score; }
        public Instant getLastUpdated() { return lastUpdated; }
    }
    
    /**
     * Approval request class
     */
    public static class ApprovalRequest {
        private final String id;
        private final String operation;
        private final String reason;
        private final String userId;
        private final Instant requestedAt;
        private boolean approved;
        private String approver;
        private Instant approvedAt;
        
        public ApprovalRequest(String id, String operation, String reason, String userId, Instant requestedAt) {
            this.id = id;
            this.operation = operation;
            this.reason = reason;
            this.userId = userId;
            this.requestedAt = requestedAt;
        }
        
        // Getters and setters
        public String getId() { return id; }
        public String getOperation() { return operation; }
        public String getReason() { return reason; }
        public String getUserId() { return userId; }
        public Instant getRequestedAt() { return requestedAt; }
        public boolean isApproved() { return approved; }
        public void setApproved(boolean approved) { this.approved = approved; }
        public String getApprover() { return approver; }
        public void setApprover(String approver) { this.approver = approver; }
        public Instant getApprovedAt() { return approvedAt; }
        public void setApprovedAt(Instant approvedAt) { this.approvedAt = approvedAt; }
    }
}
