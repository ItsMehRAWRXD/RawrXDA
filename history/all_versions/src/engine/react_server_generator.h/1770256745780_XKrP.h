#pragma once
#include <string>
#include <vector>

namespace RawrXD {

struct ReactServerConfig {
    std::string host;
    int port;
};

class ReactServerGenerator {
public:
    static std::string Generate(const std::string& html_template, const ReactServerConfig& config);
};

} // namespace RawrXD
