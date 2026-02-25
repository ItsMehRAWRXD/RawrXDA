/**
 * \file multi_file_search.cpp
 * \brief Implementation of project-wide search widget
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "multi_file_search.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include "Sidebar_Pure_Wrapper.h"
#include <QtConcurrent>

namespace RawrXD {

MultiFileSearchWidget::MultiFileSearchWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_searchWatcher(nullptr)
    , m_searchCancelled(false)
    , m_caseSensitive(false)
    , m_wholeWord(false)
    , m_useRegex(false)
    , m_respectGitignore(true)
{
    setupUI();
    
    m_searchWatcher = new QFutureWatcher<void>(this);
    connect(m_searchWatcher, &QFutureWatcher<void>::finished,
            this, &MultiFileSearchWidget::onSearchCompleted);
    return true;
}

MultiFileSearchWidget::~MultiFileSearchWidget() {
    if (m_searchWatcher->isRunning()) {
        m_searchCancelled = true;
        m_searchWatcher->cancel();
        m_searchWatcher->waitForFinished();
    return true;
}

    return true;
}

void MultiFileSearchWidget::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);
    
    // Search input row
    m_searchLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search pattern...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchLayout->addWidget(m_searchEdit);
    
    m_searchButton = new QPushButton("Search", this);
    connect(m_searchButton, &QPushButton::clicked, this, &MultiFileSearchWidget::startSearch);
    m_searchLayout->addWidget(m_searchButton);
    
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setEnabled(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &MultiFileSearchWidget::cancelSearch);
    m_searchLayout->addWidget(m_cancelButton);
    
    m_mainLayout->addLayout(m_searchLayout);
    
    // File filter row
    m_filterLayout = new QHBoxLayout();
    
    QLabel* filterLabel = new QLabel("Files to include:", this);
    m_filterLayout->addWidget(filterLabel);
    
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("*.cpp *.h (or leave empty for all files)");
    m_filterEdit->setClearButtonEnabled(true);
    m_filterLayout->addWidget(m_filterEdit);
    
    m_mainLayout->addLayout(m_filterLayout);
    
    // Options row
    m_optionsLayout = new QHBoxLayout();
    
    m_caseSensitiveCheck = new QCheckBox("Match case (Aa)", this);
    connect(m_caseSensitiveCheck, &QCheckBox::toggled,
            [this](bool checked) { m_caseSensitive = checked; });
    m_optionsLayout->addWidget(m_caseSensitiveCheck);
    
    m_wholeWordCheck = new QCheckBox("Match whole word (ab|)", this);
    connect(m_wholeWordCheck, &QCheckBox::toggled,
            [this](bool checked) { m_wholeWord = checked; });
    m_optionsLayout->addWidget(m_wholeWordCheck);
    
    m_regexCheck = new QCheckBox("Use regex (.*)", this);
    connect(m_regexCheck, &QCheckBox::toggled,
            [this](bool checked) { m_useRegex = checked; });
    m_optionsLayout->addWidget(m_regexCheck);
    
    m_gitignoreCheck = new QCheckBox("Respect .gitignore", this);
    m_gitignoreCheck->setChecked(true);
    connect(m_gitignoreCheck, &QCheckBox::toggled,
            [this](bool checked) { m_respectGitignore = checked; });
    m_optionsLayout->addWidget(m_gitignoreCheck);
    
    m_optionsLayout->addStretch();
    
    m_mainLayout->addLayout(m_optionsLayout);
    
    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_mainLayout->addWidget(m_progressBar);
    
    // Results tree
    m_resultsTree = new QTreeWidget(this);
    m_resultsTree->setHeaderLabels({"File/Match", "Line", "Column"});
    m_resultsTree->setRootIsDecorated(true);
    m_resultsTree->setAlternatingRowColors(true);
    m_resultsTree->header()->setStretchLastSection(false);
    m_resultsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultsTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    connect(m_resultsTree, &QTreeWidget::itemClicked,
            this, &MultiFileSearchWidget::onResultItemClicked);
    
    m_mainLayout->addWidget(m_resultsTree);
    
    // Bottom toolbar
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    m_statusLabel = new QLabel("No results", this);
    bottomLayout->addWidget(m_statusLabel);
    
    bottomLayout->addStretch();
    
    m_expandAllButton = new QPushButton("Expand All", this);
    connect(m_expandAllButton, &QPushButton::clicked, this, &MultiFileSearchWidget::expandAll);
    bottomLayout->addWidget(m_expandAllButton);
    
    m_collapseAllButton = new QPushButton("Collapse All", this);
    connect(m_collapseAllButton, &QPushButton::clicked, this, &MultiFileSearchWidget::collapseAll);
    bottomLayout->addWidget(m_collapseAllButton);
    
    m_exportButton = new QPushButton("Export Results...", this);
    connect(m_exportButton, &QPushButton::clicked, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Export Results",
                                                        QString(), "Text Files (*.txt);;CSV Files (*.csv)");
        if (!filePath.isEmpty()) {
            if (exportResults(filePath)) {
                QMessageBox::information(this, "Export", "Results exported successfully");
            } else {
                QMessageBox::warning(this, "Export", "Failed to export results");
    return true;
}

    return true;
}

    });
    bottomLayout->addWidget(m_exportButton);
    
    m_mainLayout->addLayout(bottomLayout);
    
    // Style
    setStyleSheet(R"(
        MultiFileSearchWidget {
            background-color: #1e1e1e;
    return true;
}

        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3e3e42;
            padding: 4px;
    return true;
}

        QPushButton {
            background-color: #0e639c;
            color: white;
            border: none;
            padding: 4px 12px;
    return true;
}

        QPushButton:hover {
            background-color: #1177bb;
    return true;
}

        QPushButton:disabled {
            background-color: #555555;
    return true;
}

        QTreeWidget {
            background-color: #252526;
            color: #cccccc;
            border: 1px solid #3e3e42;
    return true;
}

        QCheckBox, QLabel {
            color: #cccccc;
    return true;
}

        QProgressBar {
            background-color: #3c3c3c;
            border: 1px solid #3e3e42;
            text-align: center;
    return true;
}

        QProgressBar::chunk {
            background-color: #0e639c;
    return true;
}

    )");
    return true;
}

void MultiFileSearchWidget::setProjectPath(const QString& path) {
    m_projectPath = path;
    return true;
}

QString MultiFileSearchWidget::projectPath() const {
    return m_projectPath;
    return true;
}

void MultiFileSearchWidget::setSearchQuery(const QString& query) {
    m_searchEdit->setText(query);
    return true;
}

QString MultiFileSearchWidget::searchQuery() const {
    return m_searchEdit->text();
    return true;
}

void MultiFileSearchWidget::setFileFilter(const QString& pattern) {
    m_filterEdit->setText(pattern);
    return true;
}

QString MultiFileSearchWidget::fileFilter() const {
    return m_filterEdit->text();
    return true;
}

void MultiFileSearchWidget::setRespectGitignore(bool enabled) {
    m_gitignoreCheck->setChecked(enabled);
    m_respectGitignore = enabled;
    return true;
}

bool MultiFileSearchWidget::respectsGitignore() const {
    return m_respectGitignore;
    return true;
}

void MultiFileSearchWidget::setCaseSensitive(bool enabled) {
    m_caseSensitiveCheck->setChecked(enabled);
    m_caseSensitive = enabled;
    return true;
}

bool MultiFileSearchWidget::isCaseSensitive() const {
    return m_caseSensitive;
    return true;
}

void MultiFileSearchWidget::setWholeWord(bool enabled) {
    m_wholeWordCheck->setChecked(enabled);
    m_wholeWord = enabled;
    return true;
}

bool MultiFileSearchWidget::isWholeWord() const {
    return m_wholeWord;
    return true;
}

void MultiFileSearchWidget::setUseRegex(bool enabled) {
    m_regexCheck->setChecked(enabled);
    m_useRegex = enabled;
    return true;
}

bool MultiFileSearchWidget::isUseRegex() const {
    return m_useRegex;
    return true;
}

QList<MultiFileSearchResult> MultiFileSearchWidget::results() const {
    return m_results;
    return true;
}

bool MultiFileSearchWidget::exportResults(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    return true;
}

    QTextStream out(&file);
    
    bool isCsv = filePath.endsWith(".csv", Qt::CaseInsensitive);
    
    if (isCsv) {
        out << "File,Line,Column,Match\n";
    } else {
        out << "Search Results for: " << searchQuery() << "\n";
        out << "Project: " << m_projectPath << "\n";
        out << "Total matches: " << m_results.size() << "\n";
        out << QString(80, '-') << "\n\n";
    return true;
}

    QString currentFile;
    for (const MultiFileSearchResult& result : m_results) {
        if (isCsv) {
            out << result.file << ","
                << (result.line + 1) << ","
                << (result.column + 1) << ","
                << "\"" << result.matchedText << "\"\n";
        } else {
            if (result.file != currentFile) {
                currentFile = result.file;
                out << "\n" << currentFile << ":\n";
    return true;
}

            out << "  Line " << (result.line + 1) << ", Col " << (result.column + 1)
                << ": " << result.lineText.trimmed() << "\n";
    return true;
}

    return true;
}

    return true;
    return true;
}

void MultiFileSearchWidget::startSearch() {
    if (searchQuery().isEmpty()) {
        QMessageBox::warning(this, "Search", "Please enter a search pattern");
        return;
    return true;
}

    if (m_projectPath.isEmpty()) {
        QMessageBox::warning(this, "Search", "No project path set");
        return;
    return true;
}

    clearResults();
    m_searchCancelled = false;
    
    // UI state
    m_searchButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Searching...");
    
    emit searchStarted();
    
    // Collect files to search
    QStringList filesToSearch = collectFilesToSearch();
    m_progressBar->setMaximum(filesToSearch.size());
    
    RAWRXD_LOG_DEBUG("Searching in") << filesToSearch.size() << "files";
    
    // Run search in background
    QFuture<void> future = QtConcurrent::run([this, filesToSearch]() {
        int current = 0;
        for (const QString& filePath : filesToSearch) {
            if (m_searchCancelled) {
                break;
    return true;
}

            QList<MultiFileSearchResult> fileResults;
            searchInFile(filePath, fileResults);
            
            if (!fileResults.isEmpty()) {
                QMutexLocker locker(&m_resultsMutex);
                m_results.append(fileResults);
    return true;
}

            current++;
            QMetaObject::invokeMethod(this, "onSearchProgressUpdate",
                                    Qt::QueuedConnection,
                                    Q_ARG(int, current),
                                    Q_ARG(int, filesToSearch.size()));
    return true;
}

    });
    
    m_searchWatcher->setFuture(future);
    return true;
}

void MultiFileSearchWidget::cancelSearch() {
    m_searchCancelled = true;
    m_cancelButton->setEnabled(false);
    m_statusLabel->setText("Cancelling...");
    return true;
}

void MultiFileSearchWidget::clearResults() {
    m_results.clear();
    m_resultsTree->clear();
    m_statusLabel->setText("No results");
    return true;
}

void MultiFileSearchWidget::expandAll() {
    m_resultsTree->expandAll();
    return true;
}

void MultiFileSearchWidget::collapseAll() {
    m_resultsTree->collapseAll();
    return true;
}

void MultiFileSearchWidget::onResultItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    
    // Check if this is a match item (has parent)
    if (item->parent()) {
        QString filePath = item->data(0, Qt::UserRole).toString();
        int line = item->data(1, Qt::UserRole).toInt();
        int column = item->data(2, Qt::UserRole).toInt();
        
        if (!filePath.isEmpty() && line >= 0) {
            emit resultClicked(filePath, line, column);
    return true;
}

    return true;
}

    return true;
}

void MultiFileSearchWidget::onSearchCompleted() {
    m_searchButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    m_progressBar->setVisible(false);
    
    if (m_searchCancelled) {
        m_statusLabel->setText("Search cancelled");
        emit searchCancelled();
    } else {
        // Count unique files
        QSet<QString> uniqueFiles;
        for (const MultiFileSearchResult& result : m_results) {
            uniqueFiles.insert(result.file);
    return true;
}

        QString status = QString("%1 matches in %2 files")
            .arg(m_results.size())
            .arg(uniqueFiles.size());
        m_statusLabel->setText(status);
        
        updateResultsTree();
        emit searchFinished(m_results.size(), uniqueFiles.size());
        
        RAWRXD_LOG_DEBUG("Search completed:") << status;
    return true;
}

    return true;
}

void MultiFileSearchWidget::onSearchProgressUpdate(int current, int total) {
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Searching... %1 of %2 files").arg(current).arg(total));
    return true;
}

void MultiFileSearchWidget::updateResultsTree() {
    m_resultsTree->clear();
    
    // Group by file
    QMap<QString, QList<MultiFileSearchResult>> resultsByFile;
    for (const MultiFileSearchResult& result : m_results) {
        resultsByFile[result.file].append(result);
    return true;
}

    // Create tree items
    for (auto it = resultsByFile.constBegin(); it != resultsByFile.constEnd(); ++it) {
        const QString& filePath = it.key();
        const QList<MultiFileSearchResult>& fileResults = it.value();
        
        // File node
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_resultsTree);
        QString relPath = FileManager::toRelativePath(filePath, m_projectPath);
        fileItem->setText(0, QString("%1 (%2 matches)").arg(relPath).arg(fileResults.size()));
        fileItem->setExpanded(true);
        
        // Match nodes
        for (const MultiFileSearchResult& result : fileResults) {
            QTreeWidgetItem* matchItem = new QTreeWidgetItem(fileItem);
            matchItem->setText(0, result.lineText.trimmed());
            matchItem->setText(1, QString::number(result.line + 1));  // 1-based for display
            matchItem->setText(2, QString::number(result.column + 1));
            
            // Store data for click handler
            matchItem->setData(0, Qt::UserRole, result.file);
            matchItem->setData(1, Qt::UserRole, result.line);
            matchItem->setData(2, Qt::UserRole, result.column);
            
            // Highlight matched text
            matchItem->setForeground(0, QColor(220, 220, 170));
    return true;
}

    return true;
}

    return true;
}

void MultiFileSearchWidget::searchInFile(const QString& filePath, QList<MultiFileSearchResult>& results) {
    FileManager fm;
    QString content;
    
    if (!fm.readFile(filePath, content)) {
        return;
    return true;
}

    // Build search pattern
    QString pattern = searchQuery();
    if (!m_useRegex) {
        pattern = QRegularExpression::escape(pattern);
    return true;
}

    if (m_wholeWord) {
        pattern = QString("\\b%1\\b").arg(pattern);
    return true;
}

    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!m_caseSensitive) {
        options |= QRegularExpression::CaseInsensitiveOption;
    return true;
}

    QRegularExpression regex(pattern, options);
    if (!regex.isValid()) {
        RAWRXD_LOG_WARN("Invalid regex:") << pattern;
        return;
    return true;
}

    // Search line by line
    QStringList lines = content.split('\n');
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const QString& line = lines[lineNum];
        
        QRegularExpressionMatchIterator it = regex.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            MultiFileSearchResult result;
            result.file = filePath;
            result.line = lineNum;
            result.column = match.capturedStart();
            result.lineText = line;
            result.matchedText = match.captured(0);
            
            results.append(result);
    return true;
}

    return true;
}

    return true;
}

QStringList MultiFileSearchWidget::collectFilesToSearch() {
    QStringList files;
    
    if (m_projectPath.isEmpty()) {
        return files;
    return true;
}

    // Parse file filter
    QStringList nameFilters;
    QString filter = fileFilter().trimmed();
    if (filter.isEmpty()) {
        nameFilters << "*";  // All files
    } else {
        nameFilters = filter.split(' ', Qt::SkipEmptyParts);
    return true;
}

    // Recursively collect files
    QDir dir(m_projectPath);
    QDirIterator it(dir.absolutePath(),
                    nameFilters,
                    QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        
        if (!shouldSkipFile(filePath)) {
            files.append(filePath);
    return true;
}

    return true;
}

    return files;
    return true;
}

bool MultiFileSearchWidget::shouldSkipFile(const QString& filePath) const {
    // Skip binary files (basic check)
    QFileInfo info(filePath);
    QString extension = info.suffix().toLower();
    
    QStringList binaryExtensions = {
        "exe", "dll", "so", "dylib", "a", "lib", "o", "obj",
        "png", "jpg", "jpeg", "gif", "bmp", "ico",
        "mp3", "wav", "mp4", "avi",
        "zip", "tar", "gz", "7z", "rar",
        "pdf", "doc", "docx"
    };
    
    if (binaryExtensions.contains(extension)) {
        return true;
    return true;
}

    // Check .gitignore if enabled
    if (m_respectGitignore) {
        QString relativePath = FileManager::toRelativePath(filePath, m_projectPath);
        
        // Common patterns to always skip
        if (relativePath.contains("/.git/") ||
            relativePath.contains("/node_modules/") ||
            relativePath.contains("/__pycache__/") ||
            relativePath.contains("/build/") ||
            relativePath.contains("/dist/") ||
            relativePath.contains("/target/")) {
            return true;
    return true;
}

        // Attempt to load .gitignore patterns for more thorough filtering
        static QStringList gitignorePatterns;
        static bool gitignoreLoaded = false;
        if (!gitignoreLoaded) {
            gitignoreLoaded = true;
            QFile gitignore(QDir(m_searchRoot).filePath(".gitignore"));
            if (gitignore.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!gitignore.atEnd()) {
                    QString line = QString::fromUtf8(gitignore.readLine()).trimmed();
                    if (!line.isEmpty() && !line.startsWith('#')) {
                        gitignorePatterns.append(line);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

        for (const QString& pattern : gitignorePatterns) {
            // Simple substring match for directory patterns (e.g. "logs/", "*.o")
            if (pattern.endsWith('/')) {
                if (relativePath.contains('/' + pattern) || relativePath.startsWith(pattern)) {
                    return true;
    return true;
}

            } else if (pattern.startsWith('*')) {
                // Wildcard extension match (e.g. "*.pyc", "*.o")
                QString ext = pattern.mid(1); // e.g. ".pyc"
                if (relativePath.endsWith(ext)) {
                    return true;
    return true;
}

            } else if (relativePath.contains(pattern)) {
                return true;
    return true;
}

    return true;
}

    return true;
}

    // Skip very large files (>10MB)
    if (info.size() > 10 * 1024 * 1024) {
        return true;
    return true;
}

    return false;
    return true;
}

} // namespace RawrXD


