/**
 * Voice processor smoke (Qt-free, C++20).
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
    const auto vp = root / "src" / "orchestration" / "voice_processor.cpp";
    const auto vph = root / "src" / "orchestration" / "voice_processor.hpp";

    const bool ok = fs::exists(vp) && fs::exists(vph) &&
                    !readAll(vp).empty() && !readAll(vph).empty();

    std::cout << "voice_processor_test: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
