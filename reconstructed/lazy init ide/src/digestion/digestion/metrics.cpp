#include "digestion_metrics.h"

void DigestionMetricsCollector::start() {
    m_metrics.startMs = QDateTime::currentMSecsSinceEpoch();
    m_metrics.endMs = 0;
    m_metrics.elapsedMs = 0;
}

void DigestionMetricsCollector::updateBytes(qint64 bytes) {
    m_metrics.bytesProcessed = bytes;
}

void DigestionMetricsCollector::updateCounts(int totalFiles, int scannedFiles, int stubsFound, int fixesApplied) {
    m_metrics.totalFiles = totalFiles;
    m_metrics.scannedFiles = scannedFiles;
    m_metrics.stubsFound = stubsFound;
    m_metrics.fixesApplied = fixesApplied;
}

void DigestionMetricsCollector::finish() {
    m_metrics.endMs = QDateTime::currentMSecsSinceEpoch();
    if (m_metrics.startMs > 0) {
        m_metrics.elapsedMs = m_metrics.endMs - m_metrics.startMs;
    }
}

DigestionMetrics DigestionMetricsCollector::snapshot() const {
    return m_metrics;
}
