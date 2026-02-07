import java.io.*;
import java.util.concurrent.CompletableFuture;

public class TSBridge {
    private final ProcessBuilder nodeProcess;
    
    public TSBridge() {
        this.nodeProcess = new ProcessBuilder("node", "ai-first-editor/dist/cli.js");
        this.nodeProcess.directory(new File("."));
    }
    
    public CompletableFuture<String> complete(String prompt) {
        return CompletableFuture.supplyAsync(() -> {
            try {
                Process proc = nodeProcess.start();
                try (PrintWriter writer = new PrintWriter(proc.getOutputStream())) {
                    writer.println("/complete \"" + prompt + "\"");
                    writer.println("/exit");
                }
                
                StringBuilder result = new StringBuilder();
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream()))) {
                    String line;
                    while ((line = reader.readLine()) != null) {
                        if (!line.startsWith(">") && !line.contains("CLI")) {
                            result.append(line).append("\n");
                        }
                    }
                }
                
                proc.waitFor();
                return result.toString().trim();
            } catch (Exception e) {
                return "Error: " + e.getMessage();
            }
        });
    }
    
    public void setProvider(String provider) {
        try {
            Process proc = nodeProcess.start();
            try (PrintWriter writer = new PrintWriter(proc.getOutputStream())) {
                writer.println("/switch " + provider);
                writer.println("/exit");
            }
            proc.waitFor();
        } catch (Exception e) {
            System.err.println("Failed to set provider: " + e.getMessage());
        }
    }
}