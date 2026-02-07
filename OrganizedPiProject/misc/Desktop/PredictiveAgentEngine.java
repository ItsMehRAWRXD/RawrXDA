// PredictiveAgentEngine.java - Proactive AI agent suggestions based on coding context
package com.aicli;

import com.aicli.plugins.PluginManagerService;
import com.aicli.monitoring.TracerService;
import dev.langchain4j.model.chat.ChatLanguageModel;
import dev.langchain4j.model.openai.OpenAiChatModel;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.time.Instant;
import java.time.Duration;

/**
 * Predictive agent engine that proactively suggests actions based on current coding context.
 * Uses semantic code analysis, user behavior patterns, and project structure to anticipate
 * developer needs and provide intelligent suggestions.
 */
public class PredictiveAgentEngine {
    private final ChatLanguageModel model;
    private final CodebaseIndexer codebaseIndexer;
    private final Map<String, UserBehaviorProfile> userProfiles;
    private final Map<String, ProjectContext> projectContexts;
    private final ExecutorService executorService;
    private final TracerService tracerService;
    private final PluginManagerService pluginManager;
    
    // Context analysis components
    private final Map<String, List<CodePattern>> detectedPatterns;
    private final Map<String, List<PotentialIssue>> potentialIssues;
    private final Map<String, List<RefactoringOpportunity>> refactoringOpportunities;

    public PredictiveAgentEngine(String apiKey, Path projectRoot) throws IOException {
        this.model = OpenAiChatModel.builder()
                .apiKey(apiKey)
                .modelName("gpt-4o")
                .build();
        this.codebaseIndexer = new CodebaseIndexer();
        this.codebaseIndexer.indexCodebase(projectRoot);
        this.userProfiles = new ConcurrentHashMap<>();
        this.projectContexts = new ConcurrentHashMap<>();
        this.executorService = Executors.newFixedThreadPool(5);
        this.tracerService = new TracerService();
        this.pluginManager = new PluginManagerService(projectRoot);
        this.detectedPatterns = new ConcurrentHashMap<>();
        this.potentialIssues = new ConcurrentHashMap<>();
        this.refactoringOpportunities = new ConcurrentHashMap<>();
    }

    /**
     * Analyze current coding context and generate proactive suggestions
     */
    public List<ProactiveSuggestion> analyzeContext(String userId, String currentFile, 
                                                   int cursorLine, String recentChanges) {
        try {
            // Get user behavior profile
            UserBehaviorProfile profile = getUserProfile(userId);
            
            // Analyze current file context
            FileContext fileContext = analyzeFileContext(currentFile, cursorLine);
            
            // Detect patterns and potential issues
            List<CodePattern> patterns = detectCodePatterns(currentFile);
            List<PotentialIssue> issues = detectPotentialIssues(currentFile, recentChanges);
            List<RefactoringOpportunity> refactoring = detectRefactoringOpportunities(currentFile);
            
            // Generate proactive suggestions
            return generateProactiveSuggestions(profile, fileContext, patterns, issues, refactoring);
            
        } catch (Exception e) {
            System.err.println("Error in predictive analysis: " + e.getMessage());
            return Collections.emptyList();
        }
    }

    /**
     * Get or create user behavior profile
     */
    private UserBehaviorProfile getUserProfile(String userId) {
        return userProfiles.computeIfAbsent(userId, k -> new UserBehaviorProfile(userId));
    }

    /**
     * Analyze file context around cursor position
     */
    private FileContext analyzeFileContext(String filePath, int cursorLine) {
        try {
            List<CodebaseIndexer.CodeSegment> segments = codebaseIndexer.getSegmentsForFile(filePath);
            
            // Find segment containing cursor
            CodebaseIndexer.CodeSegment currentSegment = null;
            for (CodebaseIndexer.CodeSegment segment : segments) {
                if (cursorLine >= segment.getStartLine() && cursorLine <= segment.getEndLine()) {
                    currentSegment = segment;
                    break;
                }
            }
            
            // Get surrounding context
            List<CodebaseIndexer.CodeSegment> surroundingSegments = getSurroundingSegments(segments, cursorLine, 3);
            
            return new FileContext(filePath, cursorLine, currentSegment, surroundingSegments);
            
        } catch (Exception e) {
            System.err.println("Error analyzing file context: " + e.getMessage());
            return new FileContext(filePath, cursorLine, null, Collections.emptyList());
        }
    }

    /**
     * Detect code patterns in the current file
     */
    private List<CodePattern> detectCodePatterns(String filePath) {
        List<CodePattern> patterns = new ArrayList<>();
        
        try {
            List<CodebaseIndexer.CodeSegment> segments = codebaseIndexer.getSegmentsForFile(filePath);
            
            for (CodebaseIndexer.CodeSegment segment : segments) {
                String content = segment.getContent().toLowerCase();
                
                // Detect common patterns
                if (content.contains("try") && content.contains("catch")) {
                    patterns.add(new CodePattern("exception_handling", segment, "Exception handling pattern detected"));
                }
                
                if (content.contains("if") && content.contains("else")) {
                    patterns.add(new CodePattern("conditional_logic", segment, "Conditional logic pattern detected"));
                }
                
                if (content.contains("for") || content.contains("while")) {
                    patterns.add(new CodePattern("iteration", segment, "Iteration pattern detected"));
                }
                
                if (content.contains("class") && content.contains("extends")) {
                    patterns.add(new CodePattern("inheritance", segment, "Inheritance pattern detected"));
                }
                
                if (content.contains("interface") && content.contains("implements")) {
                    patterns.add(new CodePattern("interface_implementation", segment, "Interface implementation pattern detected"));
                }
            }
            
        } catch (Exception e) {
            System.err.println("Error detecting patterns: " + e.getMessage());
        }
        
        return patterns;
    }

    /**
     * Detect potential issues in the code
     */
    private List<PotentialIssue> detectPotentialIssues(String filePath, String recentChanges) {
        List<PotentialIssue> issues = new ArrayList<>();
        
        try {
            List<CodebaseIndexer.CodeSegment> segments = codebaseIndexer.getSegmentsForFile(filePath);
            
            for (CodebaseIndexer.CodeSegment segment : segments) {
                String content = segment.getContent();
                
                // Check for common issues
                if (content.contains("System.out.println")) {
                    issues.add(new PotentialIssue("debug_print", segment, 
                        "Consider using proper logging instead of System.out.println", "WARNING"));
                }
                
                if (content.contains("TODO") || content.contains("FIXME")) {
                    issues.add(new PotentialIssue("todo_comment", segment, 
                        "TODO/FIXME comment found - consider addressing", "INFO"));
                }
                
                if (content.contains("null") && content.contains("==")) {
                    issues.add(new PotentialIssue("null_comparison", segment, 
                        "Consider using Objects.equals() for null-safe comparison", "WARNING"));
                }
                
                if (content.contains("catch") && content.contains("Exception")) {
                    issues.add(new PotentialIssue("broad_exception", segment, 
                        "Consider catching specific exceptions instead of generic Exception", "WARNING"));
                }
            }
            
        } catch (Exception e) {
            System.err.println("Error detecting issues: " + e.getMessage());
        }
        
        return issues;
    }

    /**
     * Detect refactoring opportunities
     */
    private List<RefactoringOpportunity> detectRefactoringOpportunities(String filePath) {
        List<RefactoringOpportunity> opportunities = new ArrayList<>();
        
        try {
            List<CodebaseIndexer.CodeSegment> segments = codebaseIndexer.getSegmentsForFile(filePath);
            
            for (CodebaseIndexer.CodeSegment segment : segments) {
                String content = segment.getContent();
                
                // Detect long methods
                if (segment.getType().equals("method") && content.split("\n").length > 20) {
                    opportunities.add(new RefactoringOpportunity("extract_method", segment, 
                        "Method is quite long - consider extracting smaller methods", "MEDIUM"));
                }
                
                // Detect duplicate code
                if (content.contains("if (") && content.contains("else if (")) {
                    opportunities.add(new RefactoringOpportunity("simplify_conditionals", segment, 
                        "Consider using switch statement or strategy pattern", "LOW"));
                }
                
                // Detect magic numbers
                if (content.matches(".*\\b\\d{3,}\\b.*")) {
                    opportunities.add(new RefactoringOpportunity("extract_constants", segment, 
                        "Consider extracting magic numbers as named constants", "LOW"));
                }
            }
            
        } catch (Exception e) {
            System.err.println("Error detecting refactoring opportunities: " + e.getMessage());
        }
        
        return opportunities;
    }

    /**
     * Generate proactive suggestions based on analysis
     */
    private List<ProactiveSuggestion> generateProactiveSuggestions(UserBehaviorProfile profile, 
                                                                  FileContext fileContext,
                                                                  List<CodePattern> patterns,
                                                                  List<PotentialIssue> issues,
                                                                  List<RefactoringOpportunity> refactoring) {
        List<ProactiveSuggestion> suggestions = new ArrayList<>();
        
        // Generate suggestions based on patterns
        for (CodePattern pattern : patterns) {
            suggestions.add(new ProactiveSuggestion(
                "pattern_suggestion",
                "Code Pattern Detected",
                "I noticed you're using " + pattern.getDescription() + ". " +
                "Would you like me to suggest best practices or alternatives?",
                "INFO",
                pattern.getSegment(),
                Arrays.asList("Show best practices", "Suggest alternatives", "Ignore")
            ));
        }
        
        // Generate suggestions based on issues
        for (PotentialIssue issue : issues) {
            suggestions.add(new ProactiveSuggestion(
                "issue_suggestion",
                "Potential Issue",
                issue.getDescription(),
                issue.getSeverity(),
                issue.getSegment(),
                Arrays.asList("Fix automatically", "Show explanation", "Ignore")
            ));
        }
        
        // Generate suggestions based on refactoring opportunities
        for (RefactoringOpportunity opportunity : refactoring) {
            suggestions.add(new ProactiveSuggestion(
                "refactoring_suggestion",
                "Refactoring Opportunity",
                opportunity.getDescription(),
                "SUGGESTION",
                opportunity.getSegment(),
                Arrays.asList("Apply refactoring", "Show preview", "Ignore")
            ));
        }
        
        // Generate context-aware suggestions
        if (fileContext.getCurrentSegment() != null) {
            String segmentType = fileContext.getCurrentSegment().getType();
            
            if ("method".equals(segmentType)) {
                suggestions.add(new ProactiveSuggestion(
                    "method_suggestion",
                    "Method Enhancement",
                    "I can help you add documentation, tests, or improve this method. What would you like to do?",
                    "SUGGESTION",
                    fileContext.getCurrentSegment(),
                    Arrays.asList("Add documentation", "Generate tests", "Optimize performance", "Ignore")
                ));
            } else if ("class".equals(segmentType)) {
                suggestions.add(new ProactiveSuggestion(
                    "class_suggestion",
                    "Class Enhancement",
                    "I can help you add constructors, implement interfaces, or refactor this class. What would you like to do?",
                    "SUGGESTION",
                    fileContext.getCurrentSegment(),
                    Arrays.asList("Add constructors", "Implement interface", "Refactor", "Ignore")
                ));
            }
        }
        
        return suggestions;
    }

    /**
     * Get surrounding segments for context analysis
     */
    private List<CodebaseIndexer.CodeSegment> getSurroundingSegments(List<CodebaseIndexer.CodeSegment> segments, 
                                                                    int cursorLine, int contextLines) {
        List<CodebaseIndexer.CodeSegment> surrounding = new ArrayList<>();
        
        for (CodebaseIndexer.CodeSegment segment : segments) {
            if (Math.abs(segment.getStartLine() - cursorLine) <= contextLines) {
                surrounding.add(segment);
            }
        }
        
        return surrounding;
    }

    /**
     * Update user behavior profile based on actions
     */
    public void updateUserBehavior(String userId, String action, String context) {
        UserBehaviorProfile profile = getUserProfile(userId);
        profile.recordAction(action, context, Instant.now());
    }

    /**
     * Get project-wide insights and suggestions
     */
    public ProjectInsights getProjectInsights(String projectPath) {
        try {
            CodebaseIndexer.CodebaseStats stats = codebaseIndexer.getStats();
            
            // Analyze project structure
            List<String> insights = new ArrayList<>();
            
            if (stats.getTypeCounts().getOrDefault("class", 0L) > 50) {
                insights.add("Large number of classes detected - consider modularization");
            }
            
            if (stats.getTypeCounts().getOrDefault("method", 0L) > 200) {
                insights.add("Many methods detected - consider code organization");
            }
            
            // Generate project-level suggestions
            List<ProactiveSuggestion> projectSuggestions = new ArrayList<>();
            
            if (insights.size() > 0) {
                projectSuggestions.add(new ProactiveSuggestion(
                    "project_insight",
                    "Project Analysis",
                    "I've analyzed your project structure. " + String.join(" ", insights),
                    "INFO",
                    null,
                    Arrays.asList("Show detailed analysis", "Suggest improvements", "Ignore")
                ));
            }
            
            return new ProjectInsights(stats, insights, projectSuggestions);
            
        } catch (Exception e) {
            System.err.println("Error generating project insights: " + e.getMessage());
            return new ProjectInsights(null, Collections.emptyList(), Collections.emptyList());
        }
    }

    // Inner classes for data structures

    public static class UserBehaviorProfile {
        private final String userId;
        private final List<UserAction> actions;
        private final Map<String, Integer> actionFrequency;
        private final Map<String, List<String>> contextPatterns;

        public UserBehaviorProfile(String userId) {
            this.userId = userId;
            this.actions = new ArrayList<>();
            this.actionFrequency = new HashMap<>();
            this.contextPatterns = new HashMap<>();
        }

        public void recordAction(String action, String context, Instant timestamp) {
            actions.add(new UserAction(action, context, timestamp));
            actionFrequency.merge(action, 1, Integer::sum);
            contextPatterns.computeIfAbsent(action, k -> new ArrayList<>()).add(context);
        }

        public List<String> getCommonActions() {
            return actionFrequency.entrySet().stream()
                    .sorted(Map.Entry.<String, Integer>comparingByValue().reversed())
                    .limit(5)
                    .map(Map.Entry::getKey)
                    .collect(java.util.stream.Collectors.toList());
        }

        // Getters
        public String getUserId() { return userId; }
        public List<UserAction> getActions() { return actions; }
        public Map<String, Integer> getActionFrequency() { return actionFrequency; }
    }

    public static class UserAction {
        private final String action;
        private final String context;
        private final Instant timestamp;

        public UserAction(String action, String context, Instant timestamp) {
            this.action = action;
            this.context = context;
            this.timestamp = timestamp;
        }

        // Getters
        public String getAction() { return action; }
        public String getContext() { return context; }
        public Instant getTimestamp() { return timestamp; }
    }

    public static class FileContext {
        private final String filePath;
        private final int cursorLine;
        private final CodebaseIndexer.CodeSegment currentSegment;
        private final List<CodebaseIndexer.CodeSegment> surroundingSegments;

        public FileContext(String filePath, int cursorLine, CodebaseIndexer.CodeSegment currentSegment,
                          List<CodebaseIndexer.CodeSegment> surroundingSegments) {
            this.filePath = filePath;
            this.cursorLine = cursorLine;
            this.currentSegment = currentSegment;
            this.surroundingSegments = surroundingSegments;
        }

        // Getters
        public String getFilePath() { return filePath; }
        public int getCursorLine() { return cursorLine; }
        public CodebaseIndexer.CodeSegment getCurrentSegment() { return currentSegment; }
        public List<CodebaseIndexer.CodeSegment> getSurroundingSegments() { return surroundingSegments; }
    }

    public static class CodePattern {
        private final String patternType;
        private final CodebaseIndexer.CodeSegment segment;
        private final String description;

        public CodePattern(String patternType, CodebaseIndexer.CodeSegment segment, String description) {
            this.patternType = patternType;
            this.segment = segment;
            this.description = description;
        }

        // Getters
        public String getPatternType() { return patternType; }
        public CodebaseIndexer.CodeSegment getSegment() { return segment; }
        public String getDescription() { return description; }
    }

    public static class PotentialIssue {
        private final String issueType;
        private final CodebaseIndexer.CodeSegment segment;
        private final String description;
        private final String severity;

        public PotentialIssue(String issueType, CodebaseIndexer.CodeSegment segment, String description, String severity) {
            this.issueType = issueType;
            this.segment = segment;
            this.description = description;
            this.severity = severity;
        }

        // Getters
        public String getIssueType() { return issueType; }
        public CodebaseIndexer.CodeSegment getSegment() { return segment; }
        public String getDescription() { return description; }
        public String getSeverity() { return severity; }
    }

    public static class RefactoringOpportunity {
        private final String refactoringType;
        private final CodebaseIndexer.CodeSegment segment;
        private final String description;
        private final String priority;

        public RefactoringOpportunity(String refactoringType, CodebaseIndexer.CodeSegment segment, String description, String priority) {
            this.refactoringType = refactoringType;
            this.segment = segment;
            this.description = description;
            this.priority = priority;
        }

        // Getters
        public String getRefactoringType() { return refactoringType; }
        public CodebaseIndexer.CodeSegment getSegment() { return segment; }
        public String getDescription() { return description; }
        public String getPriority() { return priority; }
    }

    public static class ProactiveSuggestion {
        private final String suggestionId;
        private final String title;
        private final String description;
        private final String type;
        private final CodebaseIndexer.CodeSegment segment;
        private final List<String> actions;

        public ProactiveSuggestion(String suggestionId, String title, String description, String type,
                                  CodebaseIndexer.CodeSegment segment, List<String> actions) {
            this.suggestionId = suggestionId;
            this.title = title;
            this.description = description;
            this.type = type;
            this.segment = segment;
            this.actions = actions;
        }

        // Getters
        public String getSuggestionId() { return suggestionId; }
        public String getTitle() { return title; }
        public String getDescription() { return description; }
        public String getType() { return type; }
        public CodebaseIndexer.CodeSegment getSegment() { return segment; }
        public List<String> getActions() { return actions; }
    }

    public static class ProjectContext {
        private final String projectPath;
        private final Map<String, Object> metadata;
        private final Instant lastAnalyzed;

        public ProjectContext(String projectPath) {
            this.projectPath = projectPath;
            this.metadata = new HashMap<>();
            this.lastAnalyzed = Instant.now();
        }

        // Getters
        public String getProjectPath() { return projectPath; }
        public Map<String, Object> getMetadata() { return metadata; }
        public Instant getLastAnalyzed() { return lastAnalyzed; }
    }

    public static class ProjectInsights {
        private final CodebaseIndexer.CodebaseStats stats;
        private final List<String> insights;
        private final List<ProactiveSuggestion> suggestions;

        public ProjectInsights(CodebaseIndexer.CodebaseStats stats, List<String> insights, List<ProactiveSuggestion> suggestions) {
            this.stats = stats;
            this.insights = insights;
            this.suggestions = suggestions;
        }

        // Getters
        public CodebaseIndexer.CodebaseStats getStats() { return stats; }
        public List<String> getInsights() { return insights; }
        public List<ProactiveSuggestion> getSuggestions() { return suggestions; }
    }

    public void shutdown() {
        executorService.shutdown();
    }
}
