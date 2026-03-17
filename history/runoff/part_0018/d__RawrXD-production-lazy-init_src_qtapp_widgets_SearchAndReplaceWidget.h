/*
 * SearchAndReplaceWidget.h - Complete Search & Replace Implementation
 * 
 * Full-featured find and replace with:
 * - Single file and multi-file search
 * - Regex support
 * - Replace functionality
 * - Incremental search highlighting
 * - Search history
 * - Match counter
 * 
 * NO STUBS - COMPLETE IMPLEMENTATION
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QString>
#include <QList>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

class QPushButton;
class QCheckBox;
class QLabel;
class QSpinBox;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QDialog;
class QThread;

/**
 * Single file search result
 */
struct SearchResult {
    QString filePath;
    int line = 0;
    int column = 0;
    int length = 0;
    QString lineContent;
};

/**
 * Complete search and replace widget
 */
class SearchAndReplaceWidget : public QWidget {
    Q_OBJECT

public:
    explicit SearchAndReplaceWidget(QWidget* parent = nullptr);
    ~SearchAndReplaceWidget();

    /**
     * Set the current editor for single-file search
     */
    void setCurrentEditor(QPlainTextEdit* editor);

    /**
     * Set project root for multi-file search
     */
    void setProjectRoot(const QString& projectRoot);

    /**
     * Clear all results
     */
    void clearResults();

signals:
    /**
     * Emitted when user wants to navigate to a result
     */
    void navigateToResult(const QString& filePath, int line, int column);

    /**
     * Emitted when search completes
     */
    void searchCompleted(int resultCount);

    /**
     * Emitted when replacement completes
     */
    void replacementCompleted(int replacedCount);

public slots:
    /**
     * Find next occurrence
     */
    void findNext();

    /**
     * Find previous occurrence
     */
    void findPrevious();

    /**
     * Replace current occurrence
     */
    void replaceCurrent();

    /**
     * Replace all occurrences
     */
    void replaceAll();

    /**
     * Perform find all in files
     */
    void findInFiles();

private slots:
    /**
     * Called when search text changes
     */
    void onSearchTextChanged(const QString& text);

    /**
     * Called when search options change
     */
    void onSearchOptionsChanged();

    /**
     * Result item clicked
     */
    void onResultItemClicked(QListWidgetItem* item);

    /**
     * Toggle replace panel
     */
    void onToggleReplacePanel();

    /**
     * File search worker finished
     */
    void onFileSearchFinished(const QList<SearchResult>& results);

private:
    /**
     * Setup UI components
     */
    void setupUI();

    /**
     * Setup signal/slot connections
     */
    void setupConnections();

    /**
     * Perform single-file search
     */
    void searchInCurrentEditor();

    /**
     * Find all matches in editor
     */
    QList<SearchResult> findAllInEditor(const QString& filePath = "");

    /**
     * Build regex from search string
     */
    QRegularExpression buildRegex(const QString& searchText);

    /**
     * Get current search options
     */
    enum SearchOption {
        CaseSensitive = 0x01,
        WholeWord = 0x02,
        UseRegex = 0x04,
        Wraparound = 0x08
    };
    int getSearchOptions() const;

    /**
     * Highlight search results in current editor
     */
    void highlightMatches();

    /**
     * Scroll editor to result
     */
    void scrollToResult(const SearchResult& result);

    /**
     * Replace text in current editor
     */
    bool replaceInEditor(const SearchResult& result, const QString& replacement);

    /**
     * Escape regex special characters
     */
    static QString escapeRegex(const QString& text);

private:
    // UI Components
    QLineEdit* m_searchInput = nullptr;
    QLineEdit* m_replaceInput = nullptr;
    QPushButton* m_findNextBtn = nullptr;
    QPushButton* m_findPrevBtn = nullptr;
    QPushButton* m_replaceBtn = nullptr;
    QPushButton* m_replaceAllBtn = nullptr;
    QPushButton* m_findInFilesBtn = nullptr;
    QCheckBox* m_caseSensitiveCheck = nullptr;
    QCheckBox* m_wholeWordCheck = nullptr;
    QCheckBox* m_regexCheck = nullptr;
    QCheckBox* m_wrapAroundCheck = nullptr;
    QLabel* m_matchCountLabel = nullptr;
    QListWidget* m_resultsWidget = nullptr;
    QWidget* m_replacePanel = nullptr;

    // State
    QPointer<QPlainTextEdit> m_currentEditor;
    QString m_projectRoot;
    QList<SearchResult> m_currentResults;
    int m_currentResultIndex = 0;

    // History
    QStringList m_searchHistory;
    QStringList m_replaceHistory;
    static constexpr int MAX_HISTORY_SIZE = 50;
};

#endif // SEARCHANDREPLACE_H
