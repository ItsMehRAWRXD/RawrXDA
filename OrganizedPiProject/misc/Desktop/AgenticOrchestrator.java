// AgenticOrchestrator.java - Core AI orchestration component
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

import java.io.*;
import java.net.*;
import java.net.http.*;
import java.nio.file.*;
import java.time.Duration;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;
import java.util.logging.Logger;
import java.util.logging.Level;

/**
 * Core AI orchestration component that manages complex multi-step task execution.
 * Provides intelligent task planning, execution coordination, and real-time streaming
 * of execution events to clients.
 * 
 * Features:
 * - AI-powered task planning and decomposition
 * - Multi-step execution with dependency resolution
 * - Real-time event streaming
 * - Plugin integration and management
 * - Secure code execution
 * - Error handling and recovery
 * - Resource management and monitoring
 */
public class AgenticOrchestrator {
    private static final Logger logger = Logger.getLogger(AgenticOrchestrator.class.getName());
    
    // Core configuration
    private final String apiKey;
    private final Path pluginsDirectory;
    private final ObjectMapper objectMapper;
    
    // Core services
    private final PluginManagerService pluginManager;
    private final PermissionManager permissionManager;
    private final GraalVmSecureExecutor secureExecutor;
    private final TracerService tracerService;
    
    // HTTP client for AI API calls
    private final HttpClient httpClient;
    
    // Configuration
    private final int maxRequestsPerTurn;
    
    // Execution state
    private final Map<String, TaskExecution> activeExecutions = new ConcurrentHashMap<>();
    private final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(4);
    
    /**
     * Initialize the orchestrator with API key and plugins directory
     */
    public AgenticOrchestrator(String apiKey, Path pluginsDirectory) {
        this(apiKey, pluginsDirectory, 2);
    }
    
    /**
     * Initialize the orchestrator with API key, plugins directory, and max requests per turn
     */
    public AgenticOrchestrator(String apiKey, Path pluginsDirectory, int maxRequestsPerTurn) {
        this.apiKey = apiKey;
        this.pluginsDirectory = pluginsDirectory;
        this.maxRequestsPerTurn = maxRequestsPerTurn;
        this.objectMapper = new ObjectMapper();
        
        // Initialize services
        this.permissionManager = new PermissionManager();
        this.secureExecutor = new GraalVmSecureExecutor(permissionManager);
        this.pluginManager = new PluginManagerService(pluginsDirectory);
        this.tracerService = new TracerService();
        
        // Initialize HTTP client
        this.httpClient = HttpClient.newBuilder()
                .connectTimeout(Duration.ofSeconds(30))
                .build();
        
        logger.info("AgenticOrchestrator initialized successfully");
    }
    
    /**
     * Stream execution of a complex task with real-time events
     */
    public void streamExecution(Consumer<Event> eventConsumer, String prompt, String userId) {
        String executionId = UUID.randomUUID().toString();
        
        try {
            // Create execution context
            TaskExecution execution = new TaskExecution(executionId, userId, prompt);
            activeExecutions.put(executionId, execution);
            
            // Start execution in background
            scheduler.submit(() -> {
                executeWithEvents(execution, eventConsumer);
            });
            
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Failed to start orchestration execution", e);
            eventConsumer.accept(new Event("error", null, Map.of("error", e.getMessage())));
        }
    }
    
    /**
     * Continue execution of remaining tasks after confirmation
     */
    public void continueExecution(String executionId, Consumer<Event> eventConsumer) {
        TaskExecution execution = activeExecutions.get(executionId);
        if (execution == null) {
            eventConsumer.accept(new Event("error", null, Map.of("error", "Execution not found: " + executionId)));
            return;
        }
        
        // Continue from where it left off
        scheduler.submit(() -> {
            continueWithEvents(execution, eventConsumer);
        });
    }
    
    /**
     * Continue execution from where it left off
     */
    private void continueWithEvents(TaskExecution execution, Consumer<Event> eventConsumer) {
        TaskPlan plan = execution.getPlan();
        if (plan == null) {
            eventConsumer.accept(new Event("error", null, Map.of("error", "No plan found for execution")));
            return;
        }
        
        // Find the next task to execute
        List<Task> tasks = plan.getTasks();
        int startIndex = 0;
        for (int i = 0; i < tasks.size(); i++) {
            if (!tasks.get(i).isCompleted() && !tasks.get(i).hasError()) {
                startIndex = i;
                break;
            }
        }
        
        executeWithEventsFromIndex(execution, eventConsumer, startIndex);
    }
    
    /**
     * Execute task with event streaming
     */
    private void executeWithEvents(TaskExecution execution, Consumer<Event> eventConsumer) {
        executeWithEventsFromIndex(execution, eventConsumer, 0);
    }
    
    /**
     * Execute task with event streaming from a specific index
     */
    private void executeWithEventsFromIndex(TaskExecution execution, Consumer<Event> eventConsumer, int startIndex) {
        var span = tracerService.createPluginExecutionSpan("orchestration", execution.getExecutionId());
        
        try (var scope = tracerService.activateSpan(span)) {
            TaskPlan plan = execution.getPlan();
            
            if (startIndex == 0) {
                // Step 1: Create execution plan
                eventConsumer.accept(new Event("plan_created", null, Map.of(
                    "executionId", execution.getExecutionId(),
                    "userId", execution.getUserId()
                )));
                
                plan = createExecutionPlan(execution.getPrompt(), execution.getUserId());
                execution.setPlan(plan);
                
                eventConsumer.accept(new Event("plan_ready", plan, Map.of(
                    "taskCount", plan.getTasks().size(),
                    "estimatedDuration", plan.estimateDuration()
                )));
            }
            
            // Step 2: Execute tasks from startIndex
            List<Task> tasks = plan.getTasks();
            int executedCount = startIndex;
            for (int i = startIndex; i < tasks.size(); i++) {
                Task task = tasks.get(i);
                if (executedCount >= maxRequestsPerTurn + startIndex) {
                    // Emit confirmation event
                    eventConsumer.accept(new Event("confirm_continue", null, Map.of(
                        "executed", executedCount,
                        "remaining", tasks.size() - executedCount,
                        "message", "Continue to iterate? " + (tasks.size() - executedCount) + " tasks remaining.",
                        "executionId", execution.getExecutionId()
                    )));
                    break;
                }
                
                try {
                    eventConsumer.accept(new Event("task_started", task, Map.of(
                        "taskId", task.getId(),
                        "type", task.getType()
                    )));
                    
                    String result = executeTask(task);
                    task.setResult(result);
                    task.setCompletedAt(System.currentTimeMillis());
                    
                    eventConsumer.accept(new Event("task_completed", task, Map.of(
                        "taskId", task.getId(),
                        "result", result
                    )));
                    
                    executedCount++;
                    
                } catch (Exception e) {
                    task.setError(e.getMessage());
                    task.setCompletedAt(System.currentTimeMillis());
                    
                    eventConsumer.accept(new Event("task_failed", task, Map.of(
                        "taskId", task.getId(),
                        "error", e.getMessage()
                    )));
                }
            }
            
            // Step 3: Complete execution only if all tasks executed
            if (executedCount >= plan.getTasks().size()) {
                execution.setCompletedAt(System.currentTimeMillis());
                eventConsumer.accept(new Event("execution_complete", execution, Map.of(
                    "executionId", execution.getExecutionId(),
                    "totalTasks", plan.getTasks().size(),
                    "completedTasks", plan.getTasks().stream().mapToInt(t -> t.isCompleted() ? 1 : 0).sum()
                )));
            }
            
        } catch (Exception e) {
            logger.log(Level.SEVERE, "Execution failed", e);
            eventConsumer.accept(new Event("error", null, Map.of("error", e.getMessage())));
        } finally {
            tracerService.completeSpan(span);
            activeExecutions.remove(execution.getExecutionId());
        }
    }
    
    /**
     * Create an intelligent execution plan from a prompt
     */
    private TaskPlan createExecutionPlan(String prompt, String userId) throws Exception {
        // Use AI to analyze prompt and create execution plan
        String planPrompt = String.format(
            "Analyze the following request and create a detailed execution plan with specific tasks:\n\n" +
            "Request: %s\n\n" +
            "Create a JSON plan with tasks that include:\n" +
            "- type: ai_query, file_operation, plugin_execution, tool_usage\n" +
            "- description: clear description of what to do\n" +
            "- dependencies: array of task IDs this depends on\n" +
            "- priority: 1-10 (higher = more important)\n\n" +
            "Return only valid JSON.",
            prompt
        );
        
        String aiResponse = callAI(planPrompt);
        return parseTaskPlan(aiResponse, userId);
    }
    
    /**
     * Execute a single task
     */
    private String executeTask(Task task) throws Exception {
        var span = tracerService.createPluginExecutionSpan("task-execution", task.getId());
        
        try (var scope = tracerService.activateSpan(span)) {
            switch (task.getType()) {
                case "ai_query":
                    return executeAIQuery(task);
                    
                case "file_operation":
                    return executeFileOperation(task);
                    
                case "plugin_execution":
                    return executePluginCode(task);
                    
                case "tool_usage":
                    return executeToolUsage(task);
                    
                default:
                    throw new IllegalArgumentException("Unknown task type: " + task.getType());
            }
        } finally {
            tracerService.completeSpan(span);
        }
    }
    
    /**
     * Execute AI query task
     */
    private String executeAIQuery(Task task) throws Exception {
        String prompt = task.getDescription();
        if (task.getInput() != null) {
            prompt += "\n\nInput: " + task.getInput();
        }
        
        return callAI(prompt);
    }
    
    /**
     * Execute file operation task
     */
    private String executeFileOperation(Task task) throws Exception {
        String operation = task.getOperation();
        String filePath = task.getFilePath();
        
        if (filePath == null) {
            throw new IllegalArgumentException("File path required for file operations");
        }
        
        Path path = Paths.get(filePath);
        
        switch (operation) {
            case "read":
                return Files.readString(path);
                
            case "write":
                String content = task.getContent();
                if (content == null) {
                    content = task.getInput();
                }
                Files.writeString(path, content);
                return "File written successfully";
                
            case "create":
                Files.createDirectories(path.getParent());
                Files.createFile(path);
                return "File created successfully";
                
            case "delete":
                Files.deleteIfExists(path);
                return "File deleted successfully";
                
            default:
                throw new IllegalArgumentException("Unknown file operation: " + operation);
        }
    }
    
    /**
     * Execute plugin code
     */
    private String executePluginCode(Task task) throws Exception {
        String pluginId = task.getPluginId();
        String code = task.getCode();
        String className = task.getClassName();
        
        if (pluginId == null || code == null || className == null) {
            throw new IllegalArgumentException("Plugin ID, code, and class name required");
        }
        
        // Check permissions
        if (!permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND)) {
            throw new SecurityException("Plugin " + pluginId + " lacks execution permission");
        }
        
        return secureExecutor.execute(pluginId, code, className);
    }
    
    /**
     * Execute tool usage
     */
    private String executeToolUsage(Task task) throws Exception {
        String toolName = task.getToolName();
        String input = task.getInput();
        
        if (toolName == null) {
            throw new IllegalArgumentException("Tool name required");
        }
        
        // Find and execute tool
        Optional<Tool> tool = pluginManager.getTools().stream()
                .filter(t -> t.getName().equals(toolName))
                .findFirst();
        
        if (tool.isEmpty()) {
            throw new IllegalArgumentException("Tool not found: " + toolName);
        }
        
        return tool.get().execute(input);
    }
    
    /**
     * Call AI API with prompt
     */
    private String callAI(String prompt) throws Exception {
        String url = "https://api.openai.com/v1/chat/completions";
        
        Map<String, Object> requestBody = Map.of(
            "model", "gpt-3.5-turbo",
            "messages", Arrays.asList(
                Map.of("role", "user", "content", prompt)
            ),
            "max_tokens", 2048,
            "temperature", 0.7
        );
        
        String jsonBody = objectMapper.writeValueAsString(requestBody);
        
        HttpRequest request = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .header("Authorization", "Bearer " + apiKey)
                .header("Content-Type", "application/json")
                .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
                .build();
        
        HttpResponse<String> response = httpClient.send(request, HttpResponse.BodyHandlers.ofString());
        
        if (response.statusCode() != 200) {
            throw new IOException("AI API call failed: " + response.statusCode() + " " + response.body());
        }
        
        JsonNode responseNode = objectMapper.readTree(response.body());
        return responseNode.get("choices").get(0).get("message").get("content").asText();
    }
    
    /**
     * Parse AI response into task plan
     */
    private TaskPlan parseTaskPlan(String aiResponse, String userId) throws Exception {
        try {
            // Try to extract JSON from response
            String jsonStr = extractJsonFromResponse(aiResponse);
            JsonNode planNode = objectMapper.readTree(jsonStr);
            
            TaskPlan plan = new TaskPlan();
            plan.setUserId(userId);
            plan.setCreatedAt(System.currentTimeMillis());
            
            JsonNode tasksNode = planNode.get("tasks");
            if (tasksNode != null && tasksNode.isArray()) {
                for (JsonNode taskNode : tasksNode) {
                    Task task = new Task(
                        taskNode.get("type").asText(),
                        taskNode.get("description").asText()
                    );
                    
                    if (taskNode.has("priority")) {
                        task.setPriority(taskNode.get("priority").asInt());
                    }
                    
                    if (taskNode.has("dependencies")) {
                        List<String> deps = new ArrayList<>();
                        for (JsonNode dep : taskNode.get("dependencies")) {
                            deps.add(dep.asText());
                        }
                        task.setDependencies(deps);
                    }
                    
                    plan.addTask(task);
                }
            }
            
            return plan;
            
        } catch (Exception e) {
            // Fallback: create a simple plan
            logger.log(Level.WARNING, "Failed to parse AI plan, creating fallback", e);
            return createFallbackPlan(aiResponse, userId);
        }
    }
    
    /**
     * Extract JSON from AI response
     */
    private String extractJsonFromResponse(String response) {
        // Find JSON block in response
        int start = response.indexOf("{");
        int end = response.lastIndexOf("}");
        
        if (start != -1 && end != -1 && end > start) {
            return response.substring(start, end + 1);
        }
        
        return response;
    }
    
    /**
     * Create fallback plan when AI parsing fails
     */
    private TaskPlan createFallbackPlan(String prompt, String userId) {
        TaskPlan plan = new TaskPlan();
        plan.setUserId(userId);
        plan.setCreatedAt(System.currentTimeMillis());
        
        // Create a simple plan with AI query task
        Task task = new Task("ai_query", "Process request: " + prompt);
        task.setPriority(10);
        plan.addTask(task);
        
        return plan;
    }
    
    /**
     * Shutdown orchestrator and cleanup resources
     */
    public void shutdown() {
        logger.info("Shutting down AgenticOrchestrator...");
        
        scheduler.shutdown();
        try {
            if (!scheduler.awaitTermination(10, TimeUnit.SECONDS)) {
                scheduler.shutdownNow();
            }
        } catch (InterruptedException e) {
            scheduler.shutdownNow();
            Thread.currentThread().interrupt();
        }
        
        if (secureExecutor != null) {
            secureExecutor.shutdown();
        }
        
        if (tracerService != null) {
            tracerService.cleanup();
        }
        
        logger.info("AgenticOrchestrator shutdown complete");
    }
    
    // Inner classes for data structures
    
    /**
     * Represents a task in the execution plan
     */
    public static class Task {
        private String id;
        private String type;
        private String description;
        private String result;
        private String error;
        private long completedAt;
        private int priority = 5;
        private List<String> dependencies = new ArrayList<>();
        
        // Additional fields for specific task types
        private String input;
        private String operation;
        private String filePath;
        private String content;
        private String pluginId;
        private String code;
        private String className;
        private String toolName;
        
        public Task() {
            this.id = UUID.randomUUID().toString();
        }
        
        public Task(String type, String description) {
            this();
            this.type = type;
            this.description = description;
        }

        // Getters and setters
        public String getId() { return id; }
        public void setId(String id) { this.id = id; }
        
        public String getType() { return type; }
        public void setType(String type) { this.type = type; }
        
        public String getDescription() { return description; }
        public void setDescription(String description) { this.description = description; }
        
        public String getResult() { return result; }
        public void setResult(String result) { this.result = result; }
        
        public String getError() { return error; }
        public void setError(String error) { this.error = error; }
        
        public long getCompletedAt() { return completedAt; }
        public void setCompletedAt(long completedAt) { this.completedAt = completedAt; }
        
        public int getPriority() { return priority; }
        public void setPriority(int priority) { this.priority = priority; }
        
        public List<String> getDependencies() { return dependencies; }
        public void setDependencies(List<String> dependencies) { this.dependencies = dependencies; }
        
        public String getInput() { return input; }
        public void setInput(String input) { this.input = input; }
        
        public String getOperation() { return operation; }
        public void setOperation(String operation) { this.operation = operation; }
        
        public String getFilePath() { return filePath; }
        public void setFilePath(String filePath) { this.filePath = filePath; }
        
        public String getContent() { return content; }
        public void setContent(String content) { this.content = content; }
        
        public String getPluginId() { return pluginId; }
        public void setPluginId(String pluginId) { this.pluginId = pluginId; }
        
        public String getCode() { return code; }
        public void setCode(String code) { this.code = code; }
        
        public String getClassName() { return className; }
        public void setClassName(String className) { this.className = className; }
        
        public String getToolName() { return toolName; }
        public void setToolName(String toolName) { this.toolName = toolName; }
        
        public boolean isCompleted() { return completedAt > 0; }
        public boolean hasError() { return error != null && !error.isEmpty(); }
    }
    
    /**
     * Represents a complete execution plan
     */
    public static class TaskPlan {
        private String userId;
        private long createdAt;
        private long estimatedDuration;
        private List<Task> tasks = new ArrayList<>();
        
        // Getters and setters
        public String getUserId() { return userId; }
        public void setUserId(String userId) { this.userId = userId; }
        
        public long getCreatedAt() { return createdAt; }
        public void setCreatedAt(long createdAt) { this.createdAt = createdAt; }
        
        public long getEstimatedDuration() { return estimatedDuration; }
        public void setEstimatedDuration(long estimatedDuration) { this.estimatedDuration = estimatedDuration; }
        
        public List<Task> getTasks() { return tasks; }
        public void setTasks(List<Task> tasks) { this.tasks = tasks; }
        
        public void addTask(Task task) { tasks.add(task); }
        
        public long estimateDuration() {
            // Simple estimation based on task count and types
            return tasks.size() * 2000L; // 2 seconds per task
        }
    }
    
    /**
     * Represents an execution event
     */
    public static class Event {
        private String type;
        private Object data;
        private Map<String, Object> metadata;
        private long timestamp;
        
        public Event(String type, Object data, Map<String, Object> metadata) {
            this.type = type;
            this.data = data;
            this.metadata = metadata != null ? metadata : new HashMap<>();
            this.timestamp = System.currentTimeMillis();
        }
        
        // Getters and setters
        public String getType() { return type; }
        public void setType(String type) { this.type = type; }
        
        public Object getData() { return data; }
        public void setData(Object data) { this.data = data; }
        
        public Map<String, Object> getMetadata() { return metadata; }
        public void setMetadata(Map<String, Object> metadata) { this.metadata = metadata; }
        
        public long getTimestamp() { return timestamp; }
        public void setTimestamp(long timestamp) { this.timestamp = timestamp; }
    }
    
    /**
     * Represents an active task execution
     */
    private static class TaskExecution {
        private String executionId;
        private String userId;
        private String prompt;
        private TaskPlan plan;
        private long startedAt;
        private long completedAt;
        
        public TaskExecution(String executionId, String userId, String prompt) {
            this.executionId = executionId;
            this.userId = userId;
            this.prompt = prompt;
            this.startedAt = System.currentTimeMillis();
        }
        
        // Getters and setters
        public String getExecutionId() { return executionId; }
        public String getUserId() { return userId; }
        public String getPrompt() { return prompt; }
        
        public TaskPlan getPlan() { return plan; }
        public void setPlan(TaskPlan plan) { this.plan = plan; }
        
        public long getStartedAt() { return startedAt; }
        public long getCompletedAt() { return completedAt; }
        public void setCompletedAt(long completedAt) { this.completedAt = completedAt; }
        
        public Object getData() { return this; }
    }
}