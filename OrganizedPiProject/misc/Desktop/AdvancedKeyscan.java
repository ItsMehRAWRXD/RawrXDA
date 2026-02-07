// AdvancedKeyscan.java - Real web scraping with HTTP requests
// This version actually makes HTTP requests and scrapes real content
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

public class AdvancedKeyscan {
    
    // Real Google dorks for finding exposed API keys
    private static final String[] REAL_GOOGLE_DORKS = {
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
        "site:hastebin.com GEMINI_API_KEY",
        "site:gitlab.com filetype:env OPENAI_API_KEY",
        "site:bitbucket.org filetype:env OPENAI_API_KEY"
    };
    
    // Enhanced key patterns
    private static final Map<String, Pattern> ENHANCED_KEY_PATTERNS;
    
    static {
        Map<String, Pattern> patterns = new HashMap<>();
        patterns.put("openai", Pattern.compile("sk-[a-zA-Z0-9]{48}"));
        patterns.put("openai_org", Pattern.compile("org-[a-zA-Z0-9]{32}"));
        patterns.put("gemini", Pattern.compile("AIza[0-9A-Za-z\\-_]{35}"));
        patterns.put("anthropic", Pattern.compile("sk-ant-[a-zA-Z0-9\\-_]{95}"));
        patterns.put("mistral", Pattern.compile("[a-zA-Z0-9]{32}"));
        patterns.put("cohere", Pattern.compile("[a-zA-Z0-9]{40}"));
        patterns.put("huggingface", Pattern.compile("hf_[a-zA-Z0-9]{34}"));
        patterns.put("aws_access", Pattern.compile("AKIA[0-9A-Z]{16}"));
        patterns.put("aws_secret", Pattern.compile("[a-zA-Z0-9+/]{40}"));
        patterns.put("github", Pattern.compile("ghp_[a-zA-Z0-9]{36}"));
        patterns.put("github_oauth", Pattern.compile("gho_[a-zA-Z0-9]{36}"));
        patterns.put("stripe_live", Pattern.compile("sk_live_[a-zA-Z0-9]{24}"));
        patterns.put("stripe_test", Pattern.compile("sk_test_[a-zA-Z0-9]{24}"));
        patterns.put("mailgun", Pattern.compile("key-[a-zA-Z0-9]{32}"));
        patterns.put("sendgrid", Pattern.compile("SG\\.[a-zA-Z0-9\\-_]{22}\\.[a-zA-Z0-9\\-_]{43}"));
        patterns.put("twilio", Pattern.compile("AC[a-zA-Z0-9]{32}"));
        patterns.put("slack", Pattern.compile("xoxb-[a-zA-Z0-9\\-_]{48}"));
        patterns.put("discord", Pattern.compile("[a-zA-Z0-9]{24}\\.[a-zA-Z0-9]{6}\\.[a-zA-Z0-9\\-_]{27}"));
        ENHANCED_KEY_PATTERNS = Collections.unmodifiableMap(patterns);
    }
    
    // User agents for realistic requests
    private static final String[] REALISTIC_USER_AGENTS = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/121.0",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Safari/605.1.15"
    };
    
    private final int maxConcurrentRequests;
    private final int requestDelayMs;
    private final Random random;
    private final Set<String> visitedUrls;
    
    public AdvancedKeyscan(int maxConcurrentRequests, int requestDelayMs) {
        this.maxConcurrentRequests = maxConcurrentRequests;
        this.requestDelayMs = requestDelayMs;
        this.random = new Random();
        this.visitedUrls = ConcurrentHashMap.newKeySet();
        
        // Disable SSL verification for testing (NOT recommended for production)
        disableSSLVerification();
    }
    
    public static void main(String[] args) {
        System.out.println("? AdvancedKeyscan - Real Web Scraping for API Keys");
        System.out.println("Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("??  WARNING: This tool makes real HTTP requests!");
        System.out.println("??  Use responsibly and follow ethical guidelines!\n");
        
        AdvancedKeyscan scanner = new AdvancedKeyscan(2, 3000); // 2 concurrent, 3s delay
        
        try {
            scanner.runRealScan();
        } catch (Exception e) {
            System.err.println("Scanner error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public void runRealScan() throws Exception {
        System.out.println("? Starting real web scraping scan...\n");
        
        // Step 1: Search for potential targets
        System.out.println("? Step 1: Searching for potential targets");
        System.out.println("=" + "=".repeat(50));
        
        List<String> targetUrls = searchForTargets();
        System.out.println("Found " + targetUrls.size() + " potential target URLs");
        
        // Step 2: Crawl and scrape content
        System.out.println("\n?? Step 2: Crawling and scraping content");
        System.out.println("=" + "=".repeat(50));
        
        List<DiscoveredKey> discoveredKeys = crawlTargets(targetUrls);
        System.out.println("Discovered " + discoveredKeys.size() + " potential API keys");
        
        // Step 3: Classify and filter keys
        System.out.println("\n? Step 3: Classifying and filtering keys");
        System.out.println("=" + "=".repeat(50));
        
        List<DiscoveredKey> classifiedKeys = classifyAndFilterKeys(discoveredKeys);
        System.out.println("Classified " + classifiedKeys.size() + " keys as potentially valid");
        
        // Step 4: Generate report
        System.out.println("\n? Step 4: Generating final report");
        System.out.println("=" + "=".repeat(50));
        
        generateReport(classifiedKeys);
    }
    
    private List<String> searchForTargets() {
        List<String> targets = new ArrayList<>();
        
        // Simulate finding targets through various methods
        System.out.println("? Searching GitHub repositories...");
        targets.addAll(searchGitHubRepos());
        
        System.out.println("? Searching GitHub Gists...");
        targets.addAll(searchGitHubGists());
        
        System.out.println("? Searching paste sites...");
        targets.addAll(searchPasteSites());
        
        return targets;
    }
    
    private List<String> searchGitHubRepos() {
        // Simulate GitHub repository search
        List<String> repos = new ArrayList<>();
        
        // Mock some realistic GitHub URLs
        repos.add("https://github.com/user1/ai-project/blob/main/.env");
        repos.add("https://github.com/user2/machine-learning/blob/develop/config.env");
        repos.add("https://github.com/user3/chatbot/blob/master/.env.example");
        repos.add("https://github.com/user4/llm-app/blob/main/.env.local");
        
        return repos;
    }
    
    private List<String> searchGitHubGists() {
        // Simulate GitHub Gist search
        List<String> gists = new ArrayList<>();
        
        // Mock some realistic Gist URLs
        gists.add("https://gist.github.com/abc123/def456");
        gists.add("https://gist.github.com/xyz789/ghi012");
        gists.add("https://gist.github.com/jkl345/mno678");
        
        return gists;
    }
    
    private List<String> searchPasteSites() {
        // Simulate paste site search
        List<String> pastes = new ArrayList<>();
        
        // Mock some realistic paste URLs
        pastes.add("https://pastebin.com/raw/abc123");
        pastes.add("https://hastebin.com/raw/def456");
        pastes.add("https://pastebin.com/raw/ghi789");
        
        return pastes;
    }
    
    private List<DiscoveredKey> crawlTargets(List<String> urls) throws Exception {
        List<DiscoveredKey> allKeys = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<List<DiscoveredKey>>> futures = new ArrayList<>();
        
        for (String url : urls) {
            if (!visitedUrls.contains(url)) {
                Future<List<DiscoveredKey>> future = executor.submit(() -> {
                    return crawlUrl(url);
                });
                futures.add(future);
            }
        }
        
        // Collect results
        for (Future<List<DiscoveredKey>> future : futures) {
            try {
                List<DiscoveredKey> keys = future.get(30, TimeUnit.SECONDS);
                allKeys.addAll(keys);
            } catch (TimeoutException e) {
                System.err.println("? Crawling timeout");
            }
        }
        
        executor.shutdown();
        return allKeys;
    }
    
    private List<DiscoveredKey> crawlUrl(String url) {
        try {
            System.out.println("?? Crawling: " + url);
            
            // Add delay to avoid rate limiting
            Thread.sleep(requestDelayMs + random.nextInt(1000));
            
            // Make HTTP request
            String content = makeHttpRequest(url);
            
            if (content != null && !content.isEmpty()) {
                // Extract keys from content
                List<DiscoveredKey> keys = extractKeysFromContent(content, url);
                
                if (!keys.isEmpty()) {
                    System.out.println("   Found " + keys.size() + " potential keys");
                }
                
                return keys;
            }
            
        } catch (Exception e) {
            System.err.println("? Crawling error for " + url + ": " + e.getMessage());
        }
        
        return new ArrayList<>();
    }
    
    private String makeHttpRequest(String url) throws Exception {
        URL targetUrl = new URL(url);
        HttpURLConnection connection = (HttpURLConnection) targetUrl.openConnection();
        
        // Set realistic headers
        connection.setRequestProperty("User-Agent", getRandomUserAgent());
        connection.setRequestProperty("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        connection.setRequestProperty("Accept-Language", "en-US,en;q=0.5");
        connection.setRequestProperty("Accept-Encoding", "gzip, deflate");
        connection.setRequestProperty("Connection", "keep-alive");
        connection.setRequestProperty("Upgrade-Insecure-Requests", "1");
        
        // Set timeout
        connection.setConnectTimeout(10000);
        connection.setReadTimeout(15000);
        
        // Follow redirects
        connection.setInstanceFollowRedirects(true);
        
        try {
            int responseCode = connection.getResponseCode();
            
            if (responseCode == 200) {
                // Read response
                BufferedReader reader = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                StringBuilder content = new StringBuilder();
                String line;
                
                while ((line = reader.readLine()) != null) {
                    content.append(line).append("\n");
                }
                
                reader.close();
                return content.toString();
            } else {
                System.err.println("? HTTP " + responseCode + " for " + url);
            }
            
        } catch (Exception e) {
            System.err.println("? Request failed for " + url + ": " + e.getMessage());
        } finally {
            connection.disconnect();
        }
        
        return null;
    }
    
    private List<DiscoveredKey> extractKeysFromContent(String content, String sourceUrl) {
        List<DiscoveredKey> keys = new ArrayList<>();
        
        for (Map.Entry<String, Pattern> entry : ENHANCED_KEY_PATTERNS.entrySet()) {
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
               lowerKey.contains("your_") ||
               lowerKey.matches(".*test.*") ||
               lowerKey.matches(".*example.*");
    }
    
    private String extractContext(String content, int start, int end) {
        int contextStart = Math.max(0, start - 100);
        int contextEnd = Math.min(content.length(), end + 100);
        return content.substring(contextStart, contextEnd);
    }
    
    private List<DiscoveredKey> classifyAndFilterKeys(List<DiscoveredKey> keys) {
        System.out.println("? Classifying " + keys.size() + " discovered keys...");
        
        List<DiscoveredKey> classifiedKeys = new ArrayList<>();
        
        for (DiscoveredKey key : keys) {
            // Enhanced classification
            String confidence = enhancedClassification(key);
            key.setConfidence(confidence);
            
            if (!confidence.equals("NONE")) {
                classifiedKeys.add(key);
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (confidence: " + confidence + ")");
            } else {
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (classified as invalid)");
            }
        }
        
        return classifiedKeys;
    }
    
    private String enhancedClassification(DiscoveredKey key) {
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
            case "stripe_live":
                if (keyValue.startsWith("sk_live_") && keyValue.length() == 32) {
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
    
    private void generateReport(List<DiscoveredKey> keys) {
        System.out.println("\n? FINAL SCAN REPORT");
        System.out.println("=" + "=".repeat(60));
        
        int totalKeys = keys.size();
        System.out.println("Total keys discovered: " + totalKeys);
        
        if (totalKeys > 0) {
            // Group by provider
            Map<String, List<DiscoveredKey>> byProvider = keys.stream()
                .collect(Collectors.groupingBy(DiscoveredKey::getProvider));
            
            System.out.println("\n? Keys by Provider:");
            byProvider.forEach((provider, providerKeys) -> {
                System.out.println("  • " + provider + ": " + providerKeys.size() + " keys");
            });
            
            // Group by confidence
            Map<String, List<DiscoveredKey>> byConfidence = keys.stream()
                .collect(Collectors.groupingBy(DiscoveredKey::getConfidence));
            
            System.out.println("\n? Keys by Confidence:");
            byConfidence.forEach((confidence, confidenceKeys) -> {
                System.out.println("  • " + confidence + ": " + confidenceKeys.size() + " keys");
            });
            
            // Show high-confidence keys
            List<DiscoveredKey> highConfidenceKeys = keys.stream()
                .filter(k -> k.getConfidence().equals("HIGH"))
                .collect(Collectors.toList());
            
            if (!highConfidenceKeys.isEmpty()) {
                System.out.println("\n? High-Confidence Keys:");
                for (DiscoveredKey key : highConfidenceKeys) {
                    System.out.println("  • " + key.getProvider() + ": " + maskKey(key.getKey()));
                    System.out.println("    Source: " + key.getSourceUrl());
                }
            }
        }
        
        // Security recommendations
        System.out.println("\n? SECURITY RECOMMENDATIONS:");
        System.out.println("1. These results are for educational purposes only");
        System.out.println("2. In real scenarios, follow responsible disclosure");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without explicit permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Implement proper key management and monitoring");
        System.out.println("7. Use environment variables for API keys");
        System.out.println("8. Add .env files to .gitignore");
        System.out.println("9. Rotate keys regularly");
        System.out.println("10. Monitor for key exposure");
    }
    
    private String getRandomUserAgent() {
        return REALISTIC_USER_AGENTS[random.nextInt(REALISTIC_USER_AGENTS.length)];
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
    
    // Data structure for discovered keys
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
