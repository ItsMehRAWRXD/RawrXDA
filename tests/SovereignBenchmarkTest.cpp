// ============================================================================
// SovereignBenchmarkTest.cpp - Comprehensive Performance Validation
// ============================================================================

#include "../src/core/SovereignTextBuffer.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>

using namespace RawrXD;

class SovereignBenchmarkTest {
public:
    static void RunAllTests() {
        std::cout << "=== Sovereign Text Buffer Benchmark Suite ===\n\n";
        
        test_micro_operations();
        test_large_file_performance();
        test_multi_cursor_scenario();
        test_memory_efficiency();
        test_concurrent_access_patterns();
        test_real_world_workload();
        
        std::cout << "\n=== Benchmark Complete ===\n";
    }
    
private:
    static void test_micro_operations() {
        std::cout << "1. Micro-Operations Benchmark:\n";
        
        SovereignTextBuffer buffer;
        const size_t iterations = 100000;
        
        // Test single character insertion
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            buffer.insert(i, "x");
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double tps = iterations / (total_ms / 1000.0);
        
        std::cout << "   Single char insertion: " << tps << " ops/sec\n";
        
        // Test string insertion
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations / 10; ++i) {
            buffer.insert(i * 10, "hello world");
        }
        end = std::chrono::high_resolution_clock::now();
        
        total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        tps = (iterations / 10) / (total_ms / 1000.0);
        
        std::cout << "   String insertion: " << tps << " ops/sec\n";
        
        // Test deletion
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations / 2; ++i) {
            buffer.remove(i, 1);
        }
        end = std::chrono::high_resolution_clock::now();
        
        total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        tps = (iterations / 2) / (total_ms / 1000.0);
        
        std::cout << "   Deletion: " << tps << " ops/sec\n";
    }
    
    static void test_large_file_performance() {
        std::cout << "\n2. Large File Performance (10MB):\n";
        
        SovereignTextBuffer buffer;
        
        // Create 10MB content
        const size_t file_size = 10 * 1024 * 1024;
        std::string large_content(file_size, 'a');
        
        // Insert large content
        auto start = std::chrono::high_resolution_clock::now();
        buffer.insert(0, large_content);
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double mb_per_sec = (file_size / (1024.0 * 1024.0)) / (total_ms / 1000.0);
        
        std::cout << "   Bulk insertion: " << mb_per_sec << " MB/sec\n";
        
        // Test random access editing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, file_size - 100);
        
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            size_t pos = dist(gen);
            buffer.insert(pos, "edit");
        }
        end = std::chrono::high_resolution_clock::now();
        
        total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double edits_per_sec = 1000 / (total_ms / 1000.0);
        
        std::cout << "   Random access edits: " << edits_per_sec << " ops/sec\n";
        
        // Test line counting performance
        start = std::chrono::high_resolution_clock::now();
        size_t line_count = buffer.get_line_count();
        end = std::chrono::high_resolution_clock::now();
        
        total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "   Line count (" << line_count << " lines): " << total_ms << " ms\n";
    }
    
    static void test_multi_cursor_scenario() {
        std::cout << "\n3. Multi-cursor Simulation:\n";
        
        SovereignTextBuffer buffer;
        
        // Create base content
        buffer.insert(0, std::string(100000, 'x'));
        
        // Simulate multi-cursor edits (10 simultaneous insertion points)
        auto start = std::chrono::high_resolution_clock::now();
        
        buffer.begin_batch_edit();
        for (int i = 0; i < 10; ++i) {
            size_t pos = i * 10000; // 10 insertion points
            buffer.insert(pos, "multi_edit_" + std::to_string(i));
        }
        buffer.end_batch_edit();
        
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double batch_time_per_edit = total_ms / 10.0;
        
        std::cout << "   Batch multi-cursor edits: " << batch_time_per_edit << " ms per edit\n";
        
        // Test individual cursor movement performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            size_t pos = (i * 1000) % buffer.get_size();
            buffer.set_cursor_position(pos);
        }
        end = std::chrono::high_resolution_clock::now();
        
        total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double cursor_moves_per_sec = 100 / (total_ms / 1000.0);
        
        std::cout << "   Cursor movement: " << cursor_moves_per_sec << " moves/sec\n";
    }
    
    static void test_memory_efficiency() {
        std::cout << "\n4. Memory Efficiency Analysis:\n";
        
        SovereignTextBuffer buffer;
        
        // Track memory usage
        size_t initial_memory = get_process_memory();
        
        // Insert 1MB of content
        const size_t content_size = 1024 * 1024;
        std::string content(content_size, 'a');
        buffer.insert(0, content);
        
        size_t after_insert_memory = get_process_memory();
        size_t memory_used = after_insert_memory - initial_memory;
        
        std::cout << "   Memory for 1MB content: " << (memory_used / 1024.0) << " KB\n";
        std::cout << "   Memory efficiency: " << (content_size / (double)memory_used) << "x\n";
        
        // Test memory usage after edits
        for (int i = 0; i < 1000; ++i) {
            buffer.insert(i * 100, "edit");
        }
        
        size_t after_edits_memory = get_process_memory();
        size_t edit_memory_used = after_edits_memory - after_insert_memory;
        
        std::cout << "   Additional memory for 1000 edits: " << (edit_memory_used / 1024.0) << " KB\n";
        std::cout << "   Edit memory efficiency: " << (4000.0 / edit_memory_used) << "x\n";
    }
    
    static void test_concurrent_access_patterns() {
        std::cout << "\n5. Concurrent Access Patterns:\n";
        
        // Note: Full concurrent testing requires multi-threading implementation
        // This test simulates the pattern without actual threads
        
        SovereignTextBuffer buffer;
        buffer.insert(0, std::string(100000, 'b'));
        
        // Test read performance during writes
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t read_ops = 0;
        for (int i = 0; i < 10000; ++i) {
            // Simulated read operation
            char c = buffer.at(i % 1000);
            read_ops++;
            
            // Periodic writes
            if (i % 100 == 0) {
                buffer.insert(i, "w");
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double ops_per_sec = read_ops / (total_ms / 1000.0);
        
        std::cout << "   Mixed read/write operations: " << ops_per_sec << " ops/sec\n";
        std::cout << "   (Note: Full concurrency testing requires thread implementation)\n";
    }
    
    static void test_real_world_workload() {
        std::cout << "\n6. Real-world Workload Simulation:\n";
        
        SovereignTextBuffer buffer;
        
        // Simulate programming workflow: typing, deleting, navigating
        const std::vector<std::string> keywords = {
            "function", "class", "return", "if", "else", "for", "while", "const", "let", "var"
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> keyword_dist(0, keywords.size() - 1);
        std::uniform_int_distribution<size_t> pos_dist(0, 999);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Simulate 30 seconds of intensive editing
        size_t operations = 0;
        size_t current_pos = 0;
        
        for (int i = 0; i < 30000; ++i) { // 30k operations ~= 30 seconds
            int op_type = i % 10;
            
            switch (op_type) {
                case 0: // Insert keyword
                    buffer.insert(current_pos, keywords[keyword_dist(gen)]);
                    current_pos += keywords[keyword_dist(gen)].length();
                    break;
                case 1: // Insert punctuation
                    buffer.insert(current_pos, ";");
                    current_pos++;
                    break;
                case 2: // Insert newline
                    buffer.insert(current_pos, "\n");
                    current_pos++;
                    break;
                case 3: // Delete backward
                    if (current_pos > 0) {
                        buffer.remove(current_pos - 1, 1);
                        current_pos--;
                    }
                    break;
                case 4: // Navigate
                    current_pos = pos_dist(gen) % std::max(buffer.get_size(), size_t(1));
                    buffer.set_cursor_position(current_pos);
                    break;
                default: // Type normally
                    buffer.insert(current_pos, "x");
                    current_pos++;
                    break;
            }
            
            operations++;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        double total_seconds = std::chrono::duration<double>(end - start).count();
        double ops_per_sec = operations / total_seconds;
        
        std::cout << "   Real-world editing workload: " << ops_per_sec << " ops/sec\n";
        std::cout << "   Total operations: " << operations << " in " << total_seconds << " seconds\n";
        
        // Show final document statistics
        std::cout << "   Final document: " << buffer.get_size() << " characters, " 
                  << buffer.get_line_count() << " lines\n";
    }
    
    static size_t get_process_memory() {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
    }
};

int main() {
    try {
        SovereignBenchmarkTest::RunAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}