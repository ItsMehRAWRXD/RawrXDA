#pragma once

#include <QString>
#include <QObject>
#include <QMap>
#include <QStack>
#include <memory>
#include <functional>

/**
 * @file command_dispatcher.h
 * @brief Central command routing and undo/redo management
 * 
 * Provides:
 * - Command registration and execution
 * - Undo/redo stack management
 * - Menu/button command wiring
 * - Macro recording
 * - Command history and diagnostics
 */

class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    /**
     * Base command interface
     */
    class Command {
    public:
        virtual ~Command() = default;

        /**
         * Execute the command
         */
        virtual bool execute() = 0;

        /**
         * Undo the command
         */
        virtual bool undo() = 0;

        /**
         * Get human-readable command name
         */
        virtual QString getName() const = 0;

        /**
         * Whether this command can be undone
         */
        virtual bool isUndoable() const { return true; }

        /**
         * Get command category for organization
         */
        virtual QString getCategory() const { return "General"; }
    };

    using CommandFunc = std::function<bool()>;
    using CommandUndoFunc = std::function<bool()>;

    static CommandDispatcher& instance();

    /**
     * Register a simple command with lambda/function
     */
    void registerCommand(const QString& id,
                        const QString& name,
                        CommandFunc execute,
                        CommandUndoFunc undo = nullptr);

    /**
     * Execute command by ID
     */
    bool executeCommand(const QString& commandId);

    /**
     * Execute custom command object
     */
    bool executeCommand(std::unique_ptr<Command> command);

    /**
     * Undo last command
     */
    bool undo();

    /**
     * Redo last undone command
     */
    bool redo();

    /**
     * Check if undo is available
     */
    bool canUndo() const;

    /**
     * Check if redo is available
     */
    bool canRedo() const;

    /**
     * Get undo command name
     */
    QString getUndoName() const;

    /**
     * Get redo command name
     */
    QString getRedoName() const;

    /**
     * Clear undo/redo stack
     */
    void clearHistory();

    /**
     * Start macro (batch multiple commands)
     */
    void beginMacro(const QString& name);

    /**
     * End macro
     */
    void endMacro();

    /**
     * Check if recording macro
     */
    bool isRecordingMacro() const;

    /**
     * Get command history (last N commands)
     */
    QStringList getCommandHistory(int count = 20) const;

    /**
     * Get statistics
     */
    struct Statistics {
        int totalCommandsExecuted = 0;
        int currentUndoStackSize = 0;
        int currentRedoStackSize = 0;
        int peakUndoStackSize = 0;
        double totalExecutionTimeMs = 0.0;
    };

    Statistics getStatistics() const;

    /**
     * Get all registered command IDs
     */
    QStringList getRegisteredCommands() const;

    /**
     * Get command info
     */
    struct CommandInfo {
        QString id;
        QString name;
        QString category;
        bool undoable = true;
    };

    CommandInfo getCommandInfo(const QString& id) const;

signals:
    /**
     * Command executed
     */
    void commandExecuted(const QString& commandId);

    /**
     * Command execution failed
     */
    void commandFailed(const QString& commandId, const QString& error);

    /**
     * Undo performed
     */
    void undoPerformed(const QString& commandName);

    /**
     * Redo performed
     */
    void redoPerformed(const QString& commandName);

    /**
     * Undo/redo availability changed
     */
    void undoRedoStateChanged();

    /**
     * Macro started
     */
    void macroStarted(const QString& name);

    /**
     * Macro ended
     */
    void macroEnded(const QString& name);

private:
    struct CommandEntry {
        QString id;
        QString name;
        QString category;
        CommandFunc executeFunc;
        CommandUndoFunc undoFunc;
        bool undoable = true;
    };

    class MacroCommand : public Command {
    public:
        MacroCommand(const QString& name) : m_name(name) {}

        bool execute() override;
        bool undo() override;
        QString getName() const override { return m_name; }

        void addCommand(std::unique_ptr<Command> cmd);

    private:
        QString m_name;
        QStack<std::unique_ptr<Command>> m_commands;
    };

    CommandDispatcher();
    ~CommandDispatcher() override;

    QMap<QString, CommandEntry> m_registeredCommands;
    QStack<std::unique_ptr<Command>> m_undoStack;
    QStack<std::unique_ptr<Command>> m_redoStack;
    QStack<QString> m_commandHistory;

    std::unique_ptr<MacroCommand> m_currentMacro;
    bool m_isRecordingMacro = false;

    Statistics m_statistics;

    static constexpr int MAX_HISTORY_SIZE = 100;
    static constexpr int MAX_UNDO_STACK_SIZE = 100;
};
