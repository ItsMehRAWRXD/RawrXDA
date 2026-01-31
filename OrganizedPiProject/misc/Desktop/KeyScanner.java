// KeyScanner.java - Responsible API Key Discovery Tool
// Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510
// IMPORTANT: This tool is for educational and security research purposes only

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.regex.*;
import java.util.concurrent.*;
import java.util.stream.Collectors;

public class KeyScanner {
    
    // Key patterns for different providers
    private static final Map<String, Pattern> KEY_PATTERNS = Map.of(
        "openai", Pattern.compile("sk-[a-zA-Z0-9]{48}"),
        "gemini", Pattern.compile("AIza[0-9A-Za-z\\-_]{35}"),
        "anthropic", Pattern.compile("sk-ant-[a-zA-Z0-9\\-_]{95}"),
        "mistral", Pattern.compile("[a-zA-Z0-9]{32}"),
        "cohere", Pattern.compile("[a-zA-Z0-9]{40}"),
        "huggingface", Pattern.compile("hf_[a-zA-Z0-9]{34}"),
        "aws", Pattern.compile("AKIA[0-9A-Z]{16}"),
        "github", Pattern.compile("ghp_[a-zA-Z0-9]{36}"),
        "stripe", Pattern.compile("sk_live_[a-zA-Z0-9]{24}"),
        "mailgun", Pattern.compile("key-[a-zA-Z0-9]{32}")
    );
    
    // Variable names that commonly contain API keys
    private static final Set<String> KEY_VARIABLES = Set.of(
        "API_KEY", "API_SECRET", "SECRET_KEY", "ACCESS_KEY", "PRIVATE_KEY",
        "OPENAI_API_KEY", "GEMINI_API_KEY", "ANTHROPIC_API_KEY",
        "AWS_ACCESS_KEY", "AWS_SECRET_KEY", "STRIPE_SECRET_KEY",
        "GITHUB_TOKEN", "GITLAB_TOKEN", "BITBUCKET_TOKEN"
    );
    
    private final String githubToken;
    private final boolean enableVerification;
    private final int maxConcurrentRequests;
    
    public KeyScanner(String githubToken, boolean enableVerification, int maxConcurrentRequests) {
        this.githubToken = githubToken;
        this.enableVerification = enableVerification;
        this.maxConcurrentRequests = maxConcurrentRequests;
    }
    
    public static void main(String[] args) {
        System.out.println("? KeyScanner - Responsible API Key Discovery Tool");
        System.out.println("??  WARNING: This tool is for educational and security research purposes only!");
        System.out.println("??  Always follow responsible disclosure practices!\n");
        
        // Configuration
        String githubToken = System.getenv("GITHUB_TOKEN");
        boolean enableVerification = false; // Set to true only for legitimate research
        int maxConcurrentRequests = 5;
        
        KeyScanner scanner = new KeyScanner(githubToken, enableVerification, maxConcurrentRequests);
        
        // Search terms for different file types
        String[] searchTerms = {
            "OPENAI_API_KEY",
            "GEMINI_API_KEY", 
            "ANTHROPIC_API_KEY",
            "AWS_ACCESS_KEY",
            "STRIPE_SECRET_KEY",
            "GITHUB_TOKEN"
        };
        
        String[] fileTypes = {".env", "config", "secrets", "credentials"};
        
        try {
            scanner.scanGitHubGists(searchTerms, fileTypes);
        } catch (Exception e) {
            System.err.println("Scanner error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public void scanGitHubGists(String[] searchTerms, String[] fileTypes) throws Exception {
        System.out.println("? Scanning GitHub Gists for potential API key exposures...\n");
        
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<ScanResult>> futures = new ArrayList<>();
        
        for (String term : searchTerms) {
            for (String fileType : fileTypes) {
                Future<ScanResult> future = executor.submit(() -> {
                    return searchGists(term, fileType);
                });
                futures.add(future);
            }
        }
        
        // Collect results
        List<ScanResult> results = new ArrayList<>();
        for (Future<ScanResult> future : futures) {
            try {
                ScanResult result = future.get(30, TimeUnit.SECONDS);
                if (result != null && !result.getFoundKeys().isEmpty()) {
                    results.add(result);
                }
            } catch (TimeoutException e) {
                System.err.println("Search timeout for term");
            }
        }
        
        executor.shutdown();
        
        // Process and report results
        processResults(results);
    }
    
    private ScanResult searchGists(String searchTerm, String fileType) {
        try {
            // Search GitHub Gists (simplified version)
            String searchUrl = String.format(
                "https://gist.github.com/search?q=%s+filename:%s",
                URLEncoder.encode(searchTerm, "UTF-8"),
                URLEncoder.encode(fileType, "UTF-8")
            );
            
            System.out.println("? Searching: " + searchTerm + " in " + fileType + " files");
            
            // In a real implementation, you would:
            // 1. Parse the search results HTML
            // 2. Extract Gist IDs
            // 3. Fetch Gist content via GitHub API
            // 4. Analyze content for API keys
            
            // For demonstration, we'll simulate finding some keys
            return simulateKeyDiscovery(searchTerm, fileType);
            
        } catch (Exception e) {
            System.err.println("Search error for " + searchTerm + ": " + e.getMessage());
            return null;
        }
    }
    
    private ScanResult simulateKeyDiscovery(String searchTerm, String fileType) {
        // This is a simulation - in real implementation, you'd parse actual Gist content
        List<DiscoveredKey> foundKeys = new ArrayList<>();
        
        // Simulate finding some keys (these are obviously fake)
        if (searchTerm.contains("OPENAI")) {
            foundKeys.add(new DiscoveredKey(
                "sk-test1234567890abcdef1234567890abcdef12",
                "openai",
                "HIGH",
                "https://gist.github.com/simulated123",
                "OPENAI_API_KEY=sk-test1234567890abcdef1234567890abcdef12"
            ));
        }
        
        if (searchTerm.contains("GEMINI")) {
            foundKeys.add(new DiscoveredKey(
                "AIzaTest1234567890abcdef1234567890abcdef",
                "gemini", 
                "MEDIUM",
                "https://gist.github.com/simulated456",
                "GEMINI_API_KEY=AIzaTest1234567890abcdef1234567890abcdef"
            ));
        }
        
        return new ScanResult(searchTerm, fileType, foundKeys);
    }
    
    private void processResults(List<ScanResult> results) {
        System.out.println("\n? Scan Results Summary:");
        System.out.println("=" + "=".repeat(50));
        
        int totalKeys = 0;
        Map<String, Integer> providerCounts = new HashMap<>();
        
        for (ScanResult result : results) {
            if (!result.getFoundKeys().isEmpty()) {
                System.out.println("\n? Search Term: " + result.getSearchTerm());
                System.out.println("? File Type: " + result.getFileType());
                System.out.println("? Keys Found: " + result.getFoundKeys().size());
                
                for (DiscoveredKey key : result.getFoundKeys()) {
                    System.out.println("  • Provider: " + key.getProvider());
                    System.out.println("  • Confidence: " + key.getConfidence());
                    System.out.println("  • Key: " + maskKey(key.getKey()));
                    System.out.println("  • Context: " + key.getContext());
                    System.out.println("  • Source: " + key.getSource());
                    
                    totalKeys++;
                    providerCounts.merge(key.getProvider(), 1, Integer::sum);
                }
            }
        }
        
        System.out.println("\n? Summary:");
        System.out.println("Total keys found: " + totalKeys);
        System.out.println("Provider breakdown:");
        providerCounts.forEach((provider, count) -> 
            System.out.println("  • " + provider + ": " + count));
        
        if (totalKeys > 0) {
            System.out.println("\n??  IMPORTANT REMINDERS:");
            System.out.println("1. These are simulated results for demonstration");
            System.out.println("2. In real usage, ALWAYS follow responsible disclosure");
            System.out.println("3. Contact key owners immediately if real keys are found");
            System.out.println("4. Never use exposed keys without permission");
            System.out.println("5. Report security issues through proper channels");
        }
    }
    
    private String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(key.length() - 8) + key.substring(key.length() - 4);
    }
    
    // Inner classes for data structures
    public static class ScanResult {
        private final String searchTerm;
        private final String fileType;
        private final List<DiscoveredKey> foundKeys;
        
        public ScanResult(String searchTerm, String fileType, List<DiscoveredKey> foundKeys) {
            this.searchTerm = searchTerm;
            this.fileType = fileType;
            this.foundKeys = foundKeys;
        }
        
        public String getSearchTerm() { return searchTerm; }
        public String getFileType() { return fileType; }
        public List<DiscoveredKey> getFoundKeys() { return foundKeys; }
    }
    
    public static class DiscoveredKey {
        private final String key;
        private final String provider;
        private final String confidence;
        private final String source;
        private final String context;
        
        public DiscoveredKey(String key, String provider, String confidence, String source, String context) {
            this.key = key;
            this.provider = provider;
            this.confidence = confidence;
            this.source = source;
            this.context = context;
        }
        
        public String getKey() { return key; }
        public String getProvider() { return provider; }
        public String getConfidence() { return confidence; }
        public String getSource() { return source; }
        public String getContext() { return context; }
    }
}
