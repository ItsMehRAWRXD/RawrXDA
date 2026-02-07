import java.util.*;
import java.util.concurrent.*;
import java.util.function.Function;

public class CursorLikeIDE {
    private final Map<String, Function<String, String>> providers = new HashMap<>();
    private final ExecutorService executor = Executors.newVirtualThreadPerTaskExecutor();
    private String activeProvider = "gemini";
    
    public CursorLikeIDE() {
        providers.put("gemini", input -> "Gemini: " + input);
        providers.put("chatgpt", input -> "ChatGPT: " + input);
        providers.put("claude", input -> "Claude: " + input);
        providers.put("AmazonQ", input -> "AmazonQ: " + input);
    }
    
    public CompletableFuture<String> complete(String code, int cursor) {
        return CompletableFuture
            .supplyAsync(() -> providers.get(activeProvider).apply(code), executor)
            .orTimeout(400, TimeUnit.MILLISECONDS)
            .exceptionally(ex -> "// AI completion failed");
    }
    
    public CompletableFuture<String> chat(String message) {
        return CompletableFuture
            .supplyAsync(() -> providers.get(activeProvider).apply("Chat: " + message), executor)
            .orTimeout(2, TimeUnit.SECONDS);
    }
    
    public void switchProvider(String provider) {
        if (providers.containsKey(provider)) activeProvider = provider;
    }
    
    public String[] getProviders() { return providers.keySet().toArray(new String[0]); }
    
    public void shutdown() { executor.shutdown(); }
}