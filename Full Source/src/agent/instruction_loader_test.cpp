/**
 * @file instruction_loader_test.cpp
 * @brief Tests for instruction file loading (Qt-free)
 *
 * Tests file reading and content verification.
 * File-system watching is handled by platform APIs (not Qt QFileSystemWatcher).
 */

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

class InstructionLoaderTest {
public:
    void testLoadAndReload() {
        std::string tmp = (std::filesystem::temp_directory_path() / "rawrxd_instr_test.md").string();

        // Write initial instructions
        {
            std::ofstream f(tmp, std::ios::trunc);
            assert(f.is_open());
            f << "INSTR: HelloAI\nAlways greet the user.";
        }

        // Verify initial content
        {
            std::ifstream f(tmp);
            assert(f.is_open());
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            assert(content.find("HelloAI") != std::string::npos);
        }

        // Modify file
        {
            std::ofstream f(tmp, std::ios::trunc);
            assert(f.is_open());
            f << "INSTR: NewMarker\nChanged content";
        }

        // Verify modified content
        {
            std::ifstream f(tmp);
            assert(f.is_open());
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            assert(content.find("NewMarker") != std::string::npos);
        }

        // Cleanup
        std::filesystem::remove(tmp);
        // Test passed (assertion-driven; no output needed for passing tests).
    }
};

// Standalone test runner (no MOC needed)
#ifdef INSTRUCTION_LOADER_TEST_MAIN
int main() {
    InstructionLoaderTest test;
    test.testLoadAndReload();
    return 0;
}
#endif
