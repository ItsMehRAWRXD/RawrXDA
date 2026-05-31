// ============================================================================
// interactive_refactoring_test.cpp — Interactive Refactoring Test Suite
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "refactoring/interactive_refactoring.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using namespace RawrXD::Refactoring;

// ============================================================================
// Test Utilities
// ============================================================================

class TestFixture {
public:
    std::string testDir;
    
    TestFixture() {
        testDir = fs::temp_directory_path().string() + "/refactoring_test_" + 
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
// Session Management Tests
// ============================================================================

void testSessionCreation() {
    std::cout << "Testing session creation...\n";
    
    auto engine = createInteractiveRefactoringEngine();
    assert(engine != nullptr);
    
    std::string sessionId = engine->createSession("Test refactoring session");
    assert(!sessionId.empty());
    
    auto session = engine->getSession(sessionId);
    assert(session.has_value());
    assert(session->description == "Test refactoring session");
    assert(!session->completed);
    
    std::cout << "  ✓ Session creation test passed\n";
}

void testSessionEnd() {
    std::cout << "Testing session end...\n";
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Test session");
    
    bool ended = engine->endSession(sessionId);
    assert(ended);
    
    auto session = engine->getSession(sessionId);
    assert(session.has_value());
    assert(session->completed);
    
    std::cout << "  ✓ Session end test passed\n";
}

void testActiveSessions() {
    std::cout << "Testing active sessions...\n";
    
    auto engine = createInteractiveRefactoringEngine();
    
    std::string session1 = engine->createSession("Session 1");
    std::string session2 = engine->createSession("Session 2");
    
    auto active = engine->getActiveSessions();
    assert(active.size() == 2);
    
    engine->endSession(session1);
    
    active = engine->getActiveSessions();
    assert(active.size() == 1);
    
    std::cout << "  ✓ Active sessions test passed\n";
}

// ============================================================================
// Conversation Tests
// ============================================================================

void testConversationMessages() {
    std::cout << "Testing conversation messages...\n";
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Conversation test");
    
    ConversationMessage userMsg;
    userMsg.role = ConversationMessage::Role::User;
    userMsg.content = "Please rename function foo to bar";
    
    engine->addMessage(sessionId, userMsg);
    
    ConversationMessage assistantMsg;
    assistantMsg.role = ConversationMessage::Role::Assistant;
    assistantMsg.content = "I'll help you rename the function";
    
    engine->addMessage(sessionId, assistantMsg);
    
    auto conversation = engine->getConversation(sessionId);
    assert(conversation.size() == 2);
    assert(conversation[0].role == ConversationMessage::Role::User);
    assert(conversation[1].role == ConversationMessage::Role::Assistant);
    
    std::cout << "  ✓ Conversation messages test passed\n";
}

void testConversationContext() {
    std::cout << "Testing conversation context...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
export function hello(name: string): string {
    return 'Hello, ' + name;
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Context test");
    
    engine->addFile(sessionId, file);
    
    auto context = engine->getContext(sessionId);
    assert(!context.activeFiles.empty());
    assert(context.fileContents.count(file) > 0);
    
    std::cout << "  ✓ Conversation context test passed\n";
}

// ============================================================================
// File Operations Tests
// ============================================================================

void testFileAddition() {
    std::cout << "Testing file addition...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "export function test() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("File test");
    
    bool added = engine->addFile(sessionId, file);
    assert(added);
    
    auto activeFiles = engine->getActiveFiles(sessionId);
    assert(!activeFiles.empty());
    assert(activeFiles[0] == file);
    
    std::cout << "  ✓ File addition test passed\n";
}

void testFileRemoval() {
    std::cout << "Testing file removal...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "export function test() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("File removal test");
    
    engine->addFile(sessionId, file);
    
    bool removed = engine->removeFile(sessionId, file);
    assert(removed);
    
    auto activeFiles = engine->getActiveFiles(sessionId);
    assert(activeFiles.empty());
    
    std::cout << "  ✓ File removal test passed\n";
}

// ============================================================================
// Edit Operations Tests
// ============================================================================

void testEditProposal() {
    std::cout << "Testing edit proposal...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function oldName() {
    return 'test';
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Edit proposal test");
    
    engine->addFile(sessionId, file);
    
    TextEdit edit;
    edit.range.start = {2, 0};
    edit.range.end = {4, 1};
    edit.oldText = "function oldName() {\n    return 'test';\n}";
    edit.newText = "function newName() {\n    return 'test';\n}";
    edit.description = "Rename function";
    edit.source = file;
    
    bool proposed = engine->proposeEdit(sessionId, edit);
    assert(proposed);
    
    auto pending = engine->getPendingEdits(sessionId);
    assert(!pending.empty());
    
    std::cout << "  ✓ Edit proposal test passed\n";
}

void testEditApplication() {
    std::cout << "Testing edit application...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function oldName() {
    return 'test';
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Edit application test");
    
    engine->addFile(sessionId, file);
    
    TextEdit edit;
    edit.range.start = {2, 0};
    edit.range.end = {4, 1};
    edit.oldText = "function oldName() {\n    return 'test';\n}";
    edit.newText = "function newName() {\n    return 'test';\n}";
    edit.description = "Rename function";
    edit.source = file;
    
    engine->proposeEdit(sessionId, edit);
    
    auto pending = engine->getPendingEdits(sessionId);
    assert(!pending.empty());
    
    bool applied = engine->applyEdit(sessionId, pending[0].id);
    assert(applied);
    
    pending = engine->getPendingEdits(sessionId);
    assert(pending.empty());
    
    std::cout << "  ✓ Edit application test passed\n";
}

void testEditRejection() {
    std::cout << "Testing edit rejection...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "function test() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Edit rejection test");
    
    engine->addFile(sessionId, file);
    
    TextEdit edit;
    edit.range.start = {1, 0};
    edit.range.end = {1, 20};
    edit.oldText = "function test() {}";
    edit.newText = "function newTest() {}";
    edit.source = file;
    
    engine->proposeEdit(sessionId, edit);
    
    auto pending = engine->getPendingEdits(sessionId);
    assert(!pending.empty());
    
    bool rejected = engine->rejectEdit(sessionId, pending[0].id);
    assert(rejected);
    
    pending = engine->getPendingEdits(sessionId);
    assert(pending.empty());
    
    std::cout << "  ✓ Edit rejection test passed\n";
}

void testBatchEditOperations() {
    std::cout << "Testing batch edit operations...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function a() {}
function b() {}
function c() {}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Batch edit test");
    
    engine->addFile(sessionId, file);
    
    // Propose multiple edits
    for (int i = 0; i < 3; i++) {
        TextEdit edit;
        edit.range.start = {static_cast<uint32_t>(i + 2), 0};
        edit.range.end = {static_cast<uint32_t>(i + 2), 15};
        edit.oldText = "function " + std::string(1, 'a' + i) + "() {}";
        edit.newText = "function new" + std::string(1, 'A' + i) + "() {}";
        edit.source = file;
        engine->proposeEdit(sessionId, edit);
    }
    
    auto pending = engine->getPendingEdits(sessionId);
    assert(pending.size() == 3);
    
    // Apply all
    bool applied = engine->applyAllEdits(sessionId);
    assert(applied);
    
    pending = engine->getPendingEdits(sessionId);
    assert(pending.empty());
    
    std::cout << "  ✓ Batch edit operations test passed\n";
}

// ============================================================================
// Diff Generation Tests
// ============================================================================

void testDiffGeneration() {
    std::cout << "Testing diff generation...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function oldName() {
    return 'test';
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Diff test");
    
    engine->addFile(sessionId, file);
    
    TextEdit edit;
    edit.range.start = {2, 0};
    edit.range.end = {4, 1};
    edit.oldText = "function oldName() {\n    return 'test';\n}";
    edit.newText = "function newName() {\n    return 'test';\n}";
    edit.source = file;
    
    engine->proposeEdit(sessionId, edit);
    engine->applyAllEdits(sessionId);
    
    auto diff = engine->generateDiff(sessionId, file);
    assert(!diff.hunks.empty());
    
    std::cout << "  ✓ Diff generation test passed\n";
}

void testDiffPreview() {
    std::cout << "Testing diff preview...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function oldName() {
    return 'test';
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Preview test");
    
    engine->addFile(sessionId, file);
    
    TextEdit edit;
    edit.range.start = {2, 0};
    edit.range.end = {4, 1};
    edit.oldText = "function oldName() {\n    return 'test';\n}";
    edit.newText = "function newName() {\n    return 'test';\n}";
    edit.source = file;
    
    engine->proposeEdit(sessionId, edit);
    engine->applyAllEdits(sessionId);
    
    PreviewOptions options;
    options.showLineNumbers = true;
    options.colorize = true;
    
    auto preview = engine->previewChanges(sessionId, options);
    assert(!preview.formatted.empty());
    assert(!preview.summary.empty());
    assert(preview.statistics.count("filesChanged") > 0);
    
    std::cout << "  ✓ Diff preview test passed\n";
}

// ============================================================================
// Checkpoint and Rollback Tests
// ============================================================================

void testCheckpointCreation() {
    std::cout << "Testing checkpoint creation...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "function test() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Checkpoint test");
    
    engine->addFile(sessionId, file);
    
    std::string checkpointId = engine->createCheckpoint(sessionId, "Initial state");
    assert(!checkpointId.empty());
    
    auto checkpoints = engine->getCheckpoints(sessionId);
    assert(!checkpoints.empty());
    assert(checkpoints[0].description == "Initial state");
    
    std::cout << "  ✓ Checkpoint creation test passed\n";
}

void testRollback() {
    std::cout << "Testing rollback...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "function oldName() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Rollback test");
    
    engine->addFile(sessionId, file);
    
    // Create checkpoint
    std::string checkpointId = engine->createCheckpoint(sessionId, "Before rename");
    
    // Make edit
    TextEdit edit;
    edit.range.start = {1, 0};
    edit.range.end = {1, 20};
    edit.oldText = "function oldName() {}";
    edit.newText = "function newName() {}";
    edit.source = file;
    
    engine->proposeEdit(sessionId, edit);
    engine->applyAllEdits(sessionId);
    
    // Verify change
    auto session = engine->getSession(sessionId);
    assert(session->changes[0].modifiedContent != session->changes[0].originalContent);
    
    // Rollback
    auto result = engine->rollback(sessionId, checkpointId);
    assert(result.success);
    
    std::cout << "  ✓ Rollback test passed\n";
}

// ============================================================================
// Conflict Detection Tests
// ============================================================================

void testConflictDetection() {
    std::cout << "Testing conflict detection...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function test() {
    return 'hello';
}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Conflict test");
    
    engine->addFile(sessionId, file);
    
    // Create overlapping edits
    TextEdit edit1;
    edit1.range.start = {2, 0};
    edit1.range.end = {3, 1};
    edit1.oldText = "function test() {\n    return 'hello';\n}";
    edit1.newText = "function test() {\n    return 'world';\n}";
    edit1.source = file;
    
    TextEdit edit2;
    edit2.range.start = {2, 0};
    edit2.range.end = {3, 1};
    edit2.oldText = "function test() {\n    return 'hello';\n}";
    edit2.newText = "function test() {\n    return 'goodbye';\n}";
    edit2.source = file;
    
    engine->proposeEdit(sessionId, edit1);
    engine->proposeEdit(sessionId, edit2);
    
    auto conflicts = engine->detectConflicts(sessionId);
    assert(!conflicts.empty());
    
    std::cout << "  ✓ Conflict detection test passed\n";
}

// ============================================================================
// Refactoring Operations Tests
// ============================================================================

void testAvailableRefactorings() {
    std::cout << "Testing available refactorings...\n";
    
    auto engine = createInteractiveRefactoringEngine();
    
    auto refactorings = engine->getAvailableRefactorings();
    assert(!refactorings.empty());
    
    // Check for common refactorings
    bool hasRename = false;
    bool hasExtractFunction = false;
    
    for (const auto& ref : refactorings) {
        if (ref.kind == RefactoringKind::Rename) hasRename = true;
        if (ref.kind == RefactoringKind::ExtractFunction) hasExtractFunction = true;
    }
    
    assert(hasRename);
    assert(hasExtractFunction);
    
    std::cout << "  ✓ Available refactorings test passed\n";
}

void testExecuteRefactoring() {
    std::cout << "Testing refactoring execution...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", "function oldName() {}");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Execute refactoring test");
    
    engine->addFile(sessionId, file);
    
    std::unordered_map<std::string, std::string> params;
    params["file"] = file;
    params["oldName"] = "oldName";
    params["newName"] = "newName";
    
    bool executed = engine->executeRefactoring(sessionId, RefactoringKind::Rename, params);
    assert(executed);
    
    std::cout << "  ✓ Refactoring execution test passed\n";
}

// ============================================================================
// Statistics Tests
// ============================================================================

void testStatistics() {
    std::cout << "Testing statistics...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.ts", R"(
function a() {}
function b() {}
)");
    
    auto engine = createInteractiveRefactoringEngine();
    std::string sessionId = engine->createSession("Statistics test");
    
    engine->addFile(sessionId, file);
    
    // Propose and apply edits
    TextEdit edit1;
    edit1.range.start = {2, 0};
    edit1.range.end = {2, 15};
    edit1.oldText = "function a() {}";
    edit1.newText = "function newA() {}";
    edit1.source = file;
    
    TextEdit edit2;
    edit2.range.start = {3, 0};
    edit2.range.end = {3, 15};
    edit2.oldText = "function b() {}";
    edit2.newText = "function newB() {}";
    edit2.source = file;
    
    engine->proposeEdit(sessionId, edit1);
    engine->proposeEdit(sessionId, edit2);
    
    auto stats = engine->getStatistics(sessionId);
    assert(stats["totalEdits"] == 2);
    assert(stats["pendingEdits"] == 2);
    assert(stats["appliedEdits"] == 0);
    
    engine->applyEdit(sessionId, edit1.id);
    
    stats = engine->getStatistics(sessionId);
    assert(stats["appliedEdits"] == 1);
    assert(stats["pendingEdits"] == 1);
    
    std::cout << "  ✓ Statistics test passed\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Interactive Refactoring Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Session management tests
    std::cout << "--- Session Management Tests ---\n";
    testSessionCreation();
    testSessionEnd();
    testActiveSessions();
    
    // Conversation tests
    std::cout << "\n--- Conversation Tests ---\n";
    testConversationMessages();
    testConversationContext();
    
    // File operations tests
    std::cout << "\n--- File Operations Tests ---\n";
    testFileAddition();
    testFileRemoval();
    
    // Edit operations tests
    std::cout << "\n--- Edit Operations Tests ---\n";
    testEditProposal();
    testEditApplication();
    testEditRejection();
    testBatchEditOperations();
    
    // Diff generation tests
    std::cout << "\n--- Diff Generation Tests ---\n";
    testDiffGeneration();
    testDiffPreview();
    
    // Checkpoint and rollback tests
    std::cout << "\n--- Checkpoint and Rollback Tests ---\n";
    testCheckpointCreation();
    testRollback();
    
    // Conflict detection tests
    std::cout << "\n--- Conflict Detection Tests ---\n";
    testConflictDetection();
    
    // Refactoring operations tests
    std::cout << "\n--- Refactoring Operations Tests ---\n";
    testAvailableRefactorings();
    testExecuteRefactoring();
    
    // Statistics tests
    std::cout << "\n--- Statistics Tests ---\n";
    testStatistics();
    
    std::cout << "\n========================================\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
