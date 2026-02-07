import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.JsonNode;

public class ChatGPTClient {
    private final HttpClient client = HttpClient.newHttpClient();
    private final ObjectMapper mapper = new ObjectMapper();
    private final String apiKey;
    
    public ChatGPTClient(String apiKey) {
        this.apiKey = apiKey;
    }
    
    public CompletableFuture<String> complete(String prompt) {
        try {
            String requestBody = String.format(
                "{\"model\":\"gpt-4\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":1000}", 
                prompt.replace("\"", "\\\"")
            );
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.openai.com/v1/chat/completions"))
                .header("Authorization", "Bearer " + apiKey)
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
                
            return client.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(this::parseResponse);
        } catch (Exception e) {
            return CompletableFuture.completedFuture("Error: " + e.getMessage());
        }
    }
    
    public CompletableFuture<String> refreshModels() {
        try {
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.openai.com/v1/models"))
                .header("Authorization", "Bearer " + apiKey)
                .GET()
                .build();
                
            return client.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(response -> "Models refreshed: " + response.statusCode());
        } catch (Exception e) {
            return CompletableFuture.completedFuture("Refresh failed: " + e.getMessage());
        }
    }
    
    private String parseResponse(HttpResponse<String> response) {
        try {
            JsonNode json = mapper.readTree(response.body());
            return json.path("choices").get(0).path("message").path("content").asText();
        } catch (Exception e) {
            return "Parse error: " + e.getMessage();
        }
    }
}