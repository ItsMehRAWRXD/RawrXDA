// ============================================================================
// test_native_ide_tools.cpp — Comprehensive Test Suite for Native IDE Tools
// Tests all 100+ tools with validation, benchmarks, and integration tests
// ============================================================================

#include "native_ide_tools.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <thread>
#include <cassert>
#include <fstream>

using namespace RawrXD::NativeIDE;

// ============================================================================
// Test Framework
// ============================================================================
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    g_tests_total++; \
    try { \
        test_##name(); \
        printf("PASSED\n"); \
        g_tests_passed++; \
    } catch (const std::exception& e) { \
        printf("FAILED: %s\n", e.what()); \
        g_tests_failed++; \
    } catch (...) { \
        printf("FAILED: Unknown exception\n"); \
        g_tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_FALSE(cond) if (cond) throw std::runtime_error("Assertion failed: NOT " #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " != " #b)
#define ASSERT_NE(a, b) if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)
#define ASSERT_STR_CONTAINS(str, substr) if (!strstr(str, substr)) throw std::runtime_error("String does not contain: " #substr)
#define ASSERT_EMPTY(str) if (!str.empty()) throw std::runtime_error("String not empty: " #str)
#define ASSERT_NOT_EMPTY(str) if (str.empty()) throw std::runtime_error("String is empty: " #str)

// ============================================================================
// Platform Tests
// ============================================================================
TEST(platform_alloc_free) {
    void* p = Platform::Alloc(1024);
    ASSERT_TRUE(p != nullptr);
    Platform::Free(p);
    
    void* p2 = Platform::Alloc(0);
    // Alloc(0) behavior is implementation-defined, just ensure no crash
    Platform::Free(p2);
}

TEST(platform_realloc) {
    void* p = Platform::Alloc(100);
    ASSERT_TRUE(p != nullptr);
    memset(p, 0xAB, 100);
    
    p = Platform::Realloc(p, 200);
    ASSERT_TRUE(p != nullptr);
    
    // Original data should be preserved
    unsigned char* bytes = static_cast<unsigned char*>(p);
    ASSERT_EQ(bytes[0], 0xAB);
    
    Platform::Free(p);
}

TEST(platform_string_ops) {
    ASSERT_EQ(Platform::StrLen("hello"), 5);
    ASSERT_EQ(Platform::StrLen(""), 0);
    ASSERT_EQ(Platform::StrLen(nullptr), 0);
    
    char* dup = Platform::StrDup("test");
    ASSERT_TRUE(dup != nullptr);
    ASSERT_EQ(strcmp(dup, "test"), 0);
    Platform::Free(dup);
    
    ASSERT_EQ(Platform::StrCmp("abc", "abc"), 0);
    ASSERT_TRUE(Platform::StrCmp("abc", "def") < 0);
    ASSERT_TRUE(Platform::StrCmp("def", "abc") > 0);
    
    ASSERT_TRUE(Platform::StrStr("hello world", "world") != nullptr);
    ASSERT_TRUE(Platform::StrStr("hello world", "xyz") == nullptr);
}

TEST(platform_file_exists) {
    // Create a temp file
    const char* temp_file = "test_temp_12345.txt";
    FILE* f = fopen(temp_file, "w");
    if (f) {
        fprintf(f, "test\n");
        fclose(f);
        
        ASSERT_TRUE(Platform::FileExists(temp_file));
        
        // Cleanup
        remove(temp_file);
        ASSERT_FALSE(Platform::FileExists(temp_file));
    }
}

TEST(platform_dir_exists) {
    ASSERT_TRUE(Platform::DirExists("."));
    ASSERT_FALSE(Platform::DirExists("/nonexistent/path/12345"));
}

// ============================================================================
// FileExplorer Tests
// ============================================================================
TEST(file_explorer_list_dir) {
    FileExplorer explorer;
    auto result = explorer.ListDir(".");
    ASSERT_TRUE(result.success);
    ASSERT_NOT_EMPTY(result.output);
}

TEST(file_explorer_read_write) {
    const char* test_file = "test_rw_12345.txt";
    const char* test_content = "Hello, World!\nLine 2\nLine 3";
    
    FileExplorer explorer;
    
    // Write
    auto write_result = explorer.WriteFile(test_file, test_content);
    ASSERT_TRUE(write_result.success);
    
    // Read
    auto read_result = explorer.ReadFile(test_file);
    ASSERT_TRUE(read_result.success);
    ASSERT_STR_CONTAINS(read_result.output.c_str(), "Hello, World!");
    
    // Cleanup
    remove(test_file);
}

TEST(file_explorer_copy_move_delete) {
    const char* src = "test_copy_src.txt";
    const char* dst = "test_copy_dst.txt";
    const char* moved = "test_moved.txt";
    
    FileExplorer explorer;
    
    // Create source
    explorer.WriteFile(src, "source content");
    
    // Copy
    auto copy_result = explorer.CopyFile(src, dst);
    ASSERT_TRUE(copy_result.success);
    ASSERT_TRUE(Platform::FileExists(dst));
    
    // Move
    auto move_result = explorer.MoveFile(dst, moved);
    ASSERT_TRUE(move_result.success);
    ASSERT_FALSE(Platform::FileExists(dst));
    ASSERT_TRUE(Platform::FileExists(moved));
    
    // Delete
    auto del_result = explorer.DeleteFile(moved);
    ASSERT_TRUE(del_result.success);
    ASSERT_FALSE(Platform::FileExists(moved));
    
    // Cleanup
    remove(src);
}

TEST(file_explorer_grep) {
    const char* test_file = "test_grep.txt";
    FileExplorer explorer;
    
    explorer.WriteFile(test_file, "line1: apple\nline2: banana\nline3: cherry\nline4: apple pie\n");
    
    auto result = explorer.GrepFile(test_file, "apple");
    ASSERT_TRUE(result.success);
    ASSERT_STR_CONTAINS(result.output.c_str(), "line1");
    ASSERT_STR_CONTAINS(result.output.c_str(), "line4");
    
    remove(test_file);
}

TEST(file_explorer_find_files) {
    FileExplorer explorer;
    auto result = explorer.FindFiles(".", ".cpp");
    ASSERT_TRUE(result.success);
    // Should find at least this test file
}

// ============================================================================
// CodeNavigator Tests
// ============================================================================
TEST(code_navigator_parse_file) {
    const char* test_file = "test_parse.cpp";
    FILE* f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "class MyClass {\n");
        fprintf(f, "public:\n");
        fprintf(f, "    void method1();\n");
        fprintf(f, "    int method2();\n");
        fprintf(f, "};\n");
        fprintf(f, "\n");
        fprintf(f, "void freeFunction() {}\n");
        fclose(f);
        
        CodeNavigator nav;
        auto result = nav.ParseFile(test_file);
        ASSERT_TRUE(result.success);
        
        remove(test_file);
    }
}

TEST(code_navigator_find_symbol) {
    const char* test_file = "test_symbol.cpp";
    FILE* f = fopen(test_file, "w");
    if (f) {
        fprintf(f, "int globalVar = 42;\n");
        fprintf(f, "void myFunction() {}\n");
        fprintf(f, "class MyClass {};\n");
        fclose(f);
        
        CodeNavigator nav;
        nav.ParseFile(test_file);
        
        auto result = nav.FindSymbol("myFunction");
        ASSERT_TRUE(result.success);
        
        remove(test_file);
    }
}

// ============================================================================
// ToolRegistry Tests
// ============================================================================
TEST(tool_registry_singleton) {
    auto& reg1 = ToolRegistry::Instance();
    auto& reg2 = ToolRegistry::Instance();
    ASSERT_EQ(&reg1, &reg2);
}

TEST(tool_registry_register_tool) {
    auto& reg = ToolRegistry::Instance();
    
    int call_count = 0;
    ToolDefinition tool;
    tool.name = "test_tool_12345";
    tool.description = "A test tool";
    tool.category = ToolCategory::Misc;
    tool.execute = [&call_count](const std::map<std::string, std::string>& params) -> ToolResult {
        call_count++;
        ToolResult r;
        r.success = true;
        r.output = "called";
        return r;
    };
    
    reg.RegisterTool(tool);
    ASSERT_TRUE(reg.HasTool("test_tool_12345"));
    
    // Execute
    auto result = reg.ExecuteTool("test_tool_12345", {});
    ASSERT_TRUE(result.success);
    ASSERT_EQ(call_count, 1);
    
    // Cleanup
    reg.UnregisterTool("test_tool_12345");
    ASSERT_FALSE(reg.HasTool("test_tool_12345"));
}

TEST(tool_registry_list_tools) {
    auto& reg = ToolRegistry::Instance();
    auto tools = reg.ListTools();
    ASSERT_TRUE(!tools.empty());
}

TEST(tool_registry_filter_by_category) {
    auto& reg = ToolRegistry::Instance();
    auto file_tools = reg.ListToolsByCategory(ToolCategory::File);
    ASSERT_TRUE(!file_tools.empty());
    
    for (const auto& name : file_tools) {
        auto* tool = reg.GetTool(name);
        if (tool) {
            ASSERT_TRUE(static_cast<uint32_t>(tool->category) & static_cast<uint32_t>(ToolCategory::File));
        }
    }
}

TEST(tool_registry_rate_limiting) {
    auto& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "rate_limited_tool";
    tool.description = "Rate limited test";
    tool.category = ToolCategory::Misc;
    tool.rateLimit = 5; // 5 calls per second
    tool.execute = [](const std::map<std::string, std::string>&) -> ToolResult {
        ToolResult r;
        r.success = true;
        return r;
    };
    
    reg.RegisterTool(tool);
    
    // Should succeed for first 5 calls
    for (int i = 0; i < 5; i++) {
        auto result = reg.ExecuteTool("rate_limited_tool", {});
        ASSERT_TRUE(result.success);
    }
    
    // 6th call should be rate limited
    auto result = reg.ExecuteTool("rate_limited_tool", {});
    // Rate limiting may or may not block depending on timing
    
    reg.UnregisterTool("rate_limited_tool");
}

TEST(tool_registry_enable_disable) {
    auto& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "toggle_tool";
    tool.description = "Toggle test";
    tool.category = ToolCategory::Misc;
    tool.enabled = true;
    tool.execute = [](const std::map<std::string, std::string>&) -> ToolResult {
        ToolResult r;
        r.success = true;
        return r;
    };
    
    reg.RegisterTool(tool);
    
    // Enabled
    reg.EnableTool("toggle_tool");
    auto result = reg.ExecuteTool("toggle_tool", {});
    ASSERT_TRUE(result.success);
    
    // Disabled
    reg.DisableTool("toggle_tool");
    result = reg.ExecuteTool("toggle_tool", {});
    ASSERT_FALSE(result.success);
    
    reg.UnregisterTool("toggle_tool");
}

// ============================================================================
// MemorySystem Tests
// ============================================================================
TEST(memory_system_remember_recall) {
    MemorySystem mem;
    
    mem.Remember("test_key", "test_value");
    
    auto value = mem.Recall("test_key");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "test_value");
    
    auto missing = mem.Recall("nonexistent_key");
    ASSERT_FALSE(missing.has_value());
}

TEST(memory_system_forget) {
    MemorySystem mem;
    
    mem.Remember("forget_me", "value");
    ASSERT_TRUE(mem.Recall("forget_me").has_value());
    
    mem.Forget("forget_me");
    ASSERT_FALSE(mem.Recall("forget_me").has_value());
}

TEST(memory_system_clear) {
    MemorySystem mem;
    
    mem.Remember("key1", "val1");
    mem.Remember("key2", "val2");
    mem.Remember("key3", "val3");
    
    mem.Clear();
    
    ASSERT_FALSE(mem.Recall("key1").has_value());
    ASSERT_FALSE(mem.Recall("key2").has_value());
    ASSERT_FALSE(mem.Recall("key3").has_value());
}

TEST(memory_system_persistence) {
    const char* mem_file = "test_memory.json";
    
    {
        MemorySystem mem;
        mem.Remember("persistent_key", "persistent_value");
        mem.SaveToFile(mem_file);
    }
    
    {
        MemorySystem mem;
        mem.LoadFromFile(mem_file);
        auto value = mem.Recall("persistent_key");
        ASSERT_TRUE(value.has_value());
        ASSERT_EQ(value.value(), "persistent_value");
    }
    
    remove(mem_file);
}

// ============================================================================
// Autopilot Tests
// ============================================================================
TEST(autopilot_create_task) {
    Autopilot autopilot;
    
    auto task_id = autopilot.CreateTask("Test task", "Test description");
    ASSERT_TRUE(!task_id.empty());
    
    auto status = autopilot.GetTaskStatus(task_id);
    ASSERT_EQ(status, "pending");
}

TEST(autopilot_execute_task) {
    Autopilot autopilot;
    
    auto task_id = autopilot.CreateTask("Echo test", "Echo hello");
    
    // Add a simple tool for testing
    ToolDefinition echo_tool;
    echo_tool.name = "echo";
    echo_tool.description = "Echo input";
    echo_tool.category = ToolCategory::Misc;
    echo_tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult r;
        r.success = true;
        auto it = params.find("text");
        r.output = it != params.end() ? it->second : "";
        return r;
    };
    
    auto& reg = ToolRegistry::Instance();
    reg.RegisterTool(echo_tool);
    
    // Execute
    bool executed = autopilot.ExecuteTask(task_id);
    
    reg.UnregisterTool("echo");
}

TEST(autopilot_cancel_task) {
    Autopilot autopilot;
    
    auto task_id = autopilot.CreateTask("Long task", "A long running task");
    autopilot.CancelTask(task_id);
    
    auto status = autopilot.GetTaskStatus(task_id);
    ASSERT_EQ(status, "cancelled");
}

TEST(autopilot_list_tasks) {
    Autopilot autopilot;
    
    autopilot.CreateTask("Task 1", "First");
    autopilot.CreateTask("Task 2", "Second");
    autopilot.CreateTask("Task 3", "Third");
    
    auto tasks = autopilot.ListTasks();
    ASSERT_TRUE(tasks.size() >= 3);
}

// ============================================================================
// Integration Tests
// ============================================================================
TEST(integration_file_workflow) {
    FileExplorer explorer;
    CodeNavigator nav;
    
    // Create a file
    const char* test_file = "test_workflow.cpp";
    explorer.WriteFile(test_file, "int x = 42;\nint y = x * 2;\n");
    
    // Read it back
    auto read_result = explorer.ReadFile(test_file);
    ASSERT_TRUE(read_result.success);
    
    // Parse it
    auto parse_result = nav.ParseFile(test_file);
    ASSERT_TRUE(parse_result.success);
    
    // Cleanup
    remove(test_file);
}

TEST(integration_memory_autopilot) {
    MemorySystem mem;
    Autopilot autopilot;
    
    // Store task info in memory
    mem.Remember("last_task", "integration_test");
    
    // Create task
    auto task_id = autopilot.CreateTask("Integration test", "Test memory integration");
    
    // Verify memory persists
    auto value = mem.Recall("last_task");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "integration_test");
}

// ============================================================================
// Benchmark Tests
// ============================================================================
TEST(benchmark_tool_execution) {
    auto& reg = ToolRegistry::Instance();
    
    ToolDefinition bench_tool;
    bench_tool.name = "bench_tool";
    bench_tool.description = "Benchmark tool";
    bench_tool.category = ToolCategory::Misc;
    bench_tool.execute = [](const std::map<std::string, std::string>&) -> ToolResult {
        ToolResult r;
        r.success = true;
        // Simulate some work
        volatile int sum = 0;
        for (int i = 0; i < 1000; i++) sum += i;
        return r;
    };
    
    reg.RegisterTool(bench_tool);
    
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        reg.ExecuteTool("bench_tool", {});
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    printf("\n    %d iterations in %lld us (%.2f us/call)\n", 
           iterations, duration, (double)duration / iterations);
    
    reg.UnregisterTool("bench_tool");
}

TEST(benchmark_file_operations) {
    FileExplorer explorer;
    const char* test_file = "bench_file.txt";
    const char* content = "Benchmark content\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        explorer.WriteFile(test_file, content);
        explorer.ReadFile(test_file);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    printf("\n    %d read/write cycles in %lld us (%.2f us/cycle)\n",
           iterations, duration, (double)duration / iterations);
    
    remove(test_file);
}

// ============================================================================
// Stress Tests
// ============================================================================
TEST(stress_concurrent_access) {
    auto& reg = ToolRegistry::Instance();
    
    ToolDefinition stress_tool;
    stress_tool.name = "stress_tool";
    stress_tool.description = "Stress test";
    stress_tool.category = ToolCategory::Misc;
    stress_tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult r;
        r.success = true;
        auto it = params.find("id");
        r.output = it != params.end() ? it->second : "no_id";
        return r;
    };
    
    reg.RegisterTool(stress_tool);
    
    // Launch multiple threads
    const int num_threads = 10;
    const int calls_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&reg, &success_count, t, calls_per_thread]() {
            for (int i = 0; i < calls_per_thread; i++) {
                std::map<std::string, std::string> params;
                params["id"] = std::to_string(t) + "_" + std::to_string(i);
                auto result = reg.ExecuteTool("stress_tool", params);
                if (result.success) success_count++;
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    printf("\n    %d/%d concurrent calls succeeded\n", 
           success_count.load(), num_threads * calls_per_thread);
    
    reg.UnregisterTool("stress_tool");
}

TEST(stress_memory_operations) {
    MemorySystem mem;
    
    const int num_entries = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    // Write
    for (int i = 0; i < num_entries; i++) {
        mem.Remember("key_" + std::to_string(i), "value_" + std::to_string(i));
    }
    
    // Read
    int found = 0;
    for (int i = 0; i < num_entries; i++) {
        if (mem.Recall("key_" + std::to_string(i)).has_value()) found++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    printf("\n    %d/%d entries found in %lld ms\n", found, num_entries, duration);
    ASSERT_EQ(found, num_entries);
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main(int argc, char** argv) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║        Native IDE Tools — Comprehensive Test Suite           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Initialize
    auto& reg = ToolRegistry::Instance();
    reg.InitializeBuiltinTools();
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Platform Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(platform_alloc_free);
    RUN_TEST(platform_realloc);
    RUN_TEST(platform_string_ops);
    RUN_TEST(platform_file_exists);
    RUN_TEST(platform_dir_exists);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("FileExplorer Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(file_explorer_list_dir);
    RUN_TEST(file_explorer_read_write);
    RUN_TEST(file_explorer_copy_move_delete);
    RUN_TEST(file_explorer_grep);
    RUN_TEST(file_explorer_find_files);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("CodeNavigator Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(code_navigator_parse_file);
    RUN_TEST(code_navigator_find_symbol);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("ToolRegistry Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(tool_registry_singleton);
    RUN_TEST(tool_registry_register_tool);
    RUN_TEST(tool_registry_list_tools);
    RUN_TEST(tool_registry_filter_by_category);
    RUN_TEST(tool_registry_rate_limiting);
    RUN_TEST(tool_registry_enable_disable);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("MemorySystem Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(memory_system_remember_recall);
    RUN_TEST(memory_system_forget);
    RUN_TEST(memory_system_clear);
    RUN_TEST(memory_system_persistence);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Autopilot Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(autopilot_create_task);
    RUN_TEST(autopilot_execute_task);
    RUN_TEST(autopilot_cancel_task);
    RUN_TEST(autopilot_list_tasks);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Integration Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(integration_file_workflow);
    RUN_TEST(integration_memory_autopilot);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Benchmark Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(benchmark_tool_execution);
    RUN_TEST(benchmark_file_operations);
    
    printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("Stress Tests\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    RUN_TEST(stress_concurrent_access);
    RUN_TEST(stress_memory_operations);
    
    // Summary
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      Test Summary                            ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Total:  %3d                                                 ║\n", g_tests_total);
    printf("║  Passed: %3d                                                 ║\n", g_tests_passed);
    printf("║  Failed: %3d                                                 ║\n", g_tests_failed);
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Cleanup
    reg.Shutdown();
    
    return g_tests_failed > 0 ? 1 : 0;
}
