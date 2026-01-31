import java.io.*;
import java.nio.file.*;

public class WorkingPiEngine {
    private static final Path WORKSPACE = Paths.get("pi_workspace");
    
    public static void main(String[] args) throws Exception {
        Files.createDirectories(WORKSPACE);
        
        System.out.println("π Working Pi Engine - Multi-Language Compiler");
        System.out.println("============================================");
        
        // Test Java (always available)
        testJava();
        
        // Test other languages if available
        testPython();
        testNode();
    }
    
    static void testJava() {
        try {
            System.out.println("\n--- Testing Java ---");
            String code = "public class Test { public static void main(String[] args) { System.out.println(\"π Java: Hello World!\"); } }";
            
            Path javaFile = WORKSPACE.resolve("Test.java");
            Files.writeString(javaFile, code);
            
            // Compile
            Process compile = new ProcessBuilder("javac", javaFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            if (compile.waitFor() == 0) {
                // Run
                Process run = new ProcessBuilder("java", "Test")
                    .directory(WORKSPACE.toFile())
                    .start();
                
                printOutput(run);
            } else {
                System.out.println("❌ Java compilation failed");
            }
        } catch (Exception e) {
            System.out.println("❌ Java error: " + e.getMessage());
        }
    }
    
    static void testPython() {
        try {
            System.out.println("\n--- Testing Python ---");
            String code = "print('π Python: Hello World!')";
            
            Path pythonFile = WORKSPACE.resolve("test.py");
            Files.writeString(pythonFile, code);
            
            Process run = new ProcessBuilder("python", pythonFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            printOutput(run);
        } catch (Exception e) {
            System.out.println("❌ Python not available: " + e.getMessage());
        }
    }
    
    static void testNode() {
        try {
            System.out.println("\n--- Testing Node.js ---");
            String code = "console.log('π Node.js: Hello World!');";
            
            Path nodeFile = WORKSPACE.resolve("test.js");
            Files.writeString(nodeFile, code);
            
            Process run = new ProcessBuilder("node", nodeFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            printOutput(run);
        } catch (Exception e) {
            System.out.println("❌ Node.js not available: " + e.getMessage());
        }
    }
    
    static void printOutput(Process process) throws Exception {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println("✅ " + line);
            }
        }
        
        if (process.waitFor() != 0) {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getErrorStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println("❌ " + line);
                }
            }
        }
    }
}