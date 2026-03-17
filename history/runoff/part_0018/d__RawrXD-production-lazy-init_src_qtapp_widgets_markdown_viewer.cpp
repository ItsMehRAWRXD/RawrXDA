/**
 * @file markdown_viewer.cpp
 * @brief Full Markdown Viewer implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "markdown_viewer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QScrollBar>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
// #include <QPrinter>  // Removed - Not available in Qt 6.7.3
// #include <QPrintDialog>
#include <QTextDocument>
#include <QFont>
#include <QFontDatabase>

// Static emoji map
QMap<QString, QString> MarkdownViewer::s_emojiMap;

void MarkdownViewer::initEmojiMap() {
    if (!s_emojiMap.isEmpty()) return;
    
    s_emojiMap[":smile:"] = "😄";
    s_emojiMap[":laughing:"] = "😆";
    s_emojiMap[":blush:"] = "😊";
    s_emojiMap[":smiley:"] = "😃";
    s_emojiMap[":heart:"] = "❤️";
    s_emojiMap[":+1:"] = "👍";
    s_emojiMap[":thumbsup:"] = "👍";
    s_emojiMap[":-1:"] = "👎";
    s_emojiMap[":thumbsdown:"] = "👎";
    s_emojiMap[":star:"] = "⭐";
    s_emojiMap[":fire:"] = "🔥";
    s_emojiMap[":warning:"] = "⚠️";
    s_emojiMap[":bug:"] = "🐛";
    s_emojiMap[":rocket:"] = "🚀";
    s_emojiMap[":tada:"] = "🎉";
    s_emojiMap[":sparkles:"] = "✨";
    s_emojiMap[":check:"] = "✅";
    s_emojiMap[":x:"] = "❌";
    s_emojiMap[":question:"] = "❓";
    s_emojiMap[":exclamation:"] = "❗";
    s_emojiMap[":bulb:"] = "💡";
    s_emojiMap[":memo:"] = "📝";
    s_emojiMap[":book:"] = "📖";
    s_emojiMap[":link:"] = "🔗";
    s_emojiMap[":lock:"] = "🔒";
    s_emojiMap[":key:"] = "🔑";
    s_emojiMap[":wrench:"] = "🔧";
    s_emojiMap[":hammer:"] = "🔨";
    s_emojiMap[":gear:"] = "⚙️";
    s_emojiMap[":package:"] = "📦";
    s_emojiMap[":zap:"] = "⚡";
    s_emojiMap[":100:"] = "💯";
}

// =============================================================================
// MarkdownViewer Implementation
// =============================================================================

MarkdownViewer::MarkdownViewer(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
    , m_updateTimer(new QTimer(this))
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    initEmojiMap();
    setupUI();
    connectSignals();
    
    // Delayed preview update for performance
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(300);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownViewer::updatePreview);
}

MarkdownViewer::~MarkdownViewer() {
}

void MarkdownViewer::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    setupEditor();
    setupPreview();
    
    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_splitter);
}

void MarkdownViewer::setupToolbar() {
    m_toolbar = new QToolBar("Markdown Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // Heading selector
    m_headingCombo = new QComboBox(this);
    m_headingCombo->addItem("Paragraph", 0);
    m_headingCombo->addItem("Heading 1", 1);
    m_headingCombo->addItem("Heading 2", 2);
    m_headingCombo->addItem("Heading 3", 3);
    m_headingCombo->addItem("Heading 4", 4);
    m_headingCombo->addItem("Heading 5", 5);
    m_headingCombo->addItem("Heading 6", 6);
    connect(m_headingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        int level = m_headingCombo->itemData(index).toInt();
        if (level > 0) insertHeading(level);
    });
    m_toolbar->addWidget(m_headingCombo);
    m_toolbar->addSeparator();
    
    // Formatting buttons
    m_boldBtn = new QPushButton("B", this);
    m_boldBtn->setToolTip("Bold (Ctrl+B)");
    m_boldBtn->setShortcut(QKeySequence::Bold);
    m_boldBtn->setFont(QFont(m_boldBtn->font().family(), -1, QFont::Bold));
    m_boldBtn->setMaximumWidth(30);
    connect(m_boldBtn, &QPushButton::clicked, this, &MarkdownViewer::insertBold);
    m_toolbar->addWidget(m_boldBtn);
    
    m_italicBtn = new QPushButton("I", this);
    m_italicBtn->setToolTip("Italic (Ctrl+I)");
    m_italicBtn->setShortcut(QKeySequence::Italic);
    QFont italicFont = m_italicBtn->font();
    italicFont.setItalic(true);
    m_italicBtn->setFont(italicFont);
    m_italicBtn->setMaximumWidth(30);
    connect(m_italicBtn, &QPushButton::clicked, this, &MarkdownViewer::insertItalic);
    m_toolbar->addWidget(m_italicBtn);
    
    m_strikeBtn = new QPushButton("S̶", this);
    m_strikeBtn->setToolTip("Strikethrough");
    m_strikeBtn->setMaximumWidth(30);
    connect(m_strikeBtn, &QPushButton::clicked, this, &MarkdownViewer::insertStrikethrough);
    m_toolbar->addWidget(m_strikeBtn);
    
    m_toolbar->addSeparator();
    
    m_linkBtn = new QPushButton("🔗", this);
    m_linkBtn->setToolTip("Insert Link (Ctrl+K)");
    m_linkBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    m_linkBtn->setMaximumWidth(30);
    connect(m_linkBtn, &QPushButton::clicked, this, &MarkdownViewer::insertLink);
    m_toolbar->addWidget(m_linkBtn);
    
    m_imageBtn = new QPushButton("🖼", this);
    m_imageBtn->setToolTip("Insert Image");
    m_imageBtn->setMaximumWidth(30);
    connect(m_imageBtn, &QPushButton::clicked, this, &MarkdownViewer::insertImage);
    m_toolbar->addWidget(m_imageBtn);
    
    m_codeBtn = new QPushButton("</>" , this);
    m_codeBtn->setToolTip("Insert Code Block");
    m_codeBtn->setMaximumWidth(40);
    connect(m_codeBtn, &QPushButton::clicked, this, &MarkdownViewer::insertCodeBlock);
    m_toolbar->addWidget(m_codeBtn);
    
    m_tableBtn = new QPushButton("⊞", this);
    m_tableBtn->setToolTip("Insert Table");
    m_tableBtn->setMaximumWidth(30);
    connect(m_tableBtn, &QPushButton::clicked, this, &MarkdownViewer::insertTable);
    m_toolbar->addWidget(m_tableBtn);
    
    m_toolbar->addSeparator();
    
    m_bulletBtn = new QPushButton("•", this);
    m_bulletBtn->setToolTip("Bullet List");
    m_bulletBtn->setMaximumWidth(30);
    connect(m_bulletBtn, &QPushButton::clicked, this, &MarkdownViewer::insertBulletList);
    m_toolbar->addWidget(m_bulletBtn);
    
    m_numberBtn = new QPushButton("1.", this);
    m_numberBtn->setToolTip("Numbered List");
    m_numberBtn->setMaximumWidth(30);
    connect(m_numberBtn, &QPushButton::clicked, this, &MarkdownViewer::insertNumberedList);
    m_toolbar->addWidget(m_numberBtn);
    
    m_taskBtn = new QPushButton("☐", this);
    m_taskBtn->setToolTip("Task List");
    m_taskBtn->setMaximumWidth(30);
    connect(m_taskBtn, &QPushButton::clicked, this, &MarkdownViewer::insertTaskList);
    m_toolbar->addWidget(m_taskBtn);
    
    m_toolbar->addSeparator();
    
    m_quoteBtn = new QPushButton("❝", this);
    m_quoteBtn->setToolTip("Blockquote");
    m_quoteBtn->setMaximumWidth(30);
    connect(m_quoteBtn, &QPushButton::clicked, this, &MarkdownViewer::insertBlockquote);
    m_toolbar->addWidget(m_quoteBtn);
    
    m_hrBtn = new QPushButton("—", this);
    m_hrBtn->setToolTip("Horizontal Rule");
    m_hrBtn->setMaximumWidth(30);
    connect(m_hrBtn, &QPushButton::clicked, this, &MarkdownViewer::insertHorizontalRule);
    m_toolbar->addWidget(m_hrBtn);
    
    m_toolbar->addSeparator();
    
    m_previewBtn = new QPushButton("Preview", this);
    m_previewBtn->setToolTip("Toggle Preview (Ctrl+Shift+V)");
    m_previewBtn->setCheckable(true);
    m_previewBtn->setChecked(true);
    connect(m_previewBtn, &QPushButton::clicked, this, &MarkdownViewer::togglePreview);
    m_toolbar->addWidget(m_previewBtn);
    
    m_exportBtn = new QPushButton("Export", this);
    QMenu* exportMenu = new QMenu(this);
    exportMenu->addAction("Export to HTML...", this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export to HTML",
            m_currentFile.isEmpty() ? "document.html" : m_currentFile + ".html",
            "HTML Files (*.html)");
        if (!path.isEmpty()) exportToHtml(path);
    });
    exportMenu->addAction("Export to PDF...", this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export to PDF",
            m_currentFile.isEmpty() ? "document.pdf" : m_currentFile + ".pdf",
            "PDF Files (*.pdf)");
        if (!path.isEmpty()) exportToPdf(path);
    });
    exportMenu->addAction("Copy HTML to Clipboard", this, &MarkdownViewer::copyHtmlToClipboard);
    m_exportBtn->setMenu(exportMenu);
    m_toolbar->addWidget(m_exportBtn);
}

void MarkdownViewer::setupEditor() {
    m_editor = new QTextEdit(this);
    m_editor->setAcceptRichText(false);
    m_editor->setFont(QFont("Consolas", 11));
    m_editor->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "selection-background-color: #264f78; border: none; }");
    m_editor->setPlaceholderText("Write your Markdown here...\n\n"
        "# Heading 1\n## Heading 2\n\n"
        "**bold** *italic* ~~strikethrough~~\n\n"
        "- Bullet list\n1. Numbered list\n- [ ] Task list\n\n"
        "[Link](url) ![Image](url)\n\n"
        "```language\ncode block\n```");
}

void MarkdownViewer::setupPreview() {
    m_preview = new QTextBrowser(this);
    m_preview->setOpenExternalLinks(false);
    m_preview->setStyleSheet("QTextBrowser { border: none; }");
    
    connect(m_preview, &QTextBrowser::anchorClicked, this, &MarkdownViewer::onLinkClicked);
}

void MarkdownViewer::connectSignals() {
    connect(m_editor, &QTextEdit::textChanged, this, &MarkdownViewer::onEditorTextChanged);
    
    // Sync scrolling
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownViewer::syncScrollFromEditor);
    
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MarkdownViewer::onFileChanged);
}

// =============================================================================
// Content Management
// =============================================================================

void MarkdownViewer::setMarkdown(const QString& markdown) {
    m_editor->setPlainText(markdown);
    updatePreview();
}

QString MarkdownViewer::getMarkdown() const {
    return m_editor->toPlainText();
}

void MarkdownViewer::setHtml(const QString& html) {
    m_preview->setHtml(html);
}

QString MarkdownViewer::getHtml() const {
    // Create a non-const copy to use for rendering
    QString markdown = m_editor->toPlainText();
    MarkdownViewer* nonConstThis = const_cast<MarkdownViewer*>(this);
    return nonConstThis->renderMarkdown(markdown);
}

void MarkdownViewer::clear() {
    m_editor->clear();
    m_preview->clear();
    m_currentFile.clear();
    m_toc.clear();
}

bool MarkdownViewer::loadFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", 
            QString("Could not open file: %1").arg(file.errorString()));
        return false;
    }
    
    QTextStream in(&file);
    // Qt6: setCodec removed, UTF-8 is default
    QString content = in.readAll();
    file.close();
    
    // Stop watching old file
    if (!m_currentFile.isEmpty()) {
        m_fileWatcher->removePath(m_currentFile);
    }
    
    m_currentFile = path;
    m_editor->setPlainText(content);
    m_editor->document()->setModified(false);
    
    // Watch for external changes
    m_fileWatcher->addPath(path);
    
    updatePreview();
    emit fileLoaded(path);
    return true;
}

bool MarkdownViewer::saveFile(const QString& path) {
    QString savePath = path.isEmpty() ? m_currentFile : path;
    
    if (savePath.isEmpty()) {
        savePath = QFileDialog::getSaveFileName(this, "Save Markdown",
            "document.md", "Markdown Files (*.md *.markdown);;All Files (*)");
        if (savePath.isEmpty()) return false;
    }
    
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
            QString("Could not save file: %1").arg(file.errorString()));
        return false;
    }
    
    QTextStream out(&file);
    // Qt6: setCodec removed, UTF-8 is default
    out << m_editor->toPlainText();
    file.close();
    
    m_currentFile = savePath;
    m_editor->document()->setModified(false);
    
    emit fileSaved(savePath);
    return true;
}

bool MarkdownViewer::isModified() const {
    return m_editor->document()->isModified();
}

// =============================================================================
// View Modes
// =============================================================================

void MarkdownViewer::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    
    switch (mode) {
        case EditOnly:
            m_editor->show();
            m_preview->hide();
            break;
        case PreviewOnly:
            m_editor->hide();
            m_preview->show();
            break;
        case SplitView:
            m_editor->show();
            m_preview->show();
            break;
    }
    
    m_previewBtn->setChecked(mode != EditOnly);
}

void MarkdownViewer::togglePreview() {
    if (m_viewMode == EditOnly) {
        setViewMode(SplitView);
    } else if (m_viewMode == SplitView) {
        setViewMode(PreviewOnly);
    } else {
        setViewMode(EditOnly);
    }
}

// =============================================================================
// Markdown Rendering
// =============================================================================

void MarkdownViewer::updatePreview() {
    QString markdown = m_editor->toPlainText();
    QString html = renderMarkdown(markdown);
    QString fullHtml = getFullHtml(html);
    
    // Preserve scroll position
    int scrollPos = m_preview->verticalScrollBar()->value();
    m_preview->setHtml(fullHtml);
    m_preview->verticalScrollBar()->setValue(scrollPos);
    
    updateToc(html);
    m_previewNeedsUpdate = false;
    
    emit contentChanged();
}

QString MarkdownViewer::renderMarkdown(const QString& markdown) {
    QString html = markdown;
    
    // Order matters for parsing
    html = parseCodeBlocks(html);      // Parse code blocks first to protect their content
    html = parseHeadings(html);
    html = parseTables(html);
    html = parseTaskLists(html);
    html = parseLists(html);
    html = parseBlockquotes(html);
    html = parseHorizontalRules(html);
    html = parseLinks(html);
    html = parseImages(html);
    html = parseInlineFormatting(html);
    
    if (m_options.enableEmoji) {
        html = parseEmoji(html);
    }
    
    if (m_options.enableMath) {
        html = parseMath(html);
    }
    
    // Convert line breaks to paragraphs
    QStringList paragraphs = html.split(QRegularExpression("\n{2,}"));
    QStringList processed;
    for (const QString& p : paragraphs) {
        QString trimmed = p.trimmed();
        if (trimmed.isEmpty()) continue;
        
        // Don't wrap already wrapped elements
        if (trimmed.startsWith("<h") || trimmed.startsWith("<ul") ||
            trimmed.startsWith("<ol") || trimmed.startsWith("<pre") ||
            trimmed.startsWith("<table") || trimmed.startsWith("<blockquote") ||
            trimmed.startsWith("<hr")) {
            processed.append(trimmed);
        } else {
            QString withBr = trimmed.replace('\n', "<br>\n");
            processed.append("<p>" + withBr + "</p>");
        }
    }
    
    return processed.join("\n\n");
}

QString MarkdownViewer::parseInlineFormatting(const QString& text) {
    QString result = text;
    
    // Bold: **text** or __text__
    result.replace(QRegularExpression(R"(\*\*([^*]+)\*\*)"), "<strong>\\1</strong>");
    result.replace(QRegularExpression(R"(__([^_]+)__)"), "<strong>\\1</strong>");
    
    // Italic: *text* or _text_
    result.replace(QRegularExpression(R"(\*([^*]+)\*)"), "<em>\\1</em>");
    result.replace(QRegularExpression(R"(_([^_]+)_)"), "<em>\\1</em>");
    
    // Strikethrough: ~~text~~
    if (m_options.enableStrikethrough) {
        result.replace(QRegularExpression(R"(~~([^~]+)~~)"), "<del>\\1</del>");
    }
    
    // Inline code: `code`
    result.replace(QRegularExpression(R"(`([^`]+)`)"), "<code>\\1</code>");
    
    return result;
}

QString MarkdownViewer::parseCodeBlocks(const QString& text) {
    QString result = text;
    
    // Fenced code blocks: ```language\ncode\n```
    QRegularExpression codeBlockRe(R"(```(\w*)\n([\s\S]*?)```)");
    QRegularExpressionMatchIterator it = codeBlockRe.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString language = match.captured(1);
        QString code = match.captured(2);
        
        QString highlighted = highlightCode(code.toHtmlEscaped(), language);
        QString replacement = QString("<pre><code class=\"language-%1\">%2</code></pre>")
            .arg(language.isEmpty() ? "text" : language)
            .arg(highlighted);
        
        result.replace(match.captured(0), replacement);
    }
    
    // Indented code blocks (4 spaces or tab)
    QRegularExpression indentedRe(R"((?:^|\n)((?:(?:    |\t).+\n?)+))");
    it = indentedRe.globalMatch(result);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString code = match.captured(1);
        code.replace(QRegularExpression("^(    |\\t)", QRegularExpression::MultilineOption), "");
        result.replace(match.captured(0), 
            "\n<pre><code>" + code.toHtmlEscaped() + "</code></pre>");
    }
    
    return result;
}

QString MarkdownViewer::parseHeadings(const QString& text) {
    QString result = text;
    
    // ATX-style headings: # Heading
    for (int i = 6; i >= 1; --i) {
        QString pattern = QString("^%1 (.+)$").arg(QString(i, '#'));
        QRegularExpression re(pattern, QRegularExpression::MultilineOption);
        
        QRegularExpressionMatchIterator it = re.globalMatch(result);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString heading = match.captured(1).trimmed();
            QString anchor = heading.toLower().replace(' ', '-')
                .replace(QRegularExpression("[^a-z0-9-]"), "");
            
            QString replacement = QString("<h%1 id=\"%2\">%3</h%1>")
                .arg(i).arg(anchor).arg(heading);
            result.replace(match.captured(0), replacement);
        }
    }
    
    // Setext-style headings
    result.replace(QRegularExpression(R"(^(.+)\n=+$)", QRegularExpression::MultilineOption),
                  "<h1>\\1</h1>");
    result.replace(QRegularExpression(R"(^(.+)\n-+$)", QRegularExpression::MultilineOption),
                  "<h2>\\1</h2>");
    
    return result;
}

QString MarkdownViewer::parseTables(const QString& text) {
    if (!m_options.enableTables) return text;
    
    QString result = text;
    
    // Simple GFM table parsing
    QRegularExpression tableRe(R"((?:^\|.+\|\n)+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = tableRe.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString tableText = match.captured(0);
        QStringList rows = tableText.split('\n', Qt::SkipEmptyParts);
        
        if (rows.size() < 2) continue;
        
        QString html = "<table>\n";
        
        // Header row
        QStringList headerCells = rows[0].split('|', Qt::SkipEmptyParts);
        html += "<thead><tr>";
        for (const QString& cell : headerCells) {
            html += "<th>" + cell.trimmed() + "</th>";
        }
        html += "</tr></thead>\n";
        
        // Skip separator row (index 1)
        
        // Body rows
        html += "<tbody>\n";
        for (int i = 2; i < rows.size(); ++i) {
            QStringList cells = rows[i].split('|', Qt::SkipEmptyParts);
            html += "<tr>";
            for (const QString& cell : cells) {
                html += "<td>" + cell.trimmed() + "</td>";
            }
            html += "</tr>\n";
        }
        html += "</tbody>\n</table>";
        
        result.replace(tableText, html);
    }
    
    return result;
}

QString MarkdownViewer::parseLists(const QString& text) {
    QString result = text;
    
    // Unordered lists: - item, * item, + item
    QRegularExpression ulRe(R"((?:^[-*+] .+\n?)+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = ulRe.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString listText = match.captured(0);
        QStringList items = listText.split('\n', Qt::SkipEmptyParts);
        
        QString html = "<ul>\n";
        for (const QString& item : items) {
            QString content = item.mid(2).trimmed();
            html += "<li>" + content + "</li>\n";
        }
        html += "</ul>";
        
        result.replace(listText, html);
    }
    
    // Ordered lists: 1. item
    QRegularExpression olRe(R"((?:^\d+\. .+\n?)+)", QRegularExpression::MultilineOption);
    it = olRe.globalMatch(result);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString listText = match.captured(0);
        QStringList items = listText.split('\n', Qt::SkipEmptyParts);
        
        QString html = "<ol>\n";
        for (const QString& item : items) {
            int dotPos = item.indexOf(". ");
            QString content = item.mid(dotPos + 2).trimmed();
            html += "<li>" + content + "</li>\n";
        }
        html += "</ol>";
        
        result.replace(listText, html);
    }
    
    return result;
}

QString MarkdownViewer::parseTaskLists(const QString& text) {
    if (!m_options.enableTaskLists) return text;
    
    QString result = text;
    
    // Task lists: - [ ] unchecked, - [x] checked
    result.replace(QRegularExpression(R"(^- \[x\] (.+)$)", QRegularExpression::MultilineOption),
                  "<li class=\"task\"><input type=\"checkbox\" checked disabled> \\1</li>");
    result.replace(QRegularExpression(R"(^- \[ \] (.+)$)", QRegularExpression::MultilineOption),
                  "<li class=\"task\"><input type=\"checkbox\" disabled> \\1</li>");
    
    return result;
}

QString MarkdownViewer::parseLinks(const QString& text) {
    QString result = text;
    
    // Inline links: [text](url)
    result.replace(QRegularExpression(R"(\[([^\]]+)\]\(([^)]+)\))"),
                  "<a href=\"\\2\">\\1</a>");
    
    // Autolinks
    if (m_options.enableAutolinks) {
        result.replace(QRegularExpression(R"((?<!["\(])(https?://[^\s<]+))"),
                      "<a href=\"\\1\">\\1</a>");
    }
    
    return result;
}

QString MarkdownViewer::parseImages(const QString& text) {
    QString result = text;
    
    // Images: ![alt](url)
    result.replace(QRegularExpression(R"(!\[([^\]]*)\]\(([^)]+)\))"),
                  "<img src=\"\\2\" alt=\"\\1\" style=\"max-width: 100%;\">");
    
    return result;
}

QString MarkdownViewer::parseBlockquotes(const QString& text) {
    QString result = text;
    
    // Blockquotes: > text
    QRegularExpression bqRe(R"((?:^> .+\n?)+)", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = bqRe.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString bqText = match.captured(0);
        QString content = bqText;
        content.replace(QRegularExpression("^> ", QRegularExpression::MultilineOption), "");
        
        result.replace(bqText, "<blockquote>" + content.trimmed() + "</blockquote>");
    }
    
    return result;
}

QString MarkdownViewer::parseHorizontalRules(const QString& text) {
    QString result = text;
    
    // Horizontal rules: ---, ***, ___
    result.replace(QRegularExpression(R"(^[-*_]{3,}$)", QRegularExpression::MultilineOption),
                  "<hr>");
    
    return result;
}

QString MarkdownViewer::parseEmoji(const QString& text) {
    QString result = text;
    
    for (auto it = s_emojiMap.begin(); it != s_emojiMap.end(); ++it) {
        result.replace(it.key(), it.value());
    }
    
    return result;
}

QString MarkdownViewer::parseMath(const QString& text) {
    QString result = text;
    
    // Display math: $$...$$
    result.replace(QRegularExpression(R"(\$\$([^$]+)\$\$)"),
                  "<div class=\"math-display\">\\1</div>");
    
    // Inline math: $...$
    result.replace(QRegularExpression(R"(\$([^$]+)\$)"),
                  "<span class=\"math-inline\">\\1</span>");
    
    return result;
}

QString MarkdownViewer::parseMermaid(const QString& text) {
    QString result = text;
    
    // Mermaid diagrams in code blocks
    result.replace(QRegularExpression(R"(<pre><code class="language-mermaid">([^<]+)</code></pre>)"),
                  "<div class=\"mermaid\">\\1</div>");
    
    return result;
}

QString MarkdownViewer::highlightCode(const QString& code, const QString& language) {
    if (!m_options.enableSyntaxHighlight || language.isEmpty()) {
        return code;
    }
    
    QString result = code;
    
    // Simple keyword highlighting (full implementation would use a proper lexer)
    QMap<QString, QStringList> keywords;
    keywords["cpp"] = {"int", "void", "return", "if", "else", "for", "while", "class",
                       "struct", "public", "private", "protected", "virtual", "override",
                       "const", "static", "nullptr", "true", "false", "auto", "template",
                       "typename", "namespace", "using", "include", "define"};
    keywords["python"] = {"def", "class", "if", "elif", "else", "for", "while", "return",
                          "import", "from", "as", "try", "except", "finally", "with",
                          "lambda", "yield", "pass", "break", "continue", "True", "False", "None"};
    keywords["javascript"] = {"function", "const", "let", "var", "if", "else", "for", "while",
                              "return", "class", "extends", "import", "export", "default",
                              "async", "await", "try", "catch", "throw", "new", "this",
                              "true", "false", "null", "undefined"};
    
    QString lang = language.toLower();
    if (lang == "c" || lang == "h" || lang == "hpp") lang = "cpp";
    if (lang == "js" || lang == "ts" || lang == "typescript") lang = "javascript";
    if (lang == "py") lang = "python";
    
    if (keywords.contains(lang)) {
        for (const QString& kw : keywords[lang]) {
            QRegularExpression kwRe(QString("\\b(%1)\\b").arg(kw));
            result.replace(kwRe, "<span class=\"keyword\">\\1</span>");
        }
    }
    
    // Highlight strings
    result.replace(QRegularExpression(R"("([^"\\]|\\.)*")"),
                  "<span class=\"string\">\\0</span>");
    result.replace(QRegularExpression(R"('([^'\\]|\\.)*')"),
                  "<span class=\"string\">\\0</span>");
    
    // Highlight comments
    result.replace(QRegularExpression(R"(//.*$)", QRegularExpression::MultilineOption),
                  "<span class=\"comment\">\\0</span>");
    result.replace(QRegularExpression(R"(#.*$)", QRegularExpression::MultilineOption),
                  "<span class=\"comment\">\\0</span>");
    
    // Highlight numbers
    result.replace(QRegularExpression(R"(\b\d+\.?\d*\b)"),
                  "<span class=\"number\">\\0</span>");
    
    return result;
}

QString MarkdownViewer::getCodeHighlightCss() {
    return R"(
        .keyword { color: #569cd6; }
        .string { color: #ce9178; }
        .comment { color: #6a9955; }
        .number { color: #b5cea8; }
        pre { background-color: #1e1e1e; padding: 12px; border-radius: 4px; overflow-x: auto; }
        code { font-family: 'Consolas', 'Monaco', monospace; font-size: 13px; }
        pre code { color: #d4d4d4; }
        :not(pre) > code { background-color: #2d2d2d; padding: 2px 6px; border-radius: 3px; }
    )";
}

// =============================================================================
// TOC Generation
// =============================================================================

void MarkdownViewer::updateToc(const QString& html) {
    m_toc.clear();
    
    // Regex to match headings like <h1 id="heading">Heading Text</h1>
    QRegularExpression headingRe("<h([1-6])[^>]*id=\"([^\"]*)\"[^>]*>([^<]*)</h\\1>");
    QRegularExpressionMatchIterator it = headingRe.globalMatch(html);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        TocEntry entry;
        entry.level = match.captured(1).toInt();
        entry.anchor = match.captured(2);
        entry.text = match.captured(3);
        m_toc.append(entry);
    }
    
    emit tocUpdated(m_toc);
}

QVector<TocEntry> MarkdownViewer::getTableOfContents() const {
    return m_toc;
}

void MarkdownViewer::scrollToHeading(const QString& anchor) {
    m_preview->scrollToAnchor(anchor);
}

// =============================================================================
// CSS and HTML Generation
// =============================================================================

QString MarkdownViewer::getPreviewCss() {
    return QString(R"(
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;
            font-size: 14px;
            line-height: 1.6;
            color: #d4d4d4;
            background-color: #1e1e1e;
            padding: 20px;
            max-width: 900px;
            margin: 0 auto;
        }
        h1, h2, h3, h4, h5, h6 {
            color: #e0e0e0;
            margin-top: 24px;
            margin-bottom: 16px;
            font-weight: 600;
            line-height: 1.25;
        }
        h1 { font-size: 2em; border-bottom: 1px solid #444; padding-bottom: 0.3em; }
        h2 { font-size: 1.5em; border-bottom: 1px solid #444; padding-bottom: 0.3em; }
        h3 { font-size: 1.25em; }
        a { color: #4fc3f7; text-decoration: none; }
        a:hover { text-decoration: underline; }
        blockquote {
            padding: 0 1em;
            color: #9e9e9e;
            border-left: 4px solid #444;
            margin: 0;
        }
        table {
            border-collapse: collapse;
            width: 100%%;
            margin: 16px 0;
        }
        th, td {
            border: 1px solid #444;
            padding: 8px 12px;
            text-align: left;
        }
        th { background-color: #2d2d2d; }
        tr:nth-child(even) { background-color: #252525; }
        hr {
            border: none;
            border-top: 1px solid #444;
            margin: 24px 0;
        }
        img { max-width: 100%%; height: auto; }
        li.task { list-style-type: none; margin-left: -20px; }
        li.task input { margin-right: 8px; }
        .math-display {
            text-align: center;
            padding: 16px;
            background-color: #252525;
            border-radius: 4px;
            font-family: 'Times New Roman', serif;
            font-size: 1.2em;
        }
        .math-inline {
            font-family: 'Times New Roman', serif;
        }
        %1
    )").arg(getCodeHighlightCss());
}

QString MarkdownViewer::getFullHtml(const QString& bodyHtml) {
    return QString(R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>%1</style>
</head>
<body>
%2
</body>
</html>
    )").arg(getPreviewCss(), bodyHtml);
}

// =============================================================================
// Export
// =============================================================================

void MarkdownViewer::exportToHtml(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file.");
        return;
    }
    
    QTextStream out(&file);
    // Qt6: setCodec removed, UTF-8 is default
    out << getFullHtml(renderMarkdown(m_editor->toPlainText()));
    file.close();
}

void MarkdownViewer::exportToPdf(const QString& path) {
    // PDF export not available - QPrinter requires Qt PrintSupport module
    // which is not enabled in this build configuration
    Q_UNUSED(path);
    QMessageBox::information(this, "Export PDF", 
        "PDF export is not available in this build configuration.\n"
        "Please export to HTML instead.");
}

void MarkdownViewer::copyHtmlToClipboard() {
    QApplication::clipboard()->setText(renderMarkdown(m_editor->toPlainText()));
}

// =============================================================================
// Editor Actions
// =============================================================================

void MarkdownViewer::insertBold() {
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        cursor.insertText("**" + text + "**");
    } else {
        cursor.insertText("**bold**");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2);
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 4);
        m_editor->setTextCursor(cursor);
    }
}

void MarkdownViewer::insertItalic() {
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        cursor.insertText("*" + text + "*");
    } else {
        cursor.insertText("*italic*");
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 6);
        m_editor->setTextCursor(cursor);
    }
}

void MarkdownViewer::insertStrikethrough() {
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        cursor.insertText("~~" + text + "~~");
    } else {
        cursor.insertText("~~strikethrough~~");
    }
}

void MarkdownViewer::insertLink() {
    bool ok;
    QString url = QInputDialog::getText(this, "Insert Link", "URL:", QLineEdit::Normal, "https://", &ok);
    if (!ok || url.isEmpty()) return;
    
    QTextCursor cursor = m_editor->textCursor();
    QString text = cursor.hasSelection() ? cursor.selectedText() : "link text";
    cursor.insertText(QString("[%1](%2)").arg(text, url));
}

void MarkdownViewer::insertImage() {
    QString path = QFileDialog::getOpenFileName(this, "Select Image",
        QString(), "Images (*.png *.jpg *.jpeg *.gif *.svg *.webp);;All Files (*)");
    if (path.isEmpty()) return;
    
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(QString("![%1](%2)").arg(QFileInfo(path).baseName(), path));
}

void MarkdownViewer::insertCodeBlock() {
    QTextCursor cursor = m_editor->textCursor();
    QString code = cursor.hasSelection() ? cursor.selectedText() : "code";
    cursor.insertText("```\n" + code + "\n```");
}

void MarkdownViewer::insertTable() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(
        "| Header 1 | Header 2 | Header 3 |\n"
        "|----------|----------|----------|\n"
        "| Cell 1   | Cell 2   | Cell 3   |\n"
        "| Cell 4   | Cell 5   | Cell 6   |\n"
    );
}

void MarkdownViewer::insertHeading(int level) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText(QString(level, '#') + " ");
}

void MarkdownViewer::insertBulletList() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("- ");
}

void MarkdownViewer::insertNumberedList() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("1. ");
}

void MarkdownViewer::insertTaskList() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("- [ ] ");
}

void MarkdownViewer::insertHorizontalRule() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.insertText("\n\n---\n\n");
}

void MarkdownViewer::insertBlockquote() {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.insertText("> ");
}

// =============================================================================
// Slots
// =============================================================================

void MarkdownViewer::onEditorTextChanged() {
    m_previewNeedsUpdate = true;
    m_updateTimer->start();  // Debounce updates
}

void MarkdownViewer::onLinkClicked(const QUrl& url) {
    if (url.scheme() == "file" || url.isRelative()) {
        // Internal link - scroll to anchor
        scrollToHeading(url.fragment());
    } else {
        // External link - open in browser
        QDesktopServices::openUrl(url);
    }
    emit linkClicked(url);
}

void MarkdownViewer::onFileChanged(const QString& path) {
    if (path == m_currentFile) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "File Changed", "The file has been modified externally. Reload?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            loadFile(path);
        }
    }
}

void MarkdownViewer::syncScrollFromEditor() {
    if (!m_syncScrolling || m_viewMode == PreviewOnly) return;
    
    QScrollBar* editorBar = m_editor->verticalScrollBar();
    QScrollBar* previewBar = m_preview->verticalScrollBar();
    
    if (editorBar->maximum() == 0) return;
    
    double ratio = static_cast<double>(editorBar->value()) / editorBar->maximum();
    previewBar->setValue(static_cast<int>(ratio * previewBar->maximum()));
}

void MarkdownViewer::syncScrollFromPreview() {
    // Reverse sync if needed
}

void MarkdownViewer::setOptions(const MarkdownOptions& options) {
    m_options = options;
    updatePreview();
}
