import java.util.*;
import java.util.concurrent.CompletableFuture;
import java.io.*;
import java.nio.file.*;

public class ChatInterface {
    private final CursorLikeIDE ide;
    private final Map<String, List<String>> chats = new HashMap<>();
    private String activeChat = "main";
    private int chatX = 0;
    private int chatY = 0;
    private int chatWidth = 50;
    
    public ChatInterface(CursorLikeIDE ide) {
        this.ide = ide;
        chats.put("main", new ArrayList<>());
    }
    
    public CompletableFuture<String> send(String message) {
        chats.get(activeChat).add("User: " + message);
        
        return ide.chat(message)
            .thenApply(response -> {
                chats.get(activeChat).add("AI: " + response);
                return response;
            });
    }
    
    public void newChat(String name) {
        chats.put(name, new ArrayList<>());
        activeChat = name;
    }
    
    public void switchChat(String name) {
        if (chats.containsKey(name)) activeChat = name;
    }
    
    public void clear() { chats.get(activeChat).clear(); }
    
    public void deleteChat(String name) {
        if (!name.equals("main") && chats.containsKey(name)) {
            chats.remove(name);
            if (activeChat.equals(name)) activeChat = "main";
        }
    }
    
    public List<String> getHistory() { return new ArrayList<>(chats.get(activeChat)); }
    
    public String[] getChatNames() { return chats.keySet().toArray(new String[0]); }
    
    public void exportChat(String filename) {
        try {
            List<String> lines = new ArrayList<>();
            lines.add("=== " + activeChat + " Chat ===");
            lines.addAll(chats.get(activeChat));
            Files.write(Paths.get(filename + ".txt"), lines);
            System.out.println("[SAVED] Exported to " + filename + ".txt");
        } catch (Exception e) {
            System.out.println("[ERROR] Export failed: " + e.getMessage());
        }
    }
    
    public void importChat(String filename) {
        try {
            List<String> lines = Files.readAllLines(Paths.get(filename));
            String chatName = filename.replace(".txt", "");
            chats.put(chatName, new ArrayList<>(lines.subList(1, lines.size())));
            activeChat = chatName;
            System.out.println("[LOADED] Imported " + chatName);
        } catch (Exception e) {
            System.out.println("[ERROR] Import failed: " + e.getMessage());
        }
    }
    
    public void moveChat(int x, int y) {
        this.chatX = x;
        this.chatY = y;
    }
    
    public void resizeChat(int width) {
        this.chatWidth = width;
    }
    
    public void showChat() {
        // Clear screen and position cursor
        System.out.print("\033[2J\033[H");
        
        // Move to chat position
        for (int i = 0; i < chatY; i++) System.out.println();
        
        String padding = " ".repeat(chatX);
        String border = "=".repeat(chatWidth);
        
        System.out.println(padding + border);
        System.out.println(padding + String.format("%-" + chatWidth + "s", activeChat + " Chat"));
        System.out.println(padding + border);
        
        chats.get(activeChat).forEach(msg -> {
            String wrappedMsg = wrapText(msg, chatWidth - 2);
            System.out.println(padding + "| " + wrappedMsg + " ".repeat(Math.max(0, chatWidth - wrappedMsg.length() - 3)) + "|");
        });
        
        System.out.println(padding + border);
        System.out.println("\nPosition: (" + chatX + "," + chatY + ") Width: " + chatWidth);
        System.out.println("Commands: /move <x> <y>, /resize <width>");
    }
    
    private String wrapText(String text, int width) {
        if (text.length() <= width) return text;
        return text.substring(0, width - 3) + "...";
    }
}