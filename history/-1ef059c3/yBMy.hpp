// symbol_parser.hpp - Extract symbols from source code
#pragma once

#include "go_to_symbol_dialog.hpp"
#include <QString>
#include <QList>
#include <QRegularExpression>

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
