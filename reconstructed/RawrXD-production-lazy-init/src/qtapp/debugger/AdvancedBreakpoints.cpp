#include "AdvancedBreakpoints.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

// ============================================================================
// Breakpoint Implementation
// ============================================================================

QJsonObject Breakpoint::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["file"] = file;
    obj["line"] = line;
    obj["function"] = function;
    obj["type"] = static_cast<int>(type);
    obj["enabled"] = enabled;
    obj["condition"] = condition;
    obj["conditionType"] = static_cast<int>(conditionType);
    obj["hitCount"] = hitCount;
    obj["hitCountTarget"] = hitCountTarget;
    obj["logMessage"] = logMessage;
    obj["action"] = static_cast<int>(action);
    obj["groupName"] = groupName;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["addressFilter"] = static_cast<qint64>(addressFilter);
    
    QJsonArray commandsArray;
    for (const auto& cmd : commandsToExecute) {
        commandsArray.append(cmd);
    }
    obj["commands"] = commandsArray;
    
    QJsonArray varsArray;
    for (const auto& var : modifyVariablesList) {
        varsArray.append(var);
    }
    obj["variables"] = varsArray;
    
    return obj;
}

Breakpoint Breakpoint::fromJson(const QJsonObject& obj) {
    Breakpoint bp;
    bp.id = obj["id"].toInt();
    bp.file = obj["file"].toString();
    bp.line = obj["line"].toInt();
    bp.function = obj["function"].toString();
    bp.type = static_cast<BreakpointType>(obj["type"].toInt());
    bp.enabled = obj["enabled"].toBool();
    bp.condition = obj["condition"].toString();
    bp.conditionType = static_cast<BreakpointCondition>(obj["conditionType"].toInt());
    bp.hitCount = obj["hitCount"].toInt();
    bp.hitCountTarget = obj["hitCountTarget"].toInt();
    bp.logMessage = obj["logMessage"].toString();
    bp.action = static_cast<BreakpointAction>(obj["action"].toInt());
    bp.groupName = obj["groupName"].toString();
    bp.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    bp.addressFilter = obj["addressFilter"].toVariant().toLongLong();
    
    QJsonArray commandsArray = obj["commands"].toArray();
    for (const auto& cmd : commandsArray) {
        bp.commandsToExecute.append(cmd.toString());
    }
    
    QJsonArray varsArray = obj["variables"].toArray();
    for (const auto& var : varsArray) {
        bp.modifyVariablesList.append(var.toString());
    }
    
    return bp;
}

// ============================================================================
// Watchpoint Implementation
// ============================================================================

QJsonObject Watchpoint::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["variableName"] = variableName;
    obj["expression"] = expression;
    obj["previousValue"] = previousValue;
    obj["currentValue"] = currentValue;
    obj["enabled"] = enabled;
    obj["triggerOnRead"] = triggerOnRead;
    obj["triggerOnWrite"] = triggerOnWrite;
    obj["hitCount"] = hitCount;
    obj["logMessage"] = logMessage;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    return obj;
}

Watchpoint Watchpoint::fromJson(const QJsonObject& obj) {
    Watchpoint wp;
    wp.id = obj["id"].toInt();
    wp.variableName = obj["variableName"].toString();
    wp.expression = obj["expression"].toString();
    wp.previousValue = obj["previousValue"].toString();
    wp.currentValue = obj["currentValue"].toString();
    wp.enabled = obj["enabled"].toBool();
    wp.triggerOnRead = obj["triggerOnRead"].toBool();
    wp.triggerOnWrite = obj["triggerOnWrite"].toBool();
    wp.hitCount = obj["hitCount"].toInt();
    wp.logMessage = obj["logMessage"].toString();
    wp.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    return wp;
}

// ============================================================================
// AdvancedBreakpointManager Implementation
// ============================================================================

AdvancedBreakpointManager::AdvancedBreakpointManager(QObject* parent)
    : QObject(parent), m_nextBreakpointId(1), m_nextWatchpointId(1),
      m_logFormat("[%file%:%line%] Breakpoint %id%: %message%") {
}

int AdvancedBreakpointManager::addBreakpoint(const Breakpoint& bp) {
    Breakpoint newBp = bp;
    newBp.id = m_nextBreakpointId++;
    newBp.createdAt = QDateTime::currentDateTime();
    newBp.hitCount = 0;
    
    m_breakpoints[newBp.id] = newBp;
    
    if (!newBp.groupName.isEmpty()) {
        if (!m_breakpointGroups.contains(newBp.groupName)) {
            m_breakpointGroups[newBp.groupName] = QList<int>();
        }
        m_breakpointGroups[newBp.groupName].append(newBp.id);
    }
    
    emit breakpointAdded(newBp.id);
    return newBp.id;
}

bool AdvancedBreakpointManager::removeBreakpoint(int id) {
    if (!m_breakpoints.contains(id)) {
        return false;
    }
    
    const auto& bp = m_breakpoints[id];
    if (!bp.groupName.isEmpty() && m_breakpointGroups.contains(bp.groupName)) {
        m_breakpointGroups[bp.groupName].removeAll(id);
        if (m_breakpointGroups[bp.groupName].isEmpty()) {
            m_breakpointGroups.remove(bp.groupName);
        }
    }
    
    m_breakpoints.remove(id);
    emit breakpointRemoved(id);
    return true;
}

bool AdvancedBreakpointManager::updateBreakpoint(int id, const Breakpoint& bp) {
    if (!m_breakpoints.contains(id)) {
        return false;
    }
    
    Breakpoint updated = bp;
    updated.id = id;
    updated.createdAt = m_breakpoints[id].createdAt;
    updated.hitCount = m_breakpoints[id].hitCount;
    
    const auto& oldBp = m_breakpoints[id];
    if (!oldBp.groupName.isEmpty() && oldBp.groupName != updated.groupName) {
        m_breakpointGroups[oldBp.groupName].removeAll(id);
    }
    
    if (!updated.groupName.isEmpty()) {
        if (!m_breakpointGroups.contains(updated.groupName)) {
            m_breakpointGroups[updated.groupName] = QList<int>();
        }
        if (!m_breakpointGroups[updated.groupName].contains(id)) {
            m_breakpointGroups[updated.groupName].append(id);
        }
    }
    
    m_breakpoints[id] = updated;
    emit breakpointModified(id);
    return true;
}

Breakpoint AdvancedBreakpointManager::getBreakpoint(int id) const {
    return m_breakpoints.value(id, Breakpoint());
}

QList<Breakpoint> AdvancedBreakpointManager::getAllBreakpoints() const {
    return m_breakpoints.values();
}

QList<Breakpoint> AdvancedBreakpointManager::getBreakpointsInFile(const QString& file) const {
    QList<Breakpoint> result;
    for (const auto& bp : m_breakpoints.values()) {
        if (bp.file == file && bp.enabled) {
            result.append(bp);
        }
    }
    return result;
}

QList<Breakpoint> AdvancedBreakpointManager::getBreakpointsByGroup(const QString& groupName) const {
    QList<Breakpoint> result;
    if (!m_breakpointGroups.contains(groupName)) {
        return result;
    }
    
    for (int id : m_breakpointGroups.value(groupName)) {
        if (m_breakpoints.contains(id)) {
            result.append(m_breakpoints[id]);
        }
    }
    return result;
}

bool AdvancedBreakpointManager::setConditionalBreakpoint(int id, const QString& condition, BreakpointCondition type) {
    if (!m_breakpoints.contains(id)) {
        return false;
    }
    
    m_breakpoints[id].condition = condition;
    m_breakpoints[id].conditionType = type;
    m_breakpoints[id].type = BreakpointType::ConditionalBreakpoint;
    emit breakpointModified(id);
    return true;
}

bool AdvancedBreakpointManager::evaluateCondition(int id, const QMap<QString, QString>& variables) const {
    if (!m_breakpoints.contains(id)) {
        return false;
    }
    
    const auto& bp = m_breakpoints[id];
    return matchesCondition(variables.value(bp.condition, ""), bp.condition, bp.conditionType);
}

QString AdvancedBreakpointManager::getConditionString(int id) const {
    if (!m_breakpoints.contains(id)) {
        return "";
    }
    return m_breakpoints[id].condition;
}

int AdvancedBreakpointManager::getHitCount(int id) const {
    return m_breakpoints.value(id, Breakpoint()).hitCount;
}

void AdvancedBreakpointManager::incrementHitCount(int id) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].hitCount++;
    }
}

void AdvancedBreakpointManager::resetHitCount(int id) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].hitCount = 0;
    }
}

void AdvancedBreakpointManager::setHitCountTarget(int id, int target) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].hitCountTarget = target;
    }
}

bool AdvancedBreakpointManager::shouldPauseOnHit(int id) const {
    if (!m_breakpoints.contains(id)) {
        return false;
    }
    
    const auto& bp = m_breakpoints[id];
    if (!bp.enabled) {
        return false;
    }
    
    if (bp.hitCountTarget <= 0) {
        return true;
    }
    
    return bp.hitCount >= bp.hitCountTarget;
}

void AdvancedBreakpointManager::createGroup(const QString& groupName) {
    if (!m_breakpointGroups.contains(groupName)) {
        m_breakpointGroups[groupName] = QList<int>();
        emit groupCreated(groupName);
    }
}

void AdvancedBreakpointManager::deleteGroup(const QString& groupName) {
    if (m_breakpointGroups.contains(groupName)) {
        m_breakpointGroups.remove(groupName);
        emit groupDeleted(groupName);
    }
}

void AdvancedBreakpointManager::addBreakpointToGroup(int breakpointId, const QString& groupName) {
    if (!m_breakpointGroups.contains(groupName)) {
        createGroup(groupName);
    }
    if (!m_breakpointGroups[groupName].contains(breakpointId)) {
        m_breakpointGroups[groupName].append(breakpointId);
    }
}

void AdvancedBreakpointManager::removeBreakpointFromGroup(int breakpointId, const QString& groupName) {
    if (m_breakpointGroups.contains(groupName)) {
        m_breakpointGroups[groupName].removeAll(breakpointId);
    }
}

void AdvancedBreakpointManager::enableGroup(const QString& groupName) {
    if (m_breakpointGroups.contains(groupName)) {
        for (int id : m_breakpointGroups[groupName]) {
            if (m_breakpoints.contains(id)) {
                m_breakpoints[id].enabled = true;
            }
        }
    }
}

void AdvancedBreakpointManager::disableGroup(const QString& groupName) {
    if (m_breakpointGroups.contains(groupName)) {
        for (int id : m_breakpointGroups[groupName]) {
            if (m_breakpoints.contains(id)) {
                m_breakpoints[id].enabled = false;
            }
        }
    }
}

QStringList AdvancedBreakpointManager::getAllGroups() const {
    return m_breakpointGroups.keys();
}

int AdvancedBreakpointManager::addWatchpoint(const Watchpoint& wp) {
    Watchpoint newWp = wp;
    newWp.id = m_nextWatchpointId++;
    newWp.createdAt = QDateTime::currentDateTime();
    newWp.hitCount = 0;
    
    m_watchpoints[newWp.id] = newWp;
    return newWp.id;
}

bool AdvancedBreakpointManager::removeWatchpoint(int id) {
    if (!m_watchpoints.contains(id)) {
        return false;
    }
    m_watchpoints.remove(id);
    return true;
}

bool AdvancedBreakpointManager::updateWatchpoint(int id, const Watchpoint& wp) {
    if (!m_watchpoints.contains(id)) {
        return false;
    }
    
    Watchpoint updated = wp;
    updated.id = id;
    updated.createdAt = m_watchpoints[id].createdAt;
    updated.hitCount = m_watchpoints[id].hitCount;
    
    m_watchpoints[id] = updated;
    return true;
}

Watchpoint AdvancedBreakpointManager::getWatchpoint(int id) const {
    return m_watchpoints.value(id, Watchpoint());
}

QList<Watchpoint> AdvancedBreakpointManager::getAllWatchpoints() const {
    return m_watchpoints.values();
}

bool AdvancedBreakpointManager::updateWatchpointValue(int id, const QString& newValue) {
    if (!m_watchpoints.contains(id)) {
        return false;
    }
    
    auto& wp = m_watchpoints[id];
    wp.previousValue = wp.currentValue;
    wp.currentValue = newValue;
    wp.hitCount++;
    
    if (wp.triggerOnWrite || (wp.triggerOnRead && !wp.previousValue.isEmpty())) {
        emit watchpointTriggered(id, wp.previousValue, newValue);
    }
    
    return true;
}

void AdvancedBreakpointManager::setLogMessage(int id, const QString& message) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].logMessage = message;
    }
}

QString AdvancedBreakpointManager::getLogMessage(int id) const {
    return m_breakpoints.value(id, Breakpoint()).logMessage;
}

void AdvancedBreakpointManager::setLogFormat(const QString& format) {
    m_logFormat = format;
}

QString AdvancedBreakpointManager::formatLogMessage(int id, const QMap<QString, QString>& context) const {
    if (!m_breakpoints.contains(id)) {
        return "";
    }
    return formatLogMessageInternal(m_breakpoints[id], context);
}

void AdvancedBreakpointManager::addCommandToExecute(int id, const QString& command) {
    if (m_breakpoints.contains(id)) {
        if (!m_breakpoints[id].commandsToExecute.contains(command)) {
            m_breakpoints[id].commandsToExecute.append(command);
        }
    }
}

void AdvancedBreakpointManager::removeCommandFromBreakpoint(int id, const QString& command) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].commandsToExecute.removeAll(command);
    }
}

QStringList AdvancedBreakpointManager::getCommandsForBreakpoint(int id) const {
    return m_breakpoints.value(id, Breakpoint()).commandsToExecute;
}

void AdvancedBreakpointManager::addVariableModification(int id, const QString& varName, const QString& newValue) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].modifyVariablesList.append(varName + "=" + newValue);
    }
}

QMap<QString, QString> AdvancedBreakpointManager::getVariableModifications(int id) const {
    QMap<QString, QString> result;
    const auto& mods = m_breakpoints.value(id, Breakpoint()).modifyVariablesList;
    for (const auto& mod : mods) {
        QStringList parts = mod.split('=');
        if (parts.size() == 2) {
            result[parts[0]] = parts[1];
        }
    }
    return result;
}

void AdvancedBreakpointManager::enableBreakpoint(int id) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].enabled = true;
        emit breakpointModified(id);
    }
}

void AdvancedBreakpointManager::disableBreakpoint(int id) {
    if (m_breakpoints.contains(id)) {
        m_breakpoints[id].enabled = false;
        emit breakpointModified(id);
    }
}

void AdvancedBreakpointManager::enableAllBreakpoints() {
    for (auto& bp : m_breakpoints) {
        bp.enabled = true;
    }
}

void AdvancedBreakpointManager::disableAllBreakpoints() {
    for (auto& bp : m_breakpoints) {
        bp.enabled = false;
    }
}

bool AdvancedBreakpointManager::isBreakpointEnabled(int id) const {
    return m_breakpoints.value(id, Breakpoint()).enabled;
}

QList<Breakpoint> AdvancedBreakpointManager::findBreakpoints(const QString& searchPattern) const {
    QList<Breakpoint> result;
    QRegularExpression regex(searchPattern);
    
    for (const auto& bp : m_breakpoints.values()) {
        if (regex.match(bp.file).hasMatch() || 
            regex.match(bp.function).hasMatch() || 
            regex.match(bp.condition).hasMatch()) {
            result.append(bp);
        }
    }
    return result;
}

QList<Breakpoint> AdvancedBreakpointManager::getBreakpointsByType(BreakpointType type) const {
    QList<Breakpoint> result;
    for (const auto& bp : m_breakpoints.values()) {
        if (bp.type == type) {
            result.append(bp);
        }
    }
    return result;
}

QList<Breakpoint> AdvancedBreakpointManager::getActiveBreakpoints() const {
    QList<Breakpoint> result;
    for (const auto& bp : m_breakpoints.values()) {
        if (bp.enabled) {
            result.append(bp);
        }
    }
    return result;
}

void AdvancedBreakpointManager::saveToJson(const QString& filePath) const {
    QJsonObject root = toJson();
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void AdvancedBreakpointManager::loadFromJson(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isObject()) {
        fromJson(doc.object());
    }
}

QJsonObject AdvancedBreakpointManager::toJson() const {
    QJsonObject root;
    
    QJsonArray bpsArray;
    for (const auto& bp : m_breakpoints.values()) {
        bpsArray.append(bp.toJson());
    }
    root["breakpoints"] = bpsArray;
    
    QJsonArray wpsArray;
    for (const auto& wp : m_watchpoints.values()) {
        wpsArray.append(wp.toJson());
    }
    root["watchpoints"] = wpsArray;
    
    return root;
}

void AdvancedBreakpointManager::fromJson(const QJsonObject& obj) {
    m_breakpoints.clear();
    m_watchpoints.clear();
    m_breakpointGroups.clear();
    m_nextBreakpointId = 1;
    m_nextWatchpointId = 1;
    
    QJsonArray bpsArray = obj["breakpoints"].toArray();
    for (const auto& bpJson : bpsArray) {
        Breakpoint bp = Breakpoint::fromJson(bpJson.toObject());
        m_breakpoints[bp.id] = bp;
        m_nextBreakpointId = std::max(m_nextBreakpointId, bp.id + 1);
        
        if (!bp.groupName.isEmpty()) {
            if (!m_breakpointGroups.contains(bp.groupName)) {
                m_breakpointGroups[bp.groupName] = QList<int>();
            }
            m_breakpointGroups[bp.groupName].append(bp.id);
        }
    }
    
    QJsonArray wpsArray = obj["watchpoints"].toArray();
    for (const auto& wpJson : wpsArray) {
        Watchpoint wp = Watchpoint::fromJson(wpJson.toObject());
        m_watchpoints[wp.id] = wp;
        m_nextWatchpointId = std::max(m_nextWatchpointId, wp.id + 1);
    }
    
    emit breakpointsLoaded(m_breakpoints.size());
}

int AdvancedBreakpointManager::getTotalBreakpointCount() const {
    return m_breakpoints.size();
}

int AdvancedBreakpointManager::getEnabledBreakpointCount() const {
    int count = 0;
    for (const auto& bp : m_breakpoints.values()) {
        if (bp.enabled) {
            count++;
        }
    }
    return count;
}

int AdvancedBreakpointManager::getTotalHitCount() const {
    int total = 0;
    for (const auto& bp : m_breakpoints.values()) {
        total += bp.hitCount;
    }
    return total;
}

QMap<QString, int> AdvancedBreakpointManager::getHitCountPerBreakpoint() const {
    QMap<QString, int> result;
    for (const auto& bp : m_breakpoints.values()) {
        QString key = QString("%1:%2").arg(bp.file, QString::number(bp.line));
        result[key] = bp.hitCount;
    }
    return result;
}

bool AdvancedBreakpointManager::matchesCondition(const QString& value, const QString& condition, BreakpointCondition type) const {
    switch (type) {
        case BreakpointCondition::Equal:
            return value == condition;
        case BreakpointCondition::NotEqual:
            return value != condition;
        case BreakpointCondition::GreaterThan:
            return value.toDouble() > condition.toDouble();
        case BreakpointCondition::LessThan:
            return value.toDouble() < condition.toDouble();
        case BreakpointCondition::GreaterOrEqual:
            return value.toDouble() >= condition.toDouble();
        case BreakpointCondition::LessOrEqual:
            return value.toDouble() <= condition.toDouble();
        case BreakpointCondition::Contains:
            return value.contains(condition);
        case BreakpointCondition::Matches: {
            QRegularExpression regex(condition);
            return regex.match(value).hasMatch();
        }
        case BreakpointCondition::Custom:
            return true;
    }
    return false;
}

QString AdvancedBreakpointManager::formatLogMessageInternal(const Breakpoint& bp, const QMap<QString, QString>& context) const {
    QString formatted = m_logFormat;
    formatted.replace("%file%", bp.file);
    formatted.replace("%line%", QString::number(bp.line));
    formatted.replace("%id%", QString::number(bp.id));
    formatted.replace("%function%", bp.function);
    formatted.replace("%message%", bp.logMessage);
    
    for (auto it = context.begin(); it != context.end(); ++it) {
        formatted.replace(QString("%%%1%").arg(it.key()), it.value());
    }
    
    return formatted;
}

void AdvancedBreakpointManager::clearInvalidGroupReferences() {
    QList<QString> emptyGroups;
    for (auto it = m_breakpointGroups.begin(); it != m_breakpointGroups.end(); ++it) {
        if (it.value().isEmpty()) {
            emptyGroups.append(it.key());
        }
    }
    
    for (const auto& groupName : emptyGroups) {
        m_breakpointGroups.remove(groupName);
    }
}

// ============================================================================
// BreakpointTemplate Implementation
// ============================================================================

BreakpointTemplate::BreakpointTemplate(const QString& name)
    : m_name(name), m_conditionType(BreakpointCondition::Custom),
      m_hitCountTarget(-1), m_action(BreakpointAction::Pause) {
}

void BreakpointTemplate::setFile(const QString& file) {
    m_file = file;
}

void BreakpointTemplate::setFunction(const QString& function) {
    m_function = function;
}

void BreakpointTemplate::setCondition(const QString& condition, BreakpointCondition type) {
    m_condition = condition;
    m_conditionType = type;
}

void BreakpointTemplate::setLogMessage(const QString& message) {
    m_logMessage = message;
}

void BreakpointTemplate::addCommand(const QString& command) {
    m_commands.append(command);
}

void BreakpointTemplate::setHitCountTarget(int target) {
    m_hitCountTarget = target;
}

void BreakpointTemplate::setAction(BreakpointAction action) {
    m_action = action;
}

Breakpoint BreakpointTemplate::createBreakpoint(const QString& file, int line) const {
    Breakpoint bp;
    bp.file = file;
    bp.line = line;
    bp.type = BreakpointType::LineBreakpoint;
    bp.enabled = true;
    bp.condition = m_condition;
    bp.conditionType = m_conditionType;
    bp.logMessage = m_logMessage;
    bp.commandsToExecute = m_commands;
    bp.hitCountTarget = m_hitCountTarget;
    bp.action = m_action;
    return bp;
}

Breakpoint BreakpointTemplate::createFunctionBreakpoint(const QString& function) const {
    Breakpoint bp;
    bp.function = function;
    bp.type = BreakpointType::FunctionBreakpoint;
    bp.enabled = true;
    bp.condition = m_condition;
    bp.conditionType = m_conditionType;
    bp.logMessage = m_logMessage;
    bp.commandsToExecute = m_commands;
    bp.hitCountTarget = m_hitCountTarget;
    bp.action = m_action;
    return bp;
}
