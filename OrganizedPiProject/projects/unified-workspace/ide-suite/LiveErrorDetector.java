import javax.swing.*;
import javax.swing.text.*;
import java.awt.*;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;
import java.util.regex.*;
import javax.tools.*;
import java.io.*;
import java.net.*;

public class LiveErrorDetector {
    private final JTextPane textPane;
    private final ErrorHighlighter highlighter;
    private final ErrorAnalyzer analyzer;
    private final ScheduledExecutorService executor;
    private final List<ErrorInfo> currentErrors = new ArrayList<>();
    private String currentLanguage = "java";
    
    public LiveErrorDetector(JTextPane textPane) {
        this.textPane = textPane;
        this.highlighter = new ErrorHighlighter(textPane);
        this.analyzer = new ErrorAnalyzer();
        this.executor = Executors.newSingleThreadScheduledExecutor();
        
        setupDocumentListener();
    }
    
    public void start() {
        // Start periodic error checking
        executor.scheduleWithFixedDelay(this::checkErrors, 1, 2, TimeUnit.SECONDS);
    }
    
    public void stop() {
        executor.shutdown();
    }
    
    private void setupDocumentListener() {
        textPane.getDocument().addDocumentListener(new DocumentListener() {
            public void insertUpdate(DocumentEvent e) {
                scheduleErrorCheck();
            }
            public void removeUpdate(DocumentEvent e) {
                scheduleErrorCheck();
            }
            public void changedUpdate(DocumentEvent e) {}
        });
    }
    
    private void scheduleErrorCheck() {
        // Debounce rapid typing
        executor.schedule(this::checkErrors, 500, TimeUnit.MILLISECONDS);
    }
    
    public void checkErrors() {
        SwingUtilities.invokeLater(() -> {
            String text = textPane.getText();
            List<ErrorInfo> errors = analyzer.analyzeCode(text, currentLanguage);
            
            synchronized (currentErrors) {
                currentErrors.clear();
                currentErrors.addAll(errors);
            }
            
            highlighter.highlightErrors(errors);
            
            // Fire property change for IDE integration
            textPane.firePropertyChange("errorsDetected", null, new ArrayList<>(errors));
        });
    }
    
    public void setLanguage(String language) {
        this.currentLanguage = language;
        checkErrors();
    }
    
    public List<ErrorInfo> getCurrentErrors() {
        synchronized (currentErrors) {
            return new ArrayList<>(currentErrors);
        }
    }
}

class ErrorAnalyzer {
    private final JavaCompiler javaCompiler;
    private final InMemoryJavaFileManager fileManager;

    public ErrorAnalyzer() {
        this.javaCompiler = ToolProvider.getSystemJavaCompiler();
        if (this.javaCompiler == null) {
            throw new IllegalStateException("JavaCompiler not found. Make sure you are running on a JDK, not a JRE.");
        }
        this.fileManager = new InMemoryJavaFileManager(javaCompiler.getStandardFileManager(null, null, null));
    }

    public List<ErrorInfo> analyzeCode(String code, String language) {
        if (!"java".equalsIgnoreCase(language) || javaCompiler == null) {
            return analyzeGenericCode(code); // Fallback for non-Java or no compiler
        }
        return compileJavaCode(code);
    }

    private String extractClassName(String code) {
        Pattern pattern = Pattern.compile("public\\s+class\\s+([\\w$]+)");
        Matcher matcher = pattern.matcher(code);
        if (matcher.find()) {
            return matcher.group(1);
        }
        // Fallback for non-public classes, simple case
        pattern = Pattern.compile("class\\s+([\\w$]+)");
        matcher = pattern.matcher(code);
        if (matcher.find()) {
            return matcher.group(1);
        }
        return null; // Or a default name if appropriate
    }

    private List<ErrorInfo> compileJavaCode(String code) {
        List<ErrorInfo> errors = new ArrayList<>();
        String className = extractClassName(code);

        if (className == null) {
            // Cannot compile without a class name. Maybe add a generic error.
            // errors.add(new ErrorInfo("No class definition found.", 1, 0, ErrorType.ERROR));
            return errors; // Or return some other indicator of failure
        }

        fileManager.addSourceFile(className, code);
        
        DiagnosticCollector<JavaFileObject> diagnostics = new DiagnosticCollector<>();
        Iterable<? extends JavaFileObject> compilationUnits = fileManager.getJavaFileObjectsFromStrings(Collections.singletonList(className));

        JavaCompiler.CompilationTask task = javaCompiler.getTask(
            null, fileManager, diagnostics, null, null, compilationUnits);

        task.call();

        for (Diagnostic<? extends JavaFileObject> diagnostic : diagnostics.getDiagnostics()) {
            ErrorType type = diagnostic.getKind() == Diagnostic.Kind.ERROR ?
                ErrorType.ERROR : ErrorType.WARNING;
            
            // Correct the line number if it's from an in-memory source
            long errorLine = diagnostic.getLineNumber();
            long errorColumn = diagnostic.getColumnNumber();

            errors.add(new ErrorInfo(
                diagnostic.getMessage(null),
                (int) errorLine,
                (int) errorColumn,
                type
            ));
        }
        return errors;
    }

    
    private List<ErrorInfo> analyzeGenericCode(String code) {
        List<ErrorInfo> errors = new ArrayList<>();
        // This can be expanded with more generic checks if needed.
        return errors;
    }
}

class ErrorHighlighter {
    private final JTextPane textPane;
    private final Map<ErrorInfo, Object> highlightTags = new HashMap<>();
    
    public ErrorHighlighter(JTextPane textPane) {
        this.textPane = textPane;
    }
    
    public void highlightErrors(List<ErrorInfo> errors) {
        // Clear previous highlights
        clearHighlights();
        
        StyledDocument doc = textPane.getStyledDocument();
        
        for (ErrorInfo error : errors) {
            try {
                int startOffset = getOffsetFromLineColumn(error.getLine(), error.getColumn());
                int endOffset = Math.min(startOffset + 10, doc.getLength()); // Highlight 10 chars or less
                
                Style style = getErrorStyle(error.getType());
                doc.setCharacterAttributes(startOffset, endOffset - startOffset, style, false);
                
                // Store for later removal
                highlightTags.put(error, null);
                
            } catch (Exception e) {
                // Ignore invalid positions
            }
        }
    }
    
    private void clearHighlights() {
        StyledDocument doc = textPane.getStyledDocument();
        Style defaultStyle = StyleContext.getDefaultStyleContext().getStyle(StyleContext.DEFAULT_STYLE);
        doc.setCharacterAttributes(0, doc.getLength(), defaultStyle, true);
        highlightTags.clear();
    }
    
    private int getOffsetFromLineColumn(int line, int column) {
        Element root = textPane.getDocument().getDefaultRootElement();
        if (line <= 0 || line > root.getElementCount()) return 0;
        
        Element lineElement = root.getElement(line - 1);
        int lineStart = lineElement.getStartOffset();
        int lineEnd = lineElement.getEndOffset();
        
        return Math.min(lineStart + Math.max(0, column), lineEnd - 1);
    }
    
    private Style getErrorStyle(ErrorType type) {
        Style style = StyleContext.getDefaultStyleContext().addStyle(type.name(), null);
        
        switch (type) {
            case ERROR:
                StyleConstants.setForeground(style, Color.RED);
                StyleConstants.setUnderline(style, true);
                break;
            case WARNING:
                StyleConstants.setForeground(style, new Color(255, 140, 0));
                StyleConstants.setUnderline(style, true);
                break;
            case INFO:
                StyleConstants.setForeground(style, Color.BLUE);
                StyleConstants.setItalic(style, true);
                break;
        }
        
        return style;
    }
}

class ErrorInfo {
    private final String message;
    private final int line;
    private final int column;
    private final ErrorType type;
    
    public ErrorInfo(String message, int line, int column, ErrorType type) {
        this.message = message;
        this.line = line;
        this.column = column;
        this.type = type;
    }
    
    public String getMessage() { return message; }
    public int getLine() { return line; }
    public int getColumn() { return column; }
    public ErrorType getType() { return type; }
    
    @Override
    public String toString() {
        return String.format("[%d:%d] %s: %s", line, column, type, message);
    }
    
    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof ErrorInfo)) return false;
        ErrorInfo other = (ErrorInfo) obj;
        return line == other.line && column == other.column && 
               Objects.equals(message, other.message) && type == other.type;
    }
    
    @Override
    public int hashCode() {
        return Objects.hash(message, line, column, type);
    }
}

enum ErrorType {
    ERROR, WARNING, INFO
}

class InMemoryJavaFileManager extends ForwardingJavaFileManager<JavaFileManager> {
    private final Map<String, JavaSourceFromString> sourceFiles = new HashMap<>();

    public InMemoryJavaFileManager(JavaFileManager fileManager) {
        super(fileManager);
    }

    public void addSourceFile(String className, String code) {
        sourceFiles.put(className, new JavaSourceFromString(className, code));
    }

    @Override
    public JavaFileObject getJavaFileForInput(Location location, String className, JavaFileObject.Kind kind) throws IOException {
        JavaSourceFromString sourceFile = sourceFiles.get(className);
        if (sourceFile != null) {
            return sourceFile;
        }
        return super.getJavaFileForInput(location, className, kind);
    }

    public Iterable<JavaFileObject> getJavaFileObjectsFromStrings(Iterable<String> classNames) {
        List<JavaFileObject> compilationUnits = new ArrayList<>();
        for (String className : classNames) {
            JavaSourceFromString sourceFile = sourceFiles.get(className);
            if (sourceFile != null) {
                compilationUnits.add(sourceFile);
            }
        }
        return compilationUnits;
    }
}

class JavaSourceFromString extends SimpleJavaFileObject {
    final String code;

    JavaSourceFromString(String name, String code) {
        super(URI.create("string:///" + name.replace('.', '/') + Kind.SOURCE.extension), Kind.SOURCE);
        this.code = code;
    }

    @Override
    public CharSequence getCharContent(boolean ignoreEncodingErrors) {
        return code;
    }
}