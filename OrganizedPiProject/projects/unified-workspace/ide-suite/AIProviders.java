import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;

public class AIProviders {
    private static final HttpClient client = HttpClient.newHttpClient();
    
    interface AIProvider {
        String process(String prompt, RequestType type) throws Exception;
        boolean isAvailable();
        String getStatus();
    }
    
    enum RequestType {
        CODE_COMPLETION, CHAT, REVIEW
    }
    
    // Amazon Q Provider
    static class AmazonQProvider implements AIProvider {
        private final String apiKey;
        
        public AmazonQProvider(String apiKey) {
            this.apiKey = apiKey;
        }
        
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            String requestBody = String.format(
                "{\"prompt\":\"%s\",\"type\":\"%s\",\"max_tokens\":1000}", 
                prompt.replace("\"", "\\\""), type
            );
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create("https://api.amazonq.aws/v1/completions"))
                .header("Authorization", "Bearer " + apiKey)
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
                
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseResponse(response.body());
        }
        
        @Override
        public boolean isAvailable() {
            try {
                HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.amazonq.aws/v1/status"))
                    .header("Authorization", "Bearer " + apiKey)
                    .GET()
                    .build();
                return client.send(request, HttpResponse.BodyHandlers.ofString()).statusCode() == 200;
            } catch (Exception e) {
                return false;
            }
        }
        
        @Override
        public String getStatus() {
            return isAvailable() ? "Amazon Q Connected" : "Amazon Q Unavailable";
        }
        
        private String parseResponse(String json) {
            int start = json.indexOf("\"text\":\"") + 8;
            int end = json.indexOf("\"", start);
            return start > 7 && end > start ? json.substring(start, end) : "No response";
        }
    }
    
    // Local AI Provider using Ollama
    static class LocalAIProvider implements AIProvider {
        private final String baseUrl;
        
        public LocalAIProvider(String baseUrl) {
            this.baseUrl = baseUrl != null ? baseUrl : "http://localhost:11434";
        }
        
        @Override
        public String process(String prompt, RequestType type) throws Exception {
            String model = type == RequestType.CODE_COMPLETION ? "codellama" : "llama2";
            String requestBody = String.format(
                "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}", 
                model, prompt.replace("\"", "\\\"")
            );
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(baseUrl + "/api/generate"))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                .build();
                
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseOllamaResponse(response.body());
        }
        
        @Override
        public boolean isAvailable() {
            try {
                HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(baseUrl + "/api/tags"))
                    .GET()
                    .build();
                return client.send(request, HttpResponse.BodyHandlers.ofString()).statusCode() == 200;
            } catch (Exception e) {
                return false;
            }
        }
        
        @Override
        public String getStatus() {
            return isAvailable() ? "Local AI Ready" : "Local AI Offline";
        }
        
        private String parseOllamaResponse(String json) {
            int start = json.indexOf("\"response\":\"") + 12;
            int end = json.indexOf("\"", start);
            return start > 11 && end > start ? json.substring(start, end) : "No response";
        }
    }
}