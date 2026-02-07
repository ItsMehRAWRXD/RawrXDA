import java.net.http.*;
import java.net.URI;
import java.util.*;
import java.util.concurrent.*;
import javax.swing.*;
import java.awt.*;

public class MultiCopilot extends JFrame {
    private final Map<String, AIProvider> providers = new HashMap<>();
    private final Set<String> activeProviders = new HashSet<>();
    private final JTextArea chatArea = new JTextArea(20, 60);
    private final JTextField input = new JTextField(60);
    private final JPanel providerPanel = new JPanel();
    
    public MultiCopilot() {
        setupProviders();
        setupUI();
    }
    
    private void setupProviders() {
        providers.put("OpenAI", new OpenAIProvider());
        providers.put("Amazon Q", new AmazonQProvider());
        providers.put("GitHub", new GitHubProvider());
        providers.put("Claude", new ClaudeProvider());
    }
    
    private void setupUI() {
        setTitle("Multi-Copilot Chat");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        
        // Provider toggles
        providerPanel.setLayout(new FlowLayout());
        for (String name : providers.keySet()) {
            JCheckBox cb = new JCheckBox(name);
            cb.addActionListener(e -> {
                if (cb.isSelected()) {
                    activeProviders.add(name);
                    chatArea.append("[" + name + " ACTIVE]\n");
                } else {
                    activeProviders.remove(name);
                    chatArea.append("[" + name + " INACTIVE]\n");
                }
            });
            providerPanel.add(cb);
        }
        
        // Chat area
        chatArea.setEditable(false);
        chatArea.setBackground(Color.BLACK);
        chatArea.setForeground(Color.GREEN);
        
        // Input
        input.addActionListener(e -> sendToAll());
        
        JPanel inputPanel = new JPanel(new BorderLayout());
        inputPanel.add(input, BorderLayout.CENTER);
        JButton send = new JButton("Send to All Active");
        send.addActionListener(e -> sendToAll());
        inputPanel.add(send, BorderLayout.EAST);
        
        add(providerPanel, BorderLayout.NORTH);
        add(new JScrollPane(chatArea), BorderLayout.CENTER);
        add(inputPanel, BorderLayout.SOUTH);
        
        pack();
    }
    
    private void sendToAll() {
        String message = input.getText().trim();
        if (message.isEmpty()) return;
        
        chatArea.append("You: " + message + "\n");
        input.setText("");
        
        for (String providerName : activeProviders) {
            AIProvider provider = providers.get(providerName);
            CompletableFuture.supplyAsync(() -> provider.complete(message))
                .thenAccept(response -> SwingUtilities.invokeLater(() -> 
                    chatArea.append(providerName + ": " + response + "\n")));
        }
    }
    
    interface AIProvider {
        String complete(String prompt);
    }
    
    class OpenAIProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        
        public String complete(String prompt) {
            try {
                String json = "{\"model\":\"gpt-3.5-turbo\",\"max_tokens\":4000,\"messages\":[{\"role\":\"user\",\"content\":\"" + prompt + "\"}]}";
                HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.openai.com/v1/chat/completions"))
                    .header("Authorization", "Bearer " + System.getenv("OPENAI_API_KEY"))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
                
                HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
                return parseOpenAI(resp.body());
            } catch (Exception e) {
                return "Error: " + e.getMessage();
            }
        }
        
        private String parseOpenAI(String json) {
            if (json.contains("\"content\":")) {
                int start = json.indexOf("\"content\":\"") + 11;
                int end = json.lastIndexOf("\"");
                return json.substring(start, end).replaceAll("\\n", "\n");
            }
            return json;
        }
    }
    
    class AmazonQProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        
        public String complete(String prompt) {
            try {
                String json = "{\"inputText\":\"" + prompt + "\",\"textGenerationConfig\":{\"maxTokenCount\":8000,\"stopSequences\":[],\"temperature\":0.1,\"topP\":1}}";
                HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create("https://bedrock-runtime.us-east-1.amazonaws.com/model/amazon.titan-text-express-v1/invoke"))
                    .header("Authorization", "AWS4-HMAC-SHA256 " + System.getenv("AWS_ACCESS_KEY"))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
                
                HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
                return parseAmazonQ(resp.body());
            } catch (Exception e) {
                return "Amazon Q: " + prompt.substring(0, Math.min(50, prompt.length())) + "...";
            }
        }
        
        private String parseAmazonQ(String json) {
            if (json.contains("\"outputText\":")) {
                int start = json.indexOf("\"outputText\":\"") + 14;
                int end = json.lastIndexOf("\"");
                return json.substring(start, end).replaceAll("\\n", "\n");
            }
            return json;
        }
    }
    
    class GitHubProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        
        public String complete(String prompt) {
            try {
                String json = "{\"prompt\":\"" + prompt + "\",\"suffix\":\"\",\"max_tokens\":8000,\"temperature\":0.1,\"top_p\":1,\"n\":1,\"stream\":false}";
                HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.github.com/copilot/completions"))
                    .header("Authorization", "Bearer " + System.getenv("GITHUB_TOKEN"))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
                
                HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
                return parseGitHub(resp.body());
            } catch (Exception e) {
                return prompt + "\n// GitHub completion";
            }
        }
        
        private String parseGitHub(String json) {
            if (json.contains("\"text\":")) {
                int start = json.indexOf("\"text\":\"") + 8;
                int end = json.lastIndexOf("\"");
                return json.substring(start, end).replaceAll("\\n", "\n");
            }
            return json;
        }
    }
    
    class ClaudeProvider implements AIProvider {
        private final HttpClient client = HttpClient.newHttpClient();
        
        public String complete(String prompt) {
            try {
                String json = "{\"model\":\"claude-3-5-sonnet-20241022\",\"max_tokens\":8000,\"messages\":[{\"role\":\"user\",\"content\":\"" + prompt + "\"}]}";
                HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create("https://api.anthropic.com/v1/messages"))
                    .header("x-api-key", System.getenv("ANTHROPIC_API_KEY"))
                    .header("Content-Type", "application/json")
                    .header("anthropic-version", "2023-06-01")
                    .POST(HttpRequest.BodyPublishers.ofString(json))
                    .build();
                
                HttpResponse<String> resp = client.send(req, HttpResponse.BodyHandlers.ofString());
                return parseClaude(resp.body());
            } catch (Exception e) {
                return "Claude: " + prompt.substring(0, Math.min(40, prompt.length())) + "...";
            }
        }
        
        private String parseClaude(String json) {
            if (json.contains("\"text\":")) {
                int start = json.indexOf("\"text\":\"") + 8;
                int end = json.lastIndexOf("\"");
                return json.substring(start, end).replaceAll("\\n", "\n");
            }
            return json;
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new MultiCopilot().setVisible(true));
    }
}