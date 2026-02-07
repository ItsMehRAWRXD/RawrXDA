public class UnicodeEmojiDoc {
    public static void printEmojiInfo(String text) {
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (Character.getType(c) == Character.OTHER_SYMBOL || 
                (c >= 0xD800 && c <= 0xDFFF)) {
                System.out.printf("U+%04X %c\n", (int)c, c);
            }
        }
    }
    
    public static String getEmojiUnicode(String emoji) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < emoji.length(); i++) {
            sb.append(String.format("U+%04X ", (int)emoji.charAt(i)));
        }
        return sb.toString().trim();
    }
}

// TestCopilotSecurity.java
class TestCopilotSecurity {
    public static void main(String[] args) {
        // Initialize Copilot with security features
        CopilotIntegration copilot = new CopilotIntegration();
        
        // Configure security settings
        copilot.setEncryptionEnabled(true);
        copilot.setEmojiRemovalEnabled(true);
        
        // Test emoji removal
        EmojiRemover.testEmojiRemoval();
        
        // Test completion with emojis and security
        String testPrompt = "public class Calculator { ? // implement calculator ?";
        
        copilot.complete(testPrompt)
            .whenComplete((result, error) -> {
                if (error != null) {
                    System.err.println("Error: " + error.getMessage());
                } else {
                    System.out.println("Secure completion: " + result);
                }
            });
        
        // Check security status
        System.out.println("Security Status: " + copilot.getSecurityStatus());
    }
}

// CopilotTestRunner.java
class CopilotTestRunner {
    public static void main(String[] args) {
        System.out.println("? Starting Copilot Integration Tests");
        
        CopilotIntegration copilot = new CopilotIntegration();
        
        // Run all tests
        copilot.runDiagnostics();
        copilot.testEmojiRemoval();
        copilot.performanceTest();
        
        // Test hooks
        copilot.testHook("public class Calculator {");
        
        // Test completion with retry
        copilot.completeWithRetry("public void calculateSum(", 3)
            .whenComplete((result, error) -> {
                if (error == null) {
                    System.out.println("? Final retry result: " + result);
                } else {
                    System.err.println("? Retry completion failed: " + error.getMessage());
                }
            });
        
        // Print NASA parser stats
        NASAParser.printStats();
        
        System.out.println("? All tests completed!");
    }
}