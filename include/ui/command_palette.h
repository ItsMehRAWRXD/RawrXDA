#pragma once
/**
 * @file command_palette.h
 * @brief Quick command access and fuzzy search
 * Batch 4 - Item 48: Command palette
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace RawrXD::UI {

struct Command {
    std::string id;
    std::string title;
    std::string description;
    std::string category;
    std::string icon;
    std::vector<std::string> keybindings;
    bool isEnabled;
    bool isVisible;
    int priority;
};

struct CommandPaletteItem {
    Command command;
    float score;
    std::vector<std::pair<int, int>> matches;
};

enum class CommandPaletteMode {
    Commands,
    Files,
    Symbols,
    Recent,
    Everything
};

class CommandPalette {
public:
    CommandPalette();
    ~CommandPalette();

    // Initialization
    void initialize();
    void shutdown();

    // Registration
    void registerCommand(const Command& command);
    void unregisterCommand(const std::string& commandId);
    void updateCommand(const std::string& commandId, std::function<void(Command&)> updater);

    // Default commands
    void registerDefaultCommands();
    void registerEditorCommands();
    void registerViewCommands();
    void registerHelpCommands();

    // Query
    std::vector<CommandPaletteItem> query(const std::string& input, size_t limit = 50);
    std::vector<CommandPaletteItem> queryCommands(const std::string& input);
    std::vector<CommandPaletteItem> queryFiles(const std::string& input);
    std::vector<CommandPaletteItem> querySymbols(const std::string& input);

    // Execution
    bool executeCommand(const std::string& commandId);
    bool executeSelected();

    // Selection
    void selectNext();
    void selectPrevious();
    void selectFirst();
    void selectLast();
    std::optional<CommandPaletteItem> getSelected() const;
    void setSelectedIndex(size_t index);

    // Mode
    void setMode(CommandPaletteMode mode);
    CommandPaletteMode getMode() const;
    void cycleMode();

    // Visibility
    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    // Input
    void setInput(const std::string& input);
    std::string getInput() const;
    void clearInput();
    void backspace();
    void appendChar(char c);

    // Recent commands
    void addToRecent(const std::string& commandId);
    std::vector<Command> getRecentCommands(size_t limit = 10);
    void clearRecent();

    // Events
    using VisibilityCallback = std::function<void(bool visible)>;
    using SelectionCallback = std::function<void(const Command&)>;
    void onVisibilityChanged(VisibilityCallback callback);
    void onSelectionChanged(SelectionCallback callback);
    void onCommandExecuted(std::function<void(const std::string&)> callback);

    // Configuration
    void setShowKeybindings(bool show);
    void setShowIcons(bool show);
    void setFuzzyMatching(bool enabled);
    void setPreserveInput(bool preserve);

private:
    std::map<std::string, Command> m_commands;
    std::vector<CommandPaletteItem> m_results;
    std::vector<std::string> m_recentCommands;
    std::string m_input;
    size_t m_selectedIndex{0};
    CommandPaletteMode m_mode{CommandPaletteMode::Commands};
    bool m_visible{false};
    bool m_showKeybindings{true};
    bool m_showIcons{true};
    bool m_fuzzyMatching{true};
    bool m_preserveInput{false};

    VisibilityCallback m_visibilityCallback;
    SelectionCallback m_selectionCallback;
    std::function<void(const std::string&)> m_executeCallback;

    float calculateScore(const std::string& query, const Command& command);
    float calculateFuzzyScore(const std::string& query, const std::string& target);
    std::vector<std::pair<int, int>> findMatches(const std::string& query, const std::string& target);
    void updateResults();
    void notifyVisibilityChanged();
    void notifySelectionChanged();
};

// Global instance
CommandPalette& getCommandPalette();

// Utility functions
std::string normalizeInput(const std::string& input);
bool fuzzyMatch(const std::string& pattern, const std::string& target);

} // namespace RawrXD::UI
