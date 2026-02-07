// AiCliEnhanced.java - AI Middleman CLI that routes HTTP requests to AI APIs
import picocli.CommandLine;
import picocli.CommandLine.*;
import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.Callable;
import java.util.stream.Collectors;

/**
 * AI Middleman CLI - Routes user requests to AI APIs via HTTP.
 * Acts as a proxy between user commands and AI services.
 */
@Command(name = "ai-middleman",
        mixinStandardHelpOptions = true,
        version = "ai-middleman 2.0",
        description = "AI Middleman CLI - Routes requests to OpenAI, Gemini, Claude, Ollama, and more",
        subcommands = {
            AiCliEnhanced.ChatCommand.class,
            AiCliEnhanced.RefactorCommand.class,
            AiCliEnhanced.ExplainCommand.class,
            AiCliEnhanced.DebugCommand.class,
            AiCliEnhanced.GenerateCommand.class,
            AiCliEnhanced.TestCommand.class
        })
public class AiCliEnhanced implements Runnable {
    public static void main(String[] args) {
        System.exit(new CommandLine(new AiCliEnhanced()).execute(args));
    }

    @Override
    public void run() {
        CommandLine.usage(this, System.out);
    }

    /* ==========================  AI MIDDLEMAN COMMANDS  ========================== */
    
    @Command(name = "chat", description = "Chat with AI")
    static class ChatCommand implements Callable<Integer> {
        @Parameters(paramLabel = "MESSAGE", description = "Message to send to AI")
        String message;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai", 
                description = "AI Provider: openai, gemini, claude, ollama, cohere")
        String provider;
        
        @Option(names = {"--key", "-k"}, description = "API key (or set env var)")
        String apiKey;
        
        @Option(names = {"--model", "-m"}, description = "Model to use")
        String model;
        
        @Option(names = {"--temperature", "-t"}, defaultValue = "0.7", description = "Temperature (0.0-1.0)")
        double temperature;
        
        @Option(names = {"--max-tokens"}, defaultValue = "1000", description = "Max tokens in response")
        int maxTokens;
        
        @Override
        public Integer call() throws Exception {
            AIMiddleman middleman = new AIMiddleman(provider, apiKey, model, temperature, maxTokens);
            String response = middleman.sendRequest(message);
            System.out.println(response);
            return 0;
        }
    }
    
    @Command(name = "refactor", description = "Refactor code using AI")
    static class RefactorCommand implements Callable<Integer> {
        @Parameters(paramLabel = "INSTRUCTION", description = "Refactoring instruction")
        String instruction;
        
        @Option(names = {"--file", "-f"}, description = "File to refactor")
        File file;
        
        @Option(names = {"--stdin"}, description = "Read code from stdin")
        boolean stdin;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai")
        String provider;
        
        @Option(names = {"--key", "-k"})
        String apiKey;
        
        @Option(names = {"--out", "-o"}, description = "Output file")
        File output;
        
        @Override
        public Integer call() throws Exception {
            String code = readInput();
            String prompt = "Refactor the following code: " + instruction + "\n\nCode:\n" + code;
            
            AIMiddleman middleman = new AIMiddleman(provider, apiKey);
            String refactoredCode = middleman.sendRequest(prompt);
            
            if (output != null) {
                Files.writeString(output.toPath(), refactoredCode);
                System.err.println("SUCCESS Refactored code written to " + output.getName());
            } else {
                System.out.println(refactoredCode);
            }
            return 0;
        }
        
        private String readInput() throws Exception {
            if (stdin) {
                return new String(System.in.readAllBytes(), StandardCharsets.UTF_8);
            } else if (file != null) {
                return Files.readString(file.toPath());
            } else {
                throw new IllegalArgumentException("Specify --file or --stdin");
            }
        }
    }
    
    @Command(name = "explain", description = "Explain code using AI")
    static class ExplainCommand implements Callable<Integer> {
        @Option(names = {"--file", "-f"}, description = "File to explain")
        File file;
        
        @Option(names = {"--stdin"}, description = "Read code from stdin")
        boolean stdin;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai")
        String provider;
        
        @Option(names = {"--key", "-k"})
        String apiKey;
        
        @Override
        public Integer call() throws Exception {
            String code = readInput();
            String prompt = "Explain the following code in detail:\n\n" + code;
            
            AIMiddleman middleman = new AIMiddleman(provider, apiKey);
            String explanation = middleman.sendRequest(prompt);
            
            System.out.println(explanation);
            return 0;
        }
        
        private String readInput() throws Exception {
            if (stdin) {
                return new String(System.in.readAllBytes(), StandardCharsets.UTF_8);
            } else if (file != null) {
                return Files.readString(file.toPath());
            } else {
                throw new IllegalArgumentException("Specify --file or --stdin");
            }
        }
    }
    
    @Command(name = "debug", description = "Debug code using AI")
    static class DebugCommand implements Callable<Integer> {
        @Option(names = {"--file", "-f"}, description = "File to debug")
        File file;
        
        @Option(names = {"--error"}, description = "Error message to analyze")
        String errorMessage;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai")
        String provider;
        
        @Option(names = {"--key", "-k"})
        String apiKey;
        
        @Override
        public Integer call() throws Exception {
            String code = file != null ? Files.readString(file.toPath()) : "";
            String prompt = "Debug the following code";
            if (errorMessage != null) {
                prompt += " with this error: " + errorMessage;
            }
            prompt += "\n\nCode:\n" + code;
            
            AIMiddleman middleman = new AIMiddleman(provider, apiKey);
            String debugInfo = middleman.sendRequest(prompt);
            
            System.out.println(debugInfo);
            return 0;
        }
    }
    
    @Command(name = "generate", description = "Generate code using AI")
    static class GenerateCommand implements Callable<Integer> {
        @Parameters(paramLabel = "DESCRIPTION", description = "Code generation description")
        String description;
        
        @Option(names = {"--lang", "-l"}, defaultValue = "java", description = "Programming language")
        String language;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai")
        String provider;
        
        @Option(names = {"--key", "-k"})
        String apiKey;
        
        @Option(names = {"--out", "-o"}, description = "Output file")
        File output;
        
        @Override
        public Integer call() throws Exception {
            String prompt = "Generate " + language + " code for: " + description;
            
            AIMiddleman middleman = new AIMiddleman(provider, apiKey);
            String generatedCode = middleman.sendRequest(prompt);
            
            if (output != null) {
                Files.writeString(output.toPath(), generatedCode);
                System.err.println("SUCCESS Generated code written to " + output.getName());
            } else {
                System.out.println(generatedCode);
            }
            return 0;
        }
    }
    
    @Command(name = "test", description = "Generate tests using AI")
    static class TestCommand implements Callable<Integer> {
        @Option(names = {"--file", "-f"}, description = "File to generate tests for")
        File file;
        
        @Option(names = {"--framework"}, defaultValue = "junit", description = "Test framework")
        String framework;
        
        @Option(names = {"--provider", "-p"}, defaultValue = "openai")
        String provider;
        
        @Option(names = {"--key", "-k"})
        String apiKey;
        
        @Option(names = {"--out", "-o"}, description = "Output file")
        File output;
        
        @Override
        public Integer call() throws Exception {
            String code = Files.readString(file.toPath());
            String prompt = "Generate " + framework + " tests for the following code:\n\n" + code;
            
            AIMiddleman middleman = new AIMiddleman(provider, apiKey);
            String tests = middleman.sendRequest(prompt);
            
            if (output != null) {
                Files.writeString(output.toPath(), tests);
                System.err.println("SUCCESS Generated tests written to " + output.getName());
            } else {
                System.out.println(tests);
            }
            return 0;
        }
    }

    /* ==========================  AI MIDDLEMAN CORE  ========================== */
    
    /**
     * AI Middleman - Routes HTTP requests to different AI providers
     */
    static class AIMiddleman {
        private final String provider;
        private final String apiKey;
        private final String model;
        private final double temperature;
        private final int maxTokens;
        private final HttpClient httpClient;
        
        public AIMiddleman(String provider, String apiKey) {
            this(provider, apiKey, null, 0.7, 1000);
        }
        
        public AIMiddleman(String provider, String apiKey, String model, double temperature, int maxTokens) {
            this.provider = provider.toLowerCase();
            this.apiKey = apiKey;
            this.model = model;
            this.temperature = temperature;
            this.maxTokens = maxTokens;
            this.httpClient = HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(30))
                    .build();
        }
        
        /**
         * Send request to AI provider via HTTP
         */
        public String sendRequest(String prompt) throws Exception {
            System.err.println("AI Middleman: Routing to " + provider + " API...");
            
            String response = "";
            Exception lastError = null;
            
            try {
                switch (provider) {
                    case "openai":
                    case "chatgpt":
                        response = callOpenAI(prompt);
                        break;
                    case "gemini":
                        response = callGemini(prompt);
                        break;
                    case "claude":
                        response = callClaude(prompt);
                        break;
                    case "ollama":
                        response = callOllama(prompt);
                        break;
                    case "cohere":
                        response = callCohere(prompt);
                        break;
                    default:
                        throw new IllegalArgumentException("Unknown provider: " + provider);
                }
                
                System.err.println("SUCCESS AI Middleman: Success from " + provider);
                return response;
                
            } catch (Exception e) {
                lastError = e;
                System.err.println("X AI Middleman: " + provider + " failed: " + e.getMessage());
                
                // Try fallback to Gemini
                if (!"gemini".equals(provider)) {
                    System.err.println("RETRY AI Middleman: Trying fallback to Gemini...");
                    try {
                        response = callGemini(prompt);
                        System.err.println("SUCCESS AI Middleman: Fallback successful");
                        return response;
                    } catch (Exception fallbackError) {
                        System.err.println("X AI Middleman: Fallback also failed");
                    }
                }
                
                throw new IOException("All AI providers failed. Last error: " + lastError.getMessage(), lastError);
            }
        }
        
        /**
         * Call OpenAI API
         */
        private String callOpenAI(String prompt) throws Exception {
            String key = getApiKey("OPENAI_API_KEY");
            String modelName = model != null ? model : "gpt-3.5-turbo";
            
            String requestBody = String.format("""
                {
                    "model": "%s",
                    "messages": [{"role": "user", "content": "%s"}],
                    "temperature": %.1f,
                    "max_tokens": %d
                }
                """, modelName, escapeJson(prompt), temperature, maxTokens);
            
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.openai.com/v1/chat/completions"))
                    .header("Authorization", "Bearer " + key)
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                    .build();
            
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new IOException("OpenAI API error " + response.statusCode() + ": " + response.body());
            }
            
            return parseOpenAIResponse(response.body());
        }
        
        /**
         * Call Gemini API
         */
        private String callGemini(String prompt) throws Exception {
            String key = getApiKey("GEMINI_API_KEY");
            String modelName = model != null ? model : "gemini-pro";
            
            String url = "https://generativelanguage.googleapis.com/v1beta/models/" + 
                        modelName + ":generateContent?key=" + URLEncoder.encode(key, StandardCharsets.UTF_8);
            
            String requestBody = String.format("""
                {
                    "contents": [{"parts": [{"text": "%s"}]}],
                    "generationConfig": {
                        "temperature": %.1f,
                        "maxOutputTokens": %d
                    }
                }
                """, escapeJson(prompt), temperature, maxTokens);
            
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                    .build();
            
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new IOException("Gemini API error " + response.statusCode() + ": " + response.body());
            }
            
            return parseGeminiResponse(response.body());
        }
        
        /**
         * Call Claude API
         */
        private String callClaude(String prompt) throws Exception {
            String key = getApiKey("CLAUDE_API_KEY");
            String modelName = model != null ? model : "claude-3-sonnet-20240229";
            
            String requestBody = String.format("""
                {
                    "model": "%s",
                    "max_tokens": %d,
                    "temperature": %.1f,
                    "messages": [{"role": "user", "content": "%s"}]
                }
                """, modelName, maxTokens, temperature, escapeJson(prompt));
            
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.anthropic.com/v1/messages"))
                    .header("x-api-key", key)
                    .header("Content-Type", "application/json")
                    .header("anthropic-version", "2023-06-01")
                    .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                    .build();
            
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new IOException("Claude API error " + response.statusCode() + ": " + response.body());
            }
            
            return parseClaudeResponse(response.body());
        }
        
        /**
         * Call Ollama API
         */
        private String callOllama(String prompt) throws Exception {
            String modelName = model != null ? model : "llama2";
            String url = "http://localhost:11434/api/generate";
            
            String requestBody = String.format("""
                {
                    "model": "%s",
                    "prompt": "%s",
                    "stream": false,
                    "options": {
                        "temperature": %.1f,
                        "num_predict": %d
                    }
                }
                """, modelName, escapeJson(prompt), temperature, maxTokens);
            
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                    .build();
            
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new IOException("Ollama API error " + response.statusCode() + ": " + response.body());
            }
            
            return parseOllamaResponse(response.body());
        }
        
        /**
         * Call Cohere API
         */
        private String callCohere(String prompt) throws Exception {
            String key = getApiKey("COHERE_API_KEY");
            String modelName = model != null ? model : "command";
            
            String requestBody = String.format("""
                {
                    "model": "%s",
                    "prompt": "%s",
                    "max_tokens": %d,
                    "temperature": %.1f
                }
                """, modelName, escapeJson(prompt), maxTokens, temperature);
            
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.cohere.ai/v1/generate"))
                    .header("Authorization", "Bearer " + key)
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(requestBody))
                    .build();
            
            HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
            
            if (response.statusCode() != 200) {
                throw new IOException("Cohere API error " + response.statusCode() + ": " + response.body());
            }
            
            return parseCohereResponse(response.body());
        }
        
        /**
         * Get API key from parameter or environment
         */
        private String getApiKey(String envVar) {
            if (apiKey != null && !apiKey.isEmpty()) {
                return apiKey;
            }
            String envKey = System.getenv(envVar);
            if (envKey != null && !envKey.isEmpty()) {
                return envKey;
            }
            throw new IllegalStateException("API key not found. Set " + envVar + " environment variable or use --key option");
        }
        
        /**
         * Parse OpenAI response
         */
        private String parseOpenAIResponse(String responseBody) {
            try {
                // Simple JSON parsing - look for "content" field
                int contentStart = responseBody.indexOf("\"content\":\"") + 11;
                int contentEnd = responseBody.indexOf("\"", contentStart);
                if (contentStart > 10 && contentEnd > contentStart) {
                    return responseBody.substring(contentStart, contentEnd)
                            .replace("\\n", "\n")
                            .replace("\\\"", "\"")
                            .replace("\\\\", "\\");
                }
                return "Error parsing OpenAI response";
            } catch (Exception e) {
                return "Error parsing OpenAI response: " + e.getMessage();
            }
        }
        
        /**
         * Parse Gemini response
         */
        private String parseGeminiResponse(String responseBody) {
            try {
                // Simple JSON parsing - look for "text" field
                int textStart = responseBody.indexOf("\"text\":\"") + 8;
                int textEnd = responseBody.indexOf("\"", textStart);
                if (textStart > 7 && textEnd > textStart) {
                    return responseBody.substring(textStart, textEnd)
                            .replace("\\n", "\n")
                            .replace("\\\"", "\"")
                            .replace("\\\\", "\\");
                }
                return "Error parsing Gemini response";
            } catch (Exception e) {
                return "Error parsing Gemini response: " + e.getMessage();
            }
        }
        
        /**
         * Parse Claude response
         */
        private String parseClaudeResponse(String responseBody) {
            try {
                // Simple JSON parsing - look for "text" field
                int textStart = responseBody.indexOf("\"text\":\"") + 8;
                int textEnd = responseBody.indexOf("\"", textStart);
                if (textStart > 7 && textEnd > textStart) {
                    return responseBody.substring(textStart, textEnd)
                            .replace("\\n", "\n")
                            .replace("\\\"", "\"")
                            .replace("\\\\", "\\");
                }
                return "Error parsing Claude response";
            } catch (Exception e) {
                return "Error parsing Claude response: " + e.getMessage();
            }
        }
        
        /**
         * Parse Ollama response
         */
        private String parseOllamaResponse(String responseBody) {
            try {
                // Simple JSON parsing - look for "response" field
                int responseStart = responseBody.indexOf("\"response\":\"") + 12;
                int responseEnd = responseBody.indexOf("\"", responseStart);
                if (responseStart > 11 && responseEnd > responseStart) {
                    return responseBody.substring(responseStart, responseEnd)
                            .replace("\\n", "\n")
                            .replace("\\\"", "\"")
                            .replace("\\\\", "\\");
                }
                return "Error parsing Ollama response";
            } catch (Exception e) {
                return "Error parsing Ollama response: " + e.getMessage();
            }
        }
        
        /**
         * Parse Cohere response
         */
        private String parseCohereResponse(String responseBody) {
            try {
                // Simple JSON parsing - look for "text" field
                int textStart = responseBody.indexOf("\"text\":\"") + 8;
                int textEnd = responseBody.indexOf("\"", textStart);
                if (textStart > 7 && textEnd > textStart) {
                    return responseBody.substring(textStart, textEnd)
                            .replace("\\n", "\n")
                            .replace("\\\"", "\"")
                            .replace("\\\\", "\\");
                }
                return "Error parsing Cohere response";
            } catch (Exception e) {
                return "Error parsing Cohere response: " + e.getMessage();
            }
        }
        
        /**
         * Escape JSON string
         */
        private String escapeJson(String text) {
            return text.replace("\\", "\\\\")
                      .replace("\"", "\\\"")
                      .replace("\n", "\\n")
                      .replace("\r", "\\r")
                      .replace("\t", "\\t");
        }
    }
}
