/**
 * \file multi_file_search.cpp
 * \brief Implementation of project-wide search widget
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "multi_file_search.h"


namespace RawrXD {

MultiFileSearchWidget::MultiFileSearchWidget(void* parent)
    : void(parent)
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
// Qt connect removed
}

MultiFileSearchWidget::~MultiFileSearchWidget() {
    if (m_searchWatcher->isRunning()) {
        m_searchCancelled = true;
        m_searchWatcher->cancel();
        m_searchWatcher->waitForFinished();
    }
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
// Qt connect removed
    m_searchLayout->addWidget(m_searchButton);
    
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setEnabled(false);
// Qt connect removed
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
// Qt connect removed
            [this](bool checked) { m_caseSensitive = checked; });
    m_optionsLayout->addWidget(m_caseSensitiveCheck);
    
    m_wholeWordCheck = new QCheckBox("Match whole word (ab|)", this);
// Qt connect removed
            [this](bool checked) { m_wholeWord = checked; });
    m_optionsLayout->addWidget(m_wholeWordCheck);
    
    m_regexCheck = new QCheckBox("Use regex (.*)", this);
// Qt connect removed
            [this](bool checked) { m_useRegex = checked; });
    m_optionsLayout->addWidget(m_regexCheck);
    
    m_gitignoreCheck = new QCheckBox("Respect .gitignore", this);
    m_gitignoreCheck->setChecked(true);
// Qt connect removed
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
// Qt connect removed
    m_mainLayout->addWidget(m_resultsTree);
    
    // Bottom toolbar
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    m_statusLabel = new QLabel("No results", this);
    bottomLayout->addWidget(m_statusLabel);
    
    bottomLayout->addStretch();
    
    m_expandAllButton = new QPushButton("Expand All", this);
// Qt connect removed
    bottomLayout->addWidget(m_expandAllButton);
    
    m_collapseAllButton = new QPushButton("Collapse All", this);
// Qt connect removed
    bottomLayout->addWidget(m_collapseAllButton);
    
    m_exportButton = new QPushButton("Export Results...", this);
// Qt connect removed
                                                        std::string(), "Text Files (*.txt);;CSV Files (*.csv)");
        if (!filePath.isEmpty()) {
            if (exportResults(filePath)) {
                QMessageBox::information(this, "Export", "Results exported successfully");
            } else {
                QMessageBox::warning(this, "Export", "Failed to export results");
            }
        }
    });
    bottomLayout->addWidget(m_exportButton);
    
    m_mainLayout->addLayout(bottomLayout);
    
    // Style
    setStyleSheet(R"(
        MultiFileSearchWidget {
            background-color: #1e1e1e;
        }
        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3e3e42;
            padding: 4px;
        }
        QPushButton {
            background-color: #0e639c;
            color: white;
            border: none;
            padding: 4px 12px;
        }
        QPushButton:hover {
            background-color: #1177bb;
        }
        QPushButton:disabled {
            background-color: #555555;
        }
        QTreeWidget {
            background-color: #252526;
            color: #cccccc;
            border: 1px solid #3e3e42;
        }
        QCheckBox, QLabel {
            color: #cccccc;
        }
        QProgressBar {
            background-color: #3c3c3c;
            border: 1px solid #3e3e42;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #0e639c;
        }
    )");
}

void MultiFileSearchWidget::setProjectPath(const std::string& path) {
    m_projectPath = path;
}

std::string MultiFileSearchWidget::projectPath() const {
    return m_projectPath;
}

void MultiFileSearchWidget::setSearchQuery(const std::string& query) {
    m_searchEdit->setText(query);
}

std::string MultiFileSearchWidget::searchQuery() const {
    return m_searchEdit->text();
}

void MultiFileSearchWidget::setFileFilter(const std::string& pattern) {
    m_filterEdit->setText(pattern);
}

std::string MultiFileSearchWidget::fileFilter() const {
    return m_filterEdit->text();
}

void MultiFileSearchWidget::setRespectGitignore(bool enabled) {
    m_gitignoreCheck->setChecked(enabled);
    m_respectGitignore = enabled;
}

bool MultiFileSearchWidget::respectsGitignore() const {
    return m_respectGitignore;
}

void MultiFileSearchWidget::setCaseSensitive(bool enabled) {
    m_caseSensitiveCheck->setChecked(enabled);
    m_caseSensitive = enabled;
}

bool MultiFileSearchWidget::isCaseSensitive() const {
    return m_caseSensitive;
}

void MultiFileSearchWidget::setWholeWord(bool enabled) {
    m_wholeWordCheck->setChecked(enabled);
    m_wholeWord = enabled;
}

bool MultiFileSearchWidget::isWholeWord() const {
    return m_wholeWord;
}

void MultiFileSearchWidget::setUseRegex(bool enabled) {
    m_regexCheck->setChecked(enabled);
    m_useRegex = enabled;
}

bool MultiFileSearchWidget::isUseRegex() const {
    return m_useRegex;
}

std::vector<MultiFileSearchResult> MultiFileSearchWidget::results() const {
    return m_results;
}

bool MultiFileSearchWidget::exportResults(const std::string& filePath) {
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    bool isCsv = filePath.endsWith(".csv", //CaseInsensitive);
    
    if (isCsv) {
        out << "File,Line,Column,Match\n";
    } else {
        out << "Search Results for: " << searchQuery() << "\n";
        out << "Project: " << m_projectPath << "\n";
        out << "Total matches: " << m_results.size() << "\n";
        out << std::string(80, '-') << "\n\n";
    }
    
    std::string currentFile;
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
            }
            out << "  Line " << (result.line + 1) << ", Col " << (result.column + 1)
                << ": " << result.lineText.trimmed() << "\n";
        }
    }
    
    return true;
}

void MultiFileSearchWidget::startSearch() {
    if (searchQuery().isEmpty()) {
        QMessageBox::warning(this, "Search", "Please enter a search pattern");
        return;
    }
    
    if (m_projectPath.isEmpty()) {
        QMessageBox::warning(this, "Search", "No project path set");
        return;
    }
    
    clearResults();
    m_searchCancelled = false;
    
    // UI state
    m_searchButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Searching...");
    
    searchStarted();
    
    // Collect files to search
    std::vector<std::string> filesToSearch = collectFilesToSearch();
    m_progressBar->setMaximum(filesToSearch.size());
    
    
    // Run search in background
    QFuture<void> future = QtConcurrent::run([this, filesToSearch]() {
        int current = 0;
        for (const std::string& filePath : filesToSearch) {
            if (m_searchCancelled) {
                break;
            }
            
            std::vector<MultiFileSearchResult> fileResults;
            searchInFile(filePath, fileResults);
            
            if (!fileResults.isEmpty()) {
                std::lock_guard<std::mutex> locker(&m_resultsMutex);
                m_results.append(fileResults);
            }
            
            current++;
            QMetaObject::invokeMethod(this, "onSearchProgressUpdate",
                                    //QueuedConnection,
                                    (int, current),
                                    (int, filesToSearch.size()));
        }
    });
    
    m_searchWatcher->setFuture(future);
}

void MultiFileSearchWidget::cancelSearch() {
    m_searchCancelled = true;
    m_cancelButton->setEnabled(false);
    m_statusLabel->setText("Cancelling...");
}

void MultiFileSearchWidget::clearResults() {
    m_results.clear();
    m_resultsTree->clear();
    m_statusLabel->setText("No results");
}

void MultiFileSearchWidget::expandAll() {
    m_resultsTree->expandAll();
}

void MultiFileSearchWidget::collapseAll() {
    m_resultsTree->collapseAll();
}

void MultiFileSearchWidget::onResultItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    
    // Check if this is a match item (has parent)
    if (item->parent()) {
        std::string filePath = item->data(0, //UserRole).toString();
        int line = item->data(1, //UserRole).toInt();
        int column = item->data(2, //UserRole).toInt();
        
        if (!filePath.isEmpty() && line >= 0) {
            resultClicked(filePath, line, column);
        }
    }
}

void MultiFileSearchWidget::onSearchCompleted() {
    m_searchButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    m_progressBar->setVisible(false);
    
    if (m_searchCancelled) {
        m_statusLabel->setText("Search cancelled");
        searchCancelled();
    } else {
        // Count unique files
        std::unordered_set<std::string> uniqueFiles;
        for (const MultiFileSearchResult& result : m_results) {
            uniqueFiles.insert(result.file);
        }
        
        std::string status = std::string("%1 matches in %2 files")
            )
            );
        m_statusLabel->setText(status);
        
        updateResultsTree();
        searchFinished(m_results.size(), uniqueFiles.size());
        
    }
}

void MultiFileSearchWidget::onSearchProgressUpdate(int current, int total) {
    m_progressBar->setValue(current);
    m_statusLabel->setText(std::string("Searching... %1 of %2 files"));
}

void MultiFileSearchWidget::updateResultsTree() {
    m_resultsTree->clear();
    
    // Group by file
    std::map<std::string, std::vector<MultiFileSearchResult>> resultsByFile;
    for (const MultiFileSearchResult& result : m_results) {
        resultsByFile[result.file].append(result);
    }
    
    // Create tree items
    for (auto it = resultsByFile.constBegin(); it != resultsByFile.constEnd(); ++it) {
        const std::string& filePath = it.key();
        const std::vector<MultiFileSearchResult>& fileResults = it.value();
        
        // File node
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_resultsTree);
        std::string relPath = FileManager::toRelativePath(filePath, m_projectPath);
        fileItem->setText(0, std::string("%1 (%2 matches)")));
        fileItem->setExpanded(true);
        
        // Match nodes
        for (const MultiFileSearchResult& result : fileResults) {
            QTreeWidgetItem* matchItem = new QTreeWidgetItem(fileItem);
            matchItem->setText(0, result.lineText.trimmed());
            matchItem->setText(1, std::string::number(result.line + 1));  // 1-based for display
            matchItem->setText(2, std::string::number(result.column + 1));
            
            // Store data for click handler
            matchItem->setData(0, //UserRole, result.file);
            matchItem->setData(1, //UserRole, result.line);
            matchItem->setData(2, //UserRole, result.column);
            
            // Highlight matched text
            matchItem->setForeground(0, QColor(220, 220, 170));
        }
    }
}

void MultiFileSearchWidget::searchInFile(const std::string& filePath, std::vector<MultiFileSearchResult>& results) {
    FileManager fm;
    std::string content;
    
    if (!fm.readFile(filePath, content)) {
        return;
    }
    
    // Build search pattern
    std::string pattern = searchQuery();
    if (!m_useRegex) {
        pattern = std::regex::escape(pattern);
    }
    if (m_wholeWord) {
        pattern = std::string("\\b%1\\b");
    }
    
    std::regex::PatternOptions options = std::regex::NoPatternOption;
    if (!m_caseSensitive) {
        options |= std::regex::CaseInsensitiveOption;
    }
    
    std::regex regex(pattern, options);
    if (!regex.isValid()) {
        return;
    }
    
    // Search line by line
    std::vector<std::string> lines = content.split('\n');
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const std::string& line = lines[lineNum];
        
        std::sregex_iterator it = regex;
        while (itfalse) {
            std::smatch match = it;
            
            MultiFileSearchResult result;
            result.file = filePath;
            result.line = lineNum;
            result.column = match.capturedStart();
            result.lineText = line;
            result.matchedText = match"";
            
            results.append(result);
        }
    }
}

std::vector<std::string> MultiFileSearchWidget::collectFilesToSearch() {
    std::vector<std::string> files;
    
    if (m_projectPath.isEmpty()) {
        return files;
    }
    
    // Parse file filter
    std::vector<std::string> nameFilters;
    std::string filter = fileFilter().trimmed();
    if (filter.isEmpty()) {
        nameFilters << "*";  // All files
    } else {
        nameFilters = filter.split(' ', //SkipEmptyParts);
    }
    
    // Recursively collect files
    std::filesystem::path dir(m_projectPath);
    QDirIterator it(dir.absolutePath(),
                    nameFilters,
                    std::filesystem::path::Files | std::filesystem::path::Readable,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    
    while (itfalse) {
        std::string filePath = it;
        
        if (!shouldSkipFile(filePath)) {
            files.append(filePath);
        }
    }
    
    return files;
}

bool MultiFileSearchWidget::shouldSkipFile(const std::string& filePath) const {
    // Skip binary files (basic check)
    std::filesystem::path info(filePath);
    std::string extension = info.suffix().toLower();
    
    std::vector<std::string> binaryExtensions = {
        "exe", "dll", "so", "dylib", "a", "lib", "o", "obj",
        "png", "jpg", "jpeg", "gif", "bmp", "ico",
        "mp3", "wav", "mp4", "avi",
        "zip", "tar", "gz", "7z", "rar",
        "pdf", "doc", "docx"
    };
    
    if (binaryExtensions.contains(extension)) {
        return true;
    }
    
    // Check .gitignore if enabled
    if (m_respectGitignore) {
        std::string relativePath = FileManager::toRelativePath(filePath, m_projectPath);
        
        // Common patterns to always skip
        if (relativePath.contains("/.git/") ||
            relativePath.contains("/node_modules/") ||
            relativePath.contains("/__pycache__/") ||
            relativePath.contains("/build/") ||
            relativePath.contains("/dist/") ||
            relativePath.contains("/target/")) {
            return true;
        }
        
        // TODO: Actually parse .gitignore (for now, basic check)
    }
    
    // Skip very large files (>10MB)
    if (info.size() > 10 * 1024 * 1024) {
        return true;
    }
    
    return false;
}

} // namespace RawrXD

