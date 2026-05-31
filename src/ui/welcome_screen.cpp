#include "ui/welcome_screen.h"
#include <fstream>

namespace RawrXD::UI {

WelcomeScreen::WelcomeScreen() = default;
WelcomeScreen::~WelcomeScreen() {
    shutdown();
}

bool WelcomeScreen::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDWelcomeScreen";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDWelcomeScreen", "Welcome",
                            WS_CHILD | WS_VISIBLE,
                            0, 0, 800, 600,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    if (m_hwnd) {
        createControls();
        registerDefaultItems();
    }

    return m_hwnd != nullptr;
}

void WelcomeScreen::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND WelcomeScreen::getHandle() const {
    return m_hwnd;
}

void WelcomeScreen::show() {
    m_visible = true;
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
    }
}

void WelcomeScreen::hide() {
    m_visible = false;
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool WelcomeScreen::isVisible() const {
    return m_visible;
}

void WelcomeScreen::addRecentFile(const std::string& path) {
    // Remove if already exists
    m_recentFiles.erase(std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
        [&path](const RecentFile& f) { return f.path == path; }), m_recentFiles.end());

    RecentFile file;
    file.path = path;
    file.name = path.substr(path.find_last_of("/\\") + 1);
    file.lastOpened = std::chrono::system_clock::now();
    file.exists = true; // Check if file exists

    m_recentFiles.insert(m_recentFiles.begin(), file);

    // Keep only last 10
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }

    updateRecentFiles();
}

void WelcomeScreen::removeRecentFile(const std::string& path) {
    m_recentFiles.erase(std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
        [&path](const RecentFile& f) { return f.path == path; }), m_recentFiles.end());
    updateRecentFiles();
}

void WelcomeScreen::clearRecentFiles() {
    m_recentFiles.clear();
    updateRecentFiles();
}

std::vector<RecentFile> WelcomeScreen::getRecentFiles(size_t limit) const {
    if (m_recentFiles.size() <= limit) {
        return m_recentFiles;
    }
    return std::vector<RecentFile>(m_recentFiles.begin(), m_recentFiles.begin() + limit);
}

void WelcomeScreen::updateRecentFiles() {
    // Update UI
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void WelcomeScreen::addGettingStartedItem(const GettingStartedItem& item) {
    m_gettingStartedItems.push_back(item);
}

void WelcomeScreen::removeGettingStartedItem(const std::string& itemId) {
    m_gettingStartedItems.erase(std::remove_if(m_gettingStartedItems.begin(), m_gettingStartedItems.end(),
        [&itemId](const GettingStartedItem& item) { return item.id == itemId; }), m_gettingStartedItems.end());
}

void WelcomeScreen::registerDefaultItems() {
    addGettingStartedItem({"newFile", "New File...", "Create a new file", "file", []() {}});
    addGettingStartedItem({"openFolder", "Open Folder...", "Open a folder", "folder", []() {}});
    addGettingStartedItem({"cloneRepo", "Clone Repository...", "Clone a git repository", "git", []() {}});
}

void WelcomeScreen::registerWalkthrough(const Walkthrough& walkthrough) {
    m_walkthroughs.push_back(walkthrough);
}

void WelcomeScreen::startWalkthrough(const std::string& walkthroughId) {
    m_currentWalkthrough = walkthroughId;
    m_currentStep = 0;
}

void WelcomeScreen::nextWalkthroughStep() {
    auto it = std::find_if(m_walkthroughs.begin(), m_walkthroughs.end(),
        [this](const Walkthrough& w) { return w.id == m_currentWalkthrough; });

    if (it != m_walkthroughs.end() && m_currentStep + 1 < it->steps.size()) {
        m_currentStep++;
    }
}

void WelcomeScreen::previousWalkthroughStep() {
    if (m_currentStep > 0) {
        m_currentStep--;
    }
}

void WelcomeScreen::endWalkthrough() {
    m_currentWalkthrough.clear();
    m_currentStep = 0;
}

void WelcomeScreen::setShowOnStartup(bool show) {
    m_showOnStartup = show;
}

bool WelcomeScreen::getShowOnStartup() const {
    return m_showOnStartup;
}

void WelcomeScreen::setCustomContent(const std::string& html) {
    // Set custom HTML content
}

void WelcomeScreen::setBackgroundImage(const std::string& path) {
    // Set background image
}

void WelcomeScreen::onOpenRecentFile(FileOpenCallback callback) {
    m_openRecentCallback = callback;
}

void WelcomeScreen::onOpenFolder(FolderOpenCallback callback) {
    m_openFolderCallback = callback;
}

void WelcomeScreen::onCloneRepository(CloneCallback callback) {
    m_cloneCallback = callback;
}

void WelcomeScreen::saveState(const std::string& path) {
    // Save state to file
}

void WelcomeScreen::loadState(const std::string& path) {
    // Load state from file
}

void WelcomeScreen::createControls() {
    // Create welcome screen controls
}

void WelcomeScreen::layout() {
    // Layout controls
}

void WelcomeScreen::render() {
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void WelcomeScreen::drawRecentFiles(HDC hdc, RECT& rect) {
    // Draw recent files section
}

void WelcomeScreen::drawGettingStarted(HDC hdc, RECT& rect) {
    // Draw getting started section
}

void WelcomeScreen::drawWalkthrough(HDC hdc, RECT& rect) {
    // Draw walkthrough section
}

LRESULT WelcomeScreen::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            // Draw welcome screen
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            // Handle clicks
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WelcomeScreen::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WelcomeScreen* screen = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        screen = reinterpret_cast<WelcomeScreen*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(screen));
    } else {
        screen = reinterpret_cast<WelcomeScreen*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (screen) {
        return screen->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
WelcomeScreen& getWelcomeScreen() {
    static WelcomeScreen screen;
    return screen;
}

} // namespace RawrXD::UI
