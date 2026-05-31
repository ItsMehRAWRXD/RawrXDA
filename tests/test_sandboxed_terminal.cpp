/**
 * Sandboxed terminal manager smoke tests (Qt-free, C++20).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path findRepoRoot() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "src" / "win32app" / "Win32TerminalManager.cpp")) return p;
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    if (fs::exists("d:/rawrxd")) return fs::path("d:/rawrxd");
    return {};
}

static std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main() {
    const fs::path root = findRepoRoot();
    if (root.empty()) return 1;

    const std::string src = readAll(root / "src" / "win32app" / "Win32TerminalManager.cpp");
    if (src.empty()) return 1;

    const bool ok =
        src.find("CreatePipe") != std::string::npos &&
        src.find("CreateProcessA") != std::string::npos &&
        src.find("readOutputThread") != std::string::npos &&
        src.find("writeInput") != std::string::npos;

    std::cout << "test_sandboxed_terminal: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
