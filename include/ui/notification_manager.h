#pragma once
/**
 * @file notification_manager.h
 * @brief Toast notifications and message center
 * Batch 4 - Item 54: Notification manager
 */

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class NotificationSeverity {
    Info,
    Success,
    Warning,
    Error
};

enum class NotificationSource {
    System,
    Extension,
    LanguageServer,
    Debugger,
    Git,
    Build,
    Custom
};

struct NotificationAction {
    std::string id;
    std::string label;
    std::function<void()> handler;
};

struct Notification {
    std::string id;
    std::string title;
    std::string message;
    NotificationSeverity severity;
    NotificationSource source;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::milliseconds duration;
    bool persistent;
    bool read;
    std::vector<NotificationAction> actions;
    std::string sourceId;
    HICON icon;
};

class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Notifications
    std::string showNotification(const std::string& title,
                                  const std::string& message,
                                  NotificationSeverity severity = NotificationSeverity::Info,
                                  NotificationSource source = NotificationSource::System);
    std::string showNotification(const Notification& notification);

    // Convenience methods
    std::string showInfo(const std::string& title, const std::string& message);
    std::string showSuccess(const std::string& title, const std::string& message);
    std::string showWarning(const std::string& title, const std::string& message);
    std::string showError(const std::string& title, const std::string& message);

    // Progress notifications
    std::string showProgress(const std::string& title,
                              const std::string& message,
                              int progress,
                              int total);
    void updateProgress(const std::string& notificationId, int progress, int total);
    void completeProgress(const std::string& notificationId,
                          bool success,
                          const std::string& message);

    // Management
    void dismissNotification(const std::string& notificationId);
    void dismissAllNotifications();
    void markAsRead(const std::string& notificationId);
    void markAllAsRead();

    // Queries
    std::vector<Notification> getNotifications() const;
    std::vector<Notification> getUnreadNotifications() const;
    std::vector<Notification> getNotificationsBySource(NotificationSource source) const;
    std::optional<Notification> getNotification(const std::string& notificationId) const;
    size_t getUnreadCount() const;
    bool hasUnread() const;

    // Message center
    void showMessageCenter();
    void hideMessageCenter();
    void toggleMessageCenter();
    bool isMessageCenterVisible() const;

    // Settings
    void setDoNotDisturb(bool enabled);
    bool isDoNotDisturb() const;
    void setNotificationDuration(NotificationSeverity severity, std::chrono::milliseconds duration);

    // Events
    using NotificationCallback = std::function<void(const Notification&)>;
    void onNotificationShown(NotificationCallback callback);
    void onNotificationDismissed(std::function<void(const std::string&)> callback);
    void onNotificationClicked(std::function<void(const std::string&)> callback);

    // Persistence
    void saveHistory(const std::string& path);
    void loadHistory(const std::string& path);
    void clearHistory();

private:
    HWND m_parent{nullptr};
    HWND m_messageCenter{nullptr};
    std::vector<Notification> m_notifications;
    std::map<NotificationSeverity, std::chrono::milliseconds> m_durations;
    bool m_doNotDisturb{false};
    uint32_t m_nextId{1};

    NotificationCallback m_showCallback;
    std::function<void(const std::string&)> m_dismissCallback;
    std::function<void(const std::string&)> m_clickCallback;

    void createToastWindow(const Notification& notification);
    void destroyToastWindow(const std::string& notificationId);
    void updateMessageCenter();
    std::string generateId();
};

// Global instance
NotificationManager& getNotificationManager();

} // namespace RawrXD::UI
