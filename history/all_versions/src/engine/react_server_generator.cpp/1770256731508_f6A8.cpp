#include "engine/react_server_generator.h"
#include <sstream>

namespace RawrXD {

std::string ReactServerGenerator::Generate(const std::string& html_template, const ReactServerConfig& config) {
    std::string output = html_template;
    
    // Simple template injection
    std::string config_script = "<script>window.SERVER_CONFIG = { host: '" + config.host + "', port: " + std::to_string(config.port) + " };</script>";
    
    size_t head_pos = output.find("</head>");
    if (head_pos != std::string::npos) {
        output.insert(head_pos, config_script);
    } else {
        output += config_script;
    }
    
    return output;
}

} // namespace RawrXD
