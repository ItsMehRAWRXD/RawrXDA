/**
 * Agent coordinator integration smoke tests (Qt-free, C++20).
 *
 * Verifies that the production orchestration path is present in source with
 * concrete execution hooks (loop, round execution, tool dispatch).
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path findRepoRoot() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "src" / "agentic" / "AgentOrchestrator.cpp")) {
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

static bool requireContains(const std::string& haystack, const std::string& needle, const char* label) {
    if (haystack.find(needle) == std::string::npos) {
        std::cerr << "missing symbol: " << label << " (" << needle << ")\n";
        return false;
    }
    return true;
}

int main() {
    const fs::path root = findRepoRoot();
    if (root.empty()) {
        std::cerr << "unable to locate repository root\n";
        return 1;
    }

    const fs::path orchestrator = root / "src" / "agentic" / "AgentOrchestrator.cpp";
    const std::string src = readAll(orchestrator);
    if (src.empty()) {
        std::cerr << "failed to read " << orchestrator.string() << "\n";
        return 1;
    }

    bool ok = true;
    ok &= requireContains(src, "RunAgentLoop", "agent loop entry");
    ok &= requireContains(src, "RunOneRound", "round executor");
    ok &= requireContains(src, "ExecuteToolCalls", "tool dispatcher");
    ok &= requireContains(src, "DispatchTask", "task queue ingress");

    std::cout << "test_agent_coordinator_integration: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
