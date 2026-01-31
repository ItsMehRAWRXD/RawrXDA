import java.util.Scanner;

public class SimpleCursor {
    public static void main(String[] args) {
        CursorLikeIDE ide = new CursorLikeIDE();
        InlineAssistant assistant = new InlineAssistant(ide);
        ChatInterface chat = new ChatInterface(ide);
        Scanner scanner = new Scanner(System.in);
        
        System.out.println("Simple Cursor IDE Started");
        System.out.println("Commands: /chat <msg>, /move <x> <y>, /resize <width>, /new <name>, /open <name>");
        System.out.println("         /export <file>, /import <file>, /delete <name>, /switch <provider>");
        System.out.println("         /code <code>, /chats, /history, /quit");
        
        while (true) {
            System.out.print("> ");
            String input = "";
            try {
                input = scanner.nextLine();
            } catch (java.util.NoSuchElementException e) {
                System.out.println("No input available. Exiting...");
                break;
            }
            
            if (input.startsWith("/quit")) break;
            
            if (input.startsWith("/chat ")) {
                chat.send(input.substring(6))
                    .thenAccept(response -> System.out.println("[AI] " + response));
            }
            else if (input.startsWith("/switch ")) {
                ide.switchProvider(input.substring(8));
                System.out.println("[OK] Switched provider");
            }
            else if (input.startsWith("/code ")) {
                assistant.onCodeChange(input.substring(6), 0);
            }
            else if (input.equals("/providers")) {
                System.out.println("[PROVIDERS] " + String.join(", ", ide.getProviders()));
            }
            else if (input.startsWith("/new ")) {
                chat.newChat(input.substring(5));
                System.out.println("? Created chat: " + input.substring(5));
            }
            else if (input.startsWith("/open ")) {
                chat.switchChat(input.substring(6));
                System.out.println("? Opened chat: " + input.substring(6));
            }
            else if (input.equals("/chats")) {
                System.out.println("? " + String.join(", ", chat.getChatNames()));
            }
            else if (input.startsWith("/export ")) {
                chat.exportChat(input.substring(8));
            }
            else if (input.startsWith("/import ")) {
                chat.importChat(input.substring(8));
            }
            else if (input.startsWith("/delete ")) {
                chat.deleteChat(input.substring(8));
                System.out.println("[DELETED] Chat removed");
            }
            else if (input.startsWith("/move ")) {
                String[] coords = input.substring(6).split(" ");
                if (coords.length == 2) {
                    chat.moveChat(Integer.parseInt(coords[0]), Integer.parseInt(coords[1]));
                    System.out.println("[MOVED] Chat repositioned");
                }
            }
            else if (input.startsWith("/resize ")) {
                chat.resizeChat(Integer.parseInt(input.substring(8)));
                System.out.println("[RESIZED] Chat width changed");
            }
            else if (input.equals("/history")) {
                chat.showChat();
            }
        }
        
        ide.shutdown();
        assistant.shutdown();
        System.out.println("Goodbye!");
    }
}