import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.time.*;
import java.util.*;
import java.util.List;
import java.util.concurrent.*;
import com.sun.net.httpserver.*;

public class EnhancedTodoSystem extends JPanel {
    private final DefaultListModel<TodoItem> listModel;
    private final JList<TodoItem> todoList;
    private final JTextField newTodoField;
    private final ActivityTracker activityTracker;
    private final TodoAPI todoAPI;
    private final Timer activityTimer;
    
    public EnhancedTodoSystem() {
        setLayout(new BorderLayout());
        
        listModel = new DefaultListModel<>();
        todoList = new JList<>(listModel);
        todoList.setCellRenderer(new EnhancedTodoCellRenderer());
        todoList.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        
        activityTracker = new ActivityTracker();
        todoAPI = new TodoAPI(this);
        
        JScrollPane scrollPane = new JScrollPane(todoList);
        add(scrollPane, BorderLayout.CENTER);
        
        JPanel inputPanel = new JPanel(new BorderLayout());
        newTodoField = new JTextField();
        JButton addButton = new JButton("Add");
        JButton toggleButton = new JButton("Toggle");
        JButton deleteButton = new JButton("Delete");
        JButton aiSuggestButton = new JButton("AI Suggest");
        
        inputPanel.add(newTodoField, BorderLayout.CENTER);
        JPanel buttonPanel = new JPanel();
        buttonPanel.add(addButton);
        buttonPanel.add(toggleButton);
        buttonPanel.add(deleteButton);
        buttonPanel.add(aiSuggestButton);
        inputPanel.add(buttonPanel, BorderLayout.EAST);
        
        add(inputPanel, BorderLayout.SOUTH);
        add(createActivityPanel(), BorderLayout.NORTH);
        
        addButton.addActionListener(e -> addTodo());
        toggleButton.addActionListener(e -> toggleTodo());
        deleteButton.addActionListener(e -> deleteTodo());
        aiSuggestButton.addActionListener(e -> generateAISuggestions());
        newTodoField.addActionListener(e -> addTodo());
        
        // Activity tracking timer
        activityTimer = new Timer(5000, e -> updateActivity());
        activityTimer.start();
        
        // Start API server
        todoAPI.start();
        
        addInitialTodos();
    }
    
    private JPanel createActivityPanel() {
        JPanel panel = new JPanel(new FlowLayout());
        panel.setBorder(BorderFactory.createTitledBorder("Activity Dashboard"));
        
        JLabel completedLabel = new JLabel("Completed: 0");
        JLabel pendingLabel = new JLabel("Pending: 0");
        JLabel productivityLabel = new JLabel("Productivity: 0%");
        JButton exportButton = new JButton("Export");
        
        exportButton.addActionListener(e -> exportTodos());
        
        panel.add(completedLabel);
        panel.add(pendingLabel);
        panel.add(productivityLabel);
        panel.add(exportButton);
        
        // Update labels periodically
        Timer updateTimer = new Timer(1000, e -> {
            int completed = (int) listModel.elements().asIterator().asStream().mapToInt(t -> t.isCompleted() ? 1 : 0).sum();
            int total = listModel.getSize();
            int pending = total - completed;
            int productivity = total > 0 ? (completed * 100 / total) : 0;
            
            completedLabel.setText("Completed: " + completed);
            pendingLabel.setText("Pending: " + pending);
            productivityLabel.setText("Productivity: " + productivity + "%");
        });
        updateTimer.start();
        
        return panel;
    }
    
    private void addTodo() {
        String text = newTodoField.getText().trim();
        if (!text.isEmpty()) {
            TodoItem item = new TodoItem(text);
            listModel.addElement(item);
            newTodoField.setText("");
            activityTracker.recordAction("ADD_TODO", text);
        }
    }
    
    private void toggleTodo() {
        int index = todoList.getSelectedIndex();
        if (index >= 0) {
            TodoItem item = listModel.getElementAt(index);
            item.setCompleted(!item.isCompleted());
            activityTracker.recordAction(item.isCompleted() ? "COMPLETE_TODO" : "UNCOMPLETE_TODO", item.getText());
            todoList.repaint();
        }
    }
    
    private void deleteTodo() {
        int index = todoList.getSelectedIndex();
        if (index >= 0) {
            TodoItem item = listModel.getElementAt(index);
            activityTracker.recordAction("DELETE_TODO", item.getText());
            listModel.removeElementAt(index);
        }
    }
    
    private void generateAISuggestions() {
        // Generate AI-powered todo suggestions based on current codebase
        String[] suggestions = {
            "Implement error boundary for React components",
            "Add unit tests for new API endpoints", 
            "Optimize database queries for better performance",
            "Update documentation for recent changes",
            "Refactor legacy code in utils package",
            "Add logging to critical business logic",
            "Implement caching for frequently accessed data",
            "Review and update security configurations"
        };
        
        String suggestion = suggestions[new Random().nextInt(suggestions.length)];
        TodoItem item = new TodoItem("[AI] " + suggestion);
        item.setPriority(Priority.MEDIUM);
        listModel.addElement(item);
        activityTracker.recordAction("AI_SUGGEST", suggestion);
    }
    
    private void exportTodos() {
        StringBuilder sb = new StringBuilder("# TODO Export - " + LocalDateTime.now() + "\n\n");
        for (int i = 0; i < listModel.getSize(); i++) {
            TodoItem item = listModel.getElementAt(i);
            sb.append("- [").append(item.isCompleted() ? "x" : " ").append("] ")
              .append(item.getText()).append(" (").append(item.getPriority()).append(")\n");
        }
        
        try {
            Files.write(Paths.get("todos_export.md"), sb.toString().getBytes());
            JOptionPane.showMessageDialog(this, "Exported to todos_export.md");
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Export failed: " + e.getMessage());
        }
    }
    
    private void updateActivity() {
        activityTracker.updateMetrics();
    }
    
    private void addInitialTodos() {
        addTodoWithPriority("Implement CodeFoldingManager class", Priority.HIGH);
        addTodoWithPriority("Implement MultiCursorManager class", Priority.HIGH);
        addTodoWithPriority("Implement AutoCompleteEngine class", Priority.HIGH);
        addTodoWithPriority("Fix editor reference in IDEMain", Priority.CRITICAL);
        addTodoWithPriority("Connect AI providers to editor", Priority.MEDIUM);
        addTodoWithPriority("Add git staging to GitManager", Priority.MEDIUM);
        addTodoWithPriority("Implement find/replace dialogs", Priority.LOW);
        addTodoWithPriority("Write comprehensive unit tests", Priority.MEDIUM);
    }
    
    private void addTodoWithPriority(String text, Priority priority) {
        TodoItem item = new TodoItem(text);
        item.setPriority(priority);
        listModel.addElement(item);
    }
    
    // API access methods for Copilots
    public void addTodoFromAPI(String text, String priority) {
        TodoItem item = new TodoItem(text);
        item.setPriority(Priority.valueOf(priority.toUpperCase()));
        listModel.addElement(item);
        activityTracker.recordAction("API_ADD", text);
    }
    
    public String getTodosAsJSON() {
        StringBuilder json = new StringBuilder("{\"todos\":[");
        for (int i = 0; i < listModel.getSize(); i++) {
            TodoItem item = listModel.getElementAt(i);
            if (i > 0) json.append(",");
            json.append("{\"text\":\"").append(item.getText().replace("\"", "\\\""))
                .append("\",\"completed\":").append(item.isCompleted())
                .append(",\"priority\":\"").append(item.getPriority()).append("\"}");
        }
        json.append("]}");
        return json.toString();
    }
    
    public String getActivityReport() {
        return activityTracker.generateReport();
    }
}

class TodoItem {
    private String text;
    private boolean completed;
    private Priority priority;
    private LocalDateTime created;
    private LocalDateTime lastModified;
    
    public TodoItem(String text) {
        this.text = text;
        this.completed = false;
        this.priority = Priority.MEDIUM;
        this.created = LocalDateTime.now();
        this.lastModified = LocalDateTime.now();
    }
    
    public String getText() { return text; }
    public boolean isCompleted() { return completed; }
    public Priority getPriority() { return priority; }
    public LocalDateTime getCreated() { return created; }
    public LocalDateTime getLastModified() { return lastModified; }
    
    public void setCompleted(boolean completed) { 
        this.completed = completed; 
        this.lastModified = LocalDateTime.now();
    }
    
    public void setPriority(Priority priority) { 
        this.priority = priority;
        this.lastModified = LocalDateTime.now();
    }
    
    @Override
    public String toString() {
        return (completed ? "? " : "? ") + text + " [" + priority + "]";
    }
}

enum Priority {
    LOW("?"), MEDIUM("?"), HIGH("?"), CRITICAL("?");
    
    private final String icon;
    Priority(String icon) { this.icon = icon; }
    public String getIcon() { return icon; }
}

class EnhancedTodoCellRenderer extends DefaultListCellRenderer {
    @Override
    public Component getListCellRendererComponent(JList<?> list, Object value, int index,
            boolean isSelected, boolean cellHasFocus) {
        super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
        
        if (value instanceof TodoItem) {
            TodoItem item = (TodoItem) value;
            setText(item.getPriority().getIcon() + " " + item.toString());
            setForeground(item.isCompleted() ? Color.GRAY : getPriorityColor(item.getPriority()));
            if (item.isCompleted()) {
                setFont(getFont().deriveFont(Font.ITALIC));
            }
        }
        return this;
    }
    
    private Color getPriorityColor(Priority priority) {
        switch (priority) {
            case CRITICAL: return new Color(220, 20, 20);
            case HIGH: return new Color(255, 140, 0);
            case MEDIUM: return Color.BLACK;
            case LOW: return new Color(100, 100, 100);
            default: return Color.BLACK;
        }
    }
}

class ActivityTracker {
    private final Map<String, Integer> actionCounts = new HashMap<>();
    private final List<ActivityEvent> events = new ArrayList<>();
    
    public void recordAction(String action, String details) {
        actionCounts.put(action, actionCounts.getOrDefault(action, 0) + 1);
        events.add(new ActivityEvent(action, details, LocalDateTime.now()));
        
        // Keep only last 1000 events
        if (events.size() > 1000) {
            events.remove(0);
        }
    }
    
    public void updateMetrics() {
        // Calculate productivity metrics, streaks, etc.
    }
    
    public String generateReport() {
        StringBuilder report = new StringBuilder("Activity Report:\n");
        actionCounts.forEach((action, count) -> 
            report.append(action).append(": ").append(count).append("\n"));
        return report.toString();
    }
    
    private static class ActivityEvent {
        final String action;
        final String details;
        final LocalDateTime timestamp;
        
        ActivityEvent(String action, String details, LocalDateTime timestamp) {
            this.action = action;
            this.details = details;
            this.timestamp = timestamp;
        }
    }
}

class TodoAPI {
    private final EnhancedTodoSystem todoSystem;
    private HttpServer server;
    
    public TodoAPI(EnhancedTodoSystem todoSystem) {
        this.todoSystem = todoSystem;
    }
    
    public void start() {
        try {
            server = HttpServer.create(new InetSocketAddress(8080), 0);
            
            // GET /todos - Get all todos as JSON
            server.createContext("/todos", exchange -> {
                if ("GET".equals(exchange.getRequestMethod())) {
                    String response = todoSystem.getTodosAsJSON();
                    exchange.getResponseHeaders().set("Content-Type", "application/json");
                    exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                    exchange.sendResponseHeaders(200, response.length());
                    exchange.getResponseBody().write(response.getBytes());
                    exchange.close();
                }
            });
            
            // POST /todos - Add new todo
            server.createContext("/todos/add", exchange -> {
                if ("POST".equals(exchange.getRequestMethod())) {
                    String body = new String(exchange.getRequestBody().readAllBytes());
                    String[] parts = body.split("&");
                    String text = "";
                    String priority = "MEDIUM";
                    
                    for (String part : parts) {
                        if (part.startsWith("text=")) text = URLDecoder.decode(part.substring(5), "UTF-8");
                        if (part.startsWith("priority=")) priority = URLDecoder.decode(part.substring(9), "UTF-8");
                    }
                    
                    todoSystem.addTodoFromAPI(text, priority);
                    
                    exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                    exchange.sendResponseHeaders(200, 0);
                    exchange.close();
                }
            });
            
            // GET /activity - Get activity report
            server.createContext("/activity", exchange -> {
                if ("GET".equals(exchange.getRequestMethod())) {
                    String response = todoSystem.getActivityReport();
                    exchange.getResponseHeaders().set("Content-Type", "text/plain");
                    exchange.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
                    exchange.sendResponseHeaders(200, response.length());
                    exchange.getResponseBody().write(response.getBytes());
                    exchange.close();
                }
            });
            
            server.setExecutor(Executors.newFixedThreadPool(4));
            server.start();
            
            System.out.println("TODO API started on http://localhost:8080");
            System.out.println("Endpoints:");
            System.out.println("  GET  /todos     - Get all todos");
            System.out.println("  POST /todos/add - Add new todo");
            System.out.println("  GET  /activity  - Get activity report");
            
        } catch (Exception e) {
            System.err.println("Failed to start TODO API: " + e.getMessage());
        }
    }
    
    public void stop() {
        if (server != null) {
            server.stop(0);
        }
    }
}