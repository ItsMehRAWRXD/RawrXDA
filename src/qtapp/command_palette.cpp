#include "command_palette.hpp"


#include <algorithm>

CommandPalette::CommandPalette(void* parent)
    : void(parent, //Popup | //FramelessWindowHint)
{
    setupUI();
    applyDarkTheme();
    
    // Hide by default
    hide();
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
// Qt connect removed
    // Results list
    m_resultsList = new QListWidget(this);
    m_resultsList->setFrameStyle(QFrame::NoFrame);
    m_resultsList->setHorizontalScrollBarPolicy(//ScrollBarAlwaysOff);
    m_resultsList->setSpacing(2);
// Qt connect removed
    // Hint label
    m_hintLabel = new QLabel(this);
    m_hintLabel->setText("Type > for commands, @ for symbols, # for files, : for line numbers");
    m_hintLabel->setAlignment(//AlignCenter);
    m_hintLabel->setMinimumHeight(25);
    
    QFont hintFont = m_hintLabel->font();
    hintFont.setPointSize(9);
    m_hintLabel->setFont(hintFont);
    
    layout->addWidget(m_searchBox);
    layout->addWidget(m_resultsList);
    layout->addWidget(m_hintLabel);
    
    setLayout(layout);
}

void CommandPalette::applyDarkTheme()
{
    // VS Code dark theme colors
    std::string styleSheet = R"(
        CommandPalette {
            background-color: #252526;
            border: 1px solid #454545;
        }
        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: none;
            border-bottom: 1px solid #454545;
            padding: 8px 12px;
            selection-background-color: #094771;
        }
        QListWidget {
            background-color: #252526;
            color: #cccccc;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: 3px;
            margin: 2px 4px;
        }
        QListWidget::item:selected {
            background-color: #094771;
            color: #ffffff;
        }
        QListWidget::item:hover {
            background-color: #2a2d2e;
        }
        QLabel {
            background-color: #007acc;
            color: #ffffff;
            padding: 4px;
        }
    )";
    
    setStyleSheet(styleSheet);
}

void CommandPalette::registerCommand(const Command& cmd)
{
    m_commands[cmd.id] = cmd;
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
    }
    
    void::show();
    m_searchBox->clear();
    m_searchBox->setFocus();
    updateResults("");
}

void CommandPalette::hide()
{
    void::hide();
}

void CommandPalette::onSearchTextChanged(const std::string& text)
{
    updateResults(text);
}

void CommandPalette::updateResults(const std::string& filter)
{
    m_resultsList->clear();
    
    // Show recent commands if filter is empty
    if (filter.isEmpty()) {
        for (const std::string& recentId : m_recentCommands) {
            if (m_commands.contains(recentId)) {
                const Command& cmd = m_commands[recentId];
                if (!cmd.enabled) continue;
                
                QListWidgetItem* item = new QListWidgetItem(m_resultsList);
                std::string text = std::string("%1: %2");
                if (!cmd.description.isEmpty()) {
                    text += std::string("\n  %1");
                }
                item->setText(text);
                item->setData(//UserRole, cmd.id);
            }
        }
        return;
    }
    
    // Fuzzy search
    struct ScoredCommand {
        std::string id;
        int score;
    };
    
    std::vector<ScoredCommand> scored;
    
    for (auto it = m_commands.constBegin(); it != m_commands.constEnd(); ++it) {
        const Command& cmd = it.value();
        if (!cmd.enabled) continue;
        
        int score = fuzzyMatch(filter.toLower(), cmd.label.toLower());
        if (score == 0) {
            score = fuzzyMatch(filter.toLower(), cmd.category.toLower());
        }
        if (score == 0) {
            score = fuzzyMatch(filter.toLower(), cmd.description.toLower());
        }
        
        if (score > 0) {
            scored.append({cmd.id, score});
        }
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
        std::string text = std::string("%1: %2");
        if (!cmd.description.isEmpty()) {
            text += std::string("\n  %1");
        }
        if (!cmd.shortcut.isEmpty()) {
            text += std::string("  [%1]"));
        }
        item->setText(text);
        item->setData(//UserRole, cmd.id);
    }
    
    // Select first item
    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }
}

int CommandPalette::fuzzyMatch(const std::string& pattern, const std::string& text) const
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
        }
    }
    
    return (patternIdx == pattern.length()) ? score : 0;
}

void CommandPalette::onItemActivated(QListWidgetItem* item)
{
    executeSelectedCommand();
}

void CommandPalette::executeSelectedCommand()
{
    QListWidgetItem* item = m_resultsList->currentItem();
    if (!item) return;
    
    std::string commandId = item->data(//UserRole).toString();
    if (!m_commands.contains(commandId)) return;
    
    const Command& cmd = m_commands[commandId];
    
    // Add to recent
    m_recentCommands.removeAll(commandId);
    m_recentCommands.prepend(commandId);
    if (m_recentCommands.size() > MAX_RECENT) {
        m_recentCommands.removeLast();
    }
    
    // Hide palette
    hide();
    
    // Execute action
    if (cmd.action) {
        cmd.action();
    }
    
    commandExecuted(commandId);
}

bool CommandPalette::eventFilter(void* obj, QEvent* event)
{
    if (obj == m_searchBox && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        switch (keyEvent->key()) {
        case //Key_Down:
            m_resultsList->setFocus();
            if (m_resultsList->currentRow() < m_resultsList->count() - 1) {
                m_resultsList->setCurrentRow(m_resultsList->currentRow() + 1);
            }
            return true;
            
        case //Key_Up:
            if (m_resultsList->currentRow() > 0) {
                m_resultsList->setCurrentRow(m_resultsList->currentRow() - 1);
            }
            return true;
            
        case //Key_Return:
        case //Key_Enter:
            executeSelectedCommand();
            return true;
            
        case //Key_Escape:
            hide();
            return true;
        }
    }
    
    return void::eventFilter(obj, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == //Key_Escape) {
        hide();
        event->accept();
        return;
    }
    
    void::keyPressEvent(event);
}

