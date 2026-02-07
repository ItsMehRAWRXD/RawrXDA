import java.awt.*;
import java.awt.event.*;
import java.awt.datatransfer.*;
import javax.swing.*;
import java.util.*;

public class AutoResponder extends JFrame {
    private final Map<String, String> responses = new HashMap<>();
    private final JTextArea chatArea = new JTextArea(20, 50);
    private final JTextField inputField = new JTextField(50);
    private boolean autoMode = false;
    
    public AutoResponder() {
        setupResponses();
        setupUI();
        setupHotkeys();
    }
    
    private void setupResponses() {
        responses.put("1", "Continue to iterate?");
        responses.put("2", "I'm compiling the project now. Will share results shortly.");
        responses.put("3", "I'm updating the code based on our discussion.");
        responses.put("4", "I'm analyzing the implementation to identify issues.");
        responses.put("5", "I'm currently busy but I've noted your request.");
        responses.put("6", "In progress - working on the native compiler integration.");
    }
    
    private void setupUI() {
        setTitle("Auto Responder");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        
        chatArea.setEditable(false);
        chatArea.setBackground(Color.BLACK);
        chatArea.setForeground(Color.GREEN);
        
        JPanel panel = new JPanel(new BorderLayout());
        panel.add(new JScrollPane(chatArea), BorderLayout.CENTER);
        panel.add(inputField, BorderLayout.SOUTH);
        
        JPanel controls = new JPanel();
        JButton toggleBtn = new JButton("Toggle Auto");
        toggleBtn.addActionListener(e -> {
            autoMode = !autoMode;
            chatArea.append("[AUTO: " + (autoMode ? "ON" : "OFF") + "]\n");
        });
        controls.add(toggleBtn);
        panel.add(controls, BorderLayout.NORTH);
        
        add(panel);
        pack();
        
        inputField.addActionListener(e -> {
            String msg = inputField.getText();
            chatArea.append("You: " + msg + "\n");
            if (autoMode) processMessage(msg);
            inputField.setText("");
        });
    }
    
    private void setupHotkeys() {
        for (int i = 1; i <= 6; i++) {
            final String key = String.valueOf(i);
            KeyStroke ks = KeyStroke.getKeyStroke("alt " + i);
            getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW).put(ks, "response" + i);
            getRootPane().getActionMap().put("response" + i, new AbstractAction() {
                public void actionPerformed(ActionEvent e) {
                    sendResponse(responses.get(key));
                }
            });
        }
    }
    
    private void processMessage(String msg) {
        if (msg.contains("compile") || msg.contains("build")) {
            sendResponse(responses.get("2"));
        } else if (msg.contains("update") || msg.contains("change")) {
            sendResponse(responses.get("3"));
        } else if (msg.contains("issue") || msg.contains("problem")) {
            sendResponse(responses.get("4"));
        } else if (msg.length() > 20) {
            sendResponse(responses.get("5"));
        }
    }
    
    private void sendResponse(String response) {
        chatArea.append("AI: " + response + "\n");
        chatArea.setCaretPosition(chatArea.getDocument().getLength());
    }
    
    public void processClipboard(String content) {
        chatArea.append("Clipboard: " + content.substring(0, Math.min(100, content.length())) + "\n");
        if (autoMode) {
            processMessage(content);
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new AutoResponder().setVisible(true));
    }
}