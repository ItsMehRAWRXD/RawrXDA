#pragma once
class NotificationCenter {

public:
    enum class NotificationLevel {
        Info,
        Success,
        Warning,
        Error
    };

    explicit NotificationCenter(void* parent = nullptr);

    void addNotification(const std::string& title, const std::string& message, const std::string& category = "General");
    void notify(const std::string& title, const std::string& message, NotificationLevel level = NotificationLevel::Info);
    void clearAll();
    int notificationCount() const;
\npublic:\n    void notificationAdded(const std::string& title, const std::string& message, const std::string& category);

private:
    void updateCountLabel();

    QListWidget* m_list{};
    void* m_countLabel{};
    void* m_clearButton{};
};

