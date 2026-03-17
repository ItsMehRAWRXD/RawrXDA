// RawrXD Agentic IDE - Themed Code Editor Implementation
// Enterprise-grade code editor with theme integration
#include "ThemedCodeEditor.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QTextBlock>
#include <QFontDatabase>
#include <QScrollBar>
#include <QDebug>
#include <chrono>

namespace RawrXD {

// ============================================================
// LineNumberArea Implementation
// ============================================================

LineNumberArea::LineNumberArea(ThemedCodeEditor* editor)
    : QWidget(editor)
    , m_codeEditor(editor) {
}

QSize LineNumberArea::sizeHint() const {
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent* event) {
    m_codeEditor->lineNumberAreaPaintEvent(event);
}

// ============================================================
// ThemedSyntaxHighlighter Implementation
// ============================================================

ThemedSyntaxHighlighter::ThemedSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    updateThemeColors();
    
    // Connect to theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ThemedSyntaxHighlighter::updateThemeColors);
    connect(&ThemeManager::instance(), &ThemeManager::colorsUpdated,
            this, &ThemedSyntaxHighlighter::updateThemeColors);
}

void ThemedSyntaxHighlighter::setLanguage(Language lang) {
    if (m_language == lang) return;
    
    m_language = lang;
    m_highlightingRules.clear();
    
    switch (lang) {
        case Cpp:
            setupCppRules();
            break;
        case Python:
            setupPythonRules();
            break;
        case JavaScript:
        case TypeScript:
            setupJavaScriptRules();
            break;
        case JSON:
            setupJsonRules();
            break;
        case XML:
            setupXmlRules();
            break;
        case Markdown:
            setupMarkdownRules();
            break;
        default:
            break;
    }
    
    rehighlight();
}

void ThemedSyntaxHighlighter::updateThemeColors() {
    const ThemeColors& colors = ThemeManager::instance().currentColors();
    
    m_keywordFormat.setForeground(colors.keywordColor);
    m_keywordFormat.setFontWeight(QFont::Bold);
    
    m_classFormat.setForeground(colors.classColor);
    m_classFormat.setFontWeight(QFont::Bold);
    
    m_singleLineCommentFormat.setForeground(colors.commentColor);
    m_singleLineCommentFormat.setFontItalic(true);
    
    m_multiLineCommentFormat.setForeground(colors.commentColor);
    m_multiLineCommentFormat.setFontItalic(true);
    
    m_quotationFormat.setForeground(colors.stringColor);
    
    m_functionFormat.setForeground(colors.functionColor);
    
    m_numberFormat.setForeground(colors.numberColor);
    
    m_operatorFormat.setForeground(colors.operatorColor);
    
    m_preprocessorFormat.setForeground(colors.preprocessorColor);
    
    // Rebuild rules with new colors
    Language currentLang = m_language;
    m_language = None;
    setLanguage(currentLang);
}

void ThemedSyntaxHighlighter::setupCppRules() {
    HighlightingRule rule;
    
    // C++ keywords
    QStringList keywordPatterns = {
        "\\bchar\\b", "\\bclass\\b", "\\bconst\\b", "\\bdouble\\b",
        "\\benum\\b", "\\bexplicit\\b", "\\bfriend\\b", "\\binline\\b",
        "\\bint\\b", "\\blong\\b", "\\bnamespace\\b", "\\boperator\\b",
        "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b", "\\bshort\\b",
        "\\bsignals\\b", "\\bsigned\\b", "\\bslots\\b", "\\bstatic\\b",
        "\\bstruct\\b", "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b",
        "\\bunion\\b", "\\bunsigned\\b", "\\bvirtual\\b", "\\bvoid\\b",
        "\\bvolatile\\b", "\\bbool\\b", "\\bauto\\b", "\\busing\\b",
        "\\bnew\\b", "\\bdelete\\b", "\\breturn\\b", "\\bif\\b",
        "\\belse\\b", "\\bfor\\b", "\\bwhile\\b", "\\bdo\\b",
        "\\bbreak\\b", "\\bcontinue\\b", "\\bswitch\\b", "\\bcase\\b",
        "\\bdefault\\b", "\\btry\\b", "\\bcatch\\b", "\\bthrow\\b",
        "\\bnullptr\\b", "\\btrue\\b", "\\bfalse\\b", "\\bsizeof\\b",
        "\\btypeid\\b", "\\bstatic_cast\\b", "\\bdynamic_cast\\b",
        "\\breinterpret_cast\\b", "\\bconst_cast\\b", "\\bconstexpr\\b",
        "\\bnoexcept\\b", "\\boverride\\b", "\\bfinal\\b", "\\bdecltype\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    // Class names (Qt-style Q* classes and others)
    rule.pattern = QRegularExpression("\\bQ[A-Za-z]+\\b");
    rule.format = m_classFormat;
    m_highlightingRules.append(rule);
    
    // Class/struct definitions
    rule.pattern = QRegularExpression("\\b(?:class|struct)\\s+([A-Za-z_][A-Za-z0-9_]*)");
    rule.format = m_classFormat;
    m_highlightingRules.append(rule);
    
    // Function calls
    rule.pattern = QRegularExpression("\\b([A-Za-z_][A-Za-z0-9_]*)(?=\\s*\\()");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Numbers (int, float, hex)
    rule.pattern = QRegularExpression("\\b(?:0x[0-9A-Fa-f]+|\\d+\\.?\\d*(?:[eE][+-]?\\d+)?[fFlLuU]*)\\b");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);
    
    // Preprocessor directives
    rule.pattern = QRegularExpression("^\\s*#[a-zA-Z_]+\\b.*$");
    rule.format = m_preprocessorFormat;
    m_highlightingRules.append(rule);
    
    // Strings
    rule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Character literals
    rule.pattern = QRegularExpression("'(?:[^'\\\\]|\\\\.)*'");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Single-line comments
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = m_singleLineCommentFormat;
    m_highlightingRules.append(rule);
    
    // Multi-line comments
    m_commentStartExpression = QRegularExpression("/\\*");
    m_commentEndExpression = QRegularExpression("\\*/");
}

void ThemedSyntaxHighlighter::setupPythonRules() {
    HighlightingRule rule;
    
    // Python keywords
    QStringList keywordPatterns = {
        "\\band\\b", "\\bas\\b", "\\bassert\\b", "\\basync\\b",
        "\\bawait\\b", "\\bbreak\\b", "\\bclass\\b", "\\bcontinue\\b",
        "\\bdef\\b", "\\bdel\\b", "\\belif\\b", "\\belse\\b",
        "\\bexcept\\b", "\\bFalse\\b", "\\bfinally\\b", "\\bfor\\b",
        "\\bfrom\\b", "\\bglobal\\b", "\\bif\\b", "\\bimport\\b",
        "\\bin\\b", "\\bis\\b", "\\blambda\\b", "\\bNone\\b",
        "\\bnonlocal\\b", "\\bnot\\b", "\\bor\\b", "\\bpass\\b",
        "\\braise\\b", "\\breturn\\b", "\\bTrue\\b", "\\btry\\b",
        "\\bwhile\\b", "\\bwith\\b", "\\byield\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    // Built-in functions
    QStringList builtinPatterns = {
        "\\bprint\\b", "\\blen\\b", "\\brange\\b", "\\bstr\\b",
        "\\bint\\b", "\\bfloat\\b", "\\blist\\b", "\\bdict\\b",
        "\\bset\\b", "\\btuple\\b", "\\btype\\b", "\\bisinstance\\b",
        "\\bopen\\b", "\\binput\\b", "\\bmap\\b", "\\bfilter\\b"
    };
    
    for (const QString& pattern : builtinPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }
    
    // Class definitions
    rule.pattern = QRegularExpression("\\bclass\\s+([A-Za-z_][A-Za-z0-9_]*)");
    rule.format = m_classFormat;
    m_highlightingRules.append(rule);
    
    // Function definitions
    rule.pattern = QRegularExpression("\\bdef\\s+([A-Za-z_][A-Za-z0-9_]*)");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Numbers
    rule.pattern = QRegularExpression("\\b(?:0x[0-9A-Fa-f]+|0b[01]+|0o[0-7]+|\\d+\\.?\\d*(?:[eE][+-]?\\d+)?j?)\\b");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);
    
    // Decorators
    rule.pattern = QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*");
    rule.format = m_preprocessorFormat;
    m_highlightingRules.append(rule);
    
    // Triple-quoted strings
    rule.pattern = QRegularExpression("\"\"\".*\"\"\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    rule.pattern = QRegularExpression("'''.*'''");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Single/double quoted strings
    rule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    rule.pattern = QRegularExpression("'(?:[^'\\\\]|\\\\.)*'");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Comments
    rule.pattern = QRegularExpression("#[^\n]*");
    rule.format = m_singleLineCommentFormat;
    m_highlightingRules.append(rule);
}

void ThemedSyntaxHighlighter::setupJavaScriptRules() {
    HighlightingRule rule;
    
    // JavaScript keywords
    QStringList keywordPatterns = {
        "\\bbreak\\b", "\\bcase\\b", "\\bcatch\\b", "\\bcontinue\\b",
        "\\bdebugger\\b", "\\bdefault\\b", "\\bdelete\\b", "\\bdo\\b",
        "\\belse\\b", "\\bfinally\\b", "\\bfor\\b", "\\bfunction\\b",
        "\\bif\\b", "\\bin\\b", "\\binstanceof\\b", "\\bnew\\b",
        "\\breturn\\b", "\\bswitch\\b", "\\bthis\\b", "\\bthrow\\b",
        "\\btry\\b", "\\btypeof\\b", "\\bvar\\b", "\\bvoid\\b",
        "\\bwhile\\b", "\\bwith\\b", "\\bclass\\b", "\\bconst\\b",
        "\\benum\\b", "\\bexport\\b", "\\bextends\\b", "\\bimport\\b",
        "\\bsuper\\b", "\\bimplements\\b", "\\binterface\\b", "\\blet\\b",
        "\\bpackage\\b", "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b",
        "\\bstatic\\b", "\\byield\\b", "\\basync\\b", "\\bawait\\b",
        "\\btrue\\b", "\\bfalse\\b", "\\bnull\\b", "\\bundefined\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }
    
    // Class definitions
    rule.pattern = QRegularExpression("\\bclass\\s+([A-Za-z_$][A-Za-z0-9_$]*)");
    rule.format = m_classFormat;
    m_highlightingRules.append(rule);
    
    // Function definitions
    rule.pattern = QRegularExpression("\\bfunction\\s+([A-Za-z_$][A-Za-z0-9_$]*)");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Arrow functions and method calls
    rule.pattern = QRegularExpression("\\b([A-Za-z_$][A-Za-z0-9_$]*)(?=\\s*[=:]\\s*(?:async\\s+)?(?:function|\\([^)]*\\)\\s*=>))");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Numbers
    rule.pattern = QRegularExpression("\\b(?:0x[0-9A-Fa-f]+|0b[01]+|0o[0-7]+|\\d+\\.?\\d*(?:[eE][+-]?\\d+)?)\\b");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);
    
    // Strings
    rule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    rule.pattern = QRegularExpression("'(?:[^'\\\\]|\\\\.)*'");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Template literals
    rule.pattern = QRegularExpression("`(?:[^`\\\\]|\\\\.)*`");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Single-line comments
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = m_singleLineCommentFormat;
    m_highlightingRules.append(rule);
    
    // Multi-line comments
    m_commentStartExpression = QRegularExpression("/\\*");
    m_commentEndExpression = QRegularExpression("\\*/");
}

void ThemedSyntaxHighlighter::setupJsonRules() {
    HighlightingRule rule;
    
    // Property names
    rule.pattern = QRegularExpression("\"[^\"]*\"(?=\\s*:)");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // String values
    rule.pattern = QRegularExpression(":\\s*\"[^\"]*\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Numbers
    rule.pattern = QRegularExpression(":\\s*-?\\d+\\.?\\d*(?:[eE][+-]?\\d+)?");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);
    
    // Boolean and null
    rule.pattern = QRegularExpression("\\b(?:true|false|null)\\b");
    rule.format = m_keywordFormat;
    m_highlightingRules.append(rule);
}

void ThemedSyntaxHighlighter::setupXmlRules() {
    HighlightingRule rule;
    
    // XML tags
    rule.pattern = QRegularExpression("</?[A-Za-z_][A-Za-z0-9_:-]*");
    rule.format = m_keywordFormat;
    m_highlightingRules.append(rule);
    
    // Attributes
    rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_:-]*(?=\\s*=)");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Attribute values
    rule.pattern = QRegularExpression("\"[^\"]*\"");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Comments
    rule.pattern = QRegularExpression("<!--.*-->");
    rule.format = m_singleLineCommentFormat;
    m_highlightingRules.append(rule);
}

void ThemedSyntaxHighlighter::setupMarkdownRules() {
    HighlightingRule rule;
    
    // Headers
    rule.pattern = QRegularExpression("^#{1,6}\\s.*$");
    rule.format = m_keywordFormat;
    m_highlightingRules.append(rule);
    
    // Bold
    rule.pattern = QRegularExpression("\\*\\*[^*]+\\*\\*");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);
    
    // Italic
    rule.pattern = QRegularExpression("\\*[^*]+\\*");
    rule.format = m_classFormat;
    m_highlightingRules.append(rule);
    
    // Code blocks
    rule.pattern = QRegularExpression("`[^`]+`");
    rule.format = m_quotationFormat;
    m_highlightingRules.append(rule);
    
    // Links
    rule.pattern = QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);
}

void ThemedSyntaxHighlighter::highlightBlock(const QString& text) {
    // Apply single-line rules
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    
    // Handle multi-line comments (C++/JS style)
    if (!m_commentStartExpression.pattern().isEmpty()) {
        setCurrentBlockState(0);
        
        int startIndex = 0;
        if (previousBlockState() != 1) {
            startIndex = text.indexOf(m_commentStartExpression);
        }
        
        while (startIndex >= 0) {
            QRegularExpressionMatch endMatch;
            int endIndex = text.indexOf(m_commentEndExpression, startIndex, &endMatch);
            int commentLength;
            
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }
            
            setFormat(startIndex, commentLength, m_multiLineCommentFormat);
            startIndex = text.indexOf(m_commentStartExpression, startIndex + commentLength);
        }
    }
}

// ============================================================
// ThemedCodeEditor Implementation
// ============================================================

ThemedCodeEditor::ThemedCodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lineNumberArea(new LineNumberArea(this))
    , m_highlighter(new ThemedSyntaxHighlighter(document())) {
    
    qDebug() << "[ThemedCodeEditor] Initializing...";
    
    setupEditor();
    applyTheme();
    
    // Connect signals
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &ThemedCodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &ThemedCodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &ThemedCodeEditor::highlightCurrentLine);
    
    // Connect to theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ThemedCodeEditor::applyTheme);
    connect(&ThemeManager::instance(), &ThemeManager::colorsUpdated,
            this, &ThemedCodeEditor::applyTheme);
    
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    
    qDebug() << "[ThemedCodeEditor] Initialized";
}

void ThemedCodeEditor::setupEditor() {
    // Set monospace font
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    
    // Tab settings
    QFontMetrics metrics(font);
    setTabStopDistance(4 * metrics.horizontalAdvance(' '));
    
    // Line wrap
    setLineWrapMode(QPlainTextEdit::NoWrap);
    
    // Cursor
    setCursorWidth(2);
}

void ThemedCodeEditor::applyTheme() {
    qDebug() << "[ThemedCodeEditor] Applying theme...";
    auto startTime = std::chrono::steady_clock::now();
    
    const ThemeColors& colors = ThemeManager::instance().currentColors();
    
    // Set editor colors via palette
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, colors.editorBackground);
    palette.setColor(QPalette::Text, colors.editorForeground);
    palette.setColor(QPalette::Highlight, colors.editorSelection);
    palette.setColor(QPalette::HighlightedText, colors.editorForeground);
    setPalette(palette);
    
    // Viewport background
    viewport()->setStyleSheet(QString("background-color: %1;")
        .arg(colors.editorBackground.name()));
    
    // Line number area
    m_lineNumberArea->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            color: %2;
        }
    )")
    .arg(colors.dockBackground.name())
    .arg(colors.editorLineNumbers.name()));
    
    // Scrollbars
    setStyleSheet(QString(R"(
        QPlainTextEdit {
            selection-background-color: %1;
            selection-color: %2;
        }
        QScrollBar:vertical {
            background-color: %3;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background-color: %4;
            border-radius: 6px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: %5;
        }
        QScrollBar:horizontal {
            background-color: %3;
            height: 12px;
        }
        QScrollBar::handle:horizontal {
            background-color: %4;
            border-radius: 6px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: %5;
        }
    )")
    .arg(colors.editorSelection.name())
    .arg(colors.editorForeground.name())
    .arg(colors.dockBackground.name())
    .arg(colors.editorLineNumbers.name())
    .arg(colors.editorLineNumbers.lighter(130).name()));
    
    // Update line number area and current line highlight
    m_lineNumberArea->update();
    highlightCurrentLine();
    
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qDebug() << "[ThemedCodeEditor] Theme applied in" << durationMs << "ms";
}

void ThemedCodeEditor::setSyntaxLanguage(ThemedSyntaxHighlighter::Language lang) {
    m_highlighter->setLanguage(lang);
}

ThemedSyntaxHighlighter::Language ThemedCodeEditor::syntaxLanguage() const {
    return m_highlighter->language();
}

int ThemedCodeEditor::lineNumberAreaWidth() const {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return qMax(50, space);
}

void ThemedCodeEditor::updateLineNumberAreaWidth(int) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ThemedCodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }
    
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void ThemedCodeEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ThemedCodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;
    
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        
        QColor lineColor = ThemeManager::instance().currentColors().editorCurrentLine;
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    
    setExtraSelections(extraSelections);
}

void ThemedCodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(m_lineNumberArea);
    
    const ThemeColors& colors = ThemeManager::instance().currentColors();
    painter.fillRect(event->rect(), colors.dockBackground);
    
    // Draw separator line
    painter.setPen(colors.dockBorder);
    painter.drawLine(m_lineNumberArea->width() - 1, event->rect().top(),
                     m_lineNumberArea->width() - 1, event->rect().bottom());
    
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    
    QFont font = painter.font();
    painter.setFont(font);
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            
            // Highlight current line number
            if (block == textCursor().block()) {
                painter.setPen(colors.editorForeground);
            } else {
                painter.setPen(colors.editorLineNumbers);
            }
            
            painter.drawText(0, top, m_lineNumberArea->width() - 8, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }
        
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void ThemedCodeEditor::keyPressEvent(QKeyEvent* event) {
    // Auto-indent on Enter
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QString currentLine = textCursor().block().text();
        QString indent = getIndentation(currentLine);
        
        // Add extra indent after { or :
        if (currentLine.trimmed().endsWith('{') || currentLine.trimmed().endsWith(':')) {
            indent += "    ";
        }
        
        QPlainTextEdit::keyPressEvent(event);
        insertPlainText(indent);
        return;
    }
    
    // Tab handling
    if (event->key() == Qt::Key_Tab) {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) {
            // Indent selected lines
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();
            
            cursor.setPosition(start);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.setPosition(end, QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            
            QString text = cursor.selectedText();
            QStringList lines = text.split(QChar::ParagraphSeparator);
            
            for (int i = 0; i < lines.size(); ++i) {
                lines[i] = "    " + lines[i];
            }
            
            cursor.insertText(lines.join('\n'));
        } else {
            insertPlainText("    ");
        }
        return;
    }
    
    // Backtab (shift+tab) for unindent
    if (event->key() == Qt::Key_Backtab) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
        
        if (cursor.selectedText() == "    ") {
            cursor.removeSelectedText();
        }
        return;
    }
    
    QPlainTextEdit::keyPressEvent(event);
}

QString ThemedCodeEditor::getIndentation(const QString& line) const {
    QString indent;
    for (const QChar& ch : line) {
        if (ch == ' ' || ch == '\t') {
            indent += ch;
        } else {
            break;
        }
    }
    return indent;
}

} // namespace RawrXD
