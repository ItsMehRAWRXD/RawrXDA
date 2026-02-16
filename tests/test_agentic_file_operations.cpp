/**
 * @file test_agentic_file_operations.cpp
 * @brief Behavioral tests for Keep/Undo file operations — C++20, no Qt.
 *
 * Tests AgenticFileOperations with callbacks, std::filesystem temp dir,
 * and std::fstream. Replaces QTest, QTemporaryDir, QFile, QSignalSpy.
 */

#include "agentic_file_operations.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

#define TEST_VERIFY(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } ++g_run; } while(0)
#define TEST_COMPARE(a, b) do { if ((a) != (b)) { std::fprintf(stderr, "FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); ++g_failed; } ++g_run; } while(0)

static int g_run = 0;
static int g_failed = 0;

static std::string readFileContents(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static void testCreateFileWithKeep() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_create_keep";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_create_keep.txt").string();
    std::string testContent = "This is test content for creation with Keep";

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    std::string createdPath;
    fileOps.setOnFileCreated([&createdPath](const std::string& p) { createdPath = p; });

    fileOps.createFileWithApproval(testFilePath, testContent);

    TEST_VERIFY(fs::exists(testFilePath));
    TEST_COMPARE(readFileContents(testFilePath), testContent);
    TEST_COMPARE(createdPath, testFilePath);

    fs::remove_all(tmp);
}

static void testCreateFileWithUndo() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_create_undo";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_create_undo.txt").string();
    std::string testContent = "This is test content for creation with Undo";

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    std::string undonePath;
    fileOps.setOnOperationUndone([&undonePath](const std::string& p) { undonePath = p; });

    fileOps.createFileWithApproval(testFilePath, testContent);
    TEST_VERIFY(fs::exists(testFilePath));
    fileOps.undoLastAction();
    TEST_VERIFY(!fs::exists(testFilePath));
    TEST_COMPARE(undonePath, testFilePath);

    fs::remove_all(tmp);
}

static void testModifyFileWithKeep() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_modify_keep";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_modify_keep.txt").string();
    std::string originalContent = "Original content";
    std::string modifiedContent = "Modified content";

    std::ofstream f(testFilePath);
    f << originalContent;
    f.close();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    std::string modifiedPath;
    fileOps.setOnFileModified([&modifiedPath](const std::string& p) { modifiedPath = p; });

    fileOps.modifyFileWithApproval(testFilePath, modifiedContent);

    TEST_COMPARE(readFileContents(testFilePath), modifiedContent);
    TEST_COMPARE(modifiedPath, testFilePath);

    fs::remove_all(tmp);
}

static void testModifyFileWithUndo() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_modify_undo";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_modify_undo.txt").string();
    std::string originalContent = "Original content";
    std::string modifiedContent = "Modified content";

    std::ofstream f(testFilePath);
    f << originalContent;
    f.close();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    fileOps.modifyFileWithApproval(testFilePath, modifiedContent);
    TEST_COMPARE(readFileContents(testFilePath), modifiedContent);
    fileOps.undoLastAction();
    TEST_COMPARE(readFileContents(testFilePath), originalContent);

    fs::remove_all(tmp);
}

static void testDeleteFileWithKeep() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_delete_keep";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_delete_keep.txt").string();
    std::ofstream f(testFilePath);
    f << "Content to be deleted";
    f.close();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    std::string deletedPath;
    fileOps.setOnFileDeleted([&deletedPath](const std::string& p) { deletedPath = p; });

    fileOps.deleteFileWithApproval(testFilePath);

    TEST_VERIFY(!fs::exists(testFilePath));
    TEST_COMPARE(deletedPath, testFilePath);

    fs::remove_all(tmp);
}

static void testDeleteFileWithUndo() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_delete_undo";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_delete_undo.txt").string();
    std::string testContent = "Content to be deleted and restored";
    std::ofstream f(testFilePath);
    f << testContent;
    f.close();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });
    fileOps.deleteFileWithApproval(testFilePath);
    TEST_VERIFY(!fs::exists(testFilePath));
    fileOps.undoLastAction();
    TEST_VERIFY(fs::exists(testFilePath));
    TEST_COMPARE(readFileContents(testFilePath), testContent);

    fs::remove_all(tmp);
}

static void testUndoStackManagement() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_undo_stack";
    fs::create_directories(tmp);

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    TEST_VERIFY(fileOps.getHistorySize() == 0);
    TEST_VERIFY(!fileOps.canUndo());

    for (int i = 0; i < 5; ++i) {
        std::string path = (tmp / ("test_file_" + std::to_string(i) + ".txt")).string();
        fileOps.createFileWithApproval(path, "Content for file " + std::to_string(i));
    }
    TEST_VERIFY(fileOps.getHistorySize() >= 1);
    TEST_VERIFY(fileOps.canUndo());

    while (fileOps.canUndo())
        fileOps.undoLastAction();
    TEST_VERIFY(fileOps.getHistorySize() == 0);
    TEST_VERIFY(!fileOps.canUndo());

    fs::remove_all(tmp);
}

static void testApprovalRejected() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_agentic_test_reject";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "test_reject.txt").string();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return false; });
    bool cancelled = false;
    fileOps.setOnOperationCancelled([&cancelled](const std::string&) { cancelled = true; });

    fileOps.createFileWithApproval(testFilePath, "content");
    TEST_VERIFY(!fs::exists(testFilePath));
    TEST_VERIFY(cancelled);

    fs::remove_all(tmp);
}

#define RUN(test) do { test(); } while(0)

int main() {
    std::fprintf(stdout, "AgenticFileOperations tests (C++20, no Qt)\n");
    RUN(testCreateFileWithKeep);
    RUN(testCreateFileWithUndo);
    RUN(testModifyFileWithKeep);
    RUN(testModifyFileWithUndo);
    RUN(testDeleteFileWithKeep);
    RUN(testDeleteFileWithUndo);
    RUN(testUndoStackManagement);
    RUN(testApprovalRejected);
    std::fprintf(stdout, "Done: %d run, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
