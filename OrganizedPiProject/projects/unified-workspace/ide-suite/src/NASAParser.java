import java.util.*;
import java.util.regex.*;
import java.io.*;
import java.nio.file.*;

public class NASAParser {
    private static final Map<String, String> PARSERS = new HashMap<>();
    
    static {
        PARSERS.put("json", "class J{static Object p(String s){s=s.trim();if(s.startsWith(\"{\"))return o(s);if(s.startsWith(\"[\"))return a(s);if(s.startsWith(\"\\\"\"))return s.substring(1,s.length()-1);if(s.equals(\"true\"))return true;if(s.equals(\"false\"))return false;if(s.equals(\"null\"))return null;return Double.parseDouble(s);}static Map o(String s){Map m=new HashMap();String c=s.substring(1,s.length()-1);for(String x:c.split(\",(?=(?:[^\\\"]*\\\"[^\\\"]*\\\")*[^\\\"]*$)\")){String[]kv=x.split(\":\",2);m.put(kv[0].trim().replaceAll(\"\\\"\",\"\"),p(kv[1].trim()));}return m;}static List a(String s){List l=new ArrayList();String c=s.substring(1,s.length()-1);for(String x:c.split(\",(?=(?:[^\\\"]*\\\"[^\\\"]*\\\")*[^\\\"]*$)\"))l.add(p(x.trim()));return l;}}");
        PARSERS.put("xml", "class X{static N p(String s){s=s.trim();if(!s.startsWith(\"<\"))return new T(s);int e=s.indexOf(\">\");String t=s.substring(1,e);if(s.endsWith(\"</\"+t+\">\")){String c=s.substring(e+1,s.lastIndexOf(\"<\"));return new E(t,c);}return new E(t,\"\");}static class N{String c;N(String x){c=x;}}static class T extends N{T(String x){super(x);}}static class E extends N{String t;E(String x,String y){super(y);t=x;}}}");
        PARSERS.put("csv", "class C{static List p(String s){List r=new ArrayList();for(String l:s.split(\"\\n\"))r.add(pl(l));return r;}static String[]pl(String l){List f=new ArrayList();boolean q=false;StringBuilder b=new StringBuilder();for(char c:l.toCharArray()){if(c=='\"')q=!q;else if(c==','&&!q){f.add(b.toString());b=new StringBuilder();}else b.append(c);}f.add(b.toString());return f.toArray(new String[0]);}}");
        PARSERS.put("yaml", "class Y{static Map p(String s){Map m=new HashMap();for(String l:s.split(\"\\n\")){l=l.trim();if(l.isEmpty()||l.startsWith(\"#\"))continue;if(l.contains(\":\")){String[]kv=l.split(\":\",2);String k=kv[0].trim();String v=kv[1].trim();if(v.startsWith(\"\\\"\"))v=v.substring(1,v.length()-1);m.put(k,pv(v));}}return m;}static Object pv(String v){if(v.equals(\"true\"))return true;if(v.equals(\"false\"))return false;if(v.equals(\"null\"))return null;try{return Integer.parseInt(v);}catch(Exception e){}try{return Double.parseDouble(v);}catch(Exception e){}return v;}}");
        PARSERS.put("log", "class L{static List p(String s){List l=new ArrayList();Pattern p=Pattern.compile(\"(\\\\d{4}-\\\\d{2}-\\\\d{2})\\\\s+(\\\\d{2}:\\\\d{2}:\\\\d{2})\\\\s+(\\\\w+)\\\\s+(.+)\");for(String line:s.split(\"\\n\")){Matcher m=p.matcher(line);if(m.find()){Map e=new HashMap();e.put(\"date\",m.group(1));e.put(\"time\",m.group(2));e.put(\"level\",m.group(3));e.put(\"message\",m.group(4));l.add(e);}else l.add(line);}return l;}}");
        PARSERS.put("ini", "class I{static Map p(String s){Map m=new HashMap();String sec=\"\";for(String l:s.split(\"\\n\")){l=l.trim();if(l.isEmpty()||l.startsWith(\";\"))continue;if(l.startsWith(\"[\")&&l.endsWith(\"]\")){sec=l.substring(1,l.length()-1);m.put(sec,new HashMap());}else if(l.contains(\"=\")){String[]kv=l.split(\"=\",2);if(!sec.isEmpty())((Map)m.get(sec)).put(kv[0].trim(),kv[1].trim());else m.put(kv[0].trim(),kv[1].trim());}}return m;}}");
    }
    
    public static String get(String format) {
        return PARSERS.getOrDefault(format.toLowerCase(), generate(format));
    }
    
    private static String generate(String format) {
        return "class G{static Object p(String s){return s.split(\"\\n\");}}";
    }
    
    public static Object parse(String format, String data) {
        try {
            String code = get(format);
            // Compile and execute parser
            return executeParser(code, data);
        } catch (Exception e) {
            return data.split("\n");
        }
    }
    
    private static Object executeParser(String code, String data) {
        // Simplified execution - in real implementation would use dynamic compilation
        String format = extractFormat(code);
        switch (format) {
            case "json": return parseJSON(data);
            case "xml": return parseXML(data);
            case "csv": return parseCSV(data);
            case "yaml": return parseYAML(data);
            case "log": return parseLog(data);
            case "ini": return parseINI(data);
            default: return data.split("\n");
        }
    }
    
    private static String extractFormat(String code) {
        if (code.contains("class J")) return "json";
        if (code.contains("class X")) return "xml";
        if (code.contains("class C")) return "csv";
        if (code.contains("class Y")) return "yaml";
        if (code.contains("class L")) return "log";
        if (code.contains("class I")) return "ini";
        return "generic";
    }
    
    private static Object parseJSON(String s) {
        s = s.trim();
        if (s.startsWith("{")) return parseJSONObject(s);
        if (s.startsWith("[")) return parseJSONArray(s);
        if (s.startsWith("\"")) return s.substring(1, s.length()-1);
        if (s.equals("true")) return true;
        if (s.equals("false")) return false;
        if (s.equals("null")) return null;
        try { return Double.parseDouble(s); } catch (Exception e) { return s; }
    }
    
    private static Map parseJSONObject(String s) {
        Map m = new HashMap();
        String c = s.substring(1, s.length()-1);
        for (String x : c.split(",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)")) {
            String[] kv = x.split(":", 2);
            if (kv.length == 2) {
                m.put(kv[0].trim().replaceAll("\"", ""), parseJSON(kv[1].trim()));
            }
        }
        return m;
    }
    
    private static List parseJSONArray(String s) {
        List l = new ArrayList();
        String c = s.substring(1, s.length()-1);
        for (String x : c.split(",(?=(?:[^\"]*\"[^\"]*\")*[^\"]*$)")) {
            l.add(parseJSON(x.trim()));
        }
        return l;
    }
    
    private static Object parseXML(String s) {
        s = s.trim();
        if (!s.startsWith("<")) return s;
        int e = s.indexOf(">");
        if (e == -1) return s;
        String t = s.substring(1, e);
        if (s.endsWith("</" + t + ">")) {
            String c = s.substring(e + 1, s.lastIndexOf("<"));
            Map m = new HashMap();
            m.put("tag", t);
            m.put("content", c);
            return m;
        }
        return s;
    }
    
    private static List parseCSV(String s) {
        List r = new ArrayList();
        for (String l : s.split("\n")) {
            r.add(parseCSVLine(l));
        }
        return r;
    }
    
    private static String[] parseCSVLine(String l) {
        List f = new ArrayList();
        boolean q = false;
        StringBuilder b = new StringBuilder();
        for (char c : l.toCharArray()) {
            if (c == '"') q = !q;
            else if (c == ',' && !q) {
                f.add(b.toString());
                b = new StringBuilder();
            } else b.append(c);
        }
        f.add(b.toString());
        return (String[]) f.toArray(new String[0]);
    }
    
    private static Map parseYAML(String s) {
        Map m = new HashMap();
        for (String l : s.split("\n")) {
            l = l.trim();
            if (l.isEmpty() || l.startsWith("#")) continue;
            if (l.contains(":")) {
                String[] kv = l.split(":", 2);
                if (kv.length == 2) {
                    String k = kv[0].trim();
                    String v = kv[1].trim();
                    if (v.startsWith("\"")) v = v.substring(1, v.length()-1);
                    m.put(k, parseYAMLValue(v));
                }
            }
        }
        return m;
    }
    
    private static Object parseYAMLValue(String v) {
        if (v.equals("true")) return true;
        if (v.equals("false")) return false;
        if (v.equals("null")) return null;
        try { return Integer.parseInt(v); } catch (Exception e) {}
        try { return Double.parseDouble(v); } catch (Exception e) {}
        return v;
    }
    
    private static List parseLog(String s) {
        List l = new ArrayList();
        Pattern p = Pattern.compile("(\\d{4}-\\d{2}-\\d{2})\\s+(\\d{2}:\\d{2}:\\d{2})\\s+(\\w+)\\s+(.+)");
        for (String line : s.split("\n")) {
            Matcher m = p.matcher(line);
            if (m.find()) {
                Map e = new HashMap();
                e.put("date", m.group(1));
                e.put("time", m.group(2));
                e.put("level", m.group(3));
                e.put("message", m.group(4));
                l.add(e);
            } else l.add(line);
        }
        return l;
    }
    
    private static Map parseINI(String s) {
        Map m = new HashMap();
        String sec = "";
        for (String l : s.split("\n")) {
            l = l.trim();
            if (l.isEmpty() || l.startsWith(";")) continue;
            if (l.startsWith("[") && l.endsWith("]")) {
                sec = l.substring(1, l.length()-1);
                m.put(sec, new HashMap());
            } else if (l.contains("=")) {
                String[] kv = l.split("=", 2);
                if (kv.length == 2) {
                    if (!sec.isEmpty()) {
                        ((Map)m.get(sec)).put(kv[0].trim(), kv[1].trim());
                    } else {
                        m.put(kv[0].trim(), kv[1].trim());
                    }
                }
            }
        }
        return m;
    }
    
    public static void printStats() {
        System.out.println("NASA Parser Stats:");
        System.out.println("- Supported formats: " + PARSERS.size());
        System.out.println("- Available parsers: " + String.join(", ", PARSERS.keySet()));
    }
    
    public static void main(String[] args) {
        // Test parsers
        System.out.println(parse("json", "{\"name\":\"test\",\"value\":123}"));
        System.out.println(parse("csv", "name,age,city\nJohn,25,NYC\nJane,30,LA"));
        System.out.println(parse("yaml", "name: test\nvalue: 123\nactive: true"));
        printStats();
    }
}