#ifndef ADVANCEDBREAKPOINTS_H
#define ADVANCEDBREAKPOINTS_H

#include <QString>
#include <QList>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <functional>
#include <memory>
#include "widgets/run_debug_widget.h"  // Use centralized Breakpoint definition

// Enums for breakpoint types and actions
enum class BreakpointType {
    LineBreakpoint,
    FunctionBreakpoint,
    ConditionalBreakpoint,
    WatchBreakpoint,
    ExceptionBreakpoint,
    MemoryBreakpoint,
    DataBreakpoint
};

enum class BreakpointCondition {
    Equal,
    NotEqual,
    GreaterThan,
    LessThan,
    GreaterOrEqual,
    LessOrEqual,
    Contains,
    Matches,
    Custom
};

enum class BreakpointAction {
    Pause,
    LogOnly,
    LogAndPause,
    IncrementCounter,
    ExecuteCommand,
    ModifyVariable
};

// Data structure for watchpoint
struct Watchpoint {
    int id;
    QString variableName;
    QString expression;
    QString previousValue;
    QString currentValue;
    bool enabled;
    bool triggerOnRead;
    bool triggerOnWrite;
    int hitCount;
    QString logMessage;
    QDateTime createdAt;
    
    QJsonObject toJson() const;
    static Watchpoint fromJson(const QJsonObject& obj);
};

// Main advanced breakpoints manager
class AdvancedBreakpointManager : public QObject {
    Q_OBJECT

public:
    explicit AdvancedBreakpointManager(QObject* parent = nullptr);
    ~AdvancedBreakpointManager() override = default;

    // Breakpoint management
    int addBreakpoint(const Breakpoint& bp);
    bool removeBreakpoint(int id);
    bool updateBreakpoint(int id, const Breakpoint& bp);
    Breakpoint getBreakpoint(int id) const;
    QList<Breakpoint> getAllBreakpoints() const;
    QList<Breakpoint> getBreakpointsInFile(const QString& file) const;
    QList<Breakpoint> getBreakpointsByGroup(const QString& groupName) const;
    
    // Conditional breakpoints
    bool setConditionalBreakpoint(int id, const QString& condition, BreakpointCondition type);
    bool evaluateCondition(int id, const QMap<QString, QString>& variables) const;
    QString getConditionString(int id) const;
    
    // Hit count management
    int getHitCount(int id) const;
    void incrementHitCount(int id);
    void resetHitCount(int id);
    void setHitCountTarget(int id, int target);
    bool shouldPauseOnHit(int id) const;
    
    // Breakpoint groups
    void createGroup(const QString& groupName);
    void deleteGroup(const QString& groupName);
    void addBreakpointToGroup(int breakpointId, const QString& groupName);
    void removeBreakpointFromGroup(int breakpointId, const QString& groupName);
    void enableGroup(const QString& groupName);
    void disableGroup(const QString& groupName);
    QStringList getAllGroups() const;
    
    // Watchpoint management
    int addWatchpoint(const Watchpoint& wp);
    bool removeWatchpoint(int id);
    bool updateWatchpoint(int id, const Watchpoint& wp);
    Watchpoint getWatchpoint(int id) const;
    QList<Watchpoint> getAllWatchpoints() const;
    bool updateWatchpointValue(int id, const QString& newValue);
    
    // Log actions
    void setLogMessage(int id, const QString& message);
    QString getLogMessage(int id) const;
    void setLogFormat(const QString& format);
    QString formatLogMessage(int id, const QMap<QString, QString>& context) const;
    
    // Command execution
    void addCommandToExecute(int id, const QString& command);
    void removeCommandFromBreakpoint(int id, const QString& command);
    QStringList getCommandsForBreakpoint(int id) const;
    
    // Variable modification
    void addVariableModification(int id, const QString& varName, const QString& newValue);
    QMap<QString, QString> getVariableModifications(int id) const;
    
    // Enable/Disable
    void enableBreakpoint(int id);
    void disableBreakpoint(int id);
    void enableAllBreakpoints();
    void disableAllBreakpoints();
    bool isBreakpointEnabled(int id) const;
    
    // Filtering and search
    QList<Breakpoint> findBreakpoints(const QString& searchPattern) const;
    QList<Breakpoint> getBreakpointsByType(BreakpointType type) const;
    QList<Breakpoint> getActiveBreakpoints() const;
    
    // Persistence
    void saveToJson(const QString& filePath) const;
    void loadFromJson(const QString& filePath);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
    
    // Statistics
    int getTotalBreakpointCount() const;
    int getEnabledBreakpointCount() const;
    int getTotalHitCount() const;
    QMap<QString, int> getHitCountPerBreakpoint() const;

signals:
    void breakpointAdded(int id);
    void breakpointRemoved(int id);
    void breakpointHit(int id, const QMap<QString, QString>& context);
    void breakpointModified(int id);
    void watchpointTriggered(int id, const QString& oldValue, const QString& newValue);
    void groupCreated(const QString& groupName);
    void groupDeleted(const QString& groupName);
    void breakpointsLoaded(int count);

private:
    int m_nextBreakpointId;
    int m_nextWatchpointId;
    QMap<int, Breakpoint> m_breakpoints;
    QMap<int, Watchpoint> m_watchpoints;
    QMap<QString, QList<int>> m_breakpointGroups;
    QString m_logFormat;
    
    bool matchesCondition(const QString& value, const QString& condition, BreakpointCondition type) const;
    QString formatLogMessageInternal(const Breakpoint& bp, const QMap<QString, QString>& context) const;
    void clearInvalidGroupReferences();
};

// Breakpoint template system for common patterns
class BreakpointTemplate {
public:
    explicit BreakpointTemplate(const QString& name);
    
    void setFile(const QString& file);
    void setFunction(const QString& function);
    void setCondition(const QString& condition, BreakpointCondition type);
    void setLogMessage(const QString& message);
    void addCommand(const QString& command);
    void setHitCountTarget(int target);
    void setAction(BreakpointAction action);
    
    Breakpoint createBreakpoint(const QString& file, int line) const;
    Breakpoint createFunctionBreakpoint(const QString& function) const;
    
private:
    QString m_name;
    QString m_file;
    QString m_function;
    QString m_condition;
    BreakpointCondition m_conditionType;
    QString m_logMessage;
    QStringList m_commands;
    int m_hitCountTarget;
    BreakpointAction m_action;
};

#endif // ADVANCEDBREAKPOINTS_H
