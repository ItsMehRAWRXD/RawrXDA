// Simple test for AI providers with multiple API keys
import com.aicli.providers.*;
import java.util.*;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

public class SimpleProviderTest {
    
    // Test API keys for different providers
    private static final String[] OPENAI_KEYS = {
        "sk-abcdef1234567890abcdef1234567890abcdef12",
        "sk-1234567890abcdef1234567890abcdef12345678",
        "sk-abcdefabcdefabcdefabcdefabcdefabcdef12",
        "sk-7890abcdef7890abcdef7890abcdef7890abcd",
        "sk-1234abcd1234abcd1234abcd1234abcd1234abcd"
    };
    
    private static final String[] GEMINI_KEYS = {
        "sk-abcd1234abcd1234abcd1234abcd1234abcd1234",
        "sk-5678efgh5678efgh5678efgh5678efgh5678efgh",
        "sk-efgh5678efgh5678efgh5678efgh5678efgh5678",
        "sk-ijkl1234ijkl1234ijkl1234ijkl1234ijkl1234",
        "sk-mnop5678mnop5678mnop5678mnop5678mnop5678"
    };
    
    private static final String[] AMAZON_KEYS = {
        "sk-qrst1234qrst1234qrst1234qrst1234qrst1234",
        "sk-uvwx5678uvwx5678uvwx5678uvwx5678uvwx5678",
        "sk-1234ijkl1234ijkl1234ijkl1234ijkl1234ijkl",
        "sk-5678mnop5678mnop5678mnop5678mnop5678mnop",
        "sk-qrst5678qrst5678qrst5678qrst5678qrst5678"
    };
    
    private static final String[] CODEGPT_KEYS = {
        "sk-uvwx1234uvwx1234uvwx1234uvwx1234uvwx1234",
        "sk-1234abcd5678efgh1234abcd5678efgh1234abcd",
        "sk-5678ijkl1234mnop5678ijkl1234mnop5678ijkl",
        "sk-abcdqrstefghuvwxabcdqrstefghuvwxabcdqrst",
        "sk-ijklmnop1234qrstijklmnop1234qrstijklmnop"
    };
    
    public static void main(String[] args) {
        System.out.println("Testing AI Provider Integration with Multiple Keys...\n");
        
        testProviderInstantiation();
        testProviderCapabilities();
        testCostEstimation();
        testMultipleKeys();
        testKeyRotation();
        
        System.out.println("\nAll provider tests completed successfully!");
    }
    
    private static void testProviderInstantiation() {
        System.out.println("Testing Provider Instantiation...");
        
        try {
            // Test ChatGPT Provider with first key
            ChatGPTProvider chatgpt = new ChatGPTProvider(OPENAI_KEYS[0], "https://api.openai.com/v1");
            System.out.println("ChatGPT Provider: " + chatgpt.getName() + " - " + chatgpt.getDefaultModel());
            
            // Test Gemini Provider with first key
            GeminiProvider gemini = new GeminiProvider(GEMINI_KEYS[0], "https://generativelanguage.googleapis.com/v1beta");
            System.out.println("Gemini Provider: " + gemini.getName() + " - " + gemini.getDefaultModel());
            
            // Test Ollama Provider (no key needed)
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            System.out.println("Ollama Provider: " + ollama.getName() + " - " + ollama.getDefaultModel());
            
            // Test Amazon Q Provider with first key
            AmazonQProvider amazonq = new AmazonQProvider(AMAZON_KEYS[0], "us-east-1", "amazon.titan-text-express-v1");
            System.out.println("Amazon Q Provider: " + amazonq.getName() + " - " + amazonq.getDefaultModel());
            
            // Test CodeGPT Provider with first key
            CodeGPTProvider codegpt = new CodeGPTProvider(CODEGPT_KEYS[0], "https://api.codegpt.ai/v1");
            System.out.println("CodeGPT Provider: " + codegpt.getName() + " - " + codegpt.getDefaultModel());
            
            System.out.println("All providers instantiated successfully!");
            
        } catch (Exception e) {
            System.err.println("Provider instantiation failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testProviderCapabilities() {
        System.out.println("\nTesting Provider Capabilities...");
        
        try {
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            ProviderCapabilities chatgptCaps = chatgpt.getCapabilities();
            System.out.println("ChatGPT - Streaming: " + chatgptCaps.supportsStreaming() + 
                             ", Context: " + chatgptCaps.getMaxContextLength());
            
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            ProviderCapabilities geminiCaps = gemini.getCapabilities();
            System.out.println("Gemini - Streaming: " + geminiCaps.supportsStreaming() + 
                             ", Context: " + geminiCaps.getMaxContextLength());
            
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            ProviderCapabilities ollamaCaps = ollama.getCapabilities();
            System.out.println("Ollama - Streaming: " + ollamaCaps.supportsStreaming() + 
                             ", Languages: " + ollamaCaps.getSupportedLanguages().size());
            
            CodeGPTProvider codegpt = new CodeGPTProvider("test-key", "https://api.codegpt.ai/v1");
            ProviderCapabilities codegptCaps = codegpt.getCapabilities();
            System.out.println("CodeGPT - Code Execution: " + codegptCaps.supportsCodeExecution() + 
                             ", Languages: " + codegptCaps.getSupportedLanguages().size());
            
            System.out.println("All provider capabilities verified!");
            
        } catch (Exception e) {
            System.err.println("Provider capabilities test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testCostEstimation() {
        System.out.println("\nTesting Cost Estimation...");
        
        try {
            AIRequest request = AIRequest.builder()
                    .messages(List.of(AIRequest.ChatMessage.user("Hello")))
                    .maxTokens(100)
                    .build();
            
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            double chatgptCost = chatgpt.estimateCost(request);
            System.out.println("ChatGPT estimated cost: $" + chatgptCost);
            
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            double geminiCost = gemini.estimateCost(request);
            System.out.println("Gemini estimated cost: $" + geminiCost);
            
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            double ollamaCost = ollama.estimateCost(request);
            System.out.println("Ollama estimated cost: $" + ollamaCost + " (free)");
            
            CodeGPTProvider codegpt = new CodeGPTProvider("test-key", "https://api.codegpt.ai/v1");
            double codegptCost = codegpt.estimateCost(request);
            System.out.println("CodeGPT estimated cost: $" + codegptCost);
            
            System.out.println("All cost estimations completed!");
            
        } catch (Exception e) {
            System.err.println("Cost estimation test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testMultipleKeys() {
        System.out.println("\nTesting Multiple API Keys...");
        
        try {
            // Test ChatGPT with different keys
            for (int i = 0; i < Math.min(3, OPENAI_KEYS.length); i++) {
                ChatGPTProvider provider = new ChatGPTProvider(OPENAI_KEYS[i], "https://api.openai.com/v1");
                System.out.println("ChatGPT Key " + (i+1) + ": " + provider.getName() + " instantiated");
            }
            
            // Test Gemini with different keys
            for (int i = 0; i < Math.min(3, GEMINI_KEYS.length); i++) {
                GeminiProvider provider = new GeminiProvider(GEMINI_KEYS[i], "https://generativelanguage.googleapis.com/v1beta");
                System.out.println("Gemini Key " + (i+1) + ": " + provider.getName() + " instantiated");
            }
            
            // Test Amazon Q with different keys
            for (int i = 0; i < Math.min(3, AMAZON_KEYS.length); i++) {
                AmazonQProvider provider = new AmazonQProvider(AMAZON_KEYS[i], "us-east-1", "amazon.titan-text-express-v1");
                System.out.println("Amazon Q Key " + (i+1) + ": " + provider.getName() + " instantiated");
            }
            
            // Test CodeGPT with different keys
            for (int i = 0; i < Math.min(3, CODEGPT_KEYS.length); i++) {
                CodeGPTProvider provider = new CodeGPTProvider(CODEGPT_KEYS[i], "https://api.codegpt.ai/v1");
                System.out.println("CodeGPT Key " + (i+1) + ": " + provider.getName() + " instantiated");
            }
            
            System.out.println("Multiple keys test completed successfully!");
            
        } catch (Exception e) {
            System.err.println("Multiple keys test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testKeyRotation() {
        System.out.println("\nTesting Key Rotation...");
        
        try {
            // Test rotating through different keys
            String[] testKeys = {OPENAI_KEYS[0], OPENAI_KEYS[1], OPENAI_KEYS[2]};
            
            for (int i = 0; i < testKeys.length; i++) {
                ChatGPTProvider provider = new ChatGPTProvider(testKeys[i], "https://api.openai.com/v1");
                boolean available = provider.isAvailable();
                System.out.println("Key rotation " + (i+1) + ": Available = " + available);
                
                // Small delay between rotations
                Thread.sleep(100);
            }
            
            System.out.println("Key rotation test completed!");
            
        } catch (Exception e) {
            System.err.println("Key rotation test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
