/**
 * \file report_generator.cpp
 * \brief Implementation of report generator
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "report_generator.h"
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace RawrXD::ReverseEngineering;

ReportGenerator::ReportGenerator() = default;

QString ReportGenerator::generateReport(const BinaryLoader& loader,
                                        const SymbolAnalyzer& symbolAnalyzer,
                                        const Disassembler& disassembler,
                                        const Decompiler& decompiler,
                                        ReportFormat format) {
    switch (format) {
        case ReportFormat::HTML:
            return generateHTMLReport(loader, symbolAnalyzer, disassembler);
        case ReportFormat::MARKDOWN:
            return generateMarkdownReport(loader, symbolAnalyzer, disassembler);
        case ReportFormat::JSON:
            return exportToJSON(loader, disassembler);
        case ReportFormat::CSV:
            return exportSymbolsCSV(symbolAnalyzer.symbols());
        case ReportFormat::PLAIN_TEXT:
        default:
            return generatePlainTextReport(loader, symbolAnalyzer, disassembler);
    }
}

QString ReportGenerator::exportSymbolsCSV(const QVector<Symbol>& symbols) {
    QString csv = "Address,Name,Type,Module\n";
    
    for (const auto& symbol : symbols) {
        csv += QString("0x%1,%2,%3,%4\n")
            .arg(symbol.address, 8, 16, QChar('0'))
            .arg(symbol.name)
            .arg(symbol.type)
            .arg(symbol.moduleName);
    }
    
    return csv;
}

QString ReportGenerator::exportFunctionsHeader(const QVector<DecompiledFunction>& functions) {
    QString header;
    header += "#pragma once\n\n";
    header += "// Auto-generated header from RawrXD reverse engineering\n";
    header += "// " + generateTimestamp() + "\n\n";
    header += "#include <cstdint>\n\n";
    
    for (const auto& func : functions) {
        header += QString("// Function at 0x%1\n").arg(func.address, 8, 16, QChar('0'));
        header += "// Complexity: " + QString::number(func.complexity) + "\n";
        header += func.signature + ";\n\n";
    }
    
    return header;
}

QString ReportGenerator::exportToJSON(const BinaryLoader& loader,
                                      const Disassembler& disassembler) {
    QJsonObject root;
    
    // Binary metadata
    QJsonObject binaryObj;
    auto meta = loader.metadata();
    binaryObj["format"] = BinaryLoader::formatName(meta.format);
    binaryObj["architecture"] = BinaryLoader::architectureName(meta.architecture);
    binaryObj["sectionCount"] = (int)meta.sectionCount;
    binaryObj["exportCount"] = (int)meta.exportCount;
    binaryObj["importCount"] = (int)meta.importCount;
    root["binary"] = binaryObj;
    
    // Functions
    QJsonArray functionsArray;
    for (const auto& func : disassembler.functions()) {
        QJsonObject funcObj;
        funcObj["name"] = func.name;
        funcObj["address"] = QString("0x%1").arg(func.address, 8, 16, QChar('0'));
        funcObj["size"] = (int)func.size;
        funcObj["instructionCount"] = (int)func.instructions.size();
        functionsArray.append(funcObj);
    }
    root["functions"] = functionsArray;
    
    // Timestamp
    root["timestamp"] = generateTimestamp();
    
    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson());
}

bool ReportGenerator::saveReport(const QString& filePath, const QString& content) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to save report:" << filePath;
        return false;
    }
    
    file.write(content.toUtf8());
    file.close();
    
    qDebug() << "Report saved:" << filePath;
    return true;
}

QString ReportGenerator::generateHTMLReport(const BinaryLoader& loader,
                                            const SymbolAnalyzer& symbolAnalyzer,
                                            const Disassembler& disassembler) {
    QString html;
    html += "<!DOCTYPE html>\n<html>\n<head>\n";
    html += "    <title>RawrXD Binary Analysis Report</title>\n";
    html += "    <style>\n";
    html += "        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }\n";
    html += "        h1 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }\n";
    html += "        h2 { color: #555; margin-top: 20px; }\n";
    html += "        table { border-collapse: collapse; width: 100%; margin: 10px 0; background: white; }\n";
    html += "        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    html += "        th { background: #007bff; color: white; }\n";
    html += "        .section { background: white; padding: 15px; margin: 10px 0; border-radius: 5px; }\n";
    html += "        .timestamp { color: #999; font-size: 0.9em; }\n";
    html += "    </style>\n</head>\n<body>\n";
    
    html += "<h1>Binary Analysis Report</h1>\n";
    html += "<p class='timestamp'>Generated: " + generateTimestamp() + "</p>\n";
    
    // Binary Info
    html += "<div class='section'>\n<h2>Binary Information</h2>\n";
    auto meta = loader.metadata();
    html += "<table>\n";
    html += "<tr><th>Property</th><th>Value</th></tr>\n";
    html += "<tr><td>Format</td><td>" + BinaryLoader::formatName(meta.format) + "</td></tr>\n";
    html += "<tr><td>Architecture</td><td>" + BinaryLoader::architectureName(meta.architecture) + "</td></tr>\n";
    html += "<tr><td>Sections</td><td>" + QString::number(meta.sectionCount) + "</td></tr>\n";
    html += "<tr><td>Exports</td><td>" + QString::number(meta.exportCount) + "</td></tr>\n";
    html += "<tr><td>Imports</td><td>" + QString::number(meta.importCount) + "</td></tr>\n";
    html += "</table>\n</div>\n";
    
    // Symbols
    html += "<div class='section'>\n<h2>Symbols (" + QString::number(symbolAnalyzer.symbols().size()) + ")</h2>\n";
    html += "<table>\n";
    html += "<tr><th>Address</th><th>Name</th><th>Type</th><th>Module</th></tr>\n";
    for (const auto& symbol : symbolAnalyzer.symbols().mid(0, 50)) {  // First 50
        html += "<tr>";
        html += "<td>0x" + QString("%1").arg(symbol.address, 8, 16, QChar('0')) + "</td>";
        html += "<td>" + escapeHTML(symbol.name) + "</td>";
        html += "<td>" + symbol.type + "</td>";
        html += "<td>" + symbol.moduleName + "</td>";
        html += "</tr>\n";
    }
    if (symbolAnalyzer.symbols().size() > 50) {
        html += "<tr><td colspan='4'>... and " + QString::number(symbolAnalyzer.symbols().size() - 50) + " more</td></tr>\n";
    }
    html += "</table>\n</div>\n";
    
    // Functions
    html += "<div class='section'>\n<h2>Functions (" + QString::number(disassembler.functions().size()) + ")</h2>\n";
    html += "<table>\n";
    html += "<tr><th>Address</th><th>Name</th><th>Size</th><th>Instructions</th></tr>\n";
    for (const auto& func : disassembler.functions().mid(0, 50)) {  // First 50
        html += "<tr>";
        html += "<td>0x" + QString("%1").arg(func.address, 8, 16, QChar('0')) + "</td>";
        html += "<td>" + escapeHTML(func.name) + "</td>";
        html += "<td>" + QString::number(func.size) + "</td>";
        html += "<td>" + QString::number(func.instructions.size()) + "</td>";
        html += "</tr>\n";
    }
    if (disassembler.functions().size() > 50) {
        html += "<tr><td colspan='4'>... and " + QString::number(disassembler.functions().size() - 50) + " more</td></tr>\n";
    }
    html += "</table>\n</div>\n";
    
    html += "</body>\n</html>\n";
    
    return html;
}

QString ReportGenerator::generateMarkdownReport(const BinaryLoader& loader,
                                                const SymbolAnalyzer& symbolAnalyzer,
                                                const Disassembler& disassembler) {
    QString markdown;
    markdown += "# Binary Analysis Report\n\n";
    markdown += "**Generated:** " + generateTimestamp() + "\n\n";
    
    // Binary Info
    markdown += "## Binary Information\n\n";
    auto meta = loader.metadata();
    markdown += "| Property | Value |\n";
    markdown += "|----------|-------|\n";
    markdown += "| Format | " + BinaryLoader::formatName(meta.format) + " |\n";
    markdown += "| Architecture | " + BinaryLoader::architectureName(meta.architecture) + " |\n";
    markdown += "| Sections | " + QString::number(meta.sectionCount) + " |\n";
    markdown += "| Exports | " + QString::number(meta.exportCount) + " |\n";
    markdown += "| Imports | " + QString::number(meta.importCount) + " |\n\n";
    
    // Symbols
    markdown += "## Symbols (" + QString::number(symbolAnalyzer.symbols().size()) + ")\n\n";
    markdown += "| Address | Name | Type | Module |\n";
    markdown += "|---------|------|------|--------|\n";
    for (const auto& symbol : symbolAnalyzer.symbols().mid(0, 50)) {
        markdown += QString("| 0x%1 | %2 | %3 | %4 |\n")
            .arg(symbol.address, 8, 16, QChar('0'))
            .arg(symbol.name)
            .arg(symbol.type)
            .arg(symbol.moduleName);
    }
    if (symbolAnalyzer.symbols().size() > 50) {
        markdown += "| ... | ... | ... | ... |\n";
    }
    markdown += "\n";
    
    // Functions
    markdown += "## Functions (" + QString::number(disassembler.functions().size()) + ")\n\n";
    markdown += "| Address | Name | Size | Instructions |\n";
    markdown += "|---------|------|------|---------------|\n";
    for (const auto& func : disassembler.functions().mid(0, 50)) {
        markdown += QString("| 0x%1 | %2 | %3 | %4 |\n")
            .arg(func.address, 8, 16, QChar('0'))
            .arg(func.name)
            .arg(func.size)
            .arg(func.instructions.size());
    }
    if (disassembler.functions().size() > 50) {
        markdown += "| ... | ... | ... | ... |\n";
    }
    
    return markdown;
}

QString ReportGenerator::generatePlainTextReport(const BinaryLoader& loader,
                                                 const SymbolAnalyzer& symbolAnalyzer,
                                                 const Disassembler& disassembler) {
    QString text;
    text += "================================================================================\n";
    text += "                     RawrXD Binary Analysis Report\n";
    text += "================================================================================\n\n";
    text += "Generated: " + generateTimestamp() + "\n\n";
    
    // Binary Info
    text += "BINARY INFORMATION\n";
    text += "==================\n";
    auto meta = loader.metadata();
    text += "Format:       " + BinaryLoader::formatName(meta.format) + "\n";
    text += "Architecture: " + BinaryLoader::architectureName(meta.architecture) + "\n";
    text += "Sections:     " + QString::number(meta.sectionCount) + "\n";
    text += "Exports:      " + QString::number(meta.exportCount) + "\n";
    text += "Imports:      " + QString::number(meta.importCount) + "\n\n";
    
    // Symbols
    text += "SYMBOLS (" + QString::number(symbolAnalyzer.symbols().size()) + ")\n";
    text += "========================================\n";
    for (const auto& symbol : symbolAnalyzer.symbols().mid(0, 50)) {
        text += QString("0x%1  %2  (%3) from %4\n")
            .arg(symbol.address, 8, 16, QChar('0'))
            .arg(symbol.name, -40)
            .arg(symbol.type)
            .arg(symbol.moduleName);
    }
    if (symbolAnalyzer.symbols().size() > 50) {
        text += "... and " + QString::number(symbolAnalyzer.symbols().size() - 50) + " more\n";
    }
    text += "\n";
    
    // Functions
    text += "FUNCTIONS (" + QString::number(disassembler.functions().size()) + ")\n";
    text += "========================================\n";
    for (const auto& func : disassembler.functions().mid(0, 50)) {
        text += QString("0x%1  %2  (%3 bytes, %4 instructions)\n")
            .arg(func.address, 8, 16, QChar('0'))
            .arg(func.name, -40)
            .arg(func.size)
            .arg(func.instructions.size());
    }
    if (disassembler.functions().size() > 50) {
        text += "... and " + QString::number(disassembler.functions().size() - 50) + " more\n";
    }
    
    text += "\n================================================================================\n";
    
    return text;
}

QString ReportGenerator::escapeHTML(const QString& text) {
    QString escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

QString ReportGenerator::generateTimestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}
