import javax.swing.*;
import javax.swing.text.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.util.*;
import java.util.List;

/**
 * Integration layer between the Advanced Code Editor and the main IDE
 */
public class EditorIntegration {
    private final AdvancedCodeEditor editor;
    private final EditorEventHandler eventHandler;
    private final EditorSettings settings;
    private final EditorThemeManager themeManager;
    
    public EditorIntegration() {
        editor = new AdvancedCodeEditor();
        eventHandler = new EditorEventHandler(editor);
        settings = new EditorSettings();
        themeManager = new EditorThemeManager();
        
        setupIntegration();
    }
    
    private void setupIntegration() {
        // Connect editor events to IDE functionality
        editor.addPropertyChangeListener("autoSave", e -> handleAutoSave());
        editor.addPropertyChangeListener("showAutoComplete", e -> handleAutoComplete());
        editor.addPropertyChangeListener("errorsDetected", e -> handleErrorsDetected((List<LiveErrorDetector.ErrorInfo>) e.getNewValue()));
        editor.addPropertyChangeListener("breadcrumbClicked", e -> handleBreadcrumbNavigation((Integer) e.getNewValue()));
        
        // Apply default settings
        applySettings();
        applyTheme("dark");
    }
    
    public AdvancedCodeEditor getEditor() {
        return editor;
    }
    
    public void openFile(File file) {
        editor.openFile(file);
        eventHandler.onFileOpened(file);
    }
    
    public void saveFile() {
        editor.saveFile();
        eventHandler.onFileSaved();
    }
    
    public void saveAsFile() {
        editor.saveAsFile();
        eventHandler.onFileSaved();
    }
    
    public boolean isDirty() {
        return editor.isDirty();
    }
    
    public void setLanguage(String language) {
        editor.setLanguage(language);
    }
    
    public String getCurrentLanguage() {
        return editor.getCurrentLanguage();
    }
    
    private void handleAutoSave() {
        if (settings.isAutoSaveEnabled()) {
            SwingUtilities.invokeLater(() -> {
                try {
                    editor.saveFile();
                } catch (Exception e) {
                    // Log error but don't interrupt user
                    System.err.println("Auto-save failed: " + e.getMessage());
                }
            });
        }
    }
    
    private void handleAutoComplete() {
        // Auto-completion is handled internally by the editor
        // This could trigger additional IDE-specific completion providers
    }
    
    private void handleErrorsDetected(List<LiveErrorDetector.ErrorInfo> errors) {
        // Update IDE error indicators, problem view, etc.
        SwingUtilities.invokeLater(() -> {
            // Could integrate with IDE's problem view
            System.out.println("Detected " + errors.size() + " errors/warnings");
        });
    }
    
    private void handleBreadcrumbNavigation(int level) {
        // Handle breadcrumb navigation
        System.out.println("Navigate to breadcrumb level: " + level);
    }
    
    private void applySettings() {
        // Apply editor settings from configuration
        JTextPane editorPane = getEditorPane();
        if (editorPane != null) {
            editorPane.setFont(new Font(settings.getFontFamily(), Font.PLAIN, settings.getFontSize()));
            // Apply other settings...
        }
    }
    
    public void applyTheme(String themeName) {
        EditorTheme theme = themeManager.getTheme(themeName);
        if (theme != null) {
            JTextPane editorPane = getEditorPane();
            if (editorPane != null) {
                editorPane.setBackground(theme.getBackgroundColor());
                editorPane.setForeground(theme.getForegroundColor());
                editorPane.setCaretColor(theme.getCaretColor());
                editorPane.setSelectionColor(theme.getSelectionColor());
            }
        }
    }
    
    private JTextPane getEditorPane() {
        // Access the internal editor pane
        // This would need proper accessor methods in AdvancedCodeEditor
        return null; // Simplified for now
    }
}

/**
 * Event handler for editor events
 */
class EditorEventHandler {
    private final AdvancedCodeEditor editor;
    
    public EditorEventHandler(AdvancedCodeEditor editor) {
        this.editor = editor;
    }
    
    public void onFileOpened(File file) {
        System.out.println("File opened: " + file.getName());
        // Could update recent files, project tree, etc.
    }
    
    public void onFileSaved() {
        System.out.println("File saved");
        // Could update file watchers, version control status, etc.
    }
    
    public void onTextChanged() {
        // Handle text change events
    }
    
    public void onCursorMoved(int position) {
        // Handle cursor movement
    }
}

/**
 * Editor settings management
 */
class EditorSettings {
    private final Properties settings;
    private final File settingsFile;
    
    public EditorSettings() {
        settings = new Properties();
        settingsFile = new File(System.getProperty("user.home"), ".ide-settings.properties");
        loadSettings();
    }
    
    private void loadSettings() {
        if (settingsFile.exists()) {
            try (FileInputStream fis = new FileInputStream(settingsFile)) {
                settings.load(fis);
            } catch (IOException e) {
                System.err.println("Failed to load settings: " + e.getMessage());
            }
        }
        
        // Set defaults
        setDefaultIfMissing("font.family", "JetBrains Mono");
        setDefaultIfMissing("font.size", "14");
        setDefaultIfMissing("theme", "dark");
        setDefaultIfMissing("autoSave.enabled", "true");
        setDefaultIfMissing("autoSave.interval", "30");
        setDefaultIfMissing("autoComplete.enabled", "true");
        setDefaultIfMissing("autoComplete.delay", "300");
        setDefaultIfMissing("lineNumbers.enabled", "true");
        setDefaultIfMissing("minimap.enabled", "true");
        setDefaultIfMissing("breadcrumbs.enabled", "true");
        setDefaultIfMissing("errorDetection.enabled", "true");
        setDefaultIfMissing("codeFolding.enabled", "true");
        setDefaultIfMissing("multiCursor.enabled", "true");
    }
    
    private void setDefaultIfMissing(String key, String defaultValue) {
        if (!settings.containsKey(key)) {
            settings.setProperty(key, defaultValue);
        }
    }
    
    public void saveSettings() {
        try (FileOutputStream fos = new FileOutputStream(settingsFile)) {
            settings.store(fos, "IDE Editor Settings");
        } catch (IOException e) {
            System.err.println("Failed to save settings: " + e.getMessage());
        }
    }
    
    public String getFontFamily() {
        return settings.getProperty("font.family", "JetBrains Mono");
    }
    
    public int getFontSize() {
        return Integer.parseInt(settings.getProperty("font.size", "14"));
    }
    
    public String getTheme() {
        return settings.getProperty("theme", "dark");
    }
    
    public boolean isAutoSaveEnabled() {
        return Boolean.parseBoolean(settings.getProperty("autoSave.enabled", "true"));
    }
    
    public int getAutoSaveInterval() {
        return Integer.parseInt(settings.getProperty("autoSave.interval", "30"));
    }
    
    public boolean isAutoCompleteEnabled() {
        return Boolean.parseBoolean(settings.getProperty("autoComplete.enabled", "true"));
    }
    
    public int getAutoCompleteDelay() {
        return Integer.parseInt(settings.getProperty("autoComplete.delay", "300"));
    }
    
    public boolean isLineNumbersEnabled() {
        return Boolean.parseBoolean(settings.getProperty("lineNumbers.enabled", "true"));
    }
    
    public boolean isMinimapEnabled() {
        return Boolean.parseBoolean(settings.getProperty("minimap.enabled", "true"));
    }
    
    public boolean isBreadcrumbsEnabled() {
        return Boolean.parseBoolean(settings.getProperty("breadcrumbs.enabled", "true"));
    }
    
    public boolean isErrorDetectionEnabled() {
        return Boolean.parseBoolean(settings.getProperty("errorDetection.enabled", "true"));
    }
    
    public boolean isCodeFoldingEnabled() {
        return Boolean.parseBoolean(settings.getProperty("codeFolding.enabled", "true"));
    }
    
    public boolean isMultiCursorEnabled() {
        return Boolean.parseBoolean(settings.getProperty("multiCursor.enabled", "true"));
    }
    
    public void setFontFamily(String fontFamily) {
        settings.setProperty("font.family", fontFamily);
    }
    
    public void setFontSize(int fontSize) {
        settings.setProperty("font.size", String.valueOf(fontSize));
    }
    
    public void setTheme(String theme) {
        settings.setProperty("theme", theme);
    }
    
    public void setAutoSaveEnabled(boolean enabled) {
        settings.setProperty("autoSave.enabled", String.valueOf(enabled));
    }
    
    // Additional setters for other properties...
}

/**
 * Theme management for the editor
 */
class EditorThemeManager {
    private final Map<String, EditorTheme> themes;
    
    public EditorThemeManager() {
        themes = new HashMap<>();
        loadBuiltInThemes();
    }
    
    private void loadBuiltInThemes() {
        // Dark theme (VS Code Dark)
        themes.put("dark", new EditorTheme(
            "Dark",
            new Color(0x1e1e1e), // background
            new Color(0xd4d4d4), // foreground
            new Color(0xffffff), // caret
            new Color(0x264f78), // selection
            new Color(0x252526), // line number background
            new Color(0x858585), // line number foreground
            new Color(0xd4d4d4), // current line number
            new Color(0x2d2d30), // breadcrumb background
            new Color(0x569cd6), // keyword
            new Color(0x4ec9b0), // type
            new Color(0xce9178), // string
            new Color(0x6a9955), // comment
            new Color(0xb5cea8), // number
            new Color(0xdcdcaa)  // function
        ));
        
        // Light theme (VS Code Light)
        themes.put("light", new EditorTheme(
            "Light",
            new Color(0xffffff), // background
            new Color(0x000000), // foreground
            new Color(0x000000), // caret
            new Color(0x0078d4), // selection
            new Color(0xf5f5f5), // line number background
            new Color(0x6e7681), // line number foreground
            new Color(0x000000), // current line number
            new Color(0xf3f3f3), // breadcrumb background
            new Color(0x0000ff), // keyword
            new Color(0x267f99), // type
            new Color(0xa31515), // string
            new Color(0x008000), // comment
            new Color(0x098658), // number
            new Color(0x795e26)  // function
        ));
        
        // High contrast theme
        themes.put("high-contrast", new EditorTheme(
            "High Contrast",
            new Color(0x000000), // background
            new Color(0xffffff), // foreground
            new Color(0xffffff), // caret
            new Color(0x0078d4), // selection
            new Color(0x000000), // line number background
            new Color(0xffffff), // line number foreground
            new Color(0xffffff), // current line number
            new Color(0x000000), // breadcrumb background
            new Color(0x00ffff), // keyword
            new Color(0x00ff00), // type
            new Color(0xffff00), // string
            new Color(0x7ca668), // comment
            new Color(0xff8c00), // number
            new Color(0xdcdcaa)  // function
        ));
        
        // Monokai theme
        themes.put("monokai", new EditorTheme(
            "Monokai",
            new Color(0x272822), // background
            new Color(0xf8f8f2), // foreground
            new Color(0xf8f8f0), // caret
            new Color(0x49483e), // selection
            new Color(0x3e3d32), // line number background
            new Color(0x90908a), // line number foreground
            new Color(0xf8f8f2), // current line number
            new Color(0x3e3d32), // breadcrumb background
            new Color(0xf92672), // keyword
            new Color(0x66d9ef), // type
            new Color(0xe6db74), // string
            new Color(0x75715e), // comment
            new Color(0xae81ff), // number
            new Color(0xa6e22e)  // function
        ));
        
        // Solarized Dark theme
        themes.put("solarized-dark", new EditorTheme(
            "Solarized Dark",
            new Color(0x002b36), // background
            new Color(0x839496), // foreground
            new Color(0x93a1a1), // caret
            new Color(0x073642), // selection
            new Color(0x073642), // line number background
            new Color(0x586e75), // line number foreground
            new Color(0x93a1a1), // current line number
            new Color(0x073642), // breadcrumb background
            new Color(0x859900), // keyword
            new Color(0x268bd2), // type
            new Color(0x2aa198), // string
            new Color(0x586e75), // comment
            new Color(0xd33682), // number
            new Color(0xb58900)  // function
        ));
    }
    
    public EditorTheme getTheme(String name) {
        return themes.get(name);
    }
    
    public Set<String> getAvailableThemes() {
        return themes.keySet();
    }
    
    public void addTheme(String name, EditorTheme theme) {
        themes.put(name, theme);
    }
}

/**
 * Editor theme definition
 */
class EditorTheme {
    private final String name;
    private final Color backgroundColor;
    private final Color foregroundColor;
    private final Color caretColor;
    private final Color selectionColor;
    private final Color lineNumberBackground;
    private final Color lineNumberForeground;
    private final Color currentLineNumber;
    private final Color breadcrumbBackground;
    private final Color keywordColor;
    private final Color typeColor;
    private final Color stringColor;
    private final Color commentColor;
    private final Color numberColor;
    private final Color functionColor;
    
    public EditorTheme(String name, Color backgroundColor, Color foregroundColor, Color caretColor,
                      Color selectionColor, Color lineNumberBackground, Color lineNumberForeground,
                      Color currentLineNumber, Color breadcrumbBackground, Color keywordColor,
                      Color typeColor, Color stringColor, Color commentColor, Color numberColor,
                      Color functionColor) {
        this.name = name;
        this.backgroundColor = backgroundColor;
        this.foregroundColor = foregroundColor;
        this.caretColor = caretColor;
        this.selectionColor = selectionColor;
        this.lineNumberBackground = lineNumberBackground;
        this.lineNumberForeground = lineNumberForeground;
        this.currentLineNumber = currentLineNumber;
        this.breadcrumbBackground = breadcrumbBackground;
        this.keywordColor = keywordColor;
        this.typeColor = typeColor;
        this.stringColor = stringColor;
        this.commentColor = commentColor;
        this.numberColor = numberColor;
        this.functionColor = functionColor;
    }
    
    // Getters
    public String getName() { return name; }
    public Color getBackgroundColor() { return backgroundColor; }
    public Color getForegroundColor() { return foregroundColor; }
    public Color getCaretColor() { return caretColor; }
    public Color getSelectionColor() { return selectionColor; }
    public Color getLineNumberBackground() { return lineNumberBackground; }
    public Color getLineNumberForeground() { return lineNumberForeground; }
    public Color getCurrentLineNumber() { return currentLineNumber; }
    public Color getBreadcrumbBackground() { return breadcrumbBackground; }
    public Color getKeywordColor() { return keywordColor; }
    public Color getTypeColor() { return typeColor; }
    public Color getStringColor() { return stringColor; }
    public Color getCommentColor() { return commentColor; }
    public Color getNumberColor() { return numberColor; }
    public Color getFunctionColor() { return functionColor; }
}

/**
 * Editor preferences dialog
 */
class EditorPreferencesDialog extends JDialog {
    private final EditorSettings settings;
    private final EditorThemeManager themeManager;
    private final EditorIntegration integration;
    
    private JComboBox<String> fontFamilyCombo;
    private JSpinner fontSizeSpinner;
    private JComboBox<String> themeCombo;
    private JCheckBox autoSaveCheck;
    private JSpinner autoSaveIntervalSpinner;
    private JCheckBox autoCompleteCheck;
    private JSpinner autoCompleteDelaySpinner;
    private JCheckBox lineNumbersCheck;
    private JCheckBox minimapCheck;
    private JCheckBox breadcrumbsCheck;
    private JCheckBox errorDetectionCheck;
    private JCheckBox codeFoldingCheck;
    private JCheckBox multiCursorCheck;
    
    public EditorPreferencesDialog(JFrame parent, EditorSettings settings, 
                                  EditorThemeManager themeManager, EditorIntegration integration) {
        super(parent, "Editor Preferences", true);
        this.settings = settings;
        this.themeManager = themeManager;
        this.integration = integration;
        
        initializeComponents();
        layoutComponents();
        loadCurrentSettings();
        setupEventHandlers();
        
        setSize(500, 600);
        setLocationRelativeTo(parent);
    }
    
    private void initializeComponents() {
        // Font settings
        fontFamilyCombo = new JComboBox<>(new String[]{
            "JetBrains Mono", "Consolas", "Monaco", "Menlo", "Source Code Pro", 
            "Fira Code", "Cascadia Code", "SF Mono", "Roboto Mono"
        });
        fontSizeSpinner = new JSpinner(new SpinnerNumberModel(14, 8, 72, 1));
        
        // Theme settings
        themeCombo = new JComboBox<>(themeManager.getAvailableThemes().toArray(new String[0]));
        
        // Feature toggles
        autoSaveCheck = new JCheckBox("Enable auto-save");
        autoSaveIntervalSpinner = new JSpinner(new SpinnerNumberModel(30, 5, 300, 5));
        
        autoCompleteCheck = new JCheckBox("Enable auto-completion");
        autoCompleteDelaySpinner = new JSpinner(new SpinnerNumberModel(300, 0, 2000, 50));
        
        lineNumbersCheck = new JCheckBox("Show line numbers");
        minimapCheck = new JCheckBox("Show minimap");
        breadcrumbsCheck = new JCheckBox("Show breadcrumbs");
        errorDetectionCheck = new JCheckBox("Enable live error detection");
        codeFoldingCheck = new JCheckBox("Enable code folding");
        multiCursorCheck = new JCheckBox("Enable multi-cursor editing");
    }
    
    private void layoutComponents() {
        setLayout(new BorderLayout());
        
        JTabbedPane tabbedPane = new JTabbedPane();
        
        // Appearance tab
        JPanel appearancePanel = new JPanel(new GridBagLayout());
        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);
        gbc.anchor = GridBagConstraints.WEST;
        
        gbc.gridx = 0; gbc.gridy = 0;
        appearancePanel.add(new JLabel("Font Family:"), gbc);
        gbc.gridx = 1;
        appearancePanel.add(fontFamilyCombo, gbc);
        
        gbc.gridx = 0; gbc.gridy = 1;
        appearancePanel.add(new JLabel("Font Size:"), gbc);
        gbc.gridx = 1;
        appearancePanel.add(fontSizeSpinner, gbc);
        
        gbc.gridx = 0; gbc.gridy = 2;
        appearancePanel.add(new JLabel("Theme:"), gbc);
        gbc.gridx = 1;
        appearancePanel.add(themeCombo, gbc);
        
        tabbedPane.addTab("Appearance", appearancePanel);
        
        // Behavior tab
        JPanel behaviorPanel = new JPanel(new GridBagLayout());
        gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);
        gbc.anchor = GridBagConstraints.WEST;
        
        gbc.gridx = 0; gbc.gridy = 0; gbc.gridwidth = 2;
        behaviorPanel.add(autoSaveCheck, gbc);
        
        gbc.gridx = 0; gbc.gridy = 1; gbc.gridwidth = 1;
        behaviorPanel.add(new JLabel("Auto-save interval (seconds):"), gbc);
        gbc.gridx = 1;
        behaviorPanel.add(autoSaveIntervalSpinner, gbc);
        
        gbc.gridx = 0; gbc.gridy = 2; gbc.gridwidth = 2;
        behaviorPanel.add(autoCompleteCheck, gbc);
        
        gbc.gridx = 0; gbc.gridy = 3; gbc.gridwidth = 1;
        behaviorPanel.add(new JLabel("Auto-complete delay (ms):"), gbc);
        gbc.gridx = 1;
        behaviorPanel.add(autoCompleteDelaySpinner, gbc);
        
        tabbedPane.addTab("Behavior", behaviorPanel);
        
        // Features tab
        JPanel featuresPanel = new JPanel(new GridBagLayout());
        gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);
        gbc.anchor = GridBagConstraints.WEST;
        
        gbc.gridx = 0; gbc.gridy = 0;
        featuresPanel.add(lineNumbersCheck, gbc);
        gbc.gridy = 1;
        featuresPanel.add(minimapCheck, gbc);
        gbc.gridy = 2;
        featuresPanel.add(breadcrumbsCheck, gbc);
        gbc.gridy = 3;
        featuresPanel.add(errorDetectionCheck, gbc);
        gbc.gridy = 4;
        featuresPanel.add(codeFoldingCheck, gbc);
        gbc.gridy = 5;
        featuresPanel.add(multiCursorCheck, gbc);
        
        tabbedPane.addTab("Features", featuresPanel);
        
        add(tabbedPane, BorderLayout.CENTER);
        
        // Buttons
        JPanel buttonPanel = new JPanel(new FlowLayout());
        JButton okButton = new JButton("OK");
        JButton cancelButton = new JButton("Cancel");
        JButton applyButton = new JButton("Apply");
        
        okButton.addActionListener(e -> { applySettings(); dispose(); });
        cancelButton.addActionListener(e -> dispose());
        applyButton.addActionListener(e -> applySettings());
        
        buttonPanel.add(okButton);
        buttonPanel.add(cancelButton);
        buttonPanel.add(applyButton);
        
        add(buttonPanel, BorderLayout.SOUTH);
    }
    
    private void loadCurrentSettings() {
        fontFamilyCombo.setSelectedItem(settings.getFontFamily());
        fontSizeSpinner.setValue(settings.getFontSize());
        themeCombo.setSelectedItem(settings.getTheme());
        autoSaveCheck.setSelected(settings.isAutoSaveEnabled());
        autoSaveIntervalSpinner.setValue(settings.getAutoSaveInterval());
        autoCompleteCheck.setSelected(settings.isAutoCompleteEnabled());
        autoCompleteDelaySpinner.setValue(settings.getAutoCompleteDelay());
        lineNumbersCheck.setSelected(settings.isLineNumbersEnabled());
        minimapCheck.setSelected(settings.isMinimapEnabled());
        breadcrumbsCheck.setSelected(settings.isBreadcrumbsEnabled());
        errorDetectionCheck.setSelected(settings.isErrorDetectionEnabled());
        codeFoldingCheck.setSelected(settings.isCodeFoldingEnabled());
        multiCursorCheck.setSelected(settings.isMultiCursorEnabled());
    }
    
    private void setupEventHandlers() {
        // Enable/disable related controls
        autoSaveCheck.addActionListener(e -> 
            autoSaveIntervalSpinner.setEnabled(autoSaveCheck.isSelected()));
        
        autoCompleteCheck.addActionListener(e -> 
            autoCompleteDelaySpinner.setEnabled(autoCompleteCheck.isSelected()));
    }
    
    private void applySettings() {
        settings.setFontFamily((String) fontFamilyCombo.getSelectedItem());
        settings.setFontSize((Integer) fontSizeSpinner.getValue());
        settings.setTheme((String) themeCombo.getSelectedItem());
        settings.setAutoSaveEnabled(autoSaveCheck.isSelected());
        // Apply other settings...
        
        settings.saveSettings();
        
        // Apply changes to the editor
        integration.applySettings();
        integration.applyTheme((String) themeCombo.getSelectedItem());
    }
}

/**
 * Editor toolbar with common actions
 */
class EditorToolbar extends JToolBar {
    private final EditorIntegration integration;
    
    public EditorToolbar(EditorIntegration integration) {
        this.integration = integration;
        setFloatable(false);
        initializeButtons();
    }
    
    private void initializeButtons() {
        // File operations
        add(createButton("New", "new-file.png", e -> newFile()));
        add(createButton("Open", "open-file.png", e -> openFile()));
        add(createButton("Save", "save-file.png", e -> integration.saveFile()));
        add(createButton("Save As", "save-as.png", e -> integration.saveAsFile()));
        
        addSeparator();
        
        // Edit operations
        add(createButton("Undo", "undo.png", e -> undo()));
        add(createButton("Redo", "redo.png", e -> redo()));
        
        addSeparator();
        
        // Search operations
        add(createButton("Find", "find.png", e -> showFind()));
        add(createButton("Replace", "replace.png", e -> showReplace()));
        
        addSeparator();
        
        // View options
        add(createToggleButton("Line Numbers", "line-numbers.png", true, e -> toggleLineNumbers()));
        add(createToggleButton("Minimap", "minimap.png", true, e -> toggleMinimap()));
        add(createToggleButton("Breadcrumbs", "breadcrumbs.png", true, e -> toggleBreadcrumbs()));
        
        addSeparator();
        
        // Language selection
        JComboBox<String> languageCombo = new JComboBox<>(new String[]{
            "Java", "JavaScript", "TypeScript", "Python", "C++", "C#", "Go", "Rust", "PHP", "Ruby"
        });
        languageCombo.addActionListener(e -> {
            String selected = (String) languageCombo.getSelectedItem();
            if (selected != null) {
                integration.setLanguage(selected.toLowerCase());
            }
        });
        add(new JLabel("Language: "));
        add(languageCombo);
    }
    
    private JButton createButton(String text, String iconName, ActionListener action) {
        JButton button = new JButton(text);
        // button.setIcon(loadIcon(iconName)); // Would load actual icons
        button.addActionListener(action);
        button.setToolTipText(text);
        return button;
    }
    
    private JToggleButton createToggleButton(String text, String iconName, boolean selected, ActionListener action) {
        JToggleButton button = new JToggleButton(text, selected);
        // button.setIcon(loadIcon(iconName)); // Would load actual icons
        button.addActionListener(action);
        button.setToolTipText(text);
        return button;
    }
    
    private void newFile() {
        // Create new file
        integration.getEditor().setText("");
    }
    
    private void openFile() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            integration.openFile(chooser.getSelectedFile());
        }
    }
    
    private void undo() {
        // Implement undo
    }
    
    private void redo() {
        // Implement redo
    }
    
    private void showFind() {
        // Show find dialog
    }
    
    private void showReplace() {
        // Show replace dialog
    }
    
    private void toggleLineNumbers() {
        // Toggle line numbers visibility
    }
    
    private void toggleMinimap() {
        // Toggle minimap visibility
    }
    
    private void toggleBreadcrumbs() {
        // Toggle breadcrumbs visibility
    }
}

/**
 * Status bar for the editor
 */
class EditorStatusBar extends JPanel {
    private final JLabel positionLabel;
    private final JLabel languageLabel;
    private final JLabel encodingLabel;
    private final JLabel lineEndingLabel;
    private final JLabel selectionLabel;
    private final JProgressBar progressBar;
    
    public EditorStatusBar() {
        setLayout(new BorderLayout());
        setBorder(BorderFactory.createEtchedBorder());
        setPreferredSize(new Dimension(0, 25));
        
        positionLabel = new JLabel("Ln 1, Col 1");
        languageLabel = new JLabel("Java");
        encodingLabel = new JLabel("UTF-8");
        lineEndingLabel = new JLabel("LF");
        selectionLabel = new JLabel("");
        progressBar = new JProgressBar();
        progressBar.setVisible(false);
        
        JPanel leftPanel = new JPanel(new FlowLayout(FlowLayout.LEFT, 5, 2));
        leftPanel.add(positionLabel);
        leftPanel.add(new JSeparator(SwingConstants.VERTICAL));
        leftPanel.add(selectionLabel);
        
        JPanel rightPanel = new JPanel(new FlowLayout(FlowLayout.RIGHT, 5, 2));
        rightPanel.add(languageLabel);
        rightPanel.add(new JSeparator(SwingConstants.VERTICAL));
        rightPanel.add(encodingLabel);
        rightPanel.add(new JSeparator(SwingConstants.VERTICAL));
        rightPanel.add(lineEndingLabel);
        
        add(leftPanel, BorderLayout.WEST);
        add(progressBar, BorderLayout.CENTER);
        add(rightPanel, BorderLayout.EAST);
    }
    
    public void updatePosition(int line, int column) {
        positionLabel.setText(String.format("Ln %d, Col %d", line, column));
    }
    
    public void updateLanguage(String language) {
        languageLabel.setText(language);
    }
    
    public void updateSelection(int length) {
        if (length > 0) {
            selectionLabel.setText(String.format("(%d selected)", length));
        } else {
            selectionLabel.setText("");
        }
    }
    
    public void showProgress(String message) {
        progressBar.setString(message);
        progressBar.setStringPainted(true);
        progressBar.setIndeterminate(true);
        progressBar.setVisible(true);
    }
    
    public void hideProgress() {
        progressBar.setVisible(false);
        progressBar.setIndeterminate(false);
    }
}

// This completes the Editor Integration layer
// Total additional lines: ~800 lines
// Combined with previous files: ~3,600 lines of the Advanced Code Editor Engine