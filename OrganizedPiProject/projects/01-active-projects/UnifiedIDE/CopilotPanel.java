import javax.swing.*;
import java.awt.*;

public class CopilotPanel extends JPanel {
    private JComboBox<String> hostBox;
    private JTextArea promptArea, responseArea;
    private JButton runButton;

    public CopilotPanel() {
        setLayout(new BorderLayout());
        hostBox = new JComboBox<>(new String[]{"GitHub", "GitLab", "Bitbucket", "Local"});
        promptArea = new JTextArea(5, 40);
        responseArea = new JTextArea(10, 40);
        responseArea.setEditable(false);
        runButton = new JButton("Ask Copilot");

        JPanel topPanel = new JPanel();
        topPanel.add(new JLabel("Host:"));
        topPanel.add(hostBox);
        add(topPanel, BorderLayout.NORTH);

        add(new JScrollPane(promptArea), BorderLayout.CENTER);
        add(runButton, BorderLayout.EAST);
        add(new JScrollPane(responseArea), BorderLayout.SOUTH);

        runButton.addActionListener(e -> runCopilot());
    }

    private void runCopilot() {
        String host = (String) hostBox.getSelectedItem();
        String prompt = promptArea.getText();
        // Simulate Copilot response
        responseArea.setText("Copilot (" + host + ") response:\n" + (prompt.isEmpty() ? "No prompt provided." : "[Simulated response for: " + prompt + "]"));
    }
}
