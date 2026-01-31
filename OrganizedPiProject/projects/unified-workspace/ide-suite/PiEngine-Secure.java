import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;
import java.util.*;
import java.util.logging.Logger;
import java.util.regex.Pattern;

/**
 * Secure PiEngine with input validation and security hardening
 */
public class PiEngine {
    private static final Logger logger = Logger.getLogger(PiEngine.class.getName());
    private static final Path WORKSPACE = Paths.get("pi_engine_workspace");
    private static final Map<String, LanguageRunner> runners = new HashMap<>();
    private static final Set<String> ALLOWED_LANGUAGES = Set.of("java", "cpp", "python");
    private static final Pattern DANGEROUS_PATTERNS = Pattern.compile(
        "Runtime\\.getRuntime\\(\\)|ProcessBuilder|System\\.exit|exec\\(|eval\\(");
    private static final int MAX_SOURCE_LENGTH = 10000;
    private static final int MAX_OUTPUT_LINES = 100;

    public static void main(String[] args) throws Exception {
        if (!Files.exists(WORKSPACE)) {
            Files.createDirectory(WORKSPACE);
        }

        runners.put("java", new JavaRunner());
        runners.put("cpp", new CppRunner());
        runners.put("python", new PythonRunner());

        logger.info("Testing secure Java execution");
        String javaSource = "public class HelloJava { public static void main(String[] args) { System.out.println(\"Hello from Java!\"); } }";
        ExecutionResult javaResult = compileAndRun("java", javaSource);
        logger.info("Output: " + javaResult.getOutput());
    }

    public static ExecutionResult compileAndRun(String language, String source) {
        try {
            if (source == null || source.trim().isEmpty()) {
                return new ExecutionResult(1, "", "Source code cannot be null or empty");
            }
            
            if (source.length() > MAX_SOURCE_LENGTH) {
                return new ExecutionResult(1, "", "Source code too large");
            }
            
            String langLower = language.toLowerCase(Locale.ROOT);
            if (!ALLOWED_LANGUAGES.contains(langLower)) {
                return new ExecutionResult(1, "", "Language not supported: " + language);
            }
            
            if (DANGEROUS_PATTERNS.matcher(source).find()) {
                logger.warning("Dangerous code detected");
                return new ExecutionResult(1, "", "Potentially dangerous code detected");
            }
            
            LanguageRunner runner = runners.get(langLower);
            return runner.compileAndRun(source);
            
        } catch (Exception e) {
            logger.severe("Error: " + e.getMessage());
            return new ExecutionResult(1, "", "Internal error: " + e.getMessage());
        }
    }

    private static ExecutionResult executeCommand(String... command) throws Exception {
        ProcessBuilder pb = new ProcessBuilder(command).directory(WORKSPACE.toFile());
        Process process = pb.start();

        CompletableFuture<String> outputFuture = CompletableFuture.supplyAsync(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                return readLimitedLines(reader, MAX_OUTPUT_LINES);
            } catch (IOException e) {
                return "Error reading output: " + e.getMessage();
            }
        });

        CompletableFuture<String> errorFuture = CompletableFuture.supplyAsync(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getErrorStream()))) {
                return readLimitedLines(reader, MAX_OUTPUT_LINES);
            } catch (IOException e) {
                return "Error reading error stream: " + e.getMessage();
            }
        });

        boolean finished = process.waitFor(30, TimeUnit.SECONDS);
        if (!finished) {
            process.destroyForcibly();
            throw new TimeoutException("Process timeout");
        }
        
        int exitCode = process.exitValue();
        String output = outputFuture.get(5, TimeUnit.SECONDS);
        String error = errorFuture.get(5, TimeUnit.SECONDS);

        return new ExecutionResult(exitCode, output, error);
    }
    
    private static String readLimitedLines(BufferedReader reader, int maxLines) throws IOException {
        StringBuilder sb = new StringBuilder();
        String line;
        int lineCount = 0;
        while ((line = reader.readLine()) != null && lineCount < maxLines) {
            sb.append(line).append("\n");
            lineCount++;
        }
        if (lineCount >= maxLines) {
            sb.append("[Output truncated]\n");
        }
        return sb.toString();
    }

    interface LanguageRunner {
        ExecutionResult compileAndRun(String source);
    }

    static class JavaRunner implements LanguageRunner {
        @Override
        public ExecutionResult compileAndRun(String source) {
            try {
                String className = extractClassName(source);
                Path sourceFile = WORKSPACE.resolve(className + ".java");
                Files.writeString(sourceFile, source);

                ExecutionResult compileResult = executeCommand("javac", sourceFile.toString());
                if (compileResult.getExitCode() != 0) {
                    return compileResult;
                }

                return executeCommand("java", "-cp", WORKSPACE.toString(), className);
            } catch (Exception e) {
                return new ExecutionResult(1, "", "Java error: " + e.getMessage());
            }
        }
        
        private String extractClassName(String source) {
            Pattern pattern = Pattern.compile("public\\s+class\\s+([\\w_]+)");
            java.util.regex.Matcher matcher = pattern.matcher(source);
            return matcher.find() ? matcher.group(1) : "HelloJava";
        }
    }

    static class CppRunner implements LanguageRunner {
        @Override
        public ExecutionResult compileAndRun(String source) {
            try {
                Path sourceFile = WORKSPACE.resolve("hello.cpp");
                Path outputFile = WORKSPACE.resolve("hello.exe");
                Files.writeString(sourceFile, source);

                ExecutionResult compileResult = executeCommand("g++", sourceFile.toString(), "-o", outputFile.toString());
                if (compileResult.getExitCode() != 0) {
                    return compileResult;
                }

                return executeCommand(outputFile.toString());
            } catch (Exception e) {
                return new ExecutionResult(1, "", "C++ error: " + e.getMessage());
            }
        }
    }

    static class PythonRunner implements LanguageRunner {
        @Override
        public ExecutionResult compileAndRun(String source) {
            try {
                Path sourceFile = WORKSPACE.resolve("hello.py");
                Files.writeString(sourceFile, source);

                return executeCommand("python", sourceFile.toString());
            } catch (Exception e) {
                return new ExecutionResult(1, "", "Python error: " + e.getMessage());
            }
        }
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
    public String getOutput() { return output; }
    public String getError() { return error; }
}