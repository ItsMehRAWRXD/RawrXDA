import javax.swing.*;
import java.awt.*;

public class ViewPanel extends JPanel {
    private JTextArea textArea;
    private JCheckBox wordWrapBox;
    private JButton fontButton;

    public ViewPanel() {
        setLayout(new BorderLayout());
        textArea = new JTextArea();
        JScrollPane scrollPane = new JScrollPane(textArea);
        add(scrollPane, BorderLayout.CENTER);

        wordWrapBox = new JCheckBox("Word Wrap");
        wordWrapBox.addActionListener(e -> textArea.setLineWrap(wordWrapBox.isSelected()));

        fontButton = new JButton("Change Font");
        fontButton.addActionListener(e -> changeFont());

        JPanel controlPanel = new JPanel();
        controlPanel.add(wordWrapBox);
        controlPanel.add(fontButton);
        add(controlPanel, BorderLayout.SOUTH);
    }

    private void changeFont() {
        Font current = textArea.getFont();
        Font newFont = new Font("Monospaced", Font.PLAIN, current.getSize() == 14 ? 18 : 14);
        textArea.setFont(newFont);
    }

    public void setText(String text) {
        textArea.setText(text);
    }

    public String getText() {
        return textArea.getText();
    }
}
