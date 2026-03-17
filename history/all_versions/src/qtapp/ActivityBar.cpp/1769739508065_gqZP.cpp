#include "ActivityBar.h"
#include "ActivityBarButton.h"

ActivityBar::ActivityBar(void* parent)
    : QFrame(), m_activeView(Explorer)
{
    setFixedWidth(ACTIVITY_BAR_WIDTH);
    setStyleSheet("ActivityBar { background-color: rgb(51, 51, 51); border: none; }");
    
    createButtons();
    layoutButtons();
    
    setActiveView(Explorer);
}

ActivityBar::~ActivityBar() = default;

void ActivityBar::createButtons() {
    // Stub implementation
}

void ActivityBar::layoutButtons() {
    // Stub implementation
}

void ActivityBar::setActiveView(ViewType view) {
    if (view >= 0 && view < ViewCount) {
        m_activeView = view;
    }
}

ActivityBarButton* ActivityBar::button(ViewType view) const {
    if (view >= 0 && view < m_buttons.size()) {
        return m_buttons[view];
    }
    return nullptr;
}

    // Buttons are managed by layout
}


