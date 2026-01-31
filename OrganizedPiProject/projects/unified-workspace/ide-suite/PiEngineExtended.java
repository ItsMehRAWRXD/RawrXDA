import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;
import java.util.regex.*;
import java.util.*;

/**
 * Extended π-Engine with 7 language support and enterprise security
 */
public class PiEngineExtended {
    private static final Path WORKSPACE = Paths.get("pi_workspace");
    private static final int TIMEOUT_SECONDS = 60;
    private static final int MAX_OUTPUT_LINES = 200;
    
    public static void main(String[] args) throws Exception {
        Files.createDirectories(WORKSPACE);
        
        System.out.println("π Extended Pi Engine - 7 Languages");
        System.out.println("==================================");
        
        // Test all supported languages
        testLanguage("java", "public class Test { public static void main(String[] args) { System.out.println(\"π Java works!\"); } }");
        testLanguage("python", "print('π Python works!')");
        testLanguage("cpp", "#include <iostream>\nint main() { std::cout << \"π C++ works!\" << std::endl; return 0; }");
        testLanguage("csharp", "using System; class Test { static void Main() { Console.WriteLine(\"π C# works!\"); } }");
        testLanguage("go", "package main\nimport \"fmt\"\nfunc main() { fmt.Println(\"π Go works!\") }");
        testLanguage("rust", "fn main() { println!(\"π Rust works!\"); }");
        testLanguage("asm", "section .data\n    msg db 'π ASM works!', 0xA\nsection .text\n    global _start\n_start:\n    mov eax, 4\n    mov ebx, 1\n    mov ecx, msg\n    mov edx, 13\n    int 0x80\n    mov eax, 1\n    int 0x80");
    }
    
    static void testLanguage(String lang, String code) {
        try {
            System.out.println("\n--- Testing " + lang.toUpperCase() + " ---");
            ExecutionResult result = compileAndRun(lang, code);
            if (result.exitCode == 0) {
                System.out.println("OK " + result.output.trim());
            } else {
                System.out.println("ERR " + result.error.trim());
            }
        } catch (Exception e) {
            System.out.println("ERR " + lang + " not available: " + e.getMessage());
        }
    }
    
    public static ExecutionResult compileAndRun(String language, String source) throws Exception {
        // Security validation
        if (source == null || source.trim().isEmpty()) {
            return new ExecutionResult(1, "", "Source code cannot be empty");
        }
        
        // Detect dangerous patterns
        if (containsDangerousPatterns(source)) {
            return new ExecutionResult(1, "", "Potentially dangerous code detected");
        }
        
        switch (language.toLowerCase()) {
            case "java": return runJava(source);
            case "cpp": case "c++": return runCpp(source);
            case "python": return runPython(source);
            case "csharp": case "cs": return runCSharp(source);
            case "go": return runGo(source);
            case "rust": return runRust(source);
            case "asm": case "assembly": return runAssembly(source);
            default: return new ExecutionResult(1, "", "Language not supported: " + language);
        }
    }
    
    static boolean containsDangerousPatterns(String source) {
        String[] dangerous = {"Runtime.getRuntime()", "ProcessBuilder", "System.exit", "File.delete", "rm -rf", "del /f"};
        for (String pattern : dangerous) {
            if (source.contains(pattern)) return true;
        }
        return false;
    }
    
    static ExecutionResult runJava(String source) throws Exception {
        String className = extractClassName(source);
        Path sourceFile = WORKSPACE.resolve(className + ".java");
        Files.writeString(sourceFile, source);
        
        // Compile
        ExecutionResult compileResult = executeCommand("javac", sourceFile.toString());
        if (compileResult.exitCode != 0) return compileResult;
        
        // Run
        return executeCommand("java", "-cp", WORKSPACE.toString(), className);
    }
    
    static ExecutionResult runCpp(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.cpp");
        Path outputFile = WORKSPACE.resolve("main.exe");
        Files.writeString(sourceFile, source);
        
        // Compile with C++17
        ExecutionResult compileResult = executeCommand("g++", "-std=c++17", sourceFile.toString(), "-o", outputFile.toString());
        if (compileResult.exitCode != 0) return compileResult;
        
        // Run
        return executeCommand(outputFile.toString());
    }
    
    static ExecutionResult runPython(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.py");
        Files.writeString(sourceFile, source);
        return executeCommand("python", sourceFile.toString());
    }
    
    static ExecutionResult runCSharp(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("Program.cs");
        Path outputFile = WORKSPACE.resolve("Program.exe");
        Files.writeString(sourceFile, source);
        
        // Compile
        ExecutionResult compileResult = executeCommand("csc", "/out:" + outputFile.toString(), sourceFile.toString());
        if (compileResult.exitCode != 0) return compileResult;
        
        // Run
        return executeCommand(outputFile.toString());
    }
    
    static ExecutionResult runGo(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.go");
        Files.writeString(sourceFile, source);
        return executeCommand("go", "run", sourceFile.toString());
    }
    
    static ExecutionResult runRust(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.rs");
        Path outputFile = WORKSPACE.resolve("main.exe");
        Files.writeString(sourceFile, source);
        
        // Compile
        ExecutionResult compileResult = executeCommand("rustc", sourceFile.toString(), "-o", outputFile.toString());
        if (compileResult.exitCode != 0) return compileResult;
        
        // Run
        return executeCommand(outputFile.toString());
    }
    
    static ExecutionResult runAssembly(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.asm");
        Path objectFile = WORKSPACE.resolve("main.o");
        Path outputFile = WORKSPACE.resolve("main.exe");
        Files.writeString(sourceFile, source);
        
        // Assemble
        ExecutionResult assembleResult = executeCommand("nasm", "-f", "elf64", sourceFile.toString(), "-o", objectFile.toString());
        if (assembleResult.exitCode != 0) return assembleResult;
        
        // Link
        ExecutionResult linkResult = executeCommand("ld", objectFile.toString(), "-o", outputFile.toString());
        if (linkResult.exitCode != 0) return linkResult;
        
        // Run
        return executeCommand(outputFile.toString());
    }
    
    static String extractClassName(String source) {
        Pattern pattern = Pattern.compile("public\\s+class\\s+([\\w_]+)");
        Matcher matcher = pattern.matcher(source);
        return matcher.find() ? matcher.group(1) : "Main";
    }
    
    static ExecutionResult executeCommand(String... command) throws Exception {
        ProcessBuilder pb = new ProcessBuilder(command).directory(WORKSPACE.toFile());
        Process process = pb.start();
        
        CompletableFuture<String> outputFuture = CompletableFuture.supplyAsync(() -> readStream(process.getInputStream()));
        CompletableFuture<String> errorFuture = CompletableFuture.supplyAsync(() -> readStream(process.getErrorStream()));
        
        boolean finished = process.waitFor(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        if (!finished) {
            process.destroyForcibly();
            return new ExecutionResult(1, "", "Execution timeout after " + TIMEOUT_SECONDS + " seconds");
        }
        
        String output = outputFuture.get();
        String error = errorFuture.get();
        
        return new ExecutionResult(process.exitValue(), limitOutput(output), limitOutput(error));
    }
    
    static String readStream(InputStream stream) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream))) {
            StringBuilder sb = new StringBuilder();
            String line;
            int lineCount = 0;
            while ((line = reader.readLine()) != null && lineCount < MAX_OUTPUT_LINES) {
                sb.append(line).append("\n");
                lineCount++;
            }
            return sb.toString();
        } catch (IOException e) {
            return "Error reading stream: " + e.getMessage();
        }
    }
    
    static String limitOutput(String output) {
        if (output.length() > 10000) {
            return output.substring(0, 10000) + "\n... (output truncated)";
        }
        return output;
    }
    
    static class ExecutionResult {
        final int exitCode;
        final String output;
        final String error;
        
        ExecutionResult(int exitCode, String output, String error) {
            this.exitCode = exitCode;
            this.output = output;
            this.error = error;
        }
    }
}