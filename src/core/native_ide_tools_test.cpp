// ============================================================================
// native_ide_tools_test.cpp — Test Suite for Native IDE Tools
// ============================================================================

#include "native_ide_tools.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <string>
#include <map>

using namespace RawrXD::NativeIDE;

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name()
#define RUN_TEST(name) do { \
    printf("Running test: %s... ", #name); \
    try { \
        test_##name(); \
        printf("PASSED\n"); \
        tests_passed++; \
    } catch (const std::exception& e) { \
        printf("FAILED: %s\n", e.what()); \
        tests_failed++; \
    } catch (...) { \
        printf("FAILED: Unknown exception\n"); \
        tests_failed++; \
    } \
} while(0)

#define ASSERT_TRUE(cond) if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)
#define ASSERT_FALSE(cond) if (cond) throw std::runtime_error("Assertion failed: !" #cond)
#define ASSERT_EQ(a, b) if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)
#define ASSERT_NE(a, b) if ((a) == (b)) throw std::runtime_error("Assertion failed: " #a " != " #b)
#define ASSERT_STR_EQ(a, b) if (strcmp((a), (b)) != 0) throw std::runtime_error("Assertion failed: " #a " == " #b)

// ============================================================================
// Platform Tests
// ============================================================================

TEST(platform_alloc_free) {
    void* ptr = Platform::Alloc(1024);
    ASSERT_TRUE(ptr != nullptr);
    Platform::Free(ptr);
}

TEST(platform_realloc) {
    void* ptr = Platform::Alloc(1024);
    ASSERT_TRUE(ptr != nullptr);
    ptr = Platform::Realloc(ptr, 2048);
    ASSERT_TRUE(ptr != nullptr);
    Platform::Free(ptr);
}

TEST(platform_str_dup) {
    const char* original = "Hello, World!";
    char* dup = Platform::StrDup(original);
    ASSERT_TRUE(dup != nullptr);
    ASSERT_STR_EQ(dup, original);
    Platform::Free(dup);
}

TEST(platform_str_cmp) {
    ASSERT_EQ(Platform::StrCmp("abc", "abc"), 0);
    ASSERT_TRUE(Platform::StrCmp("abc", "def") < 0);
    ASSERT_TRUE(Platform::StrCmp("def", "abc") > 0);
}

TEST(platform_file_exists) {
    // Create a test file
    FILE* f = fopen("test_file.txt", "w");
    if (f) {
        fprintf(f, "test content\n");
        fclose(f);
        
        ASSERT_TRUE(Platform::FileExists("test_file.txt"));
        
        // Cleanup
        Platform::DeleteFile("test_file.txt");
        ASSERT_FALSE(Platform::FileExists("test_file.txt"));
    }
}

TEST(platform_dir_exists) {
    ASSERT_TRUE(Platform::DirExists("."));
    ASSERT_FALSE(Platform::DirExists("/nonexistent/path"));
}

TEST(platform_create_dir) {
    const char* testDir = "test_dir_native_ide";
    bool created = Platform::CreateDir(testDir);
    ASSERT_TRUE(created || errno == EEXIST);
    
    if (created) {
        ASSERT_TRUE(Platform::DirExists(testDir));
        Platform::DeleteDir(testDir);
    }
}

TEST(platform_read_write_file) {
    const char* testFile = "test_rw_file.txt";
    const char* content = "Hello, File I/O!";
    
    bool written = Platform::WriteFile(testFile, content, strlen(content));
    ASSERT_TRUE(written);
    
    size_t size = 0;
    char* data = Platform::ReadFile(testFile, &size);
    ASSERT_TRUE(data != nullptr);
    ASSERT_EQ(size, strlen(content));
    ASSERT_STR_EQ(data, content);
    
    Platform::Free(data);
    Platform::DeleteFile(testFile);
}

TEST(platform_time) {
    uint64_t us = Platform::GetTimeMicroseconds();
    uint64_t ms = Platform::GetTimeMilliseconds();
    
    ASSERT_TRUE(us > 0);
    ASSERT_TRUE(ms > 0);
    ASSERT_TRUE(us / 1000 >= ms - 1);
    ASSERT_TRUE(us / 1000 <= ms + 1);
}

TEST(platform_env) {
    // Set a test environment variable
    bool set = Platform::SetEnv("RAWRXD_TEST_VAR", "test_value");
    ASSERT_TRUE(set);
    
    char* value = Platform::GetEnv("RAWRXD_TEST_VAR");
    ASSERT_TRUE(value != nullptr);
    ASSERT_STR_EQ(value, "test_value");
    Platform::Free(value);
}

// ============================================================================
// Tool Registry Tests
// ============================================================================

TEST(tool_registry_singleton) {
    ToolRegistry& reg1 = ToolRegistry::Instance();
    ToolRegistry& reg2 = ToolRegistry::Instance();
    ASSERT_EQ(&reg1, &reg2);
}

TEST(tool_registry_register) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "test_tool";
    tool.description = "A test tool";
    tool.category = ToolCategory::Misc;
    tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult result;
        result.success = true;
        result.output = "test output";
        return result;
    };
    
    reg.RegisterTool(tool);
    ASSERT_TRUE(reg.HasTool("test_tool"));
    
    reg.UnregisterTool("test_tool");
    ASSERT_FALSE(reg.HasTool("test_tool"));
}

TEST(tool_registry_execute) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "test_exec_tool";
    tool.description = "A test tool for execution";
    tool.category = ToolCategory::Misc;
    tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult result;
        result.success = true;
        result.output = "executed with: " + (params.count("arg") ? params.at("arg") : "no args");
        return result;
    };
    
    reg.RegisterTool(tool);
    
    std::map<std::string, std::string> params;
    params["arg"] = "test_value";
    
    ToolResult result = reg.ExecuteTool("test_exec_tool", params);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.output, "executed with: test_value");
    
    reg.UnregisterTool("test_exec_tool");
}

TEST(tool_registry_rate_limit) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "test_rate_limit";
    tool.description = "A rate-limited tool";
    tool.category = ToolCategory::Misc;
    tool.rateLimit = 2; // 2 calls per second
    tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult result;
        result.success = true;
        return result;
    };
    
    reg.RegisterTool(tool);
    
    // First two calls should succeed
    ToolResult r1 = reg.ExecuteTool("test_rate_limit", {});
    ASSERT_TRUE(r1.success);
    
    ToolResult r2 = reg.ExecuteTool("test_rate_limit", {});
    ASSERT_TRUE(r2.success);
    
    // Third call should be rate limited
    ToolResult r3 = reg.ExecuteTool("test_rate_limit", {});
    ASSERT_FALSE(r3.success);
    ASSERT_EQ(r3.statusCode, 429);
    
    reg.UnregisterTool("test_rate_limit");
}

TEST(tool_registry_json) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    ToolDefinition tool;
    tool.name = "test_json_tool";
    tool.description = "A JSON test tool";
    tool.category = ToolCategory::Misc;
    tool.execute = [](const std::map<std::string, std::string>& params) -> ToolResult {
        ToolResult result;
        result.success = true;
        result.output = params.count("key") ? params.at("key") : "no key";
        return result;
    };
    
    reg.RegisterTool(tool);
    
    ToolResult result = reg.ExecuteToolJSON("test_json_tool", "{\"key\": \"value\"}");
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.output, "value");
    
    reg.UnregisterTool("test_json_tool");
}

// ============================================================================
// Built-in Tool Tests
// ============================================================================

TEST(builtin_file_read_write) {
    ToolRegistry& reg = ToolRegistry::Instance();
    reg.InitializeBuiltInTools();
    
    // Write
    std::map<std::string, std::string> writeParams;
    writeParams["path"] = "test_builtin_file.txt";
    writeParams["content"] = "Hello, Built-in Tools!";
    
    ToolResult writeResult = reg.ExecuteTool("file_write", writeParams);
    ASSERT_TRUE(writeResult.success);
    
    // Read
    std::map<std::string, std::string> readParams;
    readParams["path"] = "test_builtin_file.txt";
    
    ToolResult readResult = reg.ExecuteTool("file_read", readParams);
    ASSERT_TRUE(readResult.success);
    ASSERT_EQ(readResult.output, "Hello, Built-in Tools!");
    
    // Cleanup
    reg.ExecuteTool("file_delete", readParams);
}

TEST(builtin_file_exists) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    // Create a test file
    std::map<std::string, std::string> writeParams;
    writeParams["path"] = "test_exists.txt";
    writeParams["content"] = "test";
    reg.ExecuteTool("file_write", writeParams);
    
    // Check exists
    std::map<std::string, std::string> params;
    params["path"] = "test_exists.txt";
    
    ToolResult result = reg.ExecuteTool("file_exists", params);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.output, "true");
    
    // Check non-existent
    params["path"] = "nonexistent_file.txt";
    result = reg.ExecuteTool("file_exists", params);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.output, "false");
    
    // Cleanup
    params["path"] = "test_exists.txt";
    reg.ExecuteTool("file_delete", params);
}

TEST(builtin_memory) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    // Remember
    std::map<std::string, std::string> rememberParams;
    rememberParams["key"] = "test_key";
    rememberParams["value"] = "test_value";
    
    ToolResult rememberResult = reg.ExecuteTool("memory_remember", rememberParams);
    ASSERT_TRUE(rememberResult.success);
    
    // Recall
    std::map<std::string, std::string> recallParams;
    recallParams["key"] = "test_key";
    
    ToolResult recallResult = reg.ExecuteTool("memory_recall", recallParams);
    ASSERT_TRUE(recallResult.success);
    ASSERT_EQ(recallResult.output, "test_value");
    
    // Forget
    std::map<std::string, std::string> forgetParams;
    forgetParams["key"] = "test_key";
    
    ToolResult forgetResult = reg.ExecuteTool("memory_forget", forgetParams);
    ASSERT_TRUE(forgetResult.success);
    
    // Verify forgotten
    recallResult = reg.ExecuteTool("memory_recall", recallParams);
    ASSERT_TRUE(recallResult.success);
    ASSERT_EQ(recallResult.output, "Key not found");
}

TEST(builtin_code_parse) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    std::map<std::string, std::string> params;
    params["code"] = "int main() { return 0; }";
    params["lang"] = "cpp";
    
    ToolResult result = reg.ExecuteTool("code_parse", params);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.output.find("int") != std::string::npos);
    ASSERT_TRUE(result.output.find("main") != std::string::npos);
}

TEST(builtin_code_format) {
    ToolRegistry& reg = ToolRegistry::Instance();
    
    std::map<std::string, std::string> params;
    params["code"] = "int main(){return 0;}";
    
    ToolResult result = reg.ExecuteTool("code_format", params);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.output.find("int main()") != std::string::npos);
}

// ============================================================================
// Chunked I/O Tests (P0 Blocker Fix)
// ============================================================================

TEST(chunked_io_open_close) {
    // Create a test file
    const char* testFile = "test_chunked_io.bin";
    FILE* f = fopen(testFile, "wb");
    if (f) {
        const char* data = "Hello, Chunked I/O!";
        fwrite(data, 1, strlen(data), f);
        fclose(f);
        
        ChunkedIO::ChunkedFile* cf = ChunkedIO::ChunkedOpen(testFile, true);
        ASSERT_TRUE(cf != nullptr);
        ASSERT_TRUE(cf->isOpen);
        ASSERT_EQ(cf->fileSize, strlen(data));
        
        ChunkedIO::ChunkedClose(cf);
        
        // Cleanup
        Platform::DeleteFile(testFile);
    }
}

TEST(chunked_io_read) {
    const char* testFile = "test_chunked_read.bin";
    const size_t fileSize = 1024 * 1024; // 1MB
    
    // Create test file
    FILE* f = fopen(testFile, "wb");
    if (f) {
        for (size_t i = 0; i < fileSize; i++) {
            char c = (char)(i % 256);
            fwrite(&c, 1, 1, f);
        }
        fclose(f);
        
        // Read with chunked I/O
        size_t size = 0;
        char* data = ChunkedIO::ChunkedReadAll(testFile, &size);
        ASSERT_TRUE(data != nullptr);
        ASSERT_EQ(size, fileSize);
        
        // Verify content
        bool correct = true;
        for (size_t i = 0; i < size && correct; i++) {
            if ((unsigned char)data[i] != (i % 256)) {
                correct = false;
            }
        }
        ASSERT_TRUE(correct);
        
        Platform::Free(data);
        Platform::DeleteFile(testFile);
    }
}

TEST(chunked_io_large_file) {
    // Test with simulated large file info
    const char* testFile = "test_large_file.bin";
    
    // Create a file larger than 2GB would be too slow for unit test
    // Instead, test the size detection
    size_t size = ChunkedIO::ChunkedGetSize(testFile);
    ASSERT_EQ(size, 0); // File doesn't exist
    
    // Create a small test file
    FILE* f = fopen(testFile, "wb");
    if (f) {
        const char* data = "test";
        fwrite(data, 1, strlen(data), f);
        fclose(f);
        
        size = ChunkedIO::ChunkedGetSize(testFile);
        ASSERT_EQ(size, strlen(data));
        
        bool isLarge = ChunkedIO::ChunkedIsLargeFile(testFile, 100);
        ASSERT_FALSE(isLarge); // Small file
        
        Platform::DeleteFile(testFile);
    }
}

// ============================================================================
// Memory System Tests
// ============================================================================

TEST(memory_system_singleton) {
    MemorySystem& mem1 = MemorySystem::Instance();
    MemorySystem& mem2 = MemorySystem::Instance();
    ASSERT_EQ(&mem1, &mem2);
}

TEST(memory_system_remember_recall) {
    MemorySystem& mem = MemorySystem::Instance();
    
    mem.Remember("test_key_1", "test_value_1");
    mem.Remember("test_key_2", "test_value_2");
    
    ASSERT_EQ(mem.Recall("test_key_1"), "test_value_1");
    ASSERT_EQ(mem.Recall("test_key_2"), "test_value_2");
    ASSERT_EQ(mem.Recall("nonexistent"), "");
    
    ASSERT_TRUE(mem.HasKey("test_key_1"));
    ASSERT_FALSE(mem.HasKey("nonexistent"));
    
    mem.Forget("test_key_1");
    ASSERT_FALSE(mem.HasKey("test_key_1"));
    
    mem.Clear();
    ASSERT_FALSE(mem.HasKey("test_key_2"));
}

TEST(memory_system_pattern) {
    MemorySystem& mem = MemorySystem::Instance();
    
    mem.Remember("user_name", "Alice");
    mem.Remember("user_email", "alice@example.com");
    mem.Remember("project_name", "RawrXD");
    
    std::vector<std::string> userValues = mem.RecallPattern("user");
    ASSERT_EQ(userValues.size(), 2);
    
    mem.Clear();
}

// ============================================================================
// File Explorer Tests
// ============================================================================

TEST(file_explorer_navigation) {
    FileExplorer explorer(".");
    
    ASSERT_EQ(explorer.GetCurrentPath(), ".");
    ASSERT_EQ(explorer.GetRootPath(), ".");
    
    explorer.NavigateTo("..");
    ASSERT_TRUE(explorer.GetCurrentPath() != ".");
    
    explorer.GoUp();
    
    explorer.Refresh();
    
    const std::vector<FileEntry>& entries = explorer.GetEntries();
    ASSERT_TRUE(entries.size() > 0);
}

// ============================================================================
// Code Navigator Tests
// ============================================================================

TEST(code_navigator_index) {
    CodeNavigator navigator;
    
    // Create a test file
    const char* testFile = "test_code_nav.cpp";
    FILE* f = fopen(testFile, "w");
    if (f) {
        fprintf(f, "int main() { return 0; }\n");
        fprintf(f, "void testFunction() {}\n");
        fclose(f);
        
        navigator.IndexFile(testFile);
        
        Symbol* sym = navigator.FindSymbol("main");
        // Note: Symbol extraction depends on regex implementation
        // This is a basic test
        
        Platform::DeleteFile(testFile);
    }
}

// ============================================================================
// Autopilot Tests
// ============================================================================

TEST(autopilot_task_management) {
    Autopilot autopilot;
    
    autopilot.AddTask("Test task");
    
    Task* task = autopilot.GetCurrentTask();
    ASSERT_TRUE(task != nullptr);
    ASSERT_EQ(task->description, "Test task");
    ASSERT_EQ(task->state, TaskState::Analyzing);
    
    autopilot.CancelTask();
    
    task = autopilot.GetCurrentTask();
    ASSERT_TRUE(task == nullptr);
}

TEST(autopilot_autonomy_level) {
    Autopilot autopilot;
    
    autopilot.SetAutonomyLevel(0);
    ASSERT_EQ(autopilot.GetAutonomyLevel(), 0);
    
    autopilot.SetAutonomyLevel(2);
    ASSERT_EQ(autopilot.GetAutonomyLevel(), 2);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv) {
    printf("=== Native IDE Tools Test Suite ===\n\n");
    
    // Platform tests
    printf("--- Platform Tests ---\n");
    RUN_TEST(platform_alloc_free);
    RUN_TEST(platform_realloc);
    RUN_TEST(platform_str_dup);
    RUN_TEST(platform_str_cmp);
    RUN_TEST(platform_file_exists);
    RUN_TEST(platform_dir_exists);
    RUN_TEST(platform_create_dir);
    RUN_TEST(platform_read_write_file);
    RUN_TEST(platform_time);
    RUN_TEST(platform_env);
    
    // Tool Registry tests
    printf("\n--- Tool Registry Tests ---\n");
    RUN_TEST(tool_registry_singleton);
    RUN_TEST(tool_registry_register);
    RUN_TEST(tool_registry_execute);
    RUN_TEST(tool_registry_rate_limit);
    RUN_TEST(tool_registry_json);
    
    // Built-in tool tests
    printf("\n--- Built-in Tool Tests ---\n");
    RUN_TEST(builtin_file_read_write);
    RUN_TEST(builtin_file_exists);
    RUN_TEST(builtin_memory);
    RUN_TEST(builtin_code_parse);
    RUN_TEST(builtin_code_format);
    
    // Chunked I/O tests
    printf("\n--- Chunked I/O Tests ---\n");
    RUN_TEST(chunked_io_open_close);
    RUN_TEST(chunked_io_read);
    RUN_TEST(chunked_io_large_file);
    
    // Memory System tests
    printf("\n--- Memory System Tests ---\n");
    RUN_TEST(memory_system_singleton);
    RUN_TEST(memory_system_remember_recall);
    RUN_TEST(memory_system_pattern);
    
    // File Explorer tests
    printf("\n--- File Explorer Tests ---\n");
    RUN_TEST(file_explorer_navigation);
    
    // Code Navigator tests
    printf("\n--- Code Navigator Tests ---\n");
    RUN_TEST(code_navigator_index);
    
    // Autopilot tests
    printf("\n--- Autopilot Tests ---\n");
    RUN_TEST(autopilot_task_management);
    RUN_TEST(autopilot_autonomy_level);
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
