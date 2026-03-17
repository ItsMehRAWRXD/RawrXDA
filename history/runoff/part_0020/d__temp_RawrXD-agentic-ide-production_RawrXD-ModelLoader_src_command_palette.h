#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <functional>

/**
 * @brief CommandPalette - Cmd-K style command palette with fuzzy search
 * 
 * Ported from cursor_cmdk.asm MASM implementation.
 * Implements a searchable list of 50+ commands for code operations and mode toggles.
 */
class CommandPalette : public QDialog {
    Q_OBJECT

public:
    // Command structure (from MASM COMMAND STRUCT)
    struct Command {
        QString name;
        QString description;
        std::function<void()> function;
    };

    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette();

    // Command management (from MASM CmdK_PopulateCommands)
    void registerCommand(const QString& name, const QString& description, std::function<void()> func);
    void registerBuiltInCommands();

    // UI control (from MASM CursorCmdK_Show)
    void showPalette();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    // Search and execution (from MASM CmdK_OnSearch / CmdK_ExecuteCommand)
    void onSearchTextChanged(const QString& text);
    void onCommandSelected();

private:
    QLineEdit* m_searchBox{nullptr};
    QListWidget* m_commandList{nullptr};
    QList<Command> m_allCommands;

    // Helper methods
    void filterCommands(const QString& searchText);
    int fuzzyMatch(const QString& text, const QString& pattern);
};
