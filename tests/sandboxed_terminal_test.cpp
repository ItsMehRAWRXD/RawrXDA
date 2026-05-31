/**
 * Terminal pool smoke tests (Qt-free, C++20).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path findRepoRoot() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "src" / "terminal_pool.cpp")) return p;
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

    const std::string src = readAll(root / "src" / "terminal_pool.cpp");
    if (src.empty()) return 1;

    const bool ok =
        src.find("createTerminal") != std::string::npos &&
        src.find("writeInput") != std::string::npos &&
        src.find("readOutput") != std::string::npos &&
        src.find("destroyTerminal") != std::string::npos;

    std::cout << "sandboxed_terminal_test: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
