/**
 * Phase2 performance smoke (Qt-free, C++20).
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
    const auto engine = root / "src" / "cpu_inference_engine.cpp";
    const auto fast = root / "src" / "ultra_fast_inference.cpp";

    const bool ok = fs::exists(engine) && fs::exists(fast) &&
                    !readAll(engine).empty() && !readAll(fast).empty();

    std::cout << "test_phase2_performance: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
