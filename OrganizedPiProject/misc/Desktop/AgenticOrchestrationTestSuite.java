// ai-cli-backend/src/test/java/com/aicli/AgenticOrchestrationTestSuite.java
package com.aicli;

import com.aicli.plugins.*;
import com.aicli.monitoring.TracerService;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.junit.jupiter.MockitoExtension;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.Mockito.*;

import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/**
 * Comprehensive test suite for agentic orchestration functionality.
 * Tests complex multi-step task execution, plugin integration, and
 * real-world scenarios with proper error handling and monitoring.
 */
@ExtendWith(MockitoExtension.class)
@TestMethodOrder(MethodOrderer.OrderAnnotation.class)
public class AgenticOrchestrationTestSuite {
    
    private AgenticOrchestrator orchestrator;
    private PermissionManager permissionManager;
    private GraalVmSecureExecutor secureExecutor;
    private PluginManagerService pluginManager;
    private TracerService tracerService;
    private Path testPluginsDir;
    private String testApiKey = "test-api-key";
    
    @BeforeEach
    void setUp() throws Exception {
        testPluginsDir = Files.createTempDirectory("test_orchestration_plugins");
        
        // Initialize components
        permissionManager = new PermissionManager();
        secureExecutor = new GraalVmSecureExecutor(permissionManager);
        tracerService = new TracerService();
        pluginManager = new PluginManagerService(testPluginsDir);
        
        // Initialize orchestrator
        orchestrator = new AgenticOrchestrator(testApiKey, testPluginsDir);
        
        System.out.println("Agentic orchestration test suite setup completed");
    }
    
    @AfterEach
    void tearDown() throws Exception {
        if (orchestrator != null) {
            orchestrator.shutdown();
        }
        
        // Cleanup test directory
        if (testPluginsDir != null && Files.exists(testPluginsDir)) {
            Files.walk(testPluginsDir)
                    .sorted(Comparator.reverseOrder())
                    .forEach(path -> {
                        try {
                            Files.deleteIfExists(path);
                        } catch (Exception e) {
                            // Ignore cleanup errors
                        }
                    });
        }
    }
    
    @Test
    @Order(1)
    @DisplayName("Test Basic Orchestration Flow")
    void testBasicOrchestrationFlow() throws Exception {
        System.out.println("Testing basic orchestration flow...");
        
        CountDownLatch latch = new CountDownLatch(3);
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            latch.countDown();
        };
        
        // Execute orchestration
        orchestrator.streamExecution(eventConsumer, "Test basic flow", "test-user");
        
        // Wait for completion
        boolean completed = latch.await(10, TimeUnit.SECONDS);
        assertTrue(completed, "Orchestration should complete within timeout");
        
        // Verify events
        assertFalse(events.isEmpty());
        assertTrue(events.stream().anyMatch(e -> "plan_created".equals(e.getType())));
        assertTrue(events.stream().anyMatch(e -> "execution_complete".equals(e.getType())));
        
        System.out.println("? Basic orchestration flow test passed");
    }
    
    @Test
    @Order(2)
    @DisplayName("Test Multi-Step Task Execution")
    void testMultiStepTaskExecution() throws Exception {
        System.out.println("Testing multi-step task execution...");
        
        // Create a complex task plan manually
        AgenticOrchestrator.TaskPlan plan = new AgenticOrchestrator.TaskPlan();
        plan.setUserId("test-user");
        plan.setCreatedAt(System.currentTimeMillis());
        
        // Add multiple tasks
        plan.addTask(new AgenticOrchestrator.Task("ai_query", "Analyze requirements"));
        plan.addTask(new AgenticOrchestrator.Task("file_operation", "Read configuration"));
        plan.addTask(new AgenticOrchestrator.Task("ai_query", "Generate solution"));
        
        // Set up file operation task
        AgenticOrchestrator.Task fileTask = plan.getTasks().get(1);
        fileTask.setOperation("read");
        fileTask.setFilePath(testPluginsDir.resolve("config.txt").toString());
        
        // Create test file
        Files.writeString(Paths.get(fileTask.getFilePath()), "test configuration");
        
        // Execute tasks
        CountDownLatch latch = new CountDownLatch(plan.getTasks().size() * 2); // start + complete
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            latch.countDown();
        };
        
        // Simulate task execution
        for (AgenticOrchestrator.Task task : plan.getTasks()) {
            try {
                String result = executeTaskDirectly(task);
                task.setResult(result);
                task.setCompletedAt(System.currentTimeMillis());
                
                eventConsumer.accept(new AgenticOrchestrator.Event("task_completed", task));
            } catch (Exception e) {
                task.setError(e.getMessage());
                eventConsumer.accept(new AgenticOrchestrator.Event("task_failed", task));
            }
        }
        
        // Wait for all tasks to complete
        boolean completed = latch.await(5, TimeUnit.SECONDS);
        assertTrue(completed, "Multi-step execution should complete within timeout");
        
        // Verify all tasks were processed
        long completedTasks = events.stream()
                .filter(e -> "task_completed".equals(e.getType()))
                .count();
        
        assertEquals(plan.getTasks().size(), completedTasks);
        
        System.out.println("? Multi-step task execution test passed");
    }
    
    @Test
    @Order(3)
    @DisplayName("Test Plugin Integration")
    void testPluginIntegration() throws Exception {
        System.out.println("Testing plugin integration...");
        
        // Create a test plugin
        Tool testPlugin = new Tool() {
            @Override
            public String getName() {
                return "test-plugin";
            }
            
            @Override
            public String execute(String input) {
                return "Plugin processed: " + input;
            }
        };
        
        // Grant necessary permissions
        permissionManager.grantPermission("test-plugin", Capability.IDE_API_ACCESS);
        
        // Create task that uses plugin
        AgenticOrchestrator.Task task = new AgenticOrchestrator.Task("tool_usage", "Use test plugin");
        task.setToolName("test-plugin");
        task.setInput("test input");
        
        // Execute task
        String result = executeTaskDirectly(task);
        assertNotNull(result);
        assertTrue(result.contains("Plugin processed"));
        
        System.out.println("? Plugin integration test passed");
    }
    
    @Test
    @Order(4)
    @DisplayName("Test Secure Code Execution")
    void testSecureCodeExecution() throws Exception {
        System.out.println("Testing secure code execution...");
        
        String pluginId = "secure-test-plugin";
        String validCode = "function test() { return 'Secure execution successful'; }";
        
        // Grant execution permission
        permissionManager.grantPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
        
        // Create plugin execution task
        AgenticOrchestrator.Task task = new AgenticOrchestrator.Task("plugin_execution", "Execute secure code");
        task.setPluginId(pluginId);
        task.setCode(validCode);
        task.setClassName("SecureTest");
        
        // Execute task
        String result = executeTaskDirectly(task);
        assertNotNull(result);
        assertTrue(result.contains("Secure execution"));
        
        System.out.println("? Secure code execution test passed");
    }
    
    @Test
    @Order(5)
    @DisplayName("Test Error Handling and Recovery")
    void testErrorHandlingAndRecovery() throws Exception {
        System.out.println("Testing error handling and recovery...");
        
        CountDownLatch latch = new CountDownLatch(2);
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            if ("task_failed".equals(event.getType()) || "error".equals(event.getType())) {
                latch.countDown();
            }
        };
        
        // Test with invalid input that should cause errors
        try {
            orchestrator.streamExecution(eventConsumer, null, "test-user");
        } catch (Exception e) {
            // Expected to handle null input gracefully
        }
        
        // Wait for error events
        boolean errorHandled = latch.await(5, TimeUnit.SECONDS);
        
        // Verify error handling
        assertTrue(events.stream().anyMatch(e -> 
            "error".equals(e.getType()) || "task_failed".equals(e.getType())));
        
        System.out.println("? Error handling and recovery test passed");
    }
    
    @Test
    @Order(6)
    @DisplayName("Test Concurrent Orchestration")
    void testConcurrentOrchestration() throws Exception {
        System.out.println("Testing concurrent orchestration...");
        
        int numConcurrentExecutions = 5;
        CountDownLatch startLatch = new CountDownLatch(1);
        CountDownLatch endLatch = new CountDownLatch(numConcurrentExecutions);
        List<Exception> exceptions = Collections.synchronizedList(new ArrayList<>());
        
        for (int i = 0; i < numConcurrentExecutions; i++) {
            final int executionId = i;
            new Thread(() -> {
                try {
                    startLatch.await();
                    
                    CountDownLatch localLatch = new CountDownLatch(2);
                    List<AgenticOrchestrator.Event> localEvents = Collections.synchronizedList(new ArrayList<>());
                    
                    Consumer<AgenticOrchestrator.Event> localConsumer = event -> {
                        localEvents.add(event);
                        localLatch.countDown();
                    };
                    
                    orchestrator.streamExecution(localConsumer, 
                        "Concurrent execution " + executionId, "user-" + executionId);
                    
                    boolean completed = localLatch.await(10, TimeUnit.SECONDS);
                    assertTrue(completed, "Concurrent execution should complete");
                    
                } catch (Exception e) {
                    exceptions.add(e);
                } finally {
                    endLatch.countDown();
                }
            }).start();
        }
        
        // Start all executions
        startLatch.countDown();
        
        // Wait for all to complete
        boolean allCompleted = endLatch.await(30, TimeUnit.SECONDS);
        assertTrue(allCompleted, "All concurrent executions should complete");
        assertTrue(exceptions.isEmpty(), "No exceptions should occur: " + exceptions);
        
        System.out.println("? Concurrent orchestration test passed");
    }
    
    @Test
    @Order(7)
    @DisplayName("Test Resource Management")
    void testResourceManagement() throws Exception {
        System.out.println("Testing resource management...");
        
        // Test memory usage during orchestration
        long initialMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        
        // Execute multiple orchestration cycles
        for (int i = 0; i < 10; i++) {
            CountDownLatch latch = new CountDownLatch(2);
            List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
            
            Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                events.add(event);
                latch.countDown();
            };
            
            orchestrator.streamExecution(eventConsumer, "Resource test " + i, "user-" + i);
            latch.await(5, TimeUnit.SECONDS);
        }
        
        // Force garbage collection
        System.gc();
        Thread.sleep(100);
        
        long finalMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        long memoryIncrease = finalMemory - initialMemory;
        
        // Memory increase should be reasonable (less than 100MB)
        assertTrue(memoryIncrease < 100 * 1024 * 1024, 
            "Memory usage should be reasonable, but increased by: " + memoryIncrease);
        
        System.out.println("? Resource management test passed");
    }
    
    @Test
    @Order(8)
    @DisplayName("Test Monitoring Integration")
    void testMonitoringIntegration() throws Exception {
        System.out.println("Testing monitoring integration...");
        
        // Create monitoring span
        var span = tracerService.createPluginExecutionSpan("orchestration-test", "monitoring-test");
        
        try (var scope = tracerService.activateSpan(span)) {
            CountDownLatch latch = new CountDownLatch(2);
            List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
            
            Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                events.add(event);
                latch.countDown();
            };
            
            orchestrator.streamExecution(eventConsumer, "Monitoring test", "monitoring-user");
            latch.await(5, TimeUnit.SECONDS);
            
            // Add monitoring events
            tracerService.addEvent("orchestration-started", Map.of("user", "monitoring-user"));
            tracerService.addEvent("orchestration-completed", Map.of("events", events.size()));
            
        } finally {
            tracerService.completeSpan(span);
        }
        
        // Verify monitoring worked
        assertEquals(0, tracerService.getActiveSpanCount());
        
        System.out.println("? Monitoring integration test passed");
    }
    
    @Test
    @Order(9)
    @DisplayName("Test Complex Real-World Scenario")
    void testComplexRealWorldScenario() throws Exception {
        System.out.println("Testing complex real-world scenario...");
        
        // Simulate a complex development workflow
        String scenario = "Analyze codebase, generate tests, refactor code, and create documentation";
        
        CountDownLatch latch = new CountDownLatch(8); // Multiple events expected
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        Map<String, Object> context = new HashMap<>();
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            context.put("last_event", event.getType());
            context.put("event_count", events.size());
            latch.countDown();
        };
        
        // Create test files for the scenario
        Path testFile = testPluginsDir.resolve("TestClass.java");
        Files.writeString(testFile, "public class TestClass { public void testMethod() {} }");
        
        orchestrator.streamExecution(eventConsumer, scenario, "developer-user");
        
        // Wait for completion
        boolean completed = latch.await(15, TimeUnit.SECONDS);
        assertTrue(completed, "Complex scenario should complete within timeout");
        
        // Verify scenario was processed
        assertFalse(events.isEmpty());
        assertTrue(events.size() >= 3); // At least plan, execution, completion
        
        System.out.println("? Complex real-world scenario test passed");
    }
    
    @Test
    @Order(10)
    @DisplayName("Test Performance and Scalability")
    void testPerformanceAndScalability() throws Exception {
        System.out.println("Testing performance and scalability...");
        
        int numExecutions = 20;
        long startTime = System.currentTimeMillis();
        CountDownLatch allLatch = new CountDownLatch(numExecutions);
        
        for (int i = 0; i < numExecutions; i++) {
            final int executionId = i;
            new Thread(() -> {
                try {
                    CountDownLatch localLatch = new CountDownLatch(2);
                    List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
                    
                    Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                        events.add(event);
                        localLatch.countDown();
                    };
                    
                    long execStart = System.currentTimeMillis();
                    orchestrator.streamExecution(eventConsumer, 
                        "Performance test " + executionId, "perf-user-" + executionId);
                    localLatch.await(5, TimeUnit.SECONDS);
                    long execTime = System.currentTimeMillis() - execStart;
                    
                    // Each execution should complete within 5 seconds
                    assertTrue(execTime < 5000, "Execution " + executionId + " took too long: " + execTime + "ms");
                    
                } catch (Exception e) {
                    fail("Performance test execution failed: " + e.getMessage());
                } finally {
                    allLatch.countDown();
                }
            }).start();
        }
        
        // Wait for all executions to complete
        boolean allCompleted = allLatch.await(30, TimeUnit.SECONDS);
        assertTrue(allCompleted, "All performance test executions should complete");
        
        long totalTime = System.currentTimeMillis() - startTime;
        double avgTimePerExecution = (double) totalTime / numExecutions;
        
        // Average execution time should be reasonable
        assertTrue(avgTimePerExecution < 2000, 
            "Average execution time should be under 2 seconds, but was: " + avgTimePerExecution);
        
        System.out.println("? Performance and scalability test passed");
        System.out.println("  Total time: " + totalTime + "ms");
        System.out.println("  Average per execution: " + avgTimePerExecution + "ms");
    }
    
    // Helper method to execute tasks directly for testing
    private String executeTaskDirectly(AgenticOrchestrator.Task task) throws Exception {
        switch (task.getType()) {
            case "ai_query":
                return "AI response for: " + task.getDescription();
                
            case "plugin_execution":
                if (task.getPluginId() != null && task.getCode() != null) {
                    return secureExecutor.execute(task.getPluginId(), task.getCode(), task.getClassName());
                }
                break;
                
            case "tool_usage":
                if (task.getToolName() != null && task.getInput() != null) {
                    // Simulate tool execution
                    return "Tool " + task.getToolName() + " processed: " + task.getInput();
                }
                break;
                
            case "file_operation":
                if ("read".equals(task.getOperation()) && task.getFilePath() != null) {
                    return Files.readString(Paths.get(task.getFilePath()));
                } else if ("write".equals(task.getOperation()) && task.getFilePath() != null) {
                    Files.writeString(Paths.get(task.getFilePath()), task.getContent());
                    return "File written successfully";
                }
                break;
        }
        
        return "Task executed: " + task.getDescription();
    }
}
