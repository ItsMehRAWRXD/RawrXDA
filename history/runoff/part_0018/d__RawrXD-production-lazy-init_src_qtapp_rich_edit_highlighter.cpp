#include "rich_edit_highlighter.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QLabel>

RichEditHighlighter::RichEditHighlighter(QWidget* parent)
    : QWidget(parent)
    , m_currentLanguage(LanguagePlainText)
    , m_keywordColor(0, 0, 255)         // Blue
    , m_stringColor(255, 0, 0)          // Red
    , m_commentColor(0, 128, 0)         // Green
    , m_errorColor(255, 0, 0)           // Red
    , m_warningColor(255, 165, 0)       // Orange
{
    setupUI();
}

RichEditHighlighter::~RichEditHighlighter() = default;

void RichEditHighlighter::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Toolbar
    auto toolbarLayout = new QHBoxLayout();
    
    auto langLabel = new QLabel("Language:");
    toolbarLayout->addWidget(langLabel);
    
    m_languageCombo = new QComboBox();
    m_languageCombo->addItem("Plain Text", LanguagePlainText);
    m_languageCombo->addItem("C++", LanguageCPP);
    m_languageCombo->addItem("Python", LanguagePython);
    m_languageCombo->addItem("MASM x64", LanguageMASM);
    m_languageCombo->addItem("JSON", LanguageJSON);
    m_languageCombo->addItem("XML", LanguageXML);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RichEditHighlighter::onLanguageChanged);
    toolbarLayout->addWidget(m_languageCombo);
    
    toolbarLayout->addSpacing(16);
    
    m_highlightButton = new QPushButton("✨ Highlight All");
    connect(m_highlightButton, &QPushButton::clicked, this, &RichEditHighlighter::onApplyHighlighting);
    toolbarLayout->addWidget(m_highlightButton);
    
    m_clearButton = new QPushButton("✕ Clear");
    connect(m_clearButton, &QPushButton::clicked, this, &RichEditHighlighter::clearHighlighting);
    toolbarLayout->addWidget(m_clearButton);
    
    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // Editor
    m_editor = new QTextEdit();
    m_editor->setPlaceholderText("Paste code here for syntax highlighting...");
    m_editor->setFont(QFont("Courier New", 10));
    connect(m_editor, &QTextEdit::textChanged, this, &RichEditHighlighter::onTextChanged);
    mainLayout->addWidget(m_editor);
    
    setLayout(mainLayout);
}

void RichEditHighlighter::setText(const QString& text) {
    m_editor->setPlainText(text);
}

void RichEditHighlighter::setLanguage(SyntaxLanguage lang) {
    m_currentLanguage = lang;
    m_languageCombo->setCurrentIndex(static_cast<int>(lang));
}

QString RichEditHighlighter::getText() const {
    return m_editor->toPlainText();
}

void RichEditHighlighter::highlightAll() {
    const QString& text = m_editor->toPlainText();
    applyKeywordHighlighting(text);
    highlightSeverities(text);
}

void RichEditHighlighter::highlightLine(int lineNumber) {
    QTextDocument* doc = m_editor->document();
    QTextBlock block = doc->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) return;
    
    const QString& lineText = block.text();
    // Apply highlighting to this line only
    applyLineHighlighting(lineText);
}

void RichEditHighlighter::clearHighlighting() {
    QTextDocument* doc = m_editor->document();
    QTextCursor cursor(doc);
    cursor.select(QTextCursor::Document);
    
    QTextCharFormat fmt;
    fmt.setForeground(QColor(0, 0, 0)); // Black
    cursor.setCharFormat(fmt);
}

void RichEditHighlighter::setKeywordColor(const QColor& color) {
    m_keywordColor = color;
}

void RichEditHighlighter::setStringColor(const QColor& color) {
    m_stringColor = color;
}

void RichEditHighlighter::setCommentColor(const QColor& color) {
    m_commentColor = color;
}

void RichEditHighlighter::setErrorColor(const QColor& color) {
    m_errorColor = color;
}

void RichEditHighlighter::setWarningColor(const QColor& color) {
    m_warningColor = color;
}

void RichEditHighlighter::onLanguageChanged(int index) {
    m_currentLanguage = static_cast<SyntaxLanguage>(m_languageCombo->itemData(index).toInt());
    highlightAll();
}

void RichEditHighlighter::onTextChanged() {
    // Incremental highlighting could be done here
    // For now, highlight on demand via button
}

void RichEditHighlighter::onApplyHighlighting() {
    highlightAll();
}

void RichEditHighlighter::applyKeywordHighlighting(const QString& text) {
    QStringList keywords = getKeywordsForLanguage(m_currentLanguage);
    if (keywords.isEmpty()) return;
    
    QTextDocument* doc = m_editor->document();
    QTextCursor cursor(doc);
    
    // Clear previous highlighting
    cursor.select(QTextCursor::Document);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(0, 0, 0));
    cursor.setCharFormat(fmt);
    
    // Apply keyword highlighting
    cursor.movePosition(QTextCursor::Start);
    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(m_keywordColor);
    keywordFmt.setFontWeight(QFont::Bold);
    
    for (const QString& keyword : keywords) {
        cursor = doc->find(keyword);
        while (!cursor.isNull()) {
            cursor.setCharFormat(keywordFmt);
            cursor = doc->find(keyword, cursor);
        }
    }
    
    // Highlight strings (simple pattern: text in quotes)
    QTextCharFormat stringFmt;
    stringFmt.setForeground(m_stringColor);
    cursor = doc->find("\"");
    while (!cursor.isNull()) {
        int start = cursor.position();
        cursor = doc->find("\"", cursor);
        if (!cursor.isNull()) {
            int end = cursor.position();
            cursor.setPosition(start);
            cursor.setPosition(end + 1, QTextCursor::KeepAnchor);
            cursor.setCharFormat(stringFmt);
        }
        cursor = doc->find("\"", cursor);
    }
    
    // Highlight comments (simple pattern: // to end of line)
    QTextCharFormat commentFmt;
    commentFmt.setForeground(m_commentColor);
    cursor = doc->find("//");
    while (!cursor.isNull()) {
        int start = cursor.position() - 2; // Back to //
        QTextBlock block = doc->findBlock(start);
        int end = block.position() + block.length();
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        cursor.setCharFormat(commentFmt);
        cursor = doc->find("//", cursor);
    }
}

void RichEditHighlighter::applyLineHighlighting(const QString& text) {
    // Simple implementation: highlight a single line
    // In practice, would use incremental parsing
    QTextDocument* doc = m_editor->document();
    QTextCursor cursor(doc);
    cursor.select(QTextCursor::LineUnderCursor);
    
    QTextCharFormat fmt;
    fmt.setBackground(QColor(240, 240, 240));
    cursor.setCharFormat(fmt);
}

void RichEditHighlighter::highlightSeverities(const QString& text) {
    // Highlight ERROR, WARNING, DEBUG lines
    QTextDocument* doc = m_editor->document();
    
    // Highlight ERROR lines
    QTextCursor cursor = doc->find("ERROR");
    QTextCharFormat errorFmt;
    errorFmt.setForeground(m_errorColor);
    errorFmt.setFontWeight(QFont::Bold);
    while (!cursor.isNull()) {
        cursor.setCharFormat(errorFmt);
        cursor = doc->find("ERROR", cursor);
    }
    
    // Highlight WARN lines
    cursor = doc->find("WARN");
    QTextCharFormat warnFmt;
    warnFmt.setForeground(m_warningColor);
    while (!cursor.isNull()) {
        cursor.setCharFormat(warnFmt);
        cursor = doc->find("WARN", cursor);
    }
    
    // Highlight DEBUG lines
    cursor = doc->find("DEBUG");
    QTextCharFormat debugFmt;
    debugFmt.setForeground(QColor(128, 128, 255)); // Light blue
    while (!cursor.isNull()) {
        cursor.setCharFormat(debugFmt);
        cursor = doc->find("DEBUG", cursor);
    }
}

QStringList RichEditHighlighter::getKeywordsForLanguage(SyntaxLanguage lang) const {
    switch (lang) {
        case LanguageCPP:
            return {
                "void", "int", "float", "double", "char", "bool", "auto",
                "const", "static", "inline", "virtual", "class", "struct", "enum",
                "if", "else", "for", "while", "switch", "case", "return",
                "new", "delete", "nullptr", "true", "false", "public", "private", "protected"
            };
        
        case LanguagePython:
            return {
                "def", "class", "if", "else", "elif", "for", "while", "return",
                "import", "from", "as", "try", "except", "finally", "with",
                "True", "False", "None", "and", "or", "not", "in", "is"
            };
        
        case LanguageMASM:
            return {
                "mov", "add", "sub", "mul", "div", "xor", "and", "or", "not",
                "jmp", "je", "jne", "ja", "jb", "jz", "jnz", "call", "ret",
                "push", "pop", "lea", "cmp", "test", "inc", "dec",
                "proc", "endp", "public", "extern", "align", "db", "dw", "dd", "dq"
            };
        
        case LanguageJSON:
            return { "true", "false", "null" };
        
        default:
            return {};
    }
}
