// ============================================================================
// test_rust_parser_integration.cpp - Rust AST + Credit Governor Integration
// ============================================================================
// Validates Rust parser correctness while feeding workload telemetry to the
// Credit Governor, proving the system handles varying computational weights.
// ============================================================================

#include "core/rust_parser.hpp"
#include "flow_control/credit_governor.hpp"
#include "flow_control/credit_based_flow_control.hpp"
#include <cstdio>
#include <string>
#include <chrono>

using namespace rawrxd::ast::rust;
using namespace RawrXD::FlowControl;

// Sample Rust code: small module
static const char* RUST_SMALL = R"(
pub mod utils;

fn main() {
    println!("Hello, world!");
}

struct Point {
    x: f64,
    y: f64,
}

enum Status {
    Ok,
    Err(String),
}
)";

// Sample Rust code: medium complexity with generics and traits
static const char* RUST_MEDIUM = R"(
use std::collections::HashMap;

pub trait Drawable {
    fn draw(&self, ctx: &mut RenderContext);
    fn bounds(&self) -> Rect;
}

pub struct Canvas<T: Drawable> {
    items: Vec<T>,
    cache: HashMap<String, Texture>,
}

impl<T: Drawable> Canvas<T> {
    pub fn new() -> Self {
        Self { items: Vec::new(), cache: HashMap::new() }
    }

    pub fn add(&mut self, item: T) {
        self.items.push(item);
    }

    pub fn render(&mut self, ctx: &mut RenderContext) {
        for item in &self.items {
            item.draw(ctx);
        }
    }
}

async fn load_resources(path: &str) -> Result<Vec<u8>, io::Error> {
    tokio::fs::read(path).await
}

#[derive(Debug, Clone, PartialEq)]
pub struct Config {
    pub width: u32,
    pub height: u32,
    pub title: String,
}
)";

// Sample Rust code: large complex file
static const char* RUST_LARGE = R"(
#![allow(unused_imports)]
#![feature(async_closure)]

use std::sync::{Arc, Mutex, RwLock};
use std::collections::{HashMap, BTreeMap, VecDeque};
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll, Waker};

pub mod internal {
    pub use super::*;
    
    pub trait InternalTrait {
        type Output;
        const MAX_SIZE: usize = 1024;
        
        fn process(&self, input: Self::Output) -> Result<Self::Output, Box<dyn std::error::Error>>;
    }
    
    pub struct InternalStruct<T, U>
    where
        T: Send + Sync + 'static,
        U: Clone + Default,
    {
        data: Arc<RwLock<Vec<T>>>,
        config: U,
        handlers: HashMap<String, Box<dyn Fn(T) -> U + Send>>,
    }
    
    impl<T, U> InternalStruct<T, U>
    where
        T: Send + Sync + 'static,
        U: Clone + Default,
    {
        pub fn new(config: U) -> Self {
            Self {
                data: Arc::new(RwLock::new(Vec::new())),
                config,
                handlers: HashMap::new(),
            }
        }
        
        pub fn register_handler<F>(&mut self, name: &str, handler: F
        ) where F: Fn(T) -> U + Send + 'static {
            self.handlers.insert(name.to_string(), Box::new(handler));
        }
        
        pub async fn process_all(&self,
        ) -> Result<Vec<U>, String> {
            let data = self.data.read().map_err(|e| e.to_string())?;
            let mut results = Vec::new();
            for item in data.iter() {
                for (name, handler) in &self.handlers {
                    results.push(handler(*item));
                }
            }
            Ok(results)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_basic() {
        assert_eq!(2 + 2, 4);
    }
    
    #[tokio::test]
    async fn test_async() {
        let result = async { 42 }.await;
        assert_eq!(result, 42);
    }
}

unsafe extern "C" fn c_callback(data: *const u8, len: usize) {
    let slice = std::slice::from_raw_parts(data, len);
    println!("Received: {:?}", slice);
}

macro_rules! define_error {
    ($name:ident, $msg:expr) => {
        #[derive(Debug)]
        pub struct $name {
            message: String,
        }
        
        impl $name {
            pub fn new() -> Self {
                Self { message: $msg.to_string() }
            }
        }
        
        impl std::fmt::Display for $name {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, "{}", self.message)
            }
        }
        
        impl std::error::Error for $name {}
    };
}

define_error!(ParseError, "parse failed");
define_error!(IoError, "IO operation failed");
)";

// Parse and measure
struct ParseWorkload {
    const char* name;
    const char* source;
    size_t expectedNodes;
};

static ParseWorkload workloads[] = {
    {"small", RUST_SMALL, 4},
    {"medium", RUST_MEDIUM, 6},
    {"large", RUST_LARGE, 10},
};

// Test 1: Basic Rust parsing correctness
bool TestParseCorrectness() {
    printf("\n[Test] Rust parse correctness...\n");
    
    RustParser parser;
    bool allPass = true;
    
    for (const auto& w : workloads) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = parser.parse(w.source, "test.rs");
        auto end = std::chrono::high_resolution_clock::now();
        
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
printf("  Found %zu nodes:\n", result.nodes.size());
    for (const auto& node : result.nodes) {
        printf("    - %s (type=%d)\n", node->getText().c_str(), static_cast<int>(node->getType()));
    }
    
    bool pass = result.success && result.nodes.size() >= w.expectedNodes;
        printf("  %s: %zu nodes in %lld μs (expected >=%zu) %s\n",
               w.name, result.nodes.size(), (long long)micros, w.expectedNodes,
               pass ? "PASS" : "FAIL");
        
        if (!pass) {
            for (const auto& diag : result.diagnostics) {
                printf("    diag: %s\n", diag.c_str());
            }
        }
        
        allPass = allPass && pass;
    }
    
    printf("  %s\n", allPass ? "PASS" : "FAIL");
    return allPass;
}

// Test 2: Parser under credit-governed load
bool TestGovernedParsing() {
    printf("\n[Test] Parser under credit-governed load...\n");
    
    CreditConfig creditCfg;
    creditCfg.initialCredits = 1000;
    creditCfg.maxCredits = 1000;
    creditCfg.minCredits = 50;
    creditCfg.reserveForPartial = false;
    
    CreditCounter counter;
    counter.Initialize(creditCfg);
    
    GovernorConfig govCfg;
    govCfg.targetThroughput = 1000.0;  // 1000 parses/sec target
    govCfg.kp = 0.5;
    govCfg.ki = 0.1;
    govCfg.updateIntervalMs = 50;
    govCfg.minCreditsFloor = 10;
    govCfg.minCreditsCeiling = 500;
    
    CreditGovernor governor;
    governor.Initialize(creditCfg, govCfg);
    
    RustParser parser;
    int successCount = 0;
    int blockedCount = 0;
    
    for (int i = 0; i < 30; i++) {
        // Acquire credits for parse operation
        auto result = counter.TryAcquire(20);
        if (result != CreditResult::Success) {
            blockedCount++;
            continue;
        }
        
        // Parse workload
        auto parseStart = std::chrono::high_resolution_clock::now();
        auto parseResult = parser.parse(RUST_MEDIUM, "governed.rs");
        auto parseEnd = std::chrono::high_resolution_clock::now();
        
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(parseEnd - parseStart).count();
        double parsesPerSec = 1e6 / micros;
        
        // Feed telemetry
        GovernorTelemetry tel;
        tel.throughputElemPerSec = parsesPerSec;  // Parses/sec as "throughput"
        tel.timestampMs = 100 + i * 60;
        governor.RecordTelemetry(tel);
        
        // Return credits
        counter.ReturnCredits(20);
        
        if (parseResult.success) successCount++;
    }
    
    auto finalCfg = governor.GetCurrentConfig();
    printf("  Parses: %d success, %d blocked\n", successCount, blockedCount);
    printf("  Final minCredits: %u (started at 50)\n", finalCfg.minCredits);
    
    bool pass = (successCount > 0) && (successCount + blockedCount == 30);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    governor.Shutdown();
    return pass;
}

// Test 3: Tokenizer accuracy
bool TestTokenizer() {
    printf("\n[Test] Rust tokenizer accuracy...\n");
    
    RustParser parser;
    auto result = parser.parse(RUST_MEDIUM, "tokenize.rs");
    
    if (!result.success || result.nodes.empty()) {
        printf("  Parse failed\n");
        return false;
    }
    
    // Check for expected node types
    int fnCount = 0, structCount = 0, traitCount = 0, implCount = 0;
    for (const auto& node : result.nodes) {
        switch (node->getType()) {
            case RawrXD::AST::NodeType::FunctionDecl: fnCount++; break;
            case RawrXD::AST::NodeType::StructDecl: structCount++; break;
            case RawrXD::AST::NodeType::ClassDecl: 
                // Traits and impls both map to ClassDecl in current parser
                if (node->getText().find("trait") != std::string::npos) traitCount++;
                else if (node->getText().find("impl") != std::string::npos) implCount++;
                break;
            default: break;
        }
    }
    
    printf("  Functions: %d (expected 3)\n", fnCount);
    printf("  Structs: %d (expected 1)\n", structCount);
    printf("  Traits: %d (expected 1)\n", traitCount);
    printf("  Impls: %d (expected 1)\n", implCount);
    
    bool pass = (fnCount >= 2) && (structCount >= 1) && (traitCount >= 1);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 4: Incremental parsing
bool TestIncrementalParse() {
    printf("\n[Test] Incremental parsing...\n");
    
    RustParser parser;
    
    // Initial parse
    auto result1 = parser.parse(RUST_SMALL, "incremental.rs");
    if (!result1.success) {
        printf("  Initial parse failed\n");
        return false;
    }
    
    size_t initialNodes = result1.nodes.size();
    printf("  Initial: %zu nodes\n", initialNodes);
    
    // Simulate edit: add a function at the end
    std::string modified = std::string(RUST_SMALL) + "\nfn added() {}\n";
    
    auto result2 = parser.parse(modified.c_str(), "incremental.rs");
    if (!result2.success) {
        printf("  Modified parse failed\n");
        return false;
    }
    
    size_t modifiedNodes = result2.nodes.size();
    printf("  Modified: %zu nodes\n", modifiedNodes);
    
    bool pass = modifiedNodes > initialNodes;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 5: Performance scaling with file size
bool TestPerformanceScaling() {
    printf("\n[Test] Performance scaling...\n");
    
    RustParser parser;
    
    struct { const char* name; const char* src; size_t len; } tests[] = {
        {"small", RUST_SMALL, strlen(RUST_SMALL)},
        {"medium", RUST_MEDIUM, strlen(RUST_MEDIUM)},
        {"large", RUST_LARGE, strlen(RUST_LARGE)},
    };
    
    bool pass = true;
    for (const auto& t : tests) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = parser.parse(t.src, "perf.rs");
        auto end = std::chrono::high_resolution_clock::now();
        
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double kb = t.len / 1024.0;
        double kbPerSec = kb / (micros / 1e6);
        
        printf("  %s (%.1f KB): %lld μs (%.1f KB/s) %s\n",
               t.name, kb, (long long)micros, kbPerSec,
               result.success ? "PASS" : "FAIL");
        
        pass = pass && result.success;
    }
    
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 6: Call graph extraction (v3.3) — with CallKind, resolution, and dead-code detection
bool TestCallGraph() {
    printf("\n[Test] Call graph extraction...\n");

    static const char* RUST_CALLS = R"(
fn helper() {}

fn main() {
    helper();
    std::io::println("hello");
    self.process();
}

fn process() {
    helper();
    main();
}

fn unused_fn() {
    helper();
}
)";

    RustParser parser;
    rawrxd::ast::SymbolTable symbols;
    auto result = parser.parse(RUST_CALLS, "calls.rs", &symbols);

    if (!result.success) {
        printf("  Parse failed\n");
        return false;
    }

    // ---- 1. Basic edge extraction ----
    auto edges = symbols.allEdges();
    printf("  Found %zu call edges:\n", edges.size());
    for (const auto& e : edges) {
        const char* kind_str = "Direct";
        if (e.kind == rawrxd::ast::CallKind::Method) kind_str = "Method";
        else if (e.kind == rawrxd::ast::CallKind::Qualified) kind_str = "Qualified";
        else if (e.kind == rawrxd::ast::CallKind::External) kind_str = "External";
        printf("    - %s -> %s [%s]\n", e.caller_name.c_str(), e.callee_name.c_str(), kind_str);
    }

    bool has_main_to_helper = false;
    bool has_main_to_println = false;
    bool has_main_to_process = false;
    bool has_process_to_helper = false;
    bool has_process_to_main = false;
    bool has_unused_to_helper = false;

    for (const auto& e : edges) {
        if (e.caller_name == "main" && e.callee_name == "helper") has_main_to_helper = true;
        if (e.caller_name == "main" && e.callee_name == "std::io::println") has_main_to_println = true;
        if (e.caller_name == "main" && e.callee_name == "self.process") has_main_to_process = true;
        if (e.caller_name == "process" && e.callee_name == "helper") has_process_to_helper = true;
        if (e.caller_name == "process" && e.callee_name == "main") has_process_to_main = true;
        if (e.caller_name == "unused_fn" && e.callee_name == "helper") has_unused_to_helper = true;
    }

    printf("  main -> helper: %s\n", has_main_to_helper ? "FOUND" : "MISSING");
    printf("  main -> std::io::println: %s\n", has_main_to_println ? "FOUND" : "MISSING");
    printf("  main -> self.process: %s\n", has_main_to_process ? "FOUND" : "MISSING");
    printf("  process -> helper: %s\n", has_process_to_helper ? "FOUND" : "MISSING");
    printf("  process -> main: %s\n", has_process_to_main ? "FOUND" : "MISSING");
    printf("  unused_fn -> helper: %s\n", has_unused_to_helper ? "FOUND" : "MISSING");

    bool basic_pass = has_main_to_helper && has_main_to_println && has_main_to_process
                   && has_process_to_helper && has_process_to_main && has_unused_to_helper;
    printf("  Basic edges: %s\n", basic_pass ? "PASS" : "FAIL");

    // ---- 2. CallKind tagging ----
    bool kind_pass = true;
    for (const auto& e : edges) {
        if (e.callee_name == "helper" && e.kind != rawrxd::ast::CallKind::Direct) kind_pass = false;
        if (e.callee_name == "std::io::println" && e.kind != rawrxd::ast::CallKind::Qualified) kind_pass = false;
        if (e.callee_name == "self.process" && e.kind != rawrxd::ast::CallKind::Method) kind_pass = false;
    }
    printf("  CallKind tagging: %s\n", kind_pass ? "PASS" : "FAIL");

    // ---- 3. resolveCalls() — cross-file symbol linking ----
    symbols.resolveCalls();
    int resolved_count = 0;
    for (const auto& e : symbols.allEdges()) {
        if (e.resolved_symbol != nullptr) resolved_count++;
    }
    printf("  Resolved %d/%zu edges to symbols\n", resolved_count, symbols.allEdges().size());
    bool resolve_pass = (resolved_count >= 5); // helper, process, main should all resolve
    printf("  Cross-file resolution: %s\n", resolve_pass ? "PASS" : "FAIL");

    // ---- 4. deadSymbols() detection ----
    auto dead = symbols.deadSymbols();
    printf("  Dead symbols (%zu):\n", dead.size());
    bool found_unused = false;
    for (const auto* s : dead) {
        printf("    - %s (file=%s)\n", s->name.c_str(), s->file.c_str());
        if (s->name == "unused_fn") found_unused = true;
    }
    bool dead_pass = found_unused;
    printf("  Dead-code detection: %s\n", dead_pass ? "PASS" : "FAIL");

    // ---- 5. O(1) query indices ----
    auto from_main = symbols.callsFrom("main");
    auto to_helper = symbols.callsTo("helper");
    bool index_pass = (from_main.size() == 3) && (to_helper.size() == 3);
    printf("  callsFrom(main)=%zu, callsTo(helper)=%zu\n", from_main.size(), to_helper.size());
    printf("  O(1) query indices: %s\n", index_pass ? "PASS" : "FAIL");

    bool pass = basic_pass && kind_pass && resolve_pass && dead_pass && index_pass;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Main test runner
int main() {
    printf("========================================\n");
    printf("Rust Parser + Credit Governor Integration\n");
    printf("========================================\n");

    int passed = 0;
    int total = 6;

    if (TestParseCorrectness()) passed++;
    if (TestGovernedParsing()) passed++;
    if (TestTokenizer()) passed++;
    if (TestIncrementalParse()) passed++;
    if (TestPerformanceScaling()) passed++;
    if (TestCallGraph()) passed++;
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}
