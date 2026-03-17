#pragma once

#include <QtGlobal>
#include <QDateTime>
#include <QString>

struct DigestionMetrics {
    qint64 startMs = 0;
    qint64 endMs = 0;
    qint64 elapsedMs = 0;
    qint64 bytesProcessed = 0;
    int totalFiles = 0;
    int scannedFiles = 0;
    int stubsFound = 0;
    int fixesApplied = 0;
};

class DigestionMetricsCollector {
public:
    void start();
    void updateBytes(qint64 bytes);
    void updateCounts(int totalFiles, int scannedFiles, int stubsFound, int fixesApplied);
    void finish();
    DigestionMetrics snapshot() const;

private:
    DigestionMetrics m_metrics;
};
