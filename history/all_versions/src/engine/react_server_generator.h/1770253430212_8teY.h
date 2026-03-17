#pragma once
#include <string>
#include <vector>

struct ReactServerConfig {
    std::string name;
    std::string description;
    int port = 5173;
    bool useSSL = false;
    bool enableHotReload = true;
    std::string outputDir;
    std::vector<std::string> features;
};

namespace RawrXD {

class ReactServerGenerator {
public:
    static std::string Generate(const std::string& projectName, const ReactServerConfig& config);
    
private:
    static std::string GeneratePackageJson(const std::string& name, const ReactServerConfig& config);
    static std::string GenerateServerTs();
    static std::string GenerateApiRouter();
    static std::string GenerateWebsocketHandler();
};

} // namespace RawrXD
