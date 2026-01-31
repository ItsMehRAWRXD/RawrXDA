// AIKeyClassifier.java - AI-powered key classification
// Based on Keyscan methodology using LLM for key validation
// This simulates the AI classification step from the Keyscan article

import java.util.*;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class AIKeyClassifier {
    
    // Simulated LLM responses for key classification
    private static final Map<String, ClassificationResult> MOCK_LLM_RESPONSES = Map.of(
        "sk-abcdef1234567890abcdef1234567890abcdef12", 
        new ClassificationResult("HIGH", "openai", "Valid OpenAI API key format"),
        
        "AIzaTest1234567890abcdef1234567890abcdef",
        new ClassificationResult("MEDIUM", "gemini", "Potential Google API key"),
        
        "sk-ant-test1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
        new ClassificationResult("HIGH", "anthropic", "Valid Anthropic API key format"),
        
        "hf_test1234567890abcdef1234567890abcdef",
        new ClassificationResult("MEDIUM", "huggingface", "Potential Hugging Face token"),
        
        "test-key-12345",
        new ClassificationResult("LOW", "unknown", "Generic test key pattern"),
        
        "placeholder-key",
        new ClassificationResult("NONE", "unknown", "Obvious placeholder value")
    );
    
    public static void main(String[] args) {
        System.out.println("? AI Key Classifier - Simulating LLM-based Key Analysis");
        System.out.println("Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510\n");
        
        AIKeyClassifier classifier = new AIKeyClassifier();
        
        // Test keys from your SimpleProviderTest
        String[] testKeys = {
            "sk-abcdef1234567890abcdef1234567890abcdef12",
            "sk-1234567890abcdef1234567890abcdef12345678", 
            "sk-abcdefabcdefabcdefabcdefabcdefabcdef12",
            "AIzaTest1234567890abcdef1234567890abcdef",
            "test-key-12345",
            "placeholder-key"
        };
        
        System.out.println("? Analyzing keys with AI classification...\n");
        
        for (String key : testKeys) {
            ClassificationResult result = classifier.classifyKey(key);
            System.out.println("Key: " + classifier.maskKey(key));
            System.out.println("  Provider: " + result.getProvider());
            System.out.println("  Confidence: " + result.getConfidence());
            System.out.println("  Analysis: " + result.getAnalysis());
            System.out.println("  Valid: " + (result.getConfidence().equals("HIGH") || result.getConfidence().equals("MEDIUM")));
            System.out.println();
        }
        
        // Simulate batch processing
        System.out.println("? Batch Processing Results:");
        classifier.processBatch(testKeys);
    }
    
    public ClassificationResult classifyKey(String key) {
        // In real implementation, this would call an LLM (Ollama, OpenAI, etc.)
        // For simulation, we'll use pattern matching and mock responses
        
        // Check if we have a mock response
        if (MOCK_LLM_RESPONSES.containsKey(key)) {
            return MOCK_LLM_RESPONSES.get(key);
        }
        
        // Pattern-based classification as fallback
        return classifyByPattern(key);
    }
    
    private ClassificationResult classifyByPattern(String key) {
        if (key.startsWith("sk-") && key.length() >= 40) {
            return new ClassificationResult("HIGH", "openai", "OpenAI API key pattern detected");
        }
        
        if (key.startsWith("AIza") && key.length() >= 35) {
            return new ClassificationResult("MEDIUM", "gemini", "Google API key pattern detected");
        }
        
        if (key.startsWith("sk-ant-") && key.length() >= 95) {
            return new ClassificationResult("HIGH", "anthropic", "Anthropic API key pattern detected");
        }
        
        if (key.startsWith("hf_") && key.length() >= 34) {
            return new ClassificationResult("MEDIUM", "huggingface", "Hugging Face token pattern detected");
        }
        
        if (key.contains("test") || key.contains("example") || key.contains("placeholder")) {
            return new ClassificationResult("NONE", "unknown", "Test/placeholder key detected");
        }
        
        return new ClassificationResult("LOW", "unknown", "Unknown key format");
    }
    
    public void processBatch(String[] keys) {
        Map<String, Integer> providerCounts = new HashMap<>();
        Map<String, Integer> confidenceCounts = new HashMap<>();
        List<String> validKeys = new ArrayList<>();
        
        for (String key : keys) {
            ClassificationResult result = classifyKey(key);
            
            providerCounts.merge(result.getProvider(), 1, Integer::sum);
            confidenceCounts.merge(result.getConfidence(), 1, Integer::sum);
            
            if (result.getConfidence().equals("HIGH") || result.getConfidence().equals("MEDIUM")) {
                validKeys.add(key);
            }
        }
        
        System.out.println("? Classification Summary:");
        System.out.println("Total keys analyzed: " + keys.length);
        System.out.println("Valid keys found: " + validKeys.size());
        
        System.out.println("\nProvider breakdown:");
        providerCounts.forEach((provider, count) -> 
            System.out.println("  • " + provider + ": " + count));
        
        System.out.println("\nConfidence breakdown:");
        confidenceCounts.forEach((confidence, count) -> 
            System.out.println("  • " + confidence + ": " + count));
        
        if (!validKeys.isEmpty()) {
            System.out.println("\n??  Valid keys found (for demonstration only):");
            validKeys.forEach(key -> 
                System.out.println("  • " + maskKey(key)));
            
            System.out.println("\n? Security Recommendations:");
            System.out.println("1. These are simulated results for educational purposes");
            System.out.println("2. In real scenarios, follow responsible disclosure");
            System.out.println("3. Never use exposed keys without permission");
            System.out.println("4. Report security issues through proper channels");
        }
    }
    
    public String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
    
    // Data structure for classification results
    public static class ClassificationResult {
        private final String confidence;
        private final String provider;
        private final String analysis;
        
        public ClassificationResult(String confidence, String provider, String analysis) {
            this.confidence = confidence;
            this.provider = provider;
            this.analysis = analysis;
        }
        
        public String getConfidence() { return confidence; }
        public String getProvider() { return provider; }
        public String getAnalysis() { return analysis; }
    }
}
