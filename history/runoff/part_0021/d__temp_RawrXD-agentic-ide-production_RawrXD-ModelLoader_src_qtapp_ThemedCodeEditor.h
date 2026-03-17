// RawrXD Agentic IDE - Themed Code Editor
// Code editor with theme-aware syntax highlighting
#pragma once

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>
#include <QWidget>

namespace RawrXD {

class ThemedCodeEditor;

/**
 * @brief LineNumberArea - Widget for displaying line numbers
 */
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(ThemedCodeEditor* editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ThemedCodeEditor* m_codeEditor;
};

/**
 * @brief ThemedSyntaxHighlighter - Syntax highlighter with theme support
 * 
 * Supports C++, Python, JavaScript, and other common languages.
 * Colors are automatically updated when theme changes.
 */
class ThemedSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    
public:
    explicit ThemedSyntaxHighlighter(QTextDocument* parent = nullptr);
    
    enum Language {
        None,
        Cpp,
        Python,
        JavaScript,
        TypeScript,
        JSON,
        XML,
        Markdown
    };
    
    void setLanguage(Language lang);
    Language language() const { return m_language; }
    
public slots:
    void updateThemeColors();
    
protected:
    void highlightBlock(const QString& text) override;
    
private:
    void setupCppRules();
    void setupPythonRules();
    void setupJavaScriptRules();
    void setupJsonRules();
    void setupXmlRules();
    void setupMarkdownRules();
    
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    
    QVector<HighlightingRule> m_highlightingRules;
    Language m_language = None;
    
    // Formats (updated from theme)
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_classFormat;
    QTextCharFormat m_singleLineCommentFormat;
    QTextCharFormat m_multiLineCommentFormat;
    QTextCharFormat m_quotationFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_operatorFormat;
    QTextCharFormat m_preprocessorFormat;
    
    // Multi-line comment handling
    QRegularExpression m_commentStartExpression;
    QRegularExpression m_commentEndExpression;
};

/**
 * @brief ThemedCodeEditor - Code editor with theme support
 * 
 * Features:
 * - Line number display
 * - Current line highlighting
 * - Theme-aware colors
 * - Syntax highlighting
 * - Auto-indent support
 */
class ThemedCodeEditor : public QPlainTextEdit {
    Q_OBJECT
    
public:
    explicit ThemedCodeEditor(QWidget* parent = nullptr);
    ~ThemedCodeEditor() override = default;
    
    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth() const;
    
    void setSyntaxLanguage(ThemedSyntaxHighlighter::Language lang);
    ThemedSyntaxHighlighter::Language syntaxLanguage() const;
    
public slots:
    void applyTheme();
    
protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    
private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);
    
private:
    void setupEditor();
    QString getIndentation(const QString& line) const;
    
    LineNumberArea* m_lineNumberArea;
    ThemedSyntaxHighlighter* m_highlighter;
};

} // namespace RawrXD
