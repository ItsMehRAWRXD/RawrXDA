#include "meta_learn.hpp"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <QSysInfo>
#include <QCryptographicHash>
#include <QThread>
#include <QDir>
#include <QDebug>
#include <QtGlobal>
#include <QStringList>
#include <QVariant>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>

namespace {

QString ensureDatabasePath() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.rawrxd");
    }

    QDir dir(base);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    return dir.filePath(QStringLiteral("perf_db.json"));
}

QString defaultGpuLabel() {
    const QString pretty = QSysInfo::prettyProductName();
    if (!pretty.isEmpty()) {
        return pretty;
    }
    const QString host = QSysInfo::machineHostName();
    if (!host.isEmpty()) {
        return host;
    }
    return QStringLiteral("unknown-gpu");
}

QString computeHardwareHash() {
    const QStringList parts{
        QSysInfo::currentCpuArchitecture(),
        QSysInfo::machineHostName(),
        QSysInfo::productType(),
        QSysInfo::productVersion(),
        QString::number(QThread::idealThreadCount())
    };

    const QByteArray payload = parts.join('|').toUtf8();
    const QByteArray hash = QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex().left(32));
}

} // namespace

QJsonArray MetaLearn::loadDB(bool* ok) {
    if (ok) {
        *ok = true;
    }

    QFile f(ensureDatabasePath());
    if (!f.exists()) {
        return {};
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "MetaLearn: failed to open" << f.fileName();
        if (ok) {
            *ok = false;
        }
        return {};
    }

    const QByteArray raw = f.readAll();
    f.close();

    if (raw.isEmpty()) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        qWarning() << "MetaLearn: invalid database format";
        if (ok) {
            *ok = false;
        }
        return {};
    }

    return doc.array();
}

MetaLearn::MetaLearn(QObject* parent)
    : QObject(parent),
      m_dbPath(ensureDatabasePath()) {
    loadDatabase();
}

QString MetaLearn::gpuHash() const {
    return hardwareKey();
}

QString MetaLearn::hardwareKey() const {
    return computeHardwareHash();
}

QString MetaLearn::resolveGpuLabel(const QString& explicitGpu) const {
    const QString trimmed = explicitGpu.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    return defaultGpuLabel();
}

bool MetaLearn::record(const QString& quant,
                       const QString& kernel,
                       const QString& gpu,
                       double tps,
                       double ppl) {
    PerfRecord rec;
    rec.quant = quant.trimmed().toUpper();
    rec.kernel = kernel.trimmed().toUpper();
    rec.gpu = resolveGpuLabel(gpu);
    rec.hardware = hardwareKey();
    rec.tps = (std::isfinite(tps) && tps > 0.0) ? tps : 0.0;
    rec.ppl = (std::isfinite(ppl) && ppl > 0.0) ? ppl : 0.0;
    rec.timestamp = QDateTime::currentMSecsSinceEpoch();

    if (rec.quant.isEmpty()) {
        rec.quant = QStringLiteral("UNKNOWN");
    }
    if (rec.kernel.isEmpty()) {
        rec.kernel = QStringLiteral("UNKNOWN");
    }

    m_records.append(rec);
    emit recordAdded(rec);

    if (!saveDatabase()) {
        m_records.removeLast();
        qWarning() << "MetaLearn: failed to persist record";
        return false;
    }

    return true;
}

bool MetaLearn::autoTuneQuant() {
    QString bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        qInfo() << "MetaLearn: no quant data available for auto-tuning";
        return false;
    }

    if (m_lastQuantSuggestion == bestQuant) {
        return true;
    }

    m_lastQuantSuggestion = bestQuant;
    emit suggestionReady(bestQuant);
    qInfo() << "MetaLearn: auto-selected quant" << bestQuant
            << "avg TPS" << avgTps << "avg PPL" << avgPpl;
    return true;
}

bool MetaLearn::autoTuneKernel() {
    QString bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        qInfo() << "MetaLearn: no kernel data available for auto-tuning";
        return false;
    }

    if (m_lastKernelSuggestion == bestKernel) {
        return true;
    }

    m_lastKernelSuggestion = bestKernel;
    emit kernelSuggestionReady(bestKernel);
    qInfo() << "MetaLearn: auto-selected kernel" << bestKernel
            << "avg TPS" << avgTps;
    return true;
}

QString MetaLearn::suggestQuant() const {
    QString bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        return QStringLiteral("Q4_0");
    }
    return bestQuant;
}

QString MetaLearn::suggestKernel() const {
    QString bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        return QStringLiteral("AVX2");
    }
    return bestKernel;
}

QList<PerfRecord> MetaLearn::getHistory(const QString& quant) const {
    if (quant.isEmpty()) {
        return m_records;
    }

    QList<PerfRecord> filtered;
    const QString qUpper = quant.trimmed().toUpper();
    for (const PerfRecord& rec : m_records) {
        if (rec.quant == qUpper) {
            filtered.append(rec);
        }
    }
    return filtered;
}

bool MetaLearn::loadDatabase() {
    m_records.clear();

    bool ok = false;
    const QJsonArray arr = MetaLearn::loadDB(&ok);
    if (!ok) {
        return false;
    }
    if (arr.isEmpty()) {
        return true;
    }
    m_records.reserve(arr.size());
    for (const QJsonValue& val : arr) {
        const QJsonObject obj = val.toObject();
        PerfRecord rec;
        rec.quant = obj.value(QStringLiteral("quant")).toString().trimmed().toUpper();
        rec.kernel = obj.value(QStringLiteral("kernel")).toString().trimmed().toUpper();
        rec.gpu = obj.value(QStringLiteral("gpu")).toString();
        rec.hardware = obj.value(QStringLiteral("sha256")).toString();
        if (rec.hardware.isEmpty()) {
            rec.hardware = obj.value(QStringLiteral("hardware")).toString();
        }
        rec.tps = obj.value(QStringLiteral("tps")).toDouble();
        rec.ppl = obj.value(QStringLiteral("ppl")).toDouble();
        rec.timestamp = static_cast<qint64>(obj.value(QStringLiteral("when")).toVariant().toLongLong());

        if (rec.quant.isEmpty()) {
            rec.quant = QStringLiteral("UNKNOWN");
        }
        if (rec.kernel.isEmpty()) {
            rec.kernel = QStringLiteral("UNKNOWN");
        }
        if (rec.gpu.isEmpty()) {
            rec.gpu = defaultGpuLabel();
        }
        if (rec.hardware.isEmpty()) {
            rec.hardware = hardwareKey();
        }
        if (rec.timestamp <= 0) {
            rec.timestamp = QDateTime::currentMSecsSinceEpoch();
        }

        m_records.append(rec);
    }

    qInfo() << "MetaLearn: loaded" << m_records.size() << "records";
    return true;
}

bool MetaLearn::saveDatabase() const {
    QJsonArray arr;
    // Note: QJsonArray doesn't have reserve() in Qt 6.7

    for (const PerfRecord& rec : m_records) {
        QJsonObject obj;
        obj.insert(QStringLiteral("quant"), rec.quant);
        obj.insert(QStringLiteral("kernel"), rec.kernel);
        obj.insert(QStringLiteral("gpu"), rec.gpu);
        obj.insert(QStringLiteral("sha256"), rec.hardware);
        obj.insert(QStringLiteral("tps"), rec.tps);
        obj.insert(QStringLiteral("ppl"), rec.ppl);
        obj.insert(QStringLiteral("when"), rec.timestamp);
        arr.append(obj);
    }

    QFileInfo info(m_dbPath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "MetaLearn: cannot create directory for" << m_dbPath;
        return false;
    }

    QFile f(m_dbPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "MetaLearn: failed to write" << m_dbPath;
        return false;
    }

    const QJsonDocument doc(arr);
    if (f.write(doc.toJson(QJsonDocument::Compact)) == -1) {
        qWarning() << "MetaLearn: failed to flush database";
        f.close();
        return false;
    }

    f.close();
    return true;
}

bool MetaLearn::computeQuantSuggestion(QString* bestQuant,
                                       double* avgTps,
                                       double* avgPpl) const {
    struct QuantStats {
        double sumTps = 0.0;
        double sumPpl = 0.0;
        int count = 0;
    };

    const QString key = hardwareKey();
    QHash<QString, QuantStats> stats;

    for (const PerfRecord& rec : m_records) {
        if (!rec.hardware.isEmpty() && rec.hardware != key) {
            continue;
        }
        if (rec.quant.isEmpty()) {
            continue;
        }
        if (!std::isfinite(rec.tps) || rec.tps <= 0.0) {
            continue;
        }
        if (!std::isfinite(rec.ppl) || rec.ppl <= 0.0) {
            continue;
        }

        QuantStats& entry = stats[rec.quant];
        entry.sumTps += rec.tps;
        entry.sumPpl += rec.ppl;
        entry.count += 1;
    }

    if (stats.isEmpty()) {
        if (bestQuant) {
            bestQuant->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        if (avgPpl) {
            *avgPpl = 0.0;
        }
        return false;
    }

    double bestPplValue = std::numeric_limits<double>::max();
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avg = it.value().sumPpl / it.value().count;
        bestPplValue = std::min(bestPplValue, avg);
    }

    const double pplLimit = bestPplValue * 1.05;
    QString chosen;
    double chosenTps = 0.0;
    double chosenPpl = 0.0;
    bool found = false;

    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avgT = it.value().sumTps / it.value().count;
        const double avgP = it.value().sumPpl / it.value().count;
        if (avgP <= pplLimit && (!found || avgT > chosenTps)) {
            found = true;
            chosen = it.key();
            chosenTps = avgT;
            chosenPpl = avgP;
        }
    }

    if (!found) {
        for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
            if (!it.value().count) {
                continue;
            }
            const double avgT = it.value().sumTps / it.value().count;
            const double avgP = it.value().sumPpl / it.value().count;
            if (!found || avgP < chosenPpl || (std::abs(avgP - chosenPpl) < 1e-6 && avgT > chosenTps)) {
                found = true;
                chosen = it.key();
                chosenTps = avgT;
                chosenPpl = avgP;
            }
        }
    }

    if (!found) {
        if (bestQuant) {
            bestQuant->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        if (avgPpl) {
            *avgPpl = 0.0;
        }
        return false;
    }

    if (bestQuant) {
        *bestQuant = chosen;
    }
    if (avgTps) {
        *avgTps = chosenTps;
    }
    if (avgPpl) {
        *avgPpl = chosenPpl;
    }
    return true;
}

bool MetaLearn::computeKernelSuggestion(QString* bestKernel,
                                        double* avgTps) const {
    struct KernelStats {
        double sumTps = 0.0;
        int count = 0;
    };

    const QString key = hardwareKey();
    QHash<QString, KernelStats> stats;

    for (const PerfRecord& rec : m_records) {
        if (rec.kernel.isEmpty()) {
            continue;
        }
        if (!rec.hardware.isEmpty() && rec.hardware != key) {
            continue;
        }
        if (!std::isfinite(rec.tps) || rec.tps <= 0.0) {
            continue;
        }

        KernelStats& entry = stats[rec.kernel];
        entry.sumTps += rec.tps;
        entry.count += 1;
    }

    if (stats.isEmpty()) {
        if (bestKernel) {
            bestKernel->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        return false;
    }

    QString chosen;
    double chosenTps = 0.0;
    bool found = false;

    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avgT = it.value().sumTps / it.value().count;
        if (!found || avgT > chosenTps) {
            found = true;
            chosen = it.key();
            chosenTps = avgT;
        }
    }

    if (!found) {
        if (bestKernel) {
            bestKernel->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        return false;
    }

    if (bestKernel) {
        *bestKernel = chosen;
    }
    if (avgTps) {
        *avgTps = chosenTps;
    }
    return true;
}
