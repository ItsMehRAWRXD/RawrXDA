// ai-cli-backend/src/test/java/com/aicli/CompleteSystemIntegrationTest.java
package com.aicli;

import com.aicli.plugins.*;
import com.aicli.monitoring.TracerService;
import com.aicli.server.MainServer;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.junit.jupiter.MockitoExtension;
import static org.junit.jupiter.api.Assertions.*;

import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/**
 * Complete system integration test that verifies all components
 * work together in a real-world scenario. Tests the entire AI CLI
 * platform from HTTP API to plugin execution to LSP integration.
 */
@ExtendWith(MockitoExtension.class)
@TestMethodOrder(MethodOrderer.OrderAnnotation.class)
public class CompleteSystemIntegrationTest {
    
    private MainServer mainServer;
    private AgenticOrchestrator orchestrator;
    private PermissionManager permissionManager;
    private GraalVmSecureExecutor secureExecutor;
    private PluginManagerService pluginManager;
    private TracerService tracerService;
    private Path testPluginsDir;
    private Path testWorkspaceDir;
    
    private static final int HTTP_PORT = 8081; // Use different port for testing
    private static final String TEST_API_KEY = "test-api-key-integration";
    
    @BeforeEach
    void setUp() throws Exception {
        // Create test directories
        testPluginsDir = Files.createTempDirectory("test_system_plugins");
        testWorkspaceDir = Files.createTempDirectory("test_workspace");
        
        // Initialize core components
        permissionManager = new PermissionManager();
        secureExecutor = new GraalVmSecureExecutor(permissionManager);
        tracerService = new TracerService();
        pluginManager = new PluginManagerService(testPluginsDir);
        orchestrator = new AgenticOrchestrator(TEST_API_KEY, testPluginsDir);
        
        // Start main server (this would normally be done in a separate process)
        // For testing, we'll simulate the server components
        System.setProperty("user.dir", testWorkspaceDir.toString());
        
        System.out.println("Complete system integration test setup completed");
    }
    
    @AfterEach
    void tearDown() throws Exception {
        // Cleanup all components
        if (orchestrator != null) {
            orchestrator.shutdown();
        }
        if (pluginManager != null) {
            pluginManager.stopAndUnload();
        }
        if (secureExecutor != null) {
            secureExecutor.shutdown();
        }
        if (tracerService != null) {
            tracerService.cleanup();
        }
        
        // Cleanup test directories
        cleanupDirectory(testPluginsDir);
        cleanupDirectory(testWorkspaceDir);
        
        System.out.println("Complete system integration test cleanup completed");
    }
    
    @Test
    @Order(1)
    @DisplayName("Test Complete Security Architecture Integration")
    void testCompleteSecurityArchitectureIntegration() throws Exception {
        System.out.println("Testing complete security architecture integration...");
        
        String pluginId = "integration-security-plugin";
        
        // Test 1: Permission management
        assertFalse(permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND));
        permissionManager.grantPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
        assertTrue(permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND));
        
        // Test 2: Secure execution with permissions
        String testCode = "function securityTest() { return 'Security integration successful'; }";
        GraalVmSecureExecutor.ExecutionResult result = secureExecutor.execute(pluginId, testCode, "SecurityTest", null);
        assertTrue(result.isSuccess());
        assertNotNull(result.getResult());
        
        // Test 3: Plugin management with security context
        pluginManager.grantCapability(pluginId, Capability.IDE_API_ACCESS);
        assertTrue(permissionManager.hasPermission(pluginId, Capability.IDE_API_ACCESS));
        
        // Test 4: Distributed tracing integration
        var span = tracerService.createPluginExecutionSpan(pluginId, "security-integration-test");
        try (var scope = tracerService.activateSpan(span)) {
            tracerService.addEvent("security-check-completed", Map.of("plugin", pluginId));
        } finally {
            tracerService.completeSpan(span);
        }
        
        System.out.println("? Complete security architecture integration test passed");
    }
    
    @Test
    @Order(2)
    @DisplayName("Test Agentic Orchestration with All Components")
    void testAgenticOrchestrationWithAllComponents() throws Exception {
        System.out.println("Testing agentic orchestration with all components...");
        
        // Create test files for orchestration
        Path testFile = testWorkspaceDir.resolve("TestCode.java");
        Files.writeString(testFile, "public class TestCode { public void testMethod() {} }");
        
        // Create a complex orchestration scenario
        String complexScenario = "Analyze TestCode.java, generate unit tests, and create documentation";
        
        CountDownLatch latch = new CountDownLatch(5);
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        Map<String, Object> executionContext = new HashMap<>();
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            executionContext.put("event_" + events.size(), event);
            latch.countDown();
        };
        
        // Execute orchestration with monitoring
        var span = tracerService.createPluginExecutionSpan("orchestrator", "complete-integration-test");
        try (var scope = tracerService.activateSpan(span)) {
            orchestrator.streamExecution(eventConsumer, complexScenario, "integration-user");
            
            // Wait for completion
            boolean completed = latch.await(15, TimeUnit.SECONDS);
            assertTrue(completed, "Orchestration should complete within timeout");
            
            // Verify orchestration events
            assertFalse(events.isEmpty());
            assertTrue(events.stream().anyMatch(e -> "plan_created".equals(e.getType())));
            assertTrue(events.stream().anyMatch(e -> "execution_complete".equals(e.getType())));
            
            tracerService.addEvent("orchestration-completed", Map.of("events", events.size()));
        } finally {
            tracerService.completeSpan(span);
        }
        
        System.out.println("? Agentic orchestration with all components test passed");
    }
    
    @Test
    @Order(3)
    @DisplayName("Test Plugin Lifecycle Management")
    void testPluginLifecycleManagement() throws Exception {
        System.out.println("Testing plugin lifecycle management...");
        
        // Test plugin loading
        List<Tool> initialPlugins = pluginManager.getPlugins();
        assertNotNull(initialPlugins);
        
        // Test plugin capability management
        String testPluginId = "lifecycle-test-plugin";
        pluginManager.grantCapability(testPluginId, Capability.FILE_READ_ACCESS);
        pluginManager.grantCapability(testPluginId, Capability.EXECUTE_SYSTEM_COMMAND);
        
        // Verify capabilities were granted
        assertTrue(permissionManager.hasPermission(testPluginId, Capability.FILE_READ_ACCESS));
        assertTrue(permissionManager.hasPermission(testPluginId, Capability.EXECUTE_SYSTEM_COMMAND));
        
        // Test plugin execution through orchestrator
        AgenticOrchestrator.Task task = new AgenticOrchestrator.Task("plugin_execution", "Test plugin lifecycle");
        task.setPluginId(testPluginId);
        task.setCode("function lifecycleTest() { return 'Plugin lifecycle successful'; }");
        task.setClassName("LifecycleTest");
        
        String result = executeTaskThroughOrchestrator(task);
        assertNotNull(result);
        assertTrue(result.contains("Plugin lifecycle"));
        
        System.out.println("? Plugin lifecycle management test passed");
    }
    
    @Test
    @Order(4)
    @DisplayName("Test Multi-Threaded System Operations")
    void testMultiThreadedSystemOperations() throws Exception {
        System.out.println("Testing multi-threaded system operations...");
        
        int numThreads = 10;
        CountDownLatch startLatch = new CountDownLatch(1);
        CountDownLatch endLatch = new CountDownLatch(numThreads);
        List<Exception> exceptions = Collections.synchronizedList(new ArrayList<>());
        
        for (int i = 0; i < numThreads; i++) {
            final int threadId = i;
            new Thread(() -> {
                try {
                    startLatch.await();
                    
                    String pluginId = "thread-plugin-" + threadId;
                    
                    // Test permission management
                    permissionManager.grantPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND);
                    assertTrue(permissionManager.hasPermission(pluginId, Capability.EXECUTE_SYSTEM_COMMAND));
                    
                    // Test secure execution
                    String code = "function thread" + threadId + "() { return 'Thread " + threadId + " success'; }";
                    var result = secureExecutor.execute(pluginId, code, "ThreadTest" + threadId, null);
                    assertTrue(result.isSuccess());
                    
                    // Test orchestration
                    CountDownLatch localLatch = new CountDownLatch(2);
                    List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
                    
                    Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                        events.add(event);
                        localLatch.countDown();
                    };
                    
                    orchestrator.streamExecution(eventConsumer, "Thread " + threadId + " test", "user-" + threadId);
                    localLatch.await(5, TimeUnit.SECONDS);
                    
                    // Test tracing
                    var span = tracerService.createPluginExecutionSpan(pluginId, "thread-test");
                    tracerService.completeSpan(span);
                    
                } catch (Exception e) {
                    exceptions.add(e);
                } finally {
                    endLatch.countDown();
                }
            }).start();
        }
        
        // Start all threads
        startLatch.countDown();
        
        // Wait for completion
        boolean allCompleted = endLatch.await(30, TimeUnit.SECONDS);
        assertTrue(allCompleted, "All threaded operations should complete");
        assertTrue(exceptions.isEmpty(), "No exceptions should occur: " + exceptions);
        
        System.out.println("? Multi-threaded system operations test passed");
    }
    
    @Test
    @Order(5)
    @DisplayName("Test Resource Management and Cleanup")
    void testResourceManagementAndCleanup() throws Exception {
        System.out.println("Testing resource management and cleanup...");
        
        // Test initial resource state
        long initialMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        int initialSpanCount = tracerService.getActiveSpanCount();
        
        // Create and use resources
        List<var> spans = new ArrayList<>();
        for (int i = 0; i < 10; i++) {
            var span = tracerService.createPluginExecutionSpan("resource-test-" + i, "cleanup-test");
            spans.add(span);
        }
        
        // Verify resources are active
        assertEquals(10, tracerService.getActiveSpanCount());
        
        // Create temporary files
        List<Path> tempFiles = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            Path tempFile = testWorkspaceDir.resolve("temp-" + i + ".txt");
            Files.writeString(tempFile, "Temporary content " + i);
            tempFiles.add(tempFile);
        }
        
        // Execute orchestration cycles
        for (int i = 0; i < 5; i++) {
            CountDownLatch latch = new CountDownLatch(2);
            List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
            
            Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                events.add(event);
                latch.countDown();
            };
            
            orchestrator.streamExecution(eventConsumer, "Resource test " + i, "resource-user-" + i);
            latch.await(3, TimeUnit.SECONDS);
        }
        
        // Cleanup resources
        for (var span : spans) {
            tracerService.completeSpan(span);
        }
        
        for (Path tempFile : tempFiles) {
            Files.deleteIfExists(tempFile);
        }
        
        // Force cleanup
        System.gc();
        Thread.sleep(100);
        
        // Verify cleanup
        assertEquals(0, tracerService.getActiveSpanCount());
        assertTrue(Files.list(testWorkspaceDir).count() <= 1); // Only directory itself
        
        long finalMemory = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        long memoryIncrease = finalMemory - initialMemory;
        
        // Memory increase should be reasonable
        assertTrue(memoryIncrease < 50 * 1024 * 1024, 
            "Memory usage should be reasonable after cleanup");
        
        System.out.println("? Resource management and cleanup test passed");
    }
    
    @Test
    @Order(6)
    @DisplayName("Test Error Recovery and Resilience")
    void testErrorRecoveryAndResilience() throws Exception {
        System.out.println("Testing error recovery and resilience...");
        
        // Test 1: Invalid plugin execution
        String invalidPluginId = "invalid-plugin";
        String maliciousCode = "eval('malicious code');";
        
        var result1 = secureExecutor.execute(invalidPluginId, maliciousCode, "InvalidTest", null);
        assertFalse(result1.isSuccess());
        assertTrue(result1.getError().contains("Execution not permitted") || 
                  result1.getError().contains("Code validation failed"));
        
        // Test 2: Orchestration with errors
        CountDownLatch latch = new CountDownLatch(3);
        List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
        
        Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
            events.add(event);
            latch.countDown();
        };
        
        try {
            orchestrator.streamExecution(eventConsumer, null, "error-test-user");
        } catch (Exception e) {
            // Expected to handle gracefully
        }
        
        // Wait for error handling
        boolean errorHandled = latch.await(5, TimeUnit.SECONDS);
        
        // Test 3: System continues to work after errors
        CountDownLatch recoveryLatch = new CountDownLatch(2);
        List<AgenticOrchestrator.Event> recoveryEvents = Collections.synchronizedList(new ArrayList<>());
        
        Consumer<AgenticOrchestrator.Event> recoveryConsumer = event -> {
            recoveryEvents.add(event);
            recoveryLatch.countDown();
        };
        
        orchestrator.streamExecution(recoveryConsumer, "Recovery test", "recovery-user");
        boolean recovered = recoveryLatch.await(5, TimeUnit.SECONDS);
        
        assertTrue(recovered, "System should recover from errors");
        assertFalse(recoveryEvents.isEmpty(), "Recovery should produce events");
        
        System.out.println("? Error recovery and resilience test passed");
    }
    
    @Test
    @Order(7)
    @DisplayName("Test Complete Workflow Simulation")
    void testCompleteWorkflowSimulation() throws Exception {
        System.out.println("Testing complete workflow simulation...");
        
        // Simulate a complete development workflow
        String workflow = "Complete development workflow: analyze, code, test, document";
        
        // Step 1: Create project files
        Path mainFile = testWorkspaceDir.resolve("Main.java");
        Path testFile = testWorkspaceDir.resolve("MainTest.java");
        
        Files.writeString(mainFile, """
            public class Main {
                public int calculate(int a, int b) {
                    return a + b;
                }
            }
            """);
        
        // Step 2: Execute comprehensive orchestration
        CountDownLatch workflowLatch = new CountDownLatch(8);
        List<AgenticOrchestrator.Event> workflowEvents = Collections.synchronizedList(new ArrayList<>());
        Map<String, Object> workflowContext = new HashMap<>();
        
        Consumer<AgenticOrchestrator.Event> workflowConsumer = event -> {
            workflowEvents.add(event);
            workflowContext.put("step_" + workflowEvents.size(), event.getType());
            workflowLatch.countDown();
        };
        
        // Step 3: Execute with comprehensive monitoring
        var workflowSpan = tracerService.createPluginExecutionSpan("workflow", "complete-development-workflow");
        try (var scope = tracerService.activateSpan(workflowSpan)) {
            
            tracerService.addEvent("workflow-started", Map.of("project", "Main.java"));
            
            orchestrator.streamExecution(workflowConsumer, workflow, "developer-user");
            
            // Wait for workflow completion
            boolean workflowCompleted = workflowLatch.await(20, TimeUnit.SECONDS);
            assertTrue(workflowCompleted, "Complete workflow should finish");
            
            // Verify workflow steps
            assertFalse(workflowEvents.isEmpty());
            assertTrue(workflowEvents.stream().anyMatch(e -> "plan_created".equals(e.getType())));
            assertTrue(workflowEvents.stream().anyMatch(e -> "execution_complete".equals(e.getType())));
            
            tracerService.addEvent("workflow-completed", Map.of(
                "events", workflowEvents.size(),
                "files_processed", 2,
                "user", "developer-user"
            ));
            
        } finally {
            tracerService.completeSpan(workflowSpan);
        }
        
        // Step 4: Verify project files still exist and are accessible
        assertTrue(Files.exists(mainFile));
        assertTrue(Files.exists(testFile));
        
        String mainContent = Files.readString(mainFile);
        assertTrue(mainContent.contains("calculate"));
        
        System.out.println("? Complete workflow simulation test passed");
    }
    
    @Test
    @Order(8)
    @DisplayName("Test Performance Under Load")
    void testPerformanceUnderLoad() throws Exception {
        System.out.println("Testing performance under load...");
        
        int loadTestExecutions = 50;
        long startTime = System.currentTimeMillis();
        CountDownLatch loadLatch = new CountDownLatch(loadTestExecutions);
        List<Long> executionTimes = Collections.synchronizedList(new ArrayList<>());
        
        for (int i = 0; i < loadTestExecutions; i++) {
            final int executionId = i;
            new Thread(() -> {
                try {
                    long execStart = System.currentTimeMillis();
                    
                    CountDownLatch localLatch = new CountDownLatch(2);
                    List<AgenticOrchestrator.Event> events = Collections.synchronizedList(new ArrayList<>());
                    
                    Consumer<AgenticOrchestrator.Event> eventConsumer = event -> {
                        events.add(event);
                        localLatch.countDown();
                    };
                    
                    orchestrator.streamExecution(eventConsumer, 
                        "Load test execution " + executionId, "load-user-" + executionId);
                    
                    boolean completed = localLatch.await(5, TimeUnit.SECONDS);
                    assertTrue(completed, "Load test execution should complete");
                    
                    long execTime = System.currentTimeMillis() - execStart;
                    executionTimes.add(execTime);
                    
                } catch (Exception e) {
                    fail("Load test execution failed: " + e.getMessage());
                } finally {
                    loadLatch.countDown();
                }
            }).start();
        }
        
        // Wait for all load test executions
        boolean allCompleted = loadLatch.await(60, TimeUnit.SECONDS);
        assertTrue(allCompleted, "All load test executions should complete");
        
        long totalTime = System.currentTimeMillis() - startTime;
        
        // Calculate performance metrics
        double avgExecutionTime = executionTimes.stream().mapToLong(Long::longValue).average().orElse(0);
        long maxExecutionTime = executionTimes.stream().mapToLong(Long::longValue).max().orElse(0);
        double throughput = (double) loadTestExecutions / (totalTime / 1000.0);
        
        // Performance assertions
        assertTrue(avgExecutionTime < 3000, 
            "Average execution time should be under 3 seconds: " + avgExecutionTime);
        assertTrue(maxExecutionTime < 10000, 
            "Max execution time should be under 10 seconds: " + maxExecutionTime);
        assertTrue(throughput > 1, 
            "Throughput should be at least 1 execution per second: " + throughput);
        
        System.out.println("? Performance under load test passed");
        System.out.println("  Total time: " + totalTime + "ms");
        System.out.println("  Average execution: " + avgExecutionTime + "ms");
        System.out.println("  Max execution: " + maxExecutionTime + "ms");
        System.out.println("  Throughput: " + throughput + " exec/sec");
    }
    
    // Helper methods
    
    private void cleanupDirectory(Path directory) {
        if (directory != null && Files.exists(directory)) {
            try {
                Files.walk(directory)
                        .sorted(Comparator.reverseOrder())
                        .forEach(path -> {
                            try {
                                Files.deleteIfExists(path);
                            } catch (Exception e) {
                                // Ignore cleanup errors
                            }
                        });
            } catch (Exception e) {
                // Ignore cleanup errors
            }
        }
    }
    
    private String executeTaskThroughOrchestrator(AgenticOrchestrator.Task task) throws Exception {
        // Simulate task execution through orchestrator
        switch (task.getType()) {
            case "plugin_execution":
                if (task.getPluginId() != null && task.getCode() != null) {
                    return secureExecutor.execute(task.getPluginId(), task.getCode(), task.getClassName());
                }
                break;
                
            case "file_operation":
                if ("read".equals(task.getOperation()) && task.getFilePath() != null) {
                    return Files.readString(Paths.get(task.getFilePath()));
                }
                break;
                
            default:
                return "Task executed: " + task.getDescription();
        }
        
        return "Task completed: " + task.getDescription();
    }
}
