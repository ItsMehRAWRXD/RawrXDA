import javax.swing.*;
import java.awt.*;

public class SelectionPanel extends JPanel {
    private JTextArea textArea;
    private JButton selectAllButton, clearSelectionButton;

    public SelectionPanel() {
        setLayout(new BorderLayout());
        textArea = new JTextArea();
        JScrollPane scrollPane = new JScrollPane(textArea);
        add(scrollPane, BorderLayout.CENTER);

        selectAllButton = new JButton("Select All");
        clearSelectionButton = new JButton("Clear Selection");

        selectAllButton.addActionListener(e -> textArea.selectAll());
        clearSelectionButton.addActionListener(e -> textArea.select(0, 0));

        JPanel buttonPanel = new JPanel();
        buttonPanel.add(selectAllButton);
        buttonPanel.add(clearSelectionButton);
        add(buttonPanel, BorderLayout.SOUTH);
    }

    public String getSelectedText() {
        return textArea.getSelectedText();
    }

    public void setText(String text) {
        textArea.setText(text);
    }
}
