import javax.swing.*;
import java.awt.event.ActionEvent;
import java.io.File;

/**
 * Extensions to IDEMain for the Advanced Code Editor integration
 */
public class IDEMainExtensions {
    
    // Helper methods for editor integration
    public static String getCurrentEditorText(EditorIntegration editorIntegration) {
        return editorIntegration.getEditor().getText();
    }
    
    public static void setCurrentEditorText(EditorIntegration editorIntegration, String text) {
        editorIntegration.getEditor().setText(text);
    }
    
    public static void appendToEditor(EditorIntegration editorIntegration, String text) {
        String currentText = getCurrentEditorText(editorIntegration);
        setCurrentEditorText(editorIntegration, currentText + text);
    }
    
    public static void loadWelcomeContent(EditorIntegration editorIntegration) {
        String welcomeContent = "// Welcome to Advanced Agentic IDE - Enterprise Edition\n" +
                               "// Features:\n" +
                               "//   - Advanced syntax highlighting for 50+ languages\n" +
                               "//   - Multi-cursor editing\n" +
                               "//   - Code folding and minimap\n" +
                               "//   - Live error detection\n" +
                               "//   - Intelligent auto-completion\n" +
                               "//   - AI-powered code review and fixes\n" +
                               "//\n" +
                               "// Use 'AI Tools > Auto-Fix Code' to apply AI fixes directly.\n" +
                               "// Press Ctrl+Space for auto-completion.\n" +
                               "// Use Ctrl+Alt+Up/Down for multi-cursor editing.\n\n" +
                               "public class HelloWorld {\n" +
                               "    public static void main(String[] args) {\n" +
                               "        System.out.println(\"Hello, Advanced IDE!\");\n" +
                               "    }\n" +
                               "}\n";
        setCurrentEditorText(editorIntegration, welcomeContent);
        editorIntegration.setLanguage("java");
    }
    
    // File operations
    public static void newFile(EditorIntegration editorIntegration) {
        if (editorIntegration.isDirty()) {
            int result = JOptionPane.showConfirmDialog(null, 
                "Current file has unsaved changes. Save before creating new file?", 
                "Unsaved Changes", JOptionPane.YES_NO_CANCEL_OPTION);
            if (result == JOptionPane.YES_OPTION) {
                editorIntegration.saveFile();
            } else if (result == JOptionPane.CANCEL_OPTION) {
                return;
            }
        }
        setCurrentEditorText(editorIntegration, "");
    }
    
    public static void openFile(EditorIntegration editorIntegration, EditorStatusBar statusBar) {
        JFileChooser chooser = new JFileChooser();
        chooser.setFileFilter(new javax.swing.filechooser.FileNameExtensionFilter(
            "Source Files", "java", "js", "ts", "py", "cpp", "c", "cs", "go", "rs", "php", "rb"));
        
        if (chooser.showOpenDialog(null) == JFileChooser.APPROVE_OPTION) {
            File file = chooser.getSelectedFile();
            editorIntegration.openFile(file);
            if (statusBar != null) {
                statusBar.updateLanguage(editorIntegration.getCurrentLanguage());
            }
        }
    }
    
    public static void showPreferences(JFrame parent) {
        JOptionPane.showMessageDialog(parent, "Preferences dialog would open here.", "Preferences", JOptionPane.INFORMATION_MESSAGE);
    }
    
    public static void toggleLineNumbers() {
        JOptionPane.showMessageDialog(null, "Line numbers toggled.", "View", JOptionPane.INFORMATION_MESSAGE);
    }
    
    public static void toggleMinimap() {
        JOptionPane.showMessageDialog(null, "Minimap toggled.", "View", JOptionPane.INFORMATION_MESSAGE);
    }
    
    public static void toggleBreadcrumbs() {
        JOptionPane.showMessageDialog(null, "Breadcrumbs toggled.", "View", JOptionPane.INFORMATION_MESSAGE);
    }
    
    // Updated AI methods
    public static void autoFixCode(EditorIntegration editorIntegration, EditorStatusBar statusBar, AIOrchestrator orchestrator) {
        String currentCode = getCurrentEditorText(editorIntegration);
        if (currentCode.trim().isEmpty()) {
            JOptionPane.showMessageDialog(null, "Editor is empty. Nothing to fix.", "Auto-Fix", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        if (statusBar != null) statusBar.showProgress("Auto-fixing code...");
        setCurrentEditorText(editorIntegration, "--- Auto-fixing code, please wait... ---\n\n" + currentCode);

        orchestrator.reviewCode(currentCode).whenComplete((fixedCode, error) -> {
            SwingUtilities.invokeLater(() -> {
                if (statusBar != null) statusBar.hideProgress();
                if (error != null) {
                    setCurrentEditorText(editorIntegration, "// Auto-fix failed: " + error.getMessage() + "\n\n" + currentCode);
                    JOptionPane.showMessageDialog(null, "Auto-fix failed: " + error.getMessage(), "Error", JOptionPane.ERROR_MESSAGE);
                } else {
                    setCurrentEditorText(editorIntegration, fixedCode);
                    JOptionPane.showMessageDialog(null, "Code has been auto-fixed and applied.", "Success", JOptionPane.INFORMATION_MESSAGE);
                }
            });
        });
    }
    
    public static void runProviderHealthCheck(EditorIntegration editorIntegration, EditorStatusBar statusBar, AIOrchestrator orchestrator) {
        if (statusBar != null) statusBar.showProgress("Running health checks...");
        setCurrentEditorText(editorIntegration, "Running AI provider health checks...\n");
        orchestrator.runHealthChecks().thenAccept(results -> {
            SwingUtilities.invokeLater(() -> {
                if (statusBar != null) statusBar.hideProgress();
                StringBuilder report = new StringBuilder("--- AI Provider Status Report ---\n\n");
                results.forEach((provider, status) -> {
                    report.append(String.format("%-15s: %s\n", provider.toUpperCase(), status));
                });
                report.append("\n--- End of Report ---");
                setCurrentEditorText(editorIntegration, report.toString());
                JOptionPane.showMessageDialog(null, "Health check complete. See editor for results.");
            });
        });
    }
}