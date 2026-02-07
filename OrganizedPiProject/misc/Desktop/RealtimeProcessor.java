import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public class RealtimeProcessor {
    private final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(2);
    private final AtomicBoolean running = new AtomicBoolean(false);
    private final Runnable task;
    
    public RealtimeProcessor(Runnable task) {
        this.task = task;
    }
    
    public void start() {
        if (running.compareAndSet(false, true)) {
            scheduler.scheduleAtFixedRate(() -> {
                try { task.run(); } 
                catch (Exception e) { System.err.println("Task error: " + e.getMessage()); }
            }, 0, 100, TimeUnit.MILLISECONDS);
        }
    }
    
    public void stop() {
        running.set(false);
        scheduler.shutdown();
    }
}