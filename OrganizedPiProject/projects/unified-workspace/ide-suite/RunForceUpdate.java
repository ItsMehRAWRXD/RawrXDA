import java.io.*;
import java.nio.file.*;
import java.util.stream.Stream;

public class RunForceUpdate {
    public static void main(String[] args) {
        ForceAIUpdate.enableAutoAccept();
        
        try (Stream<Path> paths = Files.walk(Paths.get("."))) {
            paths.filter(Files::isRegularFile)
                 .filter(p -> p.toString().endsWith(".java"))
                 .forEach(RunForceUpdate::processFile);
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
        }
    }
    
    private static void processFile(Path file) {
        try {
            String content = Files.readString(file);
            String updated = content.replaceAll("// COMPLETED
                                   .replaceAll("DONE:", "DONE:")
                                   .replaceAll("FIXED:", "FIXED:");
            ForceAIUpdate.forceUpdate(file.toString(), updated);
        } catch (Exception e) {
            System.err.println("Failed to process: " + file);
        }
    }
}