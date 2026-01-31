import javax.swing.*;
import javax.swing.text.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.io.*;
import java.nio.file.*;
import java.util.regex.*;

public class AdvancedCodeEditor extends JPanel {
    private final JTextPane editorPane;
    private final SyntaxHighlighter highlighter;
    private String currentLanguage = "java";
    private File currentFile;
    private boolean isDirty = false;
    private File lastDropped;
    private Path workspace = Paths.get(".");
    
    public AdvancedCodeEditor() {
        setLayout(new BorderLayout());
        editorPane = new JTextPane();
        highlighter = new SyntaxHighlighter();
        
        JPanel bottomPanel = new JPanel(new FlowLayout());
        JButton runBtn = new JButton("RUN π");
        runBtn.addActionListener(e -> embeddedRun());
        bottomPanel.add(runBtn);
        
        add(new JScrollPane(editorPane), BorderLayout.CENTER);
        add(bottomPanel, BorderLayout.SOUTH);
        
        editorPane.getDocument().addDocumentListener(new DocumentListener() {
            public void insertUpdate(DocumentEvent e) { highlightCode(); }
            public void removeUpdate(DocumentEvent e) { highlightCode(); }
            public void changedUpdate(DocumentEvent e) { highlightCode(); }
        });
    }
    
    private void highlightCode() {
        SwingUtilities.invokeLater(() -> highlighter.highlight(editorPane, currentLanguage));
    }
    
    public void openFile(File file) {
        if (file == null) {
            editorPane.setText("");
            currentFile = null;
            isDirty = false;
            return;
        }
        
        try {
            String content = Files.readString(file.toPath());
            editorPane.setText(content);
            currentFile = file;
            currentLanguage = detectLanguage(file.getName());
            isDirty = false;
            highlightCode();
        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Error opening file: " + e.getMessage());
        }
    }
    
    public void saveFile() {
        if (currentFile == null) {
            JFileChooser chooser = new JFileChooser();
            if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
                currentFile = chooser.getSelectedFile();
            } else return;
        }
        
        try {
            Files.writeString(currentFile.toPath(), editorPane.getText());
            isDirty = false;
        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Error saving file: " + e.getMessage());
        }
    }
    
    private String detectLanguage(String filename) {
        if (filename == null) return "text";
        String ext = "";
        int i = filename.lastIndexOf('.');
        if (i > 0) {
            ext = filename.substring(i + 1).toLowerCase();
        }
        switch (ext) {
            case "java": return "java";
            case "js": return "javascript";
            case "py": return "python";
            case "cs": return "csharp";
            case "c": return "c";
            case "cpp": return "cpp";
            case "asm": return "asm";
            default: return "text";
        }
    }
    
    public void setText(String text) { editorPane.setText(text); }
    public String getText() { return editorPane.getText(); }
    public boolean isDirty() { return isDirty; }
    public void setLanguage(String language) { 
        this.currentLanguage = language;
        highlightCode();
    }
    
    private PaidTierManager tierManager = new PaidTierManager();
    private int compilationCount = 0;
    
    private void embeddedRun() {
        if (currentFile == null && editorPane.getText().trim().isEmpty()) {
            JOptionPane.showMessageDialog(this, "Nothing to run!");
            return;
        }
        
        // Check tier limits
        if (!tierManager.checkCompilationLimit(compilationCount)) {
            JOptionPane.showMessageDialog(this, tierManager.getUpgradeMessage());
            return;
        }
        
        try {
            compilationCount++;
            PiBeacon.log("3,14PIZL0G1C embedded-run-start for " + currentLanguage + " [" + tierManager.getCurrentTier() + "]");
            
            String source = editorPane.getText();
            if (source.trim().isEmpty()) {
                JOptionPane.showMessageDialog(this, "Source is empty!");
                return;
            }

            switch (currentLanguage) {
                case "csharp":
                    runCSharp(source);
                    break;
                case "java":
                    runJava(source);
                    break;
                case "c":
                case "cpp":
                    runCpp(source);
                    break;
                case "asm":
                    runAsm(source);
                    break;
                default:
                    JOptionPane.showMessageDialog(this, "RUN π is not supported for " + currentLanguage + " yet.");
            }

        } catch (Exception ex) {
            JOptionPane.showMessageDialog(this, "RUN failed: " + ex.getMessage(), "RUN π Error", JOptionPane.ERROR_MESSAGE);
        }
    }

    private void runCSharp(String source) throws Exception {
        Path roslynBox = Paths.get("RoslynBox");
        if (!Files.exists(roslynBox.resolve("RoslynBoxEngine.dll"))) {
            throw new FileNotFoundException("RoslynBox not found – drop the 5 MB bundle first");
        }
        
        if (source.trim().isEmpty()) {
            source = "using System; public class Runner { public static void Main() { Console.WriteLine(\"π embedded run – C#\"); } }";
        }
        
        byte[] dll = compileWithRoslynBox(source, roslynBox);
        
        String ext = chooseExtension(new String[]{"exe", "dll"});
        Path outFile = workspace.resolve("EmbeddedRun." + ext);
        
        Files.write(outFile, dll);
        
        new ProcessBuilder(outFile.toString())
            .directory(workspace.toFile())
            .redirectError(ProcessBuilder.Redirect.INHERIT)
            .redirectOutput(ProcessBuilder.Redirect.INHERIT)
            .start();
        
        PiBeacon.log("3,14PIZL0G1C C# run launched → " + outFile);
    }

    private void runJava(String source) throws Exception {
        String className = "EmbeddedRun";
        Pattern classPattern = Pattern.compile("public\\s+class\\s+([\\w_]+)");
        Matcher matcher = classPattern.matcher(source);
        if (matcher.find()) {
            className = matcher.group(1);
        }
        
        Path sourceFile = workspace.resolve(className + ".java");
        Files.writeString(sourceFile, source);

        if (!isCompilerAvailable("javac")) {
            throw new IOException("javac is not installed or not in the system's PATH.");
        }
        ProcessBuilder javac = new ProcessBuilder("javac", sourceFile.toString());
        javac.directory(workspace.toFile());
        Process p = javac.start();
        if (p.waitFor() != 0) {
            try (InputStream err = p.getErrorStream()) {
                String error = new String(err.readAllBytes());
                throw new IOException("Java compilation failed:\n" + error);
            }
        }

        String ext = chooseExtension(new String[]{"class", "jar"});
        if ("jar".equals(ext)) {
            Path jarFile = workspace.resolve(className + ".jar");
            ProcessBuilder jar = new ProcessBuilder("jar", "cfe", jarFile.toString(), className, className + ".class");
            jar.directory(workspace.toFile());
            if (jar.start().waitFor() != 0) throw new IOException("Failed to create JAR");
            
            new ProcessBuilder("java", "-jar", jarFile.toString()).directory(workspace.toFile()).redirectError(ProcessBuilder.Redirect.INHERIT).redirectOutput(ProcessBuilder.Redirect.INHERIT).start();
            PiBeacon.log("3,14PIZL0G1C Java JAR launched → " + jarFile);
        } else {
             new ProcessBuilder("java", className).directory(workspace.toFile()).redirectError(ProcessBuilder.Redirect.INHERIT).redirectOutput(ProcessBuilder.Redirect.INHERIT).start();
             PiBeacon.log("3,14PIZL0G1C Java class launched → " + className);
        }
    }

    private void runCpp(String source) throws Exception {
        Path sourceFile = workspace.resolve("embedded_run.cpp");
        Files.writeString(sourceFile, source);

        String ext = chooseExtension(new String[]{"exe", "bin", "elf", "so"});
        Path outFile = workspace.resolve("EmbeddedRun." + ext);

        ProcessBuilder gpp = new ProcessBuilder("g++", sourceFile.toString(), "-o", outFile.toString());
        gpp.directory(workspace.toFile());
        Process p = gpp.start();
        if (p.waitFor() != 0) {
             try (InputStream err = p.getErrorStream()) {
                String error = new String(err.readAllBytes());
                throw new IOException("C/C++ compilation failed:\n" + error);
            }
        }

        new ProcessBuilder(outFile.toString()).directory(workspace.toFile()).redirectError(ProcessBuilder.Redirect.INHERIT).redirectOutput(ProcessBuilder.Redirect.INHERIT).start();
        PiBeacon.log("3,14PIZL0G1C C/C++ launched → " + outFile);
    }

    private void runAsm(String source) throws Exception {
        // This is a placeholder for a real embedded assembler
        PiBeacon.log("ASM run: This would typically involve an assembler and linker.");
        JOptionPane.showMessageDialog(this, "ASM execution is a placeholder.\nIn a real scenario, this would involve a complex build process.", "ASM RUN π", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private byte[] compileWithRoslynBox(String csharp, Path boxDir) throws Exception {
        ProcessBuilder pb = new ProcessBuilder(
            "dotnet", "exec", "--runtimeconfig", boxDir.resolve("RoslynBox.runtimeconfig.json").toString(),
            boxDir.resolve("RoslynBoxEngine.dll").toString(),
            csharp, "EmbeddedRun"
        );
        pb.directory(boxDir.toFile());
        Process p = pb.start();
        if (p.waitFor() != 0) throw new IOException("RoslynBox compile failed");
        try (InputStream in = p.getInputStream()) {
            return in.readAllBytes();
        }
    }
    
    private String chooseExtension(String[] exts) {
        String result = (String) JOptionPane.showInputDialog(this,
            "Launch extension:", "RUN π",
            JOptionPane.QUESTION_MESSAGE, null, exts, exts[0]);
        return result != null ? result : exts[0];
    }
}

class PiBeacon {
    public static void log(String message) {
        System.out.println("π " + message);
    }
}

class SyntaxHighlighter {
    private final Map<String, LanguageDefinition> languages = new HashMap<>();
    
    public SyntaxHighlighter() {
        languages.put("java", new LanguageDefinition(
            Arrays.asList("public", "private", "class", "interface", "if", "else", "for", "while"),
            Arrays.asList("String", "int", "boolean", "void"),
            Arrays.asList("true", "false", "null")
        ));
    }
    
    public void highlight(JTextPane pane, String language) {
        LanguageDefinition lang = languages.get(language);
        if (lang == null) return;
        
        StyledDocument doc = pane.getStyledDocument();
        String text = pane.getText();
        
        doc.setCharacterAttributes(0, text.length(), getDefaultStyle(), true);
        
        for (String keyword : lang.keywords) {
            highlightWord(doc, text, keyword, getKeywordStyle());
        }
    }
    
    private void highlightWord(StyledDocument doc, String text, String word, SimpleAttributeSet style) {
        int index = 0;
        while ((index = text.indexOf(word, index)) != -1) {
            if (isWordBoundary(text, index, word.length())) {
                doc.setCharacterAttributes(index, word.length(), style, false);
            }
            index += word.length();
        }
    }
    
    private boolean isWordBoundary(String text, int start, int length) {
        if (start > 0 && Character.isLetterOrDigit(text.charAt(start - 1))) return false;
        if (start + length < text.length() && Character.isLetterOrDigit(text.charAt(start + length))) return false;
        return true;
    }
    
    private SimpleAttributeSet getDefaultStyle() {
        SimpleAttributeSet style = new SimpleAttributeSet();
        StyleConstants.setForeground(style, Color.BLACK);
        return style;
    }
    
    private SimpleAttributeSet getKeywordStyle() {
        SimpleAttributeSet style = new SimpleAttributeSet();
        StyleConstants.setForeground(style, Color.BLUE);
        StyleConstants.setBold(style, true);
        return style;
    }
}

class LanguageDefinition {
    public final java.util.List<String> keywords;
    public final java.util.List<String> types;
    public final java.util.List<String> literals;
    
    public LanguageDefinition(java.util.List<String> keywords, java.util.List<String> types, java.util.List<String> literals) {
        this.keywords = keywords;
        this.types = types;
        this.literals = literals;
    }
}