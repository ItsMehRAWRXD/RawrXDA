import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class MyIDE extends JFrame {
    private JTextArea editor;
    private JTree fileTree;
    private JTabbedPane tabs;
    
    public MyIDE() {
        setTitle("My IDE");
        setSize(1200, 800);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        
        // Create components
        editor = new JTextArea();
        editor.setFont(new Font("Consolas", Font.PLAIN, 14));
        
        fileTree = new JTree();
        tabs = new JTabbedPane();
        
        // Layout
        JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
            new JScrollPane(fileTree), 
            new JScrollPane(editor));
        split.setDividerLocation(250);
        
        add(split);
        
        // Menu
        JMenuBar menu = new JMenuBar();
        JMenu file = new JMenu("File");
        file.add(new JMenuItem("New"));
        file.add(new JMenuItem("Open"));
        file.add(new JMenuItem("Save"));
        menu.add(file);
        setJMenuBar(menu);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            new MyIDE().setVisible(true);
        });
    }
}