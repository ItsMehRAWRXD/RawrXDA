// SimpleJavaIde.java - Secure Java IDE with syntax highlighting
// Compile: javac -cp ".;javafx-controls.jar;javafx-fxml.jar" SimpleJavaIde.java
// Run: java -cp ".;javafx-controls.jar;javafx-fxml.jar" SimpleJavaIde

import javafx.application.Application;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.BorderPane;
import javafx.scene.layout.VBox;
import javafx.stage.FileChooser;
import javafx.stage.Stage;
import org.fxmisc.richtext.CodeArea;
import org.fxmisc.richtext.model.StyleSpans;
import org.fxmisc.richtext.model.StyleSpansBuilder;

import javax.tools.JavaCompiler;
import javax.tools.ToolProvider;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Collection;
import java.util.Collections;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class SimpleJavaIde extends Application {

    private static final String[] KEYWORDS = new String[] {
            "abstract", "assert", "boolean", "break", "byte",
            "case", "catch", "char", "class", "const",
            "continue", "default", "do", "double", "else",
            "enum", "extends", "final", "finally", "float",
            "for", "goto", "if", "implements", "import",
            "instanceof", "int", "interface", "long", "native",
            "new", "package", "private", "protected", "public",
            "return", "short", "static", "strictfp", "super",
            "switch", "synchronized", "this", "throw", "throws",
            "transient", "try", "void", "volatile", "while"
    };

    private static final String KEYWORD_PATTERN = "\\b(" + String.join("|", KEYWORDS) + ")\\b";
    private static final String PAREN_PATTERN = "\\(|\\)";
    private static final String BRACE_PATTERN = "\\{|\\}";
    private static final String BRACKET_PATTERN = "\\[|\\]";
    private static final String SEMICOLON_PATTERN = "\\;";
    private static final String STRING_PATTERN = "\"([^\"]|\\\")*\"";
    private static final String COMMENT_PATTERN = "//[^\n]*" + "|" + "/\\*(.|\\R)*?\\*/";

    private static final Pattern PATTERN = Pattern.compile(
            "(?<KEYWORD>" + KEYWORD_PATTERN + ")"
                    + "|(?<PAREN>" + PAREN_PATTERN + ")"
                    + "|(?<BRACE>" + BRACE_PATTERN + ")"
                    + "|(?<BRACKET>" + BRACKET_PATTERN + ")"
                    + "|(?<SEMICOLON>" + SEMICOLON_PATTERN + ")"
                    + "|(?<STRING>" + STRING_PATTERN + ")"
                    + "|(?<COMMENT>" + COMMENT_PATTERN + ")"
    );

    private CodeArea codeArea;
    private TextArea console;
    private Path currentFile;
    private MenuBar menuBar;

    @Override
    public void start(Stage primaryStage) {
        // Create code editor with syntax highlighting
        codeArea = new CodeArea();
        codeArea.setPrefHeight(600);
        codeArea.richChanges()
                .filter(ch -> !ch.getInserted().equals(ch.getRemoved()))
                .subscribe(change -> codeArea.setStyleSpans(0, computeHighlighting(codeArea.getText())));

        // Create console for output
        console = new TextArea();
        console.setEditable(false);
        console.setWrapText(true);
        console.setPrefHeight(200);

        // Create menu bar
        createMenuBar(primaryStage);

        // Create main layout
        BorderPane root = new BorderPane();
        root.setTop(menuBar);
        root.setCenter(codeArea);
        root.setBottom(console);
        
        // Set up scene
        Scene scene = new Scene(root, 1000, 800);
        scene.getStylesheets().add(getClass().getResource("syntax.css").toExternalForm());
        
        primaryStage.setTitle("Secure Java IDE");
        primaryStage.setScene(scene);
        primaryStage.show();
        
        // Add some sample code
        codeArea.appendText("public class HelloWorld {\n");
        codeArea.appendText("    public static void main(String[] args) {\n");
        codeArea.appendText("        System.out.println(\"Hello, World!\");\n");
        codeArea.appendText("    }\n");
        codeArea.appendText("}\n");
    }

    private void createMenuBar(Stage stage) {
        menuBar = new MenuBar();
        
        // File menu
        Menu fileMenu = new Menu("File");
        MenuItem openItem = new MenuItem("Open...");
        openItem.setOnAction(e -> openFile(stage));
        MenuItem saveItem = new MenuItem("Save");
        saveItem.setOnAction(e -> saveFile(stage));
        MenuItem saveAsItem = new MenuItem("Save As...");
        saveAsItem.setOnAction(e -> saveFileAs(stage));
        MenuItem exitItem = new MenuItem("Exit");
        exitItem.setOnAction(e -> stage.close());
        fileMenu.getItems().addAll(openItem, saveItem, saveAsItem, new SeparatorMenuItem(), exitItem);

        // Run menu
        Menu runMenu = new Menu("Run");
        MenuItem compileItem = new MenuItem("Compile");
        compileItem.setOnAction(e -> compileCode());
        MenuItem runItem = new MenuItem("Run");
        runItem.setOnAction(e -> runCode());
        MenuItem compileAndRunItem = new MenuItem("Compile & Run");
        compileAndRunItem.setOnAction(e -> compileAndRun());
        runMenu.getItems().addAll(compileItem, runItem, compileAndRunItem);

        // AI menu
        Menu aiMenu = new Menu("AI Assistant");
        MenuItem refactorItem = new MenuItem("Refactor Code");
        refactorItem.setOnAction(e -> refactorWithAI());
        MenuItem documentItem = new MenuItem("Add Documentation");
        documentItem.setOnAction(e -> documentWithAI());
        MenuItem analyzeItem = new MenuItem("Security Analysis");
        analyzeItem.setOnAction(e -> analyzeWithAI());
        aiMenu.getItems().addAll(refactorItem, documentItem, analyzeItem);

        menuBar.getMenus().addAll(fileMenu, runMenu, aiMenu);
    }

    private void openFile(Stage stage) {
        FileChooser fileChooser = new FileChooser();
        fileChooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Java Files", "*.java"));
        File file = fileChooser.showOpenDialog(stage);
        if (file != null) {
            try {
                codeArea.replaceText(Files.readString(file.toPath()));
                currentFile = file.toPath();
                stage.setTitle("Secure Java IDE - " + file.getName());
                console.appendText("Opened: " + file.getName() + "\n");
            } catch (IOException ex) {
                console.appendText("Error opening file: " + ex.getMessage() + "\n");
            }
        }
    }
    
    private void saveFile(Stage stage) {
        if (currentFile != null) {
            try {
                Files.writeString(currentFile, codeArea.getText());
                console.appendText("Saved: " + currentFile.getFileName() + "\n");
            } catch (IOException ex) {
                console.appendText("Error saving file: " + ex.getMessage() + "\n");
            }
        } else {
            saveFileAs(stage);
        }
    }
    
    private void saveFileAs(Stage stage) {
        FileChooser fileChooser = new FileChooser();
        fileChooser.getExtensionFilters().add(new FileChooser.ExtensionFilter("Java Files", "*.java"));
        File file = fileChooser.showSaveDialog(stage);
        if (file != null) {
            currentFile = file.toPath();
            saveFile(stage);
            stage.setTitle("Secure Java IDE - " + file.getName());
        }
    }

    private void compileCode() {
        if (currentFile == null) {
            console.appendText("Please save the file before compiling.\n");
            return;
        }

        console.clear();
        console.appendText("--- Compiling ---\n");

        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        StringWriter output = new StringWriter();
        int result = compiler.run(null, output, output, currentFile.toString());
        console.appendText(output.toString());

        if (result == 0) {
            console.appendText("--- Compilation successful ---\n");
        } else {
            console.appendText("--- Compilation failed ---\n");
        }
    }

    private void runCode() {
        if (currentFile == null) {
            console.appendText("Please save the file before running.\n");
            return;
        }

        try {
            String className = currentFile.getFileName().toString().replace(".java", "");
            Path classPath = currentFile.getParent();
            ProcessBuilder pb = new ProcessBuilder("java", "-cp", classPath.toString(), className);
            Process p = pb.start();

            String programOutput = new BufferedReader(new InputStreamReader(p.getInputStream()))
                .lines().collect(java.util.stream.Collectors.joining("\n"));
            String programErrors = new BufferedReader(new InputStreamReader(p.getErrorStream()))
                .lines().collect(java.util.stream.Collectors.joining("\n"));

            p.waitFor();

            console.appendText("--- Program Output ---\n");
            console.appendText(programOutput + "\n");
            if (!programErrors.isEmpty()) {
                console.appendText("--- Errors ---\n");
                console.appendText(programErrors + "\n");
            }
        } catch (Exception e) {
            console.appendText("Execution failed: " + e.getMessage() + "\n");
        }
    }

    private void compileAndRun() {
        compileCode();
        if (console.getText().contains("Compilation successful")) {
            runCode();
        }
    }

    private void refactorWithAI() {
        // This would integrate with our AiCli
        console.appendText("AI Refactoring: This feature would use the AiCli tool for intelligent refactoring.\n");
        console.appendText("Example: java -cp .;picocli-4.7.5.jar;javax.json-1.1.4.jar AiCli refactor --file " + 
                          (currentFile != null ? currentFile.toString() : "code.java") + " \"improve code structure\"\n");
    }

    private void documentWithAI() {
        // This would integrate with our AiCli
        console.appendText("AI Documentation: This feature would use the AiCli tool for intelligent documentation.\n");
        console.appendText("Example: java -cp .;picocli-4.7.5.jar;javax.json-1.1.4.jar AiCli document --file " + 
                          (currentFile != null ? currentFile.toString() : "code.java") + " --style javadoc\n");
    }

    private void analyzeWithAI() {
        // This would integrate with our AiCli
        console.appendText("AI Security Analysis: This feature would use the AiCli tool for security analysis.\n");
        console.appendText("Example: java -cp .;picocli-4.7.5.jar;javax.json-1.1.4.jar AiCli analyze-security --file " + 
                          (currentFile != null ? currentFile.toString() : "code.java") + "\n");
    }

    private StyleSpans<Collection<String>> computeHighlighting(String text) {
        Matcher matcher = PATTERN.matcher(text);
        int lastKwEnd = 0;
        StyleSpansBuilder<Collection<String>> spansBuilder = new StyleSpansBuilder<>();
        while (matcher.find()) {
            String styleClass =
                    matcher.group("KEYWORD") != null ? "keyword" :
                    matcher.group("PAREN") != null ? "paren" :
                    matcher.group("BRACE") != null ? "brace" :
                    matcher.group("BRACKET") != null ? "bracket" :
                    matcher.group("SEMICOLON") != null ? "semicolon" :
                    matcher.group("STRING") != null ? "string" :
                    matcher.group("COMMENT") != null ? "comment" :
                    null;
            spansBuilder.add(Collections.emptyList(), matcher.start() - lastKwEnd);
            spansBuilder.add(Collections.singleton(styleClass), matcher.end() - matcher.start());
            lastKwEnd = matcher.end();
        }
        spansBuilder.add(Collections.emptyList(), text.length() - lastKwEnd);
        return spansBuilder.create();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
