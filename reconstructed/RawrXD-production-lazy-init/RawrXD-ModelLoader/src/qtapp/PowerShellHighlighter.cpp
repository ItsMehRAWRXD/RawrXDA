#include "PowerShellHighlighter.h"

PowerShellHighlighter::PowerShellHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
}

void PowerShellHighlighter::setupFormats()
{
    // Keyword format
    m_keywordFormat.setForeground(Qt::blue);
    m_keywordFormat.setFontWeight(QFont::Bold);
    
    QStringList keywordPatterns;
    keywordPatterns << "\\bif\\b" << "\\belseif\\b" << "\\belse\\b" << "\\bwhile\\b"
                   << "\\bfor\\b" << "\\bforeach\\b" << "\\bswitch\\b" << "\\bcatch\\b"
                   << "\\btry\\b" << "\\bfinally\\b" << "\\bfunction\\b" << "\\bparam\\b"
                   << "\\breturn\\b" << "\\bbreak\\b" << "\\bcontinue\\b" << "\\bthrow\\b";
    
    for (const QString& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    // Built-in cmdlet format
    m_builtinFormat.setForeground(QColor(0, 128, 64));
    m_builtinFormat.setFontWeight(QFont::Bold);
    
    QStringList builtinPatterns;
    builtinPatterns << "\\bGet-\\w+" << "\\bSet-\\w+" << "\\bNew-\\w+" << "\\bRemove-\\w+"
                   << "\\bTest-\\w+" << "\\bInvoke-\\w+" << "\\bStart-\\w+" << "\\bStop-\\w+"
                   << "\\bWrite-\\w+" << "\\bRead-\\w+" << "\\bAdd-\\w+" << "\\bImport-\\w+"
                   << "\\bExport-\\w+" << "\\bConvert-\\w+";
    
    for (const QString& pattern : builtinPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_builtinFormat;
        m_highlightingRules.append(rule);
    }
    
    // String format
    m_stringFormat.setForeground(Qt::darkRed);
    
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\".*?\"|'.*?'");
    stringRule.format = m_stringFormat;
    m_highlightingRules.append(stringRule);
    
    // Number format
    m_numberFormat.setForeground(Qt::darkMagenta);
    
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b\\d+\\b");
    numberRule.format = m_numberFormat;
    m_highlightingRules.append(numberRule);
    
    // Operator format
    m_operatorFormat.setForeground(QColor(128, 0, 128));
    
    QStringList operators;
    operators << "=" << "==" << "!=" << "-eq" << "-ne" << "-lt" << "-gt" << "-le" << "-ge"
             << "-and" << "-or" << "-not" << "|" << "-like" << "-match";
    
    for (const QString& op : operators) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(QRegularExpression::escape(op));
        rule.format = m_operatorFormat;
        m_highlightingRules.append(rule);
    }
    
    // Comment format
    m_commentFormat.setForeground(Qt::darkGreen);
    m_commentFormat.setFontItalic(true);
}

void PowerShellHighlighter::highlightBlock(const QString& text)
{
    // Highlight single-line comments
    {
        QRegularExpression commentExpression("#.*$");
        QRegularExpressionMatch match = commentExpression.match(text);
        while (match.hasMatch()) {
            int startIndex = match.capturedStart();
            int length = match.capturedLength();
            setFormat(startIndex, length, m_commentFormat);
            match = commentExpression.match(text, startIndex + length);
        }
    }
    
    // Highlight multi-line comments (basic)
    {
        QRegularExpression commentStartExpression("<#");
        QRegularExpression commentEndExpression("#>");
        
        setCurrentBlockState(0);
        
        int startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(commentStartExpression);
        
        while (startIndex >= 0) {
            int endIndex = text.indexOf(commentEndExpression, startIndex);
            int commentLength;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + commentEndExpression.pattern().length();
            }
            setFormat(startIndex, commentLength, m_commentFormat);
            startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
        }
    }
    
    // Apply all highlighting rules
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            int startIndex = match.capturedStart();
            int length = match.capturedLength();
            setFormat(startIndex, length, rule.format);
        }
    }
}
