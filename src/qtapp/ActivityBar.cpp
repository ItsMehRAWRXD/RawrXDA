#include "ActivityBar.h"
#include "ActivityBarButton.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QIcon>
#include <QPushButton>

ActivityBar::ActivityBar(QWidget* parent)
    : QFrame(parent)
    , m_activeView(Explorer)
{
    setFixedWidth(ACTIVITY_BAR_WIDTH);
    setStyleSheet("ActivityBar { background-color: rgb(51, 51, 51); border: none; }");
    
    createButtons();
    layoutButtons();
    
    // Set Explorer as active by default
    setActiveView(Explorer);
}

ActivityBar::~ActivityBar()
{
}

void ActivityBar::createButtons()
{
    // Create buttons with appropriate names and icons
    const char* buttonNames[] = {
        "Explorer",
        "Search",
        "Source Control",
        "Run and Debug",
        "Extensions",
        "Settings",
        "Accounts"
    };
    
    for (int i = 0; i < ViewCount; ++i) {
        ActivityBarButton* btn = new ActivityBarButton(buttonNames[i], this);
        
        // Set a simple text icon for now (will be replaced with proper icons later)
        QString iconText = QString(QChar('A' + i));  // A, B, C, etc. as placeholder
        
        m_buttons.push_back(btn);
        
        // Connect button clicks to view change signal
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setActiveView(static_cast<ViewType>(i));
            emit viewChanged(static_cast<ViewType>(i));
        });
    }
}

void ActivityBar::layoutButtons()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Add buttons to layout
    for (ActivityBarButton* btn : m_buttons) {
        layout->addWidget(btn, 0, Qt::AlignHCenter);
    }
    
    // Add spacer at bottom to push buttons to top
    layout->addStretch();
    
    // Optional: Add account button at bottom (Settings and Accounts could go here)
    // For now, they're in the normal sequence
}

void ActivityBar::setActiveView(ViewType view)
{
    if (view >= 0 && view < ViewCount) {
        // Deactivate previous button
        if (m_activeView >= 0 && m_activeView < ViewCount) {
            if (m_buttons[m_activeView]) {
                m_buttons[m_activeView]->setActive(false);
            }
        }
        
        // Activate new button
        m_activeView = view;
        if (m_buttons[m_activeView]) {
            m_buttons[m_activeView]->setActive(true);
        }
    }
}

ActivityBarButton* ActivityBar::button(ViewType view) const
{
    if (view >= 0 && view < m_buttons.size()) {
        return m_buttons[view];
    }
    return nullptr;
}

void ActivityBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(BACKGROUND_COLOR));
}

void ActivityBar::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    // Buttons are managed by layout
}
