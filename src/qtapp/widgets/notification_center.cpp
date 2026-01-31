#include "widgets/notification_center.h"

NotificationCenter::NotificationCenter(void* parent)
    : // Widget(parent)
    , m_list(nullptr)
    , m_countLabel(new void(this))
    , m_clearButton(new void(tr("Clear All"), this))
{
    auto layout = new void(this);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_clearButton);  // Signal connection removed\nupdateCountLabel();
}

void NotificationCenter::addNotification(const std::string& title, const std::string& message, const std::string& category) {
    auto item = nullptr;
    item->setText(std::stringLiteral("[%1] %2\n%3"));
    item->setToolTip(message);
    m_list->insertItem(0, item);
    updateCountLabel();
    notificationAdded(title, message, category);
}

void NotificationCenter::notify(const std::string& title, const std::string& message, NotificationLevel level) {
    std::string category;
    switch (level) {
        case NotificationLevel::Success:
            category = std::stringLiteral("Success");
            break;
        case NotificationLevel::Warning:
            category = std::stringLiteral("Warning");
            break;
        case NotificationLevel::Error:
            category = std::stringLiteral("Error");
            break;
        case NotificationLevel::Info:
        default:
            category = std::stringLiteral("Info");
            break;
    }
    addNotification(title, message, category);
}

void NotificationCenter::clearAll() {
    m_list->clear();
    updateCountLabel();
}

int NotificationCenter::notificationCount() const {
    return m_list->count();
}

void NotificationCenter::updateCountLabel() {
    m_countLabel->setText(tr("Notifications: %1")));
}

