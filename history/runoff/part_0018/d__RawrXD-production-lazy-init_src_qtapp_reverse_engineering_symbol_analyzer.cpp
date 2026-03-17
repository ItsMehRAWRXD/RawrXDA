/**
 * \file symbol_analyzer.cpp
 * \brief Implementation of symbol analyzer
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "symbol_analyzer.h"
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

using namespace RawrXD::ReverseEngineering;

SymbolAnalyzer::SymbolAnalyzer(const BinaryLoader& loader)
    : m_loader(loader) {
    analyze();
}

void SymbolAnalyzer::analyze() {
    analyzeExports();
    analyzeImports();
    
    qDebug() << "Analyzed" << m_symbols.size() << "total symbols";
}

void SymbolAnalyzer::analyzeExports() {
    for (const auto& exp : m_loader.exports()) {
        SymbolInfo sym;
        sym.name = exp.name;
        sym.mangledName = exp.name;
        sym.demangledName = demangle(exp.name);
        sym.address = exp.address;
        sym.isFunction = true;
        sym.isExported = true;
        sym.isImported = false;
        
        m_symbols.append(sym);
        m_symbolsByName[sym.name] = sym;
        m_symbolsByAddress[sym.address] = sym;
    }
}

void SymbolAnalyzer::analyzeImports() {
    for (const auto& imp : m_loader.imports()) {
        SymbolInfo sym;
        sym.name = imp.name;
        sym.moduleName = imp.moduleName;
        sym.address = imp.address;
        sym.isFunction = true;
        sym.isImported = true;
        sym.isExported = false;
        
        m_symbols.append(sym);
        m_symbolsByName[sym.name] = sym;
        m_symbolsByAddress[sym.address] = sym;
    }
}

SymbolInfo SymbolAnalyzer::findSymbol(const QString& name) const {
    auto it = m_symbolsByName.find(name);
    if (it != m_symbolsByName.end()) {
        return it.value();
    }
    return SymbolInfo();
}

SymbolInfo SymbolAnalyzer::findSymbolAt(uint64_t address) const {
    auto it = m_symbolsByAddress.find(address);
    if (it != m_symbolsByAddress.end()) {
        return it.value();
    }
    return SymbolInfo();
}

QVector<SymbolInfo> SymbolAnalyzer::exportedSymbols() const {
    QVector<SymbolInfo> result;
    for (const auto& sym : m_symbols) {
        if (sym.isExported) {
            result.append(sym);
        }
    }
    return result;
}

QVector<SymbolInfo> SymbolAnalyzer::importedSymbols() const {
    QVector<SymbolInfo> result;
    for (const auto& sym : m_symbols) {
        if (sym.isImported) {
            result.append(sym);
        }
    }
    return result;
}

QString SymbolAnalyzer::demangle(const QString& mangled) {
    if (mangled.isEmpty()) {
        return mangled;
    }
    
    // Try MSVC demangling
    if (mangled.startsWith("?")) {
        QString demangled = demangleMSVC(mangled);
        if (demangled != mangled) {
            return demangled;
        }
    }
    
    // Try GCC/Clang demangling
    if (mangled.startsWith("_Z")) {
        QString demangled = demangleGCC(mangled);
        if (demangled != mangled) {
            return demangled;
        }
    }
    
    return mangled;
}

QString SymbolAnalyzer::demangleMSVC(const QString& mangled) {
    // MSVC Name Mangling Rules (simplified)
    // Format: ?name@namespace@class@@
    
    if (!mangled.startsWith("?")) {
        return mangled;
    }
    
    // Simple extraction of class and function names
    QStringList parts = mangled.split("@");
    if (parts.size() >= 2) {
        QString funcName = parts[0].mid(1);  // Remove leading ?
        QString className = parts.size() >= 3 ? parts[1] : "";
        
        if (!className.isEmpty() && className != "A" && className != "B") {
            return className + "::" + funcName;
        }
        return funcName;
    }
    
    return mangled;
}

QString SymbolAnalyzer::demangleGCC(const QString& mangled) {
    // GCC/Clang Name Mangling Rules (simplified)
    // Format: _Z[number][name]...
    
    if (!mangled.startsWith("_Z")) {
        return mangled;
    }
    
    // Extract function name length and name
    int pos = 2;
    int nameLen = 0;
    
    // Parse number
    while (pos < mangled.length() && mangled[pos].isDigit()) {
        nameLen = nameLen * 10 + (mangled[pos].digitValue());
        ++pos;
    }
    
    if (pos + nameLen <= mangled.length()) {
        return mangled.mid(pos, nameLen);
    }
    
    return mangled;
}

QString SymbolAnalyzer::demangleClang(const QString& mangled) {
    // Clang uses similar mangling to GCC, delegate to GCC demangler
    return demangleGCC(mangled);
}
