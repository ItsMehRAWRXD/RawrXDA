import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public class WindowManager {
    private final Map<String, ChatWindow> windows = new ConcurrentHashMap<>();
    private final Map<String, Set<String>> activeUsers = new ConcurrentHashMap<>();
    private final Scanner input = new Scanner(System.in);
    private boolean running = true;
    
    public void createWindow(String id, int x, int y, int width, int height) {
        windows.put(id, new ChatWindow(id, x, y, width, height));
        activeUsers.put(id, new HashSet<>(Arrays.asList("You")));
    }
    
    public void moveWindow(String id, int x, int y) {
        ChatWindow window = windows.get(id);
        if (window != null) window.move(x, y);
    }
    
    public void addMessage(String windowId, String user, String message) {
        ChatWindow window = windows.get(windowId);
        if (window != null) {
            window.addMessage(user + ": " + message);
            window.activeUsers.add(user);
        }
    }
    
    public void shareCode(String windowId, String user, String code) {
        ChatWindow window = windows.get(windowId);
        if (window != null) {
            window.addCode(user, code);
            // Broadcast to all joined windows
            windows.values().stream()
                .filter(w -> w.isJoined && w != window)
                .forEach(w -> w.addCode(user, code));
        }
    }
    
    public void joinWindows(String window1, String window2) {
        ChatWindow w1 = windows.get(window1);
        ChatWindow w2 = windows.get(window2);
        if (w1 != null && w2 != null && w1.overlaps(w2)) {
            w1.messages.addAll(w2.messages);
            w1.activeUsers.addAll(w2.activeUsers);
            w1.sharedCode.putAll(w2.sharedCode);
            w1.isJoined = true;
            w2.isJoined = true;
            System.out.println("[JOINED] " + window2 + " linked with " + window1);
        }
    }
    
    public void render() {
        System.out.print("\033[2J\033[H");
        windows.values().forEach(ChatWindow::render);
        System.out.println("\n[WINDOWS] " + String.join(", ", windows.keySet()));
    }
    
    public void applyAIAdvice(String windowId) {
        ChatWindow window = windows.get(windowId);
        if (window != null && !window.aiAdvice.isEmpty()) {
            String lastAdvice = window.aiAdvice.get(window.aiAdvice.size() - 1);
            String lastCode = window.sharedCode.get("You");
            if (lastCode != null) {
                String improved = window.applyAdvice(lastCode, lastAdvice);
                window.sharedCode.put("AI-Applied", improved);
                window.addMessage("[APPLIED] AI suggestion implemented");
            }
        }
    }
    
    public void start() {
        createWindow("main", 5, 2, 40, 10);
        createWindow("ai-1", 50, 5, 35, 8);
        
        System.out.println("Commands: /say <msg>, /code <window> <code>, /auto <window>, /apply <window>");
        System.out.println("         /create <name>, /move <window> <x,y>, /join <w1> <w2>, /quit");
        
        while (running) {
            render();
            System.out.print("> ");
            String cmd = input.nextLine();
            processCommand(cmd);
        }
    }
    
    private void processCommand(String cmd) {
        String[] parts = cmd.split(" ", 3);
        
        switch (parts[0]) {
            case "/create" -> createWindow(parts[1], 10, 3, 30, 6);
            case "/move" -> moveWindow(parts[1], Integer.parseInt(parts[2].split(",")[0]), 
                                     Integer.parseInt(parts[2].split(",")[1]));
            case "/say" -> addMessage("main", "You", parts[1]);
            case "/ai" -> addMessage("ai-1", "AI-Agent", parts[1]);
            case "/code" -> shareCode(parts[1], "You", parts[2]);
            case "/auto" -> windows.get(parts[1]).toggleAutoApply();
            case "/apply" -> applyAIAdvice(parts[1]);
            case "/join" -> joinWindows(parts[1], parts[2]);
            case "/quit" -> running = false;
        }
    }
}