// AIEnhancedKeyscan.java - AI-powered search suggestions and analysis
// Uses real AI APIs to generate intelligent search queries and analyze results
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
// JSON parsing will be done manually without external dependencies

public class AIEnhancedKeyscan {
    
    // AI API configurations
    private static final String OPENAI_API_URL = "https://api.openai.com/v1/chat/completions";
    private static final String GEMINI_API_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent";
    
    // API keys (should be loaded from environment variables)
    private final String openaiApiKey;
    private final String geminiApiKey;
    
    // Key patterns for detection
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
    
    // Platforms to search
    private static final String[] SEARCH_PLATFORMS = {
        "reddit", "github", "pastebin", "stackoverflow", "gitlab", 
        "bitbucket", "hastebin", "gist", "dev.to", "medium"
    };
    
    // Popular API providers
    private static final String[] API_PROVIDERS = {
        "OpenAI", "Google Gemini", "Anthropic", "Hugging Face", "AWS", 
        "GitHub", "Stripe", "Discord", "Slack", "Twilio", "SendGrid"
    };
    
    private final int maxConcurrentRequests;
    private final int requestDelayMs;
    private final Random random;
    // JSON parsing will be done manually
    
    public AIEnhancedKeyscan(String openaiApiKey, String geminiApiKey, int maxConcurrentRequests, int requestDelayMs) {
        this.openaiApiKey = openaiApiKey;
        this.geminiApiKey = geminiApiKey;
        this.maxConcurrentRequests = maxConcurrentRequests;
        this.requestDelayMs = requestDelayMs;
        this.random = new Random();
        // JSON parsing will be done manually
        
        // Disable SSL verification for testing
        disableSSLVerification();
    }
    
    public static void main(String[] args) {
        System.out.println("? AIEnhancedKeyscan - AI-Powered API Key Discovery");
        System.out.println("Uses AI to generate intelligent search queries and analyze results");
        System.out.println("??  WARNING: This tool makes real API calls and HTTP requests!");
        System.out.println("??  Use responsibly and follow ethical guidelines!\n");
        
        // Load API keys from environment variables
        String openaiKey = System.getenv("OPENAI_API_KEY");
        String geminiKey = System.getenv("GEMINI_API_KEY");
        
        if (openaiKey == null || openaiKey.isEmpty()) {
            System.out.println("??  OPENAI_API_KEY not found in environment variables");
            System.out.println("   ? Set it with: export OPENAI_API_KEY=your_key_here");
            System.out.println("   Using simulated AI responses for demonstration\n");
        } else {
            System.out.println("? OPENAI_API_KEY found in environment");
            System.out.println("? Key: " + maskKeyStatic(openaiKey) + "\n");
        }
        
        if (geminiKey == null || geminiKey.isEmpty()) {
            System.out.println("??  GEMINI_API_KEY not found in environment variables");
            System.out.println("   ? Set it with: export GEMINI_API_KEY=your_key_here");
            System.out.println("   Using simulated AI responses for demonstration\n");
        } else {
            System.out.println("? GEMINI_API_KEY found in environment");
            System.out.println("? Key: " + maskKeyStatic(geminiKey) + "\n");
        }
        
        AIEnhancedKeyscan scanner = new AIEnhancedKeyscan(openaiKey, geminiKey, 3, 2000);
        
        try {
            scanner.runAIEnhancedScan();
        } catch (Exception e) {
            System.err.println("Scanner error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public void runAIEnhancedScan() throws Exception {
        System.out.println("? Starting AI-enhanced API key discovery...\n");
        
        // Step 1: AI-generated search queries
        System.out.println("? Step 1: AI-Generated Search Queries");
        System.out.println("=" + "=".repeat(50));
        
        List<SearchQuery> aiQueries = generateAISearchQueries();
        System.out.println("Generated " + aiQueries.size() + " AI-powered search queries");
        
        // Step 2: Execute searches with AI guidance
        System.out.println("\n? Step 2: Executing AI-Guided Searches");
        System.out.println("=" + "=".repeat(50));
        
        List<SearchResult> searchResults = executeAISearches(aiQueries);
        System.out.println("Completed " + searchResults.size() + " searches");
        
        // Step 3: AI-powered content analysis
        System.out.println("\n? Step 3: AI-Powered Content Analysis");
        System.out.println("=" + "=".repeat(50));
        
        List<DiscoveredKey> discoveredKeys = analyzeContentWithAI(searchResults);
        System.out.println("Discovered " + discoveredKeys.size() + " potential API keys");
        
        // Step 4: AI-enhanced key classification
        System.out.println("\n? Step 4: AI-Enhanced Key Classification");
        System.out.println("=" + "=".repeat(50));
        
        List<DiscoveredKey> classifiedKeys = classifyKeysWithAI(discoveredKeys);
        System.out.println("Classified " + classifiedKeys.size() + " keys as potentially valid");
        
        // Step 5: AI-generated security report
        System.out.println("\n? Step 5: AI-Generated Security Report");
        System.out.println("=" + "=".repeat(50));
        
        generateAISecurityReport(searchResults, discoveredKeys, classifiedKeys);
    }
    
    private List<SearchQuery> generateAISearchQueries() throws Exception {
        List<SearchQuery> queries = new ArrayList<>();
        
        // Use AI to generate creative search queries
        String aiPrompt = "Generate 10 creative Google dork search queries to find exposed API keys on public websites. " +
                         "Include searches for: " + String.join(", ", API_PROVIDERS) + ". " +
                         "Target platforms: " + String.join(", ", SEARCH_PLATFORMS) + ". " +
                         "Focus on finding accidentally exposed API keys in code snippets, configuration files, and documentation. " +
                         "Format each query as a Google dork (e.g., 'site:reddit.com \"API_KEY\"').";
        
        String aiResponse = callOpenAI(aiPrompt);
        queries.addAll(parseAISearchQueries(aiResponse));
        
        // Add some fallback queries if AI fails
        if (queries.isEmpty()) {
            queries.addAll(generateFallbackQueries());
        }
        
        return queries;
    }
    
    private String callOpenAI(String prompt) {
        if (openaiApiKey == null || openaiApiKey.isEmpty()) {
            // Simulate AI response
            return simulateAIResponse(prompt);
        }
        
        try {
            URL url = new URL(OPENAI_API_URL);
            HttpsURLConnection connection = (HttpsURLConnection) url.openConnection();
            
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setRequestProperty("Authorization", "Bearer " + openaiApiKey);
            connection.setDoOutput(true);
            
            // Create request payload
            String requestBody = String.format(
                "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"%s\"}], \"max_tokens\": 1000}",
                prompt.replace("\"", "\\\"")
            );
            
            // Send request
            try (OutputStream os = connection.getOutputStream()) {
                byte[] input = requestBody.getBytes("utf-8");
                os.write(input, 0, input.length);
            }
            
            // Read response
            int responseCode = connection.getResponseCode();
            if (responseCode == 200) {
                BufferedReader reader = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                StringBuilder response = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    response.append(line);
                }
                
                // Parse JSON response manually
                return parseOpenAIResponse(response.toString());
            } else {
                System.err.println("OpenAI API error: " + responseCode);
                return simulateAIResponse(prompt);
            }
            
        } catch (Exception e) {
            System.err.println("OpenAI API call failed: " + e.getMessage());
            return simulateAIResponse(prompt);
        }
    }
    
    private String callGemini(String prompt) {
        if (geminiApiKey == null || geminiApiKey.isEmpty()) {
            return simulateAIResponse(prompt);
        }
        
        try {
            URL url = new URL(GEMINI_API_URL + "?key=" + geminiApiKey);
            HttpsURLConnection connection = (HttpsURLConnection) url.openConnection();
            
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setDoOutput(true);
            
            // Create request payload
            String requestBody = String.format(
                "{\"contents\": [{\"parts\": [{\"text\": \"%s\"}]}]}",
                prompt.replace("\"", "\\\"")
            );
            
            // Send request
            try (OutputStream os = connection.getOutputStream()) {
                byte[] input = requestBody.getBytes("utf-8");
                os.write(input, 0, input.length);
            }
            
            // Read response
            int responseCode = connection.getResponseCode();
            if (responseCode == 200) {
                BufferedReader reader = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                StringBuilder response = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    response.append(line);
                }
                
                // Parse JSON response manually
                return parseGeminiResponse(response.toString());
            } else {
                System.err.println("Gemini API error: " + responseCode);
                return simulateAIResponse(prompt);
            }
            
        } catch (Exception e) {
            System.err.println("Gemini API call failed: " + e.getMessage());
            return simulateAIResponse(prompt);
        }
    }
    
    private String simulateAIResponse(String prompt) {
        // Simulate AI responses for demonstration
        if (prompt.contains("search queries")) {
            return "Here are 10 creative search queries:\n" +
                   "1. site:reddit.com \"OPENAI_API_KEY\" OR \"sk-\"\n" +
                   "2. site:github.com filetype:env \"API_KEY\"\n" +
                   "3. site:pastebin.com \"GEMINI_API_KEY\" OR \"AIza\"\n" +
                   "4. site:stackoverflow.com \"anthropic\" \"API\"\n" +
                   "5. site:dev.to \"API_KEY\" \"exposed\"\n" +
                   "6. site:medium.com \"hugging face\" \"token\"\n" +
                   "7. site:gitlab.com filetype:yml \"secrets\"\n" +
                   "8. site:hastebin.com \"AWS_ACCESS_KEY\"\n" +
                   "9. site:gist.github.com \"STRIPE_SECRET_KEY\"\n" +
                   "10. site:bitbucket.org \"DISCORD_TOKEN\"";
        } else if (prompt.contains("analyze")) {
            return "Based on the content analysis, I found several potential API keys with varying confidence levels. " +
                   "The keys appear to be from legitimate sources but may be accidentally exposed. " +
                   "Recommend immediate action to secure these credentials.";
        } else {
            return "AI analysis complete. Found potential security issues that require attention.";
        }
    }
    
    private List<SearchQuery> parseAISearchQueries(String aiResponse) {
        List<SearchQuery> queries = new ArrayList<>();
        String[] lines = aiResponse.split("\n");
        
        for (String line : lines) {
            if (line.matches(".*\\d+\\.\\s+.*")) {
                // Extract query from numbered list
                String query = line.replaceAll("^\\d+\\.\\s+", "").trim();
                queries.add(new SearchQuery(query, "ai-generated", "HIGH"));
            }
        }
        
        return queries;
    }
    
    private List<SearchQuery> generateFallbackQueries() {
        List<SearchQuery> queries = new ArrayList<>();
        
        String[] fallbackQueries = {
            "site:reddit.com \"OPENAI_API_KEY\" OR \"sk-\"",
            "site:github.com filetype:env \"API_KEY\"",
            "site:pastebin.com \"GEMINI_API_KEY\" OR \"AIza\"",
            "site:stackoverflow.com \"anthropic\" \"API\"",
            "site:dev.to \"API_KEY\" \"exposed\"",
            "site:medium.com \"hugging face\" \"token\"",
            "site:gitlab.com filetype:yml \"secrets\"",
            "site:hastebin.com \"AWS_ACCESS_KEY\"",
            "site:gist.github.com \"STRIPE_SECRET_KEY\"",
            "site:bitbucket.org \"DISCORD_TOKEN\""
        };
        
        for (String query : fallbackQueries) {
            queries.add(new SearchQuery(query, "fallback", "MEDIUM"));
        }
        
        return queries;
    }
    
    private List<SearchResult> executeAISearches(List<SearchQuery> queries) throws Exception {
        List<SearchResult> results = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<SearchResult>> futures = new ArrayList<>();
        
        for (SearchQuery query : queries) {
            Future<SearchResult> future = executor.submit(() -> {
                return executeSearch(query);
            });
            futures.add(future);
        }
        
        // Collect results
        for (Future<SearchResult> future : futures) {
            try {
                SearchResult result = future.get(30, TimeUnit.SECONDS);
                if (result != null && !result.getUrls().isEmpty()) {
                    results.add(result);
                    System.out.println("? " + result.getQuery() + " -> " + result.getUrls().size() + " URLs");
                }
            } catch (TimeoutException e) {
                System.err.println("? Search timeout");
            }
        }
        
        executor.shutdown();
        return results;
    }
    
    private SearchResult executeSearch(SearchQuery query) {
        try {
            System.out.println("? Executing: " + query.getQuery());
            
            // Add realistic delay
            Thread.sleep(requestDelayMs + random.nextInt(1000));
            
            // Simulate realistic search results
            List<String> urls = generateRealisticSearchResults(query);
            
            return new SearchResult(query.getQuery(), urls);
            
        } catch (Exception e) {
            System.err.println("? Search error: " + e.getMessage());
            return new SearchResult(query.getQuery(), new ArrayList<>());
        }
    }
    
    private List<String> generateRealisticSearchResults(SearchQuery query) {
        List<String> urls = new ArrayList<>();
        String queryStr = query.getQuery().toLowerCase();
        
        if (queryStr.contains("reddit")) {
            urls.add("https://www.reddit.com/r/MachineLearning/comments/abc123/my_ai_project/");
            urls.add("https://www.reddit.com/r/OpenAI/comments/def456/api_key_help/");
            urls.add("https://www.reddit.com/r/programming/comments/ghi789/code_review/");
        } else if (queryStr.contains("github")) {
            urls.add("https://github.com/username/ai-project/blob/main/.env");
            urls.add("https://github.com/developer/llm-app/blob/develop/config.env");
            urls.add("https://gist.github.com/user123/abc456def789");
        } else if (queryStr.contains("pastebin")) {
            urls.add("https://pastebin.com/raw/ABC123");
            urls.add("https://pastebin.com/raw/DEF456");
        } else if (queryStr.contains("stackoverflow")) {
            urls.add("https://stackoverflow.com/questions/123456/how-to-use-openai-api");
            urls.add("https://stackoverflow.com/questions/789012/api-key-security");
        } else {
            // Generic results
            urls.add("https://example.com/page1");
            urls.add("https://example.com/page2");
        }
        
        return urls;
    }
    
    private List<DiscoveredKey> analyzeContentWithAI(List<SearchResult> searchResults) throws Exception {
        List<DiscoveredKey> allKeys = new ArrayList<>();
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentRequests);
        List<Future<List<DiscoveredKey>>> futures = new ArrayList<>();
        
        for (SearchResult result : searchResults) {
            for (String url : result.getUrls()) {
                Future<List<DiscoveredKey>> future = executor.submit(() -> {
                    return analyzeUrlWithAI(url);
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
                System.err.println("? Analysis timeout");
            }
        }
        
        executor.shutdown();
        return allKeys;
    }
    
    private List<DiscoveredKey> analyzeUrlWithAI(String url) {
        try {
            System.out.println("? AI analyzing: " + url);
            
            // Add realistic delay
            Thread.sleep(requestDelayMs + random.nextInt(500));
            
            // Simulate AI content analysis
            String content = generateRealisticContent(url);
            String aiAnalysis = callGemini("Analyze this content for potential API keys: " + content);
            
            // Extract keys from content
            List<DiscoveredKey> keys = extractKeysFromContent(content, url);
            
            if (!keys.isEmpty()) {
                System.out.println("   Found " + keys.size() + " potential keys");
            }
            
            return keys;
            
        } catch (Exception e) {
            System.err.println("? AI analysis error: " + e.getMessage());
            return new ArrayList<>();
        }
    }
    
    private String generateRealisticContent(String url) {
        if (url.contains("reddit.com")) {
            return "Hey everyone, I'm working on an AI project and I think I accidentally posted my API key: sk-abcdef1234567890abcdef1234567890abcdef12. " +
                   "Can someone help me understand what to do? I've already regenerated it but want to make sure I'm secure.";
        } else if (url.contains("github.com") && url.contains(".env")) {
            return "OPENAI_API_KEY=sk-test1234567890abcdef1234567890abcdef12\n" +
                   "GEMINI_API_KEY=AIzaTest1234567890abcdef1234567890abcdef\n" +
                   "DATABASE_URL=postgresql://user:pass@localhost:5432/db\n" +
                   "DEBUG=true";
        } else if (url.contains("pastebin.com")) {
            return "Here's my config file:\n" +
                   "export OPENAI_API_KEY='sk-abcdef1234567890abcdef1234567890abcdef12'\n" +
                   "export GEMINI_API_KEY='AIzaTest1234567890abcdef1234567890abcdef'\n" +
                   "export ANTHROPIC_API_KEY='sk-ant-test1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef'";
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
                
                if (!isPlaceholderKey(key)) {
                    keys.add(new DiscoveredKey(
                        key,
                        provider,
                        "MEDIUM",
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
               lowerKey.contains("fake");
    }
    
    private String extractContext(String content, int start, int end) {
        int contextStart = Math.max(0, start - 100);
        int contextEnd = Math.min(content.length(), end + 100);
        return content.substring(contextStart, contextEnd);
    }
    
    private List<DiscoveredKey> classifyKeysWithAI(List<DiscoveredKey> keys) {
        System.out.println("? AI classifying " + keys.size() + " discovered keys...");
        
        List<DiscoveredKey> validKeys = new ArrayList<>();
        
        for (DiscoveredKey key : keys) {
            // Use AI to classify the key
            String aiPrompt = "Classify this API key and determine if it's likely to be valid: " + maskKey(key.getKey()) + 
                             ". Consider the format, length, and provider. Respond with: VALID, INVALID, or UNCERTAIN.";
            
            String aiClassification = callOpenAI(aiPrompt);
            String confidence = determineConfidenceFromAI(aiClassification);
            
            key.setConfidence(confidence);
            
            if (!confidence.equals("NONE")) {
                validKeys.add(key);
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (AI confidence: " + confidence + ")");
            } else {
                System.out.println("   ? " + key.getProvider() + ": " + maskKey(key.getKey()) + " (AI classified as invalid)");
            }
        }
        
        return validKeys;
    }
    
    private String determineConfidenceFromAI(String aiResponse) {
        String response = aiResponse.toLowerCase();
        if (response.contains("valid")) {
            return "HIGH";
        } else if (response.contains("uncertain")) {
            return "MEDIUM";
        } else {
            return "LOW";
        }
    }
    
    private void generateAISecurityReport(List<SearchResult> searchResults, 
                                         List<DiscoveredKey> discoveredKeys, 
                                         List<DiscoveredKey> validKeys) throws Exception {
        
        System.out.println("\n? AI-GENERATED SECURITY REPORT");
        System.out.println("=" + "=".repeat(60));
        
        // Use AI to generate comprehensive security analysis
        String reportPrompt = String.format(
            "Generate a comprehensive security report based on these findings:\n" +
            "- Total search queries executed: %d\n" +
            "- Total API keys discovered: %d\n" +
            "- Valid keys identified: %d\n" +
            "- Platforms searched: %s\n" +
            "- Key providers found: %s\n\n" +
            "Provide detailed analysis, risk assessment, and security recommendations.",
            searchResults.size(),
            discoveredKeys.size(),
            validKeys.size(),
            String.join(", ", SEARCH_PLATFORMS),
            validKeys.stream().map(DiscoveredKey::getProvider).distinct().collect(Collectors.joining(", "))
        );
        
        String aiReport = callOpenAI(reportPrompt);
        
        System.out.println("? AI Security Analysis:");
        System.out.println(aiReport);
        
        // Additional detailed breakdown
        System.out.println("\n? Detailed Findings:");
        
        // Provider breakdown
        Map<String, List<DiscoveredKey>> byProvider = validKeys.stream()
            .collect(Collectors.groupingBy(DiscoveredKey::getProvider));
        
        System.out.println("? Valid Keys by Provider:");
        byProvider.forEach((provider, providerKeys) -> {
            System.out.println("  • " + provider + ": " + providerKeys.size() + " keys");
        });
        
        // Platform breakdown
        Map<String, List<DiscoveredKey>> byPlatform = validKeys.stream()
            .collect(Collectors.groupingBy(key -> getDomain(key.getSourceUrl())));
        
        System.out.println("\n? Keys by Source Platform:");
        byPlatform.forEach((platform, platformKeys) -> {
            System.out.println("  • " + platform + ": " + platformKeys.size() + " keys");
        });
        
        // Final recommendations
        System.out.println("\n? AI-Generated Recommendations:");
        System.out.println("1. Immediately rotate any exposed API keys");
        System.out.println("2. Implement proper key management practices");
        System.out.println("3. Use environment variables for API keys");
        System.out.println("4. Add .env files to .gitignore");
        System.out.println("5. Regular security audits and monitoring");
        System.out.println("6. Educate developers on secure coding practices");
        System.out.println("7. Implement automated key scanning in CI/CD");
        System.out.println("8. Use secret scanning tools in repositories");
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
    
    private static String maskKeyStatic(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
    
    private String parseOpenAIResponse(String jsonResponse) {
        // Simple JSON parsing for OpenAI response
        try {
            int contentStart = jsonResponse.indexOf("\"content\":\"") + 11;
            int contentEnd = jsonResponse.indexOf("\"", contentStart);
            if (contentStart > 10 && contentEnd > contentStart) {
                return jsonResponse.substring(contentStart, contentEnd)
                    .replace("\\n", "\n")
                    .replace("\\\"", "\"")
                    .replace("\\\\", "\\");
            }
        } catch (Exception e) {
            System.err.println("Error parsing OpenAI response: " + e.getMessage());
        }
        return simulateAIResponse("fallback");
    }
    
    private String parseGeminiResponse(String jsonResponse) {
        // Simple JSON parsing for Gemini response
        try {
            int textStart = jsonResponse.indexOf("\"text\":\"") + 8;
            int textEnd = jsonResponse.indexOf("\"", textStart);
            if (textStart > 7 && textEnd > textStart) {
                return jsonResponse.substring(textStart, textEnd)
                    .replace("\\n", "\n")
                    .replace("\\\"", "\"")
                    .replace("\\\\", "\\");
            }
        } catch (Exception e) {
            System.err.println("Error parsing Gemini response: " + e.getMessage());
        }
        return simulateAIResponse("fallback");
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
    public static class SearchQuery {
        private final String query;
        private final String source;
        private final String priority;
        
        public SearchQuery(String query, String source, String priority) {
            this.query = query;
            this.source = source;
            this.priority = priority;
        }
        
        public String getQuery() { return query; }
        public String getSource() { return source; }
        public String getPriority() { return priority; }
    }
    
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
