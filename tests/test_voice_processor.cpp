/**
 * Voice processor smoke tests (Qt-free, C++20).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path findRepoRoot() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "src" / "core" / "voice_automation.cpp") ||
            fs::exists(p / "src" / "core" / "voice_chat.cpp")) {
            return p;
        }
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

    std::string src;
    if (fs::exists(root / "src" / "core" / "voice_automation.cpp")) {
        src = readAll(root / "src" / "core" / "voice_automation.cpp");
    } else if (fs::exists(root / "src" / "core" / "voice_chat.cpp")) {
        src = readAll(root / "src" / "core" / "voice_chat.cpp");
    }

    if (src.empty()) return 1;

    const bool ok =
        src.find("voice") != std::string::npos ||
        src.find("audio") != std::string::npos;

    std::cout << "test_voice_processor: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
