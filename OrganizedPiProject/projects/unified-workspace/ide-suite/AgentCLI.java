import java.util.Scanner;

public class AgentCLI {
    private final AgentLLM agentLLM;
    private final Scanner scanner = new Scanner(System.in);
    
    public AgentCLI() {
        String apiKey = System.getenv("OPENAI_API_KEY");
        if (apiKey == null) {
            System.out.println("Warning: OPENAI_API_KEY not set");
        }
        this.agentLLM = new AgentLLM(apiKey);
    }
    
    public void run() {
        System.out.println("Agent CLI - Enhanced with Orchestration");
        System.out.println("Commands: /stream <prompt>, /complete <prompt>, /exit");
        
        while (true) {
            System.out.print("> ");
            String input = scanner.nextLine().trim();
            
            if (input.startsWith("/")) {
                handleCommand(input);
            } else if (!input.isEmpty()) {
                complete(input);
            }
        }
    }
    
    private void handleCommand(String command) {
        String[] parts = command.split(" ", 3);
        String cmd = parts[0];
        
        switch (cmd) {
            case "/key":
                if (parts.length >= 3) {
                    System.setProperty(parts[1].toUpperCase() + "_API_KEY", parts[2]);
                    System.out.println("API key set for: " + parts[1]);
                } else {
                    System.out.println("Usage: /key <provider> <key>");
                }
                break;
                
            case "/switch":
                if (parts.length >= 2) {
                    agentLLM.setProvider(parts[1]);
                    System.out.println("Switched to: " + parts[1]);
                } else {
                    System.out.println("Usage: /switch <provider>");
                }
                break;
                
            case "/complete":
                if (parts.length >= 2) {
                    complete(parts[1]);
                } else {
                    System.out.println("Usage: /complete <prompt>");
                }
                break;
                
            case "/providers":
                System.out.println("Available: echo, openai, anthropic");
                break;
                
            case "/stream":
                if (parts.length >= 2) {
                    stream(parts[1]);
                } else {
                    System.out.println("Usage: /stream <prompt>");
                }
                break;
                
            case "/exit":
                System.exit(0);
                break;
                
            default:
                System.out.println("Unknown command: " + cmd);
        }
    }
    
    private void complete(String prompt) {
        System.out.println("Processing...");
        agentLLM.complete(prompt)
            .whenComplete((response, error) -> {
                if (error != null) {
                    System.out.println("Error: " + error.getMessage());
                } else {
                    System.out.println("Response: " + response);
                }
            }).join();
    }
    
    private void stream(String prompt) {
        System.out.println("Streaming...");
        agentLLM.stream(prompt, token -> System.out.print(token));
        System.out.println("\nStream complete.");
    }
    
    public static void main(String[] args) {
        new AgentCLI().run();
    }
}