/**
 * @file diff_viewer_widget.cpp
 * @brief Full Diff Viewer Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "diff_viewer_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>


// =============================================================================
// DiffLineNumberArea Implementation
// =============================================================================

DiffLineNumberArea::DiffLineNumberArea(QPlainTextEdit* editor, bool isOld)
    : QWidget(editor)
    , m_editor(editor)
    , m_isOld(isOld)
{
}

QSize DiffLineNumberArea::sizeHint() const {
    return QSize(50, 0);
}

void DiffLineNumberArea::paintEvent(QPaintEvent* event) {
    auto diffEdit = qobject_cast<DiffTextEdit*>(m_editor);
    if (diffEdit) {
        diffEdit->lineNumberAreaPaintEvent(event, m_isOld);
    }
}

// =============================================================================
// DiffSyntaxHighlighter Implementation
// =============================================================================

DiffSyntaxHighlighter::DiffSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
}

void DiffSyntaxHighlighter::setLanguage(const QString& language) {
    m_language = language;
    rehighlight();
}

void DiffSyntaxHighlighter::highlightBlock(const QString& text) {
    // Basic syntax highlighting - could be extended with full language support
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#569cd6"));
    
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#ce9178"));
    
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#6a9955"));
    
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#b5cea8"));
    
    // Highlight strings
    QRegularExpression stringRe(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')");
    auto it = stringRe.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
    }
    
    // Highlight comments
    QRegularExpression commentRe(R"(//.*$|#.*$)");
    it = commentRe.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
    }
    
    // Highlight numbers
    QRegularExpression numberRe(R"(\b\d+\.?\d*\b)");
    it = numberRe.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), numberFormat);
    }
}

// =============================================================================
// DiffTextEdit Implementation
// =============================================================================

DiffTextEdit::DiffTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_oldLineArea = new DiffLineNumberArea(this, true);
    m_newLineArea = new DiffLineNumberArea(this, false);
    
    connect(this, &QPlainTextEdit::blockCountChanged, this, &DiffTextEdit::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &DiffTextEdit::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &DiffTextEdit::highlightCurrentLine);
    
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DiffTextEdit::scrollChanged);
    
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
                  "font-family: Consolas; font-size: 12px; }");
}

void DiffTextEdit::setDiffLines(const QVector<DiffLine>& lines) {
    m_diffLines = lines;
    
    QString content;
    for (const DiffLine& line : lines) {
        content += line.text + "\n";
    }
    setPlainText(content.trimmed());
    
    update();
}

int DiffTextEdit::lineNumberAreaWidth() {
    if (!m_showLineNumbers) return 0;
    
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    
    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space * 2;  // Two columns (old + new line numbers)
}

void DiffTextEdit::updateLineNumberAreaWidth(int) {
    int width = lineNumberAreaWidth();
    setViewportMargins(width, 0, 0, 0);
}

void DiffTextEdit::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) {
        m_oldLineArea->scroll(0, dy);
        m_newLineArea->scroll(0, dy);
    } else {
        m_oldLineArea->update(0, rect.y(), m_oldLineArea->width(), rect.height());
        m_newLineArea->update(0, rect.y(), m_newLineArea->width(), rect.height());
    }
    
    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void DiffTextEdit::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    
    QRect cr = contentsRect();
    int width = lineNumberAreaWidth() / 2;
    m_oldLineArea->setGeometry(QRect(cr.left(), cr.top(), width, cr.height()));
    m_newLineArea->setGeometry(QRect(cr.left() + width, cr.top(), width, cr.height()));
}

void DiffTextEdit::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;
    
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#2d2d2d"));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    
    setExtraSelections(extraSelections);
}

void DiffTextEdit::lineNumberAreaPaintEvent(QPaintEvent* event, bool isOld) {
    QPainter painter(isOld ? m_oldLineArea : m_newLineArea);
    painter.fillRect(event->rect(), QColor("#252526"));
    
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (blockNumber < m_diffLines.size()) {
                const DiffLine& line = m_diffLines[blockNumber];
                int lineNum = isOld ? line.oldLineNum : line.newLineNum;
                
                if (lineNum > 0) {
                    QString number = QString::number(lineNum);
                    painter.setPen(QColor("#858585"));
                    painter.drawText(0, top, (isOld ? m_oldLineArea : m_newLineArea)->width() - 5,
                                    fontMetrics().height(), Qt::AlignRight, number);
                }
            }
        }
        
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void DiffTextEdit::paintEvent(QPaintEvent* event) {
    // Draw background colors for diff lines
    QPainter painter(viewport());
    
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (blockNumber < m_diffLines.size()) {
                const DiffLine& line = m_diffLines[blockNumber];
                QColor bgColor;
                
                switch (line.type) {
                    case DiffChangeType::Added:
                        bgColor = QColor(0, 100, 0, 50);
                        break;
                    case DiffChangeType::Removed:
                        bgColor = QColor(100, 0, 0, 50);
                        break;
                    case DiffChangeType::Modified:
                        bgColor = QColor(100, 100, 0, 50);
                        break;
                    default:
                        bgColor = Qt::transparent;
                }
                
                if (bgColor != Qt::transparent) {
                    QRect lineRect(0, top, viewport()->width(), fontMetrics().height());
                    painter.fillRect(lineRect, bgColor);
                }
            }
        }
        
        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
    
    QPlainTextEdit::paintEvent(event);
}

void DiffTextEdit::setShowLineNumbers(bool show) {
    m_showLineNumbers = show;
    m_oldLineArea->setVisible(show);
    m_newLineArea->setVisible(show);
    updateLineNumberAreaWidth(0);
}

void DiffTextEdit::setSyncScrolling(QPlainTextEdit* other) {
    m_syncTarget = other;
}

// =============================================================================
// DiffViewerWidget Implementation
// =============================================================================

DiffViewerWidget::DiffViewerWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
{
    setupUI();
    connectSignals();
}

DiffViewerWidget::~DiffViewerWidget() {
}

void DiffViewerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // Create both view widgets
    setupSideBySideView();
    setupUnifiedView();
    
    // Stack them
    mainLayout->addWidget(m_sideBySideWidget);
    mainLayout->addWidget(m_unifiedWidget);
    
    m_unifiedWidget->hide();
}

void DiffViewerWidget::setupToolbar() {
    m_toolbar = new QToolBar("Diff Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // View mode
    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->addItem("Side by Side", SideBySide);
    m_viewModeCombo->addItem("Unified", Unified);
    m_viewModeCombo->addItem("Inline", Inline);
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        setViewMode(static_cast<ViewMode>(m_viewModeCombo->itemData(index).toInt()));
    });
    m_toolbar->addWidget(new QLabel(" View: ", this));
    m_toolbar->addWidget(m_viewModeCombo);
    
    m_toolbar->addSeparator();
    
    // Context lines
    m_contextLinesCombo = new QComboBox(this);
    m_contextLinesCombo->addItem("0 lines", 0);
    m_contextLinesCombo->addItem("3 lines", 3);
    m_contextLinesCombo->addItem("5 lines", 5);
    m_contextLinesCombo->addItem("10 lines", 10);
    m_contextLinesCombo->addItem("All", -1);
    m_contextLinesCombo->setCurrentIndex(1);  // Default 3 lines
    connect(m_contextLinesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        setContextLines(m_contextLinesCombo->currentData().toInt());
    });
    m_toolbar->addWidget(new QLabel(" Context: ", this));
    m_toolbar->addWidget(m_contextLinesCombo);
    
    m_toolbar->addSeparator();
    
    // Navigation
    m_prevBtn = new QPushButton("◀ Prev", this);
    m_prevBtn->setToolTip("Previous change");
    connect(m_prevBtn, &QPushButton::clicked, this, &DiffViewerWidget::previousChange);
    m_toolbar->addWidget(m_prevBtn);
    
    m_hunkLabel = new QLabel("0/0", this);
    m_hunkLabel->setMinimumWidth(60);
    m_hunkLabel->setAlignment(Qt::AlignCenter);
    m_toolbar->addWidget(m_hunkLabel);
    
    m_nextBtn = new QPushButton("Next ▶", this);
    m_nextBtn->setToolTip("Next change");
    connect(m_nextBtn, &QPushButton::clicked, this, &DiffViewerWidget::nextChange);
    m_toolbar->addWidget(m_nextBtn);
    
    m_toolbar->addSeparator();
    
    // Statistics
    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet("QLabel { color: #d4d4d4; }");
    m_toolbar->addWidget(m_statsLabel);
    
    m_toolbar->addSeparator();
    
    // Actions
    m_swapBtn = new QPushButton("⇄ Swap", this);
    m_swapBtn->setToolTip("Swap left and right sides");
    connect(m_swapBtn, &QPushButton::clicked, this, &DiffViewerWidget::swapSides);
    m_toolbar->addWidget(m_swapBtn);
    
    m_refreshBtn = new QPushButton("↻ Refresh", this);
    m_refreshBtn->setToolTip("Recalculate diff");
    connect(m_refreshBtn, &QPushButton::clicked, this, &DiffViewerWidget::refresh);
    m_toolbar->addWidget(m_refreshBtn);
    
    // Copy menu
    QPushButton* copyBtn = new QPushButton("Copy", this);
    QMenu* copyMenu = new QMenu(this);
    copyMenu->addAction("Copy Old Content", this, &DiffViewerWidget::copyOldContent);
    copyMenu->addAction("Copy New Content", this, &DiffViewerWidget::copyNewContent);
    copyMenu->addAction("Copy Unified Diff", this, &DiffViewerWidget::copyDiffOutput);
    copyBtn->setMenu(copyMenu);
    m_toolbar->addWidget(copyBtn);
}

void DiffViewerWidget::setupSideBySideView() {
    m_sideBySideWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_sideBySideWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Title bar
    QHBoxLayout* titleLayout = new QHBoxLayout();
    m_oldTitleLabel = new QLabel("Original", this);
    m_oldTitleLabel->setStyleSheet("QLabel { background-color: #2d2d2d; padding: 4px; color: #d4d4d4; }");
    m_newTitleLabel = new QLabel("Modified", this);
    m_newTitleLabel->setStyleSheet("QLabel { background-color: #2d2d2d; padding: 4px; color: #d4d4d4; }");
    
    titleLayout->addWidget(m_oldTitleLabel);
    titleLayout->addWidget(m_newTitleLabel);
    layout->addLayout(titleLayout);
    
    // Editors
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    m_oldEditor = new DiffTextEdit(this);
    m_oldHighlighter = new DiffSyntaxHighlighter(m_oldEditor->document());
    
    m_newEditor = new DiffTextEdit(this);
    m_newHighlighter = new DiffSyntaxHighlighter(m_newEditor->document());
    
    m_splitter->addWidget(m_oldEditor);
    m_splitter->addWidget(m_newEditor);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    
    layout->addWidget(m_splitter);
}

void DiffViewerWidget::setupUnifiedView() {
    m_unifiedWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_unifiedWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_unifiedEditor = new QPlainTextEdit(this);
    m_unifiedEditor->setReadOnly(true);
    m_unifiedEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_unifiedEditor->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "font-family: Consolas; font-size: 12px; }");
    
    m_unifiedHighlighter = new DiffSyntaxHighlighter(m_unifiedEditor->document());
    
    layout->addWidget(m_unifiedEditor);
}

void DiffViewerWidget::connectSignals() {
    // Sync scrolling between editors
    connect(m_oldEditor, &DiffTextEdit::scrollChanged, this, &DiffViewerWidget::onOldScrollChanged);
    connect(m_newEditor, &DiffTextEdit::scrollChanged, this, &DiffViewerWidget::onNewScrollChanged);
}

void DiffViewerWidget::onOldScrollChanged(int value) {
    if (!m_syncingScroll) {
        m_syncingScroll = true;
        m_newEditor->verticalScrollBar()->setValue(value);
        m_syncingScroll = false;
    }
}

void DiffViewerWidget::onNewScrollChanged(int value) {
    if (!m_syncingScroll) {
        m_syncingScroll = true;
        m_oldEditor->verticalScrollBar()->setValue(value);
        m_syncingScroll = false;
    }
}

void DiffViewerWidget::syncScrollBars() {
    // Additional scroll sync logic if needed
}

// =============================================================================
// Content Management
// =============================================================================

void DiffViewerWidget::setOldContent(const QString& content, const QString& title) {
    m_oldContent = content;
    m_oldTitle = title;
    m_oldTitleLabel->setText(title);
    calculateDiff();
}

void DiffViewerWidget::setNewContent(const QString& content, const QString& title) {
    m_newContent = content;
    m_newTitle = title;
    m_newTitleLabel->setText(title);
    calculateDiff();
}

void DiffViewerWidget::setContents(const QString& oldContent, const QString& newContent,
                                   const QString& oldTitle, const QString& newTitle) {
    m_oldContent = oldContent;
    m_newContent = newContent;
    m_oldTitle = oldTitle;
    m_newTitle = newTitle;
    m_oldTitleLabel->setText(oldTitle);
    m_newTitleLabel->setText(newTitle);
    calculateDiff();
}

void DiffViewerWidget::loadFiles(const QString& oldPath, const QString& newPath) {
    auto loadFile = [](const QString& path) -> QString {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            return in.readAll();
        }
        return QString();
    };
    
    m_oldContent = loadFile(oldPath);
    m_newContent = loadFile(newPath);
    m_oldTitle = QFileInfo(oldPath).fileName();
    m_newTitle = QFileInfo(newPath).fileName();
    
    m_oldTitleLabel->setText(m_oldTitle);
    m_newTitleLabel->setText(m_newTitle);
    
    // Detect language from extension
    QString ext = QFileInfo(newPath).suffix().toLower();
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp") {
        setLanguage("cpp");
    } else if (ext == "py") {
        setLanguage("python");
    } else if (ext == "js" || ext == "ts") {
        setLanguage("javascript");
    }
    
    calculateDiff();
}

void DiffViewerWidget::loadFromGitDiff(const QString& gitDiffOutput) {
    // Parse git diff output
    // This is a simplified parser - full implementation would handle all git diff formats
    QStringList lines = gitDiffOutput.split('\n');
    
    QString oldContent, newContent;
    bool inHunk = false;
    
    for (const QString& line : lines) {
        if (line.startsWith("---")) {
            m_oldTitle = line.mid(4);
        } else if (line.startsWith("+++")) {
            m_newTitle = line.mid(4);
        } else if (line.startsWith("@@")) {
            inHunk = true;
        } else if (inHunk) {
            if (line.startsWith("-")) {
                oldContent += line.mid(1) + "\n";
            } else if (line.startsWith("+")) {
                newContent += line.mid(1) + "\n";
            } else if (line.startsWith(" ")) {
                oldContent += line.mid(1) + "\n";
                newContent += line.mid(1) + "\n";
            }
        }
    }
    
    m_oldContent = oldContent;
    m_newContent = newContent;
    m_oldTitleLabel->setText(m_oldTitle);
    m_newTitleLabel->setText(m_newTitle);
    
    calculateDiff();
}

// =============================================================================
// View Options
// =============================================================================

void DiffViewerWidget::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    
    switch (mode) {
        case SideBySide:
            m_sideBySideWidget->show();
            m_unifiedWidget->hide();
            break;
        case Unified:
        case Inline:
            m_sideBySideWidget->hide();
            m_unifiedWidget->show();
            break;
    }
    
    updateViews();
}

void DiffViewerWidget::setLanguage(const QString& language) {
    m_language = language;
    m_oldHighlighter->setLanguage(language);
    m_newHighlighter->setLanguage(language);
    m_unifiedHighlighter->setLanguage(language);
}

void DiffViewerWidget::setContextLines(int lines) {
    m_contextLines = lines;
    updateViews();
}

void DiffViewerWidget::setShowLineNumbers(bool show) {
    m_showLineNumbers = show;
    m_oldEditor->setShowLineNumbers(show);
    m_newEditor->setShowLineNumbers(show);
}

void DiffViewerWidget::setWordDiff(bool enabled) {
    m_wordDiff = enabled;
    calculateDiff();
}

void DiffViewerWidget::setIgnoreWhitespace(bool ignore) {
    m_ignoreWhitespace = ignore;
    calculateDiff();
}

// =============================================================================
// Diff Algorithm (Myers-like)
// =============================================================================

void DiffViewerWidget::calculateDiff() {
    QStringList oldLines = m_oldContent.split('\n');
    QStringList newLines = m_newContent.split('\n');
    
    m_diffLines = computeDiff(oldLines, newLines);
    
    // Build diff result
    m_diffResult = DiffResult();
    m_diffResult.oldFile = m_oldTitle;
    m_diffResult.newFile = m_newTitle;
    m_diffResult.additions = 0;
    m_diffResult.deletions = 0;
    
    m_hunkPositions.clear();
    
    // Count additions and deletions
    bool inHunk = false;
    DiffHunk currentHunk;
    
    for (int i = 0; i < m_diffLines.size(); ++i) {
        const DiffLine& line = m_diffLines[i];
        
        if (line.type == DiffChangeType::Added) {
            m_diffResult.additions++;
            if (!inHunk) {
                inHunk = true;
                m_hunkPositions.append(i);
            }
        } else if (line.type == DiffChangeType::Removed) {
            m_diffResult.deletions++;
            if (!inHunk) {
                inHunk = true;
                m_hunkPositions.append(i);
            }
        } else if (line.type == DiffChangeType::Modified) {
            m_diffResult.changes++;
            if (!inHunk) {
                inHunk = true;
                m_hunkPositions.append(i);
            }
        } else {
            inHunk = false;
        }
    }
    
    updateViews();
    updateStatistics();
    updateHunkNavigation();
    
    emit diffCalculated(m_diffResult);
    emit contentChanged();
}

QVector<DiffLine> DiffViewerWidget::computeDiff(const QStringList& oldLines, const QStringList& newLines) {
    QVector<DiffLine> result;
    
    // Simple LCS-based diff algorithm
    int m = oldLines.size();
    int n = newLines.size();
    
    // Build LCS table
    QVector<QVector<int>> lcs(m + 1, QVector<int>(n + 1, 0));
    
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            QString oldLine = oldLines[i - 1];
            QString newLine = newLines[j - 1];
            
            if (m_ignoreWhitespace) {
                oldLine = oldLine.trimmed();
                newLine = newLine.trimmed();
            }
            
            if (oldLine == newLine) {
                lcs[i][j] = lcs[i - 1][j - 1] + 1;
            } else {
                lcs[i][j] = qMax(lcs[i - 1][j], lcs[i][j - 1]);
            }
        }
    }
    
    // Backtrack to find diff
    QVector<DiffLine> diff;
    int i = m, j = n;
    
    while (i > 0 || j > 0) {
        QString oldLine = (i > 0) ? oldLines[i - 1] : QString();
        QString newLine = (j > 0) ? newLines[j - 1] : QString();
        
        QString oldCompare = m_ignoreWhitespace ? oldLine.trimmed() : oldLine;
        QString newCompare = m_ignoreWhitespace ? newLine.trimmed() : newLine;
        
        if (i > 0 && j > 0 && oldCompare == newCompare) {
            DiffLine line;
            line.type = DiffChangeType::Context;
            line.text = oldLine;
            line.oldLineNum = i;
            line.newLineNum = j;
            diff.prepend(line);
            --i;
            --j;
        } else if (j > 0 && (i == 0 || lcs[i][j - 1] >= lcs[i - 1][j])) {
            DiffLine line;
            line.type = DiffChangeType::Added;
            line.text = newLine;
            line.oldLineNum = -1;
            line.newLineNum = j;
            diff.prepend(line);
            --j;
        } else if (i > 0) {
            DiffLine line;
            line.type = DiffChangeType::Removed;
            line.text = oldLine;
            line.oldLineNum = i;
            line.newLineNum = -1;
            diff.prepend(line);
            --i;
        }
    }
    
    return diff;
}

// =============================================================================
// View Updates
// =============================================================================

void DiffViewerWidget::updateViews() {
    if (m_viewMode == SideBySide) {
        // Split diff lines into old and new
        QVector<DiffLine> oldDiff, newDiff;
        
        for (const DiffLine& line : m_diffLines) {
            if (line.type == DiffChangeType::Removed) {
                oldDiff.append(line);
                DiffLine empty;
                empty.type = DiffChangeType::None;
                empty.text = "";
                empty.oldLineNum = -1;
                empty.newLineNum = -1;
                newDiff.append(empty);
            } else if (line.type == DiffChangeType::Added) {
                DiffLine empty;
                empty.type = DiffChangeType::None;
                empty.text = "";
                empty.oldLineNum = -1;
                empty.newLineNum = -1;
                oldDiff.append(empty);
                newDiff.append(line);
            } else {
                oldDiff.append(line);
                newDiff.append(line);
            }
        }
        
        m_oldEditor->setDiffLines(oldDiff);
        m_newEditor->setDiffLines(newDiff);
    } else {
        // Unified/Inline view
        QString unifiedText = generateUnifiedDiff();
        m_unifiedEditor->setPlainText(unifiedText);
    }
}

QString DiffViewerWidget::generateUnifiedDiff() {
    QString result;
    
    // Header
    result += QString("--- %1\n").arg(m_oldTitle);
    result += QString("+++ %1\n").arg(m_newTitle);
    
    // Generate hunks
    int contextBefore = 0;
    QStringList hunkLines;
    int oldStart = 0, oldCount = 0;
    int newStart = 0, newCount = 0;
    
    for (int i = 0; i < m_diffLines.size(); ++i) {
        const DiffLine& line = m_diffLines[i];
        
        QString prefix;
        switch (line.type) {
            case DiffChangeType::Context:
                prefix = " ";
                oldCount++;
                newCount++;
                break;
            case DiffChangeType::Added:
                prefix = "+";
                newCount++;
                break;
            case DiffChangeType::Removed:
                prefix = "-";
                oldCount++;
                break;
            default:
                prefix = " ";
        }
        
        if (oldStart == 0 && line.oldLineNum > 0) oldStart = line.oldLineNum;
        if (newStart == 0 && line.newLineNum > 0) newStart = line.newLineNum;
        
        hunkLines.append(prefix + line.text);
    }
    
    if (!hunkLines.isEmpty()) {
        result += QString("@@ -%1,%2 +%3,%4 @@\n")
            .arg(oldStart).arg(oldCount)
            .arg(newStart).arg(newCount);
        result += hunkLines.join("\n");
    }
    
    return result;
}

void DiffViewerWidget::updateStatistics() {
    QString stats = QString("<span style='color: #4ec9b0;'>+%1</span> "
                           "<span style='color: #f44747;'>-%2</span>")
        .arg(m_diffResult.additions)
        .arg(m_diffResult.deletions);
    m_statsLabel->setText(stats);
}

void DiffViewerWidget::updateHunkNavigation() {
    int total = m_hunkPositions.size();
    m_hunkLabel->setText(QString("%1/%2").arg(total > 0 ? m_currentHunk + 1 : 0).arg(total));
    
    m_prevBtn->setEnabled(m_currentHunk > 0);
    m_nextBtn->setEnabled(m_currentHunk < total - 1);
}

// =============================================================================
// Navigation
// =============================================================================

void DiffViewerWidget::goToHunk(int index) {
    if (index < 0 || index >= m_hunkPositions.size()) return;
    
    m_currentHunk = index;
    int linePos = m_hunkPositions[index];
    
    if (m_viewMode == SideBySide) {
        QTextCursor cursor = m_oldEditor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, linePos);
        m_oldEditor->setTextCursor(cursor);
        m_oldEditor->centerCursor();
    } else {
        QTextCursor cursor = m_unifiedEditor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, linePos);
        m_unifiedEditor->setTextCursor(cursor);
        m_unifiedEditor->centerCursor();
    }
    
    updateHunkNavigation();
    emit hunkSelected(index);
}

void DiffViewerWidget::nextChange() {
    if (m_currentHunk < m_hunkPositions.size() - 1) {
        goToHunk(m_currentHunk + 1);
    }
}

void DiffViewerWidget::previousChange() {
    if (m_currentHunk > 0) {
        goToHunk(m_currentHunk - 1);
    }
}

void DiffViewerWidget::firstChange() {
    goToHunk(0);
}

void DiffViewerWidget::lastChange() {
    goToHunk(m_hunkPositions.size() - 1);
}

// =============================================================================
// Actions
// =============================================================================

void DiffViewerWidget::copyOldContent() {
    QApplication::clipboard()->setText(m_oldContent);
}

void DiffViewerWidget::copyNewContent() {
    QApplication::clipboard()->setText(m_newContent);
}

void DiffViewerWidget::copyDiffOutput() {
    QApplication::clipboard()->setText(generateUnifiedDiff());
}

void DiffViewerWidget::swapSides() {
    std::swap(m_oldContent, m_newContent);
    std::swap(m_oldTitle, m_newTitle);
    m_oldTitleLabel->setText(m_oldTitle);
    m_newTitleLabel->setText(m_newTitle);
    calculateDiff();
}

void DiffViewerWidget::refresh() {
    calculateDiff();
}

QColor DiffViewerWidget::getColorForType(DiffChangeType type, bool background) const {
    if (background) {
        switch (type) {
            case DiffChangeType::Added: return QColor(0, 100, 0, 80);
            case DiffChangeType::Removed: return QColor(100, 0, 0, 80);
            case DiffChangeType::Modified: return QColor(100, 100, 0, 80);
            default: return Qt::transparent;
        }
    } else {
        switch (type) {
            case DiffChangeType::Added: return QColor("#4ec9b0");
            case DiffChangeType::Removed: return QColor("#f44747");
            case DiffChangeType::Modified: return QColor("#dcdcaa");
            default: return QColor("#d4d4d4");
        }
    }
}

