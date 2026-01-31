import java.util.*;
import java.util.regex.*;

public class Json {
    public static JsonObject parse(String json) {
        return new JsonObject(json);
    }
    
    public static class JsonObject {
        private final String json;
        
        public JsonObject(String json) {
            this.json = json;
        }
        
        public JsonArray getJsonArray(String key) {
            Pattern p = Pattern.compile("\"" + key + "\"\\s*:\\s*\\[(.*?)\\]");
            Matcher m = p.matcher(json);
            return m.find() ? new JsonArray("[" + m.group(1) + "]") : new JsonArray("[]");
        }
        
        public JsonObject getJsonObject(String key) {
            Pattern p = Pattern.compile("\"" + key + "\"\\s*:\\s*\\{(.*?)\\}");
            Matcher m = p.matcher(json);
            return m.find() ? new JsonObject("{" + m.group(1) + "}") : new JsonObject("{}");
        }
        
        public String getString(String key) {
            Pattern p = Pattern.compile("\"" + key + "\"\\s*:\\s*\"(.*?)\"");
            Matcher m = p.matcher(json);
            return m.find() ? m.group(1) : "";
        }
    }
    
    public static class JsonArray {
        private final String json;
        
        public JsonArray(String json) {
            this.json = json;
        }
        
        public JsonObject getJsonObject(int index) {
            String[] parts = json.substring(1, json.length()-1).split("\\},\\{");
            if (index < parts.length) {
                String obj = parts[index];
                if (!obj.startsWith("{")) obj = "{" + obj;
                if (!obj.endsWith("}")) obj = obj + "}";
                return new JsonObject(obj);
            }
            return new JsonObject("{}");
        }
    }
}