/**
 * @file notification_center.h
 * @brief Full Notification Center Widget for RawrXD IDE
 * @author RawrXD Team
 */

#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QTimer>
#include <QSettings>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QQueue>
#include <QDateTime>

/**
 * @brief Notification severity levels
 */
enum class NotificationLevel {
    Info,
    Success,
    Warning,
    Error,
    Progress
};

/**
 * @brief Notification action
 */
struct NotificationAction {
    QString text;
    QString actionId;
    std::function<void()> callback;
};

/**
 * @brief Single notification structure
 */
struct Notification {
    QString id;
    QString title;
    QString message;
    NotificationLevel level;
    QDateTime timestamp;
    bool read = false;
    bool persistent = false;
    int autoHideMs = 5000;
    int progress = -1;  // -1 = no progress, 0-100 = progress percentage
    QVector<NotificationAction> actions;
    QString source;  // Which component sent this
};

/**
 * @brief Toast notification popup widget
 */
class ToastNotification : public QWidget {
    Q_OBJECT

public:
    explicit ToastNotification(const Notification& notification, QWidget* parent = nullptr);
    
    QString getId() const { return m_notification.id; }
    void updateProgress(int progress);
    void close();

signals:
    void closed(const QString& id);
    void actionTriggered(const QString& notificationId, const QString& actionId);
    void clicked(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void fadeOut();

private:
    void setupUI();
    QColor getBackgroundColor() const;
    QString getIcon() const;

    Notification m_notification;
    QLabel* m_iconLabel;
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QLabel* m_progressLabel;
    QPushButton* m_closeBtn;
    QTimer* m_autoHideTimer;
    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    bool m_isHovered = false;
};

/**
 * @brief Full Notification Center Widget
 * 
 * Features:
 * - Toast notifications with auto-hide
 * - Notification history with read/unread state
 * - Multiple severity levels (info, success, warning, error)
 * - Progress notifications
 * - Action buttons on notifications
 * - Do not disturb mode
 * - Notification filtering by source/level
 * - Notification sound support
 * - Desktop notification integration
 */
class NotificationCenter : public QWidget {
    Q_OBJECT

public:
    static NotificationCenter* instance();
    
    explicit NotificationCenter(QWidget* parent = nullptr);
    ~NotificationCenter();

    // Show notifications
    QString notify(const QString& title, const QString& message, 
                   NotificationLevel level = NotificationLevel::Info);
    QString notifyWithActions(const QString& title, const QString& message,
                              NotificationLevel level, 
                              const QVector<NotificationAction>& actions);
    QString notifyProgress(const QString& title, const QString& message,
                           int progress = 0);
    
    // Update notifications
    void updateProgress(const QString& id, int progress);
    void updateMessage(const QString& id, const QString& message);
    void closeNotification(const QString& id);
    
    // History management
    QVector<Notification> getHistory() const { return m_history; }
    void clearHistory();
    void markAsRead(const QString& id);
    void markAllAsRead();
    int getUnreadCount() const;
    
    // Settings
    void setDoNotDisturb(bool enabled);
    bool isDoNotDisturb() const { return m_doNotDisturb; }
    void setMaxToasts(int max);
    void setDefaultDuration(int ms);
    void setSoundEnabled(bool enabled);
    void setDesktopNotificationsEnabled(bool enabled);

signals:
    void notificationAdded(const Notification& notification);
    void notificationClosed(const QString& id);
    void notificationClicked(const QString& id);
    void actionTriggered(const QString& notificationId, const QString& actionId);
    void unreadCountChanged(int count);

public slots:
    void showNotificationCenter();
    void hideNotificationCenter();
    void toggleNotificationCenter();

private slots:
    void onToastClosed(const QString& id);
    void onToastClicked(const QString& id);
    void onToastAction(const QString& notificationId, const QString& actionId);
    void processQueue();
    void onHistoryItemClicked(QListWidgetItem* item);

private:
    void setupUI();
    void setupToolbar();
    void connectSignals();
    
    void showToast(const Notification& notification);
    void positionToasts();
    void addToHistory(const Notification& notification);
    void updateHistoryUI();
    void playSound(NotificationLevel level);
    void sendDesktopNotification(const Notification& notification);
    
    QString generateId() const;
    void saveSettings();
    void loadSettings();

private:
    static NotificationCenter* s_instance;
    
    // UI Components
    QToolBar* m_toolbar;
    QListWidget* m_historyList;
    QLabel* m_emptyLabel;
    QPushButton* m_clearBtn;
    QPushButton* m_markReadBtn;
    QPushButton* m_dndBtn;
    QLabel* m_unreadBadge;
    
    // Active toasts
    QVector<ToastNotification*> m_activeToasts;
    QQueue<Notification> m_pendingQueue;
    
    // State
    QVector<Notification> m_history;
    bool m_doNotDisturb = false;
    int m_maxToasts = 5;
    int m_defaultDuration = 5000;
    bool m_soundEnabled = true;
    bool m_desktopNotificationsEnabled = true;
    
    QSettings* m_settings;
    QTimer* m_queueTimer;
};

