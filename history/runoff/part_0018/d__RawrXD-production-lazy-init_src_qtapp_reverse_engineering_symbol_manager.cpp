/**
 * \file symbol_manager.cpp
 * \brief Implementation of symbol manager
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "symbol_manager.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

using namespace RawrXD::ReverseEngineering;

SymbolManager::SymbolManager(const SymbolAnalyzer& analyzer)
    : m_analyzer(analyzer) {
}

bool SymbolManager::renameSymbol(const QString& originalName, const QString& newName) {
    if (originalName.isEmpty() || newName.isEmpty()) {
        return false;
    }
    
    SymbolAnnotation& annotation = m_annotations[originalName];
    annotation.originalName = originalName;
    annotation.customName = newName;
    
    qDebug() << "Renamed symbol:" << originalName << "->" << newName;
    return true;
}

QString SymbolManager::getDisplayName(const QString& symbolName) const {
    auto it = m_annotations.find(symbolName);
    if (it != m_annotations.end() && !it.value().customName.isEmpty()) {
        return it.value().customName;
    }
    return symbolName;
}

void SymbolManager::addComment(const QString& symbolName, const QString& comment) {
    m_annotations[symbolName].originalName = symbolName;
    m_annotations[symbolName].comment = comment;
    qDebug() << "Added comment to symbol:" << symbolName;
}

QString SymbolManager::getComment(const QString& symbolName) const {
    auto it = m_annotations.find(symbolName);
    if (it != m_annotations.end()) {
        return it.value().comment;
    }
    return QString();
}

QVector<SymbolInfo> SymbolManager::matchSignature(const QString& signature) const {
    QVector<SymbolInfo> matches;
    
    for (const auto& symbol : m_analyzer.symbols()) {
        // Simple matching: check if signature contains symbol name
        if (signature.contains(symbol.name)) {
            matches.append(symbol);
        }
    }
    
    return matches;
}

int SymbolManager::autoRenameSymbols() {
    int count = 0;
    
    for (const auto& symbol : m_analyzer.symbols()) {
        QString suggested = suggestName(symbol);
        if (suggested != symbol.name) {
            renameSymbol(symbol.name, suggested);
            count++;
        }
    }
    
    qDebug() << "Auto-renamed" << count << "symbols";
    return count;
}

bool SymbolManager::exportToCSV(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << filename;
        return false;
    }
    
    QTextStream stream(&file);
    
    // Write header
    stream << "Original Name,Custom Name,Address,Type,Comment,Confirmed\n";
    
    // Write symbols
    for (const auto& symbol : m_analyzer.symbols()) {
        auto it = m_annotations.find(symbol.name);
        QString customName = (it != m_annotations.end()) ? it.value().customName : "";
        QString comment = (it != m_annotations.end()) ? it.value().comment : "";
        bool confirmed = (it != m_annotations.end()) ? it.value().isConfirmed : false;
        
        stream << QString("\"%1\",\"%2\",0x%3,\"%4\",\"%5\",%6\n")
                  .arg(symbol.name)
                  .arg(customName)
                  .arg(symbol.address, 8, 16, QChar('0'))
                  .arg(symbol.type)
                  .arg(comment)
                  .arg(confirmed ? "yes" : "no");
    }
    
    file.close();
    
    qDebug() << "Exported symbols to" << filename;
    return true;
}

bool SymbolManager::importFromCSV(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << filename;
        return false;
    }
    
    QTextStream stream(&file);
    
    // Skip header
    stream.readLine();
    
    int count = 0;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        QStringList fields = line.split(",");
        
        if (fields.size() >= 3) {
            QString originalName = fields[0].remove("\"");
            QString customName = fields[1].remove("\"");
            
            if (!customName.isEmpty()) {
                renameSymbol(originalName, customName);
                count++;
            }
        }
    }
    
    file.close();
    
    qDebug() << "Imported" << count << "symbols from" << filename;
    return true;
}

int SymbolManager::applyNamingConventions() {
    int count = 0;
    
    for (const auto& symbol : m_analyzer.symbols()) {
        QString newName = symbol.name;
        
        // Apply C++ conventions
        if (looksLikeConstructor(symbol.name)) {
            newName = "ctor_" + symbol.name;
        } else if (looksLikeDestructor(symbol.name)) {
            newName = "dtor_" + symbol.name;
        } else if (looksLikeGetter(symbol.name)) {
            // Already has get_ prefix
        } else if (looksLikeSetter(symbol.name)) {
            // Already has set_ prefix
        }
        
        if (newName != symbol.name) {
            renameSymbol(symbol.name, newName);
            count++;
        }
    }
    
    return count;
}

void SymbolManager::confirmSymbol(const QString& symbolName) {
    m_annotations[symbolName].isConfirmed = true;
    qDebug() << "Confirmed symbol:" << symbolName;
}

QVector<SymbolInfo> SymbolManager::confirmedSymbols() const {
    QVector<SymbolInfo> confirmed;
    
    for (const auto& symbol : m_analyzer.symbols()) {
        auto it = m_annotations.find(symbol.name);
        if (it != m_annotations.end() && it.value().isConfirmed) {
            confirmed.append(symbol);
        }
    }
    
    return confirmed;
}

QString SymbolManager::suggestName(const SymbolInfo& symbol) {
    QString suggested = symbol.name;
    
    // Pattern: address-based names
    if (symbol.name.startsWith("sub_") || symbol.name.startsWith("loc_")) {
        // Try to infer name from demangle
        if (!symbol.demangledName.isEmpty() && symbol.demangledName != symbol.mangledName) {
            suggested = symbol.demangledName;
        }
    }
    
    return suggested;
}

bool SymbolManager::looksLikeConstructor(const QString& name) {
    return name.contains("__ctor") || name.contains("__init") || 
           (name.contains("?") && name.contains("@@"));  // MSVC mangled ctor
}

bool SymbolManager::looksLikeDestructor(const QString& name) {
    return name.contains("__dtor") || name.contains("__fini") ||
           (name.contains("?") && name.contains("@"));  // MSVC mangled dtor
}

bool SymbolManager::looksLikeGetter(const QString& name) {
    return name.contains("get_") || name.contains("Get") || name.contains("is");
}

bool SymbolManager::looksLikeSetter(const QString& name) {
    return name.contains("set_") || name.contains("Set");
}
