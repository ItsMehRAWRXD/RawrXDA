import java.util.concurrent.*;

public class InlineAssistant {
    private final CursorLikeIDE ide;
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private volatile String lastCode = "";
    private volatile int lastCursor = 0;
    
    public InlineAssistant(CursorLikeIDE ide) {
        this.ide = ide;
    }
    
    public void onCodeChange(String code, int cursor) {
        if (!code.equals(lastCode) || cursor != lastCursor) {
            lastCode = code;
            lastCursor = cursor;
            
            scheduler.schedule(() -> {
                if (code.equals(lastCode) && cursor == lastCursor) {
                    ide.complete(code, cursor)
                        .thenAccept(suggestion -> System.out.println("[SUGGESTION] " + suggestion));
                }
            }, 300, TimeUnit.MILLISECONDS);
        }
    }
    
    public void shutdown() { scheduler.shutdown(); }
}