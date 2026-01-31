import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class EnhancedCursorIDE {
    private final HttpClient client = HttpClient.newHttpClient();
    private final Map<String, String> keys = new ConcurrentHashMap<>();
    private String active = "openai";

    public CompletableFuture<String> complete(String code, int cursor) {
        return switch (active) {
            case "openai" -> openAI(code, cursor);
            case "gemini" -> gemini(code, cursor);
            case "claude" -> claude(code, cursor);
            default -> CompletableFuture.failedFuture(new IllegalStateException("Unknown provider"));
        };
    }

    private CompletableFuture<String> openAI(String code, int cursor) {
        String json = "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"user\",\"content\":\"" + code + "\"}]}";
        HttpRequest req = HttpRequest.newBuilder()
                .uri(URI.create("https://api.openai.com/v1/chat/completions"))
                .header("Authorization", "Bearer " + keys.get("openai"))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(json))
                .build();
        return client.sendAsync(req, HttpResponse.BodyHandlers.ofString())
                .thenApply(resp -> resp.statusCode() == 200 ?
                        Json.parse(resp.body()).getJsonArray("choices")
                                .getJsonObject(0).getJsonObject("message").getString("content") :
                        "Error: " + resp.statusCode());
    }

    private CompletableFuture<String> gemini(String code, int cursor) {
        String json = "{\"contents\":[{\"parts\":[{\"text\":\"" + code + "\"}]}]}";
        HttpRequest req = HttpRequest.newBuilder()
                .uri(URI.create("https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=" + keys.get("gemini")))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(json))
                .build();
        return client.sendAsync(req, HttpResponse.BodyHandlers.ofString())
                .thenApply(resp -> resp.statusCode() == 200 ?
                        Json.parse(resp.body()).getJsonArray("candidates")
                                .getJsonObject(0).getJsonObject("content").getJsonArray("parts")
                                .getJsonObject(0).getString("text") :
                        "Error: " + resp.statusCode());
    }

    private CompletableFuture<String> claude(String code, int cursor) {
        String json = "{\"model\":\"claude-3-haiku-20240307\",\"messages\":[{\"role\":\"user\",\"content\":\"" + code + "\"}]}";
        HttpRequest req = HttpRequest.newBuilder()
                .uri(URI.create("https://api.anthropic.com/v1/messages"))
                .header("x-api-key", keys.get("claude"))
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(json))
                .build();
        return client.sendAsync(req, HttpResponse.BodyHandlers.ofString())
                .thenApply(resp -> resp.statusCode() == 200 ?
                        Json.parse(resp.body()).getJsonArray("content")
                                .getJsonObject(0).getString("text") :
                        "Error: " + resp.statusCode());
    }

    public void setApiKey(String provider, String key) { keys.put(provider, key); }
    public void switchProvider(String provider) { this.active = provider; }
    public boolean hasApiKey(String provider) { return keys.containsKey(provider); }
    public String getActiveProvider() { return active; }
    public String[] getProviders() { return new String[]{"openai", "gemini", "claude"}; }
    public void shutdown() { /* nothing to close */ }
}