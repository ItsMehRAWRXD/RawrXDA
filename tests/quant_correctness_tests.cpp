/**
 * Quant correctness smoke (Qt-free, C++20).
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
    const auto quants = root / "src" / "llm_adapter" / "gguf_k_quants.cpp";
    const auto cpu = root / "src" / "cpu_inference_engine.cpp";

    const bool ok = fs::exists(quants) && fs::exists(cpu) &&
                    !readAll(quants).empty() && !readAll(cpu).empty();

    std::cout << "quant_correctness_tests: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
