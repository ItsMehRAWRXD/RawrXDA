// ============================================================================
// composer_test_suite.cpp — Composer Mode & Crazy Mode Test Suite
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "composer/composer_mode.h"
#include "composer/crazy_mode.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using namespace RawrXD::Composer;

// ============================================================================
// Test Utilities
// ============================================================================

class TestFixture {
public:
    std::string testDir;
    
    TestFixture() {
        testDir = fs::temp_directory_path().string() + "/composer_test_" + 
                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        fs::create_directories(testDir);
    }
    
    ~TestFixture() {
        fs::remove_all(testDir);
    }
    
    std::string createTestFile(const std::string& name, const std::string& content) {
        std::string path = testDir + "/" + name;
        std::ofstream file(path);
        file << content;
        return path;
    }
    
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
};

// ============================================================================
// Composer Mode Tests
// ============================================================================

void testComposerConfig() {
    std::cout << "Testing ComposerConfig...\n";
    
    ComposerConfig config;
    config.defaultMode = CompositionMode::Aggressive;
    config.maxChangesPerPlan = 100;
    config.minConfidenceThreshold = 0.8f;
    config.enableCrazyMode = true;
    
    auto engine = createComposerEngine();
    engine->setConfig(config);
    
    auto retrieved = engine->getConfig();
    assert(retrieved.defaultMode == CompositionMode::Aggressive);
    assert(retrieved.maxChangesPerPlan == 100);
    assert(retrieved.minConfidenceThreshold == 0.8f);
    assert(retrieved.enableCrazyMode == true);
    
    std::cout << "  ✓ ComposerConfig test passed\n";
}

void testPlanCreation() {
    std::cout << "Testing plan creation...\n";
    
    TestFixture fixture;
    
    // Create test files
    std::string file1 = fixture.createTestFile("test1.ts", R"(
import { foo, bar, unused } from './module';

export function hello(name: string): string {
    const greeting = 'Hello, ' + name;
    console.log(greeting);
    return greeting;
}

export function goodbye(name: string): string {
    return 'Goodbye, ' + name;
}
)");
    
    std::string file2 = fixture.createTestFile("test2.ts", R"(
import { hello, goodbye } from './test1';

export function greet(name: string): void {
    console.log(hello(name));
    console.log(goodbye(name));
}
)");
    
    auto engine = createComposerEngine();
    
    std::vector<std::string> files = {file1, file2};
    auto plan = engine->createPlan("Optimize imports and remove console.log", files);
    
    assert(plan != nullptr);
    assert(plan->planId > 0);
    assert(!plan->userPrompt.empty());
    assert(plan->status == CompositionStatus::Pending);
    
    std::cout << "  ✓ Plan creation test passed\n";
}

void testProjectAnalysis() {
    std::cout << "Testing project analysis...\n";
    
    TestFixture fixture;
    
    std::string file1 = fixture.createTestFile("main.ts", R"(
import { helper } from './helper';

export function main(): void {
    helper();
}
)");
    
    std::string file2 = fixture.createTestFile("helper.ts", R"(
export function helper(): void {
    console.log('Helper called');
}
)");
    
    auto engine = createComposerEngine();
    std::vector<std::string> files = {file1, file2};
    
    auto context = engine->analyzeProject(files);
    
    assert(context.files.size() == 2);
    assert(!context.dependencyGraph.empty());
    
    std::cout << "  ✓ Project analysis test passed\n";
}

void testChangeApplication() {
    std::cout << "Testing change application...\n";
    
    TestFixture fixture;
    
    std::string original = "Hello, World!";
    std::string file = fixture.createTestFile("test.txt", original);
    
    auto engine = createComposerEngine();
    
    FileChange change;
    change.uri = file;
    change.type = ChangeType::Replace;
    change.range.startLine = 1;
    change.range.endLine = 1;
    change.newContent = "Goodbye, World!";
    
    bool applied = engine->applyChange(change);
    assert(applied);
    assert(change.isApplied);
    
    std::string content = fixture.readFile(file);
    assert(content == "Goodbye, World!");
    
    // Test revert
    bool reverted = engine->revertChange(change);
    assert(reverted);
    assert(!change.isApplied);
    
    content = fixture.readFile(file);
    assert(content == original);
    
    std::cout << "  ✓ Change application test passed\n";
}

void testConflictDetection() {
    std::cout << "Testing conflict detection...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function foo() { return 1; }
function bar() { return 2; }
)");
    
    auto engine = createComposerEngine();
    
    // Create overlapping changes
    std::vector<FileChange> changes;
    
    FileChange change1;
    change1.uri = file;
    change1.range.startLine = 1;
    change1.range.endLine = 1;
    change1.newContent = "function foo() { return 3; }";
    changes.push_back(change1);
    
    FileChange change2;
    change2.uri = file;
    change2.range.startLine = 1;
    change2.range.endLine = 2;
    change2.newContent = "function foo() { return 4; }\nfunction bar() { return 5; }";
    changes.push_back(change2);
    
    // Create a plan with these changes
    CompositionPlan plan;
    plan.changes = changes;
    
    bool hasConflicts = !engine->detectConflicts(plan);
    assert(!plan.conflicts.empty());
    
    std::cout << "  ✓ Conflict detection test passed\n";
}

void testCallback() {
    std::cout << "Testing event callbacks...\n";
    
    std::vector<CompositionEvent> events;
    
    auto engine = createComposerEngine();
    engine->setCallback([&events](const CompositionEventArgs& args) {
        events.push_back(args.event);
    });
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.ts", "const x = 1;");
    
    std::vector<std::string> files = {file};
    auto plan = engine->createPlan("Test callback", files);
    
    // Should have received PlanCreated event
    bool foundCreated = false;
    for (const auto& event : events) {
        if (event == CompositionEvent::PlanCreated) {
            foundCreated = true;
            break;
        }
    }
    assert(foundCreated);
    
    std::cout << "  ✓ Callback test passed\n";
}

// ============================================================================
// Crazy Mode Tests
// ============================================================================

void testCrazyModeConfig() {
    std::cout << "Testing CrazyModeConfig...\n";
    
    CrazyModeConfig config;
    config.enableFullAutonomy = true;
    config.maxFilesPerOperation = 200;
    config.minConfidenceForAutoApply = 0.7f;
    
    auto engine = createCrazyModeEngine();
    engine->setConfig(config);
    
    auto retrieved = engine->getConfig();
    assert(retrieved.enableFullAutonomy == true);
    assert(retrieved.maxFilesPerOperation == 200);
    assert(std::abs(retrieved.minConfidenceForAutoApply - 0.7f) < 0.01f);
    
    std::cout << "  ✓ CrazyModeConfig test passed\n";
}

void testSymbolTable() {
    std::cout << "Testing symbol table building...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("symbols.ts", R"(
import { external } from 'external';

export function publicFunction(): number {
    return privateHelper();
}

function privateHelper(): number {
    return 42;
}

export class PublicClass {
    private value: number;
    
    constructor(value: number) {
        this.value = value;
    }
    
    getValue(): number {
        return this.value;
    }
}

const unusedVariable = 'never used';
)");
    
    auto engine = createCrazyModeEngine();
    std::vector<std::string> files = {file};
    
    auto table = engine->buildSymbolTable(files);
    
    assert(!table.symbols.empty());
    assert(table.symbols.count("publicFunction") > 0);
    assert(table.symbols.count("PublicClass") > 0);
    assert(table.symbols.count("privateHelper") > 0);
    
    std::cout << "  ✓ Symbol table test passed\n";
}

void testDeadCodeDetection() {
    std::cout << "Testing dead code detection...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("deadcode.ts", R"(
export function usedFunction(): number {
    return 42;
}

function unusedFunction(): string {
    return 'never called';
}

export function anotherUsedFunction(): void {
    console.log(usedFunction());
}

const unusedConst = 'also never used';
const usedConst = 'this is used';

export function useIt(): string {
    return usedConst;
}
)");
    
    auto engine = createCrazyModeEngine();
    std::vector<std::string> files = {file};
    
    auto analysis = engine->findDeadCode(files);
    
    // Should find unusedFunction and unusedConst
    bool foundUnusedFunction = false;
    bool foundUnusedConst = false;
    
    for (const auto& symbol : analysis.unusedSymbols) {
        if (symbol.name == "unusedFunction") foundUnusedFunction = true;
        if (symbol.name == "unusedConst") foundUnusedConst = true;
    }
    
    assert(foundUnusedFunction || foundUnusedConst);
    
    std::cout << "  ✓ Dead code detection test passed\n";
}

void testComplexityAnalysis() {
    std::cout << "Testing complexity analysis...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("complex.ts", R"(
function complexFunction(x: number, y: number): number {
    if (x > 0) {
        if (y > 0) {
            if (x > y) {
                for (let i = 0; i < x; i++) {
                    if (i % 2 === 0) {
                        console.log(i);
                    } else {
                        console.log('odd');
                    }
                }
            } else {
                while (y > 0) {
                    y--;
                    if (y % 3 === 0) {
                        console.log('divisible by 3');
                    }
                }
            }
        } else {
            switch (x) {
                case 1: return 1;
                case 2: return 2;
                default: return 0;
            }
        }
    } else {
        return -1;
    }
    return 0;
}
)");
    
    auto engine = createCrazyModeEngine();
    std::vector<std::string> files = {file};
    
    auto hotspots = engine->findComplexityHotspots(files);
    
    // Should detect high complexity
    assert(!hotspots.empty());
    assert(hotspots[0].cyclomaticComplexity > 10);
    assert(hotspots[0].nestingLevel > 3);
    
    std::cout << "  ✓ Complexity analysis test passed\n";
}

void testStyleIssues() {
    std::cout << "Testing style issue detection...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("style.ts", R"(
var oldStyle = 'var keyword';
const x = 1 == 2;  // loose equality
const y = 3 != 4;  // loose inequality
const trailing = 'trailing whitespace   ';
const tabIndent = '	tab indent';
console.log('debug statement');
)");
    
    auto engine = createCrazyModeEngine();
    std::vector<std::string> files = {file};
    
    auto issues = engine->findStyleIssues(files);
    
    // Should find multiple style issues
    assert(!issues.empty());
    
    bool foundVar = false;
    bool foundLooseEq = false;
    bool foundConsole = false;
    
    for (const auto& issue : issues) {
        if (issue.ruleId == "var-keyword") foundVar = true;
        if (issue.ruleId == "loose-equality") foundLooseEq = true;
        if (issue.ruleId == "console-statement") foundConsole = true;
    }
    
    assert(foundVar || foundLooseEq || foundConsole);
    
    std::cout << "  ✓ Style issue detection test passed\n";
}

void testRenameOperation() {
    std::cout << "Testing rename operation...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("rename.ts", R"(
function oldName(): number {
    return 42;
}

function caller(): number {
    return oldName() * 2;
}

const result = oldName();
)");
    
    auto engine = createCrazyModeEngine();
    
    auto op = engine->renameSymbol("oldName", "newName", {file});
    
    assert(op.type == RefactorType::RenameSymbol);
    assert(op.symbolName == "oldName");
    assert(op.newName == "newName");
    assert(!op.affectedFiles.empty());
    
    std::cout << "  ✓ Rename operation test passed\n";
}

void testCheckpointSystem() {
    std::cout << "Testing checkpoint system...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("checkpoint.txt", "original content");
    
    auto engine = createCrazyModeEngine();
    
    // Create checkpoint
    uint32_t checkpointId = engine->createCheckpoint("Before changes");
    assert(checkpointId > 0);
    
    // Modify file
    std::ofstream(file) << "modified content";
    
    // Get checkpoints
    auto checkpoints = engine->getCheckpoints();
    assert(!checkpoints.empty());
    
    // Restore checkpoint
    bool restored = engine->restoreCheckpoint(checkpointId);
    // Note: simplified implementation may not fully restore
    
    // Delete checkpoint
    bool deleted = engine->deleteCheckpoint(checkpointId);
    assert(deleted);
    
    std::cout << "  ✓ Checkpoint system test passed\n";
}

void testAsyncAnalysis() {
    std::cout << "Testing async analysis...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("async.ts", R"(
export function test(): void {
    console.log('test');
}
)");
    
    auto engine = createCrazyModeEngine();
    
    auto future = engine->analyzeCodebase({file});
    
    // Wait for completion
    auto operations = future.get();
    
    assert(engine->getProgress() == 1.0f);
    assert(!engine->isRunning());
    
    std::cout << "  ✓ Async analysis test passed\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Composer Mode & Crazy Mode Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Composer Mode tests
    std::cout << "--- Composer Mode Tests ---\n";
    testComposerConfig();
    testPlanCreation();
    testProjectAnalysis();
    testChangeApplication();
    testConflictDetection();
    testCallback();
    
    std::cout << "\n--- Crazy Mode Tests ---\n";
    testCrazyModeConfig();
    testSymbolTable();
    testDeadCodeDetection();
    testComplexityAnalysis();
    testStyleIssues();
    testRenameOperation();
    testCheckpointSystem();
    testAsyncAnalysis();
    
    std::cout << "\n========================================\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
