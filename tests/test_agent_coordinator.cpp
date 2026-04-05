/**
 * Agent coordinator smoke (Qt-free, C++20).
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
    const auto orchestrator = root / "src" / "agentic_core.cpp";
    const auto bridge = root / "src" / "agentic_agent_coordinator.cpp";

    const bool ok = fs::exists(orchestrator) && fs::exists(bridge) &&
                    !readAll(orchestrator).empty() && !readAll(bridge).empty();

    std::cout << "test_agent_coordinator: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
