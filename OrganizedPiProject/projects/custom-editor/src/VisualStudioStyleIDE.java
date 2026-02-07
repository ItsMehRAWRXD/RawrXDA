import java.net.http.*;
import java.net.URI;
import java.util.concurrent.CompletableFuture;
import java.util.prefs.Preferences;
import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class VisualStudioStyleIDE extends JFrame {
    private JTextArea editor;
    private JTextArea output;
    
    public VisualStudioStyleIDE() {
        setTitle("Visual Studio Style IDE");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1200, 800);
        createUI();
    }
    
    private void createUI() {
        setLayout(new BorderLayout());
        
        editor = new JTextArea();
        editor.setFont(new Font("Consolas", Font.PLAIN, 14));
        
        output = new JTextArea(10, 0);
        output.setEditable(false);
        
        JSplitPane split = new JSplitPane(JSplitPane.VERTICAL_SPLIT,
            new JScrollPane(editor), new JScrollPane(output));
        
        add(split, BorderLayout.CENTER);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new VisualStudioStyleIDE().setVisible(true));
    }
}

