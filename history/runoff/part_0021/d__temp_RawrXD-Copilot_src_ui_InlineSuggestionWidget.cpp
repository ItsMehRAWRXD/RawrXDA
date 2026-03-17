#include "InlineSuggestionWidget.h"
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeyEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QPainter>
#include <QStyleOption>
#include <QDebug>

InlineSuggestionWidget::InlineSuggestionWidget(QWidget *parent)
    : QWidget(parent)
    , m_textEdit(new QTextEdit(this))
    , m_fadeAnimation(new QPropertyAnimation(this, "windowOpacity"))
    , m_slideAnimation(nullptr)
    , m_currentIndex(0)
    , m_opacity(0.8)
    , m_theme("default")
    , m_animationEnabled(true)
    , m_acceptTimer(new QTimer(this))
{
    setupUI();
    
    // Setup shortcuts
    QShortcut* acceptShortcut = new QShortcut(QKeySequence(Qt::Key_Tab), this);
    connect(acceptShortcut, &QShortcut::activated, this, &InlineSuggestionWidget::onAcceptShortcut);
    
    QShortcut* rejectShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(rejectShortcut, &QShortcut::activated, this, &InlineSuggestionWidget::onRejectShortcut);
    
    QShortcut* cycleNextShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Period), this);
    connect(cycleNextShortcut, &QShortcut::activated, this, &InlineSuggestionWidget::onCycleNextShortcut);
    
    QShortcut* cyclePrevShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Period), this);
    connect(cyclePrevShortcut, &QShortcut::activated, this, &InlineSuggestionWidget::onCyclePreviousShortcut);
    
    // Setup animation
    m_fadeAnimation->setDuration(150);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &InlineSuggestionWidget::onAnimationFinished);
    
    // Setup accept timer to prevent accidental accepts
    m_acceptTimer->setSingleShot(true);
    m_acceptTimer->setInterval(200); // 200ms delay
}

InlineSuggestionWidget::~InlineSuggestionWidget()
{
}

void InlineSuggestionWidget::showSuggestion(const QJsonObject& suggestion, QWidget* editor, int line, int column)
{
    if (!editor) return;
    
    m_suggestions.clear();
    m_suggestions.append(suggestion);
    m_currentIndex = 0;
    
    QString text = formatSuggestionText(suggestion);
    m_textEdit->setPlainText(text);
    
    updatePosition(editor, line, column);
    
    if (m_animationEnabled) {
        startFadeIn();
    } else {
        setWindowOpacity(m_opacity);
        show();
    }
}

void InlineSuggestionWidget::hideSuggestion()
{
    if (m_animationEnabled) {
        startFadeOut();
    } else {
        hide();
    }
}

void InlineSuggestionWidget::updatePosition(QWidget* editor, int line, int column)
{
    if (!editor) return;
    
    QPoint pos = calculatePosition(editor, line, column);
    move(pos);
    
    // Adjust size to fit content
    QSize textSize = m_textEdit->document()->size().toSize();
    resize(qMin(textSize.width() + 20, 800), qMin(textSize.height() + 20, 200));
}

void InlineSuggestionWidget::setOpacity(double opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
    if (!m_animationEnabled) {
        setWindowOpacity(m_opacity);
    }
}

void InlineSuggestionWidget::setTheme(const QString& theme)
{
    m_theme = theme;
    applyTheme();
}

void InlineSuggestionWidget::enableAnimation(bool enable)
{
    m_animationEnabled = enable;
}

void InlineSuggestionWidget::cycleToNext()
{
    if (m_suggestions.isEmpty()) return;
    
    m_currentIndex = (m_currentIndex + 1) % m_suggestions.size();
    QString text = formatSuggestionText(m_suggestions[m_currentIndex]);
    m_textEdit->setPlainText(text);
    
    emit suggestionCycled(m_currentIndex);
}

void InlineSuggestionWidget::cycleToPrevious()
{
    if (m_suggestions.isEmpty()) return;
    
    m_currentIndex = (m_currentIndex - 1 + m_suggestions.size()) % m_suggestions.size();
    QString text = formatSuggestionText(m_suggestions[m_currentIndex]);
    m_textEdit->setPlainText(text);
    
    emit suggestionCycled(m_currentIndex);
}

void InlineSuggestionWidget::onAnimationFinished()
{
    if (m_fadeAnimation->endValue().toDouble() == 0.0) {
        hide();
    }
}

void InlineSuggestionWidget::onAcceptShortcut()
{
    // Prevent accidental accepts with a timer
    if (!m_acceptTimer->isActive() && !m_suggestions.isEmpty()) {
        m_acceptTimer->start();
        emit suggestionAccepted(m_suggestions[m_currentIndex]);
        hideSuggestion();
    }
}

void InlineSuggestionWidget::onRejectShortcut()
{
    emit suggestionRejected();
    hideSuggestion();
}

void InlineSuggestionWidget::onCycleNextShortcut()
{
    cycleToNext();
}

void InlineSuggestionWidget::onCyclePreviousShortcut()
{
    cycleToPrevious();
}

void InlineSuggestionWidget::setupUI()
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");
    
    m_textEdit->setReadOnly(true);
    m_textEdit->setFrameStyle(QFrame::NoFrame);
    m_textEdit->setStyleSheet(
        "QTextEdit {"
        "   background: #2d2d30;"
        "   color: #cccccc;"
        "   border: 1px solid #3f3f46;"
        "   border-radius: 4px;"
        "   font-family: 'Consolas', 'Monaco', monospace;"
        "   font-size: 12px;"
        "}"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_textEdit);
    
    setFixedSize(400, 100);
    hide();
}

void InlineSuggestionWidget::applyTheme()
{
    // Apply theme-specific styling
    if (m_theme == "dark") {
        m_textEdit->setStyleSheet(
            "QTextEdit {"
            "   background: #1e1e1e;"
            "   color: #d4d4d4;"
            "   border: 1px solid #3c3c3c;"
            "   border-radius: 4px;"
            "   font-family: 'Consolas', 'Monaco', monospace;"
            "   font-size: 12px;"
            "}"
        );
    } else if (m_theme == "light") {
        m_textEdit->setStyleSheet(
            "QTextEdit {"
            "   background: #ffffff;"
            "   color: #333333;"
            "   border: 1px solid #cccccc;"
            "   border-radius: 4px;"
            "   font-family: 'Consolas', 'Monaco', monospace;"
            "   font-size: 12px;"
            "}"
        );
    }
}

void InlineSuggestionWidget::startFadeIn()
{
    show();
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(m_opacity);
    m_fadeAnimation->start();
}

void InlineSuggestionWidget::startFadeOut()
{
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->start();
}

QString InlineSuggestionWidget::formatSuggestionText(const QJsonObject& suggestion)
{
    QString text = suggestion["text"].toString();
    
    // Limit length for display
    if (text.length() > 500) {
        text = text.left(500) + "...";
    }
    
    return text;
}

QColor InlineSuggestionWidget::getSyntaxColor(const QString& token, const QString& language)
{
    // Simple syntax coloring based on token type
    if (token.startsWith("\"") || token.startsWith("'")) {
        return QColor("#ce9178"); // String color
    }
    if (token.contains(QRegExp("^\\d+"))) {
        return QColor("#b5cea8"); // Number color
    }
    if (token == "if" || token == "else" || token == "for" || token == "while" || 
        token == "function" || token == "class" || token == "return") {
        return QColor("#569cd6"); // Keyword color
    }
    if (token == "true" || token == "false" || token == "null" || token == "undefined") {
        return QColor("#569cd6"); // Literal color
    }
    
    return QColor("#d4d4d4"); // Default color
}

QPoint InlineSuggestionWidget::calculatePosition(QWidget* editor, int line, int column)
{
    // This is a simplified implementation
    // In a real editor, you would use the editor's API to get the cursor position
    QPoint editorPos = editor->mapToGlobal(QPoint(0, 0));
    
    // Estimate position based on line/column (this would be more precise in a real implementation)
    int x = editorPos.x() + column * 8; // Approximate character width
    int y = editorPos.y() + line * 20;  // Approximate line height
    
    return QPoint(x, y);
}
