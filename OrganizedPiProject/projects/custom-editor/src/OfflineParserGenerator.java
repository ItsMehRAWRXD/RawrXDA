import java.util.*;
import java.util.regex.*;
import java.io.*;
import java.nio.file.*;

public class OfflineParserGenerator {
    private final Map<String, String> cachedParsers = new HashMap<>();
    private final Map<String, Pattern> grammarRules = new HashMap<>();
    
    public OfflineParserGenerator() {
        initializeBuiltinParsers();
        loadCachedParsers();
    }
    
    private void initializeBuiltinParsers() {
        // JSON parser
        cachedParsers.put("json", """
            public class JSONParser {
                public static Object parse(String json) {
                    json = json.trim();
                    if (json.startsWith("{")) return parseObject(json);
                    if (json.startsWith("[")) return parseArray(json);
                    if (json.startsWith("\"")) return json.substring(1, json.length()-1);
                    if (json.equals("true")) return true;
                    if (json.equals("false")) return false;
                    if (json.equals("null")) return null;
                    return Double.parseDouble(json);
                }
                private static Map<String,Object> parseObject(String json) {
                    Map<String,Object> map = new HashMap<>();
                    String content = json.substring(1, json.length()-1);
                    String[] pairs = content.split(",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)");
                    for (String pair : pairs) {
                        String[] kv = pair.split(":", 2);
                        String key = kv[0].trim().replaceAll("\"", "");
                        map.put(key, parse(kv[1].trim()));
                    }
                    return map;
                }
                private static List<Object> parseArray(String json) {
                    List<Object> list = new ArrayList<>();
                    String content = json.substring(1, json.length()-1);
                    String[] items = content.split(",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)");
                    for (String item : items) list.add(parse(item.trim()));
                    return list;
                }
            }
            """);
        
        // XML parser
        cachedParsers.put("xml", """
            public class XMLParser {
                public static Node parse(String xml) {
                    xml = xml.trim();
                    if (!xml.startsWith("<")) return new TextNode(xml);
                    int tagEnd = xml.indexOf(">");
                    String tagName = xml.substring(1, tagEnd);
                    if (xml.endsWith("</" + tagName + ">")) {
                        String content = xml.substring(tagEnd + 1, xml.lastIndexOf("<"));
                        return new ElementNode(tagName, content);
                    }
                    return new ElementNode(tagName, "");
                }
                static class Node { String content; Node(String c) { content = c; } }
                static class TextNode extends Node { TextNode(String c) { super(c); } }
                static class ElementNode extends Node { 
                    String tag; 
                    ElementNode(String t, String c) { super(c); tag = t; } 
                }
            }
            """);
        
        // CSV parser
        cachedParsers.put("csv", """
            public class CSVParser {
                public static List<String[]> parse(String csv) {
                    List<String[]> rows = new ArrayList<>();
                    String[] lines = csv.split("\\n");
                    for (String line : lines) {
                        rows.add(parseLine(line));
                    }
                    return rows;
                }
                private static String[] parseLine(String line) {
                    List<String> fields = new ArrayList<>();
                    boolean inQuotes = false;
                    StringBuilder field = new StringBuilder();
                    for (char c : line.toCharArray()) {
                        if (c == '"') inQuotes = !inQuotes;
                        else if (c == ',' && !inQuotes) {
                            fields.add(field.toString());
                            field = new StringBuilder();
                        } else field.append(c);
                    }
                    fields.add(field.toString());
                    return fields.toArray(new String[0]);
                }
            }
            """);
        
        // YAML parser
        cachedParsers.put("yaml", """
            public class YAMLParser {
                public static Map<String,Object> parse(String yaml) {
                    Map<String,Object> map = new HashMap<>();
                    String[] lines = yaml.split("\\n");
                    for (String line : lines) {
                        line = line.trim();
                        if (line.isEmpty() || line.startsWith("#")) continue;
                        if (line.contains(":")) {
                            String[] kv = line.split(":", 2);
                            String key = kv[0].trim();
                            String value = kv[1].trim();
                            if (value.startsWith("\"")) value = value.substring(1, value.length()-1);
                            map.put(key, parseValue(value));
                        }
                    }
                    return map;
                }
                private static Object parseValue(String value) {
                    if (value.equals("true")) return true;
                    if (value.equals("false")) return false;
                    if (value.equals("null")) return null;
                    try { return Integer.parseInt(value); } catch (Exception e) {}
                    try { return Double.parseDouble(value); } catch (Exception e) {}
                    return value;
                }
            }
            """);
    }
    
    public String generateParser(String format, String grammar) {
        // Check cache first
        if (cachedParsers.containsKey(format.toLowerCase())) {
            return cachedParsers.get(format.toLowerCase());
        }
        
        // Generate from grammar
        return generateFromGrammar(format, grammar);
    }
    
    private String generateFromGrammar(String format, String grammar) {
        StringBuilder parser = new StringBuilder();
        parser.append("public class ").append(format.toUpperCase()).append("Parser {\n");
        parser.append("    public static Object parse(String input) {\n");
        
        // Simple grammar-based generation
        if (grammar.contains("delimiter")) {
            String delimiter = extractDelimiter(grammar);
            parser.append("        return input.split(\"").append(delimiter).append("\");\n");
        } else if (grammar.contains("regex")) {
            String regex = extractRegex(grammar);
            parser.append("        Pattern p = Pattern.compile(\"").append(regex).append("\");\n");
            parser.append("        Matcher m = p.matcher(input);\n");
            parser.append("        List<String> matches = new ArrayList<>();\n");
            parser.append("        while (m.find()) matches.add(m.group());\n");
            parser.append("        return matches;\n");
        } else {
            // Default line-based parser
            parser.append("        return input.split(\"\\n\");\n");
        }
        
        parser.append("    }\n");
        parser.append("}\n");
        
        String code = parser.toString();
        cachedParsers.put(format.toLowerCase(), code);
        saveCachedParser(format, code);
        return code;
    }
    
    private String extractDelimiter(String grammar) {
        Pattern p = Pattern.compile("delimiter\\s*=\\s*[\"']([^\"']+)[\"']");
        Matcher m = p.matcher(grammar);
        return m.find() ? m.group(1) : ",";
    }
    
    private String extractRegex(String grammar) {
        Pattern p = Pattern.compile("regex\\s*=\\s*[\"']([^\"']+)[\"']");
        Matcher m = p.matcher(grammar);
        return m.find() ? m.group(1) : "\\w+";
    }
    
    public void cacheParser(String format, String code) {
        cachedParsers.put(format.toLowerCase(), code);
        saveCachedParser(format, code);
    }
    
    private void loadCachedParsers() {
        try {
            Path cacheDir = Paths.get("parser-cache");
            if (Files.exists(cacheDir)) {
                Files.walk(cacheDir)
                    .filter(p -> p.toString().endsWith(".java"))
                    .forEach(p -> {
                        try {
                            String format = p.getFileName().toString().replace(".java", "");
                            String code = Files.readString(p);
                            cachedParsers.put(format, code);
                        } catch (Exception e) {}
                    });
            }
        } catch (Exception e) {}
    }
    
    private void saveCachedParser(String format, String code) {
        try {
            Path cacheDir = Paths.get("parser-cache");
            Files.createDirectories(cacheDir);
            Files.writeString(cacheDir.resolve(format + ".java"), code);
        } catch (Exception e) {}
    }
    
    public Set<String> getAvailableParsers() {
        return cachedParsers.keySet();
    }
    
    public boolean hasParser(String format) {
        return cachedParsers.containsKey(format.toLowerCase());
    }
}