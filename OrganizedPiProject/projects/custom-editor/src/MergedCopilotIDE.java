import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.util.concurrent.*;

public class MergedCopilotIDE extends JFrame {
    private final MultiCopilot copilot = new MultiCopilot();
    private final JTextArea editor = new JTextArea(25, 80);
    private final JTextArea mergedChat = new JTextArea(15, 80);
    private final Map<String, Boolean> providerStates = new HashMap<>();
    
    public MergedCopilotIDE() {
        setupUI();
        integrateProviders();
    }
    
    private void setupUI() {
        setTitle("IDE with Merged Multi-Copilot");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(new BorderLayout());
        
        // Provider controls
        JPanel controls = new JPanel(new FlowLayout());
        String[] providers = {"OpenAI", "Amazon Q", "GitHub", "Claude"};
        
        for (String provider : providers) {
            JToggleButton toggle = new JToggleButton(provider);
            toggle.addActionListener(e -> {
                providerStates.put(provider, toggle.isSelected());
                mergedChat.append("[" + provider + (toggle.isSelected() ? " ON" : " OFF") + "]\n");
            });
            controls.add(toggle);
        }
        
        JButton mergeAll = new JButton("Merge All Responses");
        mergeAll.addActionListener(e -> mergeResponses());
        controls.add(mergeAll);
        
        // Editor with auto-completion
        editor.setFont(new Font("Monospaced", Font.PLAIN, 14));
        editor.addKeyListener(new KeyAdapter() {
            @Override
            public void keyPressed(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.VK_TAB) {
                    e.consume();
                    triggerCompletion();
                }
            }
        });
        
        // Merged chat area
        mergedChat.setEditable(false);
        mergedChat.setBackground(new Color(40, 40, 40));
        mergedChat.setForeground(Color.WHITE);
        
        // Layout
        JSplitPane mainSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
            new JScrollPane(editor), new JScrollPane(mergedChat));
        mainSplit.setDividerLocation(600);
        
        add(controls, BorderLayout.NORTH);
        add(mainSplit, BorderLayout.CENTER);
        
        pack();
    }
    
    private void integrateProviders() {
        // Initialize all providers as active
        providerStates.put("OpenAI", true);
        providerStates.put("Amazon Q", true);
        providerStates.put("GitHub", true);
        providerStates.put("Claude", true);
    }
    
    private void triggerCompletion() {
        String code = editor.getText();
        int pos = editor.getCaretPosition();
        String context = code.substring(Math.max(0, pos-200), pos);
        
        mergedChat.append("Requesting completions for: " + context.substring(Math.max(0, context.length()-50)) + "\n");
        
        Map<String, CompletableFuture<String>> futures = new HashMap<>();
        
        for (Map.Entry<String, Boolean> entry : providerStates.entrySet()) {
            if (entry.getValue()) {
                String provider = entry.getKey();
                futures.put(provider, getCompletion(provider, context));
            }
        }
        
        // Collect all responses
        CompletableFuture.allOf(futures.values().toArray(new CompletableFuture[0]))
            .thenRun(() -> {
                StringBuilder merged = new StringBuilder("MERGED COMPLETIONS:\n");
                for (Map.Entry<String, CompletableFuture<String>> entry : futures.entrySet()) {
                    try {
                        String response = entry.getValue().get();
                        merged.append(entry.getKey()).append(": ").append(response).append("\n");
                    } catch (Exception e) {
                        merged.append(entry.getKey()).append(": Error\n");
                    }
                }
                
                SwingUtilities.invokeLater(() -> {
                    mergedChat.append(merged.toString() + "\n");
                    // Insert best completion into editor
                    String bestCompletion = selectBestCompletion(futures);
                    editor.insert(bestCompletion, pos);
                });
            });
    }
    
    private CompletableFuture<String> getCompletion(String provider, String context) {
        return CompletableFuture.supplyAsync(() -> {
            switch (provider) {
                case "OpenAI":
                    return callOpenAI(context);
                case "Amazon Q":
                    return callAmazonQ(context);
                case "GitHub":
                    return callGitHub(context);
                case "Claude":
                    return callClaude(context);
                default:
                    return "Unknown provider";
            }
        });
    }
    
    private String callOpenAI(String context) {
        try {
            Thread.sleep(500 + (int)(Math.random() * 1000));
            return "{\n    // OpenAI completion\n    return result;\n}";
        } catch (InterruptedException e) {
            return "// OpenAI timeout";
        }
    }
    
    private String callAmazonQ(String context) {
        try {
            Thread.sleep(300 + (int)(Math.random() * 800));
            return "{\n    // Amazon Q suggestion\n    System.out.println(\"result\");\n}";
        } catch (InterruptedException e) {
            return "// Amazon Q timeout";
        }
    }
    
    private String callGitHub(String context) {
        try {
            Thread.sleep(200 + (int)(Math.random() * 600));
            return "{\n    // GitHub Copilot\n    process();\n}";
        } catch (InterruptedException e) {
            return "// GitHub timeout";
        }
    }
    
    private String callClaude(String context) {
        try {
            Thread.sleep(400 + (int)(Math.random() * 900));
            return "{\n    // Claude completion\n    execute();\n}";
        } catch (InterruptedException e) {
            return "// Claude timeout";
        }
    }
    
    private String selectBestCompletion(Map<String, CompletableFuture<String>> futures) {
        // Simple selection - pick first available
        for (CompletableFuture<String> future : futures.values()) {
            try {
                String result = future.get();
                if (!result.contains("timeout") && !result.contains("Error")) {
                    return result;
                }
            } catch (Exception e) {
                continue;
            }
        }
        return "// No completion available";
    }
    
    private void mergeResponses() {
        String prompt = JOptionPane.showInputDialog(this, "Enter prompt for all providers:");
        if (prompt == null || prompt.trim().isEmpty()) return;
        
        mergedChat.append("PROMPT: " + prompt + "\n");
        
        for (Map.Entry<String, Boolean> entry : providerStates.entrySet()) {
            if (entry.getValue()) {
                String provider = entry.getKey();
                getCompletion(provider, prompt).thenAccept(response -> 
                    SwingUtilities.invokeLater(() -> 
                        mergedChat.append(provider + ": " + response + "\n")));
            }
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new MergedCopilotIDE().setVisible(true));
    }
}