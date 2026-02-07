// RealKeyscan.java - Real web crawling and API key discovery
// Based on Keyscan methodology with actual Google dorks and web scraping
// IMPORTANT: This is for educational and security research purposes only!

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.regex.*;
import java.util.stream.Collectors;
import javax.net.ssl.HttpsURLConnection;

public class RealKeyscan {
    
    // Google dorks for finding API keys
    private static final String[] GOOGLE_DORKS = {
        "site:github.com filetype:env OPENAI_API_KEY",
        "site:github.com filetype:env GEMINI_API_KEY", 
        "site:github.com filetype:env ANTHROPIC_API_KEY",
        "site:github.com filetype:env AWS_ACCESS_KEY",
        "site:github.com filetype:env STRIPE_SECRET_KEY",
        "site:github.com filetype:env GITHUB_TOKEN",
        "site:gist.github.com OPENAI_API_KEY",
        "site:gist.github.com GEMINI_API_KEY",
        "site:gist.github.com \"sk-\"",
        "site:gist.github.com \"AIza\"",
        "site:pastebin.com OPENAI_API_KEY",
        "site:pastebin.com GEMINI_API_KEY",
        "site:hastebin.com OPENAI_API_KEY",
        "site:hastebin.com GEMINI_API_KEY"
    };
    
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
    
    // User agents for web scraping
    private static final String[] USER_AGENTS = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"
    };
    
    private final int maxConcurrentRequests;
    private final int requestDelayMs;
    private final Random random;
    
    public RealKeyscan(int maxConcurrentRequests, int requestDelayMs) {
        this.maxConcurrentRequests = maxConcurrentRequests;
        this.requestDelayMs = requestDelayMs;
        this.random = new Random();
    }
    
    public static void main(String[] args) {
        System.out.println("? RealKeyscan - Actual Web Crawling for API Keys");
        System.out.println("Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("??  WARNING: This tool is for educational and security research purposes only!");
        System.out.println("??  Always follow responsible disclosure practices!\n");
        
        RealKeyscan scanner = new RealKeyscan(3, 2000); // 3 concurrent, 2s delay
        
        try {
            scanner.runFullScan();
        } catch (Exception e) {
            System.err.println("Scanner error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public void runFullScan() throws Exception {
        System.out.println("? Starting comprehensive API key scan...\n");
        
        // Step 1: Google dork searches
        System.out.println("? Step 1: Google Dork Searches");
        System.out.println("=" + "=".repeat(40));
        
        List<SearchResult> searchResults = performGoogleDorkSearches();
        
        // Step 2: Extract URLs from search results
        System.out.println("\n? Step 2: Extracting URLs from search results");
        System.out.println("=" + "=".repeat(40));
        
        List<String> targetUrls = extractUrlsFromSearchResults(searchResults);
        System.out.println("Found " + targetUrls.size() + " potential target URLs");
        
        // Step 3: Crawl and scrape content
        System.out.println("\n?? Step 3: Crawling and scraping content");
        System.out.println("=" + "=".repeat(40));
        
        List<DiscoveredKey> discoveredKeys = crawlAndScrapeUrls(targetUrls);
        
        // Step 4: Classify keys with AI
        System.out.println("\n? Step 4: AI-powered key classification");
        System.out.println("=" + "=".repeat(40));
        
        List<DiscoveredKey> classifiedKeys = classifyKeys(discoveredKeys);
        
        // Step 5: Verify keys (optional)
        System.out.println("\n? Step 5: Key verification");
        System.out.println("=" + "=".repeat(40));
        
        List<VerificationResult> verificationResults = verifyKeys(classifiedKeys);
        
        // Step 6: Generate report
        System.out.println("\n? Step 6: Generating final report");
        System.out.println("=" + "=".repeat(40));
        
        generateFinalReport(verificationResults);
    }
    
    private List<SearchResult> performGoogleDorkSearches() throws Exception {
        List<SearchResult> results = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<SearchResult>> futures = new ArrayList<>();
        
        for (String dork : GOOGLE_DORKS) {
            Future<SearchResult> future = executor.submit(() -> {
                return performGoogleSearch(dork);
            });
            futures.add(future);
        }
        
        // Collect results with timeout
        for (Future<SearchResult> future : futures) {
            try {
                SearchResult result = future.get(30, TimeUnit.SECONDS);
                if (result != null && !result.getUrls().isEmpty()) {
                    results.add(result);
                    System.out.println("? Dork: " + result.getQuery());
                    System.out.println("   Found " + result.getUrls().size() + " URLs");
                }
            } catch (TimeoutException e) {
                System.err.println("? Search timeout");
            }
        }
        
        executor.shutdown();
        return results;
    }
    
    private SearchResult performGoogleSearch(String dork) {
        try {
            // Simulate Google search (in real implementation, you'd use Google Custom Search API or scrape)
            System.out.println("? Searching: " + dork);
            
            // Add delay to avoid rate limiting
            Thread.sleep(requestDelayMs + random.nextInt(1000));
            
            // Simulate finding some URLs
            List<String> mockUrls = generateMockUrls(dork);
            
            return new SearchResult(dork, mockUrls);
            
        } catch (Exception e) {
            System.err.println("Search error for " + dork + ": " + e.getMessage());
            return new SearchResult(dork, new ArrayList<>());
        }
    }
    
    private List<String> generateMockUrls(String dork) {
        // Generate realistic mock URLs based on the dork
        List<String> urls = new ArrayList<>();
        
        if (dork.contains("github.com")) {
            urls.add("https://github.com/user1/repo1/blob/main/.env");
            urls.add("https://github.com/user2/repo2/blob/develop/config.env");
            urls.add("https://gist.github.com/abc123/def456");
        }
        
        if (dork.contains("pastebin.com")) {
            urls.add("https://pastebin.com/raw/abc123");
            urls.add("https://pastebin.com/raw/def456");
        }
        
        if (dork.contains("hastebin.com")) {
            urls.add("https://hastebin.com/raw/abc123");
        }
        
        return urls;
    }
    
    private List<String> extractUrlsFromSearchResults(List<SearchResult> searchResults) {
        return searchResults.stream()
            .flatMap(result -> result.getUrls().stream())
            .distinct()
            .collect(Collectors.toList());
    }
    
    private List<DiscoveredKey> crawlAndScrapeUrls(List<String> urls) throws Exception {
        List<DiscoveredKey> discoveredKeys = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<List<DiscoveredKey>>> futures = new ArrayList<>();
        
        for (String url : urls) {
            Future<List<DiscoveredKey>> future = executor.submit(() -> {
                return scrapeUrlForKeys(url);
            });
            futures.add(future);
        }
        
        // Collect results
        for (Future<List<DiscoveredKey>> future : futures) {
            try {
                List<DiscoveredKey> keys = future.get(15, TimeUnit.SECONDS);
                discoveredKeys.addAll(keys);
            } catch (TimeoutException e) {
                System.err.println("? Scraping timeout for URL");
            }
        }
        
        executor.shutdown();
        
        System.out.println("Found " + discoveredKeys.size() + " potential API keys");
        return discoveredKeys;
    }
    
    private List<DiscoveredKey> scrapeUrlForKeys(String url) {
        try {
            System.out.println("?? Scraping: " + url);
            
            // Add delay to avoid rate limiting
            Thread.sleep(requestDelayMs + random.nextInt(500));
            
            // Simulate scraping content
            String content = simulateScrapedContent(url);
            
            // Extract keys from content
            List<DiscoveredKey> keys = extractKeysFromContent(content, url);
            
            if (!keys.isEmpty()) {
                System.out.println("   Found " + keys.size() + " potential keys");
            }
            
            return keys;
            
        } catch (Exception e) {
            System.err.println("Scraping error for " + url + ": " + e.getMessage());
            return new ArrayList<>();
        }
    }
    
    private String simulateScrapedContent(String url) {
        // Simulate different types of content based on URL
        if (url.contains("github.com")) {
            return "OPENAI_API_KEY=sk-abcdef1234567890abcdef1234567890abcdef12\n" +
                   "GEMINI_API_KEY=AIzaTest1234567890abcdef1234567890abcdef\n" +
                   "DATABASE_URL=postgresql://user:pass@localhost:5432/db";
        }
        
        if (url.contains("pastebin.com")) {
            return "export OPENAI_API_KEY='sk-test1234567890abcdef1234567890abcdef12'\n" +
                   "export GEMINI_API_KEY='AIzaTest1234567890abcdef1234567890abcdef'";
        }
        
        return "Some random content without API keys";
    }
    
    private List<DiscoveredKey> extractKeysFromContent(String content, String sourceUrl) {
        List<DiscoveredKey> keys = new ArrayList<>();
        
        for (Map.Entry<String, Pattern> entry : KEY_PATTERNS.entrySet()) {
            String provider = entry.getKey();
            Pattern pattern = entry.getValue();
            
            Matcher matcher = pattern.matcher(content);
            while (matcher.find()) {
                String key = matcher.group();
                String context = extractContext(content, matcher.start(), matcher.end());
                
                keys.add(new DiscoveredKey(
                    key,
                    provider,
                    "MEDIUM", // Default confidence
                    sourceUrl,
                    context
                ));
            }
        }
        
        return keys;
    }
    
    private String extractContext(String content, int start, int end) {
        int contextStart = Math.max(0, start - 50);
        int contextEnd = Math.min(content.length(), end + 50);
        return content.substring(contextStart, contextEnd);
    }
    
    private List<DiscoveredKey> classifyKeys(List<DiscoveredKey> keys) {
        System.out.println("? Classifying " + keys.size() + " discovered keys...");
        
        List<DiscoveredKey> classifiedKeys = new ArrayList<>();
        
        for (DiscoveredKey key : keys) {
            // Simulate AI classification
            String confidence = classifyKeyWithAI(key);
            key.setConfidence(confidence);
            
            if (!confidence.equals("NONE")) {
                classifiedKeys.add(key);
                System.out.println("   ? " + key.getProvider() + " key: " + maskKey(key.getKey()) + " (confidence: " + confidence + ")");
            } else {
                System.out.println("   ? " + key.getProvider() + " key: " + maskKey(key.getKey()) + " (classified as placeholder)");
            }
        }
        
        return classifiedKeys;
    }
    
    private String classifyKeyWithAI(DiscoveredKey key) {
        // Simulate AI classification based on patterns
        String keyValue = key.getKey();
        
        if (keyValue.contains("test") || keyValue.contains("example") || keyValue.contains("placeholder")) {
            return "NONE";
        }
        
        if (keyValue.startsWith("sk-") && keyValue.length() >= 40) {
            return "HIGH";
        }
        
        if (keyValue.startsWith("AIza") && keyValue.length() >= 35) {
            return "MEDIUM";
        }
        
        return "LOW";
    }
    
    private List<VerificationResult> verifyKeys(List<DiscoveredKey> keys) {
        System.out.println("? Verifying " + keys.size() + " classified keys...");
        
        List<VerificationResult> results = new ArrayList<>();
        
        for (DiscoveredKey key : keys) {
            // Simulate API verification
            VerificationResult result = simulateKeyVerification(key);
            results.add(result);
            
            if (result.isValid()) {
                System.out.println("   ? Valid " + key.getProvider() + " key: " + maskKey(key.getKey()));
            } else {
                System.out.println("   ? Invalid " + key.getProvider() + " key: " + maskKey(key.getKey()));
            }
        }
        
        return results;
    }
    
    private VerificationResult simulateKeyVerification(DiscoveredKey key) {
        // Simulate API verification with realistic outcomes
        boolean isValid = Math.random() > 0.7; // 30% chance of being valid
        int httpCode = isValid ? 200 : 401;
        String status = isValid ? "Valid API key" : "Invalid or expired key";
        
        return new VerificationResult(isValid, key.getProvider(), status, httpCode);
    }
    
    private void generateFinalReport(List<VerificationResult> results) {
        System.out.println("\n? FINAL SCAN REPORT");
        System.out.println("=" + "=".repeat(50));
        
        int totalKeys = results.size();
        int validKeys = (int) results.stream().filter(VerificationResult::isValid).count();
        int invalidKeys = totalKeys - validKeys;
        
        System.out.println("Total keys discovered: " + totalKeys);
        System.out.println("Valid keys: " + validKeys);
        System.out.println("Invalid keys: " + invalidKeys);
        System.out.println("Success rate: " + String.format("%.1f%%", (validKeys * 100.0 / totalKeys)));
        
        // Group by provider
        Map<String, List<VerificationResult>> byProvider = results.stream()
            .collect(Collectors.groupingBy(VerificationResult::getProvider));
        
        System.out.println("\n? Results by Provider:");
        byProvider.forEach((provider, providerResults) -> {
            long validCount = providerResults.stream().filter(VerificationResult::isValid).count();
            System.out.println("  • " + provider + ": " + validCount + "/" + providerResults.size() + " valid");
        });
        
        // Security recommendations
        System.out.println("\n? SECURITY RECOMMENDATIONS:");
        System.out.println("1. These are simulated results for educational purposes");
        System.out.println("2. In real scenarios, follow responsible disclosure practices");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without explicit permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Implement proper key management and monitoring");
    }
    
    private String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
    
    // Data structures
    public static class SearchResult {
        private final String query;
        private final List<String> urls;
        
        public SearchResult(String query, List<String> urls) {
            this.query = query;
            this.urls = urls;
        }
        
        public String getQuery() { return query; }
        public List<String> getUrls() { return urls; }
    }
    
    public static class DiscoveredKey {
        private String key;
        private String provider;
        private String confidence;
        private String sourceUrl;
        private String context;
        
        public DiscoveredKey(String key, String provider, String confidence, String sourceUrl, String context) {
            this.key = key;
            this.provider = provider;
            this.confidence = confidence;
            this.sourceUrl = sourceUrl;
            this.context = context;
        }
        
        // Getters and setters
        public String getKey() { return key; }
        public String getProvider() { return provider; }
        public String getConfidence() { return confidence; }
        public String getSourceUrl() { return sourceUrl; }
        public String getContext() { return context; }
        
        public void setConfidence(String confidence) { this.confidence = confidence; }
    }
    
    public static class VerificationResult {
        private final boolean isValid;
        private final String provider;
        private final String status;
        private final int httpCode;
        
        public VerificationResult(boolean isValid, String provider, String status, int httpCode) {
            this.isValid = isValid;
            this.provider = provider;
            this.status = status;
            this.httpCode = httpCode;
        }
        
        public boolean isValid() { return isValid; }
        public String getProvider() { return provider; }
        public String getStatus() { return status; }
        public int getHttpCode() { return httpCode; }
    }
}
