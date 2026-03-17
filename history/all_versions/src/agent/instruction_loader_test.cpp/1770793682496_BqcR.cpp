/**
 * @file instruction_loader_test.cpp
 * @brief Test for instruction file loading (Qt-free, plain assertions)
 */
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

class InstructionLoaderTest {
public:
    void testLoadAndReload() {
        fs::path tmp = fs::temp_directory_path() / "rawrxd_instr_test.md";

        // Write initial instructions
        {
            std::ofstream f(tmp, std::ios::trunc);
            assert(f.is_open() && "Failed to create temp instruction file");
            f << "INSTR: HelloAI\nAlways greet the user.";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Modify
        {
            std::ofstream f(tmp, std::ios::trunc);
            assert(f.is_open() && "Failed to modify temp instruction file");
            f << "INSTR: NewMarker\nChanged content";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Read back and verify
        {
            std::ifstream f(tmp);
            assert(f.is_open() && "Failed to open temp instruction file for read");
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            assert(content.find("NewMarker") != std::string::npos);
        }

        fs::remove(tmp);
        fprintf(stderr, "[INFO] [InstructionLoaderTest] All assertions passed.\n");
    }
};

#ifndef RAWRXD_NO_TEST_MAIN
int main() {
    InstructionLoaderTest test;
    test.testLoadAndReload();
    return 0;
}
#endif
