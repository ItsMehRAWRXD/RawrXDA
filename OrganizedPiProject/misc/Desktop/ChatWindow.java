import java.util.*;

public class ChatWindow {
    public final String id;
    public int x, y, width, height;
    public final List<String> messages = new ArrayList<>();
    public final Set<String> activeUsers = new HashSet<>();
    public final Map<String, String> sharedCode = new HashMap<>();
    public final List<String> aiAdvice = new ArrayList<>();
    public boolean isJoined = false;
    public boolean autoApplyAdvice = true;
    
    public ChatWindow(String id, int x, int y, int width, int height) {
        this.id = id;
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
        activeUsers.add("You");
    }
    
    public void move(int newX, int newY) {
        this.x = newX;
        this.y = newY;
    }
    
    public void addMessage(String message) {
        messages.add(message);
        if (messages.size() > height - 4) messages.remove(0);
    }
    
    public void addCode(String user, String code) {
        sharedCode.put(user, code);
        addMessage("[CODE] " + user + " shared: " + code.substring(0, Math.min(20, code.length())) + "...");
        
        // AI analyzes and gives advice
        if (user.equals("You")) {
            String advice = generateAdvice(code);
            aiAdvice.add(advice);
            addMessage("[AI-ADVICE] " + advice);
            
            if (autoApplyAdvice) {
                String improved = applyAdvice(code, advice);
                sharedCode.put("AI-Improved", improved);
                addMessage("[AUTO-APPLIED] Code updated");
            }
        }
    }
    
    private String generateAdvice(String code) {
        if (code.contains("for")) return "Consider using streams for better readability";
        if (code.contains("null")) return "Add null checks to prevent NPE";
        if (code.contains("System.out")) return "Use proper logging instead of System.out";
        if (code.length() > 100) return "Break this into smaller methods";
        return "Code looks good! Consider adding comments";
    }
    
    public String applyAdvice(String code, String advice) {
        if (advice.contains("null checks")) {
            return code.replaceAll("(\\w+)\\.(\\w+)", "Optional.ofNullable($1).map(x -> x.$2).orElse(null)");
        }
        if (advice.contains("logging")) {
            return code.replace("System.out.println", "logger.info");
        }
        return "// Improved: \n" + code + "\n// COMPLETED
    }
    
    public void toggleAutoApply() {
        autoApplyAdvice = !autoApplyAdvice;
        addMessage("[AUTO-APPLY] " + (autoApplyAdvice ? "ON" : "OFF"));
    }
    
    public boolean overlaps(ChatWindow other) {
        return x < other.x + other.width && x + width > other.x &&
               y < other.y + other.height && y + height > other.y;
    }
    
    public void render() {
        String padding = " ".repeat(x);
        String border = "+" + "-".repeat(width - 2) + "+";
        
        // Position cursor
        System.out.print("\033[" + y + ";" + x + "H");
        
        // Header with users
        System.out.println(padding + border);
        String userList = String.join(",", activeUsers);
        String header = "| " + id + " [" + userList + "]";
        System.out.println(padding + header + " ".repeat(width - header.length() - 1) + "|");
        System.out.println(padding + border);
        
        // Messages
        for (int i = 0; i < height - 4; i++) {
            String msg = i < messages.size() ? messages.get(i) : "";
            if (msg.length() > width - 4) msg = msg.substring(0, width - 7) + "...";
            System.out.println(padding + "| " + msg + " ".repeat(width - msg.length() - 3) + "|");
        }
        
        // Code and advice indicators
        if (!sharedCode.isEmpty()) {
            String codeInfo = "| [CODE: " + sharedCode.size() + "] [ADVICE: " + aiAdvice.size() + "]";
            if (autoApplyAdvice) codeInfo += " [AUTO-APPLY: ON]";
            System.out.println(padding + codeInfo + " ".repeat(Math.max(0, width - codeInfo.length() - 1)) + "|");
        }
        
        System.out.println(padding + border);
    }
}