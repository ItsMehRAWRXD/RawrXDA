/**
 * @file fuzz_test_agentic_file_operations.cpp
 * @brief Fuzz tests for Keep/Undo file operations — C++20, no Qt.
 *
 * Tests robustness with large files, special characters, unicode,
 * sequential/concurrent-style ops, path edge cases, and binary content.
 */

#include "agentic_file_operations.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#define TEST_VERIFY(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failed; } ++g_run; } while(0)

static int g_run = 0;
static int g_failed = 0;
static std::mt19937 g_rng{42};

static std::string generateRandomString(int length) {
    const char possible[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(possible) - 2));
    std::string s;
    s.reserve(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i)
        s += possible[dist(g_rng)];
    return s;
}

static std::string generateRandomUnicodeString(int length) {
    std::string s;
    s.reserve(static_cast<size_t>(length * 3));
    std::uniform_int_distribution<int> dist(0x00, 0xFF);
    for (int i = 0; i < length; ++i) {
        int cp = dist(g_rng) | (dist(g_rng) << 8);
        if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0x20;
        if (cp < 0x80) {
            s += static_cast<char>(cp);
        } else if (cp < 0x800) {
            s += static_cast<char>(0xC0 | (cp >> 6));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            s += static_cast<char>(0xE0 | (cp >> 12));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return s;
}

static std::vector<uint8_t> generateRandomBinaryData(size_t length) {
    std::vector<uint8_t> data(length);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < length; ++i)
        data[i] = static_cast<uint8_t>(dist(g_rng));
    return data;
}

static std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static void testLargeFileOperations() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_large";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "large_file_test.txt").string();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    std::string largeContent = generateRandomString(1024 * 1024);
    fileOps.createFileWithApproval(testFilePath, largeContent);

    TEST_VERIFY(fs::exists(testFilePath));
    TEST_VERIFY(fs::file_size(testFilePath) > 1000000);

    std::string modifiedContent = generateRandomString(1024 * 1024);
    fileOps.modifyFileWithApproval(testFilePath, modifiedContent);
    TEST_VERIFY(readAll(testFilePath).size() == modifiedContent.size());

    fileOps.deleteFileWithApproval(testFilePath);
    TEST_VERIFY(!fs::exists(testFilePath));

    fs::remove_all(tmp);
}

static void testSpecialCharacterFilenames() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_special";
    fs::create_directories(tmp);

    std::vector<std::string> names = {
        "test file with spaces.txt",
        "test-file-with-dashes.txt",
        "test_file_with_underscores.txt",
        "test.file.with.dots.txt",
    };

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    for (const auto& name : names) {
        std::string path = (tmp / name).string();
        std::string content = "Content for " + name;
        fileOps.createFileWithApproval(path, content);
        TEST_VERIFY(fs::exists(path));
        fileOps.modifyFileWithApproval(path, "Modified " + content);
        TEST_VERIFY(readAll(path).find("Modified") != std::string::npos);
        fileOps.deleteFileWithApproval(path);
        TEST_VERIFY(!fs::exists(path));
    }

    fs::remove_all(tmp);
}

static void testUnicodeContent() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_unicode";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "unicode_test.txt").string();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    std::string unicodeContent = generateRandomUnicodeString(10000);
    fileOps.createFileWithApproval(testFilePath, unicodeContent);
    TEST_VERIFY(fs::exists(testFilePath));

    std::string modifiedUnicode = generateRandomUnicodeString(10000);
    fileOps.modifyFileWithApproval(testFilePath, modifiedUnicode);
    TEST_VERIFY(fs::file_size(testFilePath) > 0);

    fileOps.deleteFileWithApproval(testFilePath);
    TEST_VERIFY(!fs::exists(testFilePath));

    fs::remove_all(tmp);
}

static void testConcurrentStyleOperations() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_concurrent";
    fs::create_directories(tmp);

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    for (int i = 0; i < 50; ++i) {
        std::string path = (tmp / ("concurrent_test_" + std::to_string(i) + ".txt")).string();
        std::string content = "Content " + std::to_string(i);

        switch (i % 3) {
            case 0:
                fileOps.createFileWithApproval(path, content);
                TEST_VERIFY(fs::exists(path));
                break;
            case 1: {
                fileOps.createFileWithApproval(path, content);
                TEST_VERIFY(fs::exists(path));
                fileOps.modifyFileWithApproval(path, "Modified content " + std::to_string(i));
                break;
            }
            case 2:
                fileOps.createFileWithApproval(path, content);
                TEST_VERIFY(fs::exists(path));
                fileOps.deleteFileWithApproval(path);
                TEST_VERIFY(!fs::exists(path));
                break;
        }
    }

    fs::remove_all(tmp);
}

static void testPathTraversalAttempts() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_traversal";
    fs::create_directories(tmp);

    std::string base = tmp.string();
    std::vector<std::string> traversalPaths = {
        base + "/../traversal_test1.txt",
        base + "/./traversal_test3.txt",
    };

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    for (const auto& path : traversalPaths) {
        fileOps.createFileWithApproval(path, "Traversal test content");
        if (fs::exists(path)) {
            fileOps.modifyFileWithApproval(path, "Modified traversal content");
            fileOps.deleteFileWithApproval(path);
        }
    }

    fs::remove_all(tmp);
}

static void testVeryLongPaths() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_long";
    fs::create_directories(tmp);
    fs::path longPath = tmp;
    for (int i = 0; i < 20; ++i)
        longPath = longPath / ("very_long_directory_name_" + std::to_string(i));
    fs::create_directories(longPath);
    std::string filePath = (longPath / "very_long_filename.txt").string();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    fileOps.createFileWithApproval(filePath, "Content for very long path");
    if (fs::exists(filePath)) {
        fileOps.modifyFileWithApproval(filePath, "Modified content for very long path");
        fileOps.deleteFileWithApproval(filePath);
    }

    fs::remove_all(tmp);
}

static void testEmptyAndWhitespaceContent() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_whitespace";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "whitespace_test.txt").string();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    fileOps.createFileWithApproval(testFilePath, "");
    TEST_VERIFY(fs::exists(testFilePath));
    TEST_VERIFY(fs::file_size(testFilePath) == 0);

    std::string whitespace = "   \t\n  \r\n  \t  ";
    fileOps.modifyFileWithApproval(testFilePath, whitespace);
    TEST_VERIFY(readAll(testFilePath) == whitespace);

    fileOps.deleteFileWithApproval(testFilePath);
    TEST_VERIFY(!fs::exists(testFilePath));

    fs::remove_all(tmp);
}

static void testBinaryContent() {
    fs::path tmp = fs::temp_directory_path() / "rawrxd_fuzz_binary";
    fs::create_directories(tmp);
    std::string testFilePath = (tmp / "binary_test.bin").string();

    auto binaryContent = generateRandomBinaryData(10000);
    std::string binStr(binaryContent.begin(), binaryContent.end());
    std::ofstream f(testFilePath, std::ios::binary);
    f.write(binStr.data(), static_cast<std::streamsize>(binStr.size()));
    f.close();

    AgenticFileOperations fileOps;
    fileOps.setApprovalCallback([](const std::string&, AgenticFileActionType, const std::string*) { return true; });

    auto modifiedBinary = generateRandomBinaryData(10000);
    std::string modStr(modifiedBinary.begin(), modifiedBinary.end());
    fileOps.modifyFileWithApproval(testFilePath, modStr);
    std::string readBack = readAll(testFilePath);
    TEST_VERIFY(readBack.size() == modStr.size());

    fileOps.deleteFileWithApproval(testFilePath);
    TEST_VERIFY(!fs::exists(testFilePath));

    fs::remove_all(tmp);
}

#define RUN(test) do { test(); } while(0)

int main() {
    std::fprintf(stdout, "Fuzz tests for AgenticFileOperations (C++20, no Qt)\n");
    RUN(testLargeFileOperations);
    RUN(testSpecialCharacterFilenames);
    RUN(testUnicodeContent);
    RUN(testConcurrentStyleOperations);
    RUN(testPathTraversalAttempts);
    RUN(testVeryLongPaths);
    RUN(testEmptyAndWhitespaceContent);
    RUN(testBinaryContent);
    std::fprintf(stdout, "Done: %d run, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
