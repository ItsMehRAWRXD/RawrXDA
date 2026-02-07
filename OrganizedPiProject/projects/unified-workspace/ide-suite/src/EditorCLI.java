import java.util.*;
import java.util.concurrent.CompletableFuture;

public class EditorCLI {
    private final Map<String, String> apiKeys = new HashMap<>();
    private String currentProvider = "openai";
    private final ApiKeyManager keyManager = new ApiKeyManager();
    
    public static void main(String[] args) {
        EditorCLI cli = new EditorCLI();
        Scanner scanner = new Scanner(System.in);
        
        System.out.println("Advanced Editor CLI - Multi-Provider AI Integration");
        System.out.println("Type /help for commands");
        
        while (true) {
            System.out.print("> ");
            String input = scanner.nextLine().trim();
            
            if (input.equals("/quit") || input.equals("/exit")) {
                break;
            }
            
            cli.processCommand(input);
        }
        
        scanner.close();
    }
    
    private void processCommand(String input) {
        if (input.startsWith("/key ")) {
            handleKeyCommand(input);
        } else if (input.startsWith("/switch ")) {
            handleSwitchCommand(input);
        } else if (input.startsWith("/complete ")) {
            handleCompleteCommand(input);
        } else if (input.equals("/providers")) {
            showProviders();
        } else if (input.equals("/help")) {
            showHelp();
        } else {
            System.out.println("Unknown command. Type /help for available commands.");
        }
    }
    
    private void handleKeyCommand(String input) {
        String[] parts = input.split(" ", 3);
        if (parts.length < 3) {
            System.out.println("Usage: /key <provider> <api-key>");
            return;
        }
        
        String provider = parts[1];
        String key = parts[2];
        
        apiKeys.put(provider, key);
        keyManager.setApiKey(provider, key);
        System.out.println("API key set for " + provider);
    }
    
    private void handleSwitchCommand(String input) {
        String[] parts = input.split(" ", 2);
        if (parts.length < 2) {
            System.out.println("Usage: /switch <provider>");
            return;
        }
        
        String provider = parts[1];
        if (!apiKeys.containsKey(provider)) {
            System.out.println("No API key set for " + provider + ". Use /key command first.");
            return;
        }
        
        currentProvider = provider;
        System.out.println("Switched to " + provider);
    }
    
    private void handleCompleteCommand(String input) {
        String prompt = input.substring(10); // Remove "/complete "
        
        if (!apiKeys.containsKey(currentProvider)) {
            System.out.println("No API key set for " + currentProvider);
            return;
        }
        
        System.out.println("Generating completion with " + currentProvider + "...");
        
        try {
            String completion = generateCompletion(prompt);
            System.out.println("Completion:");
            System.out.println(completion);
        } catch (Exception e) {
            System.out.println("Error: " + e.getMessage());
        }
    }
    
    private String generateCompletion(String prompt) {
        // Simulate AI completion based on provider
        switch (currentProvider) {
            case "openai":
                return generateOpenAICompletion(prompt);
            case "anthropic":
                return generateAnthropicCompletion(prompt);
            default:
                return "// Completion not available for " + currentProvider;
        }
    }
    
    private String generateOpenAICompletion(String prompt) {
        // Mock OpenAI completion
        if (prompt.contains("function fibonacci")) {
            return "\\n    if (n <= 1) return n;\\n    return fibonacci(n-1) + fibonacci(n-2);\\n}";
        }
        return "// OpenAI completion for: " + prompt;
    }
    
    private String generateAnthropicCompletion(String prompt) {
        // Mock Anthropic completion
        if (prompt.contains("function fibonacci")) {
            return "\\n    return n <= 1 ? n : fibonacci(n-1) + fibonacci(n-2);\\n}";
        }
        return "// Anthropic completion for: " + prompt;
    }
    
    private void showProviders() {
        System.out.println("Available providers:");
        System.out.println("- openai" + (apiKeys.containsKey("openai") ? " (configured)" : ""));
        System.out.println("- anthropic" + (apiKeys.containsKey("anthropic") ? " (configured)" : ""));
        System.out.println("Current: " + currentProvider);
    }
    
    private void showHelp() {
        System.out.println("Available commands:");
        System.out.println("/key <provider> <api-key>  - Set API key for provider");
        System.out.println("/switch <provider>         - Switch to provider");
        System.out.println("/complete \"<prompt>\"       - Generate code completion");
        System.out.println("/providers                 - Show available providers");
        System.out.println("/help                      - Show this help");
        System.out.println("/quit                      - Exit the CLI");
    }
}