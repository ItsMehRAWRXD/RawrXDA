#include "digestion_orchestrator.h"
DigestionOrchestrator::DigestionOrchestrator()
    , m_engine(new DigestionReverseEngineeringSystem(this)) {
    // // qRegisterMetaType<DigestionMetrics>("DigestionMetrics");
}

void DigestionOrchestrator::run(const std::string &rootDir, const DigestionModuleConfig &config) {
    if (!m_engine) {
        errorOccurred("Digestion engine unavailable");
        return;
    }

    dis  // Signal connection removed\nif (config.enableDatabase) {
        std::string dbError;
        if (!m_database.open(config.databasePath, &dbError)) {
            errorOccurred(std::string("Failed to open database: %1"));
            return;
        }
        if (!m_database.ensureSchema(config.schemaPath, &dbError)) {
            errorOccurred(std::string("Failed to ensure schema: %1"));
            return;
        }
    }

    m_metricsCollector.start();
    m_lastReport = void*();  // Signal connection removed\nm_metricsCollector.updateCounts(total, processed, stubs, m_engine->stats().extensionsApplied.loadAcquire());
            m_metricsCollector.updateBytes(m_engine->stats().bytesProcessed);
        }, UniqueConnection);  // Signal connection removed\nm_metricsCollector.finish();
            DigestionMetrics metrics = m_metricsCollector.snapshot();
            metrics.elapsedMs = elapsedMs;
            m_lastReport = report;
            persistReport(rootDir, config, report);
            finished(report, metrics);
        }, UniqueConnection);

    m_engine->runFullDigestionPipeline(rootDir, config.engineConfig);
}

void* DigestionOrchestrator::lastReport() const {
    return m_lastReport;
}

DigestionMetrics DigestionOrchestrator::metrics() const {
    return m_metricsCollector.snapshot();
}

void DigestionOrchestrator::persistReport(const std::string &rootDir, const DigestionModuleConfig &config, const void* &report) {
    if (!config.enableDatabase) return;

    DigestionMetrics metrics = m_metricsCollector.snapshot();
    int runId = 0;
    std::string dbError;
    if (!m_database.insertRun(rootDir, metrics, report, &runId, &dbError)) {
        errorOccurred(std::string("Failed to store run: %1"));
        return;
    }

    const void* files = report.value("files").toArray();
    for (const void* &fileVal : files) {
        if (!m_database.insertFileResult(runId, fileVal.toObject(), &dbError)) {
            errorOccurred(std::string("Failed to store file result: %1"));
            return;
        }
    }
}

