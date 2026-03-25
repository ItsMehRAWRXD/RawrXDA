<<<<<<< HEAD
// multi_file_search.cpp — Win32/C++ implementation of MultiFileSearchWidget (Qt-free).
// Provides async search with cancellation + .gitignore filtering, plus a minimal
// Win32 dialog (invoked via setShowCallback) that polls results via a timer.

#include "../include/multi_file_search.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
=======
/**
 * @file multi_file_search.cpp
 * @brief Implementation of MultiFileSearchWidget for VS Code-style multi-file search.
 *
 * @details
 * This implementation provides:
 * - Asynchronous search using QtConcurrent::run() with QFutureWatcher
 * - Thread-safe result collection via std::mutex-protected queue
 * - .gitignore pattern parsing and matching
 * - Regex and literal text search modes
 * - Grouped tree view display with file context
 * - Proper signal marshalling to main thread via queued connections
 *
 * @note Search operations run on a background thread pool. All UI updates
 *       are marshalled back to the main thread via Qt's signal system.
 *
 * @copyright MIT License
 */

#include "multi_file_search.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
>>>>>>> origin/main

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <commctrl.h>
#include <windows.h>
#pragma comment(lib, "comctl32.lib")
#endif

<<<<<<< HEAD
namespace fs = std::filesystem;
=======
/**
 * @brief Constructor - initializes all UI components and signal connections.
 */
MultiFileSearchWidget::MultiFileSearchWidget(void* parent)
    : void(parent)
    , m_searchWatcher(nullptr)
{
    setWindowTitle("Multi-File Search");
    setMinimumSize(400, 300);
>>>>>>> origin/main

// ============================================================================
// MultiFileSearchWidget public API
// ============================================================================

<<<<<<< HEAD
std::vector<MultiFileSearchResult> MultiFileSearchWidget::takePendingResults() {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    std::vector<MultiFileSearchResult> out;
    out.swap(m_pendingResults);
    return out;
=======
    auto mainLayout = new void(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Search input row
    auto searchLayout = new void();
    void* searchLabel = new void("Search:", this);
    m_searchInput = new void(this);
    m_searchInput->setPlaceholderText("Enter search query...");
    m_searchInput->setMinimumHeight(32);
    m_searchButton = new void("Search", this);
    m_searchButton->setMinimumHeight(32);
    m_searchButton->setMaximumWidth(100);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchInput, 1);
    searchLayout->addWidget(m_searchButton);
    mainLayout->addLayout(searchLayout);

    // File filter row
    auto filterLayout = new void();
    void* filterLabel = new void("File Filter:", this);
    m_fileFilterInput = new void(this);
    m_fileFilterInput->setPlaceholderText("*.cpp, *.h, *.hpp");
    m_fileFilterInput->setText("*");
    m_fileFilterInput->setMinimumHeight(28);
    m_fileFilterInput->setMaximumWidth(200);

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_fileFilterInput);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Options row
    auto optionsLayout = new void();
    m_caseSensitiveCheck = nullptr;
    m_regexCheck = nullptr;
    m_wholeWordCheck = nullptr;

    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_regexCheck);
    optionsLayout->addWidget(m_wholeWordCheck);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // Results tree view
    m_resultsTree = nullptr;
    m_resultsTree->setColumnCount(2);
    m_resultsTree->setHeaderLabels({"Location", "Context"});
    m_resultsTree->setColumnWidth(0, 200);
    m_resultsTree->setColumnWidth(1, 300);
    m_resultsTree->setMinimumHeight(150);
    m_resultsTree->setContextMenuPolicy(//NoContextMenu);
    m_resultsTree->setUniformRowHeights(true);
    mainLayout->addWidget(m_resultsTree, 1);

    // Status label
    m_statusLabel = new void("Ready", this);
    m_statusLabel->setStyleSheet("color: #666666; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // ─────────────────────────────────────────────────────────────────────
    // Connect Signals to Slots
    // ─────────────────────────────────────────────────────────────────────

    // Search button and Enter key trigger search
// Qt connect removed
// Qt connect removed
    // Tree item interactions
// Qt connect removed
    // Create future watcher for async search
    m_searchWatcher = new QFutureWatcher<void>(this);
// Qt connect removed
    // Batched result updates (queued to ensure main thread)
// Qt connect removed
                if (!m_pendingResults.empty()) {
                    for (const auto& result : m_pendingResults) {
                        addResultToTree(result);
                    }
                    m_pendingResults.clear();
                }
            }, //QueuedConnection);
>>>>>>> origin/main
}

MultiFileSearchWidget::~MultiFileSearchWidget() {
    m_searchCancelled = true;
    if (m_searchFuture.valid()) m_searchFuture.wait();
}

void MultiFileSearchWidget::show() {
    if (m_showCb) {
        m_showCb(m_showCtx ? m_showCtx : this);
    }
}

<<<<<<< HEAD
void MultiFileSearchWidget::setProjectRoot(const std::string& path) {
    m_projectRoot = path;
}

void MultiFileSearchWidget::setSearchQuery(const std::string& query) {
    m_searchQuery = query;
}

std::string MultiFileSearchWidget::searchQuery() const {
    return m_searchQuery;
}

void MultiFileSearchWidget::startSearch() {
    m_searchCancelled = false;

    if (m_projectRoot.empty() || m_searchQuery.empty()) {
        updateStatus("Set project root and search query first.");
        if (m_completedCb) m_completedCb(m_completedCtx, 0);
=======
/**
 * @brief Sets the project root directory for searches.
 */
void MultiFileSearchWidget::setProjectRoot(const std::string& path)
{
    m_projectRoot = path;
    updateStatus(std::string("Project root: %1").fileName()));
}

/**
 * @brief Programmatically sets the search query.
 */
void MultiFileSearchWidget::setSearchQuery(const std::string& query)
{
    m_searchInput->setText(query);
}

/**
 * @brief Returns the current search query.
 */
std::string MultiFileSearchWidget::searchQuery() const
{
    return m_searchInput->text();
}

/**
 * @brief Initiates asynchronous search operation.
 */
void MultiFileSearchWidget::startSearch()
{
    std::string query = m_searchInput->text().trimmed();
    if (query.empty() || m_projectRoot.empty()) {
        updateStatus("Error: Search query and project root required");
>>>>>>> origin/main
        return;
    }

    // Cancel previous search if any.
    cancelSearch();

    m_isSearching = true;
    m_totalResultCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_pendingResults.clear();
    }

    updateStatus("Searching...");

<<<<<<< HEAD
    const std::string query = m_searchQuery;
    const std::string root = m_projectRoot;
    const bool useRegex = m_useRegex;
    const bool caseSensitive = m_caseSensitive;
    const std::string filter = m_fileFilter;
=======
    bool useRegex = m_regexCheck->isChecked();
    bool caseSensitive = m_caseSensitiveCheck->isChecked();
    std::string fileFilter = m_fileFilterInput->text();
>>>>>>> origin/main

    m_searchFuture = std::async(std::launch::async, [this, query, root, useRegex, caseSensitive, filter]() {
        performSearch(query, root, useRegex, caseSensitive, filter);
    });
}

void MultiFileSearchWidget::cancelSearch() {
    m_searchCancelled = true;
    if (m_searchFuture.valid()) m_searchFuture.wait();
    m_isSearching = false;
}

void MultiFileSearchWidget::clearResults() {
    cancelSearch();
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_pendingResults.clear();
    }
    m_totalResultCount = 0;
    updateStatus("Results cleared.");
}

void MultiFileSearchWidget::focusSearchInput() {
    if (m_searchInput) {
#ifdef _WIN32
        SetFocus(m_searchInput);
        SendMessageA(m_searchInput, EM_SETSEL, 0, -1);
#endif
    }
}

void MultiFileSearchWidget::onResultItemDoubleClicked(void*, int) {
    if (!m_resultClickedCb) return;

    MultiFileSearchResult selected;
    {
<<<<<<< HEAD
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        if (m_pendingResults.empty()) return;
        selected = m_pendingResults.front();
=======
        std::lock_guard<std::mutex> locker(&m_resultsMutex);
        m_pendingResults.clear();
>>>>>>> origin/main
    }
    m_resultClickedCb(m_resultClickedCtx, selected);
}
void MultiFileSearchWidget::onSearchResultsReady() {}

<<<<<<< HEAD
void MultiFileSearchWidget::onSearchFinished() {
    m_isSearching = false;
    if (m_completedCb) m_completedCb(m_completedCtx, m_totalResultCount);
}

static bool globMatchExtList(const std::string& name, const std::string& patternCsv) {
    if (patternCsv.empty() || patternCsv == "*") return true;

    size_t p = 0;
    while (p < patternCsv.size()) {
        while (p < patternCsv.size() && (patternCsv[p] == ' ' || patternCsv[p] == '\t' || patternCsv[p] == ',')) p++;
        if (p >= patternCsv.size()) break;
        size_t end = patternCsv.find(',', p);
        if (end == std::string::npos) end = patternCsv.size();

        std::string pat = patternCsv.substr(p, end - p);
        while (!pat.empty() && (pat.back() == ' ' || pat.back() == '\t')) pat.pop_back();
        while (!pat.empty() && (pat.front() == ' ' || pat.front() == '\t')) pat.erase(pat.begin());

        // Only support "*.ext" style here; other patterns are treated as substring match.
        if (pat == "*") return true;
        if (pat.size() >= 2 && pat[0] == '*' && pat[1] == '.') {
            const std::string ext = pat.substr(1); // ".ext"
            if (name.size() >= ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
                return true;
            }
        } else if (!pat.empty()) {
            if (name.find(pat) != std::string::npos) return true;
        }

        p = end + 1;
    }
    return false;
}

void MultiFileSearchWidget::performSearch(const std::string& searchText,
    const std::string& rootPath, bool useRegex, bool caseSensitive,
    const std::string& fileFilter) {

    int filesSearched = 0;
    std::error_code ec;

    std::regex rePattern;
    if (useRegex) {
        try {
            rePattern = std::regex(searchText, caseSensitive ? std::regex::ECMAScript : std::regex::icase);
        } catch (...) {
            updateStatus("Invalid regex pattern.");
            onSearchFinished();
            return;
        }
    }

    auto findMatch = [&](const std::string& line, size_t startPos) -> std::pair<size_t, size_t> {
        if (useRegex) {
            std::smatch m;
            if (std::regex_search(line.begin() + (ptrdiff_t)startPos, line.end(), m, rePattern)) {
                return { startPos + (size_t)m.position(), (size_t)m.length() };
            }
            return { std::string::npos, 0 };
=======
/**
 * @brief Focuses the search input field.
 */
void MultiFileSearchWidget::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

/**
 * @brief Handles double-click on a result item.
 */
void MultiFileSearchWidget::onResultItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    (column);

    // Extract result data from item
    std::string file = item->data(0, //UserRole).toString();
    if (file.empty()) {
        return;
    }

    int line = item->data(0, //UserRole + 1).toInt();
    int column_pos = item->data(0, //UserRole + 2).toInt();
    std::string lineText = item->data(0, //UserRole + 3).toString();
    std::string matchedText = item->data(0, //UserRole + 4).toString();

    MultiFileSearchResult result(file, line, column_pos, lineText, matchedText);
    resultClicked(result);
}

/**
 * @brief Handles search completion and emits final signal.
 */
void MultiFileSearchWidget::onSearchFinished()
{
    // Flush any remaining pending results
    {
        std::lock_guard<std::mutex> locker(&m_resultsMutex);
        for (const auto& result : m_pendingResults) {
            addResultToTree(result);
        }
        m_pendingResults.clear();
    }

    m_isSearching = false;
    m_searchButton->setText("Search");

    if (m_searchCancelled.load(std::memory_order_acquire)) {
        updateStatus(std::string("Search cancelled. Found %1 results."));
    } else {
        updateStatus(std::string("Search complete. Found %1 results."));
    }

    searchCompleted(m_totalResultCount);
}

/**
 * @brief Core search implementation - runs on background thread.
 */
void MultiFileSearchWidget::performSearch(const std::string& searchText,
                                         const std::string& rootPath,
                                         bool useRegex,
                                         bool caseSensitive,
                                         const std::string& fileFilter)
{
    if (m_searchCancelled.load(std::memory_order_acquire)) {
        return;
    }

    // Split filters
    std::vector<std::string> filterList;
    std::stringstream ss(fileFilter);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        segment.erase(0, segment.find_first_not_of(" \t"));
        segment.erase(segment.find_last_not_of(" \t") + 1);
        if (!segment.empty()) filterList.push_back(segment);
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (m_searchCancelled.load(std::memory_order_acquire)) return;
            
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string filename = entry.path().filename().string();
                
                // Match filter
                bool matchesFilter = filterList.empty();
                for (const auto& filter : filterList) {
                    // Simple wildcard check
                    if (filename.find(filter) != std::string::npos) { // Very naive wildcard
                        matchesFilter = true;
                        break;
                    }
                }
                
                if (!matchesFilter) continue;
                
                // Search in file
                searchInFile(path, searchText, useRegex, caseSensitive);
            }
>>>>>>> origin/main
        }
    } catch (...) {}
}

<<<<<<< HEAD
        if (caseSensitive) {
            size_t p = line.find(searchText, startPos);
            return (p == std::string::npos)
                ? std::pair<size_t, size_t>{std::string::npos, (size_t)0}
                : std::pair<size_t, size_t>{p, (size_t)searchText.size()};
        }

        std::string h = line;
        std::string n = searchText;
        std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        size_t p = h.find(n, startPos);
        return (p == std::string::npos)
            ? std::pair<size_t, size_t>{std::string::npos, (size_t)0}
            : std::pair<size_t, size_t>{p, (size_t)searchText.size()};
    };

    const auto gitignorePatterns = loadGitignorePatterns(rootPath);

    try {
        for (const auto& e : fs::recursive_directory_iterator(rootPath,
                fs::directory_options::skip_permission_denied, ec)) {
            if (m_searchCancelled) break;
            if (!e.is_regular_file(ec)) continue;

            const std::string path = e.path().string();
            const std::string name = e.path().filename().string();

            if (isIgnored(path, gitignorePatterns)) continue;
            if (!globMatchExtList(name, fileFilter.empty() ? "*" : fileFilter)) continue;

            filesSearched++;
            if (m_progressCb) m_progressCb(m_progressCtx, filesSearched, m_totalResultCount);

            std::ifstream f(path, std::ios::binary);
            if (!f) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(f, line) && !m_searchCancelled) {
                lineNum++;
                size_t pos = 0;
                while (pos < line.size() && !m_searchCancelled) {
                    auto [col, len] = findMatch(line, pos);
                    if (col == std::string::npos) break;

                    std::string matched = line.substr(col, len ? len : 1);
                    MultiFileSearchResult r(path, lineNum, (int)col + 1, line, matched);

                    {
                        std::lock_guard<std::mutex> lock(m_resultsMutex);
                        m_pendingResults.push_back(r);
                        m_totalResultCount = (int)m_totalResultCount + 1;
                    }

                    if (m_progressCb) m_progressCb(m_progressCtx, filesSearched, m_totalResultCount);
                    pos = col + (len ? len : 1);
=======
void MultiFileSearchWidget::searchInFile(const std::string& filePath, const std::string& searchText, 
                                   bool useRegex, bool caseSensitive) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        bool found = false;
        if (useRegex) {
            try {
                std::regex_constants::syntax_option_type flags = std::regex::ECMAScript;
                if (!caseSensitive) flags |= std::regex::icase;
                std::regex re(searchText, flags);
                if (std::regex_search(line, re)) found = true;
            } catch (...) {}
        } else {
            auto it = std::search(
                line.begin(), line.end(),
                searchText.begin(), searchText.end(),
                [caseSensitive](char a, char b) {
                    return caseSensitive ? (a == b) : (::tolower(a) == ::tolower(b));
>>>>>>> origin/main
                }
            );
            if (it != line.end()) found = true;
        }
        
        if (found) {
            // Report result via callback
            if (onSearchResultReady) {
                SearchResult res;
                res.filePath = filePath;
                res.lineNumber = lineNum;
                res.lineText = line;
                onSearchResultReady(res);
            }
        }
    } catch (...) {
        // Best-effort search; ignore filesystem exceptions.
    }

    updateStatus("Found " + std::to_string(m_totalResultCount) +
                 " matches in " + std::to_string(filesSearched) + " files.");
    onSearchFinished();
}

<<<<<<< HEAD
namespace {
static std::string globToRegex(const std::string& glob) {
    std::string re;
    re.reserve(glob.size() * 2);
    for (char c : glob) {
        if (c == '*') re += ".*";
        else if (c == '?') re += '.';
        else if (c == '.' || c == '[' || c == ']' || c == '(' || c == ')' || c == '+' || c == '^' ||
                 c == '$' || c == '|' || c == '\\' || c == '{' || c == '}') {
            re += '\\';
            re += c;
        } else {
            re += c;
=======
/**
 * @brief Parses .gitignore file and returns compiled regex patterns.
 */
std::vector<std::regex> MultiFileSearchWidget::loadGitignorePatterns(const std::string& rootPath)
{
    std::vector<std::regex> patterns;

    std::string gitignorePath = rootPath + "/.gitignore";
    std::fstream gitignoreFile(gitignorePath);

    if (!gitignoreFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return patterns;
    }

    QTextStream stream(&gitignoreFile);
    while (!stream.atEnd()) {
        std::string line = stream.readLine().trimmed();

        // Skip comments and empty lines
        if (line.empty() || line.startsWith('#')) {
            continue;
        }

        // Convert gitignore glob pattern to regex
        std::string regexPattern = std::regex::wildcardToRegularExpression(line);
        std::regex regex(regexPattern);

        if (regex.isValid()) {
            patterns.append(regex);
>>>>>>> origin/main
        }
    }
    return re;
}
} // namespace

std::vector<std::regex> MultiFileSearchWidget::loadGitignorePatterns(const std::string& rootPath) {
    std::vector<std::regex> patterns;

    std::string gitignore = rootPath.empty() ? ".gitignore" : (rootPath + "/.gitignore");
    std::ifstream f(gitignore);
    if (!f) return patterns;

    std::string line;
    while (std::getline(f, line)) {
        size_t n = line.find_first_not_of(" \t\r");
        if (n != std::string::npos) line = line.substr(n);
        if (line.empty() || line[0] == '#') continue;
        try {
            patterns.emplace_back(globToRegex(line), std::regex::icase);
        } catch (...) {
        }
    }
    return patterns;
}

<<<<<<< HEAD
bool MultiFileSearchWidget::isIgnored(const std::string& path, const std::vector<std::regex>& patterns) {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    for (const auto& re : patterns) {
        try {
            if (std::regex_search(norm, re)) return true;
        } catch (...) {
=======
/**
 * @brief Checks if a file path matches any gitignore patterns.
 */
bool MultiFileSearchWidget::isIgnored(const std::string& filePath,
                                      const std::vector<std::regex>& patterns)
{
    for (const auto& pattern : patterns) {
        if (pattern.match(filePath).hasMatch()) {
            return true;
>>>>>>> origin/main
        }
    }
    return false;
}

void MultiFileSearchWidget::addResultToTree(const MultiFileSearchResult&) {}

void MultiFileSearchWidget::updateStatus(const std::string& message) {
#ifdef _WIN32
    if (m_statusLabel) {
        SetWindowTextA(m_statusLabel, message.c_str());
    }
#else
    (void)message;
#endif
}

// ============================================================================
// Minimal Win32 dialog implementation (optional)
// ============================================================================

#ifdef _WIN32
namespace {

static const wchar_t* kWndClass = L"RawrXD.MultiFileSearchDialog";
static constexpr UINT_PTR kPollTimer = 0xC0D3;

struct DialogState {
    MultiFileSearchWidget* widget = nullptr;
    HWND hwnd = nullptr;
    HWND edtQuery = nullptr;
    HWND edtFilter = nullptr;
    HWND chkRegex = nullptr;
    HWND chkCase = nullptr;
    HWND btnSearch = nullptr;
    HWND lst = nullptr;
    HWND lbl = nullptr;
    bool running = false;
};

static std::string w2u8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), len, nullptr, nullptr);
    return out;
}

static std::wstring u82w(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out((size_t)len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

static std::wstring getTextW(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s;
    s.resize((size_t)n);
    if (n > 0) GetWindowTextW(h, s.data(), n + 1);
    return s;
}

static void appendList(DialogState* st, const MultiFileSearchResult& r) {
    // Format: path(line:col): matched
    std::ostringstream oss;
    oss << r.file << "(" << r.line << ":" << r.column << "): " << r.matchedText;
    std::string line = oss.str();
    SendMessageW(st->lst, LB_ADDSTRING, 0, (LPARAM)u82w(line).c_str());
}

static void pollResults(DialogState* st) {
    auto batch = st->widget->takePendingResults();
    for (const auto& r : batch) appendList(st, r);

    char buf[128];
    sprintf_s(buf, "Matches: %d", st->widget->totalResultCount());
    SetWindowTextA(st->lbl, buf);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DialogState* st = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            st = reinterpret_cast<DialogState*>(cs->lpCreateParams);
            st->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

            HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            st->edtQuery = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 8, 8, 520, 24,
                hwnd, (HMENU)1001, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->edtQuery, WM_SETFONT, (WPARAM)font, TRUE);

            st->edtFilter = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"*.cpp,*.h,*.hpp,*.c,*.py,*.js,*.ts,*.json,*.md",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 8, 38, 520, 24,
                hwnd, (HMENU)1002, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->edtFilter, WM_SETFONT, (WPARAM)font, TRUE);

            st->chkRegex = CreateWindowW(L"BUTTON", L"Regex",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 540, 8, 120, 20,
                hwnd, (HMENU)1003, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->chkRegex, WM_SETFONT, (WPARAM)font, TRUE);

            st->chkCase = CreateWindowW(L"BUTTON", L"Case",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 540, 28, 120, 20,
                hwnd, (HMENU)1004, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->chkCase, WM_SETFONT, (WPARAM)font, TRUE);

            st->btnSearch = CreateWindowW(L"BUTTON", L"Search",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 540, 52, 120, 26,
                hwnd, (HMENU)1005, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->btnSearch, WM_SETFONT, (WPARAM)font, TRUE);

            st->lst = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL,
                8, 70, 652, 320, hwnd, (HMENU)1006, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->lst, WM_SETFONT, (WPARAM)font, TRUE);

            st->lbl = CreateWindowW(L"STATIC", L"Matches: 0",
                WS_CHILD | WS_VISIBLE, 8, 398, 300, 20,
                hwnd, (HMENU)1007, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(st->lbl, WM_SETFONT, (WPARAM)font, TRUE);

            SetTimer(hwnd, kPollTimer, 150, nullptr);
            return 0;
        }
        case WM_TIMER: {
            if (st && wParam == kPollTimer) pollResults(st);
            return 0;
        }
        case WM_COMMAND: {
            if (!st) break;
            if (LOWORD(wParam) == 1005) {
                // Start a new search
                st->widget->cancelSearch();
                st->widget->clearResults();
                SendMessageW(st->lst, LB_RESETCONTENT, 0, 0);

                std::wstring q = getTextW(st->edtQuery);
                std::wstring flt = getTextW(st->edtFilter);

                // Root defaults to current directory if Win32IDE didn't set one.
                std::string root = st->widget->projectRoot();
                if (root.empty()) {
                    wchar_t cwd[MAX_PATH] = {};
                    GetCurrentDirectoryW(MAX_PATH, cwd);
                    root = w2u8(cwd);
                }

                st->widget->setProjectRoot(root);
                st->widget->setSearchQuery(w2u8(q));
                st->widget->setFileFilter(w2u8(flt));
                st->widget->setUseRegex(SendMessageW(st->chkRegex, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->widget->setCaseSensitive(SendMessageW(st->chkCase, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->widget->startSearch();
            }
            return 0;
        }
        case WM_DESTROY: {
            if (st) {
                st->widget->cancelSearch();
            }
            KillTimer(hwnd, kPollTimer);
            PostQuitMessage(0);
            return 0;
        }
    }
<<<<<<< HEAD
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ensureClass() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    registered = true;
}

} // namespace

void MultiFileSearchWidget_ShowDialog(void* ctx) {
    auto* widget = reinterpret_cast<MultiFileSearchWidget*>(ctx);
    if (!widget) return;

    ensureClass();

    DialogState st;
    st.widget = widget;

    HWND hwnd = CreateWindowExW(0, kWndClass, L"Multi-File Search",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 690, 470,
        nullptr, nullptr, GetModuleHandleW(nullptr), &st);

    if (!hwnd) return;

    // Basic modal-ish loop (doesn't block the IDE main window thread if invoked from there).
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
#else
void MultiFileSearchWidget_ShowDialog(void*) {}
#endif
=======

    if (!fileItem) {
        fileItem = nullptr;
        fileItem->setText(0, result.file);
        fileItem->setData(0, //UserRole, result.file);
        fileItem->setFirstColumnSpanned(true);
        m_resultsTree->addTopLevelItem(fileItem);
    }

    // Add result as child item
    QTreeWidgetItem* resultItem = nullptr;
    resultItem->setText(0, std::string("Line %1:%2"));
    resultItem->setText(1, result.lineText);

    // Store result data in item for later retrieval
    resultItem->setData(0, //UserRole, result.file);
    resultItem->setData(0, //UserRole + 1, result.line);
    resultItem->setData(0, //UserRole + 2, result.column);
    resultItem->setData(0, //UserRole + 3, result.lineText);
    resultItem->setData(0, //UserRole + 4, result.matchedText);

    // Highlight the matched text in the context
    std::string highlightedText = result.lineText;
    int matchStart = result.lineText.indexOf(result.matchedText);
    if (matchStart != -1) {
        resultItem->setText(1, highlightedText);
    }
}

/**
 * @brief Updates the status label with current search state.
 */
void MultiFileSearchWidget::updateStatus(const std::string& message)
{
    m_statusLabel->setText(message);
}


>>>>>>> origin/main
