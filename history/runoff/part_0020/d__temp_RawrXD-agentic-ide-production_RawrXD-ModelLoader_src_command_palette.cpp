#include "command_palette.h"
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>

CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Popup)
{
    // From MASM CursorCmdK_Show - create command palette window
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(5);

    // Create search box (from MASM IDC_CMDK_SEARCH)
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Type a command or search...");
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "   font-size: 16px;"
        "   padding: 8px;"
        "   border: 1px solid #c0c0c0;"
        "   border-radius: 4px;"
        "   background: white;"
        "   color: black;"
        "}"
    );
    layout->addWidget(m_searchBox);

    // Create list view for commands (from MASM IDC_CMDK_LIST)
    m_commandList = new QListWidget(this);
    m_commandList->setStyleSheet(
        "QListWidget {"
        "   font-size: 14px;"
        "   border: 1px solid #d0d0d0;"
        "   background: white;"
        "   color: black;"
        "   outline: none;"
        "}"
        "QListWidget::item {"
        "   padding: 8px;"
        "   border-bottom: 1px solid #f0f0f0;"
        "}"
        "QListWidget::item:selected {"
        "   background: #0078d4;"
        "   color: white;"
        "}"
    );
    layout->addWidget(m_commandList);

    // Set window size (from MASM 600x400)
    setFixedSize(600, 400);

    // Connect signals
    connect(m_searchBox, &QLineEdit::textChanged, this, &CommandPalette::onSearchTextChanged);
    connect(m_commandList, &QListWidget::itemActivated, this, &CommandPalette::onCommandSelected);
    connect(m_searchBox, &QLineEdit::returnPressed, this, &CommandPalette::onCommandSelected);

    m_searchBox->installEventFilter(this);
    m_commandList->installEventFilter(this);

    registerBuiltInCommands();
}

CommandPalette::~CommandPalette() = default;

void CommandPalette::registerCommand(const QString& name, const QString& description, std::function<void()> func)
{
    m_allCommands.append({name, description, func});
}

void CommandPalette::registerBuiltInCommands()
{
    // From MASM cursor_cmdk.asm g_cmdkCommands
    registerCommand("Explain this code", "Explain the selected code", [](){ /* ExplainCode */ });
    registerCommand("Generate tests", "Create unit tests for selection", [](){ /* GenerateTests */ });
    registerCommand("Fix errors", "Fix compiler/linter errors", [](){ /* FixErrors */ });
    registerCommand("Refactor", "Refactor for clarity/performance", [](){ /* RefactorCode */ });
    registerCommand("Add docs", "Add JSDoc/docstrings", [](){ /* AddDocumentation */ });
    registerCommand("Optimize", "Optimize performance", [](){ /* OptimizeCode */ });
    registerCommand("Convert language", "Convert to another language", [](){ /* ConvertLanguage */ });
    registerCommand("Create function", "Generate function from comment", [](){ /* CreateFunction */ });
    
    // Mode toggles (from MASM)
    registerCommand("Toggle thinking UI", "Toggle the standardized thinking box", [](){ /* ToggleThinkingMode */ });
    registerCommand("Set Mode: Max", "Use Max mode (high-quality model)", [](){ /* SetModeMax */ });
    registerCommand("Set Mode: Search Web", "Enable web-search augmentation", [](){ /* SetModeSearch */ });
    registerCommand("Set Mode: Turbo", "Use turbo/deep-research mode", [](){ /* SetModeTurbo */ });
    registerCommand("Set Mode: Auto Instant", "Enable auto-instant thinking", [](){ /* SetModeAuto */ });
    registerCommand("Set Mode: Legacy", "Use legacy compatibility mode", [](){ /* SetModeLegacy */ });

    // Populate initial list
    onSearchTextChanged("");
}

void CommandPalette::showPalette()
{
    // Center on screen (from MASM DiffEngine_Show logic)
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 3; // Slightly higher than center
    move(x, y);

    m_searchBox->clear();
    m_searchBox->setFocus();
    show();
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
        
        if (obj == m_searchBox) {
            if (keyEvent->key() == Qt::Key_Down) {
                m_commandList->setFocus();
                if (m_commandList->count() > 0) {
                    m_commandList->setCurrentRow(0);
                }
                return true;
            }
        } else if (obj == m_commandList) {
            if (keyEvent->key() == Qt::Key_Up && m_commandList->currentRow() == 0) {
                m_searchBox->setFocus();
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void CommandPalette::onSearchTextChanged(const QString& text)
{
    // From MASM CmdK_OnSearch
    filterCommands(text);
}

void CommandPalette::onCommandSelected()
{
    // From MASM CmdK_ExecuteCommand
    QListWidgetItem* item = m_commandList->currentItem();
    if (!item) return;

    QString cmdName = item->text();
    for (const auto& cmd : m_allCommands) {
        if (cmd.name == cmdName) {
            hide();
            if (cmd.function) {
                cmd.function();
            }
            break;
        }
    }
}

void CommandPalette::filterCommands(const QString& searchText)
{
    m_commandList->clear();
    
    struct ScoredCommand {
        Command cmd;
        int score;
    };
    QList<ScoredCommand> scored;

    for (const auto& cmd : m_allCommands) {
        int score = fuzzyMatch(cmd.name, searchText);
        if (score > 0 || searchText.isEmpty()) {
            scored.append({cmd, score});
        }
    }

    // Sort by score (descending)
    std::sort(scored.begin(), scored.end(), [](const ScoredCommand& a, const ScoredCommand& b) {
        return a.score > b.score;
    });

    for (const auto& s : scored) {
        QListWidgetItem* item = new QListWidgetItem(s.cmd.name, m_commandList);
        item->setToolTip(s.cmd.description);
    }

    if (m_commandList->count() > 0) {
        m_commandList->setCurrentRow(0);
    }
}

int CommandPalette::fuzzyMatch(const QString& text, const QString& pattern)
{
    // Simple fuzzy match logic (from MASM String_FuzzyMatch)
    if (pattern.isEmpty()) return 100;
    
    QString t = text.toLower();
    QString p = pattern.toLower();
    
    if (t.contains(p)) return 100 - t.indexOf(p); // Exact substring match is high score
    
    int score = 0;
    int tIdx = 0;
    for (int pIdx = 0; pIdx < p.length(); ++pIdx) {
        int foundIdx = t.indexOf(p[pIdx], tIdx);
        if (foundIdx == -1) return 0; // Not a match
        
        // Bonus for consecutive characters
        if (foundIdx == tIdx) score += 10;
        
        score += 1;
        tIdx = foundIdx + 1;
    }
    
    return score;
}
