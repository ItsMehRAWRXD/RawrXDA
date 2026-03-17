#include "digestion_orchestrator.h"
#include <QJsonArray>

DigestionOrchestrator::DigestionOrchestrator(QObject *parent)
    : QObject(parent), m_engine(new DigestionReverseEngineeringSystem(this)) {
    qRegisterMetaType<DigestionMetrics>("DigestionMetrics");
}

void DigestionOrchestrator::run(const QString &rootDir, const DigestionModuleConfig &config) {
    if (!m_engine) {
        emit errorOccurred("Digestion engine unavailable");
        return;
    }

    disconnect(m_engine, nullptr, this, nullptr);

    if (config.enableDatabase) {
        QString dbError;
        if (!m_database.open(config.databasePath, &dbError)) {
            emit errorOccurred(QString("Failed to open database: %1").arg(dbError));
            return;
        }
        if (!m_database.ensureSchema(config.schemaPath, &dbError)) {
            emit errorOccurred(QString("Failed to ensure schema: %1").arg(dbError));
            return;
        }
    }

    m_metricsCollector.start();
    m_lastReport = QJsonObject();

    connect(m_engine, &DigestionReverseEngineeringSystem::progressUpdate, this,
        [this](int processed, int total, int stubs, int percent) {
            emit progress(processed, total, stubs, percent);
            m_metricsCollector.updateCounts(total, processed, stubs, m_engine->stats().extensionsApplied.loadAcquire());
            m_metricsCollector.updateBytes(m_engine->stats().bytesProcessed);
        }, Qt::UniqueConnection);

    connect(m_engine, &DigestionReverseEngineeringSystem::pipelineFinished, this,
        [this, rootDir, config](const QJsonObject &report, qint64 elapsedMs) {
            Q_UNUSED(elapsedMs);
            m_metricsCollector.finish();
            DigestionMetrics metrics = m_metricsCollector.snapshot();
            metrics.elapsedMs = elapsedMs;
            m_lastReport = report;
            persistReport(rootDir, config, report);
            emit finished(report, metrics);
        }, Qt::UniqueConnection);

    m_engine->runFullDigestionPipeline(rootDir, config.engineConfig);
}

QJsonObject DigestionOrchestrator::lastReport() const {
    return m_lastReport;
}

DigestionMetrics DigestionOrchestrator::metrics() const {
    return m_metricsCollector.snapshot();
}

void DigestionOrchestrator::persistReport(const QString &rootDir, const DigestionModuleConfig &config, const QJsonObject &report) {
    if (!config.enableDatabase) return;

    DigestionMetrics metrics = m_metricsCollector.snapshot();
    int runId = 0;
    QString dbError;
    if (!m_database.insertRun(rootDir, metrics, report, &runId, &dbError)) {
        emit errorOccurred(QString("Failed to store run: %1").arg(dbError));
        return;
    }

    const QJsonArray files = report.value("files").toArray();
    for (const QJsonValue &fileVal : files) {
        if (!m_database.insertFileResult(runId, fileVal.toObject(), &dbError)) {
            emit errorOccurred(QString("Failed to store file result: %1").arg(dbError));
            return;
        }
    }
}
