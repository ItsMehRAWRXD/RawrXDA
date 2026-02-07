import javax.swing.*;
import java.awt.*;

public class CodeEditPanel extends JPanel {
    private JTextArea codeArea;
    private JButton formatButton, goButton;

    public CodeEditPanel() {
        setLayout(new BorderLayout());
        codeArea = new JTextArea();
        codeArea.setFont(new Font("Monospaced", Font.PLAIN, 14));
        JScrollPane scrollPane = new JScrollPane(codeArea);
        add(scrollPane, BorderLayout.CENTER);

        formatButton = new JButton("Format Code");
        formatButton.addActionListener(e -> formatCode());
        goButton = new JButton("Go");
        goButton.addActionListener(e -> runCode());

        JPanel buttonPanel = new JPanel();
        buttonPanel.add(formatButton);
        buttonPanel.add(goButton);
        add(buttonPanel, BorderLayout.SOUTH);
    }

    private void formatCode() {
        // Simple formatting: trim lines and indent
        String[] lines = codeArea.getText().split("\n");
        StringBuilder formatted = new StringBuilder();
        for (String line : lines) {
            formatted.append(line.trim()).append("\n");
        }
        codeArea.setText(formatted.toString());
    }

    private void runCode() {
        JOptionPane.showMessageDialog(this, "Go: Code execution simulated.");
        // Integrate with real code runner if needed
    }

    public String getCode() {
        return codeArea.getText();
    }

    public void setCode(String code) {
        codeArea.setText(code);
    }
}
