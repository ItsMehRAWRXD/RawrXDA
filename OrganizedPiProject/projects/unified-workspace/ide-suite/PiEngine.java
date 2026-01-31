import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;

/**
 * A proof-of-concept engine that demonstrates minimal, programmatic compilation 
 * for multiple languages, inspired by the architecture of online compilers.
 */
public class PiEngine {

    private static final Path WORKSPACE = Paths.get("pi_engine_workspace");

    public static void main(String[] args) throws Exception {
        if (!Files.exists(WORKSPACE)) {
            Files.createDirectory(WORKSPACE);
        }

        System.out.println("--- Testing Java Execution ---");
        String javaSource = "public class HelloJava { public static void main(String[] args) { System.out.println(\"Hello from Java!\"); } }";
        ExecutionResult javaResult = compileAndRun("java", javaSource);
        System.out.println("Output:\n" + javaResult.getOutput());
        System.out.println("--------------------------\n");

        System.out.println("--- Testing C++ Execution ---");
        String cppSource = "#include <iostream>\nint main() { std::cout << \"Hello from C++!\" << std::endl; return 0; }";
        ExecutionResult cppResult = compileAndRun("cpp", cppSource);
        System.out.println("Output:\n" + cppResult.getOutput());
        System.out.println("-------------------------\n");
    }

    public static ExecutionResult compileAndRun(String language, String source) throws IOException, InterruptedException, ExecutionException {
        switch (language.toLowerCase()) {
            case "java":
                return runJava(source);
            case "cpp":
                return runCpp(source);
            default:
                return new ExecutionResult(1, "", "Language not supported: " + language);
        }
    }

    private static ExecutionResult runJava(String source) throws IOException, InterruptedException, ExecutionException {
        Path sourceFile = WORKSPACE.resolve("HelloJava.java");
        Files.writeString(sourceFile, source);

        // 1. Compile
        ExecutionResult compileResult = executeCommand("javac", sourceFile.toString());
        if (compileResult.getExitCode() != 0) {
            return compileResult;
        }

        // 2. Run
        return executeCommand("java", "-cp", WORKSPACE.toString(), "HelloJava");
    }

    private static ExecutionResult runCpp(String source) throws IOException, InterruptedException, ExecutionException {
        Path sourceFile = WORKSPACE.resolve("hello.cpp");
        Path outputFile = WORKSPACE.resolve("hello.exe");
        Files.writeString(sourceFile, source);

        // 1. Compile
        ExecutionResult compileResult = executeCommand("g++", sourceFile.toString(), "-o", outputFile.toString());
        if (compileResult.getExitCode() != 0) {
            return compileResult;
        }

        // 2. Run
        return executeCommand(outputFile.toString());
    }

    private static ExecutionResult executeCommand(String... command) throws IOException, InterruptedException, ExecutionException {
        ProcessBuilder pb = new ProcessBuilder(command).directory(WORKSPACE.toFile());
        Process process = pb.start();

        CompletableFuture<String> outputFuture = CompletableFuture.supplyAsync(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    sb.append(line).append("\n");
                }
                return sb.toString();
            } catch (IOException e) {
                return "Error reading output: " + e.getMessage();
            }
        });

        CompletableFuture<String> errorFuture = CompletableFuture.supplyAsync(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getErrorStream()))) {
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    sb.append(line).append("\n");
                }
                return sb.toString();
            } catch (IOException e) {
                return "Error reading error stream: " + e.getMessage();
            }
        });

        int exitCode = process.waitFor();
        String output = outputFuture.get();
        String error = errorFuture.get();

        return new ExecutionResult(exitCode, output, error);
    }
}

class ExecutionResult {
    private final int exitCode;
    private final String output;
    private final String error;

    public ExecutionResult(int exitCode, String output, String error) {
        this.exitCode = exitCode;
        this.output = output;
        this.error = error;
    }

    public int getExitCode() { return exitCode; }
    public String getOutput() { return exitCode == 0 ? output : error; }
}
