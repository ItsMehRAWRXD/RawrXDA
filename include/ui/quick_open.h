#pragma once
/**
 * @file quick_open.h
 * @brief Quick file open with fuzzy matching
 * Batch 4 - Item 56: Quick open
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::UI {

struct QuickOpenItem {
    std::string path;
    std::string name;
    std::string directory;
    std::string extension;
    float score;
    std::vector<std::pair<int, int>> matches;
    bool isRecent;
    std::chrono::system_clock::time_point lastOpened;
};

enum class QuickOpenMode {
    Files,
    Symbols,
    Commands,
    Everything
};

class QuickOpen {
public:
    QuickOpen();
    ~QuickOpen();

    // Initialization
    void initialize();
    void shutdown();

    // Indexing
    void indexWorkspace(const std::string& rootPath);
    void addToIndex(const std::string& path);
    void removeFromIndex(const std::string& path);
    void clearIndex();
    bool isIndexing() const;
    float getIndexingProgress() const;

    // Async indexing
    std::future<void> indexWorkspaceAsync(const std::string& rootPath);

    // Query
    std::vector<QuickOpenItem> query(const std::string& input, size_t limit = 50);
    std::vector<QuickOpenItem> queryFiles(const std::string& input);
    std::vector<QuickOpenItem> queryRecent(const std::string& input);

    // Display
    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    // Input
    void setInput(const std::string& input);
    std::string getInput() const;
    void clearInput();

    // Selection
    void selectNext();
    void selectPrevious();
    void selectFirst();
    void selectLast();
    std::optional<QuickOpenItem> getSelected() const;
    void setSelectedIndex(size_t index);

    // Mode
    void setMode(QuickOpenMode mode);
    QuickOpenMode getMode() const;

    // Recent files
    void addRecentFile(const std::string& path);
    void removeRecentFile(const std::string& path);
    void clearRecentFiles();

    // Configuration
    void setIncludeHidden(bool include);
    void setRespectGitignore(bool respect);
    void setMaxResults(size_t max);
    void setFuzzyMatching(bool enabled);

    // Events
    using FileOpenCallback = std::function<void(const std::string& path)>;
    using VisibilityCallback = std::function<void(bool visible)>;
    void onFileOpened(FileOpenCallback callback);
    void onVisibilityChanged(VisibilityCallback callback);

private:
    std::vector<QuickOpenItem> m_items;
    std::vector<QuickOpenItem> m_results;
    std::vector<std::string> m_recentFiles;
    std::string m_input;
    size_t m_selectedIndex{0};
    QuickOpenMode m_mode{QuickOpenMode::Files};
    bool m_visible{false};
    bool m_indexing{false};
    float m_indexProgress{0.0f};
    bool m_includeHidden{false};
    bool m_respectGitignore{true};
    size_t m_maxResults{50};
    bool m_fuzzyMatching{true};

    FileOpenCallback m_openCallback;
    VisibilityCallback m_visibilityCallback;

    float calculateScore(const std::string& query, const QuickOpenItem& item);
    float calculateFuzzyScore(const std::string& query, const std::string& target);
    std::vector<std::pair<int, int>> findMatches(const std::string& query, const std::string& target);
    void updateResults();
    void indexDirectory(const std::string& path);
    bool shouldIgnore(const std::string& path);
};

// Global instance
QuickOpen& getQuickOpen();

} // namespace RawrXD::UI
