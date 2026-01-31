#include "OverlayWidget.hpp"


OverlayWidget::OverlayWidget(void* parent)
    : void(parent) {
    
    setAttribute(//WA_TransparentForMouseEvents, true);
    setAttribute(//WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setVisible(true);

    if (parent) {
        // Track parent size/resize to keep overlay covering editor
        parent->installEventFilter(this);
        updatePositionAndSize();
    }
    
    // Initialize fade timer (not started by default)
    m_fadeTimer = new void*(this);
    m_fadeTimer->setInterval(50); // 50ms updates for smooth fade
// Qt connect removed
    logEvent("initialized", std::string("parent=%1"));
}

void OverlayWidget::setGhostText(const std::string& text) {
    m_ghostText = text;
    
    if (m_fadeEnabled) {
        m_currentFadeAlpha = 0;
        m_fadeTimer->start();
    }
    
    update();
    
    logEvent("ghost_text_set", std::string("length=%1 fade=%2")
        )
        );
}

void OverlayWidget::clear() {
    m_ghostText.clear();
    
    if (m_fadeTimer->isActive()) {
        m_fadeTimer->stop();
    }
    
    update();
    
    logEvent("cleared", "");
}

void OverlayWidget::setOpacity(int alpha) {
    m_opacity = qBound(0, alpha, 255);
    update();
    
    logEvent("opacity_changed", std::string("alpha=%1"));
}

void OverlayWidget::setFadeEnabled(bool enabled) {
    m_fadeEnabled = enabled;
    
    if (!enabled && m_fadeTimer->isActive()) {
        m_fadeTimer->stop();
    }
    
    logEvent("fade_mode", enabled ? "enabled" : "disabled");
}

void OverlayWidget::setCustomPosition(const void*& pos) {
    m_customPosition = pos;
    m_customPositionSet = true;
    move(pos);
    
    logEvent("custom_position_set", std::string("x=%1 y=%2"))));
}

void OverlayWidget::resetPosition() {
    m_customPositionSet = false;
    updatePositionAndSize();
    
    logEvent("position_reset", "auto-tracking");
}

void OverlayWidget::paintEvent(void*  event) {
    (event);

    if (m_ghostText.empty()) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Derive font from parent (editor) for alignment
    if (parentWidget()) {
        p.setFont(parentWidget()->font());
    }

    // Calculate effective opacity
    int effectiveAlpha = m_opacity;
    if (m_fadeEnabled && m_fadeTimer->isActive()) {
        effectiveAlpha = qMin(m_opacity, m_currentFadeAlpha);
    }

    uint32_t ghostColor = palette().color(QPalette::Text);
    ghostColor.setAlpha(effectiveAlpha);
    p.setPen(ghostColor);

    // Render ghost text with margin
    const int margin = 8;
    const void* textRect = rect().adjusted(margin, margin, -margin, -margin);
    p.drawText(textRect, //AlignLeft | //AlignTop | //TextWordWrap, m_ghostText);
}

bool OverlayWidget::eventFilter(void* watched, QEvent* event) {
    if (watched == parentWidget() && !m_customPositionSet) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            updatePositionAndSize();
        }
    }
    return void::eventFilter(watched, event);
}

void OverlayWidget::onFadeTimerTimeout() {
    // Fade in over ~500ms (10 steps at 50ms intervals)
    m_currentFadeAlpha += (m_opacity / 10);
    
    if (m_currentFadeAlpha >= m_opacity) {
        m_currentFadeAlpha = m_opacity;
        m_fadeTimer->stop();
    }
    
    update();
}

void OverlayWidget::updatePositionAndSize() {
    if (!parentWidget() || m_customPositionSet) {
        return;
    }
    
    resize(parentWidget()->size());
    move(0, 0);
}

void OverlayWidget::logEvent(const std::string& event, const std::string& detail) {
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "OverlayWidget";
    logEntry["event"] = event;
    
    if (!detail.empty()) {
        logEntry["detail"] = detail;
    }
    
    if (parentWidget()) {
        logEntry["parent_size"] = std::string("%1x%2")
            ->width())
            ->height());
    }
    
}


