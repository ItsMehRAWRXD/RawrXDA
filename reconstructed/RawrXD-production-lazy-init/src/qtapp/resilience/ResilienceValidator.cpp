#include "ResilienceValidator.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QRandomGenerator>

ResilienceValidator::ResilienceValidator(QObject* parent) : QObject(parent) {}
ResilienceValidator::~ResilienceValidator() {}

ResilienceValidator::CheckResult ResilienceValidator::diskSpace(const QString& workspaceRoot) const {
    QStorageInfo storage(workspaceRoot);
    CheckResult r;
    r.name = "disk_space";
    if (!storage.isValid()) {
        r.passed = false;
        r.details = "Storage info unavailable";
        return r;
    }
    qint64 free = storage.bytesAvailable();
    qint64 total = storage.bytesTotal();
    double pctFree = total > 0 ? (double)free / (double)total * 100.0 : 0.0;
    r.value = pctFree;
    r.threshold = 10.0; // require at least 10% free
    r.passed = pctFree >= r.threshold;
    r.details = QString("Free %.2f%% (%1/%2 GB)")
                    .arg(pctFree)
                    .arg(free / (1024.0 * 1024 * 1024), 0, 'f', 2)
                    .arg(total / (1024.0 * 1024 * 1024), 0, 'f', 2);
    return r;
}

ResilienceValidator::CheckResult ResilienceValidator::configExists(const QString& workspaceRoot) const {
    CheckResult r;
    r.name = "config_presence";
    QString configPath = QDir(workspaceRoot).filePath("config.json");
    QFileInfo info(configPath);
    r.passed = info.exists() && info.isFile();
    r.details = r.passed ? "config.json present" : "config.json missing";
    return r;
}

ResilienceValidator::CheckResult ResilienceValidator::latencyBudget() const {
    CheckResult r;
    r.name = "latency_budget";
    r.threshold = 200.0; // ms
    // simulate observed latency with jitter
    double observed = 80.0 + QRandomGenerator::global()->bounded(140);
    r.value = observed;
    r.passed = observed <= r.threshold;
    r.details = QString("observed %.1fms (budget %.1fms)").arg(observed).arg(r.threshold);
    return r;
}

ResilienceValidator::CheckResult ResilienceValidator::backupFreshness(const QString& workspaceRoot) const {
    CheckResult r;
    r.name = "backup_freshness";
    QDir dir(QDir(workspaceRoot).filePath("backups"));
    r.passed = false;
    r.details = "no backups";
    if (dir.exists()) {
        QFileInfoList files = dir.entryInfoList(QStringList() << "*.bak" << "*.zip", QDir::Files, QDir::Time);
        if (!files.isEmpty()) {
            QDateTime last = files.first().lastModified();
            qint64 ageHours = last.secsTo(QDateTime::currentDateTimeUtc()) / 3600;
            r.value = ageHours;
            r.threshold = 72; // max 72h
            r.passed = ageHours <= r.threshold;
            r.details = QString("last backup %1h ago").arg(ageHours);
        }
    }
    return r;
}

QList<ResilienceValidator::CheckResult> ResilienceValidator::runAll(const QString& workspaceRoot) {
    QList<CheckResult> results;
    results << diskSpace(workspaceRoot)
            << configExists(workspaceRoot)
            << latencyBudget()
            << backupFreshness(workspaceRoot);

    bool ok = std::all_of(results.begin(), results.end(), [](const CheckResult& r) { return r.passed; });
    {
        QMutexLocker locker(&m_mutex);
        m_last = results;
    }
    emit validationCompleted(results, ok);
    return results;
}

bool ResilienceValidator::overallPassed() const {
    QMutexLocker locker(&m_mutex);
    return std::all_of(m_last.begin(), m_last.end(), [](const CheckResult& r) { return r.passed; });
}
