import javax.swing.*;
import java.awt.*;

public class SplitCodeComparePanel extends JPanel {
    private JTextArea leftCodeArea;
    private JTextArea rightCodeArea;

    public SplitCodeComparePanel() {
        setLayout(new BorderLayout());
        leftCodeArea = new JTextArea();
        rightCodeArea = new JTextArea();
        leftCodeArea.setFont(new Font("Monospaced", Font.PLAIN, 14));
        rightCodeArea.setFont(new Font("Monospaced", Font.PLAIN, 14));

        JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
            new JScrollPane(leftCodeArea),
            new JScrollPane(rightCodeArea));
        splitPane.setDividerLocation(0.5);
        add(splitPane, BorderLayout.CENTER);
    }

    public String getLeftCode() {
        return leftCodeArea.getText();
    }

    public String getRightCode() {
        return rightCodeArea.getText();
    }

    public void setLeftCode(String code) {
        leftCodeArea.setText(code);
    }

    public void setRightCode(String code) {
        rightCodeArea.setText(code);
    }
}
