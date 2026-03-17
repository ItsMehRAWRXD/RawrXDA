#pragma once

#include "digestion_config_manager.h"
#include "digestion_db.h"
#include <memory>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class DigestionReverseEngineeringSystem; // Forward decl

class DigestionOrchestrator {
public:
    explicit DigestionOrchestrator();
    ~DigestionOrchestrator();

    void run(const std::string &rootDir, const DigestionModuleConfig &config);
    nlohmann::json lastReport() const;
    DigestionMetrics metrics() const;

    // Callbacks/Events
    std::function<void(int, int, int, int)> onProgress;
    std::function<void(const nlohmann::json&, const DigestionMetrics&)> onFinished;
    std::function<void(const std::string&)> onError;

private:
    void persistReport(const std::string &rootDir, const DigestionModuleConfig &config, const nlohmann::json &report);

    std::unique_ptr<DigestionReverseEngineeringSystem> m_engine;
    DigestionDatabase m_database;
    DigestionMetrics m_metrics; // Replaced custom collector with struct+logic
    nlohmann::json m_lastReport;
};
