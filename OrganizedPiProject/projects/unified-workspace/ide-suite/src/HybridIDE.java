import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.util.regex.*;
import java.util.concurrent.CompletableFuture;

public class HybridIDE extends JFrame {
    private final CopilotIntegration copilot = new CopilotIntegration();
    private final WebSocketBuildServer buildServer = new WebSocketBuildServer();
    private JTextArea editor;
    private JTextArea output;
    
    public HybridIDE() {
        setTitle("Hybrid IDE - WebSocket Build + Copilot");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1200, 800);
        
        createUI();
        startBuildServer();
    }
    
    private void createUI() {
        setLayout(new BorderLayout());
        
        // Menu
        JMenuBar menuBar = new JMenuBar();
        JMenu toolsMenu = new JMenu("Tools");
        
        JMenuItem signIn = new JMenuItem("Sign in to Copilot");
        signIn.addActionListener(e -> {
            copilot.signIn(this);
            if (copilot.isSignedIn()) {
                JOptionPane.showMessageDialog(this, "Signed in to Copilot!");
            }
        });
        
        JMenuItem build = new JMenuItem("Build Project");
        build.addActionListener(e -> buildProject());
        
        toolsMenu.add(signIn);
        toolsMenu.add(build);
        menuBar.add(toolsMenu);
        setJMenuBar(menuBar);
        
        // Editor
        editor = new JTextArea("public class Main {\n    public static void main(String[] args) {\n        \n    }\n}");
        editor.setFont(new Font("Monospaced", Font.PLAIN, 14));
        
        // Auto-completion on typing
        editor.addKeyListener(new KeyAdapter() {
            @Override
            public void keyReleased(KeyEvent e) {
                if (e.getKeyCode() == KeyEvent.VK_SPACE && copilot.isSignedIn()) {
                    String text = editor.getText();
                    int pos = editor.getCaretPosition();
                    String prompt = text.substring(Math.max(0, pos-100), pos);
                    
                    copilot.complete(prompt).thenAccept(completion -> {
                        SwingUtilities.invokeLater(() -> {
                            editor.insert(completion, pos);
                        });
                    });
                }
            }
        });
        
        // Output
        output = new JTextArea(10, 0);
        output.setEditable(false);
        output.setBackground(Color.BLACK);
        output.setForeground(Color.GREEN);
        
        // Layout
        JSplitPane split = new JSplitPane(JSplitPane.VERTICAL_SPLIT,
            new JScrollPane(editor),
            new JScrollPane(output));
        split.setDividerLocation(500);
        
        add(split, BorderLayout.CENTER);
    }
    
    private void startBuildServer() {
        new Thread(() -> {
            try {
                buildServer.start(8080);
            } catch (Exception e) {
                SwingUtilities.invokeLater(() -> 
                    output.append("Build server failed: " + e.getMessage() + "\n"));
            }
        }).start();
    }
    
    private void buildProject() {
        output.append("Building project via WebSocket...\n");
        
        // Simulate WebSocket build request
        CompletableFuture.runAsync(() -> {
            try {
                Thread.sleep(2000); // Simulate build time
                SwingUtilities.invokeLater(() -> {
                    output.append("Build completed successfully!\n");
                    output.append("Artifact: app-debug.apk\n");
                });
            } catch (InterruptedException e) {
                SwingUtilities.invokeLater(() -> 
                    output.append("Build interrupted\n"));
            }
        });
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new HybridIDE().setVisible(true));
    }
}

