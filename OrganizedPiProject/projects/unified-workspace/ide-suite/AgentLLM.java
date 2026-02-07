import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;

public class AgentLLM {
    private final AgenticOrchestratorSimple orchestrator;
    private final TSBridge tsBridge;
    
    public AgentLLM(String apiKey) {
        this.orchestrator = new AgenticOrchestratorSimple(apiKey, java.nio.file.Paths.get("plugins"));
        this.tsBridge = new TSBridge();
    }
    
    public CompletableFuture<String> complete(String prompt) {
        // Try TypeScript providers first, fallback to orchestrator
        return tsBridge.complete(prompt)
            .exceptionally(error -> {
                try {
                    return orchestrator.execute("user", prompt);
                } catch (Exception e) {
                    return "Error: " + e.getMessage();
                }
            });
    }
    
    public void setProvider(String provider) {
        tsBridge.setProvider(provider);
    }
    
    public AgenticOrchestratorSimple getOrchestrator() {
        return orchestrator;
    }
    
    public void stream(String prompt, Consumer<String> onToken) {
        orchestrator.streamExecution(event -> {
            if ("task_completed".equals(event.getType())) {
                onToken.accept((String) event.getMetadata().get("result"));
            }
        }, prompt, "user");
    }
}