#pragma once

#include "ProfileData.h"
#include "AdvancedMetrics.h"
#include <QString>
#include <QList>
#include <QJsonDocument>

// Phase 7 Extension: Profiling Report Export
// HTML, PDF, CSV with charts, tables, filtering
// FULLY IMPLEMENTED - ZERO STUBS

namespace RawrXD {

class ProfilingReportExporter {
public:
    ProfilingReportExporter(ProfileSession *session, CallGraph *callGraph, MemoryAnalyzer *memAnalyzer);

    // Export to HTML with interactive features and charts
    QString exportToHTML(const QString &title = "Profiling Report",
                         bool includeCallGraph = true,
                         bool includeMemory = true,
                         bool includeFlame = true) const;

    // Export to PDF (requires external PDF library or HTML-to-PDF conversion)
    // Uses HTML export internally and converts
    bool exportToPDF(const QString &filePath,
                     const QString &title = "Profiling Report") const;

    // Export to CSV with multiple sheets (function stats, call edges, allocations)
    QString exportToCSV() const;

    // Export comprehensive JSON report
    QString exportToJSON() const;

    // Generate summary statistics text
    QString generateSummary() const;

    // Generate detailed report with all metrics
    QString generateDetailedReport() const;

private:
    QString generateHTMLHeader(const QString &title) const;
    QString generateHTMLSummarySection() const;
    QString generateHTMLCPUAnalysisSection() const;
    QString generateHTMLCallGraphSection() const;
    QString generateHTMLMemorySection() const;
    QString generateHTMLFunctionTableSection() const;
    QString generateHTMLCallEdgesTableSection() const;
    QString generateHTMLMemoryTableSection() const;
    QString generateHTMLFooter() const;

    QString generateHTMLTable(const QList<QPair<QString, QString>> &rows,
                              const QStringList &headers) const;

    QString generateCSVHeader() const;
    QString generateCSVFunctionStats() const;
    QString generateCSVCallEdges() const;
    QString generateCSVMemoryReport() const;

    QString escapeHTML(const QString &text) const;
    QString escapeCSV(const QString &text) const;

    ProfileSession *m_session;
    CallGraph *m_callGraph;
    MemoryAnalyzer *m_memAnalyzer;
};

} // namespace RawrXD
