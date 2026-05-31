#include "portable_config.h"
#include "../hal/system_scout.h"
#include <filesystem>
#include <fstream>

namespace RawrXD {
namespace Config {

void PortableConfig::Initialize() {
    std::filesystem::create_directories(GetModelsPath());
    std::filesystem::create_directories(GetBasePath() + "\\logs");
}

std::string PortableConfig::GetBasePath() {
    return RawrXD::HAL::SystemScout::GetConfigPath();
}

std::string PortableConfig::GetSettingsPath() {
    return GetBasePath() + "\\settings.json";
}

std::string PortableConfig::GetModelsPath() {
    return GetBasePath() + "\\models";
}

bool PortableConfig::IsPortable() {
    return RawrXD::HAL::SystemScout::IsPortableMode();
}

} // namespace Config
} // namespace RawrXD
