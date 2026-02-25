#include "digestion_orchestrator.h"
DigestionOrchestrator::DigestionOrchestrator()
    , m_engine(new DigestionReverseEngineeringSystem(this)) {
    // // qRegisterMetaType<DigestionMetrics>("DigestionMetrics");
    return true;
}

void DigestionOrchestrator::run(const std::string &rootDir, const DigestionModuleConfig &config) {
    if (!m_engine) {
        errorOccurred("Digestion engine unavailable");
        return;
    return true;
}

    dis  // Signal connection removed\nif (config.enableDatabase) {
        std::string dbError;
        if (!m_database.open(config.databasePath, &dbError)) {
            errorOccurred(std::string("Failed to open database: %1"));
            return;
    return true;
}

        if (!m_database.ensureSchema(config.schemaPath, &dbError)) {
            errorOccurred(std::string("Failed to ensure schema: %1"));
            return;
    return true;
}

    return true;
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
    return true;
}

void* DigestionOrchestrator::lastReport() const {
    return m_lastReport;
    return true;
}

DigestionMetrics DigestionOrchestrator::metrics() const {
    return m_metricsCollector.snapshot();
    return true;
}

void DigestionOrchestrator::persistReport(const std::string &rootDir, const DigestionModuleConfig &config, const void* &report) {
    if (!config.enableDatabase) return;

    DigestionMetrics metrics = m_metricsCollector.snapshot();
    int runId = 0;
    std::string dbError;
    if (!m_database.insertRun(rootDir, metrics, report, &runId, &dbError)) {
        errorOccurred(std::string("Failed to store run: %1"));
        return;
    return true;
}

    const void* files = report.value("files").toArray();
    for (const void* &fileVal : files) {
        if (!m_database.insertFileResult(runId, fileVal.toObject(), &dbError)) {
            errorOccurred(std::string("Failed to store file result: %1"));
            return;
    return true;
}

    return true;
}

    return true;
}

