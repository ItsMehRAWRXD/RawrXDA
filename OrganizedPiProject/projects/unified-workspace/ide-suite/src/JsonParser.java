import java.util.*;
import java.util.regex.*;

// Simple JSON parser for basic API response handling
public class JsonParser {
    public static JsonObject parse(String json) {
        return new JsonObject(json);
    }
}

class JsonObject {
    private final String json;
    
    public JsonObject(String json) {
        this.json = json;
    }
    
    public JsonArray getJsonArray(String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\\[(.*?)\\]", Pattern.DOTALL);
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return new JsonArray(matcher.group(1));
        }
        throw new RuntimeException("Array not found: " + key);
    }
    
    public JsonObject getJsonObject(String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\\{(.*?)\\}", Pattern.DOTALL);
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return new JsonObject("{" + matcher.group(1) + "}");
        }
        throw new RuntimeException("Object not found: " + key);
    }
    
    public String getString(String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\"(.*?)\"");
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return matcher.group(1).replace("\\n", "\n").replace("\\\"", "\"");
        }
        throw new RuntimeException("String not found: " + key);
    }
}

class JsonArray {
    private final String json;
    
    public JsonArray(String json) {
        this.json = json;
    }
    
    public JsonObject getJsonObject(int index) {
        // Simple implementation - assumes first object for index 0
        if (index == 0) {
            Pattern pattern = Pattern.compile("\\{(.*?)\\}", Pattern.DOTALL);
            Matcher matcher = pattern.matcher(json);
            if (matcher.find()) {
                return new JsonObject("{" + matcher.group(1) + "}");
            }
        }
        throw new RuntimeException("Object not found at index: " + index);
    }
}