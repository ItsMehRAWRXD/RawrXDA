// ============================================================================
// Notification Service — Multi-Channel Notification System
// Unified notification delivery across multiple channels
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <queue>

namespace RawrXD::Notifications {

enum class NotificationChannel {
    EMAIL,
    SMS,
    PUSH,
    WEBHOOK,
    SLACK,
    TEAMS,
    IN_APP
};

enum class NotificationPriority {
    LOW,
    NORMAL,
    HIGH,
    URGENT
};

enum class NotificationStatus {
    PENDING,
    SENT,
    DELIVERED,
    FAILED,
    READ
};

struct Notification {
    std::string id;
    std::string title;
    std::string message;
    NotificationChannel channel;
    NotificationPriority priority;
    NotificationStatus status;
    std::string recipient;
    std::map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point sentAt;
    std::chrono::system_clock::time_point deliveredAt;
    int retryCount;
};

struct NotificationTemplate {
    std::string id;
    std::string name;
    std::string subject;
    std::string body;
    std::map<std::string, std::string> variables;
    NotificationChannel channel;
};

struct DeliveryReceipt {
    std::string notificationId;
    bool success;
    std::string errorMessage;
    std::chrono::system_clock::time_point deliveredAt;
    std::string provider;
};

class NotificationService {
public:
    explicit NotificationService(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    void RegisterTemplate(const NotificationTemplate& template_) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_templates[template_.id] = template_;
    }

    Notification SendNotification(const std::string& templateId,
                                  const std::map<std::string, std::string>& variables,
                                  const std::string& recipient,
                                  NotificationPriority priority) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        Notification notification;
        notification.id = GenerateNotificationId();
        notification.recipient = recipient;
        notification.priority = priority;
        notification.status = NotificationStatus::PENDING;
        notification.createdAt = std::chrono::system_clock::now();
        notification.retryCount = 0;
        
        // Apply template
        auto it = m_templates.find(templateId);
        if (it != m_templates.end()) {
            notification.title = ApplyTemplate(it->second.subject, variables);
            notification.message = ApplyTemplate(it->second.body, variables);
            notification.channel = it->second.channel;
        }
        
        // Queue for delivery
        m_notificationQueue.push(notification);
        m_notifications[notification.id] = notification;
        
        // Process queue
        ProcessQueue();
        
        return notification;
    }

    void SendBulkNotification(const std::string& templateId,
                             const std::map<std::string, std::string>& variables,
                             const std::vector<std::string>& recipients,
                             NotificationPriority priority) {
        for (const auto& recipient : recipients) {
            SendNotification(templateId, variables, recipient, priority);
        }
    }

    NotificationStatus GetStatus(const std::string& notificationId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_notifications.find(notificationId);
        if (it != m_notifications.end()) {
            return it->second.status;
        }
        return NotificationStatus::FAILED;
    }

    void MarkAsRead(const std::string& notificationId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_notifications.find(notificationId);
        if (it != m_notifications.end()) {
            it->second.status = NotificationStatus::READ;
        }
    }

    std::vector<Notification> GetNotificationsForRecipient(const std::string& recipient,
                                                               int limit = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<Notification> result;
        for (const auto& [id, notification] : m_notifications) {
            if (notification.recipient == recipient) {
                result.push_back(notification);
            }
        }
        
        // Sort by created time (newest first)
        std::sort(result.begin(), result.end(),
                 [](const Notification& a, const Notification& b) {
                     return a.createdAt > b.createdAt;
                 });
        
        if (result.size() > static_cast<size_t>(limit)) {
            result.resize(limit);
        }
        
        return result;
    }

    std::vector<Notification> GetUnreadNotifications(const std::string& recipient) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<Notification> result;
        for (const auto& [id, notification] : m_notifications) {
            if (notification.recipient == recipient && 
                notification.status != NotificationStatus::READ) {
                result.push_back(notification);
            }
        }
        
        return result;
    }

    void RetryFailedNotifications() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& [id, notification] : m_notifications) {
            if (notification.status == NotificationStatus::FAILED && 
                notification.retryCount < 3) {
                notification.status = NotificationStatus::PENDING;
                notification.retryCount++;
                m_notificationQueue.push(notification);
            }
        }
        
        ProcessQueue();
    }

    std::string GenerateNotificationReport() {
        std::ostringstream report;
        report << "# Notification Report\n\n";
        
        std::map<NotificationStatus, int> statusCounts;
        std::map<NotificationChannel, int> channelCounts;
        
        for (const auto& [id, notification] : m_notifications) {
            statusCounts[notification.status]++;
            channelCounts[notification.channel]++;
        }
        
        report << "## Status Summary\n";
        for (const auto& [status, count] : statusCounts) {
            report << "- " << StatusToString(status) << ": " << count << "\n";
        }
        
        report << "\n## Channel Summary\n";
        for (const auto& [channel, count] : channelCounts) {
            report << "- " << ChannelToString(channel) << ": " << count << "\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, NotificationTemplate> m_templates;
    std::map<std::string, Notification> m_notifications;
    std::queue<Notification> m_notificationQueue;

    std::string GenerateNotificationId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "notif_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string ApplyTemplate(const std::string& template_, 
                             const std::map<std::string, std::string>& variables) {
        std::string result = template_;
        for (const auto& [key, value] : variables) {
            std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
        return result;
    }

    void ProcessQueue() {
        while (!m_notificationQueue.empty()) {
            auto notification = m_notificationQueue.front();
            m_notificationQueue.pop();
            
            // Deliver notification
            bool success = DeliverNotification(notification);
            
            // Update status
            auto it = m_notifications.find(notification.id);
            if (it != m_notifications.end()) {
                if (success) {
                    it->second.status = NotificationStatus::SENT;
                    it->second.sentAt = std::chrono::system_clock::now();
                } else {
                    it->second.status = NotificationStatus::FAILED;
                }
            }
        }
    }

    bool DeliverNotification(const Notification& notification) {
        // Deliver based on channel
        switch (notification.channel) {
            case NotificationChannel::EMAIL:
                return SendEmail(notification);
            case NotificationChannel::SMS:
                return SendSMS(notification);
            case NotificationChannel::PUSH:
                return SendPush(notification);
            case NotificationChannel::SLACK:
                return SendSlack(notification);
            case NotificationChannel::IN_APP:
                return SendInApp(notification);
            default:
                return false;
        }
    }

    bool SendEmail(const Notification& notification) {
        // Email delivery logic
        return true;
    }

    bool SendSMS(const Notification& notification) {
        // SMS delivery logic
        return true;
    }

    bool SendPush(const Notification& notification) {
        // Push notification logic
        return true;
    }

    bool SendSlack(const Notification& notification) {
        // Slack delivery logic
        return true;
    }

    bool SendInApp(const Notification& notification) {
        // In-app notification logic
        return true;
    }

    std::string StatusToString(NotificationStatus status) {
        switch (status) {
            case NotificationStatus::PENDING: return "Pending";
            case NotificationStatus::SENT: return "Sent";
            case NotificationStatus::DELIVERED: return "Delivered";
            case NotificationStatus::FAILED: return "Failed";
            case NotificationStatus::READ: return "Read";
            default: return "Unknown";
        }
    }

    std::string ChannelToString(NotificationChannel channel) {
        switch (channel) {
            case NotificationChannel::EMAIL: return "Email";
            case NotificationChannel::SMS: return "SMS";
            case NotificationChannel::PUSH: return "Push";
            case NotificationChannel::WEBHOOK: return "Webhook";
            case NotificationChannel::SLACK: return "Slack";
            case NotificationChannel::TEAMS: return "Teams";
            case NotificationChannel::IN_APP: return "In-App";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Notifications
