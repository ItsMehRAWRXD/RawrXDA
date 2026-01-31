import java.net.*;
import java.nio.*;
import java.nio.file.*;
import java.util.zip.*;
import java.io.*;
import java.util.concurrent.*;
import java.security.MessageDigest;

public class WebSocketBuildServer {
    private ServerSocket server;
    private final ExecutorService executor = Executors.newCachedThreadPool();
    
    public void start(int port) throws IOException {
        server = new ServerSocket(port);
        System.out.println("Build server started on ws://localhost:" + port + "/burrr");
        
        while (!server.isClosed()) {
            Socket client = server.accept();
            executor.submit(() -> handleClient(client));
        }
    }
    
    private void handleClient(Socket client) {
        try (client) {
            InputStream in = client.getInputStream();
            OutputStream out = client.getOutputStream();
            
            // Simple WebSocket handshake
            BufferedReader reader = new BufferedReader(new InputStreamReader(in));
            String line;
            while ((line = reader.readLine()) != null && !line.isEmpty()) {
                if (line.startsWith("Sec-WebSocket-Key:")) {
                    String key = line.split(": ")[1];
                    MessageDigest sha1 = MessageDigest.getInstance("SHA-1");
                    String accept = java.util.Base64.getEncoder()
                        .encodeToString(sha1.digest((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
                        .getBytes()));
                    
                    out.write(("HTTP/1.1 101 Switching Protocols\r\n" +
                              "Upgrade: websocket\r\n" +
                              "Connection: Upgrade\r\n" +
                              "Sec-WebSocket-Accept: " + accept + "\r\n\r\n").getBytes());
                    break;
                }
            }
            
            // Handle WebSocket frames
            byte[] buffer = new byte[8192];
            while (true) {
                int len = in.read(buffer);
                if (len <= 0) break;
                
                byte opcode = buffer[0];
                switch (opcode) {
                    case 0x01 -> handleZip(out, buffer, len);
                    case 0x02 -> handleBuild(out);
                }
            }
        } catch (Exception e) {
            System.err.println("Client error: " + e.getMessage());
        }
    }
    
    private void handleZip(OutputStream out, byte[] data, int len) throws IOException {
        Path workDir = Files.createTempDirectory("burrr");
        
        try (ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(data, 1, len-1))) {
            ZipEntry entry;
            while ((entry = zis.getNextEntry()) != null) {
                Path file = workDir.resolve(entry.getName());
                Files.createDirectories(file.getParent());
                Files.copy(zis, file);
            }
        }
        
        sendMessage(out, "{\"type\":\"OK\",\"workDir\":\"" + workDir + "\"}");
    }
    
    private void handleBuild(OutputStream out) throws IOException {
        ProcessBuilder pb = new ProcessBuilder("javac", "*.java");
        pb.directory(new File("."));
        
        Process proc = pb.start();
        
        // Stream build output
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(proc.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                sendMessage(out, "{\"type\":\"LOG\",\"level\":\"INFO\",\"message\":\"" + line + "\"}");
            }
        }
        
        try {
            int exitCode = proc.waitFor();
            if (exitCode == 0) {
                sendMessage(out, "{\"type\":\"ARTIFACT\",\"file\":\"compiled.jar\"}");
            } else {
                sendMessage(out, "{\"type\":\"ERROR\",\"message\":\"Build failed\"}");
            }
        } catch (InterruptedException e) {
            sendMessage(out, "{\"type\":\"ERROR\",\"message\":\"Build interrupted\"}");
        }
        
        sendMessage(out, "{\"type\":\"DONE\"}");
    }
    
    private void sendMessage(OutputStream out, String message) throws IOException {
        byte[] data = message.getBytes();
        out.write(0x81); // Text frame
        out.write(data.length);
        out.write(data);
        out.flush();
    }
    
    public static void main(String[] args) throws IOException {
        new WebSocketBuildServer().start(8080);
    }
}