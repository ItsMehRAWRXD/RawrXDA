import java.util.concurrent.CompletableFuture;

public class SystemOrchestrator {
    private final LiveAIService aiService;
    private final RealtimeProcessor processor;
    private final NetworkLayer network;
    private final StateManager<String, Object> state;
    
    public SystemOrchestrator() {
        this.aiService = new LiveAIService(input -> "AI Response: " + input);
        this.processor = new RealtimeProcessor(() -> System.out.println("Processing..."));
        this.network = new NetworkLayer();
        this.state = new StateManager<>();
    }
    
    public CompletableFuture<Void> start() {
        return CompletableFuture.runAsync(() -> {
            try {
                network.start(8080);
                processor.start();
                state.put("status", "running");
                System.out.println("System fully operational");
            } catch (Exception e) {
                System.err.println("Startup failed: " + e.getMessage());
            }
        });
    }
    
    public void shutdown() {
        processor.stop();
        network.stop();
        aiService.shutdown();
        state.clear();
    }
}