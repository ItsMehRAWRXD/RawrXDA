import javax.swing.*;
import java.util.ArrayList;
import java.util.List;
import java.awt.BorderLayout;
import java.awt.FlowLayout;

public class ChatManager {
    private final JTabbedPane tabbedPane;
    private final List<ChatPanel> chatPanels = new ArrayList<>();
    private final AIOrchestrator orchestrator;
    private final JPanel mainPanel;
    private final JPanel controlPanel;

    public ChatManager(AIOrchestrator orchestrator) {
        this.orchestrator = orchestrator;
        this.tabbedPane = new JTabbedPane();
        this.mainPanel = new JPanel(new BorderLayout());
        this.controlPanel = createControlPanel();
        
        mainPanel.add(controlPanel, BorderLayout.NORTH);
        mainPanel.add(tabbedPane, BorderLayout.CENTER);
    }
    
    private JPanel createControlPanel() {
        JPanel panel = new JPanel(new FlowLayout());
        
        JButton newChatBtn = new JButton("New Chat");
        newChatBtn.addActionListener(e -> createNewChat());
        panel.add(newChatBtn);
        
        JComboBox<String> providerBox = new JComboBox<>(new String[]{"openai", "anthropic", "local"});
        providerBox.addActionListener(e -> 
            orchestrator.setProvider((String) providerBox.getSelectedItem()));
        panel.add(new JLabel("Provider:"));
        panel.add(providerBox);
        
        JButton statusBtn = new JButton("Status");
        statusBtn.addActionListener(e -> showStatus());
        panel.add(statusBtn);
        
        return panel;
    }
    
    private void showStatus() {
        StringBuilder status = new StringBuilder("Provider Status:\n");
        orchestrator.getProviderStatus().forEach((name, stat) -> 
            status.append(name).append(": ").append(stat).append("\n"));
        JOptionPane.showMessageDialog(mainPanel, status.toString());
    }

    public JComponent getComponent() {
        return mainPanel;
    }

    public void createNewChat() {
        ChatPanel newChatPanel = new ChatPanel(orchestrator, this);
        chatPanels.add(newChatPanel);
        tabbedPane.addTab("Chat " + chatPanels.size(), newChatPanel);
        tabbedPane.setSelectedComponent(newChatPanel);
    }

    public void closeChat(ChatPanel chatPanel) {
        int index = chatPanels.indexOf(chatPanel);
        if (index != -1) {
            tabbedPane.remove(index);
            chatPanels.remove(index);
        }
    }
}
