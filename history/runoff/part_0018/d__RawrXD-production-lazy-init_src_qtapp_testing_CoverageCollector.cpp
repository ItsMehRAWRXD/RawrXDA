#include "CoverageCollector.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>

namespace RawrXD {

CoverageCollector::CoverageCollector(QObject* parent) : QObject(parent) {}

void CoverageCollector::setWorkspace(const QString& rootPath) { m_workspace = rootPath; }

static void addOrUpdate(CoverageReport& rep, const CoverageFileMetrics& fm) {
    QString key = CoverageCollector::normalizePath(fm.filePath);
    auto it = rep.files.find(key);
    if (it == rep.files.end()) {
        rep.files.insert(key, fm);
    } else {
        CoverageFileMetrics m = it.value();
        m.linesCovered += fm.linesCovered;
        m.linesTotal += fm.linesTotal;
        m.funcsCovered += fm.funcsCovered;
        m.funcsTotal += fm.funcsTotal;
        m.branchesCovered += fm.branchesCovered;
        m.branchesTotal += fm.branchesTotal;
        it.value() = m;
    }
}

CoverageReport CoverageCollector::parseLcovInfo(const QString& infoPath) {
    CoverageReport rep; rep.tool = "lcov"; rep.generatedAt = QDateTime::currentDateTime();
    QFile f(infoPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QString("Failed to open lcov file: %1").arg(infoPath));
        return rep;
    }
    QTextStream ts(&f);
    CoverageFileMetrics cur;
    bool inFile = false;
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        if (line.startsWith("SF:")) {
            if (inFile) { addOrUpdate(rep, cur); }
            cur = CoverageFileMetrics{};
            cur.filePath = line.mid(3).trimmed();
            inFile = true;
        } else if (line.startsWith("DA:")) {
            // DA:<line number>,<execution count>[,<checksum>]
            const auto parts = line.mid(3).split(',');
            if (parts.size() >= 2) {
                int hits = parts[1].toInt();
                cur.linesTotal++;
                if (hits > 0) cur.linesCovered++;
            }
        } else if (line.startsWith("FNF:")) {
            cur.funcsTotal += line.mid(4).toInt();
        } else if (line.startsWith("FNH:")) {
            cur.funcsCovered += line.mid(4).toInt();
        } else if (line.startsWith("BRF:")) {
            cur.branchesTotal += line.mid(4).toInt();
        } else if (line.startsWith("BRH:")) {
            cur.branchesCovered += line.mid(4).toInt();
        } else if (line == "end_of_record") {
            if (inFile) { addOrUpdate(rep, cur); inFile = false; }
        }
    }
    if (inFile) { addOrUpdate(rep, cur); }
    finalizeTotals(rep);
    return rep;
}

CoverageReport CoverageCollector::parseCoberturaXml(const QString& xmlPath) {
    CoverageReport rep; rep.tool = "cobertura"; rep.generatedAt = QDateTime::currentDateTime();
    QFile f(xmlPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(QString("Failed to open cobertura xml: %1").arg(xmlPath));
        return rep;
    }
    QXmlStreamReader xr(&f);
    QString currentFile;
    CoverageFileMetrics fm;
    while (!xr.atEnd()) {
        xr.readNext();
        if (xr.isStartElement()) {
            const QString name = xr.name().toString();
            if (name == "class") {
                // <class filename="...">
                const auto attrs = xr.attributes();
                currentFile = attrs.value("filename").toString();
                fm = CoverageFileMetrics{};
                fm.filePath = currentFile;
            } else if (name == "line") {
                // <line number="12" hits="1" ...>
                const auto attrs = xr.attributes();
                bool ok1=false, ok2=false; int num = attrs.value("number").toInt(&ok1); int hits = attrs.value("hits").toInt(&ok2);
                if (ok1 && ok2) {
                    fm.linesTotal++;
                    if (hits > 0) fm.linesCovered++;
                }
            }
        } else if (xr.isEndElement()) {
            if (xr.name() == QStringLiteral("class") && !fm.filePath.isEmpty()) {
                addOrUpdate(rep, fm);
                currentFile.clear();
                fm = CoverageFileMetrics{};
            }
        }
    }
    if (xr.hasError()) {
        emit error(QString("XML parse error in %1: %2").arg(xmlPath, xr.errorString()));
    }
    finalizeTotals(rep);
    return rep;
}

CoverageReport CoverageCollector::parseLlvmCovJson(const QString& jsonPath) {
    CoverageReport rep; rep.tool = "llvm-cov"; rep.generatedAt = QDateTime::currentDateTime();
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit error(QString("Failed to open llvm-cov json: %1").arg(jsonPath));
        return rep;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return rep;
    const auto root = doc.object();
    // Modern llvm-cov export has data->files->segments; fallback to totals if present
    QJsonArray data = root.value("data").toArray();
    for (const auto& dVal : data) {
        QJsonObject dObj = dVal.toObject();
        QJsonArray files = dObj.value("files").toArray();
        for (const auto& fVal : files) {
            QJsonObject fObj = fVal.toObject();
            QString path = fObj.value("filename").toString();
            CoverageFileMetrics fm; fm.filePath = path;
            // Prefer line summary if present
            if (fObj.contains("summary")) {
                QJsonObject sum = fObj.value("summary").toObject();
                fm.linesCovered = sum.value("lines").toObject().value("covered").toInt();
                fm.linesTotal = sum.value("lines").toObject().value("count").toInt();
                fm.funcsCovered = sum.value("functions").toObject().value("covered").toInt();
                fm.funcsTotal = sum.value("functions").toObject().value("count").toInt();
                fm.branchesCovered = sum.value("branches").toObject().value("covered").toInt();
                fm.branchesTotal = sum.value("branches").toObject().value("count").toInt();
            } else if (fObj.contains("segments")) {
                // segments: [ [line, col, count, hasCount, isRegionEntry, isGapRegion], ... ]
                QJsonArray segs = fObj.value("segments").toArray();
                QSet<int> seenLines;
                for (const auto& sVal : segs) {
                    QJsonArray s = sVal.toArray();
                    if (s.size() >= 3) {
                        int line = s.at(0).toInt();
                        int count = s.at(2).toInt();
                        if (!seenLines.contains(line)) {
                            fm.linesTotal++;
                            if (count > 0) fm.linesCovered++;
                            seenLines.insert(line);
                        }
                    }
                }
            }
            addOrUpdate(rep, fm);
        }
    }
    finalizeTotals(rep);
    return rep;
}

CoverageReport CoverageCollector::discoverAndLoad() {
    CoverageReport merged; merged.generatedAt = QDateTime::currentDateTime();
    if (m_workspace.isEmpty()) return merged;
    const QStringList candidates = {
        "coverage.info", "lcov.info", "coverage/lcov.info", "build/coverage.info",
        "coverage.xml", "coverage/coverage.xml", "build/coverage.xml",
        "llvm-cov.json", "coverage/llvm-cov.json", "build/llvm-cov.json"
    };
    QDir root(m_workspace);
    for (const auto& rel : candidates) {
        QString p = root.absoluteFilePath(rel);
        if (!QFileInfo::exists(p)) continue;
        if (p.endsWith(".info")) accumulate(merged, parseLcovInfo(p));
        else if (p.endsWith(".xml")) accumulate(merged, parseCoberturaXml(p));
        else if (p.endsWith(".json")) accumulate(merged, parseLlvmCovJson(p));
    }
    finalizeTotals(merged);
    if (!merged.files.isEmpty()) emit coverageLoaded(merged);
    return merged;
}

QString CoverageCollector::normalizePath(const QString& p) {
    return QDir::fromNativeSeparators(QFileInfo(p).canonicalFilePath().isEmpty() ? QFileInfo(p).absoluteFilePath() : QFileInfo(p).canonicalFilePath());
}

void CoverageCollector::accumulate(CoverageReport& dst, const CoverageReport& part) {
    for (auto it = part.files.constBegin(); it != part.files.constEnd(); ++it) {
        addOrUpdate(dst, it.value());
    }
}

void CoverageCollector::finalizeTotals(CoverageReport& rep) {
    int totalLines = 0, coveredLines = 0;
    int totalFuncs = 0, coveredFuncs = 0;
    int totalBranches = 0, coveredBranches = 0;
    for (auto it = rep.files.constBegin(); it != rep.files.constEnd(); ++it) {
        const auto& fm = it.value();
        totalLines += fm.linesTotal; coveredLines += fm.linesCovered;
        totalFuncs += fm.funcsTotal; coveredFuncs += fm.funcsCovered;
        totalBranches += fm.branchesTotal; coveredBranches += fm.branchesCovered;
    }
    rep.totalLinePercent = totalLines > 0 ? (100.0 * coveredLines / totalLines) : 0.0;
    rep.totalFuncPercent = totalFuncs > 0 ? (100.0 * coveredFuncs / totalFuncs) : 0.0;
    rep.totalBranchPercent = totalBranches > 0 ? (100.0 * coveredBranches / totalBranches) : 0.0;
}

} // namespace RawrXD
