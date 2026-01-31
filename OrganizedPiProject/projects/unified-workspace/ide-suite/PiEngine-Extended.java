import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;
import java.util.regex.*;
import java.util.*;

/**
 * Extended π-Engine with 7 language support and enterprise security
 */
public class PiEngine {
    private static final Path WORKSPACE = createUniqueWorkspace();
    private static final int TIMEOUT_SECONDS = 60;
    private static final int MAX_OUTPUT_BYTES = 16384;

    private record ExecutionResult(int exitCode, String output, String error) {}

    private static Path createUniqueWorkspace() {
        try {
            return Files.createTempDirectory("pi_workspace_");
        } catch (IOException e) {
            throw new UncheckedIOException("Failed to create workspace", e);
        }
    }

    public static void main(String[] args) throws Exception {
        System.out.println("π Extended Pi Engine - 7 Languages");
        System.out.println("==================================");

        testLanguage("java", "public class Test { public static void main(String[] args) { System.out.println(\"π Java works!\"); } }");
        testLanguage("python", "print('π Python works!')");
        testLanguage("cpp", "#include <iostream>\nint main() { std::cout << \"π C++ works!\" << std::endl; return 0; }");
        testLanguage("csharp", "using System; class Test { static void Main() { Console.WriteLine(\"π C# works!\"); } }");
        testLanguage("go", "package main\nimport \"fmt\"\nfunc main() { fmt.Println(\"π Go works!\") }");
        testLanguage("rust", "fn main() { println!(\"π Rust works!\"); }");
        testLanguage("asm", """
section .data
    msg db "π ASM works!", 0xA
    len equ $-msg
section .text
    global _start
_start:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, len
    syscall
    mov rax, 60
    xor rdi, rdi
    syscall
""");
    }

    static void testLanguage(String lang, String code) {
        try {
            System.out.println("\n--- Testing " + lang.toUpperCase() + " ---");
            ExecutionResult result = compileAndRun(lang, code);
            if (result.exitCode() == 0) {
                System.out.println("OK " + result.output().trim());
            } else {
                System.out.println("ERR " + result.error().trim());
            }
        } catch (Exception e) {
            System.out.println("ERR " + lang + " not available: " + e.getMessage());
        }
    }

    public static ExecutionResult compileAndRun(String language, String source) throws Exception {
        if (source == null || source.trim().isEmpty()) {
            return new ExecutionResult(1, "", "Source code cannot be empty");
        }

        if (containsDangerousPatterns(source)) {
            return new ExecutionResult(1, "", "Potentially dangerous code detected");
        }

        return switch (language.toLowerCase()) {
            case "java" -> runJava(source);
            case "cpp", "c++" -> runCpp(source);
            case "python" -> runPython(source);
            case "csharp", "cs" -> runCSharp(source);
            case "go" -> runGo(source);
            case "rust" -> runRust(source);
            case "asm", "assembly" -> runAssembly(source);
            default -> new ExecutionResult(1, "", "Language not supported: " + language);
        };
    }

    static boolean containsDangerousPatterns(String source) {
        String[] dangerous = {"Runtime.getRuntime()", "ProcessBuilder", "System.exit", "File.delete", "rm -rf", "del /f"};
        return Arrays.stream(dangerous).anyMatch(source::contains);
    }

    static ExecutionResult runJava(String source) throws Exception {
        String className = extractClassName(source);
        Path sourceFile = WORKSPACE.resolve(className + ".java");
        Files.writeString(sourceFile, source);

        ExecutionResult compileResult = executeCommand("javac", sourceFile.toString());
        if (compileResult.exitCode() != 0) return compileResult;

        return executeCommand("java", "-cp", WORKSPACE.toString(), className);
    }

    static ExecutionResult runCpp(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.cpp");
        Path outputFile = WORKSPACE.resolve("main");
        Files.writeString(sourceFile, source);

        ExecutionResult compileResult = executeCommand("g++", "-std=c++17", sourceFile.toString(), "-o", outputFile.toString());
        if (compileResult.exitCode() != 0) return compileResult;

        return executeCommand(outputFile.toString());
    }

    static ExecutionResult runPython(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.py");
        Files.writeString(sourceFile, source);
        
        ExecutionResult r = executeCommand("python3", sourceFile.toString());
        return (r.exitCode() == 0 || r.error().isEmpty()) ? r : executeCommand("python", sourceFile.toString());
    }

    static ExecutionResult runCSharp(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("Program.cs");
        Files.writeString(sourceFile, source);

        ExecutionResult compile = executeCommand("dotnet", "build", "--nologo", "-o", WORKSPACE.toString(), sourceFile.toString());
        if (compile.exitCode() == 0) {
            return executeCommand("dotnet", WORKSPACE.resolve("Program.dll").toString());
        }

        compile = executeCommand("mcs", "-out:" + WORKSPACE.resolve("Program.exe"), sourceFile.toString());
        if (compile.exitCode() == 0) {
            return executeCommand("mono", WORKSPACE.resolve("Program.exe").toString());
        }
        return compile;
    }

    static ExecutionResult runGo(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.go");
        Files.writeString(sourceFile, source);
        return executeCommand("go", "run", sourceFile.toString());
    }

    static ExecutionResult runRust(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.rs");
        Path outputFile = WORKSPACE.resolve("main");
        Files.writeString(sourceFile, source);

        ExecutionResult compileResult = executeCommand("rustc", sourceFile.toString(), "-o", outputFile.toString());
        if (compileResult.exitCode() != 0) return compileResult;

        return executeCommand(outputFile.toString());
    }

    static ExecutionResult runAssembly(String source) throws Exception {
        Path sourceFile = WORKSPACE.resolve("main.asm");
        Path objectFile = WORKSPACE.resolve("main.o");
        Path outputFile = WORKSPACE.resolve("main");
        Files.writeString(sourceFile, source);

        ExecutionResult ar = executeCommand("nasm", "-felf64", sourceFile.toString(), "-o", objectFile.toString());
        if (ar.exitCode() != 0) return ar;

        ExecutionResult lr = executeCommand("ld", "-o", outputFile.toString(), objectFile.toString());
        if (lr.exitCode() != 0) return lr;

        outputFile.toFile().setExecutable(true, false);
        return executeCommand(outputFile.toString());
    }

    static String extractClassName(String source) {
        Matcher m = Pattern.compile("public\\s+class\\s+(\\w+)").matcher(source);
        return m.find() ? m.group(1) : "Test";
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

        return new ExecutionResult(process.exitValue(), limitOutput(outputFuture.get()), limitOutput(errorFuture.get()));
    }

    static String readStream(InputStream stream) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream))) {
            StringBuilder sb = new StringBuilder();
            char[] buffer = new char[4096];
            int bytesRead;
            while ((bytesRead = reader.read(buffer)) != -1 && sb.length() < MAX_OUTPUT_BYTES) {
                sb.append(buffer, 0, bytesRead);
            }
            return sb.toString();
        } catch (IOException e) {
            return "Error reading stream: " + e.getMessage();
        }
    }

    static String limitOutput(String output) {
        return output.length() > MAX_OUTPUT_BYTES ? 
               output.substring(0, MAX_OUTPUT_BYTES) + "... (output truncated)" : output;
    }
}