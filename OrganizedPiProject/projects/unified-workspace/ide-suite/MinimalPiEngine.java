import java.io.*;
import java.nio.file.*;

public class MinimalPiEngine {
    private static final Path WORKSPACE = Paths.get("pi_workspace");
    
    public static void main(String[] args) throws Exception {
        Files.createDirectories(WORKSPACE);
        
        System.out.println("π Minimal Pi Engine - Zero Dependencies");
        
        // Test Java compilation
        String javaCode = "public class Test { public static void main(String[] args) { System.out.println(\"π Java works!\"); } }";
        runJava(javaCode);
        
        // Test Python (if available)
        String pythonCode = "print('π Python works!')";
        runPython(pythonCode);
    }
    
    static void runJava(String code) throws Exception {
        Path javaFile = WORKSPACE.resolve("Test.java");
        Files.writeString(javaFile, code);
        
        ProcessBuilder javac = new ProcessBuilder("javac", javaFile.toString());
        javac.directory(WORKSPACE.toFile());
        Process compileProcess = javac.start();
        
        if (compileProcess.waitFor() == 0) {
            ProcessBuilder java = new ProcessBuilder("java", "Test");
            java.directory(WORKSPACE.toFile());
            Process runProcess = java.start();
            
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(runProcess.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println(line);
                }
            }
        } else {
            System.out.println("Java compilation failed");
        }
    }
    
    static void runPython(String code) throws Exception {
        Path pythonFile = WORKSPACE.resolve("test.py");
        Files.writeString(pythonFile, code);
        
        try {
            ProcessBuilder python = new ProcessBuilder("python", pythonFile.toString());
            python.directory(WORKSPACE.toFile());
            Process process = python.start();
            
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    System.out.println(line);
                }
            }
        } catch (IOException e) {
            System.out.println("Python not available: " + e.getMessage());
        }
    }
}