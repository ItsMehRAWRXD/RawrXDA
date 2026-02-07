import java.io.*;
import java.nio.file.*;

public class SimplePiEngine {
    private static final Path WORKSPACE = Paths.get("pi_workspace");
    
    public static void main(String[] args) throws Exception {
        Files.createDirectories(WORKSPACE);
        
        System.out.println("Pi Working Pi Engine - Multi-Language Compiler");
        System.out.println("============================================");
        
        testJava();
        testPython();
        testNode();
    }
    
    static void testJava() {
        try {
            System.out.println("\n--- Testing Java ---");
            String code = "public class Test { public static void main(String[] args) { System.out.println(\"Pi Java: Hello World!\"); } }";
            
            Path javaFile = WORKSPACE.resolve("Test.java");
            Files.writeString(javaFile, code);
            
            Process compile = new ProcessBuilder("javac", javaFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            if (compile.waitFor() == 0) {
                Process run = new ProcessBuilder("java", "Test")
                    .directory(WORKSPACE.toFile())
                    .start();
                
                printOutput(run);
            } else {
                System.out.println("X Java compilation failed");
            }
        } catch (Exception e) {
            System.out.println("X Java error: " + e.getMessage());
        }
    }
    
    static void testPython() {
        try {
            System.out.println("\n--- Testing Python ---");
            String code = "print('Pi Python: Hello World!')";
            
            Path pythonFile = WORKSPACE.resolve("test.py");
            Files.writeString(pythonFile, code);
            
            Process run = new ProcessBuilder("python", pythonFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            printOutput(run);
        } catch (Exception e) {
            System.out.println("X Python not available");
        }
    }
    
    static void testNode() {
        try {
            System.out.println("\n--- Testing Node.js ---");
            String code = "console.log('Pi Node.js: Hello World!');";
            
            Path nodeFile = WORKSPACE.resolve("test.js");
            Files.writeString(nodeFile, code);
            
            Process run = new ProcessBuilder("node", nodeFile.toString())
                .directory(WORKSPACE.toFile())
                .start();
            
            printOutput(run);
        } catch (Exception e) {
            System.out.println("X Node.js not available");
        }
    }
    
    static void printOutput(Process process) throws Exception {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println("OK " + line);
            }
        }
        
        if (process.waitFor() != 0) {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getErrorStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println("ERR " + line);
                }
            }
        }
    }
}