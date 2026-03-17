/**
 * \file report_generator.h
 * \brief Generate analysis reports and export results
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include "binary_loader.h"
#include "symbol_analyzer.h"
#include "disassembler.h"
#include "decompiler.h"

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \enum ReportFormat
 * \brief Output format for reports
 */
enum class ReportFormat {
    HTML,
    MARKDOWN,
    PDF,
    CSV,
    JSON,
    PLAIN_TEXT
};

/**
 * \class ReportGenerator
 * \brief Generate comprehensive reverse engineering reports
 */
class ReportGenerator {
public:
    /**
     * \brief Construct report generator
     */
    explicit ReportGenerator();
    
    /**
     * \brief Generate binary analysis report
     * \param loader Binary loader
     * \param symbolAnalyzer Symbol analyzer
     * \param disassembler Disassembler
     * \param decompiler Decompiler
     * \param format Output format
     * \return Report content
     */
    QString generateReport(const BinaryLoader& loader,
                          const SymbolAnalyzer& symbolAnalyzer,
                          const Disassembler& disassembler,
                          const Decompiler& decompiler,
                          ReportFormat format = ReportFormat::HTML);
    
    /**
     * \brief Export symbols to CSV
     * \param symbols Vector of symbols
     * \return CSV content
     */
    QString exportSymbolsCSV(const QVector<Symbol>& symbols);
    
    /**
     * \brief Export functions to C header
     * \param functions Vector of decompiled functions
     * \return C header content
     */
    QString exportFunctionsHeader(const QVector<DecompiledFunction>& functions);
    
    /**
     * \brief Export analysis to JSON
     * \param loader Binary loader
     * \param disassembler Disassembler
     * \return JSON content
     */
    QString exportToJSON(const BinaryLoader& loader,
                        const Disassembler& disassembler);
    
    /**
     * \brief Save report to file
     * \param filePath Output file path
     * \param content Report content
     * \return True if successful
     */
    static bool saveReport(const QString& filePath, const QString& content);

private:
    QString generateHTMLReport(const BinaryLoader& loader,
                               const SymbolAnalyzer& symbolAnalyzer,
                               const Disassembler& disassembler);
    
    QString generateMarkdownReport(const BinaryLoader& loader,
                                   const SymbolAnalyzer& symbolAnalyzer,
                                   const Disassembler& disassembler);
    
    QString generatePlainTextReport(const BinaryLoader& loader,
                                    const SymbolAnalyzer& symbolAnalyzer,
                                    const Disassembler& disassembler);
    
    QString escapeHTML(const QString& text);
    QString generateTimestamp();
};

} // namespace ReverseEngineering
} // namespace RawrXD
