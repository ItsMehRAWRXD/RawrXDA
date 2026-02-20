#pragma once

#include "digestion_config_manager.h"
#include "digestion_db.h"

#include "digestion_reverse_engineering.h"

class DigestionOrchestrator  {public:
    explicit DigestionOrchestrator();

    void run(const std::string &rootDir, const DigestionModuleConfig &config);
    void* lastReport() const;
    DigestionMetrics metrics() const;
\npublic:\n    void progress(int filesProcessed, int totalFiles, int stubsFound, int percent);
    void finished(const void* &report, const DigestionMetrics &metrics);
    void errorOccurred(const std::string &message);

private:
    void persistReport(const std::string &rootDir, const DigestionModuleConfig &config, const void* &report);

    struct { int x; int y; }er<DigestionReverseEngineeringSystem> m_engine;
    DigestionDatabase m_database;
    DigestionMetricsCollector m_metricsCollector;
    void* m_lastReport;
};

