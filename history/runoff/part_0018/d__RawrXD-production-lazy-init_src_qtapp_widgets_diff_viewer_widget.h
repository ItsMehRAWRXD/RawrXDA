/**
 * @file diff_viewer_widget.h
 * @brief Full Diff Viewer Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QToolBar>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTimer>
#include <QSettings>
#include <QSyntaxHighlighter>
#include <QPainter>

/**
 * @brief Types of diff changes
 */
enum class DiffChangeType {
    None,
    Added,
    Removed,
    Modified,
    Context
};

/**
 * @brief Structure representing a single diff hunk
 */
struct DiffHunk {
    int oldStart;
    int oldCount;
    int newStart;
    int newCount;
    QVector<QPair<DiffChangeType, QString>> lines;
};

/**
 * @brief Structure for a diff line
 */
struct DiffLine {
    DiffChangeType type;
    QString text;
    int oldLineNum = -1;
    int newLineNum = -1;
};

/**
 * @brief Structure for complete diff result
 */
struct DiffResult {
    QString oldFile;
    QString newFile;
    QVector<DiffHunk> hunks;
    int additions = 0;
    int deletions = 0;
    int changes = 0;
};

/**
 * @brief Line number area widget for diff editors
 */
class DiffLineNumberArea : public QWidget {
    Q_OBJECT

public:
    explicit DiffLineNumberArea(QPlainTextEdit* editor, bool isOld);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPlainTextEdit* m_editor;
    bool m_isOld;
};

/**
 * @brief Syntax highlighter for diff output
 */
class DiffSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit DiffSyntaxHighlighter(QTextDocument* parent = nullptr);
    void setLanguage(const QString& language);

protected:
    void highlightBlock(const QString& text) override;

private:
    QString m_language;
};

/**
 * @brief Custom text editor for diff viewing with line markers
 */
class DiffTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit DiffTextEdit(QWidget* parent = nullptr);
    
    void setDiffLines(const QVector<DiffLine>& lines);
    void lineNumberAreaPaintEvent(QPaintEvent* event, bool isOld);
    int lineNumberAreaWidth();
    
    void setShowLineNumbers(bool show);
    void setSyncScrolling(QPlainTextEdit* other);

signals:
    void scrollChanged(int value);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    DiffLineNumberArea* m_oldLineArea;
    DiffLineNumberArea* m_newLineArea;
    QVector<DiffLine> m_diffLines;
    bool m_showLineNumbers = true;
    QPlainTextEdit* m_syncTarget = nullptr;
};

/**
 * @brief Full Diff Viewer Widget
 * 
 * Features:
 * - Side-by-side diff view
 * - Inline/unified diff view
 * - Syntax highlighting for source language
 * - Line change markers (added/removed/modified)
 * - Navigation between changes
 * - Diff statistics
 * - Word-level diff highlighting
 * - Copy changes
 * - Apply/revert individual hunks
 * - Diff generation using Myers algorithm
 */
class DiffViewerWidget : public QWidget {
    Q_OBJECT

public:
    enum ViewMode {
        SideBySide,
        Unified,
        Inline
    };
    Q_ENUM(ViewMode)

    explicit DiffViewerWidget(QWidget* parent = nullptr);
    ~DiffViewerWidget();

    // Content management
    void setOldContent(const QString& content, const QString& title = "Original");
    void setNewContent(const QString& content, const QString& title = "Modified");
    void setContents(const QString& oldContent, const QString& newContent,
                     const QString& oldTitle = "Original", const QString& newTitle = "Modified");
    
    void loadFiles(const QString& oldPath, const QString& newPath);
    void loadFromGitDiff(const QString& gitDiffOutput);
    
    // View options
    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const { return m_viewMode; }
    
    void setLanguage(const QString& language);
    void setContextLines(int lines);
    void setShowLineNumbers(bool show);
    void setWordDiff(bool enabled);
    void setIgnoreWhitespace(bool ignore);
    
    // Results
    DiffResult getDiffResult() const { return m_diffResult; }
    int getAdditions() const { return m_diffResult.additions; }
    int getDeletions() const { return m_diffResult.deletions; }
    int getHunkCount() const { return m_diffResult.hunks.size(); }
    
    // Navigation
    int getCurrentHunk() const { return m_currentHunk; }
    void goToHunk(int index);

signals:
    void diffCalculated(const DiffResult& result);
    void hunkSelected(int index);
    void contentChanged();

public slots:
    void calculateDiff();
    void nextChange();
    void previousChange();
    void firstChange();
    void lastChange();
    void copyOldContent();
    void copyNewContent();
    void copyDiffOutput();
    void swapSides();
    void refresh();

private slots:
    void onOldScrollChanged(int value);
    void onNewScrollChanged(int value);
    void syncScrollBars();

private:
    void setupUI();
    void setupToolbar();
    void setupSideBySideView();
    void setupUnifiedView();
    void connectSignals();
    
    // Diff algorithm (Myers)
    QVector<DiffLine> computeDiff(const QStringList& oldLines, const QStringList& newLines);
    QVector<int> computeLCS(const QStringList& a, const QStringList& b);
    void computeWordDiff(DiffLine& oldLine, DiffLine& newLine);
    
    void updateViews();
    void updateStatistics();
    void updateHunkNavigation();
    QString generateUnifiedDiff();
    QString highlightWordChanges(const QString& oldText, const QString& newText, bool isOld);
    
    QColor getColorForType(DiffChangeType type, bool background = true) const;

private:
    // UI Components
    QToolBar* m_toolbar;
    QSplitter* m_splitter;
    QWidget* m_sideBySideWidget;
    QWidget* m_unifiedWidget;
    
    // Side-by-side view
    DiffTextEdit* m_oldEditor;
    DiffTextEdit* m_newEditor;
    QLabel* m_oldTitleLabel;
    QLabel* m_newTitleLabel;
    DiffSyntaxHighlighter* m_oldHighlighter;
    DiffSyntaxHighlighter* m_newHighlighter;
    
    // Unified view
    QPlainTextEdit* m_unifiedEditor;
    DiffSyntaxHighlighter* m_unifiedHighlighter;
    
    // Toolbar widgets
    QComboBox* m_viewModeCombo;
    QComboBox* m_contextLinesCombo;
    QPushButton* m_prevBtn;
    QPushButton* m_nextBtn;
    QLabel* m_hunkLabel;
    QLabel* m_statsLabel;
    QPushButton* m_swapBtn;
    QPushButton* m_refreshBtn;
    
    // State
    QString m_oldContent;
    QString m_newContent;
    QString m_oldTitle;
    QString m_newTitle;
    QString m_language;
    ViewMode m_viewMode = SideBySide;
    int m_contextLines = 3;
    bool m_showLineNumbers = true;
    bool m_wordDiff = true;
    bool m_ignoreWhitespace = false;
    bool m_syncingScroll = false;
    
    DiffResult m_diffResult;
    QVector<DiffLine> m_diffLines;
    QVector<int> m_hunkPositions;  // Line positions of each hunk
    int m_currentHunk = 0;
    
    QSettings* m_settings;
};
