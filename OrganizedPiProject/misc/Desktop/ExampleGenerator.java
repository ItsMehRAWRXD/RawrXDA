import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.*;
import java.util.*;

/**
 * ExampleGenerator - A program that generates programming examples based on language and difficulty
 * Uses SHA-256 hashing to deterministically select tasks for consistent output
 */
public class ExampleGenerator {
    
    /**
     * Task class representing a programming task with ID, name, and difficulty level
     */
    static class Task {
        int id;
        String name;
        int difficulty;
        
        Task(int id, String name, int difficulty) {
            this.id = id;
            this.name = name;
            this.difficulty = difficulty;
        }
    }
    
    // Predefined list of programming tasks
    static final List<Task> TASKS = Arrays.asList(
        new Task(1, "hello_world", 1),
        new Task(2, "fibonacci", 2),
        new Task(3, "reverse_string", 2),
        new Task(4, "binary_search", 3),
        new Task(5, "merge_sort", 4)
    );
    
    // Code snippets organized by language and task name
    static final Map<String, Map<String, String>> SNIPPETS = Map.of(
        "java", Map.of(
            "hello_world", "public class Hello {\n    public static void main(String[] a) {\n        System.out.println(\"Hello, world!\");\n    }\n}",
            "fibonacci", "public static int fib(int n) {\n    if (n < 2) return n;\n    return fib(n - 1) + fib(n - 2);\n}",
            "reverse_string", "public static String reverse(String s) {\n    return new StringBuilder(s).reverse().toString();\n}",
            "binary_search", "public static int bs(int[] a, int x) {\n    int l = 0, r = a.length - 1;\n    while (l <= r) {\n        int m = l + (r - l) / 2;\n        if (a[m] == x) return m;\n        if (a[m] < x) l = m + 1;\n        else r = m - 1;\n    }\n    return -1;\n}",
            "merge_sort", "public static void ms(int[] a) {\n    if (a.length < 2) return;\n    int m = a.length / 2;\n    int[] L = Arrays.copyOfRange(a, 0, m);\n    int[] R = Arrays.copyOfRange(a, m, a.length);\n    ms(L);\n    ms(R);\n    merge(a, L, R);\n}"
        )
    );
    
    /**
     * Picks n tasks deterministically based on the language using SHA-256 hashing
     * @param lang The programming language
     * @param n Number of tasks to select
     * @return List of selected tasks
     */
    static List<Task> pick(String lang, int n) {
        try {
            MessageDigest d = MessageDigest.getInstance("SHA-256");
            byte[] h = d.digest(lang.getBytes(StandardCharsets.UTF_8));
            
            // Convert first 8 bytes of hash to long for seeding
            long s = 0;
            for (int i = 0; i < 8; i++) {
                s = (s << 8) | (h[i] & 0xFF);
            }
            
            Set<Integer> seen = new LinkedHashSet<>();
            
            // Initial selection based on hash
            int[] b = {
                Math.floorMod(s, TASKS.size()),
                Math.floorMod(s / 7L, TASKS.size()),
                Math.floorMod(s / 13L, TASKS.size())
            };
            
            for (int x : b) {
                seen.add(x);
            }
            
            // Fill remaining slots if needed
            for (int k = 1; seen.size() < n; k++) {
                seen.add(Math.floorMod((int) s + 31 * k, TASKS.size()));
            }
            
            List<Task> r = new ArrayList<>();
            for (int i : seen) {
                r.add(TASKS.get(i));
            }
            
            return r.subList(0, Math.min(n, r.size()));
            
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }
    
    /**
     * Generates markdown format output for the selected tasks
     * @param lang Programming language
     * @param tasks List of tasks to format
     * @return Markdown formatted string
     */
    static String md(String lang, List<Task> tasks) {
        StringBuilder b = new StringBuilder("# " + lang.toUpperCase() + " examples\n\n");
        
        for (Task t : tasks) {
            b.append("## ").append(t.name)
             .append(" (difficulty ").append(t.difficulty).append(")\n")
             .append("```java\n")
             .append(SNIPPETS.get(lang).get(t.name))
             .append("\n```\n\n");
        }
        
        return b.toString();
    }
    
    /**
     * Generates JSON format output for the selected tasks
     * @param lang Programming language
     * @param tasks List of tasks to format
     * @return JSON formatted string
     */
    static String json(String lang, List<Task> tasks) {
        StringBuilder b = new StringBuilder("[\n");
        
        for (int i = 0; i < tasks.size(); i++) {
            Task t = tasks.get(i);
            b.append("  {\n")
             .append("    \"id\": ").append(t.id).append(",\n")
             .append("    \"name\": \"").append(t.name).append("\",\n")
             .append("    \"difficulty\": ").append(t.difficulty).append(",\n")
             .append("    \"snippet\": \"")
             .append(SNIPPETS.get(lang).get(t.name).replace("\"", "\\\""))
             .append("\"\n  }");
            
            if (i < tasks.size() - 1) {
                b.append(",");
            }
            b.append("\n");
        }
        
        return b.append("]").toString();
    }
    
    /**
     * Saves content to a file
     * @param filename The name of the file to create
     * @param content The content to write
     */
    static void saveToFile(String filename, String content) {
        try {
            Path path = Paths.get(filename);
            Files.write(path, content.getBytes(StandardCharsets.UTF_8));
            System.out.println("Saved to: " + filename);
        } catch (Exception e) {
            System.err.println("Error saving file: " + e.getMessage());
        }
    }
    
    /**
     * Main method - generates examples based on command line arguments
     * Usage: java ExampleGenerator [--lang <language>] [--output <format>] [--file]
     * @param a Command line arguments
     */
    public static void main(String[] a) {
        String lang = "java";
        String outputFormat = "both";
        boolean saveToFile = false;
        
        // Parse command line arguments
        for (int i = 0; i < a.length; i++) {
            if (a[i].equals("--lang") && i + 1 < a.length) {
                lang = a[i + 1];
                i++; // Skip next argument
            } else if (a[i].equals("--output") && i + 1 < a.length) {
                outputFormat = a[i + 1];
                i++; // Skip next argument
            } else if (a[i].equals("--file")) {
                saveToFile = true;
            }
        }
        
        List<Task> ex = pick(lang, 3);
        
        if (saveToFile) {
            // Save to files
            if (outputFormat.equals("markdown") || outputFormat.equals("both")) {
                String markdownContent = md(lang, ex);
                saveToFile(lang + "_examples.md", markdownContent);
            }
            if (outputFormat.equals("json") || outputFormat.equals("both")) {
                String jsonContent = json(lang, ex);
                saveToFile(lang + "_examples.json", jsonContent);
            }
        } else {
            // Print to console
            if (outputFormat.equals("markdown")) {
                System.out.println(md(lang, ex));
            } else if (outputFormat.equals("json")) {
                System.out.println(json(lang, ex));
            } else {
                System.out.println("=== MARKDOWN ===\n" + md(lang, ex) + "\n=== JSON ===\n" + json(lang, ex));
            }
        }
    }
}
