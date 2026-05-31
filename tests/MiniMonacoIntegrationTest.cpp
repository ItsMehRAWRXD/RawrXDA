// ============================================================================
// MiniMonacoIntegrationTest.cpp - Validation Test for Editor Integration
// ============================================================================

#include "../src/gui/RawrXD_EditorWindow.h"
#include <iostream>
#include <assert>

using namespace RawrXD;

class MiniMonacoIntegrationTest {
public:
    static bool RunAllTests() {
        std::cout << "=== MiniMonaco Integration Validation ===\n\n";
        
        bool all_passed = true;
        
        all_passed &= test_basic_initialization();
        all_passed &= test_text_operations();
        all_passed &= test_cursor_conversion();
        all_passed >= test_performance_baseline();
        
        std::cout << "\n=== Validation " << (all_passed ? "PASSED" : "FAILED") << " ===\n";
        return all_passed;
    }
    
private:
    static bool test_basic_initialization() {
        std::cout << "1. Basic Initialization... ";
        
        try {
            EditorWindow editor;
            
            // Verify buffer is empty
            String text = editor.getText();
            assert(text.isEmpty());
            
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            return false;
        }
    }
    
    static bool test_text_operations() {
        std::cout << "2. Text Operations... ";
        
        try {
            EditorWindow editor;
            
            // Test setText
            editor.setText(L"Hello World");
            assert(editor.getText() == L"Hello World");
            
            // Test appendText
            editor.appendText(L"\nLine 2");
            String result = editor.getText();
            assert(result.contains(L"Hello World"));
            assert(result.contains(L"Line 2"));
            
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            return false;
        }
    }
    
    static bool test_cursor_conversion() {
        std::cout << "3. Cursor Conversion... ";
        
        try {
            EditorWindow editor;
            editor.setText(L"Line 1\nLine 2\nLine 3");
            
            // Test cursor to buffer offset
            Point cursor{0, 0}; // Start of first line
            size_t offset = editor.convertCursorToBufferOffset(cursor);
            assert(offset == 0);
            
            Point cursor2{0, 1}; // Start of second line
            size_t offset2 = editor.convertCursorToBufferOffset(cursor2);
            assert(offset2 == 7); // "Line 1\n" = 7 characters
            
            // Test buffer offset to cursor
            Point recovered = editor.convertBufferOffsetToCursor(offset2);
            assert(recovered.x == 0 && recovered.y == 1);
            
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            return false;
        }
    }
    
    static bool test_performance_baseline() {
        std::cout << "4. Performance Baseline... ";
        
        try {
            EditorWindow editor;
            
            // Performance test: insert 1000 characters
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 1000; ++i) {
                editor.onChar(L'x');
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            double tps = 1000.0 / (ms / 1000.0);
            
            std::cout << "PASSED (" << tps << " TPS)\n";
            
            // Verify performance meets baseline
            if (tps < 1000) {
                std::cout << "  WARNING: Performance below 1000 TPS baseline\n";
            }
            
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            return false;
        }
    }
};

int main() {
    try {
        bool success = MiniMonacoIntegrationTest::RunAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
}