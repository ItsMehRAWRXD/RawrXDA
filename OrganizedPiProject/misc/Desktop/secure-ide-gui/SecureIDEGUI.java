import javax.swing.*;
import javax.swing.tree.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;

public class SecureIDEGUI extends JFrame {
    private JTextArea codeEditor;
    private JTree fileTree;
    private DefaultTreeModel fileTreeModel;
    private JTextArea terminalOutput;
    private JTextField terminalInput;
    private JTextArea aiOutput;
    private JTextField aiInput;
    private JLabel statusLabel;
    
    private String workspace = System.getProperty("user.dir");
    private AIEngine aiEngine;
    private CustomSecurityManager securityManager;
    private FileManager fileManager;
    private TerminalManager terminalManager;
    private IntegratedASMCompiler compiler;
    
    public SecureIDEGUI() {
        initializeComponents();
        setupGUI();
        loadWorkspace();
    }
    
    private void initializeComponents() {
        this.aiEngine = new AIEngine();
        this.securityManager = new CustomSecurityManager();
        this.fileManager = new FileManager(workspace, securityManager);
        this.terminalManager = new TerminalManager(securityManager);
        this.compiler = new IntegratedASMCompiler();
    }
    
    private void setupGUI() {
        setTitle("Secure IDE v1.0.0");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1200, 800);
        
        try {
            // UIManager.setLookAndFeel(UIManager.getSystemLookAndFeel());
        } catch (Exception e) {
            // Use default look and feel
        }
        
        createMenuBar();
        createMainLayout();
        setLocationRelativeTo(null);
    }
    
    private void createMenuBar() {
        JMenuBar menuBar = new JMenuBar();
        
        JMenu fileMenu = new JMenu("File");
        fileMenu.add(new JMenuItem("New")).addActionListener(e -> newFile());
        fileMenu.add(new JMenuItem("Open")).addActionListener(e -> openFile());
        fileMenu.add(new JMenuItem("Save")).addActionListener(e -> saveFile());
        
        JMenu compileMenu = new JMenu("Compile");
        compileMenu.add(new JMenuItem("Compile ASM")).addActionListener(e -> compileAssembly());
        
        menuBar.add(fileMenu);
        menuBar.add(compileMenu);
        setJMenuBar(menuBar);
    }
    
    private void createMainLayout() {
        setLayout(new BorderLayout());
        
        // File tree
        fileTreeModel = new DefaultTreeModel(new DefaultMutableTreeNode("Workspace"));
        fileTree = new JTree(fileTreeModel);
        fileTree.addTreeSelectionListener(e -> openSelectedFile());
        
        // Code editor
        codeEditor = new JTextArea();
        codeEditor.setFont(new Font("Consolas", Font.PLAIN, 14));
        
        // Terminal
        terminalOutput = new JTextArea(10, 0);
        terminalOutput.setEditable(false);
        terminalInput = new JTextField();
        terminalInput.addActionListener(e -> executeCommand());
        
        // AI Assistant
        aiOutput = new JTextArea(10, 0);
        aiOutput.setEditable(false);
        aiInput = new JTextField();
        aiInput.addActionListener(e -> processAIQuery());
        
        // Status bar
        statusLabel = new JLabel("Ready");
        
        // Layout
        JSplitPane mainSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
            new JScrollPane(fileTree),
            new JScrollPane(codeEditor));
        
        JPanel terminalPanel = new JPanel(new BorderLayout());
        terminalPanel.add(new JScrollPane(terminalOutput), BorderLayout.CENTER);
        terminalPanel.add(terminalInput, BorderLayout.SOUTH);
        
        JPanel aiPanel = new JPanel(new BorderLayout());
        aiPanel.add(new JScrollPane(aiOutput), BorderLayout.CENTER);
        aiPanel.add(aiInput, BorderLayout.SOUTH);
        
        JTabbedPane bottomPane = new JTabbedPane();
        bottomPane.add("Terminal", terminalPanel);
        bottomPane.add("AI Assistant", aiPanel);
        
        JSplitPane verticalSplit = new JSplitPane(JSplitPane.VERTICAL_SPLIT,
            mainSplit, bottomPane);
        
        add(verticalSplit, BorderLayout.CENTER);
        add(statusLabel, BorderLayout.SOUTH);
    }
    
    private void loadWorkspace() {
        try {
            DefaultMutableTreeNode root = (DefaultMutableTreeNode) fileTreeModel.getRoot();
            root.removeAllChildren();
            loadDirectory(Paths.get(workspace), root);
            fileTreeModel.reload();
        } catch (IOException e) {
            showError("Failed to load workspace: " + e.getMessage());
        }
    }
    
    private void loadDirectory(Path dir, DefaultMutableTreeNode parent) throws IOException {
        try (var stream = Files.list(dir)) {
            stream.forEach(path -> {
                DefaultMutableTreeNode node = new DefaultMutableTreeNode(path.getFileName().toString());
                parent.add(node);
                if (Files.isDirectory(path)) {
                    try {
                        loadDirectory(path, node);
                    } catch (IOException e) {
                        // Skip directories that can't be read
                    }
                }
            });
        }
    }
    
    private void newFile() {
        codeEditor.setText("");
        statusLabel.setText("New file");
    }
    
    private void openFile() {
        JFileChooser chooser = new JFileChooser(workspace);
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            try {
                String content = fileManager.readFile(chooser.getSelectedFile().getName());
                codeEditor.setText(content);
                statusLabel.setText("Opened: " + chooser.getSelectedFile().getName());
            } catch (Exception e) {
                showError("Failed to open file: " + e.getMessage());
            }
        }
    }
    
    private void saveFile() {
        JFileChooser chooser = new JFileChooser(workspace);
        if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            try {
                fileManager.writeFile(chooser.getSelectedFile().getName(), codeEditor.getText());
                statusLabel.setText("Saved: " + chooser.getSelectedFile().getName());
            } catch (Exception e) {
                showError("Failed to save file: " + e.getMessage());
            }
        }
    }
    
    private void openSelectedFile() {
        DefaultMutableTreeNode selectedNode = (DefaultMutableTreeNode) fileTree.getLastSelectedPathComponent();
        if (selectedNode != null && selectedNode.isLeaf()) {
            try {
                String filename = selectedNode.toString();
                if (filename.length() > 1) {
                    String content = fileManager.readFile(filename);
                    codeEditor.setText(content);
                    statusLabel.setText("Opened: " + filename);
                } else {
                    statusLabel.setText("Invalid filename");
                }
            } catch (Exception e) {
                showError("Failed to open file: " + e.getMessage());
            }
        }
    }
    
    private void executeCommand() {
        String command = terminalInput.getText().trim();
        if (!command.isEmpty()) {
            terminalOutput.append("> " + command + "\n");
            
            if (securityManager.validateCommand(command)) {
                terminalOutput.append("Command executed safely\n");
            } else {
                terminalOutput.append("Command blocked by security manager\n");
            }
            
            terminalInput.setText("");
            terminalOutput.setCaretPosition(terminalOutput.getDocument().getLength());
        }
    }
    
    private void processAIQuery() {
        String query = aiInput.getText().trim();
        if (!query.isEmpty()) {
            aiOutput.append("You: " + query + "\n");
            String response = aiEngine.processQuery(query);
            aiOutput.append("AI: " + response + "\n\n");
            aiInput.setText("");
            aiOutput.setCaretPosition(aiOutput.getDocument().getLength());
        }
    }
    
    private void compileAssembly() {
        SwingWorker<IntegratedASMCompiler.CompilationResult, String> worker = 
            new SwingWorker<IntegratedASMCompiler.CompilationResult, String>() {
            
            @Override
            protected IntegratedASMCompiler.CompilationResult doInBackground() throws Exception {
                publish("Starting compilation...");
                return compiler.compile(codeEditor.getText());
            }
            
            @Override
            protected void process(java.util.List<String> chunks) {
                for (String message : chunks) {
                    terminalOutput.append(message + "\n");
                }
            }
            
            @Override
            protected void done() {
                try {
                    IntegratedASMCompiler.CompilationResult result = get();
                    if (result.isSuccess()) {
                        terminalOutput.append("Compilation successful!\n");
                        terminalOutput.append("Output: " + result.getOutput() + "\n");
                    } else {
                        terminalOutput.append("Compilation failed: " + result.getError() + "\n");
                    }
                } catch (Exception e) {
                    terminalOutput.append("Compilation error: " + e.getMessage() + "\n");
                }
                statusLabel.setText("Compilation complete");
            }
        };
        
        worker.execute();
        statusLabel.setText("Compiling...");
    }
    
    private void showError(String message) {
        JOptionPane.showMessageDialog(this, message, "Error", JOptionPane.ERROR_MESSAGE);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            new SecureIDEGUI().setVisible(true);
        });
    }
}

// Supporting classes
class AIEngine {
    public String processQuery(String query) {
        return "AI response to: " + query;
    }
}

class CustomSecurityManager {
    public boolean validateCommand(String command) {
        String[] blockedCommands = {"rm", "del", "format", "shutdown"};
        for (String blocked : blockedCommands) {
            if (command.toLowerCase().contains(blocked)) {
                return false;
            }
        }
        return true;
    }
}

class FileManager {
    private String workspace;
    private CustomSecurityManager securityManager;
    
    public FileManager(String workspace, CustomSecurityManager securityManager) {
        this.workspace = workspace;
        this.securityManager = securityManager;
    }
    
    public String readFile(String filename) throws IOException {
        Path path = Paths.get(workspace, filename);
        return Files.readString(path);
    }
    
    public void writeFile(String filename, String content) throws IOException {
        Path path = Paths.get(workspace, filename);
        Files.writeString(path, content);
    }
}

class TerminalManager {
    private CustomSecurityManager securityManager;
    
    public TerminalManager(CustomSecurityManager securityManager) {
        this.securityManager = securityManager;
    }
}

class IntegratedASMCompiler {
    public CompilationResult compile(String code) {
        try {
            // Simulate compilation
            Thread.sleep(1000);
            if (code.trim().isEmpty()) {
                return new CompilationResult(false, "", "No code to compile");
            }
            return new CompilationResult(true, "Binary output", "");
        } catch (InterruptedException e) {
            return new CompilationResult(false, "", "Compilation interrupted");
        }
    }
    
    public static class CompilationResult {
        private boolean success;
        private String output;
        private String error;
        
        public CompilationResult(boolean success, String output, String error) {
            this.success = success;
            this.output = output;
            this.error = error;
        }
        
        public boolean isSuccess() { return success; }
        public String getOutput() { return output; }
        public String getError() { return error; }
    }
}
            @Override
            protected void done() {
                try {
                    IntegratedASMCompiler.CompilationResult result = get();
                    terminalOutput.append("Compilation " + 
                        (result.success ? "successful" : "failed") + "\n");
                } catch (Exception e) {
                    terminalOutput.append("Compilation error: " + e.getMessage() + "\n");
                }
            }
        };
        
        worker.execute();
    }
    
    private void showError(String message) {
        JOptionPane.showMessageDialog(this, message, "Error", JOptionPane.ERROR_MESSAGE);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new SecureIDEGUI().setVisible(true));
    }
}

// Supporting classes
class AIEngine {
    public String processQuery(String query) {
        return "Local AI response to: " + query;
    }
}

class CustomSecurityManager {
    public boolean validateCommand(String command) {
        return !command.contains("rm") && !command.contains("del");
    }
}

class FileManager {
    private final String workspace;
    private final CustomSecurityManager security;
    
    public FileManager(String workspace, CustomSecurityManager security) {
        this.workspace = workspace;
        this.security = security;
    }
    
    public String readFile(String filename) throws IOException {
        if (filename == null || filename.trim().isEmpty()) {
            throw new IOException("Invalid filename");
        }
        Path filePath = Paths.get(workspace, filename);
        if (!Files.exists(filePath)) {
            throw new IOException("File not found: " + filename);
        }
        return Files.readString(filePath);
    }
    
    public void writeFile(String filename, String content) throws IOException {
        if (filename == null || filename.trim().isEmpty()) {
            throw new IOException("Invalid filename");
        }
        Files.writeString(Paths.get(workspace, filename), content);
    }
}

class TerminalManager {
    private final CustomSecurityManager security;
    
    public TerminalManager(CustomSecurityManager security) {
        this.security = security;
    }
}

class IntegratedASMCompiler {
    public CompilationResult compile(String code) {
        return new CompilationResult(true, "Compilation successful");
    }
    
    static class CompilationResult {
        final boolean success;
        final String message;
        
        CompilationResult(boolean success, String message) {
            this.success = success;
            this.message = message;
        }
    }
}