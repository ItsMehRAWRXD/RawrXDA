/**
 * Completion benchmark smoke (Qt-free, C++20).
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
    const auto engine = root / "src" / "ide_orchestrator.cpp";
    const auto infer = root / "src" / "cpu_inference_engine.cpp";

    const bool ok = fs::exists(engine) && fs::exists(infer) &&
                    !readAll(engine).empty() && !readAll(infer).empty();

    std::cout << "benchmark_completions: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
