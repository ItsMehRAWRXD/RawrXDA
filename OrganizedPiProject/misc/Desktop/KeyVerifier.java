// KeyVerifier.java - API Key Verification System
// Based on Keyscan methodology for verifying discovered keys
// IMPORTANT: This is for educational purposes only - never use exposed keys!

import java.util.*;
import java.util.concurrent.*;
import java.util.stream.Collectors;

public class KeyVerifier {
    
    // Simulated verification results
    private static final Map<String, VerificationResult> MOCK_VERIFICATION_RESULTS = Map.of(
        "sk-abcdef1234567890abcdef1234567890abcdef12",
        new VerificationResult(true, "openai", "Valid OpenAI API key", 200),
        
        "AIzaTest1234567890abcdef1234567890abcdef",
        new VerificationResult(true, "gemini", "Valid Google API key", 200),
        
        "sk-ant-test1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
        new VerificationResult(false, "anthropic", "Invalid or expired key", 401),
        
        "test-key-12345",
        new VerificationResult(false, "unknown", "Test key - not real", 400),
        
        "placeholder-key",
        new VerificationResult(false, "unknown", "Placeholder value", 400)
    );
    
    private final int maxConcurrentVerifications;
    private final int verificationTimeoutSeconds;
    
    public KeyVerifier(int maxConcurrentVerifications, int verificationTimeoutSeconds) {
        this.maxConcurrentVerifications = maxConcurrentVerifications;
        this.verificationTimeoutSeconds = verificationTimeoutSeconds;
    }
    
    public static void main(String[] args) {
        System.out.println("? Key Verifier - API Key Validation System");
        System.out.println("Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("??  WARNING: This is for educational purposes only!\n");
        
        KeyVerifier verifier = new KeyVerifier(5, 10);
        
        // Test keys from your provider test
        String[] testKeys = {
            "sk-abcdef1234567890abcdef1234567890abcdef12",
            "sk-1234567890abcdef1234567890abcdef12345678",
            "AIzaTest1234567890abcdef1234567890abcdef",
            "test-key-12345",
            "placeholder-key"
        };
        
        System.out.println("? Verifying API keys...\n");
        
        try {
            List<VerificationResult> results = verifier.verifyKeys(testKeys);
            verifier.processVerificationResults(results);
        } catch (Exception e) {
            System.err.println("Verification error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    public List<VerificationResult> verifyKeys(String[] keys) throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(maxConcurrentVerifications);
        List<Future<VerificationResult>> futures = new ArrayList<>();
        
        System.out.println("? Starting verification of " + keys.length + " keys...");
        
        for (String key : keys) {
            Future<VerificationResult> future = executor.submit(() -> {
                return verifyKey(key);
            });
            futures.add(future);
        }
        
        // Collect results with timeout
        List<VerificationResult> results = new ArrayList<>();
        for (Future<VerificationResult> future : futures) {
            try {
                VerificationResult result = future.get(verificationTimeoutSeconds, TimeUnit.SECONDS);
                results.add(result);
            } catch (TimeoutException e) {
                System.err.println("Verification timeout for key");
                results.add(new VerificationResult(false, "unknown", "Verification timeout", 408));
            }
        }
        
        executor.shutdown();
        return results;
    }
    
    private VerificationResult verifyKey(String key) {
        // Simulate API verification delay
        try {
            Thread.sleep(100 + (int)(Math.random() * 200)); // 100-300ms delay
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        
        // Check if we have a mock result
        if (MOCK_VERIFICATION_RESULTS.containsKey(key)) {
            return MOCK_VERIFICATION_RESULTS.get(key);
        }
        
        // Simulate verification based on key patterns
        return simulateVerification(key);
    }
    
    private VerificationResult simulateVerification(String key) {
        // Simulate different verification outcomes
        if (key.startsWith("sk-") && key.length() >= 40) {
            // Simulate OpenAI API call
            boolean isValid = Math.random() > 0.3; // 70% chance of being valid
            return new VerificationResult(
                isValid, 
                "openai", 
                isValid ? "Valid OpenAI API key" : "Invalid OpenAI API key", 
                isValid ? 200 : 401
            );
        }
        
        if (key.startsWith("AIza") && key.length() >= 35) {
            // Simulate Google API call
            boolean isValid = Math.random() > 0.4; // 60% chance of being valid
            return new VerificationResult(
                isValid,
                "gemini",
                isValid ? "Valid Google API key" : "Invalid Google API key",
                isValid ? 200 : 401
            );
        }
        
        if (key.contains("test") || key.contains("example") || key.contains("placeholder")) {
            return new VerificationResult(false, "unknown", "Test/placeholder key", 400);
        }
        
        // Unknown key format
        return new VerificationResult(false, "unknown", "Unknown key format", 400);
    }
    
    public void processVerificationResults(List<VerificationResult> results) {
        System.out.println("\n? Verification Results Summary:");
        System.out.println("=" + "=".repeat(50));
        
        int totalKeys = results.size();
        int validKeys = (int) results.stream().filter(VerificationResult::isValid).count();
        int invalidKeys = totalKeys - validKeys;
        
        System.out.println("Total keys verified: " + totalKeys);
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
        
        // Show valid keys (for demonstration)
        List<VerificationResult> validResults = results.stream()
            .filter(VerificationResult::isValid)
            .collect(Collectors.toList());
        
        if (!validResults.isEmpty()) {
            System.out.println("\n? Valid Keys Found (DEMONSTRATION ONLY):");
            for (VerificationResult result : validResults) {
                System.out.println("  • Provider: " + result.getProvider());
                System.out.println("  • Status: " + result.getStatus());
                System.out.println("  • HTTP Code: " + result.getHttpCode());
            }
        }
        
        // Security recommendations
        System.out.println("\n? Security Recommendations:");
        System.out.println("1. These are simulated results for educational purposes");
        System.out.println("2. In real scenarios, follow responsible disclosure practices");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without explicit permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Consider implementing key rotation and monitoring");
    }
    
    // Data structure for verification results
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
