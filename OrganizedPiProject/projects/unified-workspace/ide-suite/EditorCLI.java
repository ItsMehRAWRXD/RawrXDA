import javax.swing.*;
import java.awt.*;
import java.util.Scanner;

public class EditorCLI {
    private static AdvancedCodeEditor editor;
    private static JFrame frame;
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            frame = new JFrame("Advanced Code Editor");
            editor = new AdvancedCodeEditor();
            
            frame.add(editor, BorderLayout.CENTER);
            frame.setSize(800, 600);
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setVisible(true);
        });
        
        Scanner scanner = new Scanner(System.in);
        System.out.println("Editor CLI started. Type 'help' for commands.");
        
        while (true) {
            System.out.print("> ");
            String input = scanner.nextLine().trim();
            
            if (input.equals("exit")) break;
            if (input.equals("help")) {
                showHelp();
                continue;
            }
            
            processCommand(input);
        }
        
        scanner.close();
        System.exit(0);
    }
    
    private static void showHelp() {
        System.out.println("Commands:");
        System.out.println("  help - Show this help");
        System.out.println("  exit - Exit the editor");
        System.out.println("  set <text> - Set editor text");
        System.out.println("  get - Get editor text");
        System.out.println("  clear - Clear editor");
        System.out.println("  load <file> - Load file");
        System.out.println("  save <file> - Save to file");
        System.out.println("  complete <prompt> - AI completion");
        System.out.println("  ping - Test AI connection");
    }
    
    private static void processCommand(String command) {
        String[] parts = command.split(" ", 2);
        String cmd = parts[0];
        
        switch (cmd) {
            case "set":
                if (parts.length > 1) {
                    SwingUtilities.invokeLater(() -> editor.setText(parts[1]));
                    System.out.println("Text set.");
                }
                break;
            case "get":
                System.out.println(editor.getText());
                break;
            case "clear":
                SwingUtilities.invokeLater(() -> editor.setText(""));
                System.out.println("Editor cleared.");
                break;
            case "load":
                if (parts.length > 1) {
                    try {
                        String content = java.nio.file.Files.readString(java.nio.file.Paths.get(parts[1]));
                        SwingUtilities.invokeLater(() -> editor.setText(content));
                        System.out.println("File loaded: " + parts[1]);
                    } catch (Exception e) {
                        System.out.println("Error loading file: " + e.getMessage());
                    }
                }
                break;
            case "save":
                if (parts.length > 1) {
                    try {
                        java.nio.file.Files.writeString(java.nio.file.Paths.get(parts[1]), editor.getText());
                        System.out.println("File saved: " + parts[1]);
                    } catch (Exception e) {
                        System.out.println("Error saving file: " + e.getMessage());
                    }
                }
                break;
            case "complete":
                if (parts.length > 1) {
                    try {
                        CopilotIntegration copilot = new CopilotIntegration();
                        copilot.complete(parts[1]).thenAccept(result -> 
                            System.out.println("Completion: " + result));
                    } catch (Exception e) {
                        System.out.println("AI completion failed: " + e.getMessage());
                    }
                }
                break;
            case "ping":
                try {
                    CopilotIntegration copilot = new CopilotIntegration();
                    copilot.ping().thenAccept(success -> 
                        System.out.println("AI Status: " + (success ? "Connected" : "Disconnected")));
                } catch (Exception e) {
                    System.out.println("Ping failed: " + e.getMessage());
                }
                break;
            default:
                System.out.println("Unknown command: " + cmd);
        }
    }
}