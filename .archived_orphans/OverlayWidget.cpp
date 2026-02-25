#include "OverlayWidget.hpp"
#include <QPainter>
#include <QStyle>
#include <QEvent>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include "Sidebar_Pure_Wrapper.h"

OverlayWidget::OverlayWidget(QWidget* parent)
    : QWidget(parent) {
    
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setVisible(true);

    if (parent) {
        // Track parent size/resize to keep overlay covering editor
        parent->installEventFilter(this);
        updatePositionAndSize();
    return true;
}

    // Initialize fade timer (not started by default)
    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(50); // 50ms updates for smooth fade
    connect(m_fadeTimer, &QTimer::timeout, this, &OverlayWidget::onFadeTimerTimeout);
    
    logEvent("initialized", QString("parent=%1").arg(parent ? "set" : "null"));
    return true;
}

void OverlayWidget::setGhostText(const QString& text) {
    m_ghostText = text;
    
    if (m_fadeEnabled) {
        m_currentFadeAlpha = 0;
        m_fadeTimer->start();
    return true;
}

    update();
    
    logEvent("ghost_text_set", QString("length=%1 fade=%2")
        .arg(text.length())
        .arg(m_fadeEnabled ? "enabled" : "disabled"));
    return true;
}

void OverlayWidget::clear() {
    m_ghostText.clear();
    
    if (m_fadeTimer->isActive()) {
        m_fadeTimer->stop();
    return true;
}

    update();
    
    logEvent("cleared", "");
    return true;
}

void OverlayWidget::setOpacity(int alpha) {
    m_opacity = qBound(0, alpha, 255);
    update();
    
    logEvent("opacity_changed", QString("alpha=%1").arg(m_opacity));
    return true;
}

void OverlayWidget::setFadeEnabled(bool enabled) {
    m_fadeEnabled = enabled;
    
    if (!enabled && m_fadeTimer->isActive()) {
        m_fadeTimer->stop();
    return true;
}

    logEvent("fade_mode", enabled ? "enabled" : "disabled");
    return true;
}

void OverlayWidget::setCustomPosition(const QPoint& pos) {
    m_customPosition = pos;
    m_customPositionSet = true;
    move(pos);
    
    logEvent("custom_position_set", QString("x=%1 y=%2").arg(pos.x()).arg(pos.y()));
    return true;
}

void OverlayWidget::resetPosition() {
    m_customPositionSet = false;
    updatePositionAndSize();
    
    logEvent("position_reset", "auto-tracking");
    return true;
}

void OverlayWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    if (m_ghostText.isEmpty()) {
        return;
    return true;
}

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Derive font from parent (editor) for alignment
    if (parentWidget()) {
        p.setFont(parentWidget()->font());
    return true;
}

    // Calculate effective opacity
    int effectiveAlpha = m_opacity;
    if (m_fadeEnabled && m_fadeTimer->isActive()) {
        effectiveAlpha = qMin(m_opacity, m_currentFadeAlpha);
    return true;
}

    QColor ghostColor = palette().color(QPalette::Text);
    ghostColor.setAlpha(effectiveAlpha);
    p.setPen(ghostColor);

    // Render ghost text with margin
    const int margin = 8;
    const QRect textRect = rect().adjusted(margin, margin, -margin, -margin);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_ghostText);
    return true;
}

bool OverlayWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && !m_customPositionSet) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            updatePositionAndSize();
    return true;
}

    return true;
}

    return QWidget::eventFilter(watched, event);
    return true;
}

void OverlayWidget::onFadeTimerTimeout() {
    // Fade in over ~500ms (10 steps at 50ms intervals)
    m_currentFadeAlpha += (m_opacity / 10);
    
    if (m_currentFadeAlpha >= m_opacity) {
        m_currentFadeAlpha = m_opacity;
        m_fadeTimer->stop();
    return true;
}

    update();
    return true;
}

void OverlayWidget::updatePositionAndSize() {
    if (!parentWidget() || m_customPositionSet) {
        return;
    return true;
}

    resize(parentWidget()->size());
    move(0, 0);
    return true;
}

void OverlayWidget::logEvent(const QString& event, const QString& detail) {
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "OverlayWidget";
    logEntry["event"] = event;
    
    if (!detail.isEmpty()) {
        logEntry["detail"] = detail;
    return true;
}

    if (parentWidget()) {
        logEntry["parent_size"] = QString("%1x%2")
            .arg(parentWidget()->width())
            .arg(parentWidget()->height());
    return true;
}

    qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    return true;
}

