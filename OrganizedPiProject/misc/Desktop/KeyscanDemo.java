// KeyscanDemo.java - Complete demonstration of Keyscan methodology
// This shows the full workflow from the Medium article

import java.util.*;
import java.util.concurrent.*;

public class KeyscanDemo {
    
    public static void main(String[] args) {
        System.out.println("KeyscanDemo - Complete API Key Discovery Workflow");
        System.out.println("Based on: https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("WARNING: Educational and security research purposes only!\n");
        
        try {
            // Step 1: Simulate Google dork searches
            System.out.println("Step 1: Google Dork Searches");
            System.out.println("=" + "=".repeat(40));
            simulateGoogleDorkSearches();
            
            // Step 2: Simulate web scraping
            System.out.println("\nStep 2: Web Scraping");
            System.out.println("=" + "=".repeat(40));
            simulateWebScraping();
            
            // Step 3: Simulate AI classification
            System.out.println("\nStep 3: AI Classification");
            System.out.println("=" + "=".repeat(40));
            simulateAIClassification();
            
            // Step 4: Simulate key verification
            System.out.println("\nStep 4: Key Verification");
            System.out.println("=" + "=".repeat(40));
            simulateKeyVerification();
            
            // Step 5: Generate final report
            System.out.println("\nStep 5: Final Report");
            System.out.println("=" + "=".repeat(40));
            generateFinalReport();
            
        } catch (Exception e) {
            System.err.println("Demo error: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void simulateGoogleDorkSearches() {
        String[] dorks = {
            "site:github.com filetype:env OPENAI_API_KEY",
            "site:gist.github.com \"sk-\"",
            "site:pastebin.com OPENAI_API_KEY"
        };
        
        for (String dork : dorks) {
            System.out.println("Searching: " + dork);
            try {
                Thread.sleep(1000); // Simulate search delay
                System.out.println("   Found 3-5 potential URLs");
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
    
    private static void simulateWebScraping() {
        String[] urls = {
            "https://github.com/user/repo/blob/main/.env",
            "https://gist.github.com/abc123/def456",
            "https://pastebin.com/raw/ghi789"
        };
        
        for (String url : urls) {
            System.out.println("Scraping: " + url);
            try {
                Thread.sleep(1500); // Simulate scraping delay
                System.out.println("   Found 1-2 potential API keys");
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
    
    private static void simulateAIClassification() {
        String[] keys = {
            "sk-abcdef1234567890abcdef1234567890abcdef12",
            "AIzaTest1234567890abcdef1234567890abcdef",
            "test-key-placeholder"
        };
        
        for (String key : keys) {
            System.out.println("Classifying: " + maskKey(key));
            try {
                Thread.sleep(800); // Simulate AI processing
                if (key.contains("test") || key.contains("placeholder")) {
                    System.out.println("   Result: NONE (placeholder detected)");
                } else {
                    System.out.println("   Result: HIGH (valid key pattern)");
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
    
    private static void simulateKeyVerification() {
        String[] keys = {
            "sk-abcdef1234567890abcdef1234567890abcdef12",
            "AIzaTest1234567890abcdef1234567890abcdef"
        };
        
        for (String key : keys) {
            System.out.println("Verifying: " + maskKey(key));
            try {
                Thread.sleep(1200); // Simulate API call
                System.out.println("   Result: " + (Math.random() > 0.5 ? "VALID" : "INVALID"));
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
    
    private static void generateFinalReport() {
        System.out.println("SCAN RESULTS:");
        System.out.println("• Total URLs scanned: 9");
        System.out.println("• Potential keys found: 6");
        System.out.println("• Keys classified as valid: 2");
        System.out.println("• Keys verified as working: 1");
        System.out.println("• Success rate: 16.7%");
        
        System.out.println("\nDISCOVERED KEYS:");
        System.out.println("• OpenAI: 1 key (verified)");
        System.out.println("• Gemini: 1 key (invalid)");
        
        System.out.println("\nSECURITY RECOMMENDATIONS:");
        System.out.println("1. This is a simulated demonstration");
        System.out.println("2. In real scenarios, follow responsible disclosure");
        System.out.println("3. Contact key owners immediately if real keys are found");
        System.out.println("4. Never use exposed keys without permission");
        System.out.println("5. Report security issues through proper channels");
        System.out.println("6. Implement proper key management and monitoring");
    }
    
    private static String maskKey(String key) {
        if (key.length() <= 8) return key;
        return key.substring(0, 4) + "*".repeat(Math.min(key.length() - 8, 20)) + key.substring(key.length() - 4);
    }
}
