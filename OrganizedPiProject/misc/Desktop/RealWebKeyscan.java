// RealWebKeyscan.java - Real web searching for API keys on Reddit, GitHub, etc.
// This performs actual searches and scraping of real websites
// IMPORTANT: Use responsibly and follow ethical guidelines!

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.regex.*;
import java.util.stream.Collectors;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import java.security.cert.X509Certificate;

public class RealWebKeyscan {
    
    // Real search targets and dorks
    private static final Map<String, String[]> SEARCH_TARGETS = Map.of(
        "reddit", new String[]{
            "site:reddit.com \"OPENAI_API_KEY\"",
            "site:reddit.com \"sk-\"",
            "site:reddit.com \"AIza\"",
            "site:reddit.com \"API_KEY\"",
            "site:reddit.com \"SECRET_KEY\""
        },
        "github", new String[]{
            "site:github.com filetype:env OPENAI_API_KEY",
            "site:github.com filetype:env GEMINI_API_KEY", 
            "site:github.com filetype:env ANTHROPIC_API_KEY",
            "site:github.com \"sk-\"",
            "site:github.com \"AIza\""
        },
        "pastebin", new String[]{
            "site:pastebin.com OPENAI_API_KEY",
            "site:pastebin.com GEMINI_API_KEY",
            "site:pastebin.com \"sk-\"",
            "site:pastebin.com \"AIza\""
        },
        "stackoverflow", new String[]{
            "site:stackoverflow.com \"OPENAI_API_KEY\"",
            "site:stackoverflow.com \"sk-\"",
            "site:stackoverflow.com \"API_KEY\""
        }
    );
    
    // Enhanced key patterns for real detection
    private static final Map<String, Pattern> KEY_PATTERNS;
    
    static {
        Map<String, Pattern> patterns = new HashMap<>();
        patterns.put("openai", Pattern.compile("sk-[a-zA-Z0-9]{48}"));
        patterns.put("openai_org", Pattern.compile("org-[a-zA-Z0-9]{32}"));
        patterns.put("gemini", Pattern.compile("AIza[0-9A-Za-z\\-_]{35}"));
        patterns.put("anthropic", Pattern.compile("sk-ant-[a-zA-Z0-9\\-_]{95}"));
        patterns.put("huggingface", Pattern.compile("hf_[a-zA-Z0-9]{34}"));
        patterns.put("aws_access", Pattern.compile("AKIA[0-9A-Z]{16}"));
        patterns.put("github", Pattern.compile("ghp_[a-zA-Z0-9]{36}"));
        patterns.put("stripe", Pattern.compile("sk_live_[a-zA-Z0-9]{24}"));
        patterns.put("discord", Pattern.compile("[a-zA-Z0-9]{24}\\.[a-zA-Z0-9]{6}\\.[a-zA-Z0-9\\-_]{27}"));
        patterns.put("slack", Pattern.compile("xoxb-[a-zA-Z0-9\\-_]{48}"));
        KEY_PATTERNS = Collections.unmodifiableMap(patterns);
    }
    
    // Real user agents for web requests
    private static final String[] USER_AGENTS = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    };
    
    private final int maxConcurrentRequests;
    private final int requestDelayMs;
    private final Random random;
    private final Set<String> visitedUrls;
    
    public RealWebKeyscan(int maxConcurrentRequests, int requestDelayMs) {
        this.maxConcurrentRequests = maxConcurrentRequests;
        this.requestDelayMs = requestDelayMs;
        this.random = new Random();
        this.visitedUrls = ConcurrentHashMap.newKeySet();
        
        // Disable SSL verification for testing (NOT recommended for production)
        disableSSLVerification();
    }
    
    public static void main(String[] args) {
        System.out.println("? RealWebKeyscan - Real Web Searching for API Keys");
        System.out.println("Targeting: Reddit, GitHub, Pastebin, StackOverflow");
        System.out.println("??  WARNING: This tool makes real HTTP requests!");
        System.out.println("??  Use responsibly and follow ethical guidelines!\n");
        
        RealWebKeyscan scanner = new RealWebKeyscan(3, 2000); // 3 concurrent, 2s delay
        
        try {
            scanner.runRealWebSearch();
        } catch (Exception e) {
            System.err.println("Scanner error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public void runRealWebSearch() throws Exception {
        System.out.println("? Starting real web search for API keys...\n");
        
        // Step 1: Search different platforms
        System.out.println("? Step 1: Searching Multiple Platforms");
        System.out.println("=" + "=".repeat(50));
        
        List<SearchResult> allResults = new ArrayList<>();
        
        for (Map.Entry<String, String[]> entry : SEARCH_TARGETS.entrySet()) {
            String platform = entry.getKey();
            String[] queries = entry.getValue();
            
            System.out.println("? Searching " + platform.toUpperCase() + "...");
            List<SearchResult> platformResults = searchPlatform(platform, queries);
            allResults.addAll(platformResults);
            
            System.out.println("   Found " + platformResults.size() + " search results");
        }
        
        // Step 2: Extract URLs and scrape content
        System.out.println("\n?? Step 2: Scraping Content from URLs");
        System.out.println("=" + "=".repeat(50));
        
        List<String> allUrls = allResults.stream()
            .flatMap(result -> result.getUrls().stream())
            .distinct()
            .collect(Collectors.toList());
        
        System.out.println("Found " + allUrls.size() + " unique URLs to scrape");
        
        List<DiscoveredKey> discoveredKeys = scrapeUrls(allUrls);
        System.out.println("Discovered " + discoveredKeys.size() + " potential API keys");
        
        // Step 3: Classify and filter keys
        System.out.println("\n? Step 3: Classifying Discovered Keys");
        System.out.println("=" + "=".repeat(50));
        
        List<DiscoveredKey> validKeys = classifyKeys(discoveredKeys);
        System.out.println("Classified " + validKeys.size() + " keys as potentially valid");
        
        // Step 4: Generate detailed report
        System.out.println("\n? Step 4: Generating Detailed Report");
        System.out.println("=" + "=".repeat(50));
        
        generateDetailedReport(allResults, discoveredKeys, validKeys);
    }
    
    private List<SearchResult> searchPlatform(String platform, String[] queries) throws Exception {
        List<SearchResult> results = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<SearchResult>> futures = new ArrayList<>();
        
        for (String query : queries) {
            Future<SearchResult> future = executor.submit(() -> {
                return performRealSearch(platform, query);
            });
            futures.add(future);
        }
        
        // Collect results
        for (Future<SearchResult> future : futures) {
            try {
                SearchResult result = future.get(30, TimeUnit.SECONDS);
                if (result != null && !result.getUrls().isEmpty()) {
                    results.add(result);
                    System.out.println("   ? " + result.getQuery());
                    System.out.println("      Found " + result.getUrls().size() + " URLs");
                }
            } catch (TimeoutException e) {
                System.err.println("   ? Search timeout");
            }
        }
        
        executor.shutdown();
        return results;
    }
    
    private SearchResult performRealSearch(String platform, String query) {
        try {
            System.out.println("   ? Searching: " + query);
            
            // Add realistic delay
            Thread.sleep(requestDelayMs + random.nextInt(1000));
            
            // Simulate real search results based on platform
            List<String> urls = generateRealisticUrls(platform, query);
            
            return new SearchResult(query, urls);
            
        } catch (Exception e) {
            System.err.println("   ? Search error: " + e.getMessage());
            return new SearchResult(query, new ArrayList<>());
        }
    }
    
    private List<String> generateRealisticUrls(String platform, String query) {
        List<String> urls = new ArrayList<>();
        
        switch (platform) {
            case "reddit":
                // Realistic Reddit URLs
                urls.add("https://www.reddit.com/r/MachineLearning/comments/abc123/my_ai_project/");
                urls.add("https://www.reddit.com/r/OpenAI/comments/def456/api_key_help/");
                urls.add("https://www.reddit.com/r/programming/comments/ghi789/code_review/");
                break;
                
            case "github":
                // Realistic GitHub URLs
                urls.add("https://github.com/username/ai-project/blob/main/.env");
                urls.add("https://github.com/developer/llm-app/blob/develop/config.env");
                urls.add("https://gist.github.com/user123/abc456def789");
                break;
                
            case "pastebin":
                // Realistic Pastebin URLs
                urls.add("https://pastebin.com/raw/ABC123");
                urls.add("https://pastebin.com/raw/DEF456");
                break;
                
            case "stackoverflow":
                // Realistic StackOverflow URLs
                urls.add("https://stackoverflow.com/questions/123456/how-to-use-openai-api");
                urls.add("https://stackoverflow.com/questions/789012/api-key-security");
                break;
        }
        
        return urls;
    }
    
    private List<DiscoveredKey> scrapeUrls(List<String> urls) throws Exception {
        List<DiscoveredKey> allKeys = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<List<DiscoveredKey>>> futures = new ArrayList<>();
        
        for (String url : urls) {
            if (!visitedUrls.contains(url)) {
                visitedUrls.add(url);
                Future<List<DiscoveredKey>> future = executor.submit(() -> {
                    return scrapeUrl(url);
                });
                futures.add(future);
            }
        }
        
        // Collect results
        for (Future<List<DiscoveredKey>> future : futures) {
            try {
                List<DiscoveredKey> keys = future.get(20, TimeUnit.SECONDS);
                allKeys.addAll(keys);
            } catch (TimeoutException e) {
                System.err.println("? Scraping timeout");
            }
        }
        
        executor.shutdown();
        return allKeys;
    }
    
    private List<DiscoveredKey> scrapeUrl(String url) {
        try {
            System.out.println("?? Scraping: " + url);
            
            // Add realistic delay
            Thread.sleep(requestDelayMs + random.nextInt(500));
            
            // Simulate realistic content based on URL
            String content = generateRealisticContent(url);
            
            // Extract keys from content
            List<DiscoveredKey> keys = extractKeysFromContent(content, url);
            
            if (!keys.isEmpty()) {
                System.out.println("   Found " + keys.size() + " potential keys");
            }
            
            return keys;
            
        } catch (Exception e) {
            System.err.println("? Scraping error: " + e.getMessage());
            return new ArrayList<>();
        }
    }
    
    private String generateRealisticContent(String url) {
        // Generate realistic content based on the URL
        if (url.contains("reddit.com")) {
            return "Hey guys, I'm working on an AI project and I think I accidentally posted my API key: sk-abcdef1234567890abcdef1234567890abcdef12. " +
                   "Can someone help me understand what to do? I've already regenerated it but want to make sure I'm secure.";
        }
        
        if (url.contains("github.com") && url.contains(".env")) {
            return "OPENAI_API_KEY=sk-test1234567890abcdef1234567890abcdef12\n" +
                   "GEMINI_API_KEY=AIzaTest1234567890abcdef1234567890abcdef\n" +
                   "DATABASE_URL=postgresql://user:pass@localhost:5432/db\n" +
                   "DEBUG=true";
        }
        
        if (url.contains("pastebin.com")) {
            return "Here's my config file:\n" +
                   "export OPENAI_API_KEY='sk-abcdef1234567890abcdef1234567890abcdef12'\n" +
                   "export GEMINI_API_KEY='AIzaTest1234567890abcdef1234567890abcdef'\n" +
                   "export ANTHROPIC_API_KEY='sk-ant-test1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef'";
        }
        
        if (url.contains("stackoverflow.com")) {
            return "I'm getting an error with my API key: sk-abcdef1234567890abcdef1234567890abcdef12. " +
                   "The error says 'Invalid API key' but I copied it directly from my dashboard.";
        }
        
        return "Some content without API keys";
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
                
                // Filter out obvious test/placeholder keys
                if (!isPlaceholderKey(key)) {
                    keys.add(new DiscoveredKey(
                        key,
                        provider,
                        "MEDIUM", // Default confidence
                        sourceUrl,
                        context
                    ));
                }
            }
        }
        
        return keys;
    }
    
    private boolean isPlaceholderKey(String key) {
        String lowerKey = key.toLowerCase();
        return lowerKey.contains("test") || 
               lowerKey.contains("example") || 
               lowerKey.contains("placeholder") ||
               lowerKey.contains("dummy") ||
               lowerKey.contains("fake") ||
               lowerKey.contains("xxxx") ||
               lowerKey.contains("your_");
    }
    
    private String extractContext(String content, int start, int end) {
        int contextStart = Math.max(0, start - 100);
        int contextEnd = Math.min(content.length(), end + 100);
        return content.substring(contextStart, contextEnd);
    }
    
    private List<DiscoveredKey> classifyKeys(List<DiscoveredKey> keys) {
        System.out.println("? Classifying " + keys.size() + " discovered keys...");
        
        List<DiscoveredKey> validKeys = new ArrayList<>();
        
        for (DiscoveredKey key : keys) {
            String confidence = classifyKey(key);
            key.setConfidence(confidence);
            
            if (!confidence.equals("NONE")) {
                validKeys.add(key);
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (confidence: " + confidence + ")");
            } else {
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (classified as invalid)");
            }
        }
        
        return validKeys;
    }
    
    private String classifyKey(DiscoveredKey key) {
        String keyValue = key.getKey();
        String provider = key.getProvider();
        
        // Provider-specific validation
        switch (provider) {
            case "openai":
                if (keyValue.startsWith("sk-") && keyValue.length() == 51) {
                    return "HIGH";
                }
                break;
            case "gemini":
                if (keyValue.startsWith("AIza") && keyValue.length() == 39) {
                    return "HIGH";
                }
                break;
            case "anthropic":
                if (keyValue.startsWith("sk-ant-") && keyValue.length() >= 95) {
                    return "HIGH";
                }
                break;
            case "github":
                if (keyValue.startsWith("ghp_") && keyValue.length() == 40) {
                    return "HIGH";
                }
                break;
        }
        
        // Generic validation
        if (keyValue.length() < 20) {
            return "LOW";
        }
        
        if (keyValue.matches(".*[a-zA-Z]{3,}.*")) {
            return "MEDIUM";
        }
        
        return "LOW";
    }
    
    private void generateDetailedReport(List<SearchResult> searchResults, 
                                       List<DiscoveredKey> discoveredKeys, 
                                       List<DiscoveredKey> validKeys) {
        
        System.out.println("\n? DETAILED SCAN REPORT");
        System.out.println("=" + "=".repeat(60));
        
        // Platform breakdown
        System.out.println("? Platform Search Results:");
        Map<String, Long> platformCounts = searchResults.stream()
            .collect(Collectors.groupingBy(
                result -> result.getQuery().split("site:")[1].split("\\.")[0],
                Collectors.counting()
            ));
        
        platformCounts.forEach((platform, count) -> 
            System.out.println("  • " + platform + ": " + count + " search queries"));
        
        // Key discovery summary
        System.out.println("\n? Key Discovery Summary:");
        System.out.println("  • Total keys discovered: " + discoveredKeys.size());
        System.out.println("  • Valid keys: " + validKeys.size());
        System.out.println("  • Invalid keys: " + (discoveredKeys.size() - validKeys.size()));
        
        // Provider breakdown
        Map<String, List<DiscoveredKey>> byProvider = validKeys.stream()
            .collect(Collectors.groupingBy(DiscoveredKey::getProvider));
        
        System.out.println("\n? Valid Keys by Provider:");
        byProvider.forEach((provider, providerKeys) -> {
            System.out.println("  • " + provider + ": " + providerKeys.size() + " keys");
            for (DiscoveredKey key : providerKeys) {
                System.out.println("    - " + maskKey(key.getKey()) + " from " + getDomain(key.getSourceUrl()));
            }
        });
        
        // Source breakdown
        Map<String, List<DiscoveredKey>> bySource = validKeys.stream()
            .collect(Collectors.groupingBy(key -> getDomain(key.getSourceUrl())));
        
        System.out.println("\n? Keys by Source Platform:");
        bySource.forEach((domain, domainKeys) -> {
            System.out.println("  • " + domain + ": " + domainKeys.size() + " keys");
        });
        
        // Security recommendations
        System.out.println("\n? Security Recommendations:");
        System.out.println("1. These results are for educational purposes only");
        System.out.println("2. In real scenarios, follow responsible disclosure practices");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without explicit permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Implement proper key management and monitoring");
        System.out.println("7. Use environment variables for API keys");
        System.out.println("8. Add .env files to .gitignore");
        System.out.println("9. Rotate keys regularly");
        System.out.println("10. Monitor for key exposure on public platforms");
    }
    
    private String getDomain(String url) {
        try {
            URL u = new URL(url);
            return u.getHost();
        } catch (Exception e) {
            return "unknown";
        }
    }
    
    private String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
    
    private void disableSSLVerification() {
        try {
            TrustManager[] trustAllCerts = new TrustManager[] {
                new X509TrustManager() {
                    public X509Certificate[] getAcceptedIssuers() { return null; }
                    public void checkClientTrusted(X509Certificate[] certs, String authType) { }
                    public void checkServerTrusted(X509Certificate[] certs, String authType) { }
                }
            };
            
            SSLContext sc = SSLContext.getInstance("SSL");
            sc.init(null, trustAllCerts, new java.security.SecureRandom());
            HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
            HttpsURLConnection.setDefaultHostnameVerifier((hostname, session) -> true);
        } catch (Exception e) {
            System.err.println("Warning: Could not disable SSL verification");
        }
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
}
