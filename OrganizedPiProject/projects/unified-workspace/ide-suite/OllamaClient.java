import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.JsonNode;

public class OllamaClient {
    private final HttpClient client = HttpClient.newHttpClient();
    private final ObjectMapper mapper = new ObjectMapper();
    private final String baseUrl;
    
    public OllamaClient(String baseUrl) {
        this.baseUrl = baseUrl != null ? baseUrl : "http://localhost:11434";
    }
    
    public CompletableFuture<String> generate(String model, String prompt) {
        try {
            String requestBody = String.format(
                "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", 
                model, prompt.replace("\"", "\\\"")
            );
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + "/api/generate"))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
                
            return client.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(this::parseResponse);
        } catch (Exception e) {
            return CompletableFuture.completedFuture("Error: " + e.getMessage());
        }
    }
    
    public CompletableFuture<Boolean> isAvailable() {
        try {
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + "/api/tags"))
                .GET()
                .build();
            return client.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(response -> response.statusCode() == 200);
        } catch (Exception e) {
            return CompletableFuture.completedFuture(false);
        }
    }
    
    private String parseResponse(HttpResponse<String> response) {
        try {
            JsonNode json = mapper.readTree(response.body());
            return json.has("response") ? json.get("response").asText() : "No response";
        } catch (Exception e) {
            return "Parse error: " + e.getMessage();
        }
    }
    
    // Amazon Q Provider
    private class AmazonQProvider implements AIProvider {
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            // Real Amazon Q integration
            String cleanPrompt = EmojiRemover.removeEmojisAdvanced(prompt);
            AmazonQClient client = new AmazonQClient(/* credentials/config */);
            String response = client.query(cleanPrompt, type);
            return EmojiRemover.removeEmojisAdvanced(response);
        }
        @Override
        public boolean isAvailable() {
            // Check actual Amazon Q availability
            return AmazonQClient.isAvailable();
        }
        @Override
        public String getStatus() {
            return AmazonQClient.getStatus();
        }
    }

    // Local AI Provider
    private class LocalAIProvider implements AIProvider {
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            String cleanPrompt = EmojiRemover.removeEmojisAdvanced(prompt);
            LocalAIEngine engine = LocalAIEngine.getInstance();
            String response = engine.process(cleanPrompt, type);
            return EmojiRemover.removeEmojisAdvanced(response);
        }
        @Override
        public boolean isAvailable() {
            return LocalAIEngine.isAvailable();
        }
        @Override
        public String getStatus() {
            return LocalAIEngine.getStatus();
        }
    }
}