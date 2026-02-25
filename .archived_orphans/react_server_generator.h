#pragma once
#include <string>
#include <vector>

namespace RawrXD {

struct ReactServerConfig {
    std::string name;
    std::string description;
    std::string outputDir;
    int port = 3000;
    std::vector<std::string> features;
};

class ReactServerGenerator {
public:
    static std::string Generate(const std::string& project_name, const ReactServerConfig& config);
};

} // namespace RawrXD
