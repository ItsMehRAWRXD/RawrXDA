// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\include\ast_analyzer.h
// AST analyzer header for semantic code analysis

#pragma once

#include <QString>
#include <memory>
#include <vector>

namespace RawrXD {
namespace SemanticAnalysis {

enum class SymbolType {
    Class,
    Function,
    Variable,
    Struct,
    Enum,
    Interface,
    Method,
    Property,
    Namespace
};

struct SymbolInfo {
    QString name;
    SymbolType type;
    QString scope;
    int lineNumber;
};

struct CrossReferenceInfo {
    QString source;
    QString target;
    QString type;  // calls, uses, inherits, implements
    int lineNumber;
};

class ASTAnalyzer {
public:
    class Impl;
    
    ASTAnalyzer();
    ~ASTAnalyzer();
    
    // File analysis
    bool analyzeFile(const QString& filePath);
    
    // Language-specific analysis
    bool analyzeCpp(const QString& content);
    bool analyzePython(const QString& content);
    bool analyzeJavaScript(const QString& content);
    bool analyzeRust(const QString& content);
    
    // Query results
    std::vector<SymbolInfo> getSymbols() const;
    std::vector<CrossReferenceInfo> getCrossReferences() const;
    
private:
    QString detectLanguage(const QString& extension);
    void addSymbol(const QString& symbolKey, SymbolType type,
                  const QString& scope, int lineNumber);
    void addCrossReference(const QString& source, const QString& target,
                          const QString& refType, int lineNumber);
    bool isKeyword(const QString& word);
    bool isBuiltinFunction(const QString& funcName);
    
    std::unique_ptr<Impl> impl;
};

} // namespace SemanticAnalysis
} // namespace RawrXD
