/**
 * @file notification_center.cpp
 * @brief Full Notification Center Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "notification_center.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QUuid>
#include <QDateTime>


// Static instance
NotificationCenter* NotificationCenter::s_instance = nullptr;

// =============================================================================
// ToastNotification Implementation
// =============================================================================

ToastNotification::ToastNotification(const Notification& notification, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_notification(notification)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedWidth(350);
    
    setupUI();
    
    // Setup opacity effect for fade animation
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);
    
    // Fade in animation
    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(200);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
    
    // Auto-hide timer
    if (!notification.persistent && notification.autoHideMs > 0) {
        m_autoHideTimer = new QTimer(this);
        m_autoHideTimer->setSingleShot(true);
        connect(m_autoHideTimer, &QTimer::timeout, this, &ToastNotification::fadeOut);
        m_autoHideTimer->start(notification.autoHideMs);
    }
}

void ToastNotification::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(8);
    
    // Header row
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_iconLabel = new QLabel(getIcon(), this);
    m_iconLabel->setStyleSheet("font-size: 16px;");
    headerLayout->addWidget(m_iconLabel);
    
    m_titleLabel = new QLabel(m_notification.title, this);
    m_titleLabel->setStyleSheet("font-weight: bold; color: #ffffff;");
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();
    
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #808080; border: none; font-size: 16px; }"
        "QPushButton:hover { color: #ffffff; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &ToastNotification::close);
    headerLayout->addWidget(m_closeBtn);
    
    mainLayout->addLayout(headerLayout);
    
    // Message
    m_messageLabel = new QLabel(m_notification.message, this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setStyleSheet("color: #d4d4d4;");
    mainLayout->addWidget(m_messageLabel);
    
    // Progress (if applicable)
    if (m_notification.progress >= 0) {
        m_progressLabel = new QLabel(QString("%1%").arg(m_notification.progress), this);
        m_progressLabel->setStyleSheet("color: #4ec9b0;");
        mainLayout->addWidget(m_progressLabel);
    }
    
    // Actions
    if (!m_notification.actions.isEmpty()) {
        QHBoxLayout* actionsLayout = new QHBoxLayout();
        actionsLayout->addStretch();
        
        for (const NotificationAction& action : m_notification.actions) {
            QPushButton* btn = new QPushButton(action.text, this);
            btn->setProperty("actionId", action.actionId);
            btn->setStyleSheet(
                "QPushButton { background: #264f78; color: #ffffff; border: none; "
                "padding: 4px 12px; border-radius: 4px; }"
                "QPushButton:hover { background: #3d6f98; }");
            connect(btn, &QPushButton::clicked, this, [this, action]() {
                emit actionTriggered(m_notification.id, action.actionId);
                if (action.callback) action.callback();
                close();
            });
            actionsLayout->addWidget(btn);
        }
        
        mainLayout->addLayout(actionsLayout);
    }
    
    adjustSize();
}

void ToastNotification::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    QColor bgColor = getBackgroundColor();
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 8, 8);
    
    // Left accent bar
    QColor accentColor;
    switch (m_notification.level) {
        case NotificationLevel::Success: accentColor = QColor("#4ec9b0"); break;
        case NotificationLevel::Warning: accentColor = QColor("#dcdcaa"); break;
        case NotificationLevel::Error: accentColor = QColor("#f44747"); break;
        case NotificationLevel::Progress: accentColor = QColor("#569cd6"); break;
        default: accentColor = QColor("#4fc3f7"); break;
    }
    painter.setBrush(accentColor);
    painter.drawRoundedRect(0, 0, 4, height(), 2, 2);
}

void ToastNotification::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_notification.id);
    }
}

void ToastNotification::enterEvent(QEnterEvent*) {
    m_isHovered = true;
    if (m_autoHideTimer && m_autoHideTimer->isActive()) {
        m_autoHideTimer->stop();
    }
}

void ToastNotification::leaveEvent(QEvent*) {
    m_isHovered = false;
    if (m_autoHideTimer && !m_notification.persistent) {
        m_autoHideTimer->start(2000);  // Shorter timeout after hover
    }
}

void ToastNotification::fadeOut() {
    m_fadeAnimation->setStartValue(1.0);
    m_fadeAnimation->setEndValue(0.0);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
        emit closed(m_notification.id);
        deleteLater();
    });
    m_fadeAnimation->start();
}

void ToastNotification::close() {
    fadeOut();
}

void ToastNotification::updateProgress(int progress) {
    m_notification.progress = progress;
    if (m_progressLabel) {
        m_progressLabel->setText(QString("%1%").arg(progress));
    }
}

QColor ToastNotification::getBackgroundColor() const {
    return QColor(45, 45, 45, 240);  // Semi-transparent dark
}

QString ToastNotification::getIcon() const {
    switch (m_notification.level) {
        case NotificationLevel::Success: return "✓";
        case NotificationLevel::Warning: return "⚠";
        case NotificationLevel::Error: return "✕";
        case NotificationLevel::Progress: return "⏳";
        default: return "ℹ";
    }
}

// =============================================================================
// NotificationCenter Implementation
// =============================================================================

NotificationCenter* NotificationCenter::instance() {
    if (!s_instance) {
        s_instance = new NotificationCenter();
    }
    return s_instance;
}

NotificationCenter::NotificationCenter(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
    , m_queueTimer(new QTimer(this))
{
    // RawrXD::Integration::ScopedInitTimer expects const char* - pass string literal directly
    RawrXD::Integration::ScopedInitTimer init("NotificationCenter");
    s_instance = this;
    
    setupUI();
    connectSignals();
    loadSettings();
    
    // Queue processing timer
    m_queueTimer->setInterval(300);
    connect(m_queueTimer, &QTimer::timeout, this, &NotificationCenter::processQueue);
}

NotificationCenter::~NotificationCenter() {
    saveSettings();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void NotificationCenter::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // History list
    m_historyList = new QListWidget(this);
    m_historyList->setStyleSheet(
        "QListWidget { background-color: #252526; border: none; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #333; }"
        "QListWidget::item:selected { background-color: #264f78; }");
    mainLayout->addWidget(m_historyList);
    
    // Empty state
    m_emptyLabel = new QLabel("No notifications", this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #808080; padding: 40px;");
    mainLayout->addWidget(m_emptyLabel);
    m_emptyLabel->hide();
    
    setMinimumSize(350, 400);
}

void NotificationCenter::setupToolbar() {
    m_toolbar = new QToolBar("Notifications", this);
    
    QLabel* titleLabel = new QLabel(" Notifications ", this);
    titleLabel->setStyleSheet("font-weight: bold; color: #d4d4d4;");
    m_toolbar->addWidget(titleLabel);
    
    m_unreadBadge = new QLabel("0", this);
    m_unreadBadge->setStyleSheet(
        "background-color: #f44747; color: white; padding: 2px 6px; "
        "border-radius: 8px; font-size: 10px;");
    m_unreadBadge->hide();
    m_toolbar->addWidget(m_unreadBadge);
    
    m_toolbar->addSeparator();
    
    m_markReadBtn = new QPushButton("Mark All Read", this);
    m_markReadBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #4fc3f7; border: none; }"
        "QPushButton:hover { text-decoration: underline; }");
    connect(m_markReadBtn, &QPushButton::clicked, this, &NotificationCenter::markAllAsRead);
    m_toolbar->addWidget(m_markReadBtn);
    
    m_clearBtn = new QPushButton("Clear All", this);
    m_clearBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #4fc3f7; border: none; }"
        "QPushButton:hover { text-decoration: underline; }");
    connect(m_clearBtn, &QPushButton::clicked, this, &NotificationCenter::clearHistory);
    m_toolbar->addWidget(m_clearBtn);
    
    m_toolbar->addSeparator();
    
    m_dndBtn = new QPushButton("🔔", this);
    m_dndBtn->setToolTip("Toggle Do Not Disturb");
    m_dndBtn->setCheckable(true);
    connect(m_dndBtn, &QPushButton::toggled, this, &NotificationCenter::setDoNotDisturb);
    m_toolbar->addWidget(m_dndBtn);
}

void NotificationCenter::connectSignals() {
    connect(m_historyList, &QListWidget::itemClicked, 
            this, &NotificationCenter::onHistoryItemClicked);
}

QString NotificationCenter::generateId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString NotificationCenter::notify(const QString& title, const QString& message,
                                   NotificationLevel level) {
    // ScopedTimer expects const char*, convert QString to const char*
    RawrXD::Integration::ScopedTimer timer("NotificationCenter", "notify", title.toUtf8().constData());
    Notification notification;
    notification.id = generateId();
    notification.title = title;
    notification.message = message;
    notification.level = level;
    notification.timestamp = QDateTime::currentDateTime();
    notification.autoHideMs = m_defaultDuration;
    
    addToHistory(notification);
    
    if (!m_doNotDisturb) {
        showToast(notification);
        playSound(level);
        sendDesktopNotification(notification);
    }
    
    emit notificationAdded(notification);
    return notification.id;
}

QString NotificationCenter::notifyWithActions(const QString& title, const QString& message,
                                              NotificationLevel level,
                                              const QVector<NotificationAction>& actions) {
    Notification notification;
    notification.id = generateId();
    notification.title = title;
    notification.message = message;
    notification.level = level;
    notification.timestamp = QDateTime::currentDateTime();
    notification.actions = actions;
    notification.autoHideMs = 0;  // No auto-hide when actions present
    notification.persistent = true;
    
    addToHistory(notification);
    
    if (!m_doNotDisturb) {
        showToast(notification);
        playSound(level);
    }
    
    emit notificationAdded(notification);
    return notification.id;
}

QString NotificationCenter::notifyProgress(const QString& title, const QString& message,
                                           int progress) {
    Notification notification;
    notification.id = generateId();
    notification.title = title;
    notification.message = message;
    notification.level = NotificationLevel::Progress;
    notification.timestamp = QDateTime::currentDateTime();
    notification.progress = progress;
    notification.persistent = true;
    notification.autoHideMs = 0;
    
    addToHistory(notification);
    
    if (!m_doNotDisturb) {
        showToast(notification);
    }
    
    emit notificationAdded(notification);
    return notification.id;
}

void NotificationCenter::updateProgress(const QString& id, int progress) {
    for (ToastNotification* toast : m_activeToasts) {
        if (toast->getId() == id) {
            toast->updateProgress(progress);
            if (progress >= 100) {
                QTimer::singleShot(1000, toast, &ToastNotification::close);
            }
            break;
        }
    }
    
    // Update history
    for (Notification& n : m_history) {
        if (n.id == id) {
            n.progress = progress;
            break;
        }
    }
}

void NotificationCenter::updateMessage(const QString& id, const QString& message) {
    for (Notification& n : m_history) {
        if (n.id == id) {
            n.message = message;
            break;
        }
    }
    updateHistoryUI();
}

void NotificationCenter::closeNotification(const QString& id) {
    for (ToastNotification* toast : m_activeToasts) {
        if (toast->getId() == id) {
            toast->close();
            break;
        }
    }
}

void NotificationCenter::showToast(const Notification& notification) {
    if (m_activeToasts.size() >= m_maxToasts) {
        // Queue the notification
        m_pendingQueue.enqueue(notification);
        if (!m_queueTimer->isActive()) {
            m_queueTimer->start();
        }
        return;
    }
    
    ToastNotification* toast = new ToastNotification(notification, nullptr);
    
    connect(toast, &ToastNotification::closed, this, &NotificationCenter::onToastClosed);
    connect(toast, &ToastNotification::clicked, this, &NotificationCenter::onToastClicked);
    connect(toast, &ToastNotification::actionTriggered, this, &NotificationCenter::onToastAction);
    
    m_activeToasts.append(toast);
    toast->show();
    
    positionToasts();
}

void NotificationCenter::positionToasts() {
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeom = screen->availableGeometry();
    
    int rightMargin = 20;
    int topMargin = 20;
    int spacing = 10;
    
    int y = topMargin;
    
    for (ToastNotification* toast : m_activeToasts) {
        int x = screenGeom.right() - toast->width() - rightMargin;
        toast->move(x, y);
        y += toast->height() + spacing;
    }
}

void NotificationCenter::onToastClosed(const QString& id) {
    for (int i = 0; i < m_activeToasts.size(); ++i) {
        if (m_activeToasts[i]->getId() == id) {
            m_activeToasts.removeAt(i);
            break;
        }
    }
    
    positionToasts();
    emit notificationClosed(id);
}

void NotificationCenter::onToastClicked(const QString& id) {
    markAsRead(id);
    emit notificationClicked(id);
}

void NotificationCenter::onToastAction(const QString& notificationId, const QString& actionId) {
    emit actionTriggered(notificationId, actionId);
}

void NotificationCenter::processQueue() {
    if (m_pendingQueue.isEmpty()) {
        m_queueTimer->stop();
        return;
    }
    
    if (m_activeToasts.size() < m_maxToasts) {
        Notification notification = m_pendingQueue.dequeue();
        showToast(notification);
    }
}

void NotificationCenter::addToHistory(const Notification& notification) {
    m_history.prepend(notification);
    
    // Limit history size
    if (m_history.size() > 100) {
        m_history.resize(100);
    }
    
    updateHistoryUI();
    emit unreadCountChanged(getUnreadCount());
}

void NotificationCenter::updateHistoryUI() {
    m_historyList->clear();
    
    for (const Notification& n : m_history) {
        QListWidgetItem* item = new QListWidgetItem();
        
        QString icon;
        switch (n.level) {
            case NotificationLevel::Success: icon = "✓"; break;
            case NotificationLevel::Warning: icon = "⚠"; break;
            case NotificationLevel::Error: icon = "✕"; break;
            case NotificationLevel::Progress: icon = "⏳"; break;
            default: icon = "ℹ"; break;
        }
        
        QString text = QString("%1 %2\n%3\n%4")
            .arg(icon)
            .arg(n.title)
            .arg(n.message)
            .arg(n.timestamp.toString("hh:mm:ss"));
        
        item->setText(text);
        item->setData(Qt::UserRole, n.id);
        
        if (!n.read) {
            item->setBackground(QColor("#2d2d2d"));
        }
        
        m_historyList->addItem(item);
    }
    
    m_emptyLabel->setVisible(m_history.isEmpty());
    m_historyList->setVisible(!m_history.isEmpty());
    
    // Update badge
    int unread = getUnreadCount();
    m_unreadBadge->setText(QString::number(unread));
    m_unreadBadge->setVisible(unread > 0);
}

void NotificationCenter::onHistoryItemClicked(QListWidgetItem* item) {
    QString id = item->data(Qt::UserRole).toString();
    markAsRead(id);
    emit notificationClicked(id);
}

void NotificationCenter::markAsRead(const QString& id) {
    for (Notification& n : m_history) {
        if (n.id == id) {
            n.read = true;
            break;
        }
    }
    updateHistoryUI();
    emit unreadCountChanged(getUnreadCount());
}

void NotificationCenter::markAllAsRead() {
    for (Notification& n : m_history) {
        n.read = true;
    }
    updateHistoryUI();
    emit unreadCountChanged(0);
}

int NotificationCenter::getUnreadCount() const {
    int count = 0;
    for (const Notification& n : m_history) {
        if (!n.read) count++;
    }
    return count;
}

void NotificationCenter::clearHistory() {
    m_history.clear();
    updateHistoryUI();
    emit unreadCountChanged(0);
}

void NotificationCenter::setDoNotDisturb(bool enabled) {
    m_doNotDisturb = enabled;
    m_dndBtn->setChecked(enabled);
    m_dndBtn->setText(enabled ? "🔕" : "🔔");
    m_dndBtn->setToolTip(enabled ? "Do Not Disturb (ON)" : "Do Not Disturb (OFF)");
}

void NotificationCenter::setMaxToasts(int max) {
    m_maxToasts = max;
}

void NotificationCenter::setDefaultDuration(int ms) {
    m_defaultDuration = ms;
}

void NotificationCenter::setSoundEnabled(bool enabled) {
    m_soundEnabled = enabled;
}

void NotificationCenter::setDesktopNotificationsEnabled(bool enabled) {
    m_desktopNotificationsEnabled = enabled;
}

void NotificationCenter::playSound(NotificationLevel level) {
    if (!m_soundEnabled) return;
    
    // Would play notification sound based on level
    // QSound::play(":/sounds/notification.wav");
}

void NotificationCenter::sendDesktopNotification(const Notification& notification) {
    if (!m_desktopNotificationsEnabled) return;
    
    // Would use system notification API (QSystemTrayIcon or native APIs)
}

void NotificationCenter::showNotificationCenter() {
    show();
    raise();
}

void NotificationCenter::hideNotificationCenter() {
    hide();
}

void NotificationCenter::toggleNotificationCenter() {
    if (isVisible()) {
        hideNotificationCenter();
    } else {
        showNotificationCenter();
    }
}

void NotificationCenter::saveSettings() {
    m_settings->setValue("Notifications/DoNotDisturb", m_doNotDisturb);
    m_settings->setValue("Notifications/SoundEnabled", m_soundEnabled);
    m_settings->setValue("Notifications/DesktopEnabled", m_desktopNotificationsEnabled);
    m_settings->setValue("Notifications/DefaultDuration", m_defaultDuration);
    m_settings->setValue("Notifications/MaxToasts", m_maxToasts);
}

void NotificationCenter::loadSettings() {
    m_doNotDisturb = m_settings->value("Notifications/DoNotDisturb", false).toBool();
    m_soundEnabled = m_settings->value("Notifications/SoundEnabled", true).toBool();
    m_desktopNotificationsEnabled = m_settings->value("Notifications/DesktopEnabled", true).toBool();
    m_defaultDuration = m_settings->value("Notifications/DefaultDuration", 5000).toInt();
    m_maxToasts = m_settings->value("Notifications/MaxToasts", 5).toInt();
    
    setDoNotDisturb(m_doNotDisturb);
}

