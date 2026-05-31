#pragma once
/**
 * @file status_bar.h
 * @brief Status bar with multiple sections and progress
 * Batch 4 - Item 50: Status bar
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class StatusItemAlignment {
    Left,
    Right,
    Center
};

enum class StatusItemType {
    Text,
    Icon,
    Progress,
    Button,
    Separator
};

struct StatusBarItem {
    std::string id;
    std::string text;
    std::string tooltip;
    StatusItemType type;
    StatusItemAlignment alignment;
    int width;
    bool visible;
    bool enabled;
    std::function<void()> clickHandler;

    // For progress items
    int progressValue;
    int progressMax;
    bool progressIndeterminate;

    // For icon items
    HICON icon;
};

struct StatusBarSection {
    std::string name;
    std::vector<StatusBarItem> items;
    int priority;
};

class StatusBar {
public:
    StatusBar();
    ~StatusBar();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void setHeight(int height);
    int getHeight() const;
    void resize(int width, int height);

    // Items
    void addItem(const StatusBarItem& item);
    void removeItem(const std::string& itemId);
    void updateItem(const std::string& itemId, const std::string& text);
    void updateItem(const std::string& itemId, std::function<void(StatusBarItem&)> updater);
    std::optional<StatusBarItem> getItem(const std::string& itemId) const;

    // Visibility
    void setItemVisible(const std::string& itemId, bool visible);
    void setItemEnabled(const std::string& itemId, bool enabled);

    // Progress
    void setProgress(const std::string& itemId, int value, int max);
    void setProgressIndeterminate(const std::string& itemId, bool indeterminate);
    void showProgress(const std::string& itemId);
    void hideProgress(const std::string& itemId);

    // Default items
    void addDefaultItems();
    void addCursorPositionItem();
    void addLineEndingItem();
    void addLanguageItem();
    void addEncodingItem();
    void addGitBranchItem();
    void addErrorsWarningsItem();
    void addProgressItem();

    // Updates
    void setCursorPosition(int line, int column, int selection);
    void setLanguage(const std::string& language);
    void setEncoding(const std::string& encoding);
    void setLineEnding(const std::string& lineEnding);
    void setGitBranch(const std::string& branch);
    void setErrorsWarnings(int errors, int warnings);

    // Messages
    void showMessage(const std::string& message, int timeoutMs = 5000);
    void showError(const std::string& message);
    void showWarning(const std::string& message);
    void showInfo(const std::string& message);
    void clearMessage();

    // Events
    using ItemClickCallback = std::function<void(const std::string& itemId)>;
    void onItemClick(ItemClickCallback callback);

    // Drawing
    void redraw();
    void update();

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    int m_height{24};
    std::vector<StatusBarSection> m_sections;
    std::map<std::string, size_t> m_itemIndex;
    std::string m_currentMessage;
    ItemClickCallback m_clickCallback;

    void calculateLayout();
    void drawItem(HDC hdc, const StatusBarItem& item, RECT& rect);
    void drawProgress(HDC hdc, const StatusBarItem& item, RECT& rect);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
StatusBar& getStatusBar();

} // namespace RawrXD::UI
