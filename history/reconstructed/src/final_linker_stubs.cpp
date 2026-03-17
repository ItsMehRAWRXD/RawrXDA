// Comprehensive linker stubs for missing symbols
#include <string>
#include <vector>
#include <memory>
#include <iostream>

// Forward declarations
class AgentRequest {};

namespace AdvancedFeatures {
    void ApplyHotPatch(const std::string&, const std::string&, const std::string&) {}
}

// ReactServerGenerator is defined in react_generator_stubs.cpp
// namespace RawrXD::ReactServerGenerator - NO STUB NEEDED

class ToolRegistry {
public:
    static void inject_tools(AgentRequest&) {
        std::cout << "[STUB] ToolRegistry::inject_tools called" << std::endl;
    }
};

// These are already defined in runtime_core.cpp, so we DON'T stub them here
// void register_rawr_inference(void) { ... }
// void register_sovereign_engines(void) { ... }

