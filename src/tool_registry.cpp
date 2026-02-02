#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace RawrXD {

class ToolRegistry {
public:
    void registerTool(const std::string& name) {
        spdlog::info("Tool registered: {}", name);
    }
};

}
