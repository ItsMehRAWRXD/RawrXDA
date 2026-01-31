// SecurityAnalyzer.java - Comprehensive security analysis with vulnerability detection

import com.aicli.plugins.*;
import java.util.*;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.logging.Logger;
import java.util.logging.Level;
import java.time.Instant;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Security analyzer that performs comprehensive vulnerability detection and security analysis
 * for code, queries, tools, and user requests.
 */
public class SecurityAnalyzer {
    private static final Logger logger = Logger.getLogger(SecurityAnalyzer.class.getName());
    
    // Configuration flags
    private boolean vulnerabilityDetectionEnabled = true;
    private boolean codeAnalysisEnabled = true;
    private boolean queryAnalysisEnabled = true;
    private boolean toolAnalysisEnabled = true;
    
    // Vulnerability detection patterns
    private final Map<String, List<Pattern>> vulnerabilityPatterns = new ConcurrentHashMap<>();
    private final Map<String, SecurityThreat> threatDatabase = new ConcurrentHashMap<>();
    
    // Analysis results cache
    private final Map<String, SecurityAnalysisResult> analysisCache = new ConcurrentHashMap<>();
    
    public SecurityAnalyzer() {
        initializeVulnerabilityPatterns();
        initializeThreatDatabase();
    }
    
    /**
     * Analyze a user request for security threats
     */
    public SecurityAnalysisResult analyzeRequest(String request, String userId) {
        String cacheKey = "request:" + request.hashCode() + ":" + userId;
        
        // Check cache first
        SecurityAnalysisResult cached = analysisCache.get(cacheKey);
        if (cached != null && !cached.isExpired()) {
            return cached;
        }
        
        SecurityAnalysisResult result = new SecurityAnalysisResult();
        result.setRequestId(UUID.randomUUID().toString());
        result.setUserId(userId);
        result.setAnalyzedAt(Instant.now());
        
        try {
            // Analyze for injection attacks
            analyzeInjectionAttacks(request, result);
            
            // Analyze for privilege escalation attempts
            analyzePrivilegeEscalation(request, result);
            
            // Analyze for data exfiltration attempts
            analyzeDataExfiltration(request, result);
            
            // Analyze for malicious intent
            analyzeMaliciousIntent(request, result);
            
            // Analyze for ethical violations
            analyzeEthicalViolations(request, result);
            
            // Determine overall safety
            result.setSafe(!result.hasThreats());
            
            // Cache result
            analysisCache.put(cacheKey, result);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Security analysis failed for request", e);
            result.setSafe(false);
            result.addThreat(new SecurityThreat("ANALYSIS_ERROR", "Security analysis failed", "HIGH"));
        }
        
        return result;
    }
    
    /**
     * Analyze code for security vulnerabilities
     */
    public SecurityAnalysisResult analyzeCode(String code) {
        String cacheKey = "code:" + code.hashCode();
        
        // Check cache first
        SecurityAnalysisResult cached = analysisCache.get(cacheKey);
        if (cached != null && !cached.isExpired()) {
            return cached;
        }
        
        SecurityAnalysisResult result = new SecurityAnalysisResult();
        result.setRequestId(UUID.randomUUID().toString());
        result.setAnalyzedAt(Instant.now());
        
        try {
            // Analyze for code injection vulnerabilities
            analyzeCodeInjection(code, result);
            
            // Analyze for buffer overflow vulnerabilities
            analyzeBufferOverflow(code, result);
            
            // Analyze for memory corruption vulnerabilities
            analyzeMemoryCorruption(code, result);
            
            // Analyze for authentication bypasses
            analyzeAuthenticationBypass(code, result);
            
            // Analyze for authorization flaws
            analyzeAuthorizationFlaws(code, result);
            
            // Analyze for cryptographic weaknesses
            analyzeCryptographicWeaknesses(code, result);
            
            // Analyze for input validation issues
            analyzeInputValidation(code, result);
            
            // Analyze for error handling vulnerabilities
            analyzeErrorHandling(code, result);
            
            // Determine overall safety
            result.setSafe(!result.hasThreats());
            
            // Cache result
            analysisCache.put(cacheKey, result);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Code analysis failed", e);
            result.setSafe(false);
            result.addThreat(new SecurityThreat("ANALYSIS_ERROR", "Code analysis failed", "HIGH"));
        }
        
        return result;
    }
    
    /**
     * Analyze a query for security threats
     */
    public SecurityAnalysisResult analyzeQuery(String query) {
        String cacheKey = "query:" + query.hashCode();
        
        // Check cache first
        SecurityAnalysisResult cached = analysisCache.get(cacheKey);
        if (cached != null && !cached.isExpired()) {
            return cached;
        }
        
        SecurityAnalysisResult result = new SecurityAnalysisResult();
        result.setRequestId(UUID.randomUUID().toString());
        result.setAnalyzedAt(Instant.now());
        
        try {
            // Analyze for SQL injection
            analyzeSQLInjection(query, result);
            
            // Analyze for NoSQL injection
            analyzeNoSQLInjection(query, result);
            
            // Analyze for command injection
            analyzeCommandInjection(query, result);
            
            // Analyze for LDAP injection
            analyzeLDAPInjection(query, result);
            
            // Analyze for XPath injection
            analyzeXPathInjection(query, result);
            
            // Analyze for template injection
            analyzeTemplateInjection(query, result);
            
            // Determine overall safety
            result.setSafe(!result.hasThreats());
            
            // Cache result
            analysisCache.put(cacheKey, result);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Query analysis failed", e);
            result.setSafe(false);
            result.addThreat(new SecurityThreat("ANALYSIS_ERROR", "Query analysis failed", "HIGH"));
        }
        
        return result;
    }
    
    /**
     * Analyze a tool for security issues
     */
    public SecurityAnalysisResult analyzeTool(Tool tool) {
        String cacheKey = "tool:" + tool.getName().hashCode();
        
        // Check cache first
        SecurityAnalysisResult cached = analysisCache.get(cacheKey);
        if (cached != null && !cached.isExpired()) {
            return cached;
        }
        
        SecurityAnalysisResult result = new SecurityAnalysisResult();
        result.setRequestId(UUID.randomUUID().toString());
        result.setAnalyzedAt(Instant.now());
        
        try {
            // Analyze tool capabilities
            analyzeToolCapabilities(tool, result);
            
            // Analyze tool configuration
            analyzeToolConfiguration(tool, result);
            
            // Analyze tool dependencies
            analyzeToolDependencies(tool, result);
            
            // Analyze tool metadata
            analyzeToolMetadata(tool, result);
            
            // Determine overall safety
            result.setSafe(!result.hasThreats());
            
            // Cache result
            analysisCache.put(cacheKey, result);
            
        } catch (Exception e) {
            logger.log(Level.WARNING, "Tool analysis failed", e);
            result.setSafe(false);
            result.addThreat(new SecurityThreat("ANALYSIS_ERROR", "Tool analysis failed", "HIGH"));
        }
        
        return result;
    }
    
    /**
     * Initialize vulnerability detection patterns
     */
    private void initializeVulnerabilityPatterns() {
        // SQL Injection patterns
        vulnerabilityPatterns.put("SQL_INJECTION", Arrays.asList(
            Pattern.compile("(?i).*('|(\\-\\-)|(;)|(\\|\\|)|(\\+)).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(union|select|insert|update|delete|drop|create|alter).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile("(?i).*(or|and)\\s+\\d+\\s*=\\s*\\d+.*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Command Injection patterns
        vulnerabilityPatterns.put("COMMAND_INJECTION", Arrays.asList(
            Pattern.compile(".*[;&|`$(){}[\\]<>].*"),
            Pattern.compile("(?i).*(cmd|command|exec|system|shell).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*\\$\\{.*\\}.*"),
            Pattern.compile(".*`.*`.*")
        ));
        
        // XSS patterns
        vulnerabilityPatterns.put("XSS", Arrays.asList(
            Pattern.compile(".*<script.*>.*</script>.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*javascript:.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*on\\w+\\s*=.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*<iframe.*>.*</iframe>.*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Path Traversal patterns
        vulnerabilityPatterns.put("PATH_TRAVERSAL", Arrays.asList(
            Pattern.compile(".*\\.\\./.*"),
            Pattern.compile(".*\\.\\.\\\\\\.*"),
            Pattern.compile(".*%2e%2e%2f.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*%2e%2e%5c.*", Pattern.CASE_INSENSITIVE)
        ));
        
        // LDAP Injection patterns
        vulnerabilityPatterns.put("LDAP_INJECTION", Arrays.asList(
            Pattern.compile(".*[()=*!&|].*"),
            Pattern.compile("(?i).*(cn|ou|dc|objectclass).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // NoSQL Injection patterns
        vulnerabilityPatterns.put("NOSQL_INJECTION", Arrays.asList(
            Pattern.compile(".*\\$\\w+.*"),
            Pattern.compile(".*\\{\\s*\\$.*\\}.*"),
            Pattern.compile("(?i).*(where|find|query).*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Template Injection patterns
        vulnerabilityPatterns.put("TEMPLATE_INJECTION", Arrays.asList(
            Pattern.compile(".*\\{\\{.*\\}\\}.*"),
            Pattern.compile(".*<%%.*%%>.*"),
            Pattern.compile(".*\\$\\{.*\\}.*"),
            Pattern.compile(".*#\\{.*\\}.*")
        ));
        
        // Code Injection patterns
        vulnerabilityPatterns.put("CODE_INJECTION", Arrays.asList(
            Pattern.compile("(?i).*(eval|exec|system|shell_exec|passthru).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*Function\\(.*\\).*"),
            Pattern.compile(".*setTimeout\\(.*\\).*"),
            Pattern.compile(".*setInterval\\(.*\\).*")
        ));
        
        // Buffer Overflow patterns
        vulnerabilityPatterns.put("BUFFER_OVERFLOW", Arrays.asList(
            Pattern.compile(".*strcpy|strcat|sprintf|gets.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*memcpy|memmove|memset.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*alloca|malloc.*", Pattern.CASE_INSENSITIVE)
        ));
        
        // Authentication Bypass patterns
        vulnerabilityPatterns.put("AUTH_BYPASS", Arrays.asList(
            Pattern.compile("(?i).*(admin|administrator|root|sa|guest).*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*password\\s*=\\s*['\"]?['\"]?.*", Pattern.CASE_INSENSITIVE),
            Pattern.compile(".*auth\\s*=\\s*false.*", Pattern.CASE_INSENSITIVE)
        ));
    }
    
    /**
     * Initialize threat database
     */
    private void initializeThreatDatabase() {
        threatDatabase.put("SQL_INJECTION", new SecurityThreat("SQL_INJECTION", 
            "SQL injection vulnerability detected", "HIGH"));
        threatDatabase.put("COMMAND_INJECTION", new SecurityThreat("COMMAND_INJECTION", 
            "Command injection vulnerability detected", "CRITICAL"));
        threatDatabase.put("XSS", new SecurityThreat("XSS", 
            "Cross-site scripting vulnerability detected", "MEDIUM"));
        threatDatabase.put("PATH_TRAVERSAL", new SecurityThreat("PATH_TRAVERSAL", 
            "Path traversal vulnerability detected", "HIGH"));
        threatDatabase.put("LDAP_INJECTION", new SecurityThreat("LDAP_INJECTION", 
            "LDAP injection vulnerability detected", "HIGH"));
        threatDatabase.put("NOSQL_INJECTION", new SecurityThreat("NOSQL_INJECTION", 
            "NoSQL injection vulnerability detected", "HIGH"));
        threatDatabase.put("TEMPLATE_INJECTION", new SecurityThreat("TEMPLATE_INJECTION", 
            "Template injection vulnerability detected", "HIGH"));
        threatDatabase.put("CODE_INJECTION", new SecurityThreat("CODE_INJECTION", 
            "Code injection vulnerability detected", "CRITICAL"));
        threatDatabase.put("BUFFER_OVERFLOW", new SecurityThreat("BUFFER_OVERFLOW", 
            "Buffer overflow vulnerability detected", "CRITICAL"));
        threatDatabase.put("AUTH_BYPASS", new SecurityThreat("AUTH_BYPASS", 
            "Authentication bypass vulnerability detected", "HIGH"));
    }
    
    // Analysis methods for different vulnerability types
    
    private void analyzeInjectionAttacks(String input, SecurityAnalysisResult result) {
        for (Map.Entry<String, List<Pattern>> entry : vulnerabilityPatterns.entrySet()) {
            String threatType = entry.getKey();
            List<Pattern> patterns = entry.getValue();
            
            for (Pattern pattern : patterns) {
                if (pattern.matcher(input).matches()) {
                    SecurityThreat threat = threatDatabase.get(threatType);
                    if (threat != null) {
                        result.addThreat(threat);
                    }
                }
            }
        }
    }
    
    private void analyzePrivilegeEscalation(String input, SecurityAnalysisResult result) {
        String[] privilegePatterns = {
            "sudo", "su ", "runas", "elevate", "admin", "root", "administrator"
        };
        
        for (String pattern : privilegePatterns) {
            if (input.toLowerCase().contains(pattern)) {
                result.addThreat(new SecurityThreat("PRIVILEGE_ESCALATION", 
                    "Potential privilege escalation attempt", "HIGH"));
                break;
            }
        }
    }
    
    private void analyzeDataExfiltration(String input, SecurityAnalysisResult result) {
        String[] exfiltrationPatterns = {
            "download", "export", "backup", "copy", "transfer", "send", "upload"
        };
        
        for (String pattern : exfiltrationPatterns) {
            if (input.toLowerCase().contains(pattern)) {
                result.addThreat(new SecurityThreat("DATA_EXFILTRATION", 
                    "Potential data exfiltration attempt", "MEDIUM"));
                break;
            }
        }
    }
    
    private void analyzeMaliciousIntent(String input, SecurityAnalysisResult result) {
        String[] maliciousPatterns = {
            "hack", "exploit", "bypass", "crack", "break", "destroy", "delete all"
        };
        
        for (String pattern : maliciousPatterns) {
            if (input.toLowerCase().contains(pattern)) {
                result.addThreat(new SecurityThreat("MALICIOUS_INTENT", 
                    "Potential malicious intent detected", "HIGH"));
                break;
            }
        }
    }
    
    private void analyzeEthicalViolations(String input, SecurityAnalysisResult result) {
        String[] ethicalViolationPatterns = {
            "harm", "hurt", "kill", "destroy", "illegal", "unethical", "immoral"
        };
        
        for (String pattern : ethicalViolationPatterns) {
            if (input.toLowerCase().contains(pattern)) {
                result.addThreat(new SecurityThreat("ETHICAL_VIOLATION", 
                    "Potential ethical violation detected", "MEDIUM"));
                break;
            }
        }
    }
    
    private void analyzeCodeInjection(String code, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("CODE_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(code).matches()) {
                    result.addThreat(threatDatabase.get("CODE_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeBufferOverflow(String code, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("BUFFER_OVERFLOW");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(code).matches()) {
                    result.addThreat(threatDatabase.get("BUFFER_OVERFLOW"));
                    break;
                }
            }
        }
    }
    
    private void analyzeMemoryCorruption(String code, SecurityAnalysisResult result) {
        String[] memoryPatterns = {
            "free\\(.*\\)", "delete\\s+.*", "unlink\\(.*\\)", "dereference"
        };
        
        for (String pattern : memoryPatterns) {
            if (Pattern.compile(pattern, Pattern.CASE_INSENSITIVE).matcher(code).matches()) {
                result.addThreat(new SecurityThreat("MEMORY_CORRUPTION", 
                    "Potential memory corruption vulnerability", "CRITICAL"));
                break;
            }
        }
    }
    
    private void analyzeAuthenticationBypass(String code, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("AUTH_BYPASS");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(code).matches()) {
                    result.addThreat(threatDatabase.get("AUTH_BYPASS"));
                    break;
                }
            }
        }
    }
    
    private void analyzeAuthorizationFlaws(String code, SecurityAnalysisResult result) {
        String[] authzPatterns = {
            "if\\s*\\(.*user.*\\)", "checkPermission", "hasRole", "isAuthorized"
        };
        
        for (String pattern : authzPatterns) {
            if (Pattern.compile(pattern, Pattern.CASE_INSENSITIVE).matcher(code).matches()) {
                result.addThreat(new SecurityThreat("AUTHORIZATION_FLAW", 
                    "Potential authorization flaw detected", "MEDIUM"));
                break;
            }
        }
    }
    
    private void analyzeCryptographicWeaknesses(String code, SecurityAnalysisResult result) {
        String[] cryptoPatterns = {
            "md5", "sha1", "des", "rc4", "md4", "md2"
        };
        
        for (String pattern : cryptoPatterns) {
            if (code.toLowerCase().contains(pattern)) {
                result.addThreat(new SecurityThreat("CRYPTOGRAPHIC_WEAKNESS", 
                    "Weak cryptographic algorithm detected", "MEDIUM"));
                break;
            }
        }
    }
    
    private void analyzeInputValidation(String code, SecurityAnalysisResult result) {
        String[] validationPatterns = {
            "trim\\(.*\\)", "strip\\(.*\\)", "sanitize\\(.*\\)", "validate\\(.*\\)"
        };
        
        boolean hasValidation = false;
        for (String pattern : validationPatterns) {
            if (Pattern.compile(pattern, Pattern.CASE_INSENSITIVE).matcher(code).matches()) {
                hasValidation = true;
                break;
            }
        }
        
        if (!hasValidation) {
            result.addThreat(new SecurityThreat("INPUT_VALIDATION", 
                "Missing input validation", "LOW"));
        }
    }
    
    private void analyzeErrorHandling(String code, SecurityAnalysisResult result) {
        String[] errorPatterns = {
            "try\\s*\\{", "catch\\s*\\(", "finally\\s*\\{", "throw\\s+"
        };
        
        boolean hasErrorHandling = false;
        for (String pattern : errorPatterns) {
            if (Pattern.compile(pattern, Pattern.CASE_INSENSITIVE).matcher(code).matches()) {
                hasErrorHandling = true;
                break;
            }
        }
        
        if (!hasErrorHandling) {
            result.addThreat(new SecurityThreat("ERROR_HANDLING", 
                "Missing error handling", "LOW"));
        }
    }
    
    private void analyzeSQLInjection(String query, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("SQL_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(query).matches()) {
                    result.addThreat(threatDatabase.get("SQL_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeNoSQLInjection(String query, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("NOSQL_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(query).matches()) {
                    result.addThreat(threatDatabase.get("NOSQL_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeCommandInjection(String query, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("COMMAND_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(query).matches()) {
                    result.addThreat(threatDatabase.get("COMMAND_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeLDAPInjection(String query, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("LDAP_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(query).matches()) {
                    result.addThreat(threatDatabase.get("LDAP_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeXPathInjection(String query, SecurityAnalysisResult result) {
        String[] xpathPatterns = {
            "//", "/", "\\*", "\\[", "\\]", "\\(", "\\)"
        };
        
        for (String pattern : xpathPatterns) {
            if (query.contains(pattern)) {
                result.addThreat(new SecurityThreat("XPATH_INJECTION", 
                    "Potential XPath injection vulnerability", "MEDIUM"));
                break;
            }
        }
    }
    
    private void analyzeTemplateInjection(String query, SecurityAnalysisResult result) {
        List<Pattern> patterns = vulnerabilityPatterns.get("TEMPLATE_INJECTION");
        if (patterns != null) {
            for (Pattern pattern : patterns) {
                if (pattern.matcher(query).matches()) {
                    result.addThreat(threatDatabase.get("TEMPLATE_INJECTION"));
                    break;
                }
            }
        }
    }
    
    private void analyzeToolCapabilities(Tool tool, SecurityAnalysisResult result) {
        Set<Capability> capabilities = tool.getRequiredCapabilities();
        
        for (Capability capability : capabilities) {
            if (capability.isHighRisk()) {
                result.addThreat(new SecurityThreat("HIGH_RISK_CAPABILITY", 
                    "Tool requires high-risk capability: " + capability, "HIGH"));
            }
        }
    }
    
    private void analyzeToolConfiguration(Tool tool, SecurityAnalysisResult result) {
        Map<String, Object> config = tool.getConfiguration();
        
        if (config.containsKey("unsafe") && Boolean.TRUE.equals(config.get("unsafe"))) {
            result.addThreat(new SecurityThreat("UNSAFE_CONFIG", 
                "Tool has unsafe configuration", "HIGH"));
        }
    }
    
    private void analyzeToolDependencies(Tool tool, SecurityAnalysisResult result) {
        Set<String> dependencies = tool.getDependencies();
        
        for (String dependency : dependencies) {
            if (dependency.contains("untrusted") || dependency.contains("malicious")) {
                result.addThreat(new SecurityThreat("UNTRUSTED_DEPENDENCY", 
                    "Tool depends on untrusted library: " + dependency, "MEDIUM"));
            }
        }
    }
    
    private void analyzeToolMetadata(Tool tool, SecurityAnalysisResult result) {
        String description = tool.getDescription().toLowerCase();
        
        if (description.contains("experimental") || description.contains("beta")) {
            result.addThreat(new SecurityThreat("EXPERIMENTAL_TOOL", 
                "Tool is experimental or beta", "LOW"));
        }
    }
    
    // Configuration methods
    
    public void setVulnerabilityDetectionEnabled(boolean enabled) {
        this.vulnerabilityDetectionEnabled = enabled;
    }
    
    public void setCodeAnalysisEnabled(boolean enabled) {
        this.codeAnalysisEnabled = enabled;
    }
    
    public void setQueryAnalysisEnabled(boolean enabled) {
        this.queryAnalysisEnabled = enabled;
    }
    
    public void setToolAnalysisEnabled(boolean enabled) {
        this.toolAnalysisEnabled = enabled;
    }
    
    /**
     * Security threat definition
     */
    public static class SecurityThreat {
        private final String type;
        private final String description;
        private final String severity;
        
        public SecurityThreat(String type, String description, String severity) {
            this.type = type;
            this.description = description;
            this.severity = severity;
        }
        
        public String getType() { return type; }
        public String getDescription() { return description; }
        public String getSeverity() { return severity; }
        
        @Override
        public String toString() {
            return String.format("[%s] %s: %s", severity, type, description);
        }
    }
    
    /**
     * Security analysis result
     */
    public static class SecurityAnalysisResult {
        private String requestId;
        private String userId;
        private Instant analyzedAt;
        private boolean safe;
        private final List<SecurityThreat> threats = new ArrayList<>();
        private String reason;
        
        public void addThreat(SecurityThreat threat) {
            threats.add(threat);
        }
        
        public boolean hasThreats() {
            return !threats.isEmpty();
        }
        
        public boolean isExpired() {
            return analyzedAt.isBefore(Instant.now().minusSeconds(300)); // 5 minutes
        }
        
        // Getters and setters
        public String getRequestId() { return requestId; }
        public void setRequestId(String requestId) { this.requestId = requestId; }
        public String getUserId() { return userId; }
        public void setUserId(String userId) { this.userId = userId; }
        public Instant getAnalyzedAt() { return analyzedAt; }
        public void setAnalyzedAt(Instant analyzedAt) { this.analyzedAt = analyzedAt; }
        public boolean isSafe() { return safe; }
        public void setSafe(boolean safe) { this.safe = safe; }
        public List<SecurityThreat> getThreats() { return threats; }
        public String getReason() { return reason; }
        public void setReason(String reason) { this.reason = reason; }
    }
}
