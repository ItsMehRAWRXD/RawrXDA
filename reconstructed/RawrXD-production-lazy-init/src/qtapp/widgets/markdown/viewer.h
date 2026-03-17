/**
 * @file markdown_viewer.h
 * @brief Full Markdown Viewer/Editor widget for RawrXD IDE
 * @author RawrXD Team
 * 
 * Provides comprehensive markdown support including:
 * - Live preview with split view
 * - GitHub Flavored Markdown support
 * - Syntax highlighting for code blocks
 * - Table of contents generation
 * - Image preview and embedding
 * - Export to HTML/PDF
 * - Math equation rendering (KaTeX)
 * - Mermaid diagram support
 */

#pragma once

#include <QWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QTextBrowser>
#include <QToolBar>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QMap>
#include <functional>

/**
 * @brief Table of contents entry
 */
struct TocEntry {
    int level = 1;
    QString text;
    QString anchor;
    int lineNumber = 0;
};

/**
 * @brief Markdown rendering options
 */
struct MarkdownOptions {
    bool enableGfm = true;              // GitHub Flavored Markdown
    bool enableTables = true;
    bool enableTaskLists = true;
    bool enableStrikethrough = true;
    bool enableAutolinks = true;
    bool enableSyntaxHighlight = true;
    bool enableMath = true;             // KaTeX math
    bool enableMermaid = true;          // Diagrams
    bool enableEmoji = true;
    bool enableFootnotes = true;
    bool sanitizeHtml = true;
    QString codeTheme = "monokai";
};

/**
 * @class MarkdownViewer
 * @brief Full-featured markdown viewer and editor widget
 */
class MarkdownViewer : public QWidget {
    Q_OBJECT

public:
    explicit MarkdownViewer(QWidget* parent = nullptr);
    ~MarkdownViewer() override;

    // Content management
    void setMarkdown(const QString& markdown);
    QString getMarkdown() const;
    void setHtml(const QString& html);
    QString getHtml() const;
    void clear();
    
    // File operations
    bool loadFile(const QString& path);
    bool saveFile(const QString& path = QString());
    QString currentFile() const { return m_currentFile; }
    bool isModified() const;
    
    // View modes
    enum ViewMode { EditOnly, PreviewOnly, SplitView };
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }
    
    // Options
    void setOptions(const MarkdownOptions& options);
    MarkdownOptions options() const { return m_options; }
    
    // TOC
    QVector<TocEntry> getTableOfContents() const;
    void scrollToHeading(const QString& anchor);
    
    // Export
    void exportToHtml(const QString& path);
    void exportToPdf(const QString& path);
    void copyHtmlToClipboard();
    
    // Editor access
    QTextEdit* editor() { return m_editor; }
    QTextBrowser* preview() { return m_preview; }

signals:
    void contentChanged();
    void fileLoaded(const QString& path);
    void fileSaved(const QString& path);
    void tocUpdated(const QVector<TocEntry>& toc);
    void linkClicked(const QUrl& url);
    void cursorPositionChanged(int line, int column);

public slots:
    void updatePreview();
    void syncScrollFromEditor();
    void syncScrollFromPreview();
    void insertBold();
    void insertItalic();
    void insertStrikethrough();
    void insertLink();
    void insertImage();
    void insertCodeBlock();
    void insertTable();
    void insertHeading(int level);
    void insertBulletList();
    void insertNumberedList();
    void insertTaskList();
    void insertHorizontalRule();
    void insertBlockquote();
    void togglePreview();

private slots:
    void onEditorTextChanged();
    void onLinkClicked(const QUrl& url);
    void onFileChanged(const QString& path);

private:
    void setupUI();
    void setupToolbar();
    void setupEditor();
    void setupPreview();
    void connectSignals();
    
    // Markdown parsing
    QString renderMarkdown(const QString& markdown);
    QString parseInlineFormatting(const QString& text);
    QString parseCodeBlocks(const QString& text);
    QString parseTables(const QString& text);
    QString parseHeadings(const QString& text);
    QString parseLists(const QString& text);
    QString parseLinks(const QString& text);
    QString parseImages(const QString& text);
    QString parseBlockquotes(const QString& text);
    QString parseTaskLists(const QString& text);
    QString parseHorizontalRules(const QString& text);
    QString parseEmoji(const QString& text);
    QString parseMath(const QString& text);
    QString parseMermaid(const QString& text);
    
    // Syntax highlighting for code blocks
    QString highlightCode(const QString& code, const QString& language);
    QString getCodeHighlightCss();
    
    // TOC generation
    void updateToc(const QString& html);
    QString generateTocHtml();
    
    // CSS for preview
    QString getPreviewCss();
    QString getFullHtml(const QString& bodyHtml);

private:
    // UI Components
    QToolBar* m_toolbar = nullptr;
    QSplitter* m_splitter = nullptr;
    QTextEdit* m_editor = nullptr;
    QTextBrowser* m_preview = nullptr;
    
    // Toolbar buttons
    QPushButton* m_boldBtn = nullptr;
    QPushButton* m_italicBtn = nullptr;
    QPushButton* m_strikeBtn = nullptr;
    QPushButton* m_linkBtn = nullptr;
    QPushButton* m_imageBtn = nullptr;
    QPushButton* m_codeBtn = nullptr;
    QPushButton* m_tableBtn = nullptr;
    QComboBox* m_headingCombo = nullptr;
    QPushButton* m_bulletBtn = nullptr;
    QPushButton* m_numberBtn = nullptr;
    QPushButton* m_taskBtn = nullptr;
    QPushButton* m_quoteBtn = nullptr;
    QPushButton* m_hrBtn = nullptr;
    QPushButton* m_previewBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    
    // State
    QString m_currentFile;
    ViewMode m_viewMode = SplitView;
    MarkdownOptions m_options;
    QVector<TocEntry> m_toc;
    bool m_syncScrolling = true;
    bool m_previewNeedsUpdate = false;
    
    // Timers
    QTimer* m_updateTimer = nullptr;
    
    // File watching
    QFileSystemWatcher* m_fileWatcher = nullptr;
    
    // Emoji map
    static QMap<QString, QString> s_emojiMap;
    static void initEmojiMap();
    
    // Settings
    QSettings* m_settings = nullptr;
};
