#include <iostream>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QString>
#include "agentic_tools.hpp"

int main(int argc, char* argv[])
{
    std::cout << "=== AgenticToolExecutor Validation ===" << std::endl;
    
    try {
        // Create executor
        AgenticToolExecutor executor;
        std::cout << "✓ AgenticToolExecutor created successfully" << std::endl;
        
        // Test 1: readFile
        QTemporaryDir tempDir;
        QString testFile = tempDir.path() + "/test.txt";
        QFile f(testFile);
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        f.write("Test content");
        f.close();
        
        ToolResult result = executor.readFile(testFile);
        if (result.success && result.output == "Test content") {
            std::cout << "✓ readFile works correctly" << std::endl;
        } else {
            std::cout << "✗ readFile failed: " << result.error.toStdString() << std::endl;
            return 1;
        }
        
        // Test 2: writeFile
        QString newFile = tempDir.path() + "/new.txt";
        result = executor.writeFile(newFile, "New content");
        if (result.success) {
            std::cout << "✓ writeFile works correctly" << std::endl;
        } else {
            std::cout << "✗ writeFile failed: " << result.error.toStdString() << std::endl;
            return 1;
        }
        
        // Test 3: listDirectory
        result = executor.listDirectory(tempDir.path());
        if (result.success && !result.output.isEmpty()) {
            std::cout << "✓ listDirectory works correctly" << std::endl;
        } else {
            std::cout << "✗ listDirectory failed: " << result.error.toStdString() << std::endl;
            return 1;
        }
        
        // Test 4: executeCommand
        result = executor.executeCommand("cmd.exe", QStringList() << "/c" << "echo" << "hello");
        if (result.success && result.output.contains("hello")) {
            std::cout << "✓ executeCommand works correctly" << std::endl;
        } else {
            std::cout << "✗ executeCommand failed: " << result.error.toStdString() << std::endl;
            return 1;
        }
        
        // Test 5: grepSearch
        result = executor.grepSearch("Test", tempDir.path());
        if (result.success) {
            std::cout << "✓ grepSearch works correctly" << std::endl;
        } else {
            std::cout << "✗ grepSearch failed: " << result.error.toStdString() << std::endl;
            return 1;
        }
        
        // Test 6: analyzeCode
        QString cppFile = tempDir.path() + "/test.cpp";
        QFile cf(cppFile);
        cf.open(QIODevice::WriteOnly | QIODevice::Text);
        cf.write("class MyClass { void method() {} }; void function() {}");
        cf.close();
        
        result = executor.analyzeCode(cppFile);
        if (result.success && result.output.contains("cpp")) {
            std::cout << "✓ analyzeCode works correctly" << std::endl;
        } else {
            std::cout << "✗ analyzeCode failed: " << result.error.toStdString() << std::endl;
            return 1;
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
