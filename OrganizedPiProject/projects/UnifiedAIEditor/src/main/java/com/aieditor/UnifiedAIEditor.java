import javax.swing.*;
import javax.swing.border.EmptyBorder;
import javax.swing.tree.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;
import java.util.regex.*;

/**
 * Unified AI-First Editor - Combines all best features from existing IDEs
 * Features: Multi-AI integration, syntax highlighting, marketplace, real-time collaboration
 */
public class UnifiedAIEditor extends JFrame {
    // Core components
    private final AIOrchestrator aiOrchestrator = new AIOrchestrator();
    private final MarketplaceManager marketplace = new MarketplaceManager();
    private final SyntaxHighlighter syntaxHighlighter = new SyntaxHighlighter();
    private final ProjectManager projectManager = new ProjectManager();
    private final BuildSystem buildSystem = new BuildSystem();
    
    // UI Components
    private JTabbedPane mainTabs;
    private CodeEditor codeEditor;
    private AIPanel aiPanel;
    private FileExplorer fileExplorer;
    private OutputPanel outputPanel;
    private MarketplacePanel marketplacePanel;
    private TerminalPanel terminalPanel;
    
    // Layout managers
    private JSplitPane mainSplit, editorSplit, rightSplit;
    
    public UnifiedAIEditor() {
        initializeUI();
        setupEventHandlers();
        startBackgroundServices();
    }
    
    private void initializeUI() {
        setTitle("Unified AI Editor - Next Generation IDE");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setSize(1600, 1000);
        setLocationRelativeTo(null);
        
        // Set modern look
        try {
            UIManager.setLookAndFeel(UIManager.getSystemLookAndFeel());
        } catch (Exception e) {
            // Fallback to default
        }
        
        createMenuBar();
        createMainLayout();
        createStatusBar();
        
        // Apply dark theme
        applyDarkTheme();
    }
    
    private void createMenuBar() {
        JMenuBar menuBar = new JMenuBar();
        
        // File Menu
        JMenu fileMenu = new JMenu("File");
        fileMenu.add(createMenuItem("New File", "Ctrl+N", e -> codeEditor.newFile()));
        fileMenu.add(createMenuItem("Open File", "Ctrl+O", e -> codeEditor.openFile()));
        fileMenu.add(createMenuItem("Save", "Ctrl+S", e -> codeEditor.save()));
        fileMenu.addSeparator();
        fileMenu.add(createMenuItem("New Project", "Ctrl+Shift+N", e -> projectManager.createProject()));
        fileMenu.add(createMenuItem("Open Project", "Ctrl+Shift+O", e -> projectManager.openProject()));
        
        // AI Menu
        JMenu aiMenu = new JMenu("AI");
        aiMenu.add(createMenuItem("Chat with AI", "Ctrl+/", e -> aiPanel.focusInput()));
        aiMenu.add(createMenuItem("Generate Code", "Ctrl+G", e -> aiOrchestrator.generateCode()));
        aiMenu.add(createMenuItem("Explain Code", "Ctrl+E", e -> aiOrchestrator.explainCode()));
        aiMenu.add(createMenuItem("Review Code", "Ctrl+R", e -> aiOrchestrator.reviewCode()));
        aiMenu.addSeparator();
        aiMenu.add(createMenuItem("AI Settings", null, e -> showAISettings()));
        
        // View Menu
        JMenu viewMenu = new JMenu("View");
        viewMenu.add(createCheckboxMenuItem("File Explorer", true, e -> toggleFileExplorer()));
        viewMenu.add(createCheckboxMenuItem("AI Panel", true, e -> toggleAIPanel()));
        viewMenu.add(createCheckboxMenuItem("Output Panel", true, e -> toggleOutputPanel()));
        viewMenu.add(createCheckboxMenuItem("Terminal", false, e -> toggleTerminal()));
        
        // Tools Menu
        JMenu toolsMenu = new JMenu("Tools");
        toolsMenu.add(createMenuItem("Build", "F9", e -> buildSystem.build()));
        toolsMenu.add(createMenuItem("Run", "F10", e -> buildSystem.run()));
        toolsMenu.add(createMenuItem("Debug", "F11", e -> buildSystem.debug()));
        toolsMenu.addSeparator();
        toolsMenu.add(createMenuItem("Marketplace", "Ctrl+M", e -> showMarketplace()));
        
        menuBar.add(fileMenu);
        menuBar.add(aiMenu);
        menuBar.add(viewMenu);
        menuBar.add(toolsMenu);
        
        setJMenuBar(menuBar);
    }
    
    private void createMainLayout() {
        // Create main components
        fileExplorer = new FileExplorer(projectManager);
        codeEditor = new CodeEditor(syntaxHighlighter, aiOrchestrator);
        aiPanel = new AIPanel(aiOrchestrator);
        outputPanel = new OutputPanel();
        terminalPanel = new TerminalPanel();
        marketplacePanel = new MarketplacePanel(marketplace);
        
        // Create tabbed interface
        mainTabs = new JTabbedPane();
        mainTabs.addTab("Editor", createEditorPanel());
        mainTabs.addTab("Marketplace", marketplacePanel);
        mainTabs.addTab("Settings", createSettingsPanel());
        
        add(mainTabs, BorderLayout.CENTER);
    }
    
    private JPanel createEditorPanel() {
        JPanel panel = new JPanel(new BorderLayout());
        
        // Left panel (File Explorer)
        JPanel leftPanel = new JPanel(new BorderLayout());
        leftPanel.add(new JLabel("Explorer", JLabel.CENTER), BorderLayout.NORTH);
        leftPanel.add(fileExplorer, BorderLayout.CENTER);
        leftPanel.setPreferredSize(new Dimension(250, 0));
        
        // Center panel (Code Editor)
        JPanel centerPanel = new JPanel(new BorderLayout());
        centerPanel.add(codeEditor, BorderLayout.CENTER);
        
        // Right panel (AI)
        JPanel rightPanel = new JPanel(new BorderLayout());
        rightPanel.add(new JLabel("AI Assistant", JLabel.CENTER), BorderLayout.NORTH);
        rightPanel.add(aiPanel, BorderLayout.CENTER);
        rightPanel.setPreferredSize(new Dimension(300, 0));
        
        // Bottom panel (Output/Terminal)
        JTabbedPane bottomTabs = new JTabbedPane();
        bottomTabs.addTab("Output", outputPanel);
        bottomTabs.addTab("Terminal", terminalPanel);
        bottomTabs.setPreferredSize(new Dimension(0, 200));
        
        // Arrange with split panes
        mainSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, leftPanel, centerPanel);
        rightSplit = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, mainSplit, rightPanel);
        editorSplit = new JSplitPane(JSplitPane.VERTICAL_SPLIT, rightSplit, bottomTabs);
        
        mainSplit.setDividerLocation(250);
        rightSplit.setDividerLocation(1000);
        editorSplit.setDividerLocation(600);
        
        panel.add(editorSplit, BorderLayout.CENTER);
        return panel;
    }
    
    private JPanel createSettingsPanel() {
        JPanel panel = new JPanel(new BorderLayout());
        panel.add(new JLabel("Settings Panel - Coming Soon", JLabel.CENTER), BorderLayout.CENTER);
        return panel;
    }
    
    private void createStatusBar() {
        JPanel statusBar = new JPanel(new FlowLayout(FlowLayout.LEFT));
        statusBar.setBorder(new EmptyBorder(2, 10, 2, 10));
        
        JLabel statusLabel = new JLabel("Ready");
        JLabel positionLabel = new JLabel("Line 1, Col 1");
        JLabel modeLabel = new JLabel("AI Mode: Active");
        
        statusBar.add(statusLabel);
        statusBar.add(Box.createHorizontalStrut(20));
        statusBar.add(positionLabel);
        statusBar.add(Box.createHorizontalStrut(20));
        statusBar.add(modeLabel);
        
        add(statusBar, BorderLayout.SOUTH);
    }
    
    private void applyDarkTheme() {
        Color darkBg = new Color(0x1e1e1e);
        Color darkText = new Color(0xd4d4d4);
        Color accentColor = new Color(0x007acc);
        
        // Apply theme to main components
        getContentPane().setBackground(darkBg);
        mainTabs.setBackground(darkBg);
        mainTabs.setForeground(darkText);
    }
    
    private void setupEventHandlers() {
        // Window events
        addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent e) {
                if (codeEditor.hasUnsavedChanges()) {
                    int result = JOptionPane.showConfirmDialog(
                        UnifiedAIEditor.this,
                        "You have unsaved changes. Save before exiting?",
                        "Unsaved Changes",
                        JOptionPane.YES_NO_CANCEL_OPTION
                    );
                    
                    if (result == JOptionPane.CANCEL_OPTION) {
                        setDefaultCloseOperation(DO_NOTHING_ON_CLOSE);
                        return;
                    } else if (result == JOptionPane.YES_OPTION) {
                        codeEditor.save();
                    }
                }
                System.exit(0);
            }
        });
        
        // Global key bindings
        setupKeyBindings();
    }
    
    private void setupKeyBindings() {
        JRootPane rootPane = getRootPane();
        
        // Ctrl+/ for AI chat
        rootPane.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
            .put(KeyStroke.getKeyStroke("control SLASH"), "aiChat");
        rootPane.getActionMap().put("aiChat", new AbstractAction() {
            @Override
            public void actionPerformed(ActionEvent e) {
                aiPanel.focusInput();
            }
        });
    }
    
    private void startBackgroundServices() {
        // Start AI orchestrator
        aiOrchestrator.initialize();
        
        // Start marketplace sync
        new Thread(() -> marketplace.syncExtensions()).start();
        
        // Start auto-save
        Timer autoSaveTimer = new Timer(30000, e -> codeEditor.autoSave());
        autoSaveTimer.start();
    }
    
    // Utility methods
    private JMenuItem createMenuItem(String text, String accelerator, ActionListener action) {
        JMenuItem item = new JMenuItem(text);
        if (accelerator != null) {
            item.setAccelerator(KeyStroke.getKeyStroke(accelerator.replace("Ctrl+", "control ")));
        }
        item.addActionListener(action);
        return item;
    }
    
    private JCheckBoxMenuItem createCheckboxMenuItem(String text, boolean selected, ActionListener action) {
        JCheckBoxMenuItem item = new JCheckBoxMenuItem(text, selected);
        item.addActionListener(action);
        return item;
    }
    
    // Toggle methods
    private void toggleFileExplorer() {
        fileExplorer.setVisible(!fileExplorer.isVisible());
        mainSplit.revalidate();
    }
    
    private void toggleAIPanel() {
        aiPanel.setVisible(!aiPanel.isVisible());
        rightSplit.revalidate();
    }
    
    private void toggleOutputPanel() {
        outputPanel.setVisible(!outputPanel.isVisible());
        editorSplit.revalidate();
    }
    
    private void toggleTerminal() {
        terminalPanel.setVisible(!terminalPanel.isVisible());
        editorSplit.revalidate();
    }
    
    private void showAISettings() {
        AISettingsDialog dialog = new AISettingsDialog(this, aiOrchestrator);
        dialog.setVisible(true);
    }
    
    private void showMarketplace() {
        mainTabs.setSelectedComponent(marketplacePanel);
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            try {
                // Enable high DPI support
                System.setProperty("sun.java2d.uiScale", "1.0");
                
                UnifiedAIEditor editor = new UnifiedAIEditor();
                editor.setVisible(true);
                
                // Show welcome message
                SwingUtilities.invokeLater(() -> {
                    JOptionPane.showMessageDialog(editor, 
                        "Welcome to Unified AI Editor!\n\n" +
                        "Features:\n" +
                        "• Multi-AI integration (OpenAI, Claude, Copilot, Amazon Q)\n" +
                        "• Advanced syntax highlighting\n" +
                        "• Extension marketplace\n" +
                        "• Real-time collaboration\n" +
                        "• Smart build system\n\n" +
                        "Press Ctrl+/ to start AI chat!",
                        "Welcome",
                        JOptionPane.INFORMATION_MESSAGE
                    );
                });
                
            } catch (Exception e) {
                JOptionPane.showMessageDialog(null, 
                    "Failed to start editor: " + e.getMessage(),
                    "Error", 
                    JOptionPane.ERROR_MESSAGE);
                e.printStackTrace();
            }
        });
    }
}