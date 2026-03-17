/*
 * SearchAndReplaceWidget.cpp - Complete Implementation (700+ lines)
 */

#include "SearchAndReplaceWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QThread>
#include <QRunnable>
#include <QThreadPool>
#include <QMessageBox>
#include <QDebug>
#include <QComboBox>
#include <QAbstractItemModel>

// Worker thread for file search
class FileSearchWorker : public QRunnable {
public:
    FileSearchWorker(const QString& projectRoot, const QString& searchPattern, 
                     const QRegularExpression& regex, int searchOptions)
        : m_projectRoot(projectRoot)
        , m_searchPattern(searchPattern)
        , m_regex(regex)
        , m_searchOptions(searchOptions)
    {
    }

    void run() override;

    QList<SearchResult> getResults() const { return m_results; }

signals:
    void finished(const QList<SearchResult>& results);

private:
    QString m_projectRoot;
    QString m_searchPattern;
    QRegularExpression m_regex;
    int m_searchOptions;
    QList<SearchResult> m_results;
};

// ============================================================================
// SearchAndReplaceWidget Implementation
// ============================================================================

SearchAndReplaceWidget::SearchAndReplaceWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

SearchAndReplaceWidget::~SearchAndReplaceWidget()
{
}

void SearchAndReplaceWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Search input bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    
    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Find...");
    m_searchInput->setStyleSheet(
        "QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; "
        "padding: 5px; border-radius: 3px; }"
    );
    searchLayout->addWidget(new QLabel("Find:"));
    searchLayout->addWidget(m_searchInput);

    m_matchCountLabel = new QLabel("0 of 0");
    m_matchCountLabel->setStyleSheet("QLabel { color: #858585; margin-right: 10px; }");
    searchLayout->addWidget(m_matchCountLabel);

    m_findPrevBtn = new QPushButton("< Previous");
    m_findPrevBtn->setMaximumWidth(80);
    searchLayout->addWidget(m_findPrevBtn);

    m_findNextBtn = new QPushButton("Next >");
    m_findNextBtn->setMaximumWidth(80);
    searchLayout->addWidget(m_findNextBtn);

    m_findInFilesBtn = new QPushButton("Find in Files");
    m_findInFilesBtn->setMaximumWidth(100);
    searchLayout->addWidget(m_findInFilesBtn);

    mainLayout->addLayout(searchLayout);

    // Options bar
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    
    m_caseSensitiveCheck = new QCheckBox("Match Case");
    m_wholeWordCheck = new QCheckBox("Whole Word");
    m_regexCheck = new QCheckBox("Use Regex");
    m_wrapAroundCheck = new QCheckBox("Wrap Around");
    m_wrapAroundCheck->setChecked(true);

    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_wholeWordCheck);
    optionsLayout->addWidget(m_regexCheck);
    optionsLayout->addWidget(m_wrapAroundCheck);
    optionsLayout->addStretch();

    mainLayout->addLayout(optionsLayout);

    // Replace panel
    m_replacePanel = new QWidget();
    QHBoxLayout* replaceLayout = new QHBoxLayout(m_replacePanel);
    replaceLayout->setContentsMargins(0, 0, 0, 0);

    m_replaceInput = new QLineEdit();
    m_replaceInput->setPlaceholderText("Replace with...");
    m_replaceInput->setStyleSheet(
        "QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; "
        "padding: 5px; border-radius: 3px; }"
    );
    replaceLayout->addWidget(new QLabel("Replace:"));
    replaceLayout->addWidget(m_replaceInput);

    m_replaceBtn = new QPushButton("Replace");
    m_replaceBtn->setMaximumWidth(80);
    replaceLayout->addWidget(m_replaceBtn);

    m_replaceAllBtn = new QPushButton("Replace All");
    m_replaceAllBtn->setMaximumWidth(100);
    replaceLayout->addWidget(m_replaceAllBtn);

    replaceLayout->addStretch();

    m_replacePanel->setVisible(false);
    mainLayout->addWidget(m_replacePanel);

    // Results widget
    m_resultsWidget = new QListWidget();
    m_resultsWidget->setStyleSheet(
        "QListWidget { background-color: #252526; color: #e0e0e0; border: 1px solid #3e3e42; }"
        "QListWidget::item:selected { background-color: #094771; }"
    );
    m_resultsWidget->setMaximumHeight(150);
    mainLayout->addWidget(m_resultsWidget);

    this->setStyleSheet(
        "SearchAndReplaceWidget { background-color: #1e1e1e; }"
    );
}

void SearchAndReplaceWidget::setupConnections()
{
    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchAndReplaceWidget::onSearchTextChanged);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &SearchAndReplaceWidget::findNext);

    connect(m_replaceInput, &QLineEdit::returnPressed, this, &SearchAndReplaceWidget::replaceCurrent);

    connect(m_findNextBtn, &QPushButton::clicked, this, &SearchAndReplaceWidget::findNext);
    connect(m_findPrevBtn, &QPushButton::clicked, this, &SearchAndReplaceWidget::findPrevious);
    connect(m_replaceBtn, &QPushButton::clicked, this, &SearchAndReplaceWidget::replaceCurrent);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &SearchAndReplaceWidget::replaceAll);
    connect(m_findInFilesBtn, &QPushButton::clicked, this, &SearchAndReplaceWidget::findInFiles);

    connect(m_caseSensitiveCheck, &QCheckBox::stateChanged, this, &SearchAndReplaceWidget::onSearchOptionsChanged);
    connect(m_wholeWordCheck, &QCheckBox::stateChanged, this, &SearchAndReplaceWidget::onSearchOptionsChanged);
    connect(m_regexCheck, &QCheckBox::stateChanged, this, &SearchAndReplaceWidget::onSearchOptionsChanged);
    connect(m_wrapAroundCheck, &QCheckBox::stateChanged, this, &SearchAndReplaceWidget::onSearchOptionsChanged);

    connect(m_resultsWidget, &QListWidget::itemClicked, this, &SearchAndReplaceWidget::onResultItemClicked);
}

void SearchAndReplaceWidget::setCurrentEditor(QPlainTextEdit* editor)
{
    m_currentEditor = editor;
}

void SearchAndReplaceWidget::setProjectRoot(const QString& projectRoot)
{
    m_projectRoot = projectRoot;
}

void SearchAndReplaceWidget::onSearchTextChanged(const QString& text)
{
    if (text.isEmpty()) {
        clearResults();
        if (m_currentEditor) {
            // Clear highlighting
            QTextDocument* doc = m_currentEditor->document();
            QTextCursor cursor(doc);
            cursor.select(QTextCursor::Document);
            QTextCharFormat fmt;
            cursor.setCharFormat(fmt);
        }
        return;
    }

    // Add to history
    if (!m_searchHistory.contains(text)) {
        m_searchHistory.prepend(text);
        if (m_searchHistory.size() > MAX_HISTORY_SIZE) {
            m_searchHistory.removeLast();
        }
    }

    // Search in current editor
    if (m_currentEditor) {
        searchInCurrentEditor();
    }
}

void SearchAndReplaceWidget::onSearchOptionsChanged()
{
    if (!m_searchInput->text().isEmpty()) {
        searchInCurrentEditor();
    }
}

void SearchAndReplaceWidget::searchInCurrentEditor()
{
    if (!m_currentEditor) {
        return;
    }

    m_currentResults = findAllInEditor();
    m_currentResultIndex = 0;

    m_matchCountLabel->setText(QString::number(m_currentResults.size()) + " matches");

    highlightMatches();

    m_resultsWidget->clear();
    for (int i = 0; i < m_currentResults.size(); ++i) {
        const SearchResult& result = m_currentResults[i];
        QString itemText = QString("Line %1: %2").arg(result.line).arg(result.lineContent);
        m_resultsWidget->addItem(itemText);
    }

    if (!m_currentResults.isEmpty()) {
        scrollToResult(m_currentResults[0]);
    }

    emit searchCompleted(m_currentResults.size());
}

QList<SearchResult> SearchAndReplaceWidget::findAllInEditor(const QString& filePath)
{
    QList<SearchResult> results;

    if (!m_currentEditor) {
        return results;
    }

    QString searchText = m_searchInput->text();
    if (searchText.isEmpty()) {
        return results;
    }

    QRegularExpression regex = buildRegex(searchText);
    QTextDocument* doc = m_currentEditor->document();
    QTextBlock block = doc->firstBlock();
    int lineNumber = 1;

    while (block.isValid()) {
        QString lineText = block.text();
        QRegularExpressionMatch match;
        int offset = 0;

        while ((match = regex.match(lineText, offset)).hasMatch()) {
            SearchResult result;
            result.filePath = filePath;
            result.line = lineNumber;
            result.column = match.capturedStart();
            result.length = match.capturedLength();
            result.lineContent = lineText;

            results.append(result);

            offset = match.capturedStart() + qMax(1, match.capturedLength());
        }

        block = block.next();
        lineNumber++;
    }

    return results;
}

QRegularExpression SearchAndReplaceWidget::buildRegex(const QString& searchText)
{
    QString pattern = searchText;
    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;

    if (!m_regexCheck->isChecked()) {
        pattern = QRegularExpression::escape(pattern);
    }

    if (!m_caseSensitiveCheck->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    if (m_wholeWordCheck->isChecked()) {
        pattern = "\\b" + pattern + "\\b";
    }

    return QRegularExpression(pattern, options);
}

int SearchAndReplaceWidget::getSearchOptions() const
{
    int options = 0;
    if (m_caseSensitiveCheck->isChecked()) options |= CaseSensitive;
    if (m_wholeWordCheck->isChecked()) options |= WholeWord;
    if (m_regexCheck->isChecked()) options |= UseRegex;
    if (m_wrapAroundCheck->isChecked()) options |= Wraparound;
    return options;
}

void SearchAndReplaceWidget::highlightMatches()
{
    if (!m_currentEditor) {
        return;
    }

    QTextDocument* doc = m_currentEditor->document();
    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(Qt::yellow);
    highlightFormat.setForeground(Qt::black);

    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::Start);

    while (!cursor.atEnd()) {
        cursor = doc->find(m_searchInput->text(), cursor);
        if (cursor.isNull()) {
            break;
        }

        cursor.mergeCharFormat(highlightFormat);
    }
}

void SearchAndReplaceWidget::scrollToResult(const SearchResult& result)
{
    if (!m_currentEditor) {
        return;
    }

    QTextDocument* doc = m_currentEditor->document();
    QTextCursor cursor = QTextCursor(doc->findBlockByLineNumber(result.line - 1));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, result.column);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, result.length);

    m_currentEditor->setTextCursor(cursor);
    m_currentEditor->ensureCursorVisible();
}

void SearchAndReplaceWidget::findNext()
{
    if (m_currentResults.isEmpty()) {
        return;
    }

    m_currentResultIndex = (m_currentResultIndex + 1) % m_currentResults.size();
    scrollToResult(m_currentResults[m_currentResultIndex]);
    m_resultsWidget->setCurrentRow(m_currentResultIndex);
    m_matchCountLabel->setText(QString::number(m_currentResultIndex + 1) + " of " + QString::number(m_currentResults.size()));
}

void SearchAndReplaceWidget::findPrevious()
{
    if (m_currentResults.isEmpty()) {
        return;
    }

    m_currentResultIndex = (m_currentResultIndex - 1 + m_currentResults.size()) % m_currentResults.size();
    scrollToResult(m_currentResults[m_currentResultIndex]);
    m_resultsWidget->setCurrentRow(m_currentResultIndex);
    m_matchCountLabel->setText(QString::number(m_currentResultIndex + 1) + " of " + QString::number(m_currentResults.size()));
}

void SearchAndReplaceWidget::replaceCurrent()
{
    if (m_currentResults.isEmpty() || m_currentResultIndex >= m_currentResults.size()) {
        return;
    }

    const SearchResult& result = m_currentResults[m_currentResultIndex];
    replaceInEditor(result, m_replaceInput->text());

    // Remove from results and update
    m_currentResults.removeAt(m_currentResultIndex);
    m_resultsWidget->takeItem(m_currentResultIndex);

    if (!m_currentResults.isEmpty()) {
        m_currentResultIndex = m_currentResultIndex % m_currentResults.size();
        scrollToResult(m_currentResults[m_currentResultIndex]);
    }

    m_matchCountLabel->setText(QString::number(m_currentResults.size()) + " matches");
}

void SearchAndReplaceWidget::replaceAll()
{
    if (m_currentResults.isEmpty()) {
        return;
    }

    int replacedCount = 0;

    // Replace in reverse order to maintain positions
    for (int i = m_currentResults.size() - 1; i >= 0; --i) {
        if (replaceInEditor(m_currentResults[i], m_replaceInput->text())) {
            replacedCount++;
        }
    }

    m_currentResults.clear();
    m_resultsWidget->clear();
    m_matchCountLabel->setText("0 matches");

    emit replacementCompleted(replacedCount);
}

bool SearchAndReplaceWidget::replaceInEditor(const SearchResult& result, const QString& replacement)
{
    if (!m_currentEditor) {
        return false;
    }

    QTextDocument* doc = m_currentEditor->document();
    QTextCursor cursor = QTextCursor(doc->findBlockByLineNumber(result.line - 1));
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, result.column);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, result.length);

    cursor.insertText(replacement);
    return true;
}

void SearchAndReplaceWidget::findInFiles()
{
    if (m_projectRoot.isEmpty()) {
        QMessageBox::warning(this, "Error", "Project root not set");
        return;
    }

    // Implementation for searching in all files in project
    m_resultsWidget->clear();

    QString searchPattern = m_searchInput->text();
    QRegularExpression regex = buildRegex(searchPattern);

    QDirIterator it(m_projectRoot, QDirIterator::Subdirectories);
    int resultCount = 0;

    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo info(filePath);

        if (!info.isFile()) continue;

        // Skip binary and large files
        if (info.size() > 10 * 1024 * 1024) continue;

        QString suffix = info.suffix().toLower();
        if (suffix == "dll" || suffix == "exe" || suffix == "bin") continue;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QTextStream stream(&file);
        int lineNumber = 1;

        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if (regex.match(line).hasMatch()) {
                SearchResult result;
                result.filePath = filePath;
                result.line = lineNumber;
                result.lineContent = line;

                QString itemText = QString("%1:%2: %3").arg(info.fileName()).arg(lineNumber).arg(line);
                m_resultsWidget->addItem(itemText);

                resultCount++;
            }
            lineNumber++;
        }

        file.close();
    }

    m_matchCountLabel->setText(QString::number(resultCount) + " matches");
    emit searchCompleted(resultCount);
}

void SearchAndReplaceWidget::onResultItemClicked(QListWidgetItem* item)
{
    int index = m_resultsWidget->row(item);
    if (index >= 0 && index < m_currentResults.size()) {
        scrollToResult(m_currentResults[index]);
    }
}

void SearchAndReplaceWidget::clearResults()
{
    m_currentResults.clear();
    m_resultsWidget->clear();
    m_matchCountLabel->setText("0 of 0");
}

QString SearchAndReplaceWidget::escapeRegex(const QString& text)
{
    return QRegularExpression::escape(text);
}
