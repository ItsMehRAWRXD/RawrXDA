#include "CommandPalette.h"

#include <QRegularExpression>
#include <QtMath>

CommandPalette::CommandPalette(QObject* parent) : QObject(parent) {}
CommandPalette::~CommandPalette() {}

bool CommandPalette::registerCommand(const Command& cmd) {
    QMutexLocker locker(&m_mutex);
    if (cmd.id.isEmpty() || m_commands.contains(cmd.id)) return false;
    m_commands.insert(cmd.id, cmd);
    emit commandRegistered(cmd.id);
    return true;
}

bool CommandPalette::unregisterCommand(const QString& id) {
    QMutexLocker locker(&m_mutex);
    if (!m_commands.contains(id)) return false;
    m_commands.remove(id);
    emit commandRemoved(id);
    return true;
}

QList<CommandPalette::Command> CommandPalette::listCommands(const QString& category) const {
    QMutexLocker locker(&m_mutex);
    QList<Command> result;
    for (const auto& cmd : m_commands) {
        if (category.isEmpty() || cmd.category == category) result.append(cmd);
    }
    return result;
}

double CommandPalette::score(const QString& query, const Command& cmd) const {
    if (query.isEmpty()) return 0.0;
    QString q = query.toLower();
    QString text = (cmd.title + " " + cmd.description + " " + cmd.id).toLower();
    int idx = text.indexOf(q);
    double freq = text.count(q);
    double recency = cmd.lastUsed.isValid() ? 1.0 / (1.0 + cmd.lastUsed.msecsTo(QDateTime::currentDateTimeUtc()) / 1000.0) : 0.0;
    return (idx >= 0 ? 2.0 : 0.0) + freq * 0.5 + cmd.usageCount * 0.2 + recency;
}

QList<CommandPalette::Command> CommandPalette::search(const QString& query, int maxResults) const {
    QMutexLocker locker(&m_mutex);
    QList<QPair<double, Command>> scored;
    for (const auto& cmd : m_commands) {
        double s = score(query, cmd);
        if (s > 0.0) scored.append(qMakePair(s, cmd));
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QList<Command> results;
    for (int i = 0; i < scored.size() && i < maxResults; ++i) results.append(scored[i].second);
    return results;
}

bool CommandPalette::execute(const QString& id) {
    QMutexLocker locker(&m_mutex);
    if (!m_commands.contains(id)) return false;
    Command& cmd = m_commands[id];
    cmd.usageCount += 1;
    cmd.lastUsed = QDateTime::currentDateTimeUtc();
    emit commandExecuted(id);
    return true;
}
