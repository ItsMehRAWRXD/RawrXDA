import javax.swing.*;
import java.awt.*;

public class CodeOnlyEditPanel extends JPanel {
    private JTextArea codeArea;

    public CodeOnlyEditPanel() {
        setLayout(new BorderLayout());
        codeArea = new JTextArea();
        codeArea.setFont(new Font("Monospaced", Font.PLAIN, 14));
        JScrollPane scrollPane = new JScrollPane(codeArea);
        add(scrollPane, BorderLayout.CENTER);
    }

    public String getCode() {
        return codeArea.getText();
    }

    public void setCode(String code) {
        codeArea.setText(code);
    }
}
