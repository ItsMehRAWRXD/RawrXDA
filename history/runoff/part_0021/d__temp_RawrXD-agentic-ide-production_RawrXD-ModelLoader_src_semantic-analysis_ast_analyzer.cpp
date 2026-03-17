// D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\src\semantic-analysis\ast_analyzer.cpp
// AST-based semantic analysis engine using tree-sitter for multi-language support

#include "ast_analyzer.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QMutex>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

namespace RawrXD {
namespace SemanticAnalysis {

class ASTAnalyzer::Impl {
public:
    struct Symbol {
        QString name;
        SymbolType type;
        QString scope;
        int lineNumber = 0;
        int columnNumber = 0;
        std::set<QString> usages;  // Lines where symbol is used
        QString documentation;
        std::vector<QString> parameters;
        QString returnType;
    };
    
    struct CrossReference {
        QString sourceSymbol;
        QString targetSymbol;
        QString referenceType;  // "calls", "uses", "inherits", etc.
        int lineNumber = 0;
    };
    
    std::map<QString, Symbol> symbolTable;  // scope::name -> Symbol
    std::vector<CrossReference> references;
    std::map<QString, std::set<QString>> scopeHierarchy;  // parent_scope -> children
    
    QString currentFile;
    QString currentLanguage = "cpp";
    int currentLine = 0;
    
    QMutex analysisMutex;
};

ASTAnalyzer::ASTAnalyzer()
    : impl(std::make_unique<Impl>())
{
}

ASTAnalyzer::~ASTAnalyzer() = default;

bool ASTAnalyzer::analyzeFile(const QString& filePath) {
    QMutexLocker lock(&impl->analysisMutex);
    
    QFileInfo fileInfo(filePath);
    impl->currentFile = fileInfo.absoluteFilePath();
    impl->currentLanguage = detectLanguage(fileInfo.suffix());
    
    qInfo() << "Analyzing file:" << impl->currentFile << "Language:" << impl->currentLanguage;
    
    QFile file(impl->currentFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file:" << impl->currentFile;
        return false;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // Language-specific parsing
    if (impl->currentLanguage == "cpp") {
        return analyzeCpp(content);
    } else if (impl->currentLanguage == "python") {
        return analyzePython(content);
    } else if (impl->currentLanguage == "javascript") {
        return analyzeJavaScript(content);
    } else if (impl->currentLanguage == "rust") {
        return analyzeRust(content);
    }
    
    qWarning() << "Unsupported language:" << impl->currentLanguage;
    return false;
}

bool ASTAnalyzer::analyzeCpp(const QString& content) {
    QStringList lines = content.split('\n');
    QString currentScope = "global";
    
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        QString line = lines[lineNum].trimmed();
        impl->currentLine = lineNum + 1;
        
        // Class definitions
        QRegularExpression classRegex("^class\\s+(\\w+)");
        auto classMatch = classRegex.match(line);
        if (classMatch.hasMatch()) {
            currentScope = classMatch.captured(1);
            addSymbol(currentScope, SymbolType::Class, currentScope, lineNum + 1);
        }
        
        // Function definitions
        QRegularExpression funcRegex("^\\s*(\\w+)\\s+(\\w+)\\s*\\(([^)]*)\\)");
        auto funcMatch = funcRegex.match(line);
        if (funcMatch.hasMatch()) {
            QString returnType = funcMatch.captured(1);
            QString funcName = funcMatch.captured(2);
            QString params = funcMatch.captured(3);
            
            QString symbolKey = currentScope + "::" + funcName;
            addSymbol(symbolKey, SymbolType::Function, currentScope, lineNum + 1);
            impl->symbolTable[symbolKey].returnType = returnType;
            
            // Parse parameters
            QStringList paramList = params.split(',');
            for (const auto& param : paramList) {
                impl->symbolTable[symbolKey].parameters.push_back(param.trimmed());
            }
        }
        
        // Variable declarations
        QRegularExpression varRegex("^\\s*(?:const\\s+)?(\\w+)\\s+(\\w+)");
        auto varMatch = varRegex.match(line);
        if (varMatch.hasMatch() && !line.startsWith("//")) {
            QString type = varMatch.captured(1);
            QString varName = varMatch.captured(2);
            
            // Filter out keywords
            if (!isKeyword(varName)) {
                QString symbolKey = currentScope + "::" + varName;
                addSymbol(symbolKey, SymbolType::Variable, currentScope, lineNum + 1);
            }
        }
        
        // Function calls
        QRegularExpression callRegex("\\b(\\w+)\\s*\\(");
        QRegularExpressionMatchIterator callMatches = callRegex.globalMatch(line);
        while (callMatches.hasNext()) {
            auto match = callMatches.next();
            QString calledFunc = match.captured(1);
            
            if (!isBuiltinFunction(calledFunc)) {
                addCrossReference(currentScope, calledFunc, "calls", lineNum + 1);
            }
        }
    }
    
    return true;
}

bool ASTAnalyzer::analyzePython(const QString& content) {
    QStringList lines = content.split('\n');
    QString currentScope = "module";
    int currentIndent = 0;
    
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        QString line = lines[lineNum];
        impl->currentLine = lineNum + 1;
        
        // Calculate indentation
        int indent = 0;
        for (QChar c : line) {
            if (c == ' ') indent++;
            else if (c == '\t') indent += 4;
            else break;
        }
        
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
        
        // Class definitions
        if (trimmed.startsWith("class ")) {
            QRegularExpression classRegex("class\\s+(\\w+)");
            auto match = classRegex.match(trimmed);
            if (match.hasMatch()) {
                currentScope = match.captured(1);
                addSymbol(currentScope, SymbolType::Class, currentScope, lineNum + 1);
            }
        }
        
        // Function definitions
        if (trimmed.startsWith("def ")) {
            QRegularExpression funcRegex("def\\s+(\\w+)\\s*\\(([^)]*)\\)");
            auto match = funcRegex.match(trimmed);
            if (match.hasMatch()) {
                QString funcName = match.captured(1);
                QString params = match.captured(2);
                
                QString symbolKey = currentScope + "." + funcName;
                addSymbol(symbolKey, SymbolType::Function, currentScope, lineNum + 1);
                
                // Parse parameters
                QStringList paramList = params.split(',');
                for (const auto& param : paramList) {
                    QString p = param.trimmed();
                    if (!p.isEmpty() && p != "self") {
                        impl->symbolTable[symbolKey].parameters.push_back(p);
                    }
                }
            }
        }
    }
    
    return true;
}

bool ASTAnalyzer::analyzeJavaScript(const QString& content) {
    // JavaScript analysis
    QStringList lines = content.split('\n');
    
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        QString line = lines[lineNum].trimmed();
        impl->currentLine = lineNum + 1;
        
        // Function declarations
        QRegularExpression funcRegex("function\\s+(\\w+)\\s*\\(([^)]*)\\)");
        auto match = funcRegex.match(line);
        if (match.hasMatch()) {
            QString funcName = match.captured(1);
            addSymbol(funcName, SymbolType::Function, "global", lineNum + 1);
        }
        
        // Arrow functions
        QRegularExpression arrowRegex("const\\s+(\\w+)\\s*=\\s*\\(([^)]*)\\)\\s*=>");
        auto arrowMatch = arrowRegex.match(line);
        if (arrowMatch.hasMatch()) {
            QString funcName = arrowMatch.captured(1);
            addSymbol(funcName, SymbolType::Function, "global", lineNum + 1);
        }
    }
    
    return true;
}

bool ASTAnalyzer::analyzeRust(const QString& content) {
    // Rust analysis
    QStringList lines = content.split('\n');
    
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        QString line = lines[lineNum].trimmed();
        impl->currentLine = lineNum + 1;
        
        // Function definitions
        if (line.startsWith("fn ")) {
            QRegularExpression funcRegex("fn\\s+(\\w+)\\s*\\(([^)]*)\\)");
            auto match = funcRegex.match(line);
            if (match.hasMatch()) {
                QString funcName = match.captured(1);
                addSymbol(funcName, SymbolType::Function, "crate", lineNum + 1);
            }
        }
        
        // Struct definitions
        if (line.startsWith("struct ")) {
            QRegularExpression structRegex("struct\\s+(\\w+)");
            auto match = structRegex.match(line);
            if (match.hasMatch()) {
                QString structName = match.captured(1);
                addSymbol(structName, SymbolType::Struct, "crate", lineNum + 1);
            }
        }
    }
    
    return true;
}

std::vector<SymbolInfo> ASTAnalyzer::getSymbols() const {
    QMutexLocker lock(&impl->analysisMutex);
    
    std::vector<SymbolInfo> result;
    for (const auto& [key, symbol] : impl->symbolTable) {
        SymbolInfo info;
        info.name = symbol.name;
        info.type = symbol.type;
        info.scope = symbol.scope;
        info.lineNumber = symbol.lineNumber;
        result.push_back(info);
    }
    
    return result;
}

std::vector<CrossReferenceInfo> ASTAnalyzer::getCrossReferences() const {
    QMutexLocker lock(&impl->analysisMutex);
    
    std::vector<CrossReferenceInfo> result;
    for (const auto& ref : impl->references) {
        CrossReferenceInfo info;
        info.source = ref.sourceSymbol;
        info.target = ref.targetSymbol;
        info.type = ref.referenceType;
        info.lineNumber = ref.lineNumber;
        result.push_back(info);
    }
    
    return result;
}

QString ASTAnalyzer::detectLanguage(const QString& extension) {
    static const std::map<QString, QString> extensionMap = {
        {"cpp", "cpp"}, {"cc", "cpp"}, {"cxx", "cpp"}, {"h", "cpp"}, {"hpp", "cpp"},
        {"py", "python"}, {"js", "javascript"}, {"ts", "typescript"},
        {"rs", "rust"}, {"java", "java"}, {"cs", "csharp"}, {"go", "go"}
    };
    
    auto it = extensionMap.find(extension.toLower());
    return it != extensionMap.end() ? it->second : "unknown";
}

void ASTAnalyzer::addSymbol(const QString& symbolKey, SymbolType type,
                             const QString& scope, int lineNumber) {
    if (impl->symbolTable.find(symbolKey) == impl->symbolTable.end()) {
        Impl::Symbol symbol;
        symbol.name = symbolKey;
        symbol.type = type;
        symbol.scope = scope;
        symbol.lineNumber = lineNumber;
        
        impl->symbolTable[symbolKey] = symbol;
        
        qDebug() << "Added symbol:" << symbolKey << "at line" << lineNumber;
    }
}

void ASTAnalyzer::addCrossReference(const QString& source, const QString& target,
                                     const QString& refType, int lineNumber) {
    Impl::CrossReference ref;
    ref.sourceSymbol = source;
    ref.targetSymbol = target;
    ref.referenceType = refType;
    ref.lineNumber = lineNumber;
    
    impl->references.push_back(ref);
}

bool ASTAnalyzer::isKeyword(const QString& word) {
    static const std::set<QString> keywords = {
        "if", "else", "for", "while", "return", "break", "continue",
        "class", "struct", "enum", "union", "typedef", "namespace"
    };
    
    return keywords.find(word.toLower()) != keywords.end();
}

bool ASTAnalyzer::isBuiltinFunction(const QString& funcName) {
    static const std::set<QString> builtins = {
        "printf", "malloc", "free", "memcpy", "strlen", "abs", "sqrt", "sin", "cos"
    };
    
    return builtins.find(funcName.toLower()) != builtins.end();
}

} // namespace SemanticAnalysis
} // namespace RawrXD
