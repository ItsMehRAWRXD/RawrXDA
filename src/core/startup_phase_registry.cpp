// ============================================================================
// startup_phase_registry.cpp — Dynamic phase order and lazy phase execution
// ============================================================================
// Loads phase order from config/startup_phases.txt; no hardcoded sequence.
// Lazy phases are skipped at boot and run on first runPhaseLazy(name).
// ============================================================================

#include "../../include/startup_phase_registry.h"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace RawrXD {
namespace Startup {

static std::vector<std::string> s_defaultOrder;
static std::unordered_map<std::string, PhaseFn> s_lazyPhases;
static std::unordered_set<std::string> s_lazyPhaseNames;
static std::unordered_set<std::string> s_lazyPhaseRun;

static void initDefaultOrder() {
    if (!s_defaultOrder.empty()) return;
    s_defaultOrder = {
        "init_common_controls",
        "first_run_gauntlet",
        "vsix_loader",
        "plugin_signature",
        "creating_ide_instance",
        "createWindow",
        "enterprise_license",
        "showWindow",
        "camellia_init",
        "masm_init",
        "swarm",
        "auto_update",
        "layout",
        "message_loop_entered",
    };
    s_lazyPhaseNames = {};
}

std::vector<std::string> getPhaseOrder() {
    initDefaultOrder();
    std::string path = "config/startup_phases.txt";
    std::ifstream f(path);
    if (!f.is_open()) {
        path = "startup_phases.txt";
        f.open(path);
    }
    if (!f.is_open())
        return s_defaultOrder;

    std::vector<std::string> order;
    std::string line;
    while (std::getline(f, line)) {
        // trim
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end == std::string::npos ? end : end - start + 1);
        if (line.empty() || line[0] == '#') continue;
        bool lazy = (line.compare(0, 5, "lazy:") == 0);
        if (lazy) {
            line = line.substr(5);
            s_lazyPhaseNames.insert(line);
        }
        order.push_back(line);
    }
    return order.empty() ? s_defaultOrder : order;
}

void registerLazyPhase(const std::string& name, PhaseFn fn) {
    s_lazyPhases[name] = std::move(fn);
}

bool runPhaseLazy(const std::string& name) {
    auto it = s_lazyPhases.find(name);
    if (it == s_lazyPhases.end()) return false;
    if (s_lazyPhaseRun.count(name)) return true;
    it->second();
    s_lazyPhaseRun.insert(name);
    return true;
}

bool isPhaseLazy(const std::string& name) {
    initDefaultOrder();
    return s_lazyPhaseNames.count(name) != 0;
}

} // namespace Startup
} // namespace RawrXD
