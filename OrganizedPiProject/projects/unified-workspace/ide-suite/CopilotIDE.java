import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.http.*;
import java.net.URI;
import java.util.*;
import java.util.concurrent.*;

public class CopilotIDE extends JFrame {
    private JTextArea editor;
    private JTextArea copilotPanel;
    private JLabel statusBar;
    private AIProvider currentProvider = AIProvider.GITHUB_COPILOT;
    private String apiKey = "";
    
    enum AIProvider {
        GITHUB_COPILOT("GitHub Copilot", "https://api.github.com/copilot"),
        OPENAI_CODEX("OpenAI Codex", "https://api.openai.com/v1/completions"),
        TABNINE("TabNine", "https://api.tabnine.com"),
        CODEWHISPERER("CodeWhisperer", "https://codewhisperer.aws.amazon.com"),
        INTELLICODE("IntelliCode", "https://api.intellicode.microsoft.com"),
        KITE("Kite", "https://api.kite.com"),
        SOURCERY("Sourcery", "https://api.sourcery.ai"),
        DEEPCODE("DeepCode", "https://api.deepcode.ai"),
        CODOTA("Codota", "https://api.codota.com"),
        AICODE("AI Code", "https://api.aicode.dev"),
        KIMI("Kimi AI", "https://api.moonshot.cn/v1/chat/completions");
        
        final String name;
        final String endpoint;
        AIProvider(String name, String endpoint) { this.name = name; this.endpoint = endpoint; }
    }
    
    public CopilotIDE() {
        setTitle("π Copilot IDE - AI-First Programming");
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setSize(1400, 900);
        
        setupUI();
        setupCopilotEngine();
        setLocationRelativeTo(null);
    }
    
    private void setupUI() {
        editor = new JTextArea();
        editor.setFont(new Font("JetBrains Mono", Font.PLAIN, 14));
        editor.setBackground(new Color(30, 30, 30));
        editor.setForeground(Color.WHITE);
        editor.setCaretColor(Color.WHITE);
        
        copilotPanel = new JTextArea();
        copilotPanel.setFont(new Font("Consolas", Font.PLAIN, 12));
        copilotPanel.setBackground(new Color(20, 20, 20));
        copilotPanel.setForeground(new Color(0, 255, 127));
        copilotPanel.setEditable(false);
        copilotPanel.setText("π Copilot Ready - Start typing for AI suggestions...");
        
        JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
            new JScrollPane(editor), new JScrollPane(copilotPanel));
        splitPane.setDividerLocation(700);
        
        JPanel topPanel = new JPanel(new FlowLayout());
        JComboBox<AIProvider> providerBox = new JComboBox<>(AIProvider.values());
        providerBox.addActionListener(e -> currentProvider = (AIProvider) providerBox.getSelectedItem());
        
        JButton keyBtn = new JButton("API Key");
        keyBtn.addActionListener(e -> setApiKey());
        
        JButton completeBtn = new JButton("Complete π");
        completeBtn.addActionListener(e -> triggerCompletion());
        
        topPanel.add(new JLabel("Provider:"));
        topPanel.add(providerBox);
        topPanel.add(keyBtn);
        topPanel.add(completeBtn);
        
        statusBar = new JLabel(" Ready - π Copilot Active");
        statusBar.setBorder(BorderFactory.createLoweredBevelBorder());
        
        add(topPanel, BorderLayout.NORTH);
        add(splitPane, BorderLayout.CENTER);
        add(statusBar, BorderLayout.SOUTH);
        
        editor.getDocument().addDocumentListener(new javax.swing.event.DocumentListener() {
            public void insertUpdate(javax.swing.event.DocumentEvent e) { onTextChange(); }
            public void removeUpdate(javax.swing.event.DocumentEvent e) { onTextChange(); }
            public void changedUpdate(javax.swing.event.DocumentEvent e) {}
        });
    }
    
    private void setupCopilotEngine() {
        javax.swing.Timer copilotTimer = new javax.swing.Timer(1000, e -> autoComplete());
        copilotTimer.start();
    }
    
    private void onTextChange() {
        SwingUtilities.invokeLater(() -> {
            String text = editor.getText();
            if (text.length() > 10) {
                copilotPanel.setText("π Analyzing: " + text.substring(Math.max(0, text.length()-50)) + "...");
            }
        });
    }
    
    private void autoComplete() {
        String currentText = editor.getText();
        if (currentText.trim().isEmpty()) return;
        
        CompletableFuture.supplyAsync(() -> {
            try {
                return generateCompletion(currentText);
            } catch (Exception e) {
                return "π Error: " + e.getMessage();
            }
        }).thenAccept(completion -> {
            SwingUtilities.invokeLater(() -> {
                copilotPanel.setText("π " + currentProvider.name + " Suggests:\n\n" + completion);
            });
        });
    }
    
    private void triggerCompletion() {
        String suggestion = copilotPanel.getText();
        if (suggestion.contains("Suggests:")) {
            String code = suggestion.substring(suggestion.indexOf("Suggests:") + 9).trim();
            if (code.startsWith("π ")) code = code.substring(2);
            
            int caretPos = editor.getCaretPosition();
            try {
                editor.getDocument().insertString(caretPos, "\n" + code, null);
                statusBar.setText(" π Completion Applied");
            } catch (Exception e) {
                statusBar.setText(" π Error applying completion");
            }
        }
    }
    
    private String generateCompletion(String context) throws Exception {
        switch (currentProvider) {
            case GITHUB_COPILOT:
                return generateGitHubCompletion(context);
            case OPENAI_CODEX:
                return generateOpenAICompletion(context);
            case KIMI:
                return generateKimiCompletion(context);
            default:
                return generateMockCompletion(context);
        }
    }
    
    private String generateGitHubCompletion(String context) throws Exception {
        if (apiKey.isEmpty()) return "π Set GitHub API key first";
        
        String prompt = "Complete this code:\n" + context;
        HttpClient client = HttpClient.newHttpClient();
        
        String json = String.format("""
            {
                "prompt": "%s",
                "max_tokens": 150,
                "temperature": 0.2,
                "stop": ["\\n\\n"]
            }
            """, prompt.replace("\"", "\\\""));
        
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(currentProvider.endpoint + "/completions"))
            .header("Authorization", "Bearer " + apiKey)
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(json))
            .build();
        
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        
        if (response.statusCode() == 200) {
            return parseCompletion(response.body());
        } else {
            return "π API Error: " + response.statusCode();
        }
    }
    
    private String generateOpenAICompletion(String context) throws Exception {
        if (apiKey.isEmpty()) return "π Set OpenAI API key first";
        
        String prompt = "// Complete this Java code:\n" + context + "\n// Completion:";
        HttpClient client = HttpClient.newHttpClient();
        
        String json = String.format("""
            {
                "model": "code-davinci-002",
                "prompt": "%s",
                "max_tokens": 100,
                "temperature": 0.1
            }
            """, prompt.replace("\"", "\\\""));
        
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create("https://api.openai.com/v1/completions"))
            .header("Authorization", "Bearer " + apiKey)
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(json))
            .build();
        
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        return response.statusCode() == 200 ? parseCompletion(response.body()) : "π OpenAI Error";
    }
    
    private String generateKimiCompletion(String context) throws Exception {
        if (apiKey.isEmpty()) return "π Set Kimi API key first";
        
        HttpClient client = HttpClient.newHttpClient();
        
        String json = String.format("""
            {
                "model": "moonshot-v1-8k",
                "messages": [
                    {"role": "system", "content": "You are a helpful coding assistant. Complete the given code."},
                    {"role": "user", "content": "Complete this Java code:\n%s"}
                ],
                "max_tokens": 150,
                "temperature": 0.2
            }
            """, context.replace("\"", "\\\""));
        
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create("https://api.moonshot.cn/v1/chat/completions"))
            .header("Authorization", "Bearer " + apiKey)
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(json))
            .build();
        
        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());
        
        if (response.statusCode() == 200) {
            return parseKimiResponse(response.body());
        } else {
            return "π Kimi Error: " + response.statusCode() + " - " + response.body();
        }
    }
    
    private String generateMockCompletion(String context) {
        String[] suggestions = {
            "public static void main(String[] args) {\n    System.out.println(\"Hello π World!\");\n}",
            "private void processData() {\n    // π AI-generated method\n    return result;\n}",
            "try {\n    // π Exception handling\n} catch (Exception e) {\n    e.printStackTrace();\n}",
            "for (int i = 0; i < length; i++) {\n    // π Loop iteration\n    process(items[i]);\n}",
            "if (condition) {\n    // π Conditional logic\n    return true;\n}"
        };
        
        return suggestions[new Random().nextInt(suggestions.length)];
    }
    
    private String parseCompletion(String jsonResponse) {
        try {
            int start = jsonResponse.indexOf("\"text\":\"") + 8;
            int end = jsonResponse.indexOf("\"", start);
            return jsonResponse.substring(start, end).replace("\\n", "\n");
        } catch (Exception e) {
            return "π Parsed: " + jsonResponse.substring(0, Math.min(200, jsonResponse.length()));
        }
    }
    
    private String parseKimiResponse(String jsonResponse) {
        try {
            int start = jsonResponse.indexOf("\"content\":\"") + 11;
            int end = jsonResponse.indexOf("\"", start);
            return jsonResponse.substring(start, end).replace("\\n", "\n");
        } catch (Exception e) {
            return "π Kimi Response: " + jsonResponse.substring(0, Math.min(300, jsonResponse.length()));
        }
    }
    
    private void setApiKey() {
        String key = JOptionPane.showInputDialog(this, 
            "Enter " + currentProvider.name + " API Key:", 
            "API Configuration", 
            JOptionPane.QUESTION_MESSAGE);
        if (key != null && !key.trim().isEmpty()) {
            apiKey = key.trim();
            statusBar.setText(" π API Key configured for " + currentProvider.name);
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            // System look and feel
            new CopilotIDE().setVisible(true);
        });
    }
}