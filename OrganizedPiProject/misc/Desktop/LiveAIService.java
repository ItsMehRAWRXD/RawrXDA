import java.util.concurrent.*;
import java.util.function.Function;

public class LiveAIService {
    private final ExecutorService executor = Executors.newVirtualThreadPerTaskExecutor();
    private final Function<String, String> aiProvider;
    
    public LiveAIService(Function<String, String> provider) {
        this.aiProvider = provider;
    }
    
    public CompletableFuture<String> process(String input) {
        return CompletableFuture
            .supplyAsync(() -> aiProvider.apply(input), executor)
            .orTimeout(2, TimeUnit.SECONDS)
            .exceptionally(ex -> "Error: " + ex.getMessage());
    }
    
    public void shutdown() { executor.shutdown(); }
}