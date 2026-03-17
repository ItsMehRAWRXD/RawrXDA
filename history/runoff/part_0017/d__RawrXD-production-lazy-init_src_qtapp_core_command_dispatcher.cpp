#include "command_dispatcher.h"

#include <QDebug>
#include <QElapsedTimer>
#include <stdexcept>

/**
 * @file command_dispatcher.cpp
 * @brief Implementation of centralized command dispatching and undo/redo
 */

CommandDispatcher& CommandDispatcher::instance() {
    static CommandDispatcher inst;
    return inst;
}

CommandDispatcher::CommandDispatcher()
    : QObject(nullptr)
    , m_isRecordingMacro(false)
{
    qDebug() << "[CommandDispatcher] Initialized";
}

CommandDispatcher::~CommandDispatcher() {
    try {
        clearHistory();
        qDebug() << "[CommandDispatcher] Shutdown";
    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error during shutdown:" << e.what();
    }
}

void CommandDispatcher::registerCommand(const QString& id,
                                       const QString& name,
                                       CommandFunc execute,
                                       CommandUndoFunc undo) {
    try {
        if (id.isEmpty() || name.isEmpty() || !execute) {
            qWarning() << "[CommandDispatcher] Invalid command registration:" << id;
            return;
        }

        CommandEntry entry;
        entry.id = id;
        entry.name = name;
        entry.category = "General";
        entry.executeFunc = execute;
        entry.undoFunc = undo;
        entry.undoable = (undo != nullptr);

        m_registeredCommands[id] = entry;

        qDebug() << "[CommandDispatcher] Registered command:" << id << "-" << name;

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error registering command:" << e.what();
    }
}

bool CommandDispatcher::executeCommand(const QString& commandId) {
    try {
        if (!m_registeredCommands.contains(commandId)) {
            QString error = QString("Unknown command: %1").arg(commandId);
            qWarning() << "[CommandDispatcher]" << error;
            emit commandFailed(commandId, error);
            return false;
        }

        const CommandEntry& entry = m_registeredCommands[commandId];

        if (!entry.executeFunc) {
            QString error = QString("Command has no execute function: %1").arg(commandId);
            qWarning() << "[CommandDispatcher]" << error;
            emit commandFailed(commandId, error);
            return false;
        }

        QElapsedTimer timer;
        timer.start();

        try {
            bool result = entry.executeFunc();

            double executionTime = timer.elapsed();
            m_statistics.totalExecutionTimeMs += executionTime;
            m_statistics.totalCommandsExecuted++;

            if (!result) {
                QString error = QString("Command execution failed: %1").arg(commandId);
                qWarning() << "[CommandDispatcher]" << error;
                emit commandFailed(commandId, error);
                return false;
            }

            // Add to undo stack if undoable and have undo function
            if (entry.undoable && entry.undoFunc) {
                // Create a simple command wrapper
                class SimpleCommand : public Command {
                public:
                    SimpleCommand(const QString& name, CommandFunc exec, CommandUndoFunc undoFunc)
                        : m_name(name), m_exec(exec), m_undoFunc(undoFunc) {}

                    bool execute() override { return m_exec ? m_exec() : false; }
                    bool undo() override { return m_undoFunc ? m_undoFunc() : false; }
                    QString getName() const override { return m_name; }

                private:
                    QString m_name;
                    CommandFunc m_exec;
                    CommandUndoFunc m_undoFunc;
                };

                auto cmd = std::make_unique<SimpleCommand>(entry.name, entry.executeFunc, entry.undoFunc);
                
                if (m_isRecordingMacro && m_currentMacro) {
                    m_currentMacro->addCommand(std::move(cmd));
                } else {
                    m_undoStack.push(std::move(cmd));
                    m_redoStack.clear();  // Clear redo when new command executed

                    // Limit undo stack size
                    if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
                        m_undoStack.removeFirst();
                    }

                    if (m_undoStack.size() > m_statistics.peakUndoStackSize) {
                        m_statistics.peakUndoStackSize = m_undoStack.size();
                    }

                    emit undoRedoStateChanged();
                }
            }

            // Add to history
            m_commandHistory.push(entry.name);
            if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
                m_commandHistory.removeFirst();
            }

            qDebug() << "[CommandDispatcher] Executed command:" << commandId 
                     << "(" << executionTime << "ms)";
            emit commandExecuted(commandId);

            return true;

        } catch (const std::exception& e) {
            QString error = QString("Command execution exception: %1").arg(e.what());
            qWarning() << "[CommandDispatcher]" << error;
            emit commandFailed(commandId, error);
            return false;
        }

    } catch (const std::exception& e) {
        QString error = QString("Command dispatch error: %1").arg(e.what());
        qWarning() << "[CommandDispatcher]" << error;
        emit commandFailed(commandId, error);
        return false;
    }
}

bool CommandDispatcher::executeCommand(std::unique_ptr<Command> command) {
    try {
        if (!command) {
            qWarning() << "[CommandDispatcher] Null command";
            return false;
        }

        QString cmdName = command->getName();

        QElapsedTimer timer;
        timer.start();

        try {
            bool result = command->execute();

            double executionTime = timer.elapsed();
            m_statistics.totalExecutionTimeMs += executionTime;
            m_statistics.totalCommandsExecuted++;

            if (!result) {
                QString error = QString("Command execution failed: %1").arg(cmdName);
                qWarning() << "[CommandDispatcher]" << error;
                return false;
            }

            // Add to undo stack if undoable
            if (command->isUndoable()) {
                if (m_isRecordingMacro && m_currentMacro) {
                    m_currentMacro->addCommand(std::move(command));
                } else {
                    m_undoStack.push(std::move(command));
                    m_redoStack.clear();

                    if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
                        m_undoStack.removeFirst();
                    }

                    if (m_undoStack.size() > m_statistics.peakUndoStackSize) {
                        m_statistics.peakUndoStackSize = m_undoStack.size();
                    }

                    emit undoRedoStateChanged();
                }
            }

            m_commandHistory.push(cmdName);
            if (m_commandHistory.size() > MAX_HISTORY_SIZE) {
                m_commandHistory.removeFirst();
            }

            qDebug() << "[CommandDispatcher] Executed custom command:" << cmdName 
                     << "(" << executionTime << "ms)";

            return true;

        } catch (const std::exception& e) {
            QString error = QString("Command execution exception: %1").arg(e.what());
            qWarning() << "[CommandDispatcher]" << error;
            return false;
        }

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error executing custom command:" << e.what();
        return false;
    }
}

bool CommandDispatcher::undo() {
    try {
        if (m_undoStack.isEmpty()) {
            qDebug() << "[CommandDispatcher] Nothing to undo";
            return false;
        }

        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();

        QElapsedTimer timer;
        timer.start();

        try {
            bool result = cmd->undo();

            double executionTime = timer.elapsed();
            m_statistics.totalExecutionTimeMs += executionTime;

            if (!result) {
                // Re-push if undo failed
                m_undoStack.push(std::move(cmd));
                qWarning() << "[CommandDispatcher] Undo failed";
                return false;
            }

            // Move to redo stack
            m_redoStack.push(std::move(cmd));

            emit undoRedoStateChanged();
            emit undoPerformed(m_undoStack.isEmpty() ? "Unknown" : m_undoStack.top()->getName());

            qDebug() << "[CommandDispatcher] Undo performed (" << executionTime << "ms)";

            return true;

        } catch (const std::exception& e) {
            // Re-push if exception
            m_undoStack.push(std::move(cmd));
            qWarning() << "[CommandDispatcher] Undo exception:" << e.what();
            return false;
        }

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error in undo:" << e.what();
        return false;
    }
}

bool CommandDispatcher::redo() {
    try {
        if (m_redoStack.isEmpty()) {
            qDebug() << "[CommandDispatcher] Nothing to redo";
            return false;
        }

        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();

        QElapsedTimer timer;
        timer.start();

        try {
            bool result = cmd->execute();

            double executionTime = timer.elapsed();
            m_statistics.totalExecutionTimeMs += executionTime;

            if (!result) {
                // Re-push if redo failed
                m_redoStack.push(std::move(cmd));
                qWarning() << "[CommandDispatcher] Redo failed";
                return false;
            }

            // Move back to undo stack
            m_undoStack.push(std::move(cmd));

            emit undoRedoStateChanged();
            emit redoPerformed(m_undoStack.top()->getName());

            qDebug() << "[CommandDispatcher] Redo performed (" << executionTime << "ms)";

            return true;

        } catch (const std::exception& e) {
            // Re-push if exception
            m_redoStack.push(std::move(cmd));
            qWarning() << "[CommandDispatcher] Redo exception:" << e.what();
            return false;
        }

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error in redo:" << e.what();
        return false;
    }
}

bool CommandDispatcher::canUndo() const {
    return !m_undoStack.isEmpty();
}

bool CommandDispatcher::canRedo() const {
    return !m_redoStack.isEmpty();
}

QString CommandDispatcher::getUndoName() const {
    if (m_undoStack.isEmpty()) {
        return "Nothing to Undo";
    }
    return QString("Undo %1").arg(m_undoStack.top()->getName());
}

QString CommandDispatcher::getRedoName() const {
    if (m_redoStack.isEmpty()) {
        return "Nothing to Redo";
    }
    return QString("Redo %1").arg(m_redoStack.top()->getName());
}

void CommandDispatcher::clearHistory() {
    try {
        m_undoStack.clear();
        m_redoStack.clear();
        m_commandHistory.clear();
        emit undoRedoStateChanged();
        qDebug() << "[CommandDispatcher] History cleared";
    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error clearing history:" << e.what();
    }
}

void CommandDispatcher::beginMacro(const QString& name) {
    try {
        if (m_isRecordingMacro) {
            qWarning() << "[CommandDispatcher] Already recording macro";
            return;
        }

        m_currentMacro = std::make_unique<MacroCommand>(name);
        m_isRecordingMacro = true;

        qDebug() << "[CommandDispatcher] Macro started:" << name;
        emit macroStarted(name);

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error starting macro:" << e.what();
        m_isRecordingMacro = false;
        m_currentMacro.reset();
    }
}

void CommandDispatcher::endMacro() {
    try {
        if (!m_isRecordingMacro || !m_currentMacro) {
            qWarning() << "[CommandDispatcher] No macro recording";
            return;
        }

        QString macroName = m_currentMacro->getName();
        auto macro = std::move(m_currentMacro);
        m_isRecordingMacro = false;

        // Execute macro
        macro->execute();

        // Add to undo stack
        m_undoStack.push(std::move(macro));
        m_redoStack.clear();

        emit undoRedoStateChanged();
        qDebug() << "[CommandDispatcher] Macro ended:" << macroName;
        emit macroEnded(macroName);

    } catch (const std::exception& e) {
        qWarning() << "[CommandDispatcher] Error ending macro:" << e.what();
        m_isRecordingMacro = false;
        m_currentMacro.reset();
    }
}

bool CommandDispatcher::isRecordingMacro() const {
    return m_isRecordingMacro && m_currentMacro != nullptr;
}

QStringList CommandDispatcher::getCommandHistory(int count) const {
    QStringList result;
    int limit = qMin(count, m_commandHistory.size());

    for (int i = 0; i < limit; ++i) {
        result.append(m_commandHistory.at(m_commandHistory.size() - 1 - i));
    }

    return result;
}

CommandDispatcher::Statistics CommandDispatcher::getStatistics() const {
    return m_statistics;
}

QStringList CommandDispatcher::getRegisteredCommands() const {
    return m_registeredCommands.keys();
}

CommandDispatcher::CommandInfo CommandDispatcher::getCommandInfo(const QString& id) const {
    CommandInfo info;
    info.id = id;

    if (m_registeredCommands.contains(id)) {
        const CommandEntry& entry = m_registeredCommands[id];
        info.name = entry.name;
        info.category = entry.category;
        info.undoable = entry.undoable;
    }

    return info;
}

// MacroCommand implementation

bool CommandDispatcher::MacroCommand::execute() {
    try {
        while (!m_commands.isEmpty()) {
            auto cmd = std::move(m_commands.top());
            m_commands.pop();
            if (!cmd->execute()) {
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[MacroCommand] Execution error:" << e.what();
        return false;
    }
}

bool CommandDispatcher::MacroCommand::undo() {
    try {
        // Undo in reverse order
        QStack<std::unique_ptr<Command>> tempStack;
        while (!m_commands.isEmpty()) {
            auto cmd = std::move(m_commands.top());
            m_commands.pop();
            if (!cmd->undo()) {
                return false;
            }
            tempStack.push(std::move(cmd));
        }
        m_commands = std::move(tempStack);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[MacroCommand] Undo error:" << e.what();
        return false;
    }
}

void CommandDispatcher::MacroCommand::addCommand(std::unique_ptr<Command> cmd) {
    if (cmd) {
        m_commands.push(std::move(cmd));
    }
}
