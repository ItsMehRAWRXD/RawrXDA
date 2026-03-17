/**
 * @file regex_tester_widget.cpp
 * @brief Full Regex Tester Widget implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "regex_tester_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// =============================================================================
// MatchHighlighter Implementation
// =============================================================================

MatchHighlighter::MatchHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Default match colors
    m_colors = {
        QColor(255, 255, 0, 100),   // Yellow
        QColor(0, 255, 255, 100),   // Cyan
        QColor(255, 0, 255, 100),   // Magenta
        QColor(0, 255, 0, 100),     // Green
        QColor(255, 128, 0, 100),   // Orange
    };
}

void MatchHighlighter::setMatches(const QVector<RegexMatch>& matches) {
    m_matches = matches;
    rehighlight();
}

void MatchHighlighter::setCurrentMatchIndex(int index) {
    m_currentMatchIndex = index;
    rehighlight();
}

void MatchHighlighter::setColors(const QVector<QColor>& colors) {
    m_colors = colors;
    rehighlight();
}

void MatchHighlighter::highlightBlock(const QString& text) {
    int blockStart = currentBlock().position();
    int blockEnd = blockStart + text.length();
    
    for (int i = 0; i < m_matches.size(); ++i) {
        const RegexMatch& match = m_matches[i];
        
        if (match.end < blockStart || match.start >= blockEnd) {
            continue;  // Match not in this block
        }
        
        int start = qMax(match.start - blockStart, 0);
        int end = qMin(match.end - blockStart, text.length());
        int length = end - start;
        
        if (length <= 0) continue;
        
        QTextCharFormat fmt;
        QColor color = m_colors[i % m_colors.size()];
        
        if (i == m_currentMatchIndex) {
            color.setAlpha(200);
            fmt.setFontWeight(QFont::Bold);
        }
        
        fmt.setBackground(color);
        setFormat(start, length, fmt);
    }
}

// =============================================================================
// RegexSyntaxHighlighter Implementation
// =============================================================================

RegexSyntaxHighlighter::RegexSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    HighlightRule rule;
    
    // Character classes
    QTextCharFormat charClassFormat;
    charClassFormat.setForeground(QColor("#4ec9b0"));
    rule.pattern = QRegularExpression(R"(\[[^\]]+\])");
    rule.format = charClassFormat;
    m_rules.append(rule);
    
    // Escape sequences
    QTextCharFormat escapeFormat;
    escapeFormat.setForeground(QColor("#ce9178"));
    rule.pattern = QRegularExpression(R"(\\[dDwWsSnrtbfvx0-9])");
    rule.format = escapeFormat;
    m_rules.append(rule);
    
    // Quantifiers
    QTextCharFormat quantifierFormat;
    quantifierFormat.setForeground(QColor("#569cd6"));
    quantifierFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(R"([*+?]|\{\d+(?:,\d*)?\})");
    rule.format = quantifierFormat;
    m_rules.append(rule);
    
    // Groups
    QTextCharFormat groupFormat;
    groupFormat.setForeground(QColor("#dcdcaa"));
    rule.pattern = QRegularExpression(R"(\((?:\?[<:!=])?|\))");
    rule.format = groupFormat;
    m_rules.append(rule);
    
    // Anchors
    QTextCharFormat anchorFormat;
    anchorFormat.setForeground(QColor("#c586c0"));
    rule.pattern = QRegularExpression(R"([\^$]|\\[bBAZz])");
    rule.format = anchorFormat;
    m_rules.append(rule);
    
    // Alternation
    QTextCharFormat altFormat;
    altFormat.setForeground(QColor("#d4d4d4"));
    altFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(R"(\|)");
    rule.format = altFormat;
    m_rules.append(rule);
}

void RegexSyntaxHighlighter::highlightBlock(const QString& text) {
    for (const HighlightRule& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// =============================================================================
// RegexTesterWidget Implementation
// =============================================================================

RegexTesterWidget::RegexTesterWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    connectSignals();
    loadPatternLibrary();
    loadHistory();
    
    // Debounce pattern updates
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(200);
    connect(m_updateTimer, &QTimer::timeout, this, &RegexTesterWidget::updateMatches);
}

RegexTesterWidget::~RegexTesterWidget() {
    saveHistory();
}

void RegexTesterWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Left panel: Pattern + Test input + Results
    QWidget* leftPanel = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    m_leftSplitter = new QSplitter(Qt::Vertical, this);
    
    setupPatternInput();
    setupTestInput();
    setupResultsView();
    setupReplacementPanel();
    
    leftLayout->addWidget(m_leftSplitter);
    
    // Right panel: Pattern library + Explanation
    m_rightTabWidget = new QTabWidget(this);
    setupPatternLibrary();
    
    // Explanation tab
    m_explanationView = new QTextEdit(this);
    m_explanationView->setReadOnly(true);
    m_explanationView->setStyleSheet(
        "QTextEdit { background-color: #252526; color: #d4d4d4; font-family: Consolas; }");
    m_rightTabWidget->addTab(m_explanationView, "Explanation");
    
    m_mainSplitter->addWidget(leftPanel);
    m_mainSplitter->addWidget(m_rightTabWidget);
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_mainSplitter);
}

void RegexTesterWidget::setupToolbar() {
    m_toolbar = new QToolBar("Regex Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    m_matchBtn = new QPushButton("Match", this);
    m_matchBtn->setToolTip("Run regex match (Ctrl+Enter)");
    connect(m_matchBtn, &QPushButton::clicked, this, &RegexTesterWidget::runMatch);
    m_toolbar->addWidget(m_matchBtn);
    
    m_replaceBtn = new QPushButton("Replace", this);
    m_replaceBtn->setToolTip("Preview replacement");
    connect(m_replaceBtn, &QPushButton::clicked, this, &RegexTesterWidget::runReplace);
    m_toolbar->addWidget(m_replaceBtn);
    
    m_toolbar->addSeparator();
    
    // Match mode
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Match All", MatchAll);
    m_modeCombo->addItem("Match First", MatchFirst);
    m_modeCombo->addItem("Full Match", MatchFullText);
    m_modeCombo->setToolTip("Match mode");
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_modeCombo);
    
    m_toolbar->addSeparator();
    
    // Options checkboxes
    m_caseSensitiveCheck = new QCheckBox("Case", this);
    m_caseSensitiveCheck->setToolTip("Case sensitive matching");
    m_caseSensitiveCheck->setChecked(true);
    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_caseSensitiveCheck);
    
    m_multilineCheck = new QCheckBox("Multi", this);
    m_multilineCheck->setToolTip("Multiline mode (^ and $ match line boundaries)");
    connect(m_multilineCheck, &QCheckBox::toggled, this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_multilineCheck);
    
    m_dotAllCheck = new QCheckBox("DotAll", this);
    m_dotAllCheck->setToolTip("Dot matches newlines");
    connect(m_dotAllCheck, &QCheckBox::toggled, this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_dotAllCheck);
    
    m_extendedCheck = new QCheckBox("Extended", this);
    m_extendedCheck->setToolTip("Extended pattern syntax (ignore whitespace and comments)");
    connect(m_extendedCheck, &QCheckBox::toggled, this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_extendedCheck);
    
    m_unicodeCheck = new QCheckBox("Unicode", this);
    m_unicodeCheck->setToolTip("Enable Unicode property support");
    m_unicodeCheck->setChecked(true);
    connect(m_unicodeCheck, &QCheckBox::toggled, this, &RegexTesterWidget::onOptionsChanged);
    m_toolbar->addWidget(m_unicodeCheck);
    
    m_toolbar->addSeparator();
    
    // Navigation
    m_prevMatchBtn = new QPushButton("◀", this);
    m_prevMatchBtn->setToolTip("Previous match");
    m_prevMatchBtn->setMaximumWidth(30);
    connect(m_prevMatchBtn, &QPushButton::clicked, this, &RegexTesterWidget::navigatePrevious);
    m_toolbar->addWidget(m_prevMatchBtn);
    
    m_matchCountLabel = new QLabel("0 matches", this);
    m_matchCountLabel->setMinimumWidth(80);
    m_toolbar->addWidget(m_matchCountLabel);
    
    m_nextMatchBtn = new QPushButton("▶", this);
    m_nextMatchBtn->setToolTip("Next match");
    m_nextMatchBtn->setMaximumWidth(30);
    connect(m_nextMatchBtn, &QPushButton::clicked, this, &RegexTesterWidget::navigateNext);
    m_toolbar->addWidget(m_nextMatchBtn);
    
    m_toolbar->addSeparator();
    
    // Export
    QPushButton* exportBtn = new QPushButton("Export", this);
    QMenu* exportMenu = new QMenu(this);
    exportMenu->addAction("Copy Matches", this, &RegexTesterWidget::copyMatches);
    exportMenu->addAction("Export to JSON...", this, &RegexTesterWidget::exportMatches);
    exportBtn->setMenu(exportMenu);
    m_toolbar->addWidget(exportBtn);
}

void RegexTesterWidget::setupPatternInput() {
    QGroupBox* patternGroup = new QGroupBox("Pattern", this);
    QVBoxLayout* layout = new QVBoxLayout(patternGroup);
    
    m_patternEdit = new QTextEdit(this);
    m_patternEdit->setMaximumHeight(80);
    m_patternEdit->setPlaceholderText("Enter regex pattern here...");
    m_patternEdit->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "font-family: Consolas; font-size: 14px; }");
    m_patternHighlighter = new RegexSyntaxHighlighter(m_patternEdit->document());
    
    m_patternStatusLabel = new QLabel(this);
    m_patternStatusLabel->setStyleSheet("QLabel { color: #4ec9b0; }");
    
    layout->addWidget(m_patternEdit);
    layout->addWidget(m_patternStatusLabel);
    
    m_leftSplitter->addWidget(patternGroup);
}

void RegexTesterWidget::setupTestInput() {
    QGroupBox* testGroup = new QGroupBox("Test String", this);
    QVBoxLayout* layout = new QVBoxLayout(testGroup);
    
    m_testInput = new QPlainTextEdit(this);
    m_testInput->setPlaceholderText("Enter test text here...");
    m_testInput->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "font-family: Consolas; font-size: 13px; }");
    m_matchHighlighter = new MatchHighlighter(m_testInput->document());
    
    layout->addWidget(m_testInput);
    m_leftSplitter->addWidget(testGroup);
}

void RegexTesterWidget::setupResultsView() {
    QGroupBox* resultsGroup = new QGroupBox("Matches", this);
    QVBoxLayout* layout = new QVBoxLayout(resultsGroup);
    
    QTabWidget* resultsTabs = new QTabWidget(this);
    
    // Match table
    m_matchTable = new QTableWidget(this);
    m_matchTable->setColumnCount(4);
    m_matchTable->setHorizontalHeaderLabels({"#", "Start", "End", "Match"});
    m_matchTable->horizontalHeader()->setStretchLastSection(true);
    m_matchTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_matchTable->setStyleSheet(
        "QTableWidget { background-color: #252526; color: #d4d4d4; }");
    
    connect(m_matchTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        navigateToMatch(row);
    });
    
    resultsTabs->addTab(m_matchTable, "All Matches");
    
    // Group tree
    m_groupTree = new QTreeWidget(this);
    m_groupTree->setHeaderLabels({"Group", "Value"});
    m_groupTree->setColumnCount(2);
    m_groupTree->header()->setStretchLastSection(true);
    m_groupTree->setStyleSheet(
        "QTreeWidget { background-color: #252526; color: #d4d4d4; }");
    
    resultsTabs->addTab(m_groupTree, "Capture Groups");
    
    layout->addWidget(resultsTabs);
    m_leftSplitter->addWidget(resultsGroup);
}

void RegexTesterWidget::setupReplacementPanel() {
    QGroupBox* replaceGroup = new QGroupBox("Replacement", this);
    QVBoxLayout* layout = new QVBoxLayout(replaceGroup);
    
    QHBoxLayout* replaceInputLayout = new QHBoxLayout();
    m_replacementEdit = new QLineEdit(this);
    m_replacementEdit->setPlaceholderText("Replacement pattern (use $1, $2, etc. for groups)");
    m_replacementEdit->setStyleSheet(
        "QLineEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: Consolas; }");
    
    m_copyReplacementBtn = new QPushButton("Copy", this);
    connect(m_copyReplacementBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_replacementPreview->toPlainText());
    });
    
    replaceInputLayout->addWidget(m_replacementEdit);
    replaceInputLayout->addWidget(m_copyReplacementBtn);
    
    m_replacementPreview = new QTextEdit(this);
    m_replacementPreview->setReadOnly(true);
    m_replacementPreview->setMaximumHeight(100);
    m_replacementPreview->setStyleSheet(
        "QTextEdit { background-color: #252526; color: #4ec9b0; font-family: Consolas; }");
    
    layout->addLayout(replaceInputLayout);
    layout->addWidget(m_replacementPreview);
    m_leftSplitter->addWidget(replaceGroup);
}

void RegexTesterWidget::setupPatternLibrary() {
    // Library tree
    m_libraryTree = new QTreeWidget(this);
    m_libraryTree->setHeaderLabels({"Pattern", "Description"});
    m_libraryTree->header()->setStretchLastSection(true);
    m_libraryTree->setStyleSheet(
        "QTreeWidget { background-color: #252526; color: #d4d4d4; }");
    
    connect(m_libraryTree, &QTreeWidget::itemDoubleClicked,
            this, &RegexTesterWidget::onPatternLibraryItemSelected);
    
    m_rightTabWidget->addTab(m_libraryTree, "Library");
    
    // History list
    m_historyList = new QListWidget(this);
    m_historyList->setStyleSheet(
        "QListWidget { background-color: #252526; color: #d4d4d4; }");
    
    connect(m_historyList, &QListWidget::itemDoubleClicked,
            this, &RegexTesterWidget::onHistoryItemSelected);
    
    m_rightTabWidget->addTab(m_historyList, "History");
    
    // Favorites list
    m_favoritesList = new QListWidget(this);
    m_favoritesList->setStyleSheet(
        "QListWidget { background-color: #252526; color: #d4d4d4; }");
    m_rightTabWidget->addTab(m_favoritesList, "Favorites");
}

void RegexTesterWidget::connectSignals() {
    connect(m_patternEdit, &QTextEdit::textChanged, this, &RegexTesterWidget::onPatternChanged);
    connect(m_testInput, &QPlainTextEdit::textChanged, this, &RegexTesterWidget::onTestTextChanged);
    connect(m_replacementEdit, &QLineEdit::textChanged, this, &RegexTesterWidget::updateReplacementPreview);
}

void RegexTesterWidget::loadPatternLibrary() {
    // Common patterns organized by category
    QMap<QString, QVector<PatternInfo>> categories;
    
    // Email & URLs
    categories["Email & URLs"] = {
        {"[\\w.-]+@[\\w.-]+\\.\\w+", "Email address", QRegularExpression::NoPatternOption, "Email & URLs"},
        {"https?://[\\w.-]+(?:/[\\w./-]*)?", "HTTP/HTTPS URL", QRegularExpression::NoPatternOption, "Email & URLs"},
        {"(?:https?://)?(?:www\\.)?([\\w-]+)\\.([\\w.]+)", "Domain name", QRegularExpression::NoPatternOption, "Email & URLs"},
    };
    
    // Numbers
    categories["Numbers"] = {
        {"\\d+", "Integer", QRegularExpression::NoPatternOption, "Numbers"},
        {"-?\\d+\\.\\d+", "Decimal number", QRegularExpression::NoPatternOption, "Numbers"},
        {"0x[0-9A-Fa-f]+", "Hexadecimal", QRegularExpression::NoPatternOption, "Numbers"},
        {"\\b\\d{1,3}(?:,\\d{3})*(?:\\.\\d+)?\\b", "Number with commas", QRegularExpression::NoPatternOption, "Numbers"},
    };
    
    // Dates & Times
    categories["Dates & Times"] = {
        {"\\d{4}-\\d{2}-\\d{2}", "ISO date (YYYY-MM-DD)", QRegularExpression::NoPatternOption, "Dates & Times"},
        {"\\d{2}/\\d{2}/\\d{4}", "US date (MM/DD/YYYY)", QRegularExpression::NoPatternOption, "Dates & Times"},
        {"\\d{2}:\\d{2}(?::\\d{2})?", "Time (HH:MM or HH:MM:SS)", QRegularExpression::NoPatternOption, "Dates & Times"},
        {"\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?(?:Z|[+-]\\d{2}:\\d{2})?", "ISO 8601 datetime", QRegularExpression::NoPatternOption, "Dates & Times"},
    };
    
    // Code & Programming
    categories["Code & Programming"] = {
        {"//.*$", "Single-line comment (C++)", QRegularExpression::MultilineOption, "Code & Programming"},
        {"/\\*[\\s\\S]*?\\*/", "Multi-line comment", QRegularExpression::NoPatternOption, "Code & Programming"},
        {"\"(?:[^\"\\\\]|\\\\.)*\"", "Double-quoted string", QRegularExpression::NoPatternOption, "Code & Programming"},
        {"'(?:[^'\\\\]|\\\\.)*'", "Single-quoted string", QRegularExpression::NoPatternOption, "Code & Programming"},
        {"\\b[A-Z][a-zA-Z0-9]*\\b", "PascalCase identifier", QRegularExpression::NoPatternOption, "Code & Programming"},
        {"\\b[a-z][a-zA-Z0-9]*\\b", "camelCase identifier", QRegularExpression::NoPatternOption, "Code & Programming"},
        {"\\b[A-Z][A-Z0-9_]*\\b", "SCREAMING_SNAKE_CASE", QRegularExpression::NoPatternOption, "Code & Programming"},
    };
    
    // Validation
    categories["Validation"] = {
        {"^[a-zA-Z0-9_]{3,16}$", "Username (3-16 chars)", QRegularExpression::NoPatternOption, "Validation"},
        {"^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d).{8,}$", "Strong password", QRegularExpression::NoPatternOption, "Validation"},
        {"^\\+?[1-9]\\d{1,14}$", "Phone number (E.164)", QRegularExpression::NoPatternOption, "Validation"},
        {"^[0-9]{5}(?:-[0-9]{4})?$", "US ZIP code", QRegularExpression::NoPatternOption, "Validation"},
        {"^[A-Z]{2}\\d{2}[A-Z0-9]{4}\\d{7}(?:[A-Z0-9]?){0,16}$", "IBAN", QRegularExpression::NoPatternOption, "Validation"},
    };
    
    // Network
    categories["Network"] = {
        {"\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b", "IPv4 address", QRegularExpression::NoPatternOption, "Network"},
        {"\\b[0-9a-fA-F]{2}(?:[:-][0-9a-fA-F]{2}){5}\\b", "MAC address", QRegularExpression::NoPatternOption, "Network"},
        {"\\b(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b", "Valid IPv4 address", QRegularExpression::NoPatternOption, "Network"},
    };
    
    // Populate tree
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        QTreeWidgetItem* categoryItem = new QTreeWidgetItem(m_libraryTree);
        categoryItem->setText(0, it.key());
        categoryItem->setExpanded(true);
        
        for (const PatternInfo& info : it.value()) {
            QTreeWidgetItem* patternItem = new QTreeWidgetItem(categoryItem);
            patternItem->setText(0, info.pattern);
            patternItem->setText(1, info.description);
            patternItem->setData(0, Qt::UserRole, QVariant::fromValue(info.pattern));
            m_patternLibrary.append(info);
        }
    }
}

void RegexTesterWidget::onPatternChanged() {
    m_updateTimer->start();
    emit patternChanged(m_patternEdit->toPlainText());
}

void RegexTesterWidget::onTestTextChanged() {
    m_updateTimer->start();
}

void RegexTesterWidget::onOptionsChanged() {
    m_matchMode = static_cast<MatchMode>(m_modeCombo->currentData().toInt());
    m_updateTimer->start();
}

void RegexTesterWidget::updateMatches() {
    QString pattern = m_patternEdit->toPlainText().trimmed();
    QString testText = m_testInput->toPlainText();
    
    m_matches.clear();
    m_matchTable->setRowCount(0);
    m_groupTree->clear();
    
    if (pattern.isEmpty()) {
        m_patternStatusLabel->setText("");
        m_matchCountLabel->setText("0 matches");
        m_matchHighlighter->setMatches(m_matches);
        return;
    }
    
    QRegularExpression regex(pattern, getCurrentOptions());
    
    if (!regex.isValid()) {
        m_patternStatusLabel->setText("Error: " + regex.errorString());
        m_patternStatusLabel->setStyleSheet("QLabel { color: #f44747; }");
        emit patternError(regex.errorString());
        return;
    }
    
    m_patternStatusLabel->setText("Valid pattern");
    m_patternStatusLabel->setStyleSheet("QLabel { color: #4ec9b0; }");
    
    // Add to history
    addToHistory(pattern);
    
    // Perform matching
    if (m_matchMode == MatchFullText) {
        QRegularExpressionMatch match = regex.match(testText, 0, QRegularExpression::NormalMatch, 
                                                     QRegularExpression::AnchorAtOffsetMatchOption);
        if (match.hasMatch() && match.captured(0) == testText) {
            RegexMatch rm;
            rm.start = match.capturedStart();
            rm.end = match.capturedEnd();
            rm.text = match.captured(0);
            
            for (int i = 0; i <= match.lastCapturedIndex(); ++i) {
                QString name = regex.namedCaptureGroups().value(i, QString::number(i));
                rm.groups.append({name, match.captured(i)});
            }
            m_matches.append(rm);
        }
    } else {
        QRegularExpressionMatchIterator it = regex.globalMatch(testText);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            RegexMatch rm;
            rm.start = match.capturedStart();
            rm.end = match.capturedEnd();
            rm.text = match.captured(0);
            
            // Capture groups
            QStringList namedGroups = regex.namedCaptureGroups();
            for (int i = 0; i <= match.lastCapturedIndex(); ++i) {
                QString name = (i < namedGroups.size() && !namedGroups[i].isEmpty()) 
                    ? namedGroups[i] : QString::number(i);
                rm.groups.append({name, match.captured(i)});
            }
            
            m_matches.append(rm);
            
            if (m_matchMode == MatchFirst) break;
        }
    }
    
    // Update UI
    updateMatchDisplay();
    highlightMatches();
    updateExplanation();
    updateReplacementPreview();
    
    m_currentMatchIndex = 0;
    emit matchesUpdated(m_matches.size());
}

void RegexTesterWidget::updateMatchDisplay() {
    m_matchCountLabel->setText(QString("%1 match%2")
        .arg(m_matches.size())
        .arg(m_matches.size() == 1 ? "" : "es"));
    
    // Populate match table
    m_matchTable->setRowCount(m_matches.size());
    for (int i = 0; i < m_matches.size(); ++i) {
        const RegexMatch& match = m_matches[i];
        m_matchTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        m_matchTable->setItem(i, 1, new QTableWidgetItem(QString::number(match.start)));
        m_matchTable->setItem(i, 2, new QTableWidgetItem(QString::number(match.end)));
        m_matchTable->setItem(i, 3, new QTableWidgetItem(match.text));
    }
    
    // Populate group tree
    m_groupTree->clear();
    for (int i = 0; i < m_matches.size(); ++i) {
        const RegexMatch& match = m_matches[i];
        QTreeWidgetItem* matchItem = new QTreeWidgetItem(m_groupTree);
        matchItem->setText(0, QString("Match %1").arg(i + 1));
        matchItem->setText(1, match.text);
        matchItem->setExpanded(true);
        
        for (const auto& group : match.groups) {
            QTreeWidgetItem* groupItem = new QTreeWidgetItem(matchItem);
            groupItem->setText(0, group.first);
            groupItem->setText(1, group.second);
        }
    }
}

void RegexTesterWidget::highlightMatches() {
    m_matchHighlighter->setMatches(m_matches);
    m_matchHighlighter->setCurrentMatchIndex(m_currentMatchIndex);
}

void RegexTesterWidget::updateReplacementPreview() {
    QString replacement = m_replacementEdit->text();
    if (replacement.isEmpty() || m_matches.isEmpty()) {
        m_replacementPreview->clear();
        return;
    }
    
    QString pattern = m_patternEdit->toPlainText().trimmed();
    QString testText = m_testInput->toPlainText();
    
    QRegularExpression regex(pattern, getCurrentOptions());
    if (!regex.isValid()) return;
    
    QString result = testText;
    result.replace(regex, replacement);
    
    m_replacementPreview->setPlainText(result);
    emit replacementReady(result);
}

void RegexTesterWidget::updateExplanation() {
    QString pattern = m_patternEdit->toPlainText().trimmed();
    m_explanationView->setHtml(explainPattern(pattern));
}

QString RegexTesterWidget::explainPattern(const QString& pattern) {
    if (pattern.isEmpty()) {
        return "<p style='color: #808080;'>Enter a pattern to see its explanation.</p>";
    }
    
    QString html = "<style>"
                   "body { font-family: Consolas; color: #d4d4d4; }"
                   ".token { padding: 2px 4px; margin: 2px; border-radius: 3px; }"
                   ".literal { background-color: #2d2d2d; }"
                   ".meta { background-color: #264f78; }"
                   ".quantifier { background-color: #4d4d00; }"
                   ".group { background-color: #3d3d00; }"
                   ".anchor { background-color: #4d004d; }"
                   ".charclass { background-color: #004d4d; }"
                   "</style>";
    
    html += "<h3>Pattern Breakdown</h3><ul>";
    
    // Simple pattern explanation (a full implementation would use a proper parser)
    int i = 0;
    while (i < pattern.length()) {
        QChar c = pattern[i];
        
        if (c == '\\' && i + 1 < pattern.length()) {
            QChar next = pattern[i + 1];
            QString escaped = QString("\\") + next;
            QString meaning;
            
            switch (next.toLatin1()) {
                case 'd': meaning = "Any digit [0-9]"; break;
                case 'D': meaning = "Any non-digit"; break;
                case 'w': meaning = "Word character [a-zA-Z0-9_]"; break;
                case 'W': meaning = "Non-word character"; break;
                case 's': meaning = "Whitespace"; break;
                case 'S': meaning = "Non-whitespace"; break;
                case 'b': meaning = "Word boundary"; break;
                case 'B': meaning = "Non-word boundary"; break;
                case 'n': meaning = "Newline"; break;
                case 'r': meaning = "Carriage return"; break;
                case 't': meaning = "Tab"; break;
                default: meaning = QString("Literal '%1'").arg(next);
            }
            
            html += QString("<li><span class='token meta'>%1</span> - %2</li>")
                .arg(escaped.toHtmlEscaped(), meaning);
            i += 2;
        }
        else if (c == '[') {
            int end = pattern.indexOf(']', i);
            if (end != -1) {
                QString charClass = pattern.mid(i, end - i + 1);
                html += QString("<li><span class='token charclass'>%1</span> - Character class</li>")
                    .arg(charClass.toHtmlEscaped());
                i = end + 1;
            } else {
                html += QString("<li><span class='token literal'>%1</span> - Literal character</li>")
                    .arg(c);
                i++;
            }
        }
        else if (c == '(' || c == ')') {
            html += QString("<li><span class='token group'>%1</span> - %2</li>")
                .arg(c)
                .arg(c == '(' ? "Start of group" : "End of group");
            i++;
        }
        else if (c == '*' || c == '+' || c == '?') {
            QString meaning = (c == '*') ? "Zero or more" :
                             (c == '+') ? "One or more" : "Zero or one";
            html += QString("<li><span class='token quantifier'>%1</span> - %2</li>")
                .arg(c).arg(meaning);
            i++;
        }
        else if (c == '{') {
            int end = pattern.indexOf('}', i);
            if (end != -1) {
                QString quant = pattern.mid(i, end - i + 1);
                html += QString("<li><span class='token quantifier'>%1</span> - Repetition count</li>")
                    .arg(quant.toHtmlEscaped());
                i = end + 1;
            } else {
                html += QString("<li><span class='token literal'>%1</span> - Literal</li>").arg(c);
                i++;
            }
        }
        else if (c == '^' || c == '$') {
            QString meaning = (c == '^') ? "Start of string/line" : "End of string/line";
            html += QString("<li><span class='token anchor'>%1</span> - %2</li>")
                .arg(c).arg(meaning);
            i++;
        }
        else if (c == '.') {
            html += QString("<li><span class='token meta'>.</span> - Any character%1</li>")
                .arg(m_dotAllCheck->isChecked() ? " (including newline)" : " (except newline)");
            i++;
        }
        else if (c == '|') {
            html += QString("<li><span class='token meta'>|</span> - Alternation (OR)</li>");
            i++;
        }
        else {
            html += QString("<li><span class='token literal'>%1</span> - Literal character</li>")
                .arg(QString(c).toHtmlEscaped());
            i++;
        }
    }
    
    html += "</ul>";
    return html;
}

QRegularExpression::PatternOptions RegexTesterWidget::getCurrentOptions() const {
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    
    if (!m_caseSensitiveCheck->isChecked()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }
    if (m_multilineCheck->isChecked()) {
        options |= QRegularExpression::MultilineOption;
    }
    if (m_dotAllCheck->isChecked()) {
        options |= QRegularExpression::DotMatchesEverythingOption;
    }
    if (m_extendedCheck->isChecked()) {
        options |= QRegularExpression::ExtendedPatternSyntaxOption;
    }
    if (m_unicodeCheck->isChecked()) {
        options |= QRegularExpression::UseUnicodePropertiesOption;
    }
    
    return options;
}

// =============================================================================
// Public API
// =============================================================================

void RegexTesterWidget::setPattern(const QString& pattern) {
    m_patternEdit->setPlainText(pattern);
}

QString RegexTesterWidget::getPattern() const {
    return m_patternEdit->toPlainText();
}

void RegexTesterWidget::setTestText(const QString& text) {
    m_testInput->setPlainText(text);
}

QString RegexTesterWidget::getTestText() const {
    return m_testInput->toPlainText();
}

void RegexTesterWidget::setReplacement(const QString& replacement) {
    m_replacementEdit->setText(replacement);
}

QString RegexTesterWidget::getReplacement() const {
    return m_replacementEdit->text();
}

QString RegexTesterWidget::getReplacementResult() const {
    return m_replacementPreview->toPlainText();
}

void RegexTesterWidget::setMatchMode(MatchMode mode) {
    m_matchMode = mode;
    m_modeCombo->setCurrentIndex(m_modeCombo->findData(mode));
}

void RegexTesterWidget::setCaseSensitive(bool sensitive) {
    m_caseSensitiveCheck->setChecked(sensitive);
}

void RegexTesterWidget::setMultilineMode(bool enabled) {
    m_multilineCheck->setChecked(enabled);
}

void RegexTesterWidget::setDotMatchesAll(bool enabled) {
    m_dotAllCheck->setChecked(enabled);
}

void RegexTesterWidget::setExtendedSyntax(bool enabled) {
    m_extendedCheck->setChecked(enabled);
}

void RegexTesterWidget::setUnicodeMode(bool enabled) {
    m_unicodeCheck->setChecked(enabled);
}

// =============================================================================
// Slots
// =============================================================================

void RegexTesterWidget::runMatch() {
    updateMatches();
}

void RegexTesterWidget::runReplace() {
    updateReplacementPreview();
}

void RegexTesterWidget::clearResults() {
    m_matches.clear();
    m_matchTable->setRowCount(0);
    m_groupTree->clear();
    m_matchCountLabel->setText("0 matches");
    m_replacementPreview->clear();
    m_matchHighlighter->setMatches(m_matches);
}

void RegexTesterWidget::copyMatches() {
    QStringList matches;
    for (const RegexMatch& match : m_matches) {
        matches.append(match.text);
    }
    QApplication::clipboard()->setText(matches.join("\n"));
}

void RegexTesterWidget::exportMatches() {
    QString path = QFileDialog::getSaveFileName(this, "Export Matches",
        "matches.json", "JSON Files (*.json);;CSV Files (*.csv);;All Files (*)");
    
    if (path.isEmpty()) return;
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file.");
        return;
    }
    
    QTextStream out(&file);
    
    if (path.endsWith(".json")) {
        QJsonArray array;
        for (const RegexMatch& match : m_matches) {
            QJsonObject obj;
            obj["text"] = match.text;
            obj["start"] = match.start;
            obj["end"] = match.end;
            
            QJsonObject groups;
            for (const auto& group : match.groups) {
                groups[group.first] = group.second;
            }
            obj["groups"] = groups;
            
            array.append(obj);
        }
        
        QJsonDocument doc(array);
        out << doc.toJson(QJsonDocument::Indented);
    } else {
        // CSV
        out << "Index,Start,End,Match\n";
        for (int i = 0; i < m_matches.size(); ++i) {
            const RegexMatch& match = m_matches[i];
            QString escapedText = match.text;
            escapedText.replace("\"", "\"\"");
            out << i + 1 << "," << match.start << "," << match.end 
                << ",\"" << escapedText << "\"\n";
        }
    }
    
    file.close();
}

void RegexTesterWidget::navigateToMatch(int index) {
    if (index < 0 || index >= m_matches.size()) return;
    
    m_currentMatchIndex = index;
    m_matchHighlighter->setCurrentMatchIndex(index);
    
    const RegexMatch& match = m_matches[index];
    
    // Scroll to match in editor
    QTextCursor cursor = m_testInput->textCursor();
    cursor.setPosition(match.start);
    cursor.setPosition(match.end, QTextCursor::KeepAnchor);
    m_testInput->setTextCursor(cursor);
    m_testInput->centerCursor();
    
    // Select row in table
    m_matchTable->selectRow(index);
}

void RegexTesterWidget::navigateNext() {
    if (m_matches.isEmpty()) return;
    navigateToMatch((m_currentMatchIndex + 1) % m_matches.size());
}

void RegexTesterWidget::navigatePrevious() {
    if (m_matches.isEmpty()) return;
    navigateToMatch((m_currentMatchIndex - 1 + m_matches.size()) % m_matches.size());
}

void RegexTesterWidget::onPatternLibraryItemSelected(QTreeWidgetItem* item) {
    QString pattern = item->data(0, Qt::UserRole).toString();
    if (!pattern.isEmpty()) {
        setPattern(pattern);
    }
}

void RegexTesterWidget::onHistoryItemSelected(QListWidgetItem* item) {
    setPattern(item->text());
}

void RegexTesterWidget::addToHistory(const QString& pattern) {
    if (pattern.isEmpty() || m_history.contains(pattern)) return;
    
    m_history.prepend(pattern);
    if (m_history.size() > 50) {
        m_history = m_history.mid(0, 50);
    }
    
    m_historyList->clear();
    m_historyList->addItems(m_history);
}

void RegexTesterWidget::loadHistory() {
    m_history = m_settings->value("RegexTester/History").toStringList();
    m_historyList->addItems(m_history);
}

void RegexTesterWidget::saveHistory() {
    m_settings->setValue("RegexTester/History", m_history);
}

void RegexTesterWidget::addToFavorites(const QString& pattern, const QString& description) {
    PatternInfo info{pattern, description, getCurrentOptions(), "Favorites"};
    m_favorites.append(info);
    m_favoritesList->addItem(QString("%1 - %2").arg(pattern, description));
}
