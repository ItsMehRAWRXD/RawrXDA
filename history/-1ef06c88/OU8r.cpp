// symbol_parser.cpp - Implementation
#include "symbol_parser.hpp"
#include <QStringList>
#include <QRegularExpression>

QList<Symbol> SymbolParser::parseSymbols(const QString& content, const QString& fileExtension)
{
    QString ext = fileExtension.toLower();
    
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c" || ext == "h" || ext == "hpp" || ext == "hxx") {
        return parseCppSymbols(content);
    } else if (ext == "py") {
        return parsePythonSymbols(content);
    } else if (ext == "js" || ext == "ts" || ext == "jsx" || ext == "tsx") {
        return parseJavaScriptSymbols(content);
    } else {
        return parseGenericSymbols(content);
    }
}

QList<Symbol> SymbolParser::parseCppSymbols(const QString& content)
{
    QList<Symbol> symbols;
    
    // Match functions: return_type function_name(params) or return_type ClassName::function_name(params)
    QRegularExpression funcRegex(R"(^\s*(?:[\w:]+\s+)?(\w+)\s*::\s*(\w+)\s*\([^)]*\)|^\s*(?:[\w:]+\s+)?(\w+)\s*\([^)]*\)\s*(?:const)?\s*[{;])");
    funcRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match classes: class ClassName
    QRegularExpression classRegex(R"(^\s*class\s+(\w+))");
    classRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match structs: struct StructName
    QRegularExpression structRegex(R"(^\s*struct\s+(\w+))");
    structRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match enums: enum EnumName
    QRegularExpression enumRegex(R"(^\s*enum\s+(?:class\s+)?(\w+))");
    enumRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match namespaces: namespace Name
    QRegularExpression namespaceRegex(R"(^\s*namespace\s+(\w+))");
    namespaceRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match macros: #define MACRO_NAME
    QRegularExpression macroRegex(R"(^\s*#define\s+(\w+))");
    macroRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Find all matches
    auto iter = funcRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = !match.captured(2).isEmpty() ? match.captured(2) : match.captured(3);
        if (!name.isEmpty() && name != "if" && name != "while" && name != "for" && name != "switch") {
            int line = lineNumber(content, match.capturedStart());
            QString sig = match.captured(0).trimmed();
            if (sig.length() > 80) sig = sig.left(77) + "...";
            symbols.append(Symbol(Symbol::Function, name, line, sig));
        }
    }
    
    iter = classRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Class, name, line));
    }
    
    iter = structRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Struct, name, line));
    }
    
    iter = enumRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Enum, name, line));
    }
    
    iter = namespaceRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Namespace, name, line));
    }
    
    iter = macroRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Macro, name, line));
    }
    
    return symbols;
}

QList<Symbol> SymbolParser::parsePythonSymbols(const QString& content)
{
    QList<Symbol> symbols;
    
    // Match functions: def function_name(params):
    QRegularExpression funcRegex(R"(^\s*def\s+(\w+)\s*\([^)]*\)\s*:)");
    funcRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match classes: class ClassName:
    QRegularExpression classRegex(R"(^\s*class\s+(\w+))");
    classRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    auto iter = funcRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        QString sig = match.captured(0).trimmed();
        symbols.append(Symbol(Symbol::Function, name, line, sig));
    }
    
    iter = classRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Class, name, line));
    }
    
    return symbols;
}

QList<Symbol> SymbolParser::parseJavaScriptSymbols(const QString& content)
{
    QList<Symbol> symbols;
    
    // Match functions: function name() or const/var/let name = function() or const/var/let name = () =>
    QRegularExpression funcRegex(R"(^\s*(?:function\s+(\w+)|(?:const|let|var)\s+(\w+)\s*=\s*(?:function|\([^)]*\)\s*=>)))");
    funcRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    // Match classes: class ClassName
    QRegularExpression classRegex(R"(^\s*class\s+(\w+))");
    classRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    auto iter = funcRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
        if (!name.isEmpty()) {
            int line = lineNumber(content, match.capturedStart());
            QString sig = match.captured(0).trimmed();
            if (sig.length() > 60) sig = sig.left(57) + "...";
            symbols.append(Symbol(Symbol::Function, name, line, sig));
        }
    }
    
    iter = classRegex.globalMatch(content);
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        int line = lineNumber(content, match.capturedStart());
        symbols.append(Symbol(Symbol::Class, name, line));
    }
    
    return symbols;
}

QList<Symbol> SymbolParser::parseGenericSymbols(const QString& content)
{
    QList<Symbol> symbols;
    
    // Generic function pattern: word followed by (
    QRegularExpression funcRegex(R"(^\s*(?:function\s+)?(\w+)\s*\()");
    funcRegex.setPatternOptions(QRegularExpression::MultilineOption);
    
    auto iter = funcRegex.globalMatch(content);
    QSet<QString> seen;
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString name = match.captured(1);
        if (!name.isEmpty() && !seen.contains(name)) {
            seen.insert(name);
            int line = lineNumber(content, match.capturedStart());
            symbols.append(Symbol(Symbol::Function, name, line));
        }
    }
    
    return symbols;
}

int SymbolParser::lineNumber(const QString& content, int position)
{
    if (position < 0 || position > content.length()) {
        return 1;
    }
    
    int line = 1;
    for (int i = 0; i < position && i < content.length(); ++i) {
        if (content[i] == '\n') {
            line++;
        }
    }
    return line;
}
