import java.net.http.*;
import java.net.URI;
import java.util.*;
import java.util.concurrent.*;
import javax.swing.*; // Added import for Swing components
import javax.swing.SwingUtilities;

/**
 * AI Orchestrator - Manages multiple AI providers and intelligent routing
 */
public class AIOrchestrator {
    private final Map<String, AIProvider> providers = new HashMap<>();
    private final ExecutorService executor = Executors.newCachedThreadPool();
    private String primaryProvider = "openai";
    private boolean multiProviderMode = false;
    
    public void initialize() {
        // Initialize all AI providers
        providers.put("openai", new OpenAIProvider());
        providers.put("claude", new ClaudeProvider());
        providers.put("copilot", new CopilotProvider());
        providers.put("amazonq", new AmazonQProvider());
        providers.put("local", new LocalAIProvider());
        
        System.out.println("AI Orchestrator initialized with " + providers.size() + " providers");
    }
    
    public CompletableFuture<String> generateCode() {
        return processRequest("Generate code based on current context", RequestType.CODE_GENERATION);
    }
    
    public CompletableFuture<String> explainCode() {
        return processRequest("Explain the selected code", RequestType.CODE_EXPLANATION);
    }
    
    public CompletableFuture<String> reviewCode() {
        return processRequest("Review code for improvements", RequestType.CODE_REVIEW);
    }
    
    public CompletableFuture<String> chat(String message) {
        return processRequest(message, RequestType.CHAT);
    }
    
    private CompletableFuture<String> processRequest(String prompt, RequestType type) {
        if (multiProviderMode) {
            return processMultiProvider(prompt, type);
        } else {
            return processSingleProvider(prompt, type);
        }
    }
    
    private CompletableFuture<String> processSingleProvider(String prompt, RequestType type) {
        AIProvider provider = providers.get(primaryProvider);
        if (provider == null) {
            return CompletableFuture.completedFuture("Error: Primary provider not available");
        }
        
        return CompletableFuture.supplyAsync(() -> {
            try {
                return provider.process(prompt, type);
            } catch (Exception e) {
                return "Error: " + e.getMessage();
            }
        }, executor);
    }
    
    private CompletableFuture<String> processMultiProvider(String prompt, RequestType type) {
        List<CompletableFuture<String>> futures = new ArrayList<>();
        
        for (Map.Entry<String, AIProvider> entry : providers.entrySet()) {
            String providerName = entry.getKey();
            AIProvider provider = entry.getValue();
            
            CompletableFuture<String> future = CompletableFuture.supplyAsync(() -> {
                try {
                    String result = provider.process(prompt, type);
                    return "[" + providerName.toUpperCase() + "]\n" + result;
                } catch (Exception e) {
                    return "[" + providerName.toUpperCase() + "] Error: " + e.getMessage();
                }
            }, executor);
            
            futures.add(future);
        }
        
        return CompletableFuture.allOf(futures.toArray(new CompletableFuture[0]))
            .thenApply(v -> {
                StringBuilder combined = new StringBuilder();
                for (CompletableFuture<String> future : futures) {
                    try {
                        combined.append(future.get()).append("\n\n");
                    } catch (Exception e) {
                        combined.append("Provider error: ").append(e.getMessage()).append("\n\n");
                    }
                }
                return combined.toString();
            });
    }
    
    // Provider management
    public void setPrimaryProvider(String provider) {
        this.primaryProvider = provider;
    }
    
    public void setMultiProviderMode(boolean enabled) {
        this.multiProviderMode = enabled;
    }
    
    public Set<String> getAvailableProviders() {
        return providers.keySet();
    }
    
    public boolean isProviderActive(String provider) {
        AIProvider p = providers.get(provider);
        return p != null && p.isAvailable();
    }
    
    // AI Provider interface
    public interface AIProvider {
        String process(String prompt, RequestType type) throws Exception;
        boolean isAvailable();
        String getStatus();
    }
    
    public enum RequestType {
        CHAT,
        CODE_GENERATION,
        CODE_EXPLANATION,
        CODE_REVIEW,
        DEBUGGING,
        REFACTORING
    }
    
    // OpenAI Provider
    private class OpenAIProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        private final String apiKey = System.getenv("OPENAI_API_KEY");
        
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            if (apiKey == null) {
                return "OpenAI API key not configured";
            }
            
            String systemPrompt = getSystemPrompt(type);
            String requestBody = String.format("""
                {
                    "model": "gpt-4",
                    "messages": [
                        {"role": "system", "content": "%s"},
                        {"role": "user", "content": "%s"}
                    ],
                    "max_tokens": 2000,
                    "temperature": 0.7
                }
                """, escapeJson(systemPrompt), escapeJson(prompt));
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.openai.com/v1/chat/completions"))
                .header("Authorization", "Bearer " + apiKey)
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
            
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseOpenAIResponse(response.body());
        }
        
        @Override
        public boolean isAvailable() {
            return apiKey != null;
        }
        
        @Override
        public String getStatus() {
            return isAvailable() ? "Connected" : "API Key Missing";
        }
        
        private String parseOpenAIResponse(String json) {
            try {
                // Simple JSON parsing for content
                int contentStart = json.indexOf("\"content\":\"") + 11;
                int contentEnd = json.indexOf("\"", contentStart);
                if (contentStart > 10 && contentEnd > contentStart) {
                    return json.substring(contentStart, contentEnd)
                        .replace("\\n", "\n")
                        .replace("\\\"", "\"")
                        .replace("\\\\", "\\");
                }
            } catch (Exception e) {
                // Fallback
            }
            return "OpenAI response received";
        }
    }
    
    // Claude Provider
    private class ClaudeProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        private final String apiKey = System.getenv("ANTHROPIC_API_KEY");
        
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            if (apiKey == null) {
                return "Claude API key not configured";
            }
            
            String systemPrompt = getSystemPrompt(type);
            String requestBody = String.format("""
                {
                    "model": "claude-3-5-sonnet-20241022",
                    "max_tokens": 2000,
                    "system": "%s",
                    "messages": [
                        {"role": "user", "content": "%s"}
                    ]
                }
                """, escapeJson(systemPrompt), escapeJson(prompt));
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.anthropic.com/v1/messages"))
                .header("x-api-key", apiKey)
                .header("Content-Type", "application/json")
                .header("anthropic-version", "2023-06-01")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
            
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseClaudeResponse(response.body());
        }
        
        @Override
        public boolean isAvailable() {
            return apiKey != null;
        }
        
        @Override
        public String getStatus() {
            return isAvailable() ? "Connected" : "API Key Missing";
        }
        
        private String parseClaudeResponse(String json) {
            try {
                // Simple JSON parsing for Claude response
                int textStart = json.indexOf("\"text\":\"") + 8;
                int textEnd = json.indexOf("\"", textStart);
                if (textStart > 7 && textEnd > textStart) {
                    return json.substring(textStart, textEnd)
                        .replace("\\n", "\n")
                        .replace("\\\"", "\"")
                        .replace("\\\\", "\\");
                }
            } catch (Exception e) {
                // Fallback
            }
            return "Claude response received";
        }
    }
    
    // GitHub Copilot Provider
    private class CopilotProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        private final String token = System.getenv("GITHUB_TOKEN");
        
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            if (token == null) {
                return "GitHub token not configured";
            }
            
            String requestBody = String.format("""
                {
                    "prompt": "%s",
                    "max_tokens": 1000,
                    "temperature": 0.2
                }
                """, escapeJson(prompt));
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.github.com/copilot/completions"))
                .header("Authorization", "Bearer " + token)
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
            
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseCopilotResponse(response.body());
        }
        
        @Override
        public boolean isAvailable() {
            return token != null;
        }
        
        @Override
        public String getStatus() {
            return isAvailable() ? "Connected" : "GitHub Token Missing";
        }
        
        private String parseCopilotResponse(String json) {
            return "Copilot: " + json.substring(0, Math.min(100, json.length()));
        }
    }
    
    // Amazon Q Provider
    private class AmazonQProvider implements AIProvider {
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            // Mock implementation - replace with actual Amazon Q integration
            Thread.sleep(500); // Simulate network call
            return "Amazon Q: " + getSystemPrompt(type) + "\n\nFor: " + prompt.substring(0, Math.min(50, prompt.length()));
        }
        
        @Override
        public boolean isAvailable() {
            return true; // Mock availability
        }
        
        @Override
        public String getStatus() {
            return "Mock Available";
        }
    }
    
    // Local AI Provider
    private class LocalAIProvider implements AIProvider {
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            // Mock local AI processing
            Thread.sleep(300);
            return "Local AI: Processing " + type + " for prompt length " + prompt.length();
        }
        
        @Override
        public boolean isAvailable() {
            return true;
        }
        
        @Override
        public String getStatus() {
            return "Local Engine Ready";
        }
    }
    
    // Utility methods
    private String getSystemPrompt(RequestType type) {
        return switch (type) {
            case CODE_GENERATION -> "You are an expert programmer. Generate clean, efficient code.";
            case CODE_EXPLANATION -> "You are a coding teacher. Explain code clearly and concisely.";
            case CODE_REVIEW -> "You are a senior developer. Review code for best practices and improvements.";
            case DEBUGGING -> "You are a debugging expert. Help identify and fix issues.";
            case REFACTORING -> "You are a refactoring expert. Suggest improvements for code quality.";
            case CHAT -> "You are a helpful AI programming assistant.";
        };
    }
    
    private String escapeJson(String str) {
        return str.replace("\\", "\\\\")
                  .replace("\"", "\\\"")
                  .replace("\n", "\\n")
                  .replace("\r", "\\r")
                  .replace("\t", "\\t");
    }
    
    private void completeMultiProviders() {
        // Example: show a dialog to select providers
        List<String> allProviders = Arrays.asList("copilot", "kimi");
        JCheckBox copilotBox = new JCheckBox("Copilot", true);
        JCheckBox kimiBox = new JCheckBox("Kimi", true);
        Object[] params = { "Select providers:", copilotBox, kimiBox };
        int res = JOptionPane.showConfirmDialog(null, params, "Multi-Provider Completion", JOptionPane.OK_CANCEL_OPTION);
        if (res == JOptionPane.OK_OPTION) {
            List<String> selected = new ArrayList<>();
            if (copilotBox.isSelected()) selected.add("copilot");
            if (kimiBox.isSelected()) selected.add("kimi");
            String text = editor.getSelectedText();
            if (text == null) text = editor.getText();
            copilot.completeMulti(text, selected).thenAccept(results -> {
                SwingUtilities.invokeLater(() -> {
                    StringBuilder sb = new StringBuilder();
                    results.forEach((provider, completion) -> {
                        sb.append("[").append(provider).append("]\n").append(completion).append("\n\n");
                    });
                    editor.append("\n" + sb.toString());
                });
            });
        }
    }
    
    private void completeCode() {
        String text = editor.getSelectedText();
        if (text == null) text = editor.getText();
        // Remove emojis before sending to Copilot
        text = EmojiRemover.removeEmojisAdvanced(text);
        copilot.complete(text).whenComplete((result, error) -> {
            SwingUtilities.invokeLater(() -> {
                if (error == null) {
                    // Remove emojis from Copilot result before displaying
                    String cleanedResult = EmojiRemover.removeEmojisAdvanced(result);
                    editor.append("\n" + cleanedResult);
                } else {
                    JOptionPane.showMessageDialog(null, "Error: " + error.getMessage());
                }
            });
        });
    }
    
    // Menu item for multi-provider completion
    public void addMultiProviderMenuItem(JMenu toolsMenu) {
        JMenuItem multiCompleteItem = new JMenuItem("Complete with Multiple Providers");
        multiCompleteItem.addActionListener(e -> completeMultiProviders());
        toolsMenu.add(multiCompleteItem);
    }
}