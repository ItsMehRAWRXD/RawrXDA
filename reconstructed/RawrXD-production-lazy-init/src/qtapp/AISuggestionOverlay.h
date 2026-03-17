/**
 * @file AISuggestionOverlay.h
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

#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTimer>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QRect>
#include <QPoint>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <memory>

namespace RawrXD {

/**
 * @brief Types of AI suggestions
 */
enum class SuggestionType {
    Completion,          ///< Code completion
    Refactoring,        ///< Code refactoring
    Optimization,       ///< Performance optimization
    Documentation,      ///< Documentation generation
    Test,              ///< Test generation
    Security,          ///< Security improvement
    BugFix,            ///< Bug fix suggestion
    Migration,         ///< Code migration
    Custom             ///< Custom suggestion type
};

/**
 * @brief Suggestion confidence levels
 */
enum class SuggestionConfidence {
    Low,                ///< Low confidence (50-70%)
    Medium,            ///< Medium confidence (70-85%)
    High,              ///< High confidence (85-95%)
    VeryHigh           ///< Very high confidence (95-100%)
};

/**
 * @brief Suggestion state
 */
enum class SuggestionState {
    Pending,            ///< Suggestion is pending
    Displayed,          ///< Suggestion is displayed
    Accepted,           ///< Suggestion was accepted
    Rejected,           ///< Suggestion was rejected
    Expired,           ///< Suggestion expired
    Error              ///< Suggestion encountered error
};

/**
 * @brief Represents a single AI suggestion
 */
class AISuggestion {
public:
    AISuggestion();
    AISuggestion(const QString& id, SuggestionType type, const QString& originalText, const QString& suggestedText);
    
    QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    
    SuggestionType type() const { return m_type; }
    void setType(SuggestionType type) { m_type = type; }
    
    QString originalText() const { return m_originalText; }
    void setOriginalText(const QString& text) { m_originalText = text; }
    
    QString suggestedText() const { return m_suggestedText; }
    void setSuggestedText(const QString& text) { m_suggestedText = text; }
    
    SuggestionConfidence confidence() const { return m_confidence; }
    void setConfidence(SuggestionConfidence confidence) { m_confidence = confidence; }
    
    SuggestionState state() const { return m_state; }
    void setState(SuggestionState state) { m_state = state; }
    
    QString description() const { return m_description; }
    void setDescription(const QString& desc) { m_description = desc; }
    
    QString source() const { return m_source; }
    void setSource(const QString& source) { m_source = source; }
    
    int position() const { return m_position; }
    void setPosition(int position) { m_position = position; }
    
    int length() const { return m_length; }
    void setLength(int length) { m_length = length; }
    
    QRect boundingRect() const { return m_boundingRect; }
    void setBoundingRect(const QRect& rect) { m_boundingRect = rect; }
    
    double opacity() const { return m_opacity; }
    void setOpacity(double opacity) { m_opacity = qBound(0.0, opacity, 1.0); }
    
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    
    bool canAccept() const;
    bool canReject() const;
    bool canModify() const;
    
    QString toDisplayString() const;
    
private:
    QString m_id;
    SuggestionType m_type;
    QString m_originalText;
    QString m_suggestedText;
    SuggestionConfidence m_confidence;
    SuggestionState m_state;
    QString m_description;
    QString m_source;
    int m_position;
    int m_length;
    QRect m_boundingRect;
    double m_opacity;
    bool m_visible;
};

/**
 * @brief Suggestion styling configuration
 */
struct SuggestionStyle {
    QColor completionColor;
    QColor refactoringColor;
    QColor optimizationColor;
    QColor documentationColor;
    QColor testColor;
    QColor securityColor;
    QColor bugfixColor;
    QColor migrationColor;
    
    QFont font;
    int fontSize;
    bool italic;
    bool bold;
    int spacing;
    
    // Animation settings
    int fadeInDuration;
    int fadeOutDuration;
    int highlightDuration;
    
    // Visual effects
    bool glow;
    QColor glowColor;
    int glowRadius;
    bool underline;
    bool strikethrough;
    
    SuggestionStyle();
};

/**
 * @brief Keyboard shortcut configuration
 */
struct ShortcutConfig {
    QString accept;
    QString acceptAndNext;
    QString reject;
    QString rejectAndNext;
    QString modify;
    QString cycle;
    QString showDetails;
    
    ShortcutConfig();
};

/**
 * @brief Complete AI Suggestion Overlay Widget
 * 
 * Provides advanced AI suggestion rendering with:
 * - Ghost text display for completions
 * - Inline refactoring suggestions
 * - Smooth animations and transitions
 * - Keyboard navigation and shortcuts
 * - Multiple suggestion sources
 * - Customizable themes and styling
 * - Accessibility support
 * 
 * Integrates with text editors to show AI suggestions
 * as non-intrusive overlay text.
 */
class AISuggestionOverlay : public QWidget {
    Q_OBJECT
    
    Q_PROPERTY(double fadeOpacity READ fadeOpacity WRITE setFadeOpacity)
    
public:
    /**
     * @brief Construct AISuggestionOverlay
     * @param parent Parent widget (usually a text editor)
     */
    explicit AISuggestionOverlay(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~AISuggestionOverlay() override;
    
    /**
     * @brief Set the associated text editor
     * @param editor Text editor widget
     */
    void setTextEditor(QPlainTextEdit* editor);
    
    /**
     * @brief Get the associated text editor
     * @return Text editor widget
     */
    QPlainTextEdit* textEditor() const { return m_textEditor; }
    
    /**
     * @brief Add a new suggestion
     * @param suggestion Suggestion to add
     * @return true if added successfully
     */
    bool addSuggestion(const AISuggestion& suggestion);
    
    /**
     * @brief Remove a suggestion by ID
     * @param suggestionId Suggestion ID to remove
     * @return true if removed successfully
     */
    bool removeSuggestion(const QString& suggestionId);
    
    /**
     * @brief Update an existing suggestion
     * @param suggestionId Suggestion ID to update
     * @param newSuggestion Updated suggestion data
     * @return true if updated successfully
     */
    bool updateSuggestion(const QString& suggestionId, const AISuggestion& newSuggestion);
    
    /**
     * @brief Get suggestion by ID
     * @param suggestionId Suggestion ID
     * @return AISuggestion object
     */
    AISuggestion getSuggestion(const QString& suggestionId) const;
    
    /**
     * @brief Get all suggestions
     * @return List of all suggestions
     */
    QList<AISuggestion> getSuggestions() const;
    
    /**
     * @brief Get suggestions by type
     * @param type Suggestion type
     * @return List of suggestions of the specified type
     */
    QList<AISuggestion> getSuggestionsByType(SuggestionType type) const;
    
    /**
     * @brief Get visible suggestions
     * @return List of visible suggestions
     */
    QList<AISuggestion> getVisibleSuggestions() const;
    
    /**
     * @brief Clear all suggestions
     */
    void clearSuggestions();
    
    /**
     * @brief Accept a suggestion
     * @param suggestionId Suggestion ID to accept
     * @return true if accepted successfully
     */
    bool acceptSuggestion(const QString& suggestionId);
    
    /**
     * @brief Reject a suggestion
     * @param suggestionId Suggestion ID to reject
     * @return true if rejected successfully
     */
    bool rejectSuggestion(const QString& suggestionId);
    
    /**
     * @brief Accept the current focused suggestion
     * @return true if a suggestion was accepted
     */
    bool acceptCurrentSuggestion();
    
    /**
     * @brief Reject the current focused suggestion
     * @return true if a suggestion was rejected
     */
    bool rejectCurrentSuggestion();
    
    /**
     * @brief Navigate to the next suggestion
     * @return true if navigation successful
     */
    bool nextSuggestion();
    
    /**
     * @brief Navigate to the previous suggestion
     * @return true if navigation successful
     */
    bool previousSuggestion();
    
    /**
     * @brief Set suggestion style
     * @param style New suggestion style
     */
    void setStyle(const SuggestionStyle& style);
    
    /**
     * @brief Get current suggestion style
     * @return Current style
     */
    SuggestionStyle style() const { return m_style; }
    
    /**
     * @brief Set keyboard shortcuts
     * @param shortcuts Shortcut configuration
     */
    void setShortcuts(const ShortcutConfig& shortcuts);
    
    /**
     * @brief Get current keyboard shortcuts
     * @return Current shortcuts
     */
    ShortcutConfig shortcuts() const { return m_shortcuts; }
    
    /**
     * @brief Enable/disable suggestion overlay
     * @param enabled Whether to enable
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if overlay is enabled
     * @return true if enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Set maximum number of visible suggestions
     * @param max Maximum suggestions
     */
    void setMaxVisibleSuggestions(int max);
    
    /**
     * @brief Get maximum visible suggestions
     * @return Maximum suggestions
     */
    int maxVisibleSuggestions() const { return m_maxVisibleSuggestions; }
    
    /**
     * @brief Set suggestion timeout (milliseconds)
     * @param timeout Timeout in ms
     */
    void setSuggestionTimeout(int timeout);
    
    /**
     * @brief Get suggestion timeout
     * @return Timeout in ms
     */
    int suggestionTimeout() const { return m_suggestionTimeout; }
    
    /**
     * @brief Show suggestion details tooltip
     * @param suggestionId Suggestion ID
     */
    void showSuggestionDetails(const QString& suggestionId);
    
    /**
     * @brief Hide suggestion details tooltip
     */
    void hideSuggestionDetails();
    
    /**
     * @brief Update suggestion positions based on text changes
     */
    void updatePositions();
    
    /**
     * @brief Force refresh of all suggestions
     */
    void refresh();
    
signals:
    /**
     * @brief Emitted when suggestion is accepted
     * @param suggestionId Accepted suggestion ID
     * @param originalText Original text
     * @param suggestedText Suggested text
     */
    void suggestionAccepted(const QString& suggestionId, const QString& originalText, const QString& suggestedText);
    
    /**
     * @brief Emitted when suggestion is rejected
     * @param suggestionId Rejected suggestion ID
     */
    void suggestionRejected(const QString& suggestionId);
    
    /**
     * @brief Emitted when suggestion state changes
     * @param suggestionId Suggestion ID
     * @param oldState Previous state
     * @param newState New state
     */
    void suggestionStateChanged(const QString& suggestionId, SuggestionState oldState, SuggestionState newState);
    
    /**
     * @brief Emitted when suggestion position changes
     * @param suggestionId Suggestion ID
     * @param newPosition New position
     */
    void suggestionPositionChanged(const QString& suggestionId, int newPosition);
    
    /**
     * @brief Emitted when focused suggestion changes
     * @param suggestionId New focused suggestion ID
     */
    void focusedSuggestionChanged(const QString& suggestionId);
    
    /**
     * @brief Emitted when overlay visibility changes
     * @param visible Whether overlay is visible
     */
    void overlayVisibilityChanged(bool visible);
    
protected:
    /**
     * @brief Paint event handler
     */
    void paintEvent(QPaintEvent* event) override;
    
    /**
     * @brief Key press event handler
     */
    void keyPressEvent(QKeyEvent* event) override;
    
    /**
     * @brief Mouse press event handler
     */
    void mousePressEvent(QMouseEvent* event) override;
    
    /**
     * @brief Mouse move event handler
     */
    void mouseMoveEvent(QMouseEvent* event) override;
    
    /**
     * @brief Enter event handler
     */
    void enterEvent(QEvent* event) override;
    
    /**
     * @brief Leave event handler
     */
    void leaveEvent(QEvent* event) override;
    
    /**
     * @brief Focus in event handler
     */
    void focusInEvent(QFocusEvent* event) override;
    
    /**
     * @brief Focus out event handler
     */
    void focusOutEvent(QFocusEvent* event) override;
    
    /**
     * @brief Resize event handler
     */
    void resizeEvent(QResizeEvent* event) override;
    
    /**
     * @brief Change event handler
     */
    void changeEvent(QEvent* event) override;
    
    /**
     * @brief Timer event handler
     */
    void timerEvent(QTimerEvent* event) override;
    
private slots:
    void onTextChanged();
    void onCursorPositionChanged();
    void onTimeoutTimer();
    void onFadeAnimationFinished();
    void onSuggestionApplied();
    
private:
    /**
     * @brief Initialize the overlay
     */
    void initialize();
    
    /**
     * @brief Calculate suggestion positions
     */
    void calculatePositions();
    
    /**
     * @brief Find suggestion at position
     * @param point Screen position
     * @return Suggestion ID if found, empty string otherwise
     */
    QString findSuggestionAt(const QPoint& point) const;
    
    /**
     * @brief Apply suggestion to text
     * @param suggestion Suggestion to apply
     * @return true if applied successfully
     */
    bool applySuggestion(const AISuggestion& suggestion);
    
    /**
     * @brief Get color for suggestion type
     * @param type Suggestion type
     * @return Color for the type
     */
    QColor getColorForType(SuggestionType type) const;
    
    /**
     * @brief Get font for suggestion
     * @param suggestion Suggestion
     * @return Configured font
     */
    QFont getFontForSuggestion(const AISuggestion& suggestion) const;
    
    /**
     * @brief Draw suggestion text
     * @param painter Painter object
     * @param suggestion Suggestion to draw
     * @param position Text position
     */
    void drawSuggestion(QPainter* painter, const AISuggestion& suggestion, const QPoint& position);
    
    /**
     * @brief Draw suggestion highlight
     * @param painter Painter object
     * @param suggestion Suggestion
     */
    void drawSuggestionHighlight(QPainter* painter, const AISuggestion& suggestion);
    
    /**
     * @brief Draw suggestion glow effect
     * @param painter Painter object
     * @param suggestion Suggestion
     */
    void drawSuggestionGlow(QPainter* painter, const AISuggestion& suggestion);
    
    /**
     * @brief Start fade animation
     * @param suggestionId Suggestion ID
     * @param fadeIn Whether to fade in or out
     */
    void startFadeAnimation(const QString& suggestionId, bool fadeIn);
    
    /**
     * @brief Update focused suggestion
     * @param suggestionId New focused suggestion ID
     */
    void updateFocusedSuggestion(const QString& suggestionId);
    
    /**
     * @brief Validate suggestion
     * @param suggestion Suggestion to validate
     * @return true if valid
     */
    bool validateSuggestion(const AISuggestion& suggestion) const;
    
    /**
     * @brief Get text cursor for position
     * @param position Document position
     * @return Text cursor
     */
    QTextCursor getCursorForPosition(int position) const;
    
    /**
     * @brief Get screen coordinates for document position
     * @param position Document position
     * @return Screen coordinates
     */
    QPoint getScreenCoordinates(int position) const;
    
    /**
     * @brief Fade opacity property access
     */
    double fadeOpacity() const { return m_fadeOpacity; }
    void setFadeOpacity(double opacity) { m_fadeOpacity = opacity; update(); }
    
    // Member variables
    QPlainTextEdit* m_textEditor;
    QMap<QString, AISuggestion> m_suggestions;
    QList<QString> m_visibleSuggestions;
    QString m_focusedSuggestionId;
    SuggestionStyle m_style;
    ShortcutConfig m_shortcuts;
    
    // State
    bool m_enabled;
    int m_maxVisibleSuggestions;
    int m_suggestionTimeout;
    int m_timeoutTimerId;
    bool m_overlayVisible;
    
    // Animation
    double m_fadeOpacity;
    std::unique_ptr<QPropertyAnimation> m_fadeAnimation;
    QString m_animatingSuggestionId;
    
    // Thread safety
    mutable QMutex m_mutex;
    
    // Constants
    static constexpr int DEFAULT_FADE_DURATION = 300;
    static constexpr int DEFAULT_HIGHLIGHT_DURATION = 500;
    static constexpr int SUGGESTION_MARGIN = 4;
};

} // namespace RawrXD
