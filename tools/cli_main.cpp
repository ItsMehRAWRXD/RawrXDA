// tools/cli_main.cpp - Minimal CLI launcher (legacy/stub)
// For full chat + agentic CLI (101% Win32 parity), build from repo root:
//   cmake -B build_ide -G Ninja && cmake --build build_ide --target RawrXD_CLI
// See Ship/CLI_PARITY.md for details.

#include <windows.h>
#include <iostream>
#include <string>

static void PrintHelp() {
    std::cout << "RawrXD CLI Stub v1.0 (minimal)\n\n";
    std::cout << "For full agentic CLI (chat, /agent, /smoke, /tools, HTTP API):\n";
    std::cout << "  cmake --build build_ide --target RawrXD_CLI\n";
    std::cout << "  build_ide\\bin\\RawrXD_CLI.exe\n\n";
    std::cout << "Options: --help -h --version -v  (this stub only)\n";
    std::cout << "See Ship/CLI_PARITY.md\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "--version" || arg == "-v") {
            PrintHelp();
            return 0;
        }
    }
    PrintHelp();
    return 0;
}
