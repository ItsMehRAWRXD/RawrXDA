#pragma once
/*
 * ActivityBar.h - VS Code-like Activity Bar for the left sidebar
 * 
 * This widget provides a 50px wide vertical toolbar with icons for switching
 * between different sidebar views (Explorer, Search, SCM, Debug, Extensions, etc.)
 * 
 * Features:
 * - Custom painted buttons with hover/active states
 * - VS Code color scheme (dark gray background, blue active indicator)
 * - Tooltips for each button
 * - Signal emission on button click for sidebar view switching
 */

#include <QFrame>
#include <QVector>
#include <QIcon>

class ActivityBarButton;

class ActivityBar : public QFrame
{
    Q_OBJECT

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

    explicit ActivityBar(QWidget* parent = nullptr);
    ~ActivityBar();

    /**
     * \brief Get the currently active view
     * \return The ViewType of the active button
     */
    ViewType activeView() const { return m_activeView; }

    /**
     * \brief Set the active view programmatically
     * \param view The ViewType to activate
     */
    void setActiveView(ViewType view);

    /**
     * \brief Get the button for a specific view
     * \param view The ViewType
     * \return Pointer to the ActivityBarButton, or nullptr if invalid
     */
    ActivityBarButton* button(ViewType view) const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    /**
     * \brief Emitted when a button is clicked
     * \param view The ViewType that was clicked
     */
    void viewChanged(ViewType view);

    /**
     * \brief Emitted when a button is hovered
     * \param view The ViewType being hovered, or -1 if not hovering
     */
    void viewHovered(int view);

private:
    void createButtons();
    void layoutButtons();

    ViewType m_activeView;
    QVector<ActivityBarButton*> m_buttons;

    // VS Code color scheme
    static constexpr QRgb BACKGROUND_COLOR = 0x333333;  // RGB(51,51,51)
    static constexpr QRgb ACTIVE_INDICATOR_COLOR = 0x007ACC;  // RGB(0,122,204)
    static constexpr QRgb HOVER_COLOR = 0x2D2D2D;  // Slightly lighter than background
    static constexpr int ACTIVITY_BAR_WIDTH = 50;
    static constexpr int BUTTON_SIZE = 48;
    static constexpr int BUTTON_ICON_SIZE = 24;
};
