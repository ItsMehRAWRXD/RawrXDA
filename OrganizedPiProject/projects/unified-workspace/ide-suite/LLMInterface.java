import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;

public class LLMInterface extends JPanel {
    private final AIOrchestrator orchestrator;
    private final JTextArea outputArea;
    private final JTextField inputField;
    private final JComboBox<String> providerSelector;
    private final JComboBox<String> typeSelector;
    
    public LLMInterface() {
        this.orchestrator = new AIOrchestrator();
        orchestrator.initialize();
        
        setLayout(new BorderLayout());
        
        // Top panel with controls
        JPanel controlPanel = new JPanel(new FlowLayout());
        
        providerSelector = new JComboBox<>(new String[]{"openai", "anthropic", "local"});
        providerSelector.addActionListener(e -> 
            orchestrator.setProvider((String) providerSelector.getSelectedItem()));
        controlPanel.add(new JLabel("Provider:"));
        controlPanel.add(providerSelector);
        
        typeSelector = new JComboBox<>(new String[]{"CHAT", "CODE_COMPLETION", "CODE_REVIEW"});
        controlPanel.add(new JLabel("Type:"));
        controlPanel.add(typeSelector);
        
        JButton statusButton = new JButton("Status");
        statusButton.addActionListener(this::showStatus);
        controlPanel.add(statusButton);
        
        add(controlPanel, BorderLayout.NORTH);
        
        // Output area
        outputArea = new JTextArea(20, 60);
        outputArea.setEditable(false);
        outputArea.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        add(new JScrollPane(outputArea), BorderLayout.CENTER);
        
        // Input panel
        JPanel inputPanel = new JPanel(new BorderLayout());
        inputField = new JTextField();
        inputField.addActionListener(this::processInput);
        
        JButton sendButton = new JButton("Send");
        sendButton.addActionListener(this::processInput);
        
        inputPanel.add(inputField, BorderLayout.CENTER);
        inputPanel.add(sendButton, BorderLayout.EAST);
        add(inputPanel, BorderLayout.SOUTH);
        
        appendOutput("LLM Interface initialized. Select provider and type your prompt.");
    }
    
    private void processInput(ActionEvent e) {
        String input = inputField.getText().trim();
        if (input.isEmpty()) return;
        
        inputField.setText("");
        appendOutput("User: " + input);
        
        AIOrchestrator.RequestType type = AIOrchestrator.RequestType.valueOf(
            (String) typeSelector.getSelectedItem());
        
        orchestrator.processRequest(input, type)
            .whenComplete((response, error) -> {
                SwingUtilities.invokeLater(() -> {
                    if (error != null) {
                        appendOutput("Error: " + error.getMessage());
                    } else {
                        appendOutput("AI: " + response);
                    }
                });
            });
    }
    
    private void showStatus(ActionEvent e) {
        appendOutput("Provider Status:");
        orchestrator.getProviderStatus().forEach((name, status) -> 
            appendOutput("  " + name + ": " + status));
        appendOutput("Active: " + orchestrator.getActiveProvider());
    }
    
    private void appendOutput(String text) {
        outputArea.append(text + "\n");
        outputArea.setCaretPosition(outputArea.getDocument().getLength());
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame("LLM Interface");
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.add(new LLMInterface());
            frame.pack();
            frame.setLocationRelativeTo(null);
            frame.setVisible(true);
        });
    }
}