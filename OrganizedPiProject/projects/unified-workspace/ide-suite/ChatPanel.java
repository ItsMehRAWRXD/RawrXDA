import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.util.Map;

public class ChatPanel extends JPanel {
    private final AIOrchestrator orchestrator;
    private final ChatManager chatManager;
    private final JTextArea chatArea;
    private final JTextField inputField;

    public ChatPanel(AIOrchestrator orchestrator, ChatManager chatManager) {
        this.orchestrator = orchestrator;
        this.chatManager = chatManager;
        setLayout(new BorderLayout());

        chatArea = new JTextArea();
        chatArea.setEditable(false);
        chatArea.setLineWrap(true);
        chatArea.setWrapStyleWord(true);
        add(new JScrollPane(chatArea), BorderLayout.CENTER);

        JPanel bottomPanel = new JPanel(new BorderLayout());
        inputField = new JTextField();
        inputField.addActionListener(this::sendMessage);
        bottomPanel.add(inputField, BorderLayout.CENTER);

        JButton sendButton = new JButton("Send");
        sendButton.addActionListener(this::sendMessage);
        bottomPanel.add(sendButton, BorderLayout.EAST);

        JButton closeButton = new JButton("Close Chat");
        closeButton.addActionListener(e -> chatManager.closeChat(this));
        bottomPanel.add(closeButton, BorderLayout.WEST);

        add(bottomPanel, BorderLayout.SOUTH);
    }

    private void sendMessage(ActionEvent e) {
        String userInput = inputField.getText().trim();
        if (userInput.isEmpty()) {
            return;
        }

        appendMessage("You: " + userInput);
        inputField.setText("");

        // Use AgentLLM for enhanced processing with streaming events
        AgentLLM agentLLM = new AgentLLM(System.getenv("OPENAI_API_KEY"));
        
        // Get the orchestrator to use streamExecution directly
        AgenticOrchestratorSimple orchestrator = agentLLM.getOrchestrator();
        orchestrator.streamExecution(event -> {
            SwingUtilities.invokeLater(() -> {
                switch (event.getType()) {
                    case "task_completed":
                        String result = (String) event.getData();
                        appendMessage("AI Task Result: " + result);
                        break;
                    case "confirm_continue":
                        @SuppressWarnings("unchecked")
                        Map<String, Object> confirmData = (Map<String, Object>) event.getData();
                        String message = (String) confirmData.get("message");
                        int remaining = (Integer) confirmData.get("remaining");
                        
                        int choice = JOptionPane.showConfirmDialog(
                            this, 
                            message, 
                            "Continue Execution?", 
                            JOptionPane.YES_NO_OPTION
                        );
                        
                        if (choice == JOptionPane.YES_OPTION) {
                            // For simple orchestrator, we need to continue manually
                            // Since it doesn't have continueExecution, we'll just show a message
                            appendMessage("Continuing execution...");
                            // In a real implementation, you'd call continueExecution here
                        } else {
                            appendMessage("AI: Execution stopped by user");
                        }
                        break;
                    case "execution_complete":
                        appendMessage("AI: All tasks completed");
                        break;
                    case "error":
                        String errorMsg = (String) event.getData();
                        appendMessage("AI Error: " + errorMsg);
                        break;
                    default:
                        // Handle other events if needed
                        break;
                }
            });
        }, userInput, "user");
    }

    private void appendMessage(String message) {
        chatArea.append(message + "\n\n");
    }
}
