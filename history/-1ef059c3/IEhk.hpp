// symbol_parser.hpp - Extract symbols from source code
#pragma once

#include <QString>
#include <QList>
#include <QRegularExpression>

// Symbol information extracted from source code
struct Symbol {
    enum Type {
        Function,
        Class,
        Struct,
        Enum,
        Namespace,
        Macro,
        Variable,
        Other
    };

    Type type;
    QString name;
    int lineNumber;
    QString signature;  // Optional: function signature or brief description

    Symbol() : type(Other), lineNumber(0) {}
    Symbol(Type t, const QString& n, int line, const QString& sig = QString())
        : type(t), name(n), lineNumber(line), signature(sig) {}
};

class SymbolParser
{
public:
    // Parse symbols from source code based on file extension
    static QList<Symbol> parseSymbols(const QString& content, const QString& fileExtension);

private:
    static QList<Symbol> parseCppSymbols(const QString& content);
    static QList<Symbol> parsePythonSymbols(const QString& content);
    static QList<Symbol> parseJavaScriptSymbols(const QString& content);
    static QList<Symbol> parseGenericSymbols(const QString& content);
    
    static int lineNumber(const QString& content, int position);
};
