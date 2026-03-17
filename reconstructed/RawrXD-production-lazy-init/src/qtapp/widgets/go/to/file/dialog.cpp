// go_to_file_dialog.cpp - Implementation
#include "go_to_file_dialog.hpp"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

GoToFileDialog::GoToFileDialog(const QStringList& allFiles, QWidget* parent)
    : QDialog(parent)
    , m_allFiles(allFiles)
    , m_filterTimer(new QTimer(this))
{
    setWindowTitle(tr("Go to File"));
    setModal(true);
    resize(600, 400);
    
    // Apply VS Code dark theme
    setStyleSheet(
        "QDialog { background-color: #252526; }"
        "QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 8px; font-size: 14px; }"
        "QListWidget { background-color: #1e1e1e; color: #e0e0e0; border: none; font-family: 'Consolas', monospace; font-size: 11pt; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:selected { background-color: #094771; }"
        "QListWidget::item:hover { background-color: #2a2d2e; }"
        "QLabel { color: #858585; font-size: 10pt; padding: 4px; }"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Search box
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Type to search files..."));
    connect(m_searchBox, &QLineEdit::textChanged, this, &GoToFileDialog::onSearchTextChanged);
    layout->addWidget(m_searchBox);
    
    // File list
    m_fileList = new QListWidget(this);
    connect(m_fileList, &QListWidget::itemActivated, this, &GoToFileDialog::onItemActivated);
    connect(m_fileList, &QListWidget::itemDoubleClicked, this, &GoToFileDialog::onItemDoubleClicked);
    layout->addWidget(m_fileList);
    
    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);
    
    // Timer for debounced filtering (reduce lag on large file lists)
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(150);
    connect(m_filterTimer, &QTimer::timeout, this, [this]() {
        filterFiles(m_searchBox->text());
    });
    
    // Initial display - show all files (limited)
    filterFiles("");
    
    // Focus search box
    m_searchBox->setFocus();
}

void GoToFileDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
        return;
    }
    
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_fileList->currentItem()) {
            onItemActivated(m_fileList->currentItem());
        }
        return;
    }
    
    if (event->key() == Qt::Key_Down) {
        if (m_fileList->count() > 0) {
            int current = m_fileList->currentRow();
            if (current < m_fileList->count() - 1) {
                m_fileList->setCurrentRow(current + 1);
            }
        }
        event->accept();
        return;
    }
    
    if (event->key() == Qt::Key_Up) {
        if (m_fileList->count() > 0) {
            int current = m_fileList->currentRow();
            if (current > 0) {
                m_fileList->setCurrentRow(current - 1);
            }
        }
        event->accept();
        return;
    }
    
    QDialog::keyPressEvent(event);
}

void GoToFileDialog::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    // Debounce: restart timer on each keystroke
    m_filterTimer->start();
}

void GoToFileDialog::onItemActivated(QListWidgetItem* item)
{
    if (!item) return;
    m_selectedFile = item->data(Qt::UserRole).toString();
    accept();
}

void GoToFileDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    onItemActivated(item);
}

void GoToFileDialog::filterFiles(const QString& pattern)
{
    m_fileList->clear();
    
    if (pattern.isEmpty()) {
        // Show recent/common files first (for now, just show first 100)
        int count = 0;
        for (const QString& filePath : m_allFiles) {
            if (count++ >= 100) break;
            
            QFileInfo info(filePath);
            QListWidgetItem* item = new QListWidgetItem(m_fileList);
            item->setText(QString("%1  (%2)").arg(info.fileName(), info.path()));
            item->setData(Qt::UserRole, filePath);
            m_fileList->addItem(item);
        }
        m_statusLabel->setText(tr("%1 files indexed").arg(m_allFiles.size()));
        return;
    }
    
    // Fuzzy search: score each file and sort by relevance
    struct Match {
        QString filePath;
        int score;
        bool operator<(const Match& other) const { return score > other.score; }  // Higher scores first
    };
    
    QList<Match> matches;
    QString lowerPattern = pattern.toLower();
    
    for (const QString& filePath : m_allFiles) {
        QFileInfo info(filePath);
        QString fileName = info.fileName();
        int score = fuzzyMatchScore(fileName, lowerPattern);
        
        if (score > 0) {
            matches.append({filePath, score});
        }
    }
    
    // Sort by score
    std::sort(matches.begin(), matches.end());
    
    // Display top matches (limit to 100 for performance)
    int displayCount = qMin(100, matches.size());
    for (int i = 0; i < displayCount; ++i) {
        const Match& match = matches[i];
        QFileInfo info(match.filePath);
        
        QListWidgetItem* item = new QListWidgetItem(m_fileList);
        item->setText(QString("%1  (%2)").arg(info.fileName(), info.path()));
        item->setData(Qt::UserRole, match.filePath);
        m_fileList->addItem(item);
    }
    
    if (m_fileList->count() > 0) {
        m_fileList->setCurrentRow(0);
    }
    
    m_statusLabel->setText(tr("%1 of %2 files").arg(displayCount).arg(matches.size()));
}

int GoToFileDialog::fuzzyMatchScore(const QString& fileName, const QString& pattern) const
{
    QString lowerFileName = fileName.toLower();
    QString lowerPattern = pattern.toLower();
    
    // Exact match gets highest score
    if (lowerFileName.contains(lowerPattern)) {
        int exactPos = lowerFileName.indexOf(lowerPattern);
        // Prefer matches at start of filename
        return 1000 - exactPos;
    }
    
    // Fuzzy match: check if all pattern characters appear in order
    int score = 0;
    int patternIdx = 0;
    int lastMatchIdx = -1;
    
    for (int i = 0; i < lowerFileName.length() && patternIdx < lowerPattern.length(); ++i) {
        if (lowerFileName[i] == lowerPattern[patternIdx]) {
            score += 10;
            
            // Bonus for consecutive matches
            if (lastMatchIdx == i - 1) {
                score += 5;
            }
            
            // Bonus for matches after path separators or uppercase letters in camelCase
            if (i > 0 && (fileName[i-1] == '/' || fileName[i-1] == '\\' || 
                         (fileName[i].isUpper() && fileName[i-1].isLower()))) {
                score += 15;
            }
            
            lastMatchIdx = i;
            patternIdx++;
        }
    }
    
    // Must match all pattern characters
    if (patternIdx < lowerPattern.length()) {
        return 0;
    }
    
    // Penalty for long filenames (prefer shorter, more specific matches)
    score -= fileName.length() / 10;
    
    return qMax(1, score);
}
