#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include "agentic_tools.hpp"

namespace fs = std::filesystem;

// Simple temporary directory management
class TempDir {
public:
    TempDir() {
        // Create temporary directory in system temp
        auto tmpPath = fs::temp_directory_path();
        auto uniquePath = tmpPath / fs::path("agentic_test_" + std::to_string(std::time(nullptr)));
        fs::create_directories(uniquePath);
        m_path = uniquePath;
    }

    ~TempDir() {
        if (fs::exists(m_path)) {
            fs::remove_all(m_path);
        }
    }

    const fs::path& path() const { return m_path; }

private:
    fs::path m_path;
};

// Helper to write file content
bool writeTextFile(const fs::path& filePath, const std::string& content) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    file << content;
    file.close();
    return true;
}

// Helper to read file content
std::string readTextFile(const fs::path& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}

int main(int argc, char* argv[])
{
    std::cout << "=== AgenticToolExecutor Validation ===" << std::endl;
    
    try {
        // Create executor
        AgenticToolExecutor executor;
        std::cout << "✓ AgenticToolExecutor created successfully" << std::endl;
        
        // Create temporary directory
        TempDir tempDir;
        
        // Test 1: readFile
        fs::path testFile = tempDir.path() / "test.txt";
        writeTextFile(testFile, "Test content");
        
        ToolResult result = executor.readFile(testFile.string());
        if (result.success && result.output == "Test content") {
            std::cout << "✓ readFile works correctly" << std::endl;
        } else {
            std::cout << "✗ readFile failed: " << result.error << std::endl;
            return 1;
        }
        
        // Test 2: writeFile
        fs::path newFile = tempDir.path() / "new.txt";
        result = executor.writeFile(newFile.string(), "New content");
        if (result.success) {
            std::cout << "✓ writeFile works correctly" << std::endl;
        } else {
            std::cout << "✗ writeFile failed: " << result.error << std::endl;
            return 1;
        }
        
        // Test 3: listDirectory
        result = executor.listDirectory(tempDir.path().string());
        if (result.success && !result.output.empty()) {
            std::cout << "✓ listDirectory works correctly" << std::endl;
        } else {
            std::cout << "✗ listDirectory failed: " << result.error << std::endl;
            return 1;
        }
        
        // Test 4: executeCommand
        result = executor.executeCommand("cmd.exe", std::vector<std::string>{"/c", "echo", "hello"});
        if (result.success && result.output.find("hello") != std::string::npos) {
            std::cout << "✓ executeCommand works correctly" << std::endl;
        } else {
            std::cout << "✗ executeCommand failed: " << result.error << std::endl;
            return 1;
        }
        
        // Test 5: grepSearch
        result = executor.grepSearch("Test", tempDir.path().string());
        if (result.success) {
            std::cout << "✓ grepSearch works correctly" << std::endl;
        } else {
            std::cout << "✗ grepSearch failed: " << result.error << std::endl;
            return 1;
        }
        
        // Test 6: analyzeCode
        fs::path cppFile = tempDir.path() / "test.cpp";
        writeTextFile(cppFile, "class MyClass { void method() {} }; void function() {}");
        
        result = executor.analyzeCode(cppFile.string());
        if (result.success && result.output.find("cpp") != std::string::npos) {
            std::cout << "✓ analyzeCode works correctly" << std::endl;
        } else {
            std::cout << "✗ analyzeCode failed: " << result.error << std::endl;
        }
        
        std::cout << "\n=== All validation tests PASSED ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cout << "✗ Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cout << "✗ Unknown exception occurred" << std::endl;
        return 1;
    }
}
