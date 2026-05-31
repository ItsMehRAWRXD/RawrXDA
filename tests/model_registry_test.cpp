/**
 * Model registry smoke tests (Qt-free, C++20).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path findRepoRoot() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "src" / "model_registry.cpp")) return p;
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

    const std::string hdr = readAll(root / "include" / "model_registry.h");
    const std::string impl = readAll(root / "src" / "model_registry.cpp");
    if (hdr.empty() || impl.empty()) return 1;

    const bool ok =
        hdr.find("class ModelRegistry") != std::string::npos &&
        hdr.find("registerModel") != std::string::npos &&
        impl.find("ModelRegistry::registerModel") != std::string::npos &&
        impl.find("ModelRegistry::setActiveModel") != std::string::npos;

    std::cout << "model_registry_test: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
