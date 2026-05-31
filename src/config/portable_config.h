#pragma once
#include <string>

namespace RawrXD {
namespace Config {

class PortableConfig {
public:
    static void Initialize();
    static std::string GetBasePath();
    static std::string GetSettingsPath();
    static std::string GetModelsPath();
    static bool IsPortable();
};

} // namespace Config
} // namespace RawrXD
