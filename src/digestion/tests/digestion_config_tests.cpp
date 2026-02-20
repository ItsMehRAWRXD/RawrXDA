// digestion_config_tests.cpp — C++20, no Qt. Uses nlohmann::json.

#include "../digestion_config_manager.h"
#include <cassert>
#include <string>

static void loadJsonConfig() {
    nlohmann::json j;
    j["digestion"] = nlohmann::json::object({
        {"chunk_size", 1024},
        {"threads", 2},
        {"apply_fixes", true},
        {"flags", nlohmann::json::array({"DEFLATE", "CRC"})}
    });
    j["database"] = nlohmann::json::object({
        {"path", "test.db"},
        {"enabled", true}
    });

    std::string error;
    DigestionModuleConfig cfg = DigestionConfigManager::loadFromJson(j, &error);
    assert(error.empty());
    assert(cfg.engineConfig.chunkSize == 1024);
    assert(cfg.engineConfig.threadCount == 2);
    assert(cfg.engineConfig.applyExtensions == true);
    assert(cfg.flags.size() == 2);
    assert(cfg.databasePath == "test.db");
}

static void loadYamlConfig() {
    const std::string yaml = R"(
digestion:
  chunk_size: 2048
  threads: 4
  flags: [DEFLATE, CRC]
database:
  path: digestion_results.db
  enabled: true
)";

    std::string error;
    DigestionModuleConfig cfg = DigestionConfigManager::loadFromYaml(yaml, &error);
    assert(error.empty());
    assert(cfg.engineConfig.chunkSize == 2048);
    assert(cfg.engineConfig.threadCount == 4);
    assert(cfg.flags.size() == 2);
    assert(cfg.databasePath == "digestion_results.db");
}

int main() {
    loadJsonConfig();
    loadYamlConfig();
    return 0;
}
