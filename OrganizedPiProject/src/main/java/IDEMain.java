import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.File;

public class IDEMain extends JFrame {
    private JTextArea editor;
    private JToolBar toolBar;
    private JLabel statusBar;
    private File currentFile;
    private boolean isDirty = false;
    private AgentLLM agentLLM;
    private JSlider transparencySlider;
    private JSlider colorSlider;
    
    public IDEMain() {
        setTitle("Advanced Agentic IDE - Enterprise Edition");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1200, 800);
        
        editor = new JTextArea();
        editor.setFont(new Font("Consolas", Font.PLAIN, 14));
        editor.getDocument().addDocumentListener(new javax.swing.event.DocumentListener() {
            public void insertUpdate(javax.swing.event.DocumentEvent e) { isDirty = true; }
            public void removeUpdate(javax.swing.event.DocumentEvent e) { isDirty = true; }
            public void changedUpdate(javax.swing.event.DocumentEvent e) { isDirty = true; }
        });
        toolBar = createToolBar();
        statusBar = new JLabel(" Ready");
        statusBar.setBorder(BorderFactory.createLoweredBevelBorder());
        
        add(toolBar, BorderLayout.NORTH);
        add(new JScrollPane(editor), BorderLayout.CENTER);
        add(statusBar, BorderLayout.SOUTH);
        
        setupMenuBar();
        setLocationRelativeTo(null);

        // TODO: Replace "YOUR_API_KEY" with your actual API key
        this.agentLLM = new AgentLLM("YOUR_API_KEY");

        // Add sliders to the toolbar
        transparencySlider = new JSlider(0, 100, 100);
        transparencySlider.setToolTipText("Adjust Transparency");
        transparencySlider.addChangeListener(e -> setTransparency(transparencySlider.getValue()));
        toolBar.add(new JLabel("Transparency:"));
        toolBar.add(transparencySlider);

        colorSlider = new JSlider(0, 255, 255);
        colorSlider.setToolTipText("Adjust Background Color");
        colorSlider.addChangeListener(e -> setBackgroundColor(colorSlider.getValue()));
        toolBar.add(new JLabel("Background Color:"));
        toolBar.add(colorSlider);
    }
    
    private JToolBar createToolBar() {
        JToolBar tb = new JToolBar();
        tb.add(createToolButton("New", "ctrl N", e -> newFile()));
        tb.add(createToolButton("Open", "ctrl O", e -> openFile()));
        tb.add(createToolButton("Save", "ctrl S", e -> saveFile()));
        tb.addSeparator();
        tb.add(createToolButton("Undo", "ctrl Z", e -> {}));
        tb.add(createToolButton("Redo", "ctrl Y", e -> {}));
        return tb;
    }
    
    private JButton createToolButton(String text, String shortcut, ActionListener action) {
        JButton btn = new JButton(text);
        btn.setToolTipText(text + " (" + shortcut + ")");
        btn.addActionListener(action);
        return btn;
    }
    
    private void setupMenuBar() {
        JMenuBar menuBar = new JMenuBar();
        
        // File Menu
        JMenu fileMenu = new JMenu("File");
        fileMenu.add(createMenuItem("New", "ctrl N", e -> newFile()));
        fileMenu.add(createMenuItem("Open", "ctrl O", e -> openFile()));
        fileMenu.addSeparator();
        fileMenu.add(createMenuItem("Save", "ctrl S", e -> saveFile()));
        fileMenu.add(createMenuItem("Save As", "ctrl shift S", e -> saveAsFile()));
        fileMenu.addSeparator();
        fileMenu.add(createMenuItem("Exit", "ctrl Q", e -> System.exit(0)));
        
        // Edit Menu
        JMenu editMenu = new JMenu("Edit");
        editMenu.add(createMenuItem("Undo", "ctrl Z", e -> {}));
        editMenu.add(createMenuItem("Redo", "ctrl Y", e -> {}));
        editMenu.addSeparator();
        editMenu.add(createMenuItem("Cut", "ctrl X", e -> {}));
        editMenu.add(createMenuItem("Copy", "ctrl C", e -> {}));
        editMenu.add(createMenuItem("Paste", "ctrl V", e -> {}));
        
        // View Menu
        JMenu viewMenu = new JMenu("View");
        viewMenu.add(createMenuItem("Toggle Minimap", "F9", e -> {}));
        viewMenu.add(createMenuItem("Toggle Line Numbers", "F10", e -> {}));
        
        // Bookmarks Menu
        JMenu bookmarkMenu = new JMenu("Bookmarks");
        bookmarkMenu.add(createMenuItem("Toggle Bookmark", "ctrl B", e -> toggleBookmark()));
        bookmarkMenu.add(createMenuItem("Next Bookmark", "F2", e -> gotoNextBookmark()));
        bookmarkMenu.add(createMenuItem("Previous Bookmark", "shift F2", e -> gotoPreviousBookmark()));
        
        // Language Menu
        JMenu langMenu = new JMenu("Language");
        langMenu.add(createMenuItem("Java", null, e -> setLanguage("java")));
        langMenu.add(createMenuItem("JavaScript", null, e -> setLanguage("javascript")));
        langMenu.add(createMenuItem("Python", null, e -> setLanguage("python")));
        
        // AI Menu
        JMenu aiMenu = new JMenu("AI");
        aiMenu.add(createMenuItem("Complete Code", "ctrl SPACE", e -> completeCode()));

        menuBar.add(fileMenu);
        menuBar.add(editMenu);
        menuBar.add(viewMenu);
        menuBar.add(bookmarkMenu);
        menuBar.add(langMenu);
        menuBar.add(aiMenu);
        setJMenuBar(menuBar);
    }
    
    private JMenuItem createMenuItem(String text, String shortcut, ActionListener action) {
        JMenuItem item = new JMenuItem(text);
        if (shortcut != null) {
            item.setAccelerator(KeyStroke.getKeyStroke(shortcut));
        }
        item.addActionListener(action);
        return item;
    }
    
    private void newFile() {
        if (isDirty()) {
            int result = JOptionPane.showConfirmDialog(this, "Save current file?");
            if (result == JOptionPane.YES_OPTION) saveFile();
            else if (result == JOptionPane.CANCEL_OPTION) return;
        }
        editor.setText("");
        currentFile = null;
        statusBar.setText(" New File");
    }
    
    private void openFile() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            File file = chooser.getSelectedFile();
            openFile(file);
            statusBar.setText(" Opened: " + file.getName());
        }
    }
    
    private void saveFile() {
        if (currentFile == null) {
            JFileChooser chooser = new JFileChooser();
            if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
                currentFile = chooser.getSelectedFile();
            } else return;
        }
        
        try {
            java.nio.file.Files.writeString(currentFile.toPath(), editor.getText());
            setTitle("Advanced Agentic IDE - " + currentFile.getName());
            isDirty = false;
            statusBar.setText(" Saved");
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Error saving file: " + e.getMessage());
        }
    }
    
    private void saveAsFile() {
        JFileChooser chooser = new JFileChooser();
        if (currentFile != null) {
            chooser.setSelectedFile(currentFile);
        }
        if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            currentFile = chooser.getSelectedFile();
            try {
                java.nio.file.Files.writeString(currentFile.toPath(), editor.getText());
                setTitle("Advanced Agentic IDE - " + currentFile.getName());
                isDirty = false;
                statusBar.setText(" Saved As");
            } catch (Exception e) {
                JOptionPane.showMessageDialog(this, "Error saving file: " + e.getMessage());
            }
        }
    }
    
    private boolean isDirty() {
        return isDirty;
    }
    
    private void toggleBookmark() {
        // Basic bookmark implementation for JTextArea
        int line = editor.getCaretPosition();
        // For now, just show a message - full implementation would require more complex bookmark management
        JOptionPane.showMessageDialog(this, "Bookmark functionality not yet implemented for basic editor");
    }
    
    private void gotoNextBookmark() {
        JOptionPane.showMessageDialog(this, "Next bookmark functionality not yet implemented for basic editor");
    }
    
    private void gotoPreviousBookmark() {
        JOptionPane.showMessageDialog(this, "Previous bookmark functionality not yet implemented for basic editor");
    }

    private void completeCode() {
        String currentText = editor.getText();
        int caretPosition = editor.getCaretPosition();
        String prompt = currentText; // Using full text as context

        statusBar.setText("AI is thinking...");
        agentLLM.complete(prompt).thenAccept(completion -> {
            SwingUtilities.invokeLater(() -> {
                try {
                    editor.getDocument().insertString(caretPosition, completion, null);
                    statusBar.setText("AI completion inserted.");
                } catch (Exception e) {
                    statusBar.setText("Failed to insert AI completion.");
                }
            });
        }).exceptionally(ex -> {
            SwingUtilities.invokeLater(() -> {
                statusBar.setText("AI completion failed: " + ex.getMessage());
            });
            return null;
        });
    }
    
    private void openFile(File file) {
        if (file == null) {
            editor.setText("");
            currentFile = null;
            return;
        }
        try {
            String content = java.nio.file.Files.readString(file.toPath());
            editor.setText(content);
            currentFile = file;
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Error: " + e.getMessage());
        }
    }
    
    private void setLanguage(String language) {}
    
    private void setTransparency(int value) {
        float opacity = value / 100f;
        setOpacity(opacity);
    }

    private void setBackgroundColor(int value) {
        Color backgroundColor = new Color(value, value, value);
        editor.setBackground(backgroundColor);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                // UIManager.setLookAndFeel(UIManager.getSystemLookAndFeel());
            } catch (Exception e) {}
            new IDEMain().setVisible(true);
        });
    }
}