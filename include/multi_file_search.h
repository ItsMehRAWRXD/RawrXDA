#pragma once

#include <atomic>
#include <future>
#include <functional>
#include <mutex>
#include <regex>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
using HWND = void*;
#endif

#include "file_manager.h"

class MultiFileSearchWidget {
public:
    using ShowCallback = void(*)(void* ctx);
    using SearchCompletedCallback = void(*)(void* ctx, int totalResults);
    using SearchProgressCallback = void(*)(void* ctx, int filesSearched, int matchesFound);
    using ResultClickedCallback = void(*)(void* ctx, const MultiFileSearchResult& result);

    MultiFileSearchWidget() = default;
    ~MultiFileSearchWidget();

    void setProjectRoot(const std::string& path);
    const std::string& projectRoot() const { return m_projectRoot; }

    void setSearchQuery(const std::string& query);
    std::string searchQuery() const;

    void setFileFilter(const std::string& filter) { m_fileFilter = filter; }
    const std::string& fileFilter() const { return m_fileFilter; }

    void setUseRegex(bool enabled) { m_useRegex = enabled; }
    bool useRegex() const { return m_useRegex; }

    void setCaseSensitive(bool enabled) { m_caseSensitive = enabled; }
    bool caseSensitive() const { return m_caseSensitive; }

    bool isSearching() const { return m_isSearching; }
    int totalResultCount() const { return m_totalResultCount; }

    void setShowCallback(ShowCallback cb, void* ctx) { m_showCb = cb; m_showCtx = ctx; }
    void setCompletedCallback(SearchCompletedCallback cb, void* ctx) { m_completedCb = cb; m_completedCtx = ctx; }
    void setProgressCallback(SearchProgressCallback cb, void* ctx) { m_progressCb = cb; m_progressCtx = ctx; }
    void setResultClickedCallback(ResultClickedCallback cb, void* ctx) { m_resultClickedCb = cb; m_resultClickedCtx = ctx; }

    void show();
    void startSearch();
    void cancelSearch();
    void clearResults();
    void focusSearchInput();

    std::vector<MultiFileSearchResult> takePendingResults();

    void onResultItemDoubleClicked(void* item, int column);
    void onSearchResultsReady();
    void onSearchFinished();

private:
    void performSearch(const std::string& searchText,
                       const std::string& rootPath,
                       bool useRegex,
                       bool caseSensitive,
                       const std::string& fileFilter);

    std::vector<std::regex> loadGitignorePatterns(const std::string& rootPath);
    bool isIgnored(const std::string& filePath, const std::vector<std::regex>& patterns);
    void addResultToTree(const MultiFileSearchResult& result);
    void updateStatus(const std::string& message);

    HWND m_searchInput = nullptr;
    HWND m_statusLabel = nullptr;

    std::string m_projectRoot;
    std::string m_searchQuery;
    std::string m_fileFilter = "*.cpp,*.h,*.hpp,*.c,*.py,*.js,*.ts,*.json,*.md";
    std::atomic<bool> m_searchCancelled{false};
    bool m_isSearching = false;
    bool m_useRegex = false;
    bool m_caseSensitive = false;
    std::future<void> m_searchFuture;

    mutable std::mutex m_resultsMutex;
    std::vector<MultiFileSearchResult> m_pendingResults;
    int m_totalResultCount = 0;

    ShowCallback m_showCb = nullptr;
    void* m_showCtx = nullptr;
    SearchCompletedCallback m_completedCb = nullptr;
    void* m_completedCtx = nullptr;
    SearchProgressCallback m_progressCb = nullptr;
    void* m_progressCtx = nullptr;
    ResultClickedCallback m_resultClickedCb = nullptr;
    void* m_resultClickedCtx = nullptr;
};

void MultiFileSearchWidget_ShowDialog(void* ctx);
