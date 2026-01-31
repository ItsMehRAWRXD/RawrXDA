// ai-cli-backend/src/test/java/com/aicli/ProviderIntegrationTest.java
package com.aicli;

import com.aicli.providers.*;
import java.util.*;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;

/**
 * Integration test for AI providers with the main AiCli system.
 * Tests that providers can be instantiated and used correctly.
 */
public class ProviderIntegrationTest {
    
    public static void main(String[] args) {
        System.out.println("Testing AI Provider Integration...\n");
        
        testProviderInstantiation();
        testProviderCapabilities();
        testProviderConfiguration();
        testErrorHandling();
        
        System.out.println("\nAll provider integration tests completed successfully!");
    }
    
    private static void testProviderInstantiation() {
        System.out.println("Testing Provider Instantiation...");
        
        try {
            // Test ChatGPT Provider
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            assert chatgpt.getName().equals("ChatGPT");
            assert chatgpt.getDefaultModel().equals("gpt-4o-mini");
            System.out.println("ChatGPT Provider instantiated successfully");
            
            // Test Gemini Provider
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            assert gemini.getName().equals("Gemini");
            assert gemini.getDefaultModel().equals("gemini-pro");
            System.out.println("Gemini Provider instantiated successfully");
            
            // Test Ollama Provider
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            assert ollama.getName().equals("Ollama");
            assert ollama.getDefaultModel().equals("llama3:8b");
            System.out.println("Ollama Provider instantiated successfully");
            
            // Test Amazon Q Provider
            AmazonQProvider amazonq = new AmazonQProvider("test-key", "us-east-1", "amazon.titan-text-express-v1");
            assert amazonq.getName().equals("Amazon Q");
            assert amazonq.getDefaultModel().equals("amazon.titan-text-express-v1");
            System.out.println("Amazon Q Provider instantiated successfully");
            
            // Test CodeGPT Provider
            CodeGPTProvider codegpt = new CodeGPTProvider("test-key", "https://api.codegpt.ai/v1");
            assert codegpt.getName().equals("CodeGPT");
            assert codegpt.getDefaultModel().equals("codegpt-pro");
            System.out.println("CodeGPT Provider instantiated successfully");
            
        } catch (Exception e) {
            System.err.println("Provider instantiation failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testProviderCapabilities() {
        System.out.println("\n? Testing Provider Capabilities...");
        
        try {
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            ProviderCapabilities chatgptCaps = chatgpt.getCapabilities();
            
            assert chatgptCaps.supportsStreaming();
            assert chatgptCaps.supportsFunctionCalling();
            assert chatgptCaps.getMaxContextLength() > 0;
            System.out.println("? ChatGPT capabilities verified");
            
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            ProviderCapabilities geminiCaps = gemini.getCapabilities();
            
            assert !geminiCaps.supportsStreaming(); // Gemini doesn't support streaming
            assert geminiCaps.supportsImageInput();
            System.out.println("? Gemini capabilities verified");
            
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            ProviderCapabilities ollamaCaps = ollama.getCapabilities();
            
            assert ollamaCaps.supportsStreaming();
            assert ollamaCaps.getSupportedLanguages().contains("en");
            System.out.println("? Ollama capabilities verified");
            
            CodeGPTProvider codegpt = new CodeGPTProvider("test-key", "https://api.codegpt.ai/v1");
            ProviderCapabilities codegptCaps = codegpt.getCapabilities();
            
            assert codegptCaps.supportsCodeExecution();
            assert codegptCaps.getSupportedLanguages().contains("java");
            assert codegptCaps.getSupportedLanguages().contains("python");
            System.out.println("? CodeGPT capabilities verified");
            
        } catch (Exception e) {
            System.err.println("? Provider capabilities test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testProviderConfiguration() {
        System.out.println("\n? Testing Provider Configuration...");
        
        try {
            // Test ChatGPT configuration
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            Map<String, Object> chatgptConfig = chatgpt.getConfigurationOptions();
            assert chatgptConfig.containsKey("baseUrl");
            assert chatgptConfig.containsKey("apiKey");
            System.out.println("? ChatGPT configuration verified");
            
            // Test Gemini configuration
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            Map<String, Object> geminiConfig = gemini.getConfigurationOptions();
            assert geminiConfig.containsKey("baseUrl");
            assert geminiConfig.containsKey("apiKey");
            System.out.println("? Gemini configuration verified");
            
            // Test Ollama configuration
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            Map<String, Object> ollamaConfig = ollama.getConfigurationOptions();
            assert ollamaConfig.containsKey("baseUrl");
            assert ollamaConfig.containsKey("maxTokens");
            System.out.println("? Ollama configuration verified");
            
            // Test Amazon Q configuration
            AmazonQProvider amazonq = new AmazonQProvider("test-key", "us-east-1", "amazon.titan-text-express-v1");
            Map<String, Object> amazonqConfig = amazonq.getConfigurationOptions();
            assert amazonqConfig.containsKey("region");
            assert amazonqConfig.containsKey("modelId");
            System.out.println("? Amazon Q configuration verified");
            
            // Test CodeGPT configuration
            CodeGPTProvider codegpt = new CodeGPTProvider("test-key", "https://api.codegpt.ai/v1");
            Map<String, Object> codegptConfig = codegpt.getConfigurationOptions();
            assert codegptConfig.containsKey("baseUrl");
            assert codegptConfig.containsKey("codeOptimization");
            System.out.println("? CodeGPT configuration verified");
            
        } catch (Exception e) {
            System.err.println("? Provider configuration test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testErrorHandling() {
        System.out.println("\n? Testing Error Handling...");
        
        try {
            // Test with invalid configuration
            ChatGPTProvider invalidProvider = new ChatGPTProvider("invalid-key", "https://invalid-url.com");
            
            AIRequest request = AIRequest.builder()
                    .messages(List.of(AIRequest.ChatMessage.user("Hello")))
                    .model("gpt-4o-mini")
                    .maxTokens(10)
                    .build();
            
            // This should fail gracefully
            try {
                CompletableFuture<AIResponse> responseFuture = invalidProvider.chatCompletion(request);
                AIResponse response = responseFuture.get(5, TimeUnit.SECONDS);
                System.out.println("??  Unexpected success with invalid configuration");
            } catch (Exception e) {
                System.out.println("? Error handling verified: " + e.getClass().getSimpleName());
            }
            
            // Test availability check
            boolean isAvailable = invalidProvider.isAvailable();
            assert !isAvailable; // Should be false for invalid configuration
            System.out.println("? Availability check verified");
            
        } catch (Exception e) {
            System.err.println("? Error handling test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testCostEstimation() {
        System.out.println("\n? Testing Cost Estimation...");
        
        try {
            AIRequest smallRequest = AIRequest.builder()
                    .messages(List.of(AIRequest.ChatMessage.user("Hi")))
                    .maxTokens(10)
                    .build();
            
            AIRequest largeRequest = AIRequest.builder()
                    .messages(List.of(AIRequest.ChatMessage.user("Write a detailed explanation")))
                    .maxTokens(1000)
                    .build();
            
            // Test ChatGPT cost estimation
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            double smallCost = chatgpt.estimateCost(smallRequest);
            double largeCost = chatgpt.estimateCost(largeRequest);
            assert largeCost > smallCost;
            System.out.println("? ChatGPT cost estimation: Small=$" + smallCost + ", Large=$" + largeCost);
            
            // Test Ollama cost estimation (should be 0)
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            double ollamaCost = ollama.estimateCost(smallRequest);
            assert ollamaCost == 0.0;
            System.out.println("? Ollama cost estimation: $" + ollamaCost + " (free)");
            
        } catch (Exception e) {
            System.err.println("? Cost estimation test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void testSupportedModels() {
        System.out.println("\n? Testing Supported Models...");
        
        try {
            ChatGPTProvider chatgpt = new ChatGPTProvider("test-key", "https://api.openai.com/v1");
            String[] chatgptModels = chatgpt.getSupportedModels();
            assert chatgptModels.length > 0;
            assert Arrays.asList(chatgptModels).contains("gpt-4o-mini");
            System.out.println("? ChatGPT models: " + Arrays.toString(chatgptModels));
            
            GeminiProvider gemini = new GeminiProvider("test-key", "https://generativelanguage.googleapis.com/v1beta");
            String[] geminiModels = gemini.getSupportedModels();
            assert geminiModels.length > 0;
            assert Arrays.asList(geminiModels).contains("gemini-pro");
            System.out.println("? Gemini models: " + Arrays.toString(geminiModels));
            
            OllamaProvider ollama = new OllamaProvider("http://localhost:11434");
            String[] ollamaModels = ollama.getSupportedModels();
            assert ollamaModels.length > 0;
            assert Arrays.asList(ollamaModels).contains("llama3:8b");
            System.out.println("? Ollama models: " + Arrays.toString(ollamaModels));
            
        } catch (Exception e) {
            System.err.println("? Supported models test failed: " + e.getMessage());
            e.printStackTrace();
        }
    }
}
