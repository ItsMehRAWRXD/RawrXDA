import java.util.*;
import java.io.*;
import java.nio.file.*;

public class ApiKeyManager {
    private final Map<String, String> keys = new HashMap<>();
    private final Path configPath = Paths.get(System.getProperty("user.home"), ".continue", "api-keys.json");
    
    public ApiKeyManager() {
        loadKeys();
    }
    
    public void setKey(String provider, String key) {
        keys.put(provider, key);
        saveKeys();
    }
    
    public void setApiKey(String provider, String key) {
        setKey(provider, key);
    }
    
    public String getKey(String provider) {
        return keys.get(provider);
    }
    
    public boolean hasKey(String provider) {
        return keys.containsKey(provider) && !keys.get(provider).isEmpty();
    }
    
    private void loadKeys() {
        try {
            if (Files.exists(configPath)) {
                String content = Files.readString(configPath);
                content = content.replaceAll("[{}\"\\s]", "");
                for (String pair : content.split(",")) {
                    String[] kv = pair.split(":");
                    if (kv.length == 2) keys.put(kv[0], kv[1]);
                }
            }
        } catch (Exception e) {
            System.err.println("Failed to load API keys: " + e.getMessage());
        }
    }
    
    private void saveKeys() {
        try {
            Files.createDirectories(configPath.getParent());
            StringBuilder json = new StringBuilder("{");
            keys.forEach((k, v) -> json.append("\"").append(k).append("\":\"").append(v).append("\","));
            if (json.length() > 1) json.setLength(json.length() - 1);
            json.append("}");
            Files.writeString(configPath, json.toString());
        } catch (Exception e) {
            System.err.println("Failed to save API keys: " + e.getMessage());
        }
    }
}