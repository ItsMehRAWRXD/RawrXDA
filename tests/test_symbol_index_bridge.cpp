// ============================================================================
// test_symbol_index_bridge.cpp — Unit test for Symbol Index Bridge
// ============================================================================

#include "bridge/symbol_index_bridge.hpp"
#include <iostream>
#include <assert>
#include <string>

using namespace rawrxd::bridge;

// Test source code
const char* TEST_SOURCE = R"(
pub fn main() {
    let x = 42;
    let y = "hello";
    print_hello(x);
}

fn print_hello(n: i32) {
    println!("{}", n);
}

struct Point {
    x: f64,
    y: f64,
}

impl Point {
    fn distance(&self, other: &Point) -> f64 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        (dx * dx + dy * dy).sqrt()
    }
}
)";

// ============================================================================
// Test 1: Basic initialization
// ============================================================================
void test_initialization() {
    std::cout << "[Test 1] Initialization... ";
    
    SymbolIndexBridge bridge;
    assert(bridge.initialize());
    assert(bridge.isInitialized());
    
    bridge.shutdown();
    assert(!bridge.isInitialized());
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 2: File indexing
// ============================================================================
void test_file_indexing() {
    std::cout << "[Test 2] File indexing... ";
    
    SymbolIndexBridge bridge;
    bridge.initialize();
    
    assert(bridge.indexFile("test.rs", TEST_SOURCE));
    assert(bridge.isFileIndexed("test.rs"));
    assert(bridge.getIndexedFileCount() == 1);
    assert(bridge.getFileVersion("test.rs") == 1);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 3: Query completions
// ============================================================================
void test_query_completions() {
    std::cout << "[Test 3] Query completions... ";
    
    SymbolIndexBridge bridge;
    bridge.initialize();
    bridge.indexFile("test.rs", TEST_SOURCE);
    
    // Query for "pri" prefix (should match "print_hello", "println")
    auto results = bridge.queryCompletions("test.rs", "pri", 10, 5);
    
    assert(!results.empty());
    
    // Check that we got relevant results
    bool found_print_hello = false;
    for (const auto& cand : results) {
        if (cand.name == "print_hello") {
            found_print_hello = true;
            assert(cand.kind == CompletionKind::Function);
            assert(!cand.signature.empty());
            break;
        }
    }
    assert(found_print_hello);
    
    std::cout << "PASS (" << results.size() << " candidates)\n";
}

// ============================================================================
// Test 4: Trigger detection
// ============================================================================
void test_trigger_detection() {
    std::cout << "[Test 4] Trigger detection... ";
    
    SymbolIndexBridge bridge;
    bridge.initialize();
    
    // Test identifier trigger
    std::string code1 = "let pri";
    auto trigger1 = bridge.detectTrigger(code1, code1.length());
    assert(trigger1.kind == TriggerKind::Identifier);
    assert(trigger1.prefix == "pri");
    
    // Test scope resolution trigger
    std::string code2 = "std::";
    auto trigger2 = bridge.detectTrigger(code2, code2.length());
    assert(trigger2.kind == TriggerKind::ScopeResolution);
    assert(trigger2.prefix == "std");
    
    // Test method call trigger
    std::string code3 = "obj.";
    auto trigger3 = bridge.detectTrigger(code3, code3.length());
    assert(trigger3.kind == TriggerKind::MethodCall);
    assert(trigger3.prefix == "obj");
    
    // Test no trigger
    std::string code4 = "let ";
    auto trigger4 = bridge.detectTrigger(code4, code4.length());
    assert(trigger4.kind == TriggerKind::None);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 5: C API
// ============================================================================
void test_c_api() {
    std::cout << "[Test 5] C API... ";
    
    // Create bridge
    auto* handle = rawrxd_symbol_index_create();
    assert(handle != nullptr);
    
    // Index file
    int result = rawrxd_symbol_index_file(handle, "test.rs", TEST_SOURCE);
    assert(result == 1);
    
    // Query completions
    size_t count = 0;
    auto* candidates = rawrxd_symbol_query_completions(
        handle, "test.rs", "pri", 10, 5, &count);
    
    assert(candidates != nullptr);
    assert(count > 0);
    
    // Check first candidate
    assert(candidates[0].name != nullptr);
    assert(strlen(candidates[0].name) > 0);
    
    // Free candidates
    rawrxd_symbol_candidates_free(candidates, count);
    
    // Detect trigger
    auto trigger = rawrxd_symbol_detect_trigger(handle, "let pri", 7);
    assert(trigger.kind == static_cast<int>(TriggerKind::Identifier));
    assert(strcmp(trigger.prefix, "pri") == 0);
    
    // Destroy bridge
    rawrxd_symbol_index_destroy(handle);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Test 6: Re-indexing (incremental update)
// ============================================================================
void test_reindexing() {
    std::cout << "[Test 6] Re-indexing... ";
    
    SymbolIndexBridge bridge;
    bridge.initialize();
    
    // Index first version
    assert(bridge.indexFile("test.rs", TEST_SOURCE));
    assert(bridge.getFileVersion("test.rs") == 1);
    
    // Re-index with updated source
    const char* updated_source = R"(
pub fn main() {
    let x = 42;
    let y = "hello";
    let z = true;
    print_hello(x);
    print_world(y);
}

fn print_hello(n: i32) {
    println!("{}", n);
}

fn print_world(s: &str) {
    println!("{}", s);
}
)";
    
    assert(bridge.indexFile("test.rs", updated_source));
    assert(bridge.getFileVersion("test.rs") == 2);
    
    // Query should find new function
    auto results = bridge.queryCompletions("test.rs", "pri", 10, 5);
    bool found_print_world = false;
    for (const auto& cand : results) {
        if (cand.name == "print_world") {
            found_print_world = true;
            break;
        }
    }
    assert(found_print_world);
    
    std::cout << "PASS\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Symbol Index Bridge Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_initialization();
        test_file_indexing();
        test_query_completions();
        test_trigger_detection();
        test_c_api();
        test_reindexing();
        
        std::cout << "\n========================================\n";
        std::cout << "All tests PASSED ✅\n";
        std::cout << "========================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n========================================\n";
        std::cerr << "Test FAILED ❌\n";
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "========================================\n";
        return 1;
    }
}
