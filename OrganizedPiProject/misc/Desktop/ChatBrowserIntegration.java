import java.awt.Desktop;
import java.net.URI;
import java.util.Map;
import java.util.HashMap;

public class ChatBrowserIntegration {
    private static final Map<String, String> AI_PROVIDERS = new HashMap<>();
    
    static {
        AI_PROVIDERS.put("chatgpt", "https://chat.openai.com");
        AI_PROVIDERS.put("claude", "https://claude.ai");
        AI_PROVIDERS.put("kimi", "https://kimi.moonshot.cn");
        AI_PROVIDERS.put("gemini", "https://gemini.google.com");
        AI_PROVIDERS.put("perplexity", "https://perplexity.ai");
        AI_PROVIDERS.put("poe", "https://poe.com");
        AI_PROVIDERS.put("character", "https://character.ai");
        AI_PROVIDERS.put("huggingface", "https://huggingface.co/chat");
    }
    
    public static void openAIChat(String provider) {
        String url = AI_PROVIDERS.get(provider.toLowerCase());
        if (url == null) {
            System.err.println("Unknown AI provider: " + provider);
            return;
        }
        
        try {
            if (Desktop.isDesktopSupported()) {
                Desktop.getDesktop().browse(new URI(url));
                System.out.println("Opened " + provider + " in browser");
            } else {
                // Fallback for systems without Desktop support
                String os = System.getProperty("os.name").toLowerCase();
                ProcessBuilder pb;
                
                if (os.contains("win")) {
                    pb = new ProcessBuilder("cmd", "/c", "start", url);
                } else if (os.contains("mac")) {
                    pb = new ProcessBuilder("open", url);
                } else {
                    // Linux
                    pb = new ProcessBuilder("xdg-open", url);
                }
                
                pb.start();
                System.out.println("Launched " + provider + " via system command");
            }
        } catch (Exception e) {
            System.err.println("Failed to open browser: " + e.getMessage());
        }
    }
    
    public static void showAIMenu() {
        System.out.println("\n=== AI Chat Providers ===");
        System.out.println("1. ChatGPT - OpenAI's conversational AI");
        System.out.println("2. Claude - Anthropic's AI assistant");
        System.out.println("3. Kimi - Moonshot AI (Chinese)");
        System.out.println("4. Gemini - Google's AI model");
        System.out.println("5. Perplexity - AI-powered search");
        System.out.println("6. Poe - Multiple AI models");
        System.out.println("7. Character.AI - Character-based AI");
        System.out.println("8. HuggingFace - Open source AI chat");
        System.out.println("0. Cancel");
        System.out.print("Choose AI provider: ");
    }
    
    public static void handleAIChoice(String choice) {
        switch (choice.trim()) {
            case "1": openAIChat("chatgpt"); break;
            case "2": openAIChat("claude"); break;
            case "3": openAIChat("kimi"); break;
            case "4": openAIChat("gemini"); break;
            case "5": openAIChat("perplexity"); break;
            case "6": openAIChat("poe"); break;
            case "7": openAIChat("character"); break;
            case "8": openAIChat("huggingface"); break;
            case "0": System.out.println("Cancelled"); break;
            default: System.out.println("Invalid choice"); break;
        }
    }
    
    public static void openWithContext(String provider, String codeContext) {
        String url = AI_PROVIDERS.get(provider.toLowerCase());
        if (url == null) return;
        
        // For some providers, we can pre-populate with context
        if (provider.equals("chatgpt")) {
            // ChatGPT supports URL parameters for context
            url += "?q=" + java.net.URLEncoder.encode("Help me with this code: " + codeContext, "UTF-8");
        }
        
        try {
            Desktop.getDesktop().browse(new URI(url));
            System.out.println("Opened " + provider + " with code context");
        } catch (Exception e) {
            System.err.println("Failed to open with context: " + e.getMessage());
        }
    }
}