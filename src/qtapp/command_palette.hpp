#pragma once


#include <functional>

/**
 * @brief VS Code/Cursor-style command palette (Ctrl+Shift+P)
 * 
 * Features:
 * - Fuzzy search for commands
 * - Recent commands tracking
 * - Category prefixes (>, @, #, :)
 * - Keyboard navigation
 * - Dark theme matching VS Code
 */
class CommandPalette : public void {

public:
    struct Command {
        std::string id;
        std::string label;
        std::string category;
        std::string description;
        QKeySequence shortcut;
        std::function<void()> action;
        bool enabled = true;
    };

    explicit CommandPalette(void* parent = nullptr);
    
    void registerCommand(const Command& cmd);
    void show();
    void hide();
    
    void commandExecuted(const std::string& commandId);
    
protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(void* obj, QEvent* event) override;
    
private:
    void onSearchTextChanged(const std::string& text);
    void onItemActivated(QListWidgetItem* item);
    void executeSelectedCommand();
    
private:
    void setupUI();
    void updateResults(const std::string& filter);
    void applyDarkTheme();
    int fuzzyMatch(const std::string& pattern, const std::string& text) const;
    
    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QLabel* m_hintLabel;
    
    std::map<std::string, Command> m_commands;
    std::vector<std::string> m_recentCommands;
    
    static constexpr int MAX_RESULTS = 10;
    static constexpr int MAX_RECENT = 5;
};

