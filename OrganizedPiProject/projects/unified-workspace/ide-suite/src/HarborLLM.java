import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;

public class HarborLLM {
    private static final HttpClient client = HttpClient.newHttpClient();
    
    public static class Provider {
        private final String name;
        private final String endpoint;
        private final String apiKey;
        
        public Provider(String name, String endpoint, String apiKey) {
            this.name = name;
            this.endpoint = endpoint;
            this.apiKey = apiKey;
        }
        
        public CompletableFuture<String> complete(String prompt) {
            return CompletableFuture.supplyAsync(() -> {
                try {
                    String body = String.format(
                        "{\"prompt\":\"%s\",\"max_tokens\":100}", 
                        prompt.replace("\"", "\\\"")
                    );
                    
                    HttpRequest request = HttpRequest.newBuilder()
                        .uri(URI.create(endpoint))
                        .header("Authorization", "Bearer " + apiKey)
                        .header("Content-Type", "application/json")
                        .POST(HttpRequest.BodyPublishers.ofString(body))
                        .build();
                    
                    HttpResponse<String> response = client.send(request, 
                        HttpResponse.BodyHandlers.ofString());
                    
                    return parseResponse(response.body());
                } catch (Exception e) {
                    return "Error: " + e.getMessage();
                }
            });
        }
        
        private String parseResponse(String json) {
            // Simple JSON parsing for completion
            int start = json.indexOf("\"text\":\"") + 8;
            if (start > 7) {
                int end = json.indexOf("\"", start);
                return json.substring(start, end);
            }
            return json;
        }
        
        public String getName() { return name; }
    }
    
    public static Provider openai(String apiKey) {
        return new Provider("OpenAI", "https://api.openai.com/v1/completions", apiKey);
    }
    
    public static Provider anthropic(String apiKey) {
        return new Provider("Anthropic", "https://api.anthropic.com/v1/complete", apiKey);
    }
    
    public static Provider local(String endpoint) {
        return new Provider("Local", endpoint, "");
    }
}