// KeyscanMain.java - Complete Keyscan Implementation
// Based on https://liaogg.medium.com/keyscan-eaa3259ba510
// Educational and security research purposes only

import java.util.*;
import java.util.concurrent.*;
import java.util.stream.Collectors;

public class KeyscanMain {
    
    public static void main(String[] args) {
        System.out.println("? Keyscan - AI-Powered API Key Discovery Tool");
        System.out.println("Based on: https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("??  WARNING: For educational and security research purposes only!");
        System.out.println("??  Always follow responsible disclosure practices!\n");
        
        // Configuration
        String[] searchTerms = {
            "OPENAI_API_KEY",
            "GEMINI_API_KEY", 
            "ANTHROPIC_API_KEY",
            "AWS_ACCESS_KEY",
            "STRIPE_SECRET_KEY",
            "GITHUB_TOKEN"
        };
        
        String[] fileTypes = {".env", "config", "secrets", "credentials"};
        
        // Initialize components
        KeyScanner scanner = new KeyScanner("", false, 5);
        AIKeyClassifier classifier = new AIKeyClassifier();
        KeyVerifier verifier = new KeyVerifier(3, 10);
        
        try {
            // Phase 1: Discovery
            System.out.println("? Phase 1: Key Discovery");
            System.out.println("=" + "=".repeat(30));
            List<KeyScanner.ScanResult> scanResults = simulateKeyDiscovery(searchTerms, fileTypes);
            
            // Phase 2: Classification
            System.out.println("\n? Phase 2: AI Classification");
            System.out.println("=" + "=".repeat(30));
            List<AIKeyClassifier.ClassificationResult> classificationResults = 
                performClassification(scanResults, classifier);
            
            // Phase 3: Verification
            System.out.println("\n? Phase 3: Key Verification");
            System.out.println("=" + "=".repeat(30));
            List<KeyVerifier.VerificationResult> verificationResults = 
                performVerification(classificationResults, verifier);
            
            // Phase 4: Reporting
            System.out.println("\n? Phase 4: Results & Recommendations");
            System.out.println("=" + "=".repeat(30));
            generateReport(scanResults, classificationResults, verificationResults);
            
        } catch (Exception e) {
            System.err.println("Keyscan execution error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static List<KeyScanner.ScanResult> simulateKeyDiscovery(String[] searchTerms, String[] fileTypes) {
        System.out.println("? Simulating GitHub Gist search...");
        
        List<KeyScanner.ScanResult> results = new ArrayList<>();
        
        // Simulate finding keys in different searches
        for (String term : searchTerms) {
            for (String fileType : fileTypes) {
                List<KeyScanner.DiscoveredKey> foundKeys = new ArrayList<>();
                
                // Simulate finding some keys based on search term
                if (term.contains("OPENAI")) {
                    foundKeys.add(new KeyScanner.DiscoveredKey(
                        "sk-abcdef1234567890abcdef1234567890abcdef12",
                        "openai",
                        "HIGH",
                        "https://gist.github.com/simulated123",
                        "OPENAI_API_KEY=sk-abcdef1234567890abcdef1234567890abcdef12"
                    ));
                }
                
                if (term.contains("GEMINI")) {
                    foundKeys.add(new KeyScanner.DiscoveredKey(
                        "AIzaTest1234567890abcdef1234567890abcdef",
                        "gemini",
                        "MEDIUM", 
                        "https://gist.github.com/simulated456",
                        "GEMINI_API_KEY=AIzaTest1234567890abcdef1234567890abcdef"
                    ));
                }
                
                if (term.contains("AWS")) {
                    foundKeys.add(new KeyScanner.DiscoveredKey(
                        "AKIATest1234567890",
                        "aws",
                        "HIGH",
                        "https://gist.github.com/simulated789",
                        "AWS_ACCESS_KEY_ID=AKIATest1234567890"
                    ));
                }
                
                if (!foundKeys.isEmpty()) {
                    results.add(new KeyScanner.ScanResult(term, fileType, foundKeys));
                }
            }
        }
        
        System.out.println("Found " + results.size() + " search results with potential keys");
        return results;
    }
    
    private static List<AIKeyClassifier.ClassificationResult> performClassification(
            List<KeyScanner.ScanResult> scanResults, AIKeyClassifier classifier) {
        
        System.out.println("? Classifying discovered keys with AI...");
        
        List<AIKeyClassifier.ClassificationResult> results = new ArrayList<>();
        
        for (KeyScanner.ScanResult scanResult : scanResults) {
            for (KeyScanner.DiscoveredKey key : scanResult.getFoundKeys()) {
                AIKeyClassifier.ClassificationResult classification = 
                    classifier.classifyKey(key.getKey());
                results.add(classification);
                
                System.out.println("  • Key: " + maskKey(key.getKey()));
                System.out.println("    Provider: " + classification.getProvider());
                System.out.println("    Confidence: " + classification.getConfidence());
            }
        }
        
        return results;
    }
    
    private static List<KeyVerifier.VerificationResult> performVerification(
            List<AIKeyClassifier.ClassificationResult> classificationResults, KeyVerifier verifier) {
        
        System.out.println("? Verifying keys with API calls...");
        
        // Extract keys from classification results
        String[] keys = classificationResults.stream()
            .map(result -> "test-key-" + result.getProvider()) // Simulate key extraction
            .toArray(String[]::new);
        
        try {
            return verifier.verifyKeys(keys);
        } catch (Exception e) {
            System.err.println("Verification failed: " + e.getMessage());
            return new ArrayList<>();
        }
    }
    
    private static void generateReport(
            List<KeyScanner.ScanResult> scanResults,
            List<AIKeyClassifier.ClassificationResult> classificationResults,
            List<KeyVerifier.VerificationResult> verificationResults) {
        
        System.out.println("? Comprehensive Keyscan Report");
        System.out.println("=" + "=".repeat(50));
        
        // Discovery statistics
        int totalKeysFound = scanResults.stream()
            .mapToInt(result -> result.getFoundKeys().size())
            .sum();
        
        System.out.println("? Discovery Phase:");
        System.out.println("  • Total keys found: " + totalKeysFound);
        System.out.println("  • Search results: " + scanResults.size());
        
        // Classification statistics
        Map<String, Long> providerCounts = classificationResults.stream()
            .collect(Collectors.groupingBy(AIKeyClassifier.ClassificationResult::getProvider, Collectors.counting()));
        
        Map<String, Long> confidenceCounts = classificationResults.stream()
            .collect(Collectors.groupingBy(AIKeyClassifier.ClassificationResult::getConfidence, Collectors.counting()));
        
        System.out.println("\n? Classification Phase:");
        System.out.println("  • Keys classified: " + classificationResults.size());
        System.out.println("  • Provider breakdown:");
        providerCounts.forEach((provider, count) -> 
            System.out.println("    - " + provider + ": " + count));
        System.out.println("  • Confidence breakdown:");
        confidenceCounts.forEach((confidence, count) -> 
            System.out.println("    - " + confidence + ": " + count));
        
        // Verification statistics
        long validKeys = verificationResults.stream()
            .filter(KeyVerifier.VerificationResult::isValid)
            .count();
        
        System.out.println("\n? Verification Phase:");
        System.out.println("  • Keys verified: " + verificationResults.size());
        System.out.println("  • Valid keys: " + validKeys);
        System.out.println("  • Invalid keys: " + (verificationResults.size() - validKeys));
        
        // Security recommendations
        System.out.println("\n? Security Recommendations:");
        System.out.println("1. This is a simulated demonstration of Keyscan methodology");
        System.out.println("2. In real usage, ALWAYS follow responsible disclosure");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without explicit permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Implement proper key management and rotation");
        System.out.println("7. Use environment variables and secure storage");
        System.out.println("8. Regular security audits and monitoring");
        
        // Best practices
        System.out.println("\n? Best Practices for API Key Security:");
        System.out.println("  • Never commit keys to version control");
        System.out.println("  • Use environment variables for configuration");
        System.out.println("  • Implement key rotation policies");
        System.out.println("  • Monitor key usage and access patterns");
        System.out.println("  • Use least-privilege access principles");
        System.out.println("  • Regular security training for developers");
    }
    
    private static String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
}
