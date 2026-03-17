#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QDateTime>

namespace RawrXD {

struct CoverageFileMetrics {
    QString filePath;
    int linesCovered = 0;
    int linesTotal = 0;
    int funcsCovered = 0;
    int funcsTotal = 0;
    int branchesCovered = 0;
    int branchesTotal = 0;
    double linePercent() const { return linesTotal > 0 ? (100.0 * linesCovered / linesTotal) : 0.0; }
    double funcPercent() const { return funcsTotal > 0 ? (100.0 * funcsCovered / funcsTotal) : 0.0; }
    double branchPercent() const { return branchesTotal > 0 ? (100.0 * branchesCovered / branchesTotal) : 0.0; }
};

struct CoverageReport {
    QMap<QString, CoverageFileMetrics> files; // key: normalized file path
    QDateTime generatedAt;
    QString tool; // lcov | cobertura | llvm-cov
    double totalLinePercent = 0.0;
    double totalFuncPercent = 0.0;
    double totalBranchPercent = 0.0;
};

class CoverageCollector : public QObject {
    Q_OBJECT
public:
    explicit CoverageCollector(QObject* parent = nullptr);

    void setWorkspace(const QString& rootPath);
    QString workspace() const { return m_workspace; }

    // Parse existing reports
    CoverageReport parseLcovInfo(const QString& infoPath);
    CoverageReport parseCoberturaXml(const QString& xmlPath);
    CoverageReport parseLlvmCovJson(const QString& jsonPath);

    // Auto-discover well-known report files under workspace/build dirs
    CoverageReport discoverAndLoad();
    
    // Public utility for path normalization
    static QString normalizePath(const QString& p);

signals:
    void coverageLoaded(const CoverageReport& report);
    void error(const QString& message);

private:
    static void accumulate(CoverageReport& dst, const CoverageReport& part);
    static void finalizeTotals(CoverageReport& rep);

    QString m_workspace;
};

} // namespace RawrXD
