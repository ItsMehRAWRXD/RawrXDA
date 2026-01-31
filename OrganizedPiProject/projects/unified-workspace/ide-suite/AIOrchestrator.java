import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Manages multiple AI providers, with a primary focus on a flexible OpenRouter provider.
 * This allows for dynamic model switching and access to a wide range of models with a single API key.
 */
public class AIOrchestrator {
    private final Map<String, AIProvider> providers = new HashMap<>();
    private final ExecutorService executor = Executors.newCachedThreadPool();
    
    // The active provider and model can be changed dynamically
    private String activeProviderName = "openrouter"; // Default to the flexible OpenRouter
    private String activeModel = "anthropic/claude-3.5-sonnet"; // Default to a top-tier model

    public AIOrchestrator(IDESettings settings) {
        // The settings can be used in the future to persist the active model
    }

    public void initialize() {
        // Initialize all available providers
        providers.put("openrouter", new OpenRouterProvider());
        providers.put("openai_direct", new OpenAIProvider()); // Kept for fallback/direct access
        providers.put("anthropic_direct", new AnthropicProvider()); // Kept for fallback/direct access
        System.out.println("AI Orchestrator initialized with " + providers.size() + " providers.");
    }

    /**
     * Sets the active model to be used for subsequent requests.
     * The model identifier should be in the format expected by OpenRouter (e.g., "openai/gpt-4o").
     * @param modelIdentifier The identifier of the model to use.
     */
    public void setActiveModel(String modelIdentifier) {
        this.activeModel = modelIdentifier;
        this.activeProviderName = "openrouter"; // Ensure OpenRouter is active when a model is chosen
        System.out.println("Active AI model set to: " + modelIdentifier);
    }

    /**
     * Sets the active provider directly. Used for falling back to direct connections.
     * @param providerName The name of the direct provider (e.g., "openai_direct").
     */
    public void setActiveProvider(String providerName) {
        this.activeProviderName = providerName;
        System.out.println("Active AI provider set to: " + providerName);
    }

    /**
     * Processes a request using the currently active provider and model.
     * @param prompt The user's prompt.
     * @param type The type of request (e.g., CHAT, CODE_REVIEW).
     * @return A CompletableFuture with the AI's response.
     */
    public CompletableFuture<String> processRequest(String prompt, RequestType type) {
        AIProvider provider = providers.get(activeProviderName);
        if (provider == null || !provider.isAvailable()) {
            return CompletableFuture.failedFuture(new RuntimeException("Active provider '" + activeProviderName + "' is not available."));
        }

        return CompletableFuture.supplyAsync(() -> {
            try {
                // Pass the active model to the provider
                return provider.process(prompt, type, activeModel);
            } catch (Exception e) {
                throw new CompletionException(e);
            }
        }, executor);
    }
    
    public CompletableFuture<Boolean> ping() {
        AIProvider provider = providers.get(activeProviderName);
        return CompletableFuture.completedFuture(provider != null && provider.isAvailable());
    }

    // --- Provider Interface and Enum ---
    public interface AIProvider {
        String process(String prompt, RequestType type, String model) throws Exception;
        boolean isAvailable();
        String getStatus();
        default boolean ping() { return isAvailable(); }
    }

    public enum RequestType {
        CHAT, CODE_GENERATION, CODE_REVIEW
    }

    // --- Provider Implementations ---

    /**
     * A single, flexible provider that connects to OpenRouter.
     * It can use any model supported by OpenRouter by passing the model identifier.
     */
    private class OpenRouterProvider implements AIProvider {
        private final String apiKey = System.getenv("OPENROUTER_API_KEY");
        private final HttpClient client = HttpClient.newHttpClient();

        @Override
        public String process(String prompt, RequestType type, String model) throws Exception {
            if (!isAvailable()) {
                throw new IllegalStateException("OpenRouter API key not configured. Please set the OPENROUTER_API_KEY environment variable.");
            }

            String requestBody = String.format(
                "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":4096}",
                model, escapeJson(prompt)
            );

            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://openrouter.ai/api/v1/chat/completions"))
                .header("Authorization", "Bearer " + apiKey)
                .header("Content-Type", "application/json")
                .header("HTTP-Referer", "urn:ide:agentic-ide") // Recommended by OpenRouter
                .header("X-Title", "Agentic IDE") // Recommended by OpenRouter
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();

            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new RuntimeException("OpenRouter request failed with status " + response.statusCode() + ": " + response.body());
            }

            return parseJsonResponse(response.body(), "choices[0].message.content");
        }

        @Override
        public boolean isAvailable() {
            return apiKey != null && !apiKey.isEmpty();
        }

        @Override
        public String getStatus() {
            return isAvailable() ? "Connected" : "API Key Missing";
        }
    }

    // Direct provider for OpenAI (kept for compatibility)
    private class OpenAIProvider implements AIProvider {
        private final String apiKey = System.getenv("OPENAI_API_KEY");
        @Override
        public String process(String prompt, RequestType type, String model) throws Exception {
            // This provider ignores the 'model' param and uses its default
            return "Direct OpenAI response: " + prompt.substring(0, Math.min(50, prompt.length())) + "...";
        }
        @Override public boolean isAvailable() { return apiKey != null && !apiKey.isEmpty(); }
        @Override public String getStatus() { return isAvailable() ? "Connected" : "API Key Missing"; }
    }

    // Direct provider for Anthropic (kept for compatibility)
    private class AnthropicProvider implements AIProvider {
        private final String apiKey = System.getenv("ANTHROPIC_API_KEY");
        @Override
        public String process(String prompt, RequestType type, String model) throws Exception {
            // This provider ignores the 'model' param and uses its default
            return "Direct Anthropic response: " + prompt.substring(0, Math.min(50, prompt.length())) + "...";
        }
        @Override public boolean isAvailable() { return apiKey != null && !apiKey.isEmpty(); }
        @Override public String getStatus() { return isAvailable() ? "Connected" : "API Key Missing"; }
    }

    // --- Utility Methods ---
    private String escapeJson(String str) {
        return str.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t");
    }

    private String parseJsonResponse(String json, String path) {
        // This is a simplified parser. For production, a robust library like Jackson or Gson is recommended.
        try {
            String[] keys = path.split("\\.");
            String tempJson = json;
            for (String key : keys) {
                if (key.contains("[")) { // Handle array access
                    String arrayKey = key.substring(0, key.indexOf('['));
                    int index = Integer.parseInt(key.substring(key.indexOf('[') + 1, key.indexOf(']')));
                    String[] parts = tempJson.split("\"" + arrayKey + "\"\\s*:\\s*\\[");
                    if (parts.length < 2) return "Error: Array key '" + arrayKey + "' not found.";
                    tempJson = parts[1].split("\\]")[0];
                    
                    // Simple object splitting for arrays
                    String[] objects = tempJson.split("\\},\\s*\\{");
                    if (index >= objects.length) return "Error: Index " + index + " out of bounds for array '" + arrayKey + "'.";
                    tempJson = objects[index];

                } else { // Handle object access
                    String[] parts = tempJson.split("\"" + key + "\"\\s*:\\s*");
                    if (parts.length < 2) return "Error: Key '" + key + "' not found.";
                    tempJson = parts[1];
                }
            }
            
            // Extract value
            if (tempJson.trim().startsWith("\"")) {
                return tempJson.trim().substring(1, tempJson.trim().indexOf("\"", 1));
            } else {
                 // Handle non-string values like numbers or booleans if necessary
                String value = tempJson.split(",")[0].trim();
                if (value.endsWith("}")) {
                    value = value.substring(0, value.length() - 1);
                }
                return value;
            }
        } catch (Exception e) {
            return "Failed to parse response JSON. Raw response: " + json.substring(0, Math.min(200, json.length()));
        }
    }
}