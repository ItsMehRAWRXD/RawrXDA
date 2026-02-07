// AiCli.java - An elegant, private, cursor-like CLI with web browsing capabilities
import picocli.CommandLine;
import picocli.CommandLine.*;
import javax.swing.JOptionPane;
import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.Callable;
import java.util.stream.Collectors;

// AI provider classes will be implemented inline

/**
 * A command-line interface for AI-powered coding tasks.
 * It is built to be language-agnostic, relying on the capabilities of
 * Large Language Models (LLMs) to handle diverse programming languages.
 * The output language can be specified via a command-line option.
 *
 * This version of the code also includes a framework for internet browsing
 * to provide the LLM with up-to-date context. It integrates with LLM backends
 * like Gemini and Ollama.
 */
@Command(name = "ai-cli",
        mixinStandardHelpOptions = true,
        version = "ai-cli 1.3",
        description = "Private, Cursor-like CLI with web browsing: refactor, expand, document, commit, plan, design, debug, security, tests, docs index, web-search",
        subcommands = {
                AiCli.Refactor.class, AiCli.Expand.class, AiCli.Document.class,
                AiCli.Commit.class, AiCli.Plan.class, AiCli.Design.class,
                AiCli.Debug.class, AiCli.AnalyzeSecurity.class, AiCli.TestGen.class,
                AiCli.Docs.class, AiCli.Ask.class, AiCli.Prompt.class,
                AiCli.WebSearch.class
        })
public class AiCli implements Runnable {
    public static void main(String[] args) {
        System.exit(new CommandLine(new AiCli()).execute(args));
    }

    @Override
    public void run() {
        CommandLine.usage(this, System.out);
    }

    /* ==========================  BACKEND INFRASTRUCTURE  ========================== */
    /**
     * Abstract base class for all AI subcommands.
     * It handles common options and the unified AI model invocation.
     */
    static abstract class AICommand implements Callable<Integer> {
        @Option(names = "--provider", defaultValue = "chatgpt", description = "AI Provider: chatgpt|amazonq|codegpt|gemini|ollama")
        String provider;
        @Option(names = "--model", description = "Model id. chatgpt: gpt-4o-mini; amazonq: titan-text-express-v1; codegpt: codegpt-pro")
        String model;
        @Option(names = "--api", description = "Override API URL")
        String api;
        @Option(names = "--key", description = "API key. Falls back to OPENAI_API_KEY, AMAZON_Q_KEY, or CODEGPT_API_KEY env vars")
        String key;
        @Option(names = "--timeout", defaultValue = "45")
        int timeoutSec;
        @Option(names = "--context", split = ",", description = "Extra files to inject (comma separated paths)")
        List<File> contextFiles;
        @Option(names = "--stdin", description = "Read primary input from stdin")
        boolean stdin;
        @Option(names = "--file", description = "Read primary input from file")
        File file;
        @Option(names = "--out", description = "Write result to file instead of stdout")
        File out;
        @Option(names = "--temperature", defaultValue = "0.2")
        double temp;
        @Option(names = "--topK", defaultValue = "40")
        int topK;
        @Option(names = "--maxTokens", defaultValue = "2048")
        int maxTokens;
        @Option(names = {"-l", "--lang"}, description = "Target programming language for code generation (e.g., 'java', 'python').")
        String language;
        @Option(names = {"--use-internet"}, description = "Allow the AI to use web search results for the response.")
        boolean useInternet;

        /* ---------- Helper methods ---------- */
        /** Reads the primary input from stdin or a specified file. */
        String readPrimary() throws Exception {
            if (stdin)
                return new String(System.in.readAllBytes(), StandardCharsets.UTF_8);
            if (file != null)
                return Files.readString(file.toPath());
            throw new IllegalArgumentException("Supply --stdin or --file");
        }

        /** Safely reads a file into a string, returning an empty string on failure. */
        String readFile(File f) {
            try {
                return Files.readString(f.toPath());
            } catch (Exception e) {
                return "";
            }
        }

        /** Compiles all context files into a single, formatted string block. */
        String contextBlock() {
            if (contextFiles == null || contextFiles.isEmpty())
                return "";
            StringBuilder sb = new StringBuilder();
            for (File f : contextFiles) {
                sb.append("\n\n=== CONTEXT: ").append(f.getPath()).append(" ===\n").append(readFile(f));
            }
            return sb.toString();
        }

        /** Provides a configured HTTP client instance. */
        HttpClient http() {
            return HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(timeoutSec))
                    .build();
        }

        /** Determines the correct Gemini API URL. */
        String geminiUrl() {
            if (api != null && !api.isBlank())
                return api;
            String m = (model != null && !model.isBlank()) ? model : "gemini-pro";
            return "https://generativelanguage.googleapis.com/v1beta/models/" + m + ":generateContent";
        }

        /** Determines the correct Ollama API URL. */
        String ollamaUrl() {
            return (api != null && !api.isBlank()) ? api : "http://localhost:11434/api/generate";
        }

        /** Retrieves the Gemini API key from options or environment variables. */
        String geminiKey() {
            String k = (key != null && !key.isBlank()) ? key : System.getenv("GEMINI_API_KEY");
            if (k == null || k.isBlank())
                throw new IllegalArgumentException("Set --key or GEMINI_API_KEY");
            return k;
        }

        /* ---------- Unified AI invocation ---------- */
        /**
         * Calls the configured AI provider with a given prompt and returns the response.
         *
         * @param prompt The user prompt to send to the AI provider.
         * @return The AI provider's response as a String.
         * @throws Exception if the API call fails.
         */
        String callAI(String prompt) throws Exception {
            String body;
            String url;
            
            if ("ollama".equalsIgnoreCase(provider)) {
                url = ollamaUrl();
                body = createOllamaRequest(prompt);
            } else { // gemini
                url = geminiUrl() + "?key=" + URLEncoder.encode(geminiKey(), StandardCharsets.UTF_8);
                body = createGeminiRequest(prompt);
            }

            HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(body))
                    .build();
            HttpResponse<String> res = http().send(req, HttpResponse.BodyHandlers.ofString());

            if (res.statusCode() != 200) {
                throw new IOException("HTTP " + res.statusCode() + ": " + res.body());
            }

            return parseAIResponse(res.body());
        }
        
        private String createOllamaRequest(String prompt) {
            return String.format(
                "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false,\"options\":{\"temperature\":%.1f,\"top_k\":%d,\"num_predict\":%d}}",
                getModelForProvider(), prompt.replace("\"", "\\\""), temp, topK, maxTokens
            );
        }
        
        private String createGeminiRequest(String prompt) {
            return String.format(
                "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"%s\"}]}],\"generationConfig\":{\"temperature\":%.1f,\"topK\":%d,\"maxOutputTokens\":%d}}",
                prompt.replace("\"", "\\\""), temp, topK, maxTokens
            );
        }
        
        private String parseAIResponse(String responseBody) {
            try {
                // Simple JSON parsing for response
                if (responseBody.contains("\"response\"")) {
                    // Ollama response
                    int start = responseBody.indexOf("\"response\":\"") + 12;
                    int end = responseBody.indexOf("\"", start);
                    return responseBody.substring(start, end).replace("\\n", "\n");
                } else if (responseBody.contains("\"text\"")) {
                    // Gemini response
                    int start = responseBody.indexOf("\"text\":\"") + 8;
                    int end = responseBody.indexOf("\"", start);
                    return responseBody.substring(start, end).replace("\\n", "\n");
                }
                return "No response content found";
            } catch (Exception e) {
                return "Error parsing response: " + e.getMessage();
            }
        }
        
        /**
         * Get model for the current provider
         */
        String getModelForProvider() {
            if (model != null && !model.isBlank()) {
                return model;
            }
            
            switch (provider.toLowerCase()) {
                case "gemini":
                    return "gemini-pro";
                case "ollama":
                    return "llama3:8b";
                default:
                    return "gpt-3.5-turbo";
            }
        }

        /** Writes the output string to a file or stdout. */
        void writeOut(String txt) throws IOException {
            if (out != null) {
                Files.writeString(out.toPath(), txt);
            } else {
                System.out.print(txt);
            }
        }

        /** Generates the instruction for the LLM, optionally specifying the language and adding web search results. */
        String generateInstruction(String baseInstruction, String prompt) {
            StringBuilder instruction = new StringBuilder(baseInstruction);
            if (language != null && !language.isBlank()) {
                instruction.append(" in ").append(language);
            }

            if (useInternet) {
                try {
                    String searchResults = performWebSearch(prompt);
                    instruction.append("\n\n=== INTERNET SEARCH RESULTS ===\n").append(searchResults);
                } catch (IOException e) {
                    System.err.println("Warning: Failed to perform web search. Proceeding without it.");
                }
            }
            return instruction.toString();
        }

        /**
         * Performs a simple web search and scrapes content.
         * Note: This is a basic implementation. For production use, consider a dedicated search API or a more robust scraping tool.
         */
        String performWebSearch(String query) throws IOException {
            // Using DuckDuckGo's "html" format for simplicity
            String searchUrl = "https://duckduckgo.com/?q=" + URLEncoder.encode(query, StandardCharsets.UTF_8) + "&t=h_&ia=web";
            System.err.println("Searching the web for: " + query);
            
            HttpRequest req = HttpRequest.newBuilder()
                .uri(URI.create(searchUrl))
                .header("User-Agent", "AiCli")
                .GET()
                .build();
            
            try {
                HttpResponse<String> res = http().send(req, HttpResponse.BodyHandlers.ofString());
                if (res.statusCode() != 200) {
                    throw new IOException("HTTP " + res.statusCode() + ": " + res.body());
                }

                String html = res.body();
                // Extract only the text from the search results
                String cleanText = html.replaceAll("<[^>]*>", " ").replaceAll("\\s+", " ").trim();
                
                // Limit to reasonable length
                if (cleanText.length() > 2000) {
                    cleanText = cleanText.substring(0, 2000) + "...";
                }

                return cleanText;
            } catch (Exception e) {
                throw new IOException("Failed to perform web search.", e);
            }
        }

        /* ---------- Core command logic, to be implemented by subcommands ---------- */
        public abstract Integer call() throws Exception;
    }

    /* ==========================  SUB-COMMANDS  ========================== */
    @Command(name = "refactor", description = "Refactor code with optional context and internet search.")
    static class Refactor extends AICommand {
        @Parameters(paramLabel = "PROMPT", description = "What to refactor")
        String prompt;
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Refactor the following code according to these instructions: " + prompt, prompt);
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "expand", description = "Expand stub / incomplete code with optional internet search.")
    static class Expand extends AICommand {
        @Parameters(paramLabel = "PROMPT", description = "Expansion instructions")
        String prompt;
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Expand the following incomplete code and stub according to these instructions: " + prompt, prompt);
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "document", description = "Document code and generate documentation with optional internet search.")
    static class Document extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "0..1", description = "Extra instructions")
        String prompt = "";
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Generate documentation for the following code", prompt);
            instruction += prompt.isBlank() ? "" : " according to these instructions:\n" + prompt;
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "commit", description = "Generate a git commit message from staged changes with optional internet search.")
    static class Commit extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "0..1", description = "Extra instructions")
        String prompt = "";
        public Integer call() throws Exception {
            String changes = executeCommand("git", "diff", "--staged");
            if (changes.isBlank()) {
                throw new IllegalStateException("No staged changes found. Please stage changes with `git add` first.");
            }
            String instruction = generateInstruction("Generate a concise git commit message for the following staged changes", prompt);
            instruction += prompt.isBlank() ? "" : " according to these instructions:\n" + prompt;
            String full = instruction + "\n\nCHANGES:\n" + changes;
            writeOut(callAI(full));
            return 0;
        }

        private String executeCommand(String... command) throws IOException, InterruptedException {
            Process process = new ProcessBuilder(command)
                    .redirectErrorStream(true)
                    .start();
            try (InputStream is = process.getInputStream()) {
                String result = new String(is.readAllBytes(), StandardCharsets.UTF_8);
                process.waitFor();
                return result;
            }
        }
    }

    @Command(name = "plan", description = "Create a plan based on a goal and context with optional internet search.")
    static class Plan extends AICommand {
        @Parameters(paramLabel = "PROMPT")
        String prompt;
        public Integer call() throws Exception {
            String ctx = contextBlock();
            String instruction = generateInstruction("Create a detailed plan to achieve the following goal: " + prompt, prompt);
            String full = instruction + ctx;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "design", description = "Design a system or architecture with optional internet search.")
    static class Design extends AICommand {
        @Parameters(paramLabel = "PROMPT")
        String prompt;
        public Integer call() throws Exception {
            String ctx = contextBlock();
            String instruction = generateInstruction("Design a system or architecture based on the following requirements: " + prompt, prompt);
            String full = instruction + ctx;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "debug", description = "Debug code and fix errors with optional internet search.")
    static class Debug extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "0..1")
        String prompt = "";
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Debug the following code and provide a fix", prompt);
            instruction += prompt.isBlank() ? "" : " according to these instructions:\n" + prompt;
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "analyze-security", description = "Analyze code for security vulnerabilities with optional internet search.")
    static class AnalyzeSecurity extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "0..1")
        String prompt = "";
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Analyze the following code for security vulnerabilities and suggest fixes", prompt);
            instruction += prompt.isBlank() ? "" : " according to these instructions:\n" + prompt;
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "test-gen", description = "Generate tests for a given code snippet with optional internet search.")
    static class TestGen extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "0..1")
        String prompt = "";
        public Integer call() throws Exception {
            String code = readPrimary();
            String ctx  = contextBlock();
            String instruction = generateInstruction("Generate unit tests for the following code", prompt);
            instruction += prompt.isBlank() ? "" : " according to these instructions:\n" + prompt;
            String full = instruction + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "docs", description = "Index and query documentation files with optional internet search.")
    static class Docs extends AICommand {
        @Parameters(paramLabel = "PROMPT", arity = "1")
        String prompt;
        public Integer call() throws Exception {
            String ctx = contextBlock();
            String instruction = generateInstruction("Use the provided documentation to answer the following question:\n" + prompt, prompt);
            String full = instruction + ctx;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "ask", description = "Ask a question with optional context and internet search.")
    static class Ask extends AICommand {
        @Parameters(paramLabel = "PROMPT")
        String prompt;
        public Integer call() throws Exception {
            String code = "";
            try {
                code = readPrimary();
            } catch (IllegalArgumentException e) {
                // Ignore, as --file or --stdin is optional for this command.
            }
            String ctx = contextBlock();
            String instruction = generateInstruction("Answer the following question:", prompt);
            String full = instruction + "\n" + prompt + ctx + "\n\nCODE:\n" + code;
            writeOut(callAI(full));
            return 0;
        }
    }

    @Command(name = "web-search", description = "Perform a web search for the given prompt.")
    static class WebSearch extends AICommand {
        @Parameters(paramLabel = "PROMPT", description = "The search query")
        String prompt;
        public Integer call() throws Exception {
            writeOut(performWebSearch(prompt));
            return 0;
        }
    }

    @Command(name = "prompt", description = "Use a GUI message box to input the prompt for a command.")
    static class Prompt extends AICommand {
        @Parameters(paramLabel = "COMMAND", description = "The command to run (e.g., refactor, ask)")
        String commandName;
        public Integer call() throws Exception {
            // Check for Swing availability
            try {
                javax.swing.UIManager.setLookAndFeel(javax.swing.UIManager.getSystemLookAndFeelClassName());
            } catch (Exception e) {
                System.err.println("Warning: Swing look and feel could not be set. Displaying default dialog.");
            }

            String promptText = JOptionPane.showInputDialog(null, "Enter your prompt for the '" + commandName + "' command:", "AI CLI Prompt", JOptionPane.PLAIN_MESSAGE);
            if (promptText == null || promptText.isBlank()) {
                System.err.println("Prompt was cancelled or empty. Exiting.");
                return 1;
            }

            // Execute the command directly with the prompt
            System.out.println("Executing " + commandName + " with prompt: " + promptText);
            
            // For simplicity, just return the prompt text
            // In a full implementation, this would delegate to the appropriate subcommand
            writeOut("Prompt for " + commandName + ": " + promptText);
            return 0;
        }
    }
}