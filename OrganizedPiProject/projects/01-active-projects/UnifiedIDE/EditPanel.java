import javax.swing.*;
import java.awt.*;

public class EditPanel extends JPanel {
    private JTextArea textArea;
    private JButton clearButton;

    public EditPanel() {
        setLayout(new BorderLayout());
        textArea = new JTextArea();
        JScrollPane scrollPane = new JScrollPane(textArea);
        add(scrollPane, BorderLayout.CENTER);

        clearButton = new JButton("Clear");
        clearButton.addActionListener(e -> textArea.setText(""));
        JPanel buttonPanel = new JPanel();
        buttonPanel.add(clearButton);
        add(buttonPanel, BorderLayout.SOUTH);
    }

    public String getText() {
        return textArea.getText();
    }

    public void setText(String text) {
        textArea.setText(text);
    }
}
