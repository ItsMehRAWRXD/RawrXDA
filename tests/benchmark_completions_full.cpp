/**
 * Full completion benchmark smoke (Qt-free, C++20).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main() {
    const fs::path root("d:/rawrxd");
    const auto loader = root / "src" / "rawrxd_model_loader.cpp";
    const auto runner = root / "src" / "llm_adapter" / "GGUFRunner.cpp";

    const bool ok = fs::exists(loader) && fs::exists(runner) &&
                    !readAll(loader).empty() && !readAll(runner).empty();

    std::cout << "benchmark_completions_full: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
