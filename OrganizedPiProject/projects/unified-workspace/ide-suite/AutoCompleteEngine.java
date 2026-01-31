import javax.swing.*;
import javax.swing.text.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;

public class AutoCompleteEngine {
    private final JTextPane textPane;
    private final CompletionPopup popup;
    private final CompletionProvider provider;
    private final Timer delayTimer;
    private String currentLanguage = "java";
    
    public AutoCompleteEngine(JTextPane textPane) {
        this.textPane = textPane;
        this.popup = new CompletionPopup(textPane);
        // Pass the textPane to the provider for context-aware completions
        this.provider = new CompletionProvider(textPane);
        this.delayTimer = new Timer(300, e -> showCompletions());
        delayTimer.setRepeats(false);
        
        setupDocumentListener();
        setupKeyBindings();
    }
    
    public void initialize() {
        provider.loadCompletions(currentLanguage);
    }
    
    private void setupDocumentListener() {
        textPane.getDocument().addDocumentListener(new DocumentListener() {
            public void insertUpdate(DocumentEvent e) {
                scheduleCompletion();
            }
            public void removeUpdate(DocumentEvent e) {
                hideCompletions();
            }
            public void changedUpdate(DocumentEvent e) {}
        });
    }
    
    private void setupKeyBindings() {
        InputMap inputMap = textPane.getInputMap();
        ActionMap actionMap = textPane.getActionMap();
        
        inputMap.put(KeyStroke.getKeyStroke("ctrl SPACE"), "showCompletions");
        inputMap.put(KeyStroke.getKeyStroke("ESCAPE"), "hideCompletions");
        inputMap.put(KeyStroke.getKeyStroke("UP"), "completionUp");
        inputMap.put(KeyStroke.getKeyStroke("DOWN"), "completionDown");
        inputMap.put(KeyStroke.getKeyStroke("ENTER"), "acceptCompletion");
        
        actionMap.put("showCompletions", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                showCompletions();
            }
        });
        
        actionMap.put("hideCompletions", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                hideCompletions();
            }
        });
        
        actionMap.put("completionUp", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                if (popup.isVisible()) {
                    popup.selectPrevious();
                } else {
                    // Default UP behavior
                    try {
                        textPane.getCaretPosition();
                        int pos = Utilities.getPreviousWord(textPane, textPane.getCaretPosition());
                        textPane.setCaretPosition(pos);
                    } catch (BadLocationException ex) {}
                }
            }
        });
        
        actionMap.put("completionDown", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                if (popup.isVisible()) {
                    popup.selectNext();
                } else {
                    // Default DOWN behavior
                    try {
                        int pos = Utilities.getNextWord(textPane, textPane.getCaretPosition());
                        textPane.setCaretPosition(pos);
                    } catch (BadLocationException ex) {}
                }
            }
        });
        
        actionMap.put("acceptCompletion", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                if (popup.isVisible()) {
                    popup.acceptSelected();
                } else {
                    // Default ENTER behavior
                    try {
                        textPane.getDocument().insertString(textPane.getCaretPosition(), "\n", null);
                    } catch (BadLocationException ex) {}
                }
            }
        });
    }
    
    private void scheduleCompletion() {
        delayTimer.restart();
    }
    
    private void showCompletions() {
        try {
            int caretPos = textPane.getCaretPosition();
            String prefix = getCurrentWord(caretPos);
            
            if (prefix.length() >= 1) { // Show completions for even a single character
                // Pass the document to the provider for analysis
                List<Completion> completions = provider.getCompletions(prefix, currentLanguage, textPane.getDocument());
                if (!completions.isEmpty()) {
                    popup.showCompletions(completions, caretPos, prefix);
                } else {
                    popup.hide();
                }
            } else {
                popup.hide();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    private void hideCompletions() {
        popup.hide();
    }
    
    private String getCurrentWord(int caretPos) {
        try {
            Document doc = textPane.getDocument();
            int start = caretPos;
            
            // Find word start
            while (start > 0) {
                char ch = doc.getText(start - 1, 1).charAt(0);
                if (!Character.isJavaIdentifierPart(ch)) break;
                start--;
            }
            
            return doc.getText(start, caretPos - start);
        } catch (BadLocationException e) {
            return "";
        }
    }
    
    public void setLanguage(String language) {
        this.currentLanguage = language;
        provider.loadCompletions(language);
    }
    
    public boolean ping() {
        return provider != null && popup != null;
    }
}

class CompletionPopup extends JWindow {
    private final JList<Completion> list;
    private final DefaultListModel<Completion> model;
    private final JTextPane textPane;
    private String currentPrefix;
    private int insertPosition;
    
    public CompletionPopup(JTextPane textPane) {
        super(SwingUtilities.getWindowAncestor(textPane));
        this.textPane = textPane;
        this.model = new DefaultListModel<>();
        this.list = new JList<>(model);
        
        setupUI();
        setupEventHandlers();
    }
    
    private void setupUI() {
        list.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        list.setCellRenderer(new CompletionCellRenderer());
        list.setVisibleRowCount(8);
        list.setFixedCellHeight(20);
        
        JScrollPane scrollPane = new JScrollPane(list);
        scrollPane.setVerticalScrollBarPolicy(JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED);
        scrollPane.setHorizontalScrollBarPolicy(JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
        scrollPane.setBorder(BorderFactory.createLineBorder(Color.GRAY));
        
        add(scrollPane);
        setSize(300, 160);
    }
    
    private void setupEventHandlers() {
        list.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    acceptSelected();
                }
            }
        });
        
        // Hide on focus loss
        addWindowFocusListener(new WindowFocusListener() {
            public void windowGainedFocus(WindowEvent e) {}
            public void windowLostFocus(WindowEvent e) {
                hide();
            }
        });
    }
    
    public void showCompletions(List<Completion> completions, int position, String prefix) {
        model.clear();
        for (Completion completion : completions) {
            model.addElement(completion);
        }
        
        if (!model.isEmpty()) {
            list.setSelectedIndex(0);
            currentPrefix = prefix;
            insertPosition = position - prefix.length();
            
            // Position popup
            try {
                Rectangle caretRect = textPane.modelToView2D(position).getBounds();
                Point screenPos = new Point(caretRect.x, caretRect.y + caretRect.height);
                SwingUtilities.convertPointToScreen(screenPos, textPane);
                setLocation(screenPos);
                setVisible(true);
            } catch (BadLocationException e) {
                e.printStackTrace();
            }
        }
    }
    
    public void selectNext() {
        int index = list.getSelectedIndex();
        if (index < model.getSize() - 1) {
            list.setSelectedIndex(index + 1);
            list.ensureIndexIsVisible(index + 1);
        }
    }
    
    public void selectPrevious() {
        int index = list.getSelectedIndex();
        if (index > 0) {
            list.setSelectedIndex(index - 1);
            list.ensureIndexIsVisible(index - 1);
        }
    }
    
    public void acceptSelected() {
        Completion selected = list.getSelectedValue();
        if (selected != null) {
            insertCompletion(selected);
            hide();
        }
    }
    
    private void insertCompletion(Completion completion) {
        try {
            Document doc = textPane.getDocument();
            
            // Remove current prefix
            doc.remove(insertPosition, currentPrefix.length());
            
            // Insert completion
            doc.insertString(insertPosition, completion.getText(), null);
            
            // Position cursor
            int newPos = insertPosition + completion.getCursorOffset();
            textPane.setCaretPosition(newPos);
            
        } catch (BadLocationException e) {
            e.printStackTrace();
        }
    }
    
    public void hide() {
        setVisible(false);
    }
}

class CompletionProvider {
    private final JTextPane textPane;
    private final Map<String, List<Completion>> staticCompletionCache = new HashMap<>();
    
    public CompletionProvider(JTextPane textPane) {
        this.textPane = textPane;
        loadAllStaticCompletions();
    }

    private void loadAllStaticCompletions() {
        // Pre-load static completions for all supported languages
        loadStaticCompletions("java");
        loadStaticCompletions("javascript");
        loadStaticCompletions("python");
        // Add other languages here
    }

    private void loadStaticCompletions(String language) {
        if (staticCompletionCache.containsKey(language)) return;
        
        List<Completion> completions = new ArrayList<>();
        
        switch (language.toLowerCase()) {
            case "java":
                loadJavaCompletions(completions);
                break;
            case "javascript":
                loadJavaScriptCompletions(completions);
                break;
            case "python":
                loadPythonCompletions(completions);
                break;
            default:
                loadGenericCompletions(completions);
        }
        
        staticCompletionCache.put(language, completions);
    }
    
    private void loadJavaCompletions(List<Completion> completions) {
        // Keywords
        String[] keywords = {"abstract", "assert", "boolean", "break", "byte", "case", "catch", 
            "char", "class", "const", "continue", "default", "do", "double", "else", "enum", 
            "extends", "final", "finally", "float", "for", "goto", "if", "implements", 
            "import", "instanceof", "int", "interface", "long", "native", "new", "package", 
            "private", "protected", "public", "return", "short", "static", "strictfp", 
            "super", "switch", "synchronized", "this", "throw", "throws", "transient", 
            "try", "void", "volatile", "while"};
        
        for (String keyword : keywords) {
            completions.add(new Completion(keyword, CompletionType.KEYWORD, keyword));
        }
        
        // Common classes
        String[] classes = {"String", "Integer", "Double", "Boolean", "ArrayList", "HashMap", 
            "HashSet", "StringBuilder", "Scanner", "File", "Date", "Calendar"};
        
        for (String cls : classes) {
            completions.add(new Completion(cls, CompletionType.CLASS, cls));
        }
        
        // Method templates
        completions.add(new Completion("main", CompletionType.TEMPLATE, 
            "public static void main(String[] args) {\n    \n}", 47));
        completions.add(new Completion("sysout", CompletionType.TEMPLATE, 
            "System.out.println();", 20));
        completions.add(new Completion("for", CompletionType.TEMPLATE, 
            "for (int i = 0; i < ; i++) {\n    \n}", 16));
        completions.add(new Completion("if", CompletionType.TEMPLATE, 
            "if () {\n    \n}", 4));
        completions.add(new Completion("try", CompletionType.TEMPLATE, 
            "try {\n    \n} catch (Exception e) {\n    \n}", 10));
    }
    
    private void loadJavaScriptCompletions(List<Completion> completions) {
        String[] keywords = {"var", "let", "const", "function", "return", "if", "else", 
            "for", "while", "do", "switch", "case", "break", "continue", "try", "catch", 
            "finally", "throw", "new", "this", "typeof", "instanceof", "in", "of"};
        
        for (String keyword : keywords) {
            completions.add(new Completion(keyword, CompletionType.KEYWORD, keyword));
        }
        
        // Built-in objects
        String[] objects = {"console", "document", "window", "Array", "Object", "String", 
            "Number", "Boolean", "Date", "Math", "JSON", "Promise"};
        
        for (String obj : objects) {
            completions.add(new Completion(obj, CompletionType.CLASS, obj));
        }
        
        // Templates
        completions.add(new Completion("function", CompletionType.TEMPLATE, 
            "function () {\n    \n}", 9));
        completions.add(new Completion("console.log", CompletionType.TEMPLATE, 
            "console.log();", 12));
    }
    
    private void loadPythonCompletions(List<Completion> completions) {
        String[] keywords = {"and", "as", "assert", "break", "class", "continue", "def", 
            "del", "elif", "else", "except", "exec", "finally", "for", "from", "global", 
            "if", "import", "in", "is", "lambda", "not", "or", "pass", "print", "raise", 
            "return", "try", "while", "with", "yield"};
        
        for (String keyword : keywords) {
            completions.add(new Completion(keyword, CompletionType.KEYWORD, keyword));
        }
        
        // Built-ins
        String[] builtins = {"len", "str", "int", "float", "list", "dict", "tuple", "set", 
            "range", "enumerate", "zip", "map", "filter", "sorted", "reversed"};
        
        for (String builtin : builtins) {
            completions.add(new Completion(builtin, CompletionType.FUNCTION, builtin + "()"));
        }
    }
    
    private void loadGenericCompletions(List<Completion> completions) {
        // Basic programming constructs
        completions.add(new Completion("if", CompletionType.KEYWORD, "if"));
        completions.add(new Completion("for", CompletionType.KEYWORD, "for"));
        completions.add(new Completion("while", CompletionType.KEYWORD, "while"));
        completions.add(new Completion("function", CompletionType.KEYWORD, "function"));
    }
    
    public List<Completion> getCompletions(String prefix, String language, Document doc) {
        List<Completion> allCompletions = new ArrayList<>();
        
        // 1. Add static completions (keywords, templates)
        allCompletions.addAll(staticCompletionCache.getOrDefault(language, new ArrayList<>()));
        
        // 2. Add dynamic completions from the current document (symbols)
        allCompletions.addAll(parseSymbolsFromDocument(doc, language));

        // 3. Filter all completions by the prefix
        List<Completion> matches = new ArrayList<>();
        String lowerPrefix = prefix.toLowerCase();
        
        for (Completion completion : allCompletions) {
            if (completion.getText().toLowerCase().startsWith(lowerPrefix)) {
                matches.add(completion);
            }
        }
        
        // 4. Sort by relevance
        matches.sort((a, b) -> {
            boolean aExact = a.getText().toLowerCase().equals(lowerPrefix);
            boolean bExact = b.getText().toLowerCase().equals(lowerPrefix);
            
            if (aExact && !bExact) return -1;
            if (!aExact && bExact) return 1;
            
            // Prioritize variables and methods over keywords and classes
            if (a.getType().getPriority() != b.getType().getPriority()) {
                return Integer.compare(a.getType().getPriority(), b.getType().getPriority());
            }
            
            return a.getText().compareToIgnoreCase(b.getText());
        });
        
        return matches.subList(0, Math.min(matches.size(), 20)); // Limit results
    }

    private List<Completion> parseSymbolsFromDocument(Document doc, String language) {
        List<Completion> symbols = new ArrayList<>();
        try {
            String text = doc.getText(0, doc.getLength());
            // Simple regex-based parsing. A real IDE would use a proper parser.
            
            // Find variable names (simple version: type and name)
            Pattern varPattern = Pattern.compile("\\b([A-Z][a-zA-Z0-9_]*)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*[=;]");
            Matcher varMatcher = varPattern.matcher(text);
            while (varMatcher.find()) {
                symbols.add(new Completion(varMatcher.group(2), CompletionType.VARIABLE, varMatcher.group(2)));
            }

            // Find method/function names
            Pattern methodPattern = Pattern.compile("(?:public|private|protected|static|\\s)*[\\w<>]+\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
            Matcher methodMatcher = methodPattern.matcher(text);
             while (methodMatcher.find()) {
                String name = methodMatcher.group(1);
                // Avoid matching constructors as methods here
                if (!name.equals(language.equals("java") ? "class" : "")) {
                     symbols.add(new Completion(name, CompletionType.METHOD, name + "()", name.length() + 1));
                }
            }

            // Find class names
            Pattern classPattern = Pattern.compile("class\\s+([A-Z][a-zA-Z0-9_]*)");
            Matcher classMatcher = classPattern.matcher(text);
            while (classMatcher.find()) {
                symbols.add(new Completion(classMatcher.group(1), CompletionType.CLASS, classMatcher.group(1)));
            }

        } catch (BadLocationException e) {
            e.printStackTrace();
        }
        return symbols;
    }
}

class Completion {
    private final String text;
    private final CompletionType type;
    private final String insertText;
    private final int cursorOffset;
    
    public Completion(String text, CompletionType type, String insertText) {
        this(text, type, insertText, insertText.length());
    }
    
    public Completion(String text, CompletionType type, String insertText, int cursorOffset) {
        this.text = text;
        this.type = type;
        this.insertText = insertText;
        this.cursorOffset = cursorOffset;
    }
    
    public String getText() { return text; }
    public CompletionType getType() { return type; }
    public String getInsertText() { return insertText; }
    public int getCursorOffset() { return cursorOffset; }
    
    @Override
    public String toString() {
        return text;
    }
}

enum CompletionType {
    KEYWORD("K", new Color(0xCC7832), 3),
    CLASS("C", new Color(0x9876AA), 2),
    METHOD("M", new Color(0xFFC66D), 1),
    FUNCTION("F", new Color(0xFFC66D), 1),
    VARIABLE("V", new Color(0x6A8759), 0),
    TEMPLATE("T", new Color(0x569CD6), 2);
    
    private final String shortName;
    private final Color color;
    private final int priority; // 0 = highest priority
    
    CompletionType(String shortName, Color color, int priority) {
        this.shortName = shortName;
        this.color = color;
        this.priority = priority;
    }
    
    public String getShortName() { return shortName; }
    public Color getColor() { return color; }
    public int getPriority() { return priority; }
}

class CompletionCellRenderer extends DefaultListCellRenderer {
    @Override
    public Component getListCellRendererComponent(JList<?> list, Object value, int index,
            boolean isSelected, boolean cellHasFocus) {
        super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
        
        if (value instanceof Completion) {
            Completion completion = (Completion) value;
            setText(completion.getText());
            
            // Add type indicator
            String typeIndicator = "[" + completion.getType().getShortName() + "] ";
            setText(typeIndicator + completion.getText());
            
            if (!isSelected) {
                setForeground(completion.getType().getColor());
            }
        }
        
        return this;
    }
}