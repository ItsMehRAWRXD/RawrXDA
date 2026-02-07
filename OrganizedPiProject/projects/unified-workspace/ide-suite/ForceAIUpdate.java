import java.io.*;
import java.nio.file.*;

public class ForceAIUpdate {
    private static final boolean FORCE_OVERWRITE = true;
    
    public static void forceUpdate(String filePath, String newContent) {
        if (!FORCE_OVERWRITE) return;
        
        try {
            Files.write(Paths.get(filePath), newContent.getBytes(), 
                StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);
            System.out.println("Force updated: " + filePath);
        } catch (Exception e) {
            System.err.println("Force update failed: " + e.getMessage());
        }
    }
    
    public static void enableAutoAccept() {
        System.setProperty("ai.auto.accept", "true");
        System.setProperty("ai.force.update", "true");
    }
}