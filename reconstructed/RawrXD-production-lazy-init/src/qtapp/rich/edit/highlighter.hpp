#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QString>

/**
 * @brief RichEditHighlighter - Qt/C++ reference implementation
 * 
 * Provides syntax highlighting for code in a QTextEdit control.
 * Features:
 * - Keyword highlighting (language-agnostic)
 * - Severity level coloring (ERROR, WARN, INFO, DEBUG)
 * - String/comment detection
 * - Performance-optimized for incremental updates
 * - Selectable language presets (C++, Python, MASM, etc.)
 * 
 * This is the Qt reference version—will be ported to Win32 RichEdit later.
 */
class RichEditHighlighter : public QWidget {
    Q_OBJECT

public:
    enum SyntaxLanguage {
        LanguagePlainText,
        LanguageCPP,
        LanguagePython,
        LanguageMASM,
        LanguageJSON,
        LanguageXML
    };

    explicit RichEditHighlighter(QWidget* parent = nullptr);
    ~RichEditHighlighter() override;

    // Set content and language
    void setText(const QString& text);
    void setLanguage(SyntaxLanguage lang);
    QString getText() const;

    // Highlight control
    void highlightAll();
    void highlightLine(int lineNumber);
    void clearHighlighting();

    // Color scheme customization
    void setKeywordColor(const QColor& color);
    void setStringColor(const QColor& color);
    void setCommentColor(const QColor& color);
    void setErrorColor(const QColor& color);
    void setWarningColor(const QColor& color);

signals:
    void contentChanged();

private slots:
    void onLanguageChanged(int index);
    void onTextChanged();
    void onApplyHighlighting();

private:
    void setupUI();
    void applyKeywordHighlighting(const QString& text);
    void applyLineHighlighting(const QString& text);
    void highlightSeverities(const QString& text);
    
    QStringList getKeywordsForLanguage(SyntaxLanguage lang) const;
    
    // UI Components
    QTextEdit* m_editor;
    QComboBox* m_languageCombo;
    QPushButton* m_highlightButton;
    QPushButton* m_clearButton;
    
    // State
    SyntaxLanguage m_currentLanguage;
    
    // Colors
    QColor m_keywordColor;
    QColor m_stringColor;
    QColor m_commentColor;
    QColor m_errorColor;
    QColor m_warningColor;
};
