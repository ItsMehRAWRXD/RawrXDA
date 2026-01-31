import java.net.http.*;
import java.net.URI;

public class KimiCopilot {
    private static final String KIMI_ENDPOINT = "http://localhost:11434/v1/chat/completions";
    private static final HttpClient client = HttpClient.newHttpClient();
    
    public static String complete(String prompt) {
        try {
            String body = String.format(
                "{\"model\":\"moonshotai/kimi-k2\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}", 
                prompt.replace("\"", "\\\"")
            );
            
            HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(KIMI_ENDPOINT))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(body))
                .build();
            
            HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
            return parseResponse(response.body());
        } catch (Exception e) {
            return "Error: " + e.getMessage();
        }
    }
    
    private static String parseResponse(String json) {
        int start = json.indexOf("\"content\":\"") + 11;
        if (start > 10) {
            int end = json.indexOf("\"", start);
            return json.substring(start, end);
        }
        return json;
    }
}