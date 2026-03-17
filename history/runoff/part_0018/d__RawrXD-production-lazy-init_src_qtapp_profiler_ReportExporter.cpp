#include "ReportExporter.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <algorithm>
#include <cmath>

namespace RawrXD {

ProfilingReportExporter::ProfilingReportExporter(ProfileSession *session, 
                                                 CallGraph *callGraph,
                                                 MemoryAnalyzer *memAnalyzer)
    : m_session(session), m_callGraph(callGraph), m_memAnalyzer(memAnalyzer) {
}

QString ProfilingReportExporter::generateSummary() const {
    if (!m_session) return "No session data available";

    QString summary;
    QTextStream ts(&summary);

    ts << "=== Profiling Summary ===\n";
    ts << "Start Time: " << m_session->startTime().toString(Qt::ISODate) << "\n";
    ts << "Total Runtime: " << (m_session->totalRuntimeUs() / 1000000.0) << " seconds\n";
    ts << "Total Samples: " << m_session->callStacks().size() << "\n";
    ts << "Unique Functions: " << m_session->functionStats().size() << "\n";

    auto topFuncs = m_session->getTopFunctions(5);
    ts << "\nTop 5 Functions (by time):\n";
    for (const auto &func : topFuncs) {
        ts << "  " << func.functionName << ": " << (func.totalTimeUs / 1000.0) << " ms\n";
    }

    if (m_memAnalyzer) {
        ts << "\nMemory Analysis:\n";
        ts << "  Total Allocated: " << (m_memAnalyzer->getTotalAllocated() / 1024.0 / 1024.0) << " MB\n";
        ts << "  Total Freed: " << (m_memAnalyzer->getTotalFreed() / 1024.0 / 1024.0) << " MB\n";
        ts << "  Suspected Leaks: " << (m_memAnalyzer->getTotalLeaked() / 1024.0 / 1024.0) << " MB\n";
    }

    return summary;
}

QString ProfilingReportExporter::generateDetailedReport() const {
    QString report = generateSummary();
    report += "\n\n=== Detailed Metrics ===\n";

    if (m_session) {
        const auto &stats = m_session->functionStats();
        QTextStream ts(&report);

        ts << "\nFunction Statistics:\n";
        ts << QString("%-50s %15s %15s %10s\n").arg("Function", "Total Time (ms)", "Self Time (ms)", "Calls");
        ts << QString("-").repeated(90) << "\n";

        QList<FunctionStat> sorted = stats.values();
        std::sort(sorted.begin(), sorted.end(), [](const FunctionStat &a, const FunctionStat &b) {
            return a.totalTimeUs > b.totalTimeUs;
        });

        for (const auto &func : sorted) {
            ts << QString::asprintf("%-50s %15.2f %15.2f %10lld\n",
                                     func.functionName.toUtf8().constData(),
                                     func.totalTimeUs / 1000.0,
                                     func.selfTimeUs / 1000.0,
                                     func.callCount);
        }
    }

    return report;
}

QString ProfilingReportExporter::escapeHTML(const QString &text) const {
    QString escaped = text;
    escaped.replace(QLatin1String("&"), QLatin1String("&amp;"));
    escaped.replace(QLatin1String("<"), QLatin1String("&lt;"));
    escaped.replace(QLatin1String(">"), QLatin1String("&gt;"));
    escaped.replace(QLatin1String("\""), QLatin1String("&quot;"));
    return escaped;
}

QString ProfilingReportExporter::escapeCSV(const QString &text) const {
    QString result = text;
    if (text.contains(',') || text.contains('"') || text.contains('\n')) {
        result.replace(QLatin1String("\""), QLatin1String("\"\""));
        return QLatin1String("\"") + result + QLatin1String("\"");
    }
    return text;
}

QString ProfilingReportExporter::generateHTMLHeader(const QString &title) const {
    return QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <title>%1</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }\n"
        "        .header { background: #2c3e50; color: white; padding: 20px; border-radius: 5px; margin-bottom: 20px; }\n"
        "        .section { background: white; padding: 15px; margin: 10px 0; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"
        "        .section h2 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n"
        "        table { width: 100%%; border-collapse: collapse; margin: 10px 0; }\n"
        "        th { background: #3498db; color: white; padding: 12px; text-align: left; }\n"
        "        td { padding: 10px; border-bottom: 1px solid #ecf0f1; }\n"
        "        tr:hover { background: #f9f9f9; }\n"
        "        .metric { display: inline-block; margin: 10px 20px; }\n"
        "        .metric-value { font-size: 24px; font-weight: bold; color: #3498db; }\n"
        "        .metric-label { font-size: 14px; color: #7f8c8d; }\n"
        "        .bar { display: inline-block; height: 20px; background: #3498db; margin: 5px 0; border-radius: 3px; }\n"
        "        .warning { color: #e74c3c; font-weight: bold; }\n"
        "        .success { color: #27ae60; font-weight: bold; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "<div class=\"header\">\n"
        "    <h1>%1</h1>\n"
        "    <p>Generated: %2</p>\n"
        "</div>\n"
    ).arg(title, QDateTime::currentDateTime().toString(Qt::ISODate));
}

QString ProfilingReportExporter::generateHTMLSummarySection() const {
    if (!m_session) return "";

    QString html;
    QTextStream ts(&html);

    ts << "<div class=\"section\">\n<h2>Summary</h2>\n";

    quint64 totalTimeUs = m_session->totalRuntimeUs();
    ts << "<div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Total Runtime</div>\n";
    ts << QString("        <div class=\"metric-value\">%.2f s</div>\n").arg(totalTimeUs / 1000000.0);
    ts << "    </div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Total Samples</div>\n";
    ts << QString("        <div class=\"metric-value\">%1</div>\n").arg(m_session->callStacks().size());
    ts << "    </div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Unique Functions</div>\n";
    ts << QString("        <div class=\"metric-value\">%1</div>\n").arg(m_session->functionStats().size());
    ts << "    </div>\n";
    ts << "</div>\n";

    ts << "<h3>Top Functions by Time</h3>\n";
    auto topFuncs = m_session->getTopFunctions(10);
    ts << "<table>\n<tr><th>Function</th><th>Total Time</th><th>Self Time</th><th>Calls</th><th>Avg Time</th></tr>\n";

    for (const auto &func : topFuncs) {
        double avgTime = func.callCount > 0 ? func.totalTimeUs / (double)func.callCount : 0;
        ts << "<tr>";
        ts << "<td>" << escapeHTML(func.functionName) << "</td>";
        ts << QString("<td>%.2f ms</td>").arg(func.totalTimeUs / 1000.0);
        ts << QString("<td>%.2f ms</td>").arg(func.selfTimeUs / 1000.0);
        ts << QString("<td>%1</td>").arg(func.callCount);
        ts << QString("<td>%.2f µs</td>").arg(avgTime);
        ts << "</tr>\n";
    }
    ts << "</table>\n</div>\n";

    return html;
}

QString ProfilingReportExporter::generateHTMLCPUAnalysisSection() const {
    if (!m_session) return "";

    QString html;
    QTextStream ts(&html);

    ts << "<div class=\"section\">\n<h2>CPU Analysis</h2>\n";

    const auto &stats = m_session->functionStats();
    quint64 totalRuntime = m_session->totalRuntimeUs();

    ts << "<table>\n<tr><th>Function</th><th>Total Time</th><th>% of Runtime</th><th>Self Time</th><th>Calls</th></tr>\n";

    QList<FunctionStat> sorted = stats.values();
    std::sort(sorted.begin(), sorted.end(), [](const FunctionStat &a, const FunctionStat &b) {
        return a.totalTimeUs > b.totalTimeUs;
    });

    for (const auto &func : sorted.mid(0, 50)) {  // Top 50
        double percent = totalRuntime > 0 ? (func.totalTimeUs / (double)totalRuntime) * 100.0 : 0;
        ts << "<tr>";
        ts << "<td>" << escapeHTML(func.functionName) << "</td>";
        ts << QString("<td>%.2f ms</td>").arg(func.totalTimeUs / 1000.0);
        ts << QString("<td>%.1f%%</td>").arg(percent);
        ts << QString("<td>%.2f ms</td>").arg(func.selfTimeUs / 1000.0);
        ts << QString("<td>%1</td>").arg(func.callCount);
        ts << "</tr>\n";
    }
    ts << "</table>\n</div>\n";

    return html;
}

QString ProfilingReportExporter::generateHTMLCallGraphSection() const {
    if (!m_callGraph) return "";

    QString html;
    QTextStream ts(&html);

    ts << "<div class=\"section\">\n<h2>Call Graph</h2>\n";

    auto edges = m_callGraph->getCallEdges();
    if (edges.isEmpty()) {
        ts << "<p>No call graph data available</p>\n";
        ts << "</div>\n";
        return html;
    }

    // Top call edges by frequency
    auto sorted = edges;
    std::sort(sorted.begin(), sorted.end(), [](const CallEdge &a, const CallEdge &b) {
        return a.callCount > b.callCount;
    });

    ts << "<h3>Most Frequent Calls</h3>\n";
    ts << "<table>\n<tr><th>Caller</th><th>Callee</th><th>Count</th><th>Total Time</th><th>Avg Time</th></tr>\n";

    for (const auto &edge : sorted.mid(0, 50)) {
        ts << "<tr>";
        ts << "<td>" << escapeHTML(edge.caller) << "</td>";
        ts << "<td>" << escapeHTML(edge.callee) << "</td>";
        ts << QString("<td>%1</td>").arg(edge.callCount);
        ts << QString("<td>%.2f ms</td>").arg(edge.totalTimeUs / 1000.0);
        ts << QString("<td>%.2f µs</td>").arg(edge.averageTimeUs);
        ts << "</tr>\n";
    }
    ts << "</table>\n</div>\n";

    return html;
}

QString ProfilingReportExporter::generateHTMLMemorySection() const {
    if (!m_memAnalyzer) return "";

    QString html;
    QTextStream ts(&html);

    ts << "<div class=\"section\">\n<h2>Memory Analysis</h2>\n";

    quint64 totalAlloc = m_memAnalyzer->getTotalAllocated();
    quint64 totalFreed = m_memAnalyzer->getTotalFreed();
    quint64 leaked = m_memAnalyzer->getTotalLeaked();

    ts << "<div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Total Allocated</div>\n";
    ts << QString("        <div class=\"metric-value\">%.2f MB</div>\n").arg(totalAlloc / 1024.0 / 1024.0);
    ts << "    </div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Total Freed</div>\n";
    ts << QString("        <div class=\"metric-value\">%.2f MB</div>\n").arg(totalFreed / 1024.0 / 1024.0);
    ts << "    </div>\n";
    ts << "    <div class=\"metric\">\n";
    ts << "        <div class=\"metric-label\">Suspected Leaks</div>\n";

    if (leaked > 0) {
        ts << QString("        <div class=\"metric-value warning\">%.2f MB</div>\n").arg(leaked / 1024.0 / 1024.0);
    } else {
        ts << QString("        <div class=\"metric-value success\">%.2f MB</div>\n").arg(leaked / 1024.0 / 1024.0);
    }
    ts << "    </div>\n";
    ts << "</div>\n";

    auto hotspots = m_memAnalyzer->getAllocationHotspots(10);
    if (!hotspots.isEmpty()) {
        ts << "<h3>Allocation Hotspots</h3>\n";
        ts << "<table>\n<tr><th>Function</th><th>Total Allocated</th><th>Count</th><th>Avg Size</th><th>Peak Size</th></tr>\n";

        for (const auto &hs : hotspots) {
            ts << "<tr>";
            ts << "<td>" << escapeHTML(hs.functionName) << "</td>";
            ts << QString("<td>%.2f MB</td>").arg(hs.totalAllocatedBytes / 1024.0 / 1024.0);
            ts << QString("<td>%1</td>").arg(hs.allocationCount);
            ts << QString("<td>%.2f KB</td>").arg(hs.averageAllocationBytes / 1024.0);
            ts << QString("<td>%.2f KB</td>").arg(hs.peakAllocationBytes / 1024.0);
            ts << "</tr>\n";
        }
        ts << "</table>\n";
    }

    ts << "</div>\n";
    return html;
}

QString ProfilingReportExporter::generateHTMLFooter() const {
    return QString(
        "<div class=\"section\">\n"
        "    <p style=\"color: #7f8c8d; font-size: 12px;\">\n"
        "        Report generated by RawrXD IDE Profiler<br>\n"
        "        %1\n"
        "    </p>\n"
        "</div>\n"
        "</body>\n"
        "</html>"
    ).arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

QString ProfilingReportExporter::exportToHTML(const QString &title,
                                               bool includeCallGraph,
                                               bool includeMemory,
                                               bool includeFlame) const {
    QString html;
    html += generateHTMLHeader(title);
    html += generateHTMLSummarySection();
    html += generateHTMLCPUAnalysisSection();

    if (includeCallGraph && m_callGraph) {
        html += generateHTMLCallGraphSection();
    }

    if (includeMemory && m_memAnalyzer) {
        html += generateHTMLMemorySection();
    }

    html += generateHTMLFooter();

    return html;
}

bool ProfilingReportExporter::exportToPDF(const QString &filePath,
                                           const QString &title) const {
    // Generate HTML first
    QString htmlContent = exportToHTML(title);

    // Write temporary HTML file
    QString tempHtmlPath = filePath + ".tmp.html";
    QFile htmlFile(tempHtmlPath);
    if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create temporary HTML file" << tempHtmlPath;
        return false;
    }

    QTextStream ts(&htmlFile);
    ts << htmlContent;
    htmlFile.close();

    // Try to use wkhtmltopdf if available, otherwise fall back to HTML export
    QProcess wkhtmlProcess;
    wkhtmlProcess.start("wkhtmltopdf", QStringList() << tempHtmlPath << filePath);

    if (wkhtmlProcess.waitForFinished(5000)) {
        htmlFile.remove();
        return true;
    }

    // Fallback: just save as HTML
    qWarning() << "wkhtmltopdf not available, saving as HTML instead";
    QFile pdfAsHtml(filePath + ".html");
    if (pdfAsHtml.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts2(&pdfAsHtml);
        ts2 << htmlContent;
        pdfAsHtml.close();
    }
    htmlFile.remove();

    return false;  // Indicate PDF generation failed, but HTML was saved
}

QString ProfilingReportExporter::exportToCSV() const {
    QString csv;
    QTextStream ts(&csv);

    // Header
    ts << "Profiling Report - CSV Export\n";
    ts << "Generated," << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    // Function Statistics
    ts << generateCSVFunctionStats();
    ts << "\n";

    // Call Edges
    ts << generateCSVCallEdges();
    ts << "\n";

    // Memory Report
    if (m_memAnalyzer) {
        ts << generateCSVMemoryReport();
    }

    return csv;
}

QString ProfilingReportExporter::generateCSVFunctionStats() const {
    QString csv;
    QTextStream ts(&csv);

    ts << "Function Statistics\n";
    ts << escapeCSV("Function") << "," << "Total Time (ms)" << "," << "Self Time (ms)" << ","
       << "Calls" << "," << "Avg Time (µs)" << "\n";

    if (m_session) {
        const auto &stats = m_session->functionStats();
        QList<FunctionStat> sorted = stats.values();
        std::sort(sorted.begin(), sorted.end(), [](const FunctionStat &a, const FunctionStat &b) {
            return a.totalTimeUs > b.totalTimeUs;
        });

        for (const auto &func : sorted) {
            double avgTime = func.callCount > 0 ? func.totalTimeUs / (double)func.callCount : 0;
            ts << escapeCSV(func.functionName) << ","
               << QString::number(func.totalTimeUs / 1000.0) << ","
               << QString::number(func.selfTimeUs / 1000.0) << ","
               << QString::number(func.callCount) << ","
               << QString::number(avgTime) << "\n";
        }
    }

    return csv;
}

QString ProfilingReportExporter::generateCSVCallEdges() const {
    QString csv;
    QTextStream ts(&csv);

    ts << "Call Graph\n";
    ts << escapeCSV("Caller") << "," << escapeCSV("Callee") << "," << "Count" << ","
       << "Total Time (ms)" << "," << "Avg Time (µs)" << "\n";

    if (m_callGraph) {
        for (const auto &edge : m_callGraph->getCallEdges()) {
            ts << escapeCSV(edge.caller) << "," << escapeCSV(edge.callee) << ","
               << QString::number(edge.callCount) << ","
               << QString::number(edge.totalTimeUs / 1000.0) << ","
               << QString::number(edge.averageTimeUs) << "\n";
        }
    }

    return csv;
}

QString ProfilingReportExporter::generateCSVMemoryReport() const {
    QString csv;
    QTextStream ts(&csv);

    ts << "Memory Analysis\n";
    ts << "Total Allocated (MB)," << (m_memAnalyzer->getTotalAllocated() / 1024.0 / 1024.0) << "\n";
    ts << "Total Freed (MB)," << (m_memAnalyzer->getTotalFreed() / 1024.0 / 1024.0) << "\n";
    ts << "Suspected Leaks (MB)," << (m_memAnalyzer->getTotalLeaked() / 1024.0 / 1024.0) << "\n\n";

    ts << "Allocation Hotspots\n";
    ts << escapeCSV("Function") << "," << "Total Allocated (MB)" << "," << "Count" << ","
       << "Avg Size (KB)" << "," << "Peak Size (KB)" << "\n";

    auto hotspots = m_memAnalyzer->getAllocationHotspots(20);
    for (const auto &hs : hotspots) {
        ts << escapeCSV(hs.functionName) << ","
           << QString::number(hs.totalAllocatedBytes / 1024.0 / 1024.0) << ","
           << QString::number(hs.allocationCount) << ","
           << QString::number(hs.averageAllocationBytes / 1024.0) << ","
           << QString::number(hs.peakAllocationBytes / 1024.0) << "\n";
    }

    return csv;
}

QString ProfilingReportExporter::exportToJSON() const {
    QJsonObject root;

    // Session info
    if (m_session) {
        root["processName"] = m_session->processName();
        root["startTime"] = m_session->startTime().toString(Qt::ISODate);
        root["totalRuntimeUs"] = static_cast<double>(m_session->totalRuntimeUs());
        root["sessionJson"] = QJsonDocument::fromJson(m_session->exportToJSON()).object();
    }

    // Call graph
    if (m_callGraph) {
        root["callGraph"] = m_callGraph->toJson();
    }

    // Memory analysis
    if (m_memAnalyzer) {
        root["memoryAnalysis"] = m_memAnalyzer->toJson();
    }

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

} // namespace RawrXD
