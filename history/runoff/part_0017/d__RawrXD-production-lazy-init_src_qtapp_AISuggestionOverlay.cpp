/**
 * @file AISuggestionOverlay.cpp
 * @brief Complete AI Suggestion Overlay for RawrXD Agentic IDE
 * 
 * Provides ghost text and inline suggestion rendering for AI-powered code assistance:
 * - Ghost text display for code completion suggestions
 * - Inline code refactoring suggestions
 * - Smooth text animations and positioning
 * - Keyboard navigation and acceptance
 * - Multiple suggestion types and sources
 * - Customizable styling and themes
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "AISuggestionOverlay.h"
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextLine>
#include <QPainter>
#include <QStyleOption>
#include <QApplication>
#include <QFontMetrics>
#include <QScreen>
#include <QToolTip>
#include <QKeySequence>
#include <QShortcut>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QPalette>
#include <QBrush>
#include <QPixmap>
#include <QPaintEngine>
#include <QFontInfo>
#include <QStyleHints>
#include <QWindow>
#include <QDesktopWidget>
#include <QScrollBar>
#include <algorithm>
#include <functional>

namespace RawrXD {

// ============================================================================
// AISuggestion Implementation
// ============================================================================

AISuggestion::AISuggestion()
    : m_type(SuggestionType::Completion)
    , m_confidence(SuggestionConfidence::Medium)
    , m_state(SuggestionState::Pending)
    , m_position(0)
    , m_length(0)
    , m_opacity(1.0)
    , m_visible(true)
{
}

AISuggestion::AISuggestion(const QString& id, SuggestionType type, const QString& originalText, const QString& suggestedText)
    : m_id(id)
    , m_type(type)
    , m_originalText(originalText)
    , m_suggestedText(suggestedText)
    , m_confidence(SuggestionConfidence::Medium)
    , m_state(SuggestionState::Pending)
    , m_position(0)
    , m_length(0)
    , m_opacity(1.0)
    , m_visible(true)
{
}

bool AISuggestion::canAccept() const
{
    return m_state == SuggestionState::Displayed && m_visible;
}

bool AISuggestion::canReject() const
{
    return m_state == SuggestionState::Displayed && m_visible;
}

bool AISuggestion::canModify() const
{
    return m_state == SuggestionState::Displayed && m_visible && 
           m_type != SuggestionType::Completion;
}

QString AISuggestion::toDisplayString() const
{
    QString prefix;
    switch (m_type) {
        case SuggestionType::Completion:
            prefix = QStringLiteral("🤖 ");
            break;
        case SuggestionType::Refactoring:
            prefix = QStringLiteral("🔧 ");
            break;
        case SuggestionType::Optimization:
            prefix = QStringLiteral("⚡ ");
            break;
        case SuggestionType::Documentation:
            prefix = QStringLiteral("📖 ");
            break;
        case SuggestionType::Test:
            prefix = QStringLiteral("🧪 ");
            break;
        case SuggestionType::Security:
            prefix = QStringLiteral("🔒 ");
            break;
        case SuggestionType::BugFix:
            prefix = QStringLiteral("🐛 ");
            break;
        case SuggestionType::Migration:
            prefix = QStringLiteral("🚀 ");
            break;
        case SuggestionType::Custom:
            prefix = QStringLiteral("✨ ");
            break;
    }
    
    QString confidenceText;
    switch (m_confidence) {
        case SuggestionConfidence::Low:
            confidenceText = QStringLiteral(" (low confidence)");
            break;
        case SuggestionConfidence::Medium:
            confidenceText = QStringLiteral(" (medium confidence)");
            break;
        case SuggestionConfidence::High:
            confidenceText = QStringLiteral(" (high confidence)");
            break;
        case SuggestionConfidence::VeryHigh:
            confidenceText = QStringLiteral(" (very high confidence)");
            break;
    }
    
    return prefix + m_suggestedText + confidenceText;
}

// ============================================================================
// SuggestionStyle Implementation
// ============================================================================

SuggestionStyle::SuggestionStyle()
    : font(QFontDatabase::systemFont(QFontDatabase::FixedFont))
    , fontSize(11)
    , italic(false)
    , bold(false)
    , spacing(1)
    , fadeInDuration(200)
    , fadeOutDuration(300)
    , highlightDuration(500)
    , glow(true)
    , glowColor(255, 255, 255, 128)
    , glowRadius(3)
    , underline(false)
    , strikethrough(false)
{
    // Default colors for different suggestion types
    completionColor = QColor(100, 149, 237);     // Cornflower Blue
    refactoringColor = QColor(34, 139, 34);     // Forest Green
    optimizationColor = QColor(255, 165, 0);     // Orange
    documentationColor = QColor(75, 0, 130);     // Indigo
    testColor = QColor(138, 43, 226);            // Blue Violet
    securityColor = QColor(220, 20, 60);         // Crimson
    bugfixColor = QColor(255, 69, 0);            // Red Orange
    migrationColor = QColor(0, 191, 255);        // Deep Sky Blue
    
    font.setPointSize(fontSize);
    if (bold) font.setBold(true);
    if (italic) font.setItalic(true);
}

// ============================================================================
// ShortcutConfig Implementation
// ============================================================================

ShortcutConfig::ShortcutConfig()
    : accept(QStringLiteral("Tab"))
    , acceptAndNext(QStringLiteral("Shift+Tab"))
    , reject(QStringLiteral("Escape"))
    , rejectAndNext(QStringLiteral("Shift+Escape"))
    , modify(QStringLiteral("Ctrl+M"))
    , cycle(QStringLiteral("Alt+Tab"))
    , showDetails(QStringLiteral("F1"))
{
}

// ============================================================================
// AISuggestionOverlay Implementation
// ============================================================================

AISuggestionOverlay::AISuggestionOverlay(QWidget* parent)
    : QWidget(parent)
    , m_textEditor(nullptr)
    , m_enabled(true)
    , m_maxVisibleSuggestions(10)
    , m_suggestionTimeout(30000) // 30 seconds
    , m_timeoutTimerId(0)
    , m_overlayVisible(false)
    , m_fadeOpacity(1.0)
    , m_fadeAnimation(nullptr)
    , m_animatingSuggestionId()
    , m_style()
    , m_shortcuts()
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);
    
    // Initialize default style and shortcuts
    m_style = SuggestionStyle();
    m_shortcuts = ShortcutConfig();
    
    // Initialize fade animation
    m_fadeAnimation = std::make_unique<QPropertyAnimation>(this, "fadeOpacity");
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fadeAnimation.get(), &QPropertyAnimation::finished, 
            this, &AISuggestionOverlay::onFadeAnimationFinished);
    
    qDebug() << "[AISuggestionOverlay] Initialized with default configuration";
}

AISuggestionOverlay::~AISuggestionOverlay()
{
    if (m_timeoutTimerId != 0) {
        killTimer(m_timeoutTimerId);
    }
    
    qDebug() << "[AISuggestionOverlay] Destroyed";
}

void AISuggestionOverlay::initialize()
{
    if (!m_textEditor) {
        return;
    }
    
    // Connect to text editor signals
    connect(m_textEditor, &QPlainTextEdit::textChanged,
            this, &AISuggestionOverlay::onTextChanged);
    connect(m_textEditor, &QPlainTextEdit::cursorPositionChanged,
            this, &AISuggestionOverlay::onCursorPositionChanged);
    
    // Set up viewport for proper positioning
    setParent(m_textEditor->viewport());
    setGeometry(m_textEditor->viewport()->geometry());
    
    // Install event filter for the text editor
    m_textEditor->viewport()->installEventFilter(this);
    
    qDebug() << "[AISuggestionOverlay] Initialized overlay for text editor";
}

void AISuggestionOverlay::setTextEditor(QPlainTextEdit* editor)
{
    m_textEditor = editor;
    initialize();
}

bool AISuggestionOverlay::addSuggestion(const AISuggestion& suggestion)
{
    if (!validateSuggestion(suggestion)) {
        qWarning() << "[AISuggestionOverlay] Invalid suggestion" << suggestion.id();
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString suggestionId = suggestion.id();
    if (m_suggestions.contains(suggestionId)) {
        qWarning() << "[AISuggestionOverlay] Suggestion already exists" << suggestionId;
        return false;
    }
    
    // Add suggestion
    m_suggestions[suggestionId] = suggestion;
    
    // Update position if text editor is available
    if (m_textEditor && m_textEditor->document()) {
        AISuggestion& addedSuggestion = m_suggestions[suggestionId];
        addedSuggestion.setPosition(getCursorForPosition(m_textEditor->textCursor().position()).position());
        
        // Calculate bounding rectangle
        QRect rect = calculateSuggestionRect(addedSuggestion);
        addedSuggestion.setBoundingRect(rect);
    }
    
    // Add to visible suggestions if within limits
    if (m_visibleSuggestions.size() < m_maxVisibleSuggestions) {
        m_visibleSuggestions.append(suggestionId);
    }
    
    // Start fade in animation
    startFadeAnimation(suggestionId, true);
    
    // Start timeout timer
    if (m_timeoutTimerId == 0) {
        m_timeoutTimerId = startTimer(m_suggestionTimeout);
    }
    
    update();
    
    qDebug() << "[AISuggestionOverlay] Added suggestion" << suggestionId << "of type" << static_cast<int>(suggestion.type());
    
    return true;
}

bool AISuggestionOverlay::removeSuggestion(const QString& suggestionId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_suggestions.contains(suggestionId)) {
        return false;
    }
    
    // Start fade out animation
    startFadeAnimation(suggestionId, false);
    
    // Remove from visible list
    m_visibleSuggestions.removeAll(suggestionId);
    
    // Remove suggestion
    m_suggestions.remove(suggestionId);
    
    update();
    
    qDebug() << "[AISuggestionOverlay] Removed suggestion" << suggestionId;
    
    return true;
}

bool AISuggestionOverlay::updateSuggestion(const QString& suggestionId, const AISuggestion& newSuggestion)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_suggestions.contains(suggestionId)) {
        return false;
    }
    
    if (newSuggestion.id() != suggestionId) {
        qWarning() << "[AISuggestionOverlay] Suggestion ID mismatch in update";
        return false;
    }
    
    // Update suggestion
    m_suggestions[suggestionId] = newSuggestion;
    
    // Update position
    AISuggestion& updatedSuggestion = m_suggestions[suggestionId];
    updatedSuggestion.setPosition(getCursorForPosition(m_textEditor->textCursor().position()).position());
    
    // Recalculate bounding rectangle
    QRect rect = calculateSuggestionRect(updatedSuggestion);
    updatedSuggestion.setBoundingRect(rect);
    
    update();
    
    qDebug() << "[AISuggestionOverlay] Updated suggestion" << suggestionId;
    
    return true;
}

AISuggestion AISuggestionOverlay::getSuggestion(const QString& suggestionId) const
{
    QMutexLocker locker(&m_mutex);
    return m_suggestions.value(suggestionId, AISuggestion());
}

QList<AISuggestion> AISuggestionOverlay::getSuggestions() const
{
    QMutexLocker locker(&m_mutex);
    return m_suggestions.values();
}

QList<AISuggestion> AISuggestionOverlay::getSuggestionsByType(SuggestionType type) const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AISuggestion> result;
    for (const AISuggestion& suggestion : m_suggestions.values()) {
        if (suggestion.type() == type) {
            result.append(suggestion);
        }
    }
    
    return result;
}

QList<AISuggestion> AISuggestionOverlay::getVisibleSuggestions() const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AISuggestion> result;
    for (const QString& suggestionId : m_visibleSuggestions) {
        const AISuggestion& suggestion = m_suggestions.value(suggestionId);
        if (suggestion.isVisible()) {
            result.append(suggestion);
        }
    }
    
    return result;
}

void AISuggestionOverlay::clearSuggestions()
{
    QMutexLocker locker(&m_mutex);
    
    // Fade out all suggestions
    for (const QString& suggestionId : m_suggestions.keys()) {
        startFadeAnimation(suggestionId, false);
    }
    
    // Clear data
    m_suggestions.clear();
    m_visibleSuggestions.clear();
    m_focusedSuggestionId.clear();
    
    // Stop timeout timer
    if (m_timeoutTimerId != 0) {
        killTimer(m_timeoutTimerId);
        m_timeoutTimerId = 0;
    }
    
    update();
    
    qDebug() << "[AISuggestionOverlay] Cleared all suggestions";
}

bool AISuggestionOverlay::acceptSuggestion(const QString& suggestionId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_suggestions.contains(suggestionId)) {
        return false;
    }
    
    AISuggestion& suggestion = m_suggestions[suggestionId];
    
    if (!suggestion.canAccept()) {
        return false;
    }
    
    // Apply suggestion to text
    if (applySuggestion(suggestion)) {
        // Update state
        suggestion.setState(SuggestionState::Accepted);
        
        // Remove from visible suggestions
        m_visibleSuggestions.removeAll(suggestionId);
        
        // Update focused suggestion if this was the focused one
        if (m_focusedSuggestionId == suggestionId) {
            m_focusedSuggestionId.clear();
        }
        
        // Emit signal
        emit suggestionAccepted(suggestionId, suggestion.originalText(), suggestion.suggestedText());
        
        update();
        
        qDebug() << "[AISuggestionOverlay] Accepted suggestion" << suggestionId;
        return true;
    }
    
    return false;
}

bool AISuggestionOverlay::rejectSuggestion(const QString& suggestionId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_suggestions.contains(suggestionId)) {
        return false;
    }
    
    AISuggestion& suggestion = m_suggestions[suggestionId];
    
    if (!suggestion.canReject()) {
        return false;
    }
    
    // Update state
    suggestion.setState(SuggestionState::Rejected);
    
    // Start fade out animation
    startFadeAnimation(suggestionId, false);
    
    // Remove from visible suggestions
    m_visibleSuggestions.removeAll(suggestionId);
    
    // Update focused suggestion if this was the focused one
    if (m_focusedSuggestionId == suggestionId) {
        m_focusedSuggestionId.clear();
    }
    
    // Emit signal
    emit suggestionRejected(suggestionId);
    
    update();
    
    qDebug() << "[AISuggestionOverlay] Rejected suggestion" << suggestionId;
    
    return true;
}

bool AISuggestionOverlay::acceptCurrentSuggestion()
{
    if (m_focusedSuggestionId.isEmpty()) {
        return false;
    }
    
    return acceptSuggestion(m_focusedSuggestionId);
}

bool AISuggestionOverlay::rejectCurrentSuggestion()
{
    if (m_focusedSuggestionId.isEmpty()) {
        return false;
    }
    
    return rejectSuggestion(m_focusedSuggestionId);
}

bool AISuggestionOverlay::nextSuggestion()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_visibleSuggestions.isEmpty()) {
        return false;
    }
    
    // Find current focused suggestion
    int currentIndex = -1;
    if (!m_focusedSuggestionId.isEmpty()) {
        currentIndex = m_visibleSuggestions.indexOf(m_focusedSuggestionId);
    }
    
    // Move to next suggestion
    int nextIndex = (currentIndex + 1) % m_visibleSuggestions.size();
    QString nextSuggestionId = m_visibleSuggestions[nextIndex];
    
    updateFocusedSuggestion(nextSuggestionId);
    
    return true;
}

bool AISuggestionOverlay::previousSuggestion()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_visibleSuggestions.isEmpty()) {
        return false;
    }
    
    // Find current focused suggestion
    int currentIndex = -1;
    if (!m_focusedSuggestionId.isEmpty()) {
        currentIndex = m_visibleSuggestions.indexOf(m_focusedSuggestionId);
    }
    
    // Move to previous suggestion
    int prevIndex = currentIndex > 0 ? currentIndex - 1 : m_visibleSuggestions.size() - 1;
    QString prevSuggestionId = m_visibleSuggestions[prevIndex];
    
    updateFocusedSuggestion(prevSuggestionId);
    
    return true;
}

void AISuggestionOverlay::setStyle(const SuggestionStyle& style)
{
    m_style = style;
    update();
}

void AISuggestionOverlay::setShortcuts(const ShortcutConfig& shortcuts)
{
    m_shortcuts = shortcuts;
}

void AISuggestionOverlay::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    
    m_enabled = enabled;
    
    if (!enabled) {
        clearSuggestions();
        hide();
    } else {
        show();
    }
    
    emit overlayVisibilityChanged(enabled);
}

void AISuggestionOverlay::setMaxVisibleSuggestions(int max)
{
    m_maxVisibleSuggestions = qMax(1, max);
    
    // Update visible suggestions if limit changed
    if (m_visibleSuggestions.size() > m_maxVisibleSuggestions) {
        while (m_visibleSuggestions.size() > m_maxVisibleSuggestions) {
            QString suggestionId = m_visibleSuggestions.takeLast();
            m_suggestions[suggestionId].setVisible(false);
        }
    }
    
    update();
}

void AISuggestionOverlay::setSuggestionTimeout(int timeout)
{
    m_suggestionTimeout = qMax(1000, timeout); // Minimum 1 second
}

void AISuggestionOverlay::showSuggestionDetails(const QString& suggestionId)
{
    if (!m_suggestions.contains(suggestionId)) {
        return;
    }
    
    const AISuggestion& suggestion = m_suggestions[suggestionId];
    
    QString tooltipText = QStringLiteral("<b>%1</b><br/>%2<br/>Confidence: %3<br/>Source: %4")
        .arg(suggestion.toDisplayString())
        .arg(suggestion.description())
        .arg(static_cast<int>(suggestion.confidence()))
        .arg(suggestion.source());
    
    QPoint screenPoint = mapToGlobal(suggestion.boundingRect().bottomRight());
    QToolTip::showText(screenPoint, tooltipText, this);
}

void AISuggestionOverlay::hideSuggestionDetails()
{
    QToolTip::hideText();
}

void AISuggestionOverlay::updatePositions()
{
    if (!m_textEditor) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Update positions for all suggestions
    for (auto& suggestion : m_suggestions.values()) {
        QTextCursor cursor = getCursorForPosition(suggestion.position());
        suggestion.setPosition(cursor.position());
        
        // Recalculate bounding rectangle
        QRect rect = calculateSuggestionRect(suggestion);
        suggestion.setBoundingRect(rect);
    }
    
    update();
}

void AISuggestionOverlay::refresh()
{
    updatePositions();
    calculatePositions();
    update();
}

// ============================================================================
// Event Handlers
// ============================================================================

void AISuggestionOverlay::paintEvent(QPaintEvent* event)
{
    if (!m_enabled || m_visibleSuggestions.isEmpty()) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    QMutexLocker locker(&m_mutex);
    
    for (const QString& suggestionId : m_visibleSuggestions) {
        if (!m_suggestions.contains(suggestionId)) {
            continue;
        }
        
        const AISuggestion& suggestion = m_suggestions[suggestionId];
        
        if (!suggestion.isVisible()) {
            continue;
        }
        
        // Set opacity
        painter.setOpacity(suggestion.opacity() * m_fadeOpacity);
        
        // Calculate text position
        QTextCursor cursor = getCursorForPosition(suggestion.position());
        QPoint textPos = getScreenCoordinates(suggestion.position());
        
        // Draw glow effect if enabled
        if (m_style.glow) {
            drawSuggestionGlow(&painter, suggestion);
        }
        
        // Draw suggestion text
        drawSuggestion(&painter, suggestion, textPos);
        
        // Draw highlight if focused
        if (suggestionId == m_focusedSuggestionId) {
            drawSuggestionHighlight(&painter, suggestion);
        }
    }
}

void AISuggestionOverlay::keyPressEvent(QKeyEvent* event)
{
    if (!m_enabled || m_visibleSuggestions.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    QKeySequence keySequence(event->key() | event->modifiers());
    
    // Handle accept shortcut
    if (keySequence.matches(QKeySequence::fromString(m_shortcuts.accept))) {
        if (acceptCurrentSuggestion()) {
            event->accept();
            return;
        }
    }
    
    // Handle reject shortcut
    if (keySequence.matches(QKeySequence::fromString(m_shortcuts.reject))) {
        if (rejectCurrentSuggestion()) {
            event->accept();
            return;
        }
    }
    
    // Handle navigation
    if (event->key() == Qt::Key_Tab && !event->modifiers()) {
        if (nextSuggestion()) {
            event->accept();
            return;
        }
    }
    
    if (event->key() == Qt::Key_Backtab) {
        if (previousSuggestion()) {
            event->accept();
            return;
        }
    }
    
    // Handle show details
    if (keySequence.matches(QKeySequence::fromString(m_shortcuts.showDetails))) {
        if (!m_focusedSuggestionId.isEmpty()) {
            showSuggestionDetails(m_focusedSuggestionId);
            event->accept();
            return;
        }
    }
    
    QWidget::keyPressEvent(event);
}

void AISuggestionOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!m_enabled) {
        QWidget::mousePressEvent(event);
        return;
    }
    
    QString clickedSuggestionId = findSuggestionAt(event->pos());
    
    if (!clickedSuggestionId.isEmpty()) {
        // Check if clicked on accept/reject area
        if (event->button() == Qt::LeftButton) {
            acceptSuggestion(clickedSuggestionId);
            event->accept();
            return;
        } else if (event->button() == Qt::RightButton) {
            rejectSuggestion(clickedSuggestionId);
            event->accept();
            return;
        }
    }
    
    QWidget::mousePressEvent(event);
}

void AISuggestionOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_enabled) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    
    QString hoverSuggestionId = findSuggestionAt(event->pos());
    
    if (hoverSuggestionId != m_focusedSuggestionId) {
        updateFocusedSuggestion(hoverSuggestionId);
    }
    
    QWidget::mouseMoveEvent(event);
}

void AISuggestionOverlay::enterEvent(QEvent* event)
{
    setMouseTracking(true);
    QWidget::enterEvent(event);
}

void AISuggestionOverlay::leaveEvent(QEvent* event)
{
    setMouseTracking(false);
    m_focusedSuggestionId.clear();
    update();
    QWidget::leaveEvent(event);
}

void AISuggestionOverlay::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
}

void AISuggestionOverlay::focusOutEvent(QFocusEvent* event)
{
    m_focusedSuggestionId.clear();
    update();
    QWidget::focusOutEvent(event);
}

void AISuggestionOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePositions();
}

void AISuggestionOverlay::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange) {
        update();
    } else if (event->type() == QEvent::PaletteChange) {
        update();
    }
    
    QWidget::changeEvent(event);
}

void AISuggestionOverlay::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_timeoutTimerId) {
        onTimeoutTimer();
    }
    
    QWidget::timerEvent(event);
}

// ============================================================================
// Private Slots
// ============================================================================

void AISuggestionOverlay::onTextChanged()
{
    if (!m_enabled) {
        return;
    }
    
    updatePositions();
}

void AISuggestionOverlay::onCursorPositionChanged()
{
    if (!m_enabled) {
        return;
    }
    
    updatePositions();
}

void AISuggestionOverlay::onTimeoutTimer()
{
    QMutexLocker locker(&m_mutex);
    
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> expiredSuggestions;
    
    for (auto& suggestion : m_suggestions) {
        // Simple timeout logic - remove suggestions that are too old
        // In a real implementation, would track creation time
        if (suggestion.state() == SuggestionState::Displayed) {
            expiredSuggestions.append(suggestion.id());
        }
    }
    
    for (const QString& suggestionId : expiredSuggestions) {
        removeSuggestion(suggestionId);
    }
}

void AISuggestionOverlay::onFadeAnimationFinished()
{
    if (m_animatingSuggestionId.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_fadeOpacity <= 0.01) {
        // Fade out complete, remove suggestion
        m_suggestions.remove(m_animatingSuggestionId);
        m_visibleSuggestions.removeAll(m_animatingSuggestionId);
    }
    
    m_animatingSuggestionId.clear();
    update();
}

void AISuggestionOverlay::onSuggestionApplied()
{
    // This would be called when a suggestion is applied to the text
    // Update positions and refresh the display
    updatePositions();
}

// ============================================================================
// Private Methods
// ============================================================================

void AISuggestionOverlay::calculatePositions()
{
    if (!m_textEditor || !m_textEditor->document()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Recalculate positions for all visible suggestions
    for (const QString& suggestionId : m_visibleSuggestions) {
        if (!m_suggestions.contains(suggestionId)) {
            continue;
        }
        
        AISuggestion& suggestion = m_suggestions[suggestionId];
        
        // Update position from text cursor
        QTextCursor cursor = getCursorForPosition(suggestion.position());
        suggestion.setPosition(cursor.position());
        
        // Recalculate bounding rectangle
        QRect rect = calculateSuggestionRect(suggestion);
        suggestion.setBoundingRect(rect);
    }
}

QString AISuggestionOverlay::findSuggestionAt(const QPoint& point) const
{
    for (const QString& suggestionId : m_visibleSuggestions) {
        const AISuggestion& suggestion = m_suggestions.value(suggestionId);
        if (suggestion.isVisible() && suggestion.boundingRect().contains(point)) {
            return suggestionId;
        }
    }
    
    return QString();
}

bool AISuggestionOverlay::applySuggestion(const AISuggestion& suggestion)
{
    if (!m_textEditor) {
        return false;
    }
    
    QTextCursor cursor = m_textEditor->textCursor();
    cursor.setPosition(suggestion.position());
    
    // Select the text to replace
    cursor.setPosition(suggestion.position() + suggestion.length(), QTextCursor::KeepAnchor);
    
    // Replace with suggested text
    cursor.insertText(suggestion.suggestedText());
    
    // Move cursor to end of inserted text
    cursor.setPosition(cursor.position());
    m_textEditor->setTextCursor(cursor);
    
    return true;
}

QColor AISuggestionOverlay::getColorForType(SuggestionType type) const
{
    switch (type) {
        case SuggestionType::Completion:
            return m_style.completionColor;
        case SuggestionType::Refactoring:
            return m_style.refactoringColor;
        case SuggestionType::Optimization:
            return m_style.optimizationColor;
        case SuggestionType::Documentation:
            return m_style.documentationColor;
        case SuggestionType::Test:
            return m_style.testColor;
        case SuggestionType::Security:
            return m_style.securityColor;
        case SuggestionType::BugFix:
            return m_style.bugfixColor;
        case SuggestionType::Migration:
            return m_style.migrationColor;
        default:
            return m_style.completionColor;
    }
}

QFont AISuggestionOverlay::getFontForSuggestion(const AISuggestion& suggestion) const
{
    QFont font = m_style.font;
    font.setPointSize(m_style.fontSize);
    
    if (m_style.bold) font.setBold(true);
    if (m_style.italic) font.setItalic(true);
    
    return font;
}

void AISuggestionOverlay::drawSuggestion(QPainter* painter, const AISuggestion& suggestion, const QPoint& position)
{
    QFont font = getFontForSuggestion(suggestion);
    QColor color = getColorForType(suggestion.type());
    
    // Set font and color
    painter->setFont(font);
    painter->setPen(color);
    
    // Draw text with proper positioning
    QPoint textPos = position;
    textPos.setY(textPos.y() + painter->fontMetrics().ascent());
    
    painter->drawText(textPos, suggestion.suggestedText());
    
    // Draw underline if configured
    if (m_style.underline) {
        QFontMetrics metrics(font);
        int textWidth = metrics.horizontalAdvance(suggestion.suggestedText());
        QRect underlineRect(textPos, QSize(textWidth, 1));
        painter->fillRect(underlineRect, color);
    }
    
    // Draw strikethrough if configured
    if (m_style.strikethrough) {
        QFontMetrics metrics(font);
        int textWidth = metrics.horizontalAdvance(suggestion.suggestedText());
        int y = textPos.y() - metrics.height() / 2;
        QRect strikethroughRect(textPos, QSize(textWidth, 1));
        painter->fillRect(strikethroughRect, color);
    }
}

void AISuggestionOverlay::drawSuggestionHighlight(QPainter* painter, const AISuggestion& suggestion)
{
    QRect highlightRect = suggestion.boundingRect();
    highlightRect.adjust(-2, -2, 2, 2); // Add padding
    
    // Draw subtle background highlight
    QColor highlightColor = getColorForType(suggestion.type());
    highlightColor.setAlpha(30); // Make it subtle
    
    painter->fillRect(highlightRect, highlightColor);
    
    // Draw border
    QPen borderPen(highlightColor);
    borderPen.setWidth(1);
    painter->setPen(borderPen);
    painter->drawRect(highlightRect);
}

void AISuggestionOverlay::drawSuggestionGlow(QPainter* painter, const AISuggestion& suggestion)
{
    QRect glowRect = suggestion.boundingRect();
    glowRect.adjust(-m_style.glowRadius, -m_style.glowRadius, 
                    m_style.glowRadius, m_style.glowRadius);
    
    // Create glow effect by drawing multiple rects with decreasing opacity
    for (int i = m_style.glowRadius; i > 0; --i) {
        QRect currentRect = glowRect;
        currentRect.adjust(-i, -i, i, i);
        
        QColor glowColor = m_style.glowColor;
        int alpha = (m_style.glowColor.alpha() * (m_style.glowRadius - i + 1)) / m_style.glowRadius;
        glowColor.setAlpha(alpha);
        
        painter->fillRect(currentRect, glowColor);
    }
}

void AISuggestionOverlay::startFadeAnimation(const QString& suggestionId, bool fadeIn)
{
    if (!m_suggestions.contains(suggestionId)) {
        return;
    }
    
    AISuggestion& suggestion = m_suggestions[suggestionId];
    
    m_animatingSuggestionId = suggestionId;
    
    if (fadeIn) {
        // Fade in
        m_fadeAnimation->setStartValue(0.0);
        m_fadeAnimation->setEndValue(1.0);
        suggestion.setState(SuggestionState::Displayed);
    } else {
        // Fade out
        m_fadeAnimation->setStartValue(1.0);
        m_fadeAnimation->setEndValue(0.0);
        suggestion.setState(SuggestionState::Expired);
    }
    
    m_fadeAnimation->start();
}

void AISuggestionOverlay::updateFocusedSuggestion(const QString& suggestionId)
{
    if (m_focusedSuggestionId == suggestionId) {
        return;
    }
    
    m_focusedSuggestionId = suggestionId;
    emit focusedSuggestionChanged(suggestionId);
    update();
}

bool AISuggestionOverlay::validateSuggestion(const AISuggestion& suggestion) const
{
    return !suggestion.id().isEmpty() && 
           !suggestion.suggestedText().isEmpty() &&
           suggestion.position() >= 0;
}

QTextCursor AISuggestionOverlay::getCursorForPosition(int position) const
{
    if (!m_textEditor || !m_textEditor->document()) {
        return QTextCursor();
    }
    
    QTextCursor cursor = m_textEditor->textCursor();
    cursor.setPosition(qMax(0, position));
    return cursor;
}

QPoint AISuggestionOverlay::getScreenCoordinates(int position) const
{
    if (!m_textEditor) {
        return QPoint();
    }
    
    QTextCursor cursor = getCursorForPosition(position);
    return m_textEditor->cursorRect(cursor).bottomRight();
}

QRect AISuggestionOverlay::calculateSuggestionRect(const AISuggestion& suggestion)
{
    if (!m_textEditor) {
        return QRect();
    }
    
    QFont font = getFontForSuggestion(suggestion);
    QFontMetrics metrics(font);
    
    QTextCursor cursor = getCursorForPosition(suggestion.position());
    QRect cursorRect = m_textEditor->cursorRect(cursor);
    
    int textWidth = metrics.horizontalAdvance(suggestion.suggestedText());
    int textHeight = metrics.height();
    
    QRect rect(cursorRect.right() + 2, cursorRect.top() - 2, 
               textWidth + 4, textHeight + 4);
    
    return rect;
}

} // namespace RawrXD
