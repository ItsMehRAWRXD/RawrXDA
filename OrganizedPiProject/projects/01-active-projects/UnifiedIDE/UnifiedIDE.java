
import javax.swing.*;
import java.awt.*;

public class UnifiedIDE extends JFrame {
    public UnifiedIDE() {
        setTitle("Unified IDE");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1600, 1000);
        setLocationRelativeTo(null);

    JTabbedPane tabs = new JTabbedPane();
    tabs.addTab("Files", new FilePanel("UnifiedIDEProject"));
    tabs.addTab("Marketplace", new MarketplacePanel());
    tabs.addTab("Run", new RunPanel());
    tabs.addTab("Chat", new ChatPanel());
    add(tabs, BorderLayout.CENTER);
    }

    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new UnifiedIDE().setVisible(true));
    }
}
