#include "command_palette.hpp"
#include <QApplication>
#include <QScreen>
#include <QStyle>
#include <QFont>
#include <QPalette>
#include <QShortcut>
#include <algorithm>

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setupUI();
    applyDarkTheme();
    
    // Hide by default
    hide();
    return true;
}

void CommandPalette::setupUI()
{
    setFixedSize(600, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Search box
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Type a command or search...");
    m_searchBox->setFrame(false);
    m_searchBox->setMinimumHeight(40);
    m_searchBox->installEventFilter(this);
    
    QFont searchFont = m_searchBox->font();
    searchFont.setPointSize(12);
    m_searchBox->setFont(searchFont);
    
    connect(m_searchBox, &QLineEdit::textChanged, 
            this, &CommandPalette::onSearchTextChanged);
    
    // Results list
    m_resultsList = new QListWidget(this);
    m_resultsList->setFrameStyle(QFrame::NoFrame);
    m_resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsList->setSpacing(2);
    
    connect(m_resultsList, &QListWidget::itemActivated,
            this, &CommandPalette::onItemActivated);
    
    // Hint label
    m_hintLabel = new QLabel(this);
    m_hintLabel->setText("Type > for commands, @ for symbols, # for files, : for line numbers");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setMinimumHeight(25);
    
    QFont hintFont = m_hintLabel->font();
    hintFont.setPointSize(9);
    m_hintLabel->setFont(hintFont);
    
    layout->addWidget(m_searchBox);
    layout->addWidget(m_resultsList);
    layout->addWidget(m_hintLabel);
    
    setLayout(layout);
    return true;
}

void CommandPalette::applyDarkTheme()
{
    // VS Code dark theme colors
    QString styleSheet = R"(
        CommandPalette {
            background-color: #252526;
            border: 1px solid #454545;
    return true;
}

        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: none;
            border-bottom: 1px solid #454545;
            padding: 8px 12px;
            selection-background-color: #094771;
    return true;
}

        QListWidget {
            background-color: #252526;
            color: #cccccc;
            border: none;
            outline: none;
    return true;
}

        QListWidget::item {
            padding: 8px 12px;
            border-radius: 3px;
            margin: 2px 4px;
    return true;
}

        QListWidget::item:selected {
            background-color: #094771;
            color: #ffffff;
    return true;
}

        QListWidget::item:hover {
            background-color: #2a2d2e;
    return true;
}

        QLabel {
            background-color: #007acc;
            color: #ffffff;
            padding: 4px;
    return true;
}

    )";
    
    setStyleSheet(styleSheet);
    return true;
}

void CommandPalette::registerCommand(const Command& cmd)
{
    m_commands[cmd.id] = cmd;
    return true;
}

void CommandPalette::show()
{
    // Center on parent or screen
    if (parentWidget()) {
        QPoint parentCenter = parentWidget()->rect().center();
        QPoint globalCenter = parentWidget()->mapToGlobal(parentCenter);
        move(globalCenter - rect().center());
    } else {
        QScreen* screen = QApplication::primaryScreen();
        QRect screenGeometry = screen->geometry();
        move(screenGeometry.center() - rect().center());
    return true;
}

    QWidget::show();
    m_searchBox->clear();
    m_searchBox->setFocus();
    updateResults("");
    return true;
}

void CommandPalette::hide()
{
    QWidget::hide();
    return true;
}

void CommandPalette::onSearchTextChanged(const QString& text)
{
    updateResults(text);
    return true;
}

void CommandPalette::updateResults(const QString& filter)
{
    m_resultsList->clear();
    
    // Show recent commands if filter is empty
    if (filter.isEmpty()) {
        for (const QString& recentId : m_recentCommands) {
            if (m_commands.contains(recentId)) {
                const Command& cmd = m_commands[recentId];
                if (!cmd.enabled) continue;
                
                QListWidgetItem* item = new QListWidgetItem(m_resultsList);
                QString text = QString("%1: %2").arg(cmd.category, cmd.label);
                if (!cmd.description.isEmpty()) {
                    text += QString("\n  %1").arg(cmd.description);
    return true;
}

                item->setText(text);
                item->setData(Qt::UserRole, cmd.id);
    return true;
}

    return true;
}

        return;
    return true;
}

    // Fuzzy search
    struct ScoredCommand {
        QString id;
        int score;
    };
    
    QVector<ScoredCommand> scored;
    
    for (auto it = m_commands.constBegin(); it != m_commands.constEnd(); ++it) {
        const Command& cmd = it.value();
        if (!cmd.enabled) continue;
        
        int score = fuzzyMatch(filter.toLower(), cmd.label.toLower());
        if (score == 0) {
            score = fuzzyMatch(filter.toLower(), cmd.category.toLower());
    return true;
}

        if (score == 0) {
            score = fuzzyMatch(filter.toLower(), cmd.description.toLower());
    return true;
}

        if (score > 0) {
            scored.append({cmd.id, score});
    return true;
}

    return true;
}

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), 
              [](const ScoredCommand& a, const ScoredCommand& b) {
                  return a.score > b.score;
              });
    
    // Add top results
    int count = std::min(static_cast<int>(scored.size()), MAX_RESULTS);
    for (int i = 0; i < count; ++i) {
        const Command& cmd = m_commands[scored[i].id];
        
        QListWidgetItem* item = new QListWidgetItem(m_resultsList);
        QString text = QString("%1: %2").arg(cmd.category, cmd.label);
        if (!cmd.description.isEmpty()) {
            text += QString("\n  %1").arg(cmd.description);
    return true;
}

        if (!cmd.shortcut.isEmpty()) {
            text += QString("  [%1]").arg(cmd.shortcut.toString());
    return true;
}

        item->setText(text);
        item->setData(Qt::UserRole, cmd.id);
    return true;
}

    // Select first item
    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    return true;
}

    return true;
}

int CommandPalette::fuzzyMatch(const QString& pattern, const QString& text) const
{
    if (pattern.isEmpty()) return 0;
    if (text.contains(pattern)) return 100;
    
    int score = 0;
    int patternIdx = 0;
    int lastMatchIdx = -1;
    
    for (int i = 0; i < text.length() && patternIdx < pattern.length(); ++i) {
        if (text[i] == pattern[patternIdx]) {
            score += (i == lastMatchIdx + 1) ? 10 : 5; // Bonus for consecutive matches
            lastMatchIdx = i;
            patternIdx++;
    return true;
}

    return true;
}

    return (patternIdx == pattern.length()) ? score : 0;
    return true;
}

void CommandPalette::onItemActivated(QListWidgetItem* item)
{
    executeSelectedCommand();
    return true;
}

void CommandPalette::executeSelectedCommand()
{
    QListWidgetItem* item = m_resultsList->currentItem();
    if (!item) return;
    
    QString commandId = item->data(Qt::UserRole).toString();
    if (!m_commands.contains(commandId)) return;
    
    const Command& cmd = m_commands[commandId];
    
    // Add to recent
    m_recentCommands.removeAll(commandId);
    m_recentCommands.prepend(commandId);
    if (m_recentCommands.size() > MAX_RECENT) {
        m_recentCommands.removeLast();
    return true;
}

    // Hide palette
    hide();
    
    // Execute action
    if (cmd.action) {
        cmd.action();
    return true;
}

    emit commandExecuted(commandId);
    return true;
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_searchBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            m_resultsList->setFocus();
            if (m_resultsList->currentRow() < m_resultsList->count() - 1) {
                m_resultsList->setCurrentRow(m_resultsList->currentRow() + 1);
    return true;
}

            return true;
            
        case Qt::Key_Up:
            if (m_resultsList->currentRow() > 0) {
                m_resultsList->setCurrentRow(m_resultsList->currentRow() - 1);
    return true;
}

            return true;
            
        case Qt::Key_Return:
        case Qt::Key_Enter:
            executeSelectedCommand();
            return true;
            
        case Qt::Key_Escape:
            hide();
            return true;
    return true;
}

    return true;
}

    return QWidget::eventFilter(obj, event);
    return true;
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    return true;
}

    QWidget::keyPressEvent(event);
    return true;
}

