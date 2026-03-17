#include "widgets/notification_center.h"

NotificationCenter::NotificationCenter(QWidget* parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
    , m_countLabel(new QLabel(this))
    , m_clearButton(new QPushButton(tr("Clear All"), this))
{
    auto layout = new QVBoxLayout(this);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_clearButton);

    connect(m_clearButton, &QPushButton::clicked, this, &NotificationCenter::clearAll);
    updateCountLabel();
}

void NotificationCenter::addNotification(const QString& title, const QString& message, const QString& category) {
    auto item = new QListWidgetItem(m_list);
    item->setText(QStringLiteral("[%1] %2\n%3").arg(category, title, message));
    item->setToolTip(message);
    m_list->insertItem(0, item);
    updateCountLabel();
    emit notificationAdded(title, message, category);
}

void NotificationCenter::notify(const QString& title, const QString& message, NotificationLevel level) {
    QString category;
    switch (level) {
        case NotificationLevel::Success:
            category = QStringLiteral("Success");
            break;
        case NotificationLevel::Warning:
            category = QStringLiteral("Warning");
            break;
        case NotificationLevel::Error:
            category = QStringLiteral("Error");
            break;
        case NotificationLevel::Info:
        default:
            category = QStringLiteral("Info");
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
    m_countLabel->setText(tr("Notifications: %1").arg(m_list->count()));
}