// KeyscanRunner.java - Main runner for all Keyscan implementations
// This demonstrates the complete Keyscan methodology from the Medium article

import java.util.*;
import java.util.concurrent.*;

public class KeyscanRunner {
    
    public static void main(String[] args) {
        System.out.println("? KeyscanRunner - Complete API Key Discovery Suite");
        System.out.println("Based on Keyscan methodology from https://liaogg.medium.com/keyscan-eaa3259ba510");
        System.out.println("??  WARNING: This is for educational and security research purposes only!");
        System.out.println("??  Always follow responsible disclosure practices!\n");
        
        Scanner scanner = new Scanner(System.in);
        
        while (true) {
            System.out.println("? Choose a Keyscan implementation:");
            System.out.println("1. Basic KeyScanner (Simulated)");
            System.out.println("2. AI Key Classifier (LLM Simulation)");
            System.out.println("3. Key Verifier (API Testing)");
            System.out.println("4. Real Keyscan (Web Crawling)");
            System.out.println("5. Advanced Keyscan (Real HTTP Requests)");
            System.out.println("6. Run All Tests");
            System.out.println("0. Exit");
            System.out.print("\nEnter your choice (0-6): ");
            
            int choice = scanner.nextInt();
            
            switch (choice) {
                case 1:
                    runBasicKeyScanner();
                    break;
                case 2:
                    runAIKeyClassifier();
                    break;
                case 3:
                    runKeyVerifier();
                    break;
                case 4:
                    runRealKeyscan();
                    break;
                case 5:
                    runAdvancedKeyscan();
                    break;
                case 6:
                    runAllTests();
                    break;
                case 0:
                    System.out.println("? Goodbye! Remember to use these tools responsibly.");
                    return;
                default:
                    System.out.println("? Invalid choice. Please try again.");
            }
            
            System.out.println("\n" + "=".repeat(60) + "\n");
        }
    }
    
    private static void runBasicKeyScanner() {
        System.out.println("? Running Basic KeyScanner...");
        try {
            KeyScanner.main(new String[]{});
        } catch (Exception e) {
            System.err.println("Error running KeyScanner: " + e.getMessage());
        }
    }
    
    private static void runAIKeyClassifier() {
        System.out.println("? Running AI Key Classifier...");
        try {
            AIKeyClassifier.main(new String[]{});
        } catch (Exception e) {
            System.err.println("Error running AIKeyClassifier: " + e.getMessage());
        }
    }
    
    private static void runKeyVerifier() {
        System.out.println("? Running Key Verifier...");
        try {
            KeyVerifier.main(new String[]{});
        } catch (Exception e) {
            System.err.println("Error running KeyVerifier: " + e.getMessage());
        }
    }
    
    private static void runRealKeyscan() {
        System.out.println("?? Running Real Keyscan...");
        try {
            RealKeyscan.main(new String[]{});
        } catch (Exception e) {
            System.err.println("Error running RealKeyscan: " + e.getMessage());
        }
    }
    
    private static void runAdvancedKeyscan() {
        System.out.println("? Running Advanced Keyscan...");
        try {
            AdvancedKeyscan.main(new String[]{});
        } catch (Exception e) {
            System.err.println("Error running AdvancedKeyscan: " + e.getMessage());
        }
    }
    
    private static void runAllTests() {
        System.out.println("? Running All Keyscan Tests...\n");
        
        String[] tests = {
            "Basic KeyScanner",
            "AI Key Classifier", 
            "Key Verifier",
            "Real Keyscan",
            "Advanced Keyscan"
        };
        
        for (int i = 0; i < tests.length; i++) {
            System.out.println("? Test " + (i + 1) + "/" + tests.length + ": " + tests[i]);
            System.out.println("-".repeat(40));
            
            try {
                switch (i) {
                    case 0:
                        KeyScanner.main(new String[]{});
                        break;
                    case 1:
                        AIKeyClassifier.main(new String[]{});
                        break;
                    case 2:
                        KeyVerifier.main(new String[]{});
                        break;
                    case 3:
                        RealKeyscan.main(new String[]{});
                        break;
                    case 4:
                        AdvancedKeyscan.main(new String[]{});
                        break;
                }
            } catch (Exception e) {
                System.err.println("? Error in " + tests[i] + ": " + e.getMessage());
            }
            
            System.out.println("\n" + "=".repeat(60) + "\n");
            
            // Add delay between tests
            try {
                Thread.sleep(2000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        
        System.out.println("? All tests completed!");
        System.out.println("\n? Remember:");
        System.out.println("• These tools are for educational purposes only");
        System.out.println("• Always follow responsible disclosure practices");
        System.out.println("• Never use exposed keys without permission");
        System.out.println("• Report security issues through proper channels");
    }
}
