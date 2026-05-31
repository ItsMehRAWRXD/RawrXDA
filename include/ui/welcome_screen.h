#pragma once
/**
 * @file welcome_screen.h
 * @brief Welcome page with recent files and getting started
 * Batch 4 - Item 55: Welcome screen
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

struct RecentFile {
    std::string path;
    std::string name;
    std::chrono::system_clock::time_point lastOpened;
    bool exists;
};

struct GettingStartedItem {
    std::string id;
    std::string title;
    std::string description;
    std::string icon;
    std::function<void()> action;
};

struct WalkthroughStep {
    std::string title;
    std::string description;
    std::string mediaPath;
    std::function<void()> action;
};

struct Walkthrough {
    std::string id;
    std::string title;
    std::string description;
    std::vector<WalkthroughStep> steps;
};

class WelcomeScreen {
public:
    WelcomeScreen();
    ~WelcomeScreen();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void show();
    void hide();
    bool isVisible() const;

    // Recent files
    void addRecentFile(const std::string& path);
    void removeRecentFile(const std::string& path);
    void clearRecentFiles();
    std::vector<RecentFile> getRecentFiles(size_t limit = 10) const;
    void updateRecentFiles();

    // Getting started
    void addGettingStartedItem(const GettingStartedItem& item);
    void removeGettingStartedItem(const std::string& itemId);
    void registerDefaultItems();

    // Walkthroughs
    void registerWalkthrough(const Walkthrough& walkthrough);
    void startWalkthrough(const std::string& walkthroughId);
    void nextWalkthroughStep();
    void previousWalkthroughStep();
    void endWalkthrough();

    // Start options
    void setShowOnStartup(bool show);
    bool getShowOnStartup() const;

    // Content
    void setCustomContent(const std::string& html);
    void setBackgroundImage(const std::string& path);

    // Events
    using FileOpenCallback = std::function<void(const std::string& path)>;
    using FolderOpenCallback = std::function<void(const std::string& path)>;
    using CloneCallback = std::function<void(const std::string& url)>;
    void onOpenRecentFile(FileOpenCallback callback);
    void onOpenFolder(FolderOpenCallback callback);
    void onCloneRepository(CloneCallback callback);

    // Persistence
    void saveState(const std::string& path);
    void loadState(const std::string& path);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    bool m_visible{true};
    bool m_showOnStartup{true};
    std::vector<RecentFile> m_recentFiles;
    std::vector<GettingStartedItem> m_gettingStartedItems;
    std::vector<Walkthrough> m_walkthroughs;
    std::string m_currentWalkthrough;
    size_t m_currentStep{0};

    FileOpenCallback m_openRecentCallback;
    FolderOpenCallback m_openFolderCallback;
    CloneCallback m_cloneCallback;

    void createControls();
    void layout();
    void render();
    void drawRecentFiles(HDC hdc, RECT& rect);
    void drawGettingStarted(HDC hdc, RECT& rect);
    void drawWalkthrough(HDC hdc, RECT& rect);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
WelcomeScreen& getWelcomeScreen();

} // namespace RawrXD::UI
