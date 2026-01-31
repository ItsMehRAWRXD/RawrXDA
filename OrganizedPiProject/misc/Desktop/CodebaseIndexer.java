// CodebaseIndexer.java - Semantic code indexing and retrieval

import java.io.IOException;
import java.nio.file.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.time.Instant;

/**
 * Advanced codebase indexing system that provides semantic search capabilities
 * for code retrieval. This implementation uses text-based similarity matching
 * and code structure analysis to find relevant code snippets.
 */
public class CodebaseIndexer {
    private final Map<String, CodeSegment> indexedSegments;
    private final Map<String, List<String>> fileIndex;
    private final Map<String, Set<String>> keywordIndex;
    private final Set<String> supportedExtensions;
    private final Pattern classPattern;
    private final Pattern methodPattern;
    private final Pattern importPattern;
    private final Pattern commentPattern;

    public CodebaseIndexer() {
        this.indexedSegments = new ConcurrentHashMap<>();
        this.fileIndex = new ConcurrentHashMap<>();
        this.keywordIndex = new ConcurrentHashMap<>();
        this.supportedExtensions = Set.of(".java", ".js", ".ts", ".py", ".cpp", ".c", ".h", ".hpp", ".cs", ".php", ".rb", ".go", ".rs");
        
        // Compile regex patterns for code analysis
        this.classPattern = Pattern.compile("(?:public\\s+|private\\s+|protected\\s+)?(?:static\\s+)?(?:final\\s+)?class\\s+(\\w+)");
        this.methodPattern = Pattern.compile("(?:public\\s+|private\\s+|protected\\s+)?(?:static\\s+)?(?:final\\s+)?(?:\\w+\\s+)*(\\w+)\\s*\\([^)]*\\)\\s*(?:throws\\s+[^{]+)?\\s*\\{");
        this.importPattern = Pattern.compile("import\\s+(?:static\\s+)?([\\w.]+);");
        this.commentPattern = Pattern.compile("(?:/\\*\\*[\\s\\S]*?\\*/|//.*?$)", Pattern.MULTILINE);
    }

    /**
     * Index a codebase directory
     */
    public void indexCodebase(Path projectPath) throws IOException {
        if (!Files.exists(projectPath) || !Files.isDirectory(projectPath)) {
            throw new IllegalArgumentException("Invalid project path: " + projectPath);
        }

        System.out.println("Starting codebase indexing for: " + projectPath);
        
        try (var stream = Files.walk(projectPath)) {
            stream.filter(Files::isRegularFile)
                  .filter(this::isSupportedFile)
                  .forEach(this::indexFile);
        }
        
        System.out.println("Indexing completed. Indexed " + indexedSegments.size() + " code segments from " + fileIndex.size() + " files.");
    }

    /**
     * Index a single file
     */
    private void indexFile(Path filePath) {
        try {
            String content = Files.readString(filePath, StandardCharsets.UTF_8);
            String relativePath = filePath.toString();
            
            // Split content into segments
            List<CodeSegment> segments = splitIntoSegments(content, relativePath);
            
            // Index each segment
            for (CodeSegment segment : segments) {
                String segmentId = generateSegmentId(relativePath, segment.getStartLine());
                indexedSegments.put(segmentId, segment);
                
                // Update file index
                fileIndex.computeIfAbsent(relativePath, k -> new ArrayList<>()).add(segmentId);
                
                // Update keyword index
                indexKeywords(segmentId, segment);
            }
            
        } catch (IOException e) {
            System.err.println("Error indexing file " + filePath + ": " + e.getMessage());
        }
    }

    /**
     * Split file content into meaningful code segments
     */
    private List<CodeSegment> splitIntoSegments(String content, String filePath) {
        List<CodeSegment> segments = new ArrayList<>();
        String[] lines = content.split("\n");
        
        StringBuilder currentSegment = new StringBuilder();
        int startLine = 1;
        int currentLine = 1;
        String currentType = "general";
        String currentName = "unknown";
        
        for (String line : lines) {
            String trimmedLine = line.trim();
            
            // Detect code structure
            if (isClassDeclaration(trimmedLine)) {
                // Save previous segment
                if (currentSegment.length() > 0) {
                    segments.add(new CodeSegment(filePath, startLine, currentLine - 1, 
                            currentSegment.toString(), currentType, currentName));
                }
                
                // Start new class segment
                currentSegment = new StringBuilder();
                startLine = currentLine;
                currentType = "class";
                currentName = extractClassName(trimmedLine);
            } else if (isMethodDeclaration(trimmedLine)) {
                // Save previous segment
                if (currentSegment.length() > 0) {
                    segments.add(new CodeSegment(filePath, startLine, currentLine - 1, 
                            currentSegment.toString(), currentType, currentName));
                }
                
                // Start new method segment
                currentSegment = new StringBuilder();
                startLine = currentLine;
                currentType = "method";
                currentName = extractMethodName(trimmedLine);
            } else if (isImportStatement(trimmedLine)) {
                // Save previous segment
                if (currentSegment.length() > 0) {
                    segments.add(new CodeSegment(filePath, startLine, currentLine - 1, 
                            currentSegment.toString(), currentType, currentName));
                }
                
                // Start new import segment
                currentSegment = new StringBuilder();
                startLine = currentLine;
                currentType = "import";
                currentName = extractImportName(trimmedLine);
            }
            
            currentSegment.append(line).append("\n");
            currentLine++;
        }
        
        // Add final segment
        if (currentSegment.length() > 0) {
            segments.add(new CodeSegment(filePath, startLine, currentLine - 1, 
                    currentSegment.toString(), currentType, currentName));
        }
        
        return segments;
    }

    /**
     * Index keywords for a code segment
     */
    private void indexKeywords(String segmentId, CodeSegment segment) {
        String content = segment.getContent().toLowerCase();
        
        // Extract keywords from content
        Set<String> keywords = extractKeywords(content);
        
        for (String keyword : keywords) {
            keywordIndex.computeIfAbsent(keyword, k -> ConcurrentHashMap.newKeySet()).add(segmentId);
        }
    }

    /**
     * Extract keywords from content
     */
    private Set<String> extractKeywords(String content) {
        Set<String> keywords = new HashSet<>();
        
        // Extract words (alphanumeric sequences)
        Pattern wordPattern = Pattern.compile("\\b[a-zA-Z][a-zA-Z0-9]*\\b");
        Matcher matcher = wordPattern.matcher(content);
        
        while (matcher.find()) {
            String word = matcher.group();
            if (word.length() > 2) { // Filter out short words
                keywords.add(word);
            }
        }
        
        // Extract camelCase and snake_case patterns
        Pattern camelCasePattern = Pattern.compile("([a-z]+[A-Z][a-z]+)");
        matcher = camelCasePattern.matcher(content);
        while (matcher.find()) {
            String camelCase = matcher.group();
            // Split camelCase into individual words
            String[] parts = camelCase.split("(?=[A-Z])");
            for (String part : parts) {
                if (part.length() > 2) {
                    keywords.add(part.toLowerCase());
                }
            }
        }
        
        return keywords;
    }

    /**
     * Retrieve relevant code context based on query
     */
    public List<String> retrieveRelevantContext(String query) {
        if (query == null || query.trim().isEmpty()) {
            return Collections.emptyList();
        }
        
        String lowerQuery = query.toLowerCase();
        Set<String> queryKeywords = extractKeywords(lowerQuery);
        
        // Score segments based on keyword matches
        Map<String, Double> segmentScores = new HashMap<>();
        
        for (String keyword : queryKeywords) {
            Set<String> matchingSegments = keywordIndex.get(keyword);
            if (matchingSegments != null) {
                for (String segmentId : matchingSegments) {
                    segmentScores.merge(segmentId, 1.0, Double::sum);
                }
            }
        }
        
        // Sort by score and return top matches
        return segmentScores.entrySet().stream()
                .sorted(Map.Entry.<String, Double>comparingByValue().reversed())
                .limit(5)
                .map(entry -> {
                    CodeSegment segment = indexedSegments.get(entry.getKey());
                    return segment != null ? formatSegmentForContext(segment) : null;
                })
                .filter(Objects::nonNull)
                .collect(Collectors.toList());
    }

    /**
     * Format code segment for context display
     */
    private String formatSegmentForContext(CodeSegment segment) {
        StringBuilder sb = new StringBuilder();
        sb.append("File: ").append(segment.getFilePath()).append("\n");
        sb.append("Lines: ").append(segment.getStartLine()).append("-").append(segment.getEndLine()).append("\n");
        sb.append("Type: ").append(segment.getType()).append(" - ").append(segment.getName()).append("\n");
        sb.append("Content:\n").append(segment.getContent()).append("\n");
        return sb.toString();
    }

    /**
     * Get all segments for a specific file
     */
    public List<CodeSegment> getSegmentsForFile(String filePath) {
        List<String> segmentIds = fileIndex.get(filePath);
        if (segmentIds == null) {
            return Collections.emptyList();
        }
        
        return segmentIds.stream()
                .map(indexedSegments::get)
                .filter(Objects::nonNull)
                .collect(Collectors.toList());
    }

    /**
     * Search for specific code patterns
     */
    public List<CodeSegment> searchByPattern(String pattern) {
        Pattern regexPattern = Pattern.compile(pattern, Pattern.CASE_INSENSITIVE);
        return indexedSegments.values().stream()
                .filter(segment -> regexPattern.matcher(segment.getContent()).find())
                .collect(Collectors.toList());
    }

    /**
     * Get code segments by type
     */
    public List<CodeSegment> getSegmentsByType(String type) {
        return indexedSegments.values().stream()
                .filter(segment -> type.equals(segment.getType()))
                .collect(Collectors.toList());
    }

    /**
     * Get statistics about the indexed codebase
     */
    public CodebaseStats getStats() {
        Map<String, Long> typeCounts = indexedSegments.values().stream()
                .collect(Collectors.groupingBy(CodeSegment::getType, Collectors.counting()));
        
        return new CodebaseStats(
                indexedSegments.size(),
                fileIndex.size(),
                keywordIndex.size(),
                typeCounts
        );
    }

    // Helper methods for code structure detection
    private boolean isSupportedFile(Path filePath) {
        String fileName = filePath.getFileName().toString();
        return supportedExtensions.stream().anyMatch(fileName::endsWith);
    }

    private boolean isClassDeclaration(String line) {
        return classPattern.matcher(line).find();
    }

    private boolean isMethodDeclaration(String line) {
        return methodPattern.matcher(line).find();
    }

    private boolean isImportStatement(String line) {
        return importPattern.matcher(line).find();
    }

    private String extractClassName(String line) {
        Matcher matcher = classPattern.matcher(line);
        return matcher.find() ? matcher.group(1) : "unknown";
    }

    private String extractMethodName(String line) {
        Matcher matcher = methodPattern.matcher(line);
        return matcher.find() ? matcher.group(1) : "unknown";
    }

    private String extractImportName(String line) {
        Matcher matcher = importPattern.matcher(line);
        return matcher.find() ? matcher.group(1) : "unknown";
    }

    private String generateSegmentId(String filePath, int startLine) {
        return filePath + ":" + startLine;
    }

    /**
     * Code segment data structure
     */
    public static class CodeSegment {
        private final String filePath;
        private final int startLine;
        private final int endLine;
        private final String content;
        private final String type;
        private final String name;
        private final Instant indexedAt;

        public CodeSegment(String filePath, int startLine, int endLine, String content, String type, String name) {
            this.filePath = filePath;
            this.startLine = startLine;
            this.endLine = endLine;
            this.content = content;
            this.type = type;
            this.name = name;
            this.indexedAt = Instant.now();
        }

        // Getters
        public String getFilePath() { return filePath; }
        public int getStartLine() { return startLine; }
        public int getEndLine() { return endLine; }
        public String getContent() { return content; }
        public String getType() { return type; }
        public String getName() { return name; }
        public Instant getIndexedAt() { return indexedAt; }

        @Override
        public String toString() {
            return String.format("CodeSegment{file='%s', lines=%d-%d, type='%s', name='%s'}", 
                    filePath, startLine, endLine, type, name);
        }
    }

    /**
     * Codebase statistics
     */
    public static class CodebaseStats {
        private final int totalSegments;
        private final int totalFiles;
        private final int totalKeywords;
        private final Map<String, Long> typeCounts;

        public CodebaseStats(int totalSegments, int totalFiles, int totalKeywords, Map<String, Long> typeCounts) {
            this.totalSegments = totalSegments;
            this.totalFiles = totalFiles;
            this.totalKeywords = totalKeywords;
            this.typeCounts = typeCounts;
        }

        public int getTotalSegments() { return totalSegments; }
        public int getTotalFiles() { return totalFiles; }
        public int getTotalKeywords() { return totalKeywords; }
        public Map<String, Long> getTypeCounts() { return typeCounts; }

        @Override
        public String toString() {
            return String.format("CodebaseStats{segments=%d, files=%d, keywords=%d, types=%s}", 
                    totalSegments, totalFiles, totalKeywords, typeCounts);
        }
    }
}
