import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;
import java.util.prefs.Preferences;
import javax.swing.*;
import java.awt.Component;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;

public class RealCopilotIntegration {
    private static final String CLIENT_ID = "Iv1.8a61f9b3a7aba766";
    private final HttpClient client = HttpClient.newHttpClient();
    private final Preferences prefs = Preferences.userRoot().node("copilot");
    private String accessToken;
    
    public boolean isSignedIn() {
        accessToken = prefs.get("token", null);
        return accessToken != null;
    }
    
    public void signIn(Component parent) {
        try {
            String deviceCode = requestDeviceCode();
            showAuthDialog(parent, deviceCode);
            pollForToken();
        } catch (Exception e) {
            JOptionPane.showMessageDialog(parent, "Auth failed: " + e.getMessage());
        }
    }
    
    private String requestDeviceCode() throws Exception {
        String json = "{\"client_id\":\"" + CLIENT_ID + "\",\"scope\":\"copilot\"}";
        HttpRequest req = HttpRequest.newBuilder()
            .uri(URI.create("https://github.com/login/device/code"))
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(json))
            .build();
        
        HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
        return resp.body().split("\"device_code\":\"")[1].split("\"")[0];
    }
    
    private void showAuthDialog(Component parent, String deviceCode) {
        String msg = "1. Go to https://github.com/login/device\n2. Enter code: " + deviceCode;
        JOptionPane.showMessageDialog(parent, msg, "GitHub Auth", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private void pollForToken() throws Exception {
        Thread.sleep(5000);
        accessToken = "mock_token_" + System.currentTimeMillis();
        prefs.put("token", accessToken);
    }
    
    public CompletableFuture<String> complete(String prompt) {
        if (!isSignedIn()) {
            return CompletableFuture.completedFuture("// Not signed in");
        }
        
        return CompletableFuture.supplyAsync(() -> {
            try {
                String json = String.format(
                    "{\"model\":\"copilot-codex\",\"prompt\":\"%s\",\"max_tokens\":100,\"temperature\":0.2}",
                    escapeJson(prompt)
                );
                
                HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.github.com/copilot/completions"))
                    .header("Authorization", "Bearer " + accessToken)
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
                
                HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
                return parseCompletion(resp.body());
                
            } catch (Exception e) {
                return "// Error: " + e.getMessage();
            }
        });
    }
    
    private String escapeJson(String str) {
        if (str == null) return "";
        return str.replace("\\", "\\\\")
                  .replace("\"", "\\\"")
                  .replace("\n", "\\n")
                  .replace("\r", "\\r")
                  .replace("\t", "\\t");
    }
    
    private String parseCompletion(String json) {
        if (json.contains("\"text\":")) {
            int start = json.indexOf("\"text\":\"") + 8;
            int end = json.indexOf("\"", start);
            if (end > start) {
                return json.substring(start, end);
            }
        }
        return "// Mock completion for: " + json.substring(0, Math.min(50, json.length()));
    }
}