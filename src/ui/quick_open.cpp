#include "ui/quick_open.h"
#include <filesystem>
#include <fstream>

namespace RawrXD::UI {

QuickOpen::QuickOpen() = default;
QuickOpen::~QuickOpen() = default;

void QuickOpen::initialize() {
    // Initialize quick open
}

void QuickOpen::shutdown() {
    clearIndex();
}

void QuickOpen::indexWorkspace(const std::string& rootPath) {
    m_items.clear();
    m_indexing = true;
    m_indexProgress = 0.0f;

    indexDirectory(rootPath);

    m_indexing = false;
    m_indexProgress = 1.0f;
}

void QuickOpen::addToIndex(const std::string& path) {
    QuickOpenItem item;
    item.path = path;
    item.name = path.substr(path.find_last_of("/\\") + 1);
    item.directory = path.substr(0, path.find_last_of("/\\"));
    item.extension = path.substr(path.find_last_of('.') + 1);
    item.score = 0.0f;
    item.isRecent = false;

    m_items.push_back(item);
}

void QuickOpen::removeFromIndex(const std::string& path) {
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
        [&path](const QuickOpenItem& item) { return item.path == path; }), m_items.end());
}

void QuickOpen::clearIndex() {
    m_items.clear();
    m_indexProgress = 0.0f;
}

bool QuickOpen::isIndexing() const {
    return m_indexing;
}

float QuickOpen::getIndexingProgress() const {
    return m_indexProgress;
}

std::future<void> QuickOpen::indexWorkspaceAsync(const std::string& rootPath) {
    return std::async(std::launch::async, [this, rootPath]() {
        indexWorkspace(rootPath);
    });
}

std::vector<QuickOpenItem> QuickOpen::query(const std::string& input, size_t limit) {
    m_input = input;
    updateResults();

    if (m_results.size() > limit) {
        return std::vector<QuickOpenItem>(m_results.begin(), m_results.begin() + limit);
    }
    return m_results;
}

std::vector<QuickOpenItem> QuickOpen::queryFiles(const std::string& input) {
    return query(input, m_maxResults);
}

std::vector<QuickOpenItem> QuickOpen::queryRecent(const std::string& input) {
    std::vector<QuickOpenItem> results;

    for (const auto& path : m_recentFiles) {
        if (path.find(input) != std::string::npos || input.empty()) {
            QuickOpenItem item;
            item.path = path;
            item.name = path.substr(path.find_last_of("/\\") + 1);
            item.isRecent = true;
            results.push_back(item);
        }
    }

    return results;
}

void QuickOpen::show() {
    m_visible = true;
    if (m_visibilityCallback) {
        m_visibilityCallback(true);
    }
}

void QuickOpen::hide() {
    m_visible = false;
    if (m_visibilityCallback) {
        m_visibilityCallback(false);
    }
}

void QuickOpen::toggle() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

bool QuickOpen::isVisible() const {
    return m_visible;
}

void QuickOpen::setInput(const std::string& input) {
    m_input = input;
    updateResults();
}

std::string QuickOpen::getInput() const {
    return m_input;
}

void QuickOpen::clearInput() {
    m_input.clear();
    m_results.clear();
    m_selectedIndex = 0;
}

void QuickOpen::selectNext() {
    if (m_selectedIndex + 1 < m_results.size()) {
        m_selectedIndex++;
    }
}

void QuickOpen::selectPrevious() {
    if (m_selectedIndex > 0) {
        m_selectedIndex--;
    }
}

void QuickOpen::selectFirst() {
    m_selectedIndex = 0;
}

void QuickOpen::selectLast() {
    if (!m_results.empty()) {
        m_selectedIndex = m_results.size() - 1;
    }
}

std::optional<QuickOpenItem> QuickOpen::getSelected() const {
    if (m_selectedIndex < m_results.size()) {
        return m_results[m_selectedIndex];
    }
    return std::nullopt;
}

void QuickOpen::setSelectedIndex(size_t index) {
    if (index < m_results.size()) {
        m_selectedIndex = index;
    }
}

void QuickOpen::setMode(QuickOpenMode mode) {
    m_mode = mode;
}

QuickOpenMode QuickOpen::getMode() const {
    return m_mode;
}

void QuickOpen::addRecentFile(const std::string& path) {
    // Remove if already exists
    m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), path), m_recentFiles.end());
    m_recentFiles.insert(m_recentFiles.begin(), path);

    // Keep only last 20
    if (m_recentFiles.size() > 20) {
        m_recentFiles.resize(20);
    }
}

void QuickOpen::removeRecentFile(const std::string& path) {
    m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), path), m_recentFiles.end());
}

void QuickOpen::clearRecentFiles() {
    m_recentFiles.clear();
}

void QuickOpen::setIncludeHidden(bool include) {
    m_includeHidden = include;
}

void QuickOpen::setRespectGitignore(bool respect) {
    m_respectGitignore = respect;
}

void QuickOpen::setMaxResults(size_t max) {
    m_maxResults = max;
}

void QuickOpen::setFuzzyMatching(bool enabled) {
    m_fuzzyMatching = enabled;
}

void QuickOpen::onFileOpened(FileOpenCallback callback) {
    m_openCallback = callback;
}

void QuickOpen::onVisibilityChanged(VisibilityCallback callback) {
    m_visibilityCallback = callback;
}

float QuickOpen::calculateScore(const std::string& query, const QuickOpenItem& item) {
    if (m_fuzzyMatching) {
        return calculateFuzzyScore(query, item.name);
    }

    // Simple substring matching
    if (item.name.find(query) != std::string::npos) {
        return 1.0f;
    }
    return 0.0f;
}

float QuickOpen::calculateFuzzyScore(const std::string& query, const std::string& target) {
    if (query.empty()) return 1.0f;
    if (target.empty()) return 0.0f;

    // Simple fuzzy matching algorithm
    size_t queryIdx = 0;
    size_t targetIdx = 0;
    int matches = 0;

    while (queryIdx < query.length() && targetIdx < target.length()) {
        if (tolower(query[queryIdx]) == tolower(target[targetIdx])) {
            matches++;
            queryIdx++;
        }
        targetIdx++;
    }

    if (queryIdx < query.length()) return 0.0f;

    return static_cast<float>(matches) / static_cast<float>(query.length());
}

std::vector<std::pair<int, int>> QuickOpen::findMatches(const std::string& query, const std::string& target) {
    std::vector<std::pair<int, int>> matches;

    size_t queryIdx = 0;
    for (size_t i = 0; i < target.length() && queryIdx < query.length(); ++i) {
        if (tolower(target[i]) == tolower(query[queryIdx])) {
            matches.push_back({static_cast<int>(i), static_cast<int>(i + 1)});
            queryIdx++;
        }
    }

    return matches;
}

void QuickOpen::updateResults() {
    m_results.clear();

    for (const auto& item : m_items) {
        float score = calculateScore(m_input, item);
        if (score > 0.0f) {
            QuickOpenItem result = item;
            result.score = score;
            result.matches = findMatches(m_input, item.name);
            m_results.push_back(result);
        }
    }

    // Sort by score
    std::sort(m_results.begin(), m_results.end(),
        [](const QuickOpenItem& a, const QuickOpenItem& b) {
            return a.score > b.score;
        });

    m_selectedIndex = 0;
}

void QuickOpen::indexDirectory(const std::string& path) {
    namespace fs = std::filesystem;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (m_indexing && !m_indexing) {
                return; // Cancelled
            }

            if (shouldIgnore(entry.path().string())) {
                continue;
            }

            if (entry.is_regular_file()) {
                addToIndex(entry.path().string());
            }
        }
    } catch (...) {
        // Ignore errors
    }
}

bool QuickOpen::shouldIgnore(const std::string& path) {
    // Check gitignore if enabled
    if (m_respectGitignore) {
        // Check .gitignore patterns
    }

    // Check hidden files
    if (!m_includeHidden) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string name = path.substr(pos + 1);
            if (!name.empty() && name[0] == '.') {
                return true;
            }
        }
    }

    return false;
}

// Global instance
QuickOpen& getQuickOpen() {
    static QuickOpen quickOpen;
    return quickOpen;
}

} // namespace RawrXD::UI
