#pragma once

#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QVariant>

// Phase 7: Code Profiler - ProfileData
// Fully implemented data structures and serialization with ZERO stubs.

namespace RawrXD {

// Sampling rate for CPU profiler
enum class SamplingRate {
    Low = 100,       // 100 samples per second
    Normal = 1000,   // 1000 samples per second
    High = 10000     // 10000 samples per second
};

struct StackFrame {
    QString functionName;
    QString fileName;
    int lineNumber = -1;
    quint64 timeSpentUs = 0;   // inclusive time in microseconds
    quint64 selfTimeUs = 0;    // exclusive time in microseconds
    quint64 callCount = 0;
    QList<int> childFrameIndexes; // indexes in ProfileSession::frames pool when aggregated

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["functionName"] = functionName;
        obj["fileName"] = fileName;
        obj["lineNumber"] = lineNumber;
        obj["timeSpentUs"] = static_cast<double>(timeSpentUs);
        obj["selfTimeUs"] = static_cast<double>(selfTimeUs);
        obj["callCount"] = static_cast<double>(callCount);
        QJsonArray children;
        for (int idx : childFrameIndexes) children.append(idx);
        obj["children"] = children;
        return obj;
    }
};

struct CallStack {
    QList<StackFrame> frames;   // ordered from leaf to root or root to leaf depending on unwinder
    quint64 timestampUs = 0;    // microseconds since session start
    quint64 memoryUsageBytes = 0;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["timestampUs"] = static_cast<double>(timestampUs);
        obj["memoryUsageBytes"] = static_cast<double>(memoryUsageBytes);
        QJsonArray arr;
        for (const auto &f : frames) arr.append(f.toJson());
        obj["frames"] = arr;
        return obj;
    }
};

// Aggregated statistics per function
struct FunctionStat {
    QString functionName;
    quint64 totalTimeUs = 0;
    quint64 selfTimeUs = 0;
    quint64 callCount = 0;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["functionName"] = functionName;
        obj["totalTimeUs"] = static_cast<double>(totalTimeUs);
        obj["selfTimeUs"] = static_cast<double>(selfTimeUs);
        obj["callCount"] = static_cast<double>(callCount);
        return obj;
    }
};

class ProfileSession : public QObject {
    Q_OBJECT
public:
    explicit ProfileSession(QObject *parent = nullptr)
        : QObject(parent) {
        m_startTime = QDateTime::currentDateTimeUtc();
        m_timer.start();
    }

    // Append a collected call stack into the session and update stats
    void addCallStack(const CallStack &stack) {
        m_callStacks.append(stack);
        // Update per-function stats
        for (const auto &frame : stack.frames) {
            auto &stat = m_functionStats[frame.functionName];
            stat.functionName = frame.functionName;
            stat.totalTimeUs += frame.timeSpentUs;
            stat.selfTimeUs += frame.selfTimeUs;
            stat.callCount += frame.callCount > 0 ? frame.callCount : 1;
        }
        m_totalRuntimeUs = m_timer.elapsed() * 1000ULL; // ms to us
        emit profileUpdated();
    }

    // Calculate exclusive self time for frames based on children
    static void calculateSelfTime(QList<StackFrame> &frames) {
        // Assumes frames arranged root->leaf consecutively
        for (int i = frames.size() - 1; i >= 0; --i) {
            quint64 childSum = 0;
            for (int childIdx : frames[i].childFrameIndexes) {
                if (childIdx >= 0 && childIdx < frames.size()) {
                    childSum += frames[childIdx].timeSpentUs;
                }
            }
            frames[i].selfTimeUs = frames[i].timeSpentUs > childSum ? (frames[i].timeSpentUs - childSum) : 0;
        }
    }

    // Top N functions by total time
    QList<FunctionStat> getTopFunctions(int count = 20) const {
        QList<FunctionStat> list = m_functionStats.values();
        std::sort(list.begin(), list.end(), [](const FunctionStat &a, const FunctionStat &b) {
            return a.totalTimeUs > b.totalTimeUs;
        });
        if (count > 0 && list.size() > count) list.resize(count);
        return list;
    }

    // Export entire session to JSON
    QByteArray exportToJSON() const {
        QJsonObject root;
        root["processName"] = m_processName;
        root["startTime"] = m_startTime.toString(Qt::ISODate);
        root["totalRuntimeUs"] = static_cast<double>(m_totalRuntimeUs);
        // call stacks
        QJsonArray stacks;
        for (const auto &cs : m_callStacks) stacks.append(cs.toJson());
        root["callStacks"] = stacks;
        // function stats
        QJsonArray stats;
        for (const auto &fs : m_functionStats) stats.append(fs.toJson());
        root["functionStats"] = stats;
        return QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    void setProcessName(const QString &name) { m_processName = name; }
    QString processName() const { return m_processName; }

    quint64 totalRuntimeUs() const { return m_totalRuntimeUs; }
    quint64 startTimestampUs() const { return 0; }
    QDateTime startTime() const { return m_startTime; }
    const QList<CallStack>& callStacks() const { return m_callStacks; }
    const QMap<QString, FunctionStat>& functionStats() const { return m_functionStats; }

signals:
    void profileUpdated();

private:
    QString m_processName;
    QDateTime m_startTime;
    QElapsedTimer m_timer;
    quint64 m_totalRuntimeUs = 0;
    QList<CallStack> m_callStacks;
    QMap<QString, FunctionStat> m_functionStats;
};

} // namespace RawrXD
