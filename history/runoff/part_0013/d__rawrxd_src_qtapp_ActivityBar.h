#pragma once
/*
 * ActivityBar.h - VS Code-like Activity Bar for the left sidebar
 * CONVERTED TO STUBS - No Qt dependencies
 */

#include "../QtGUIStubs.hpp"

class ActivityBarButton;

class ActivityBar : public QFrame {

public:
    enum ViewType {
        Explorer = 0,
        Search = 1,
        SourceControl = 2,
        Debug = 3,
        Extensions = 4,
        Settings = 5,
        Accounts = 6,
        ViewCount = 7
    };

    explicit ActivityBar(void* parent = nullptr);
    ~ActivityBar() override;

    ViewType activeView() const { return m_activeView; }
    void setActiveView(ViewType view);
    ActivityBarButton* button(ViewType view) const;

protected:
    void paintEvent(void* event) override {}
    void resizeEvent(void* event) override {}

private:
    void createButtons();
    void layoutButtons();

    ViewType m_activeView = Explorer;
    std::vector<ActivityBarButton*> m_buttons;

    static constexpr int ACTIVITY_BAR_WIDTH = 50;
    static constexpr int BUTTON_SIZE = 48;
    static constexpr int BUTTON_ICON_SIZE = 24;
};



