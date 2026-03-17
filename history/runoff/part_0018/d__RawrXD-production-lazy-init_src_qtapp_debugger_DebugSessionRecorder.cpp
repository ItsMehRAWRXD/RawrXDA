#include "DebugSessionRecorder.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cmath>

// ============================================================================
// DebugSnapshot Implementation
// ============================================================================

QJsonObject DebugSnapshot::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["timestamp"] = timestamp;
    obj["instructionPointer"] = QString::number(instructionPointer, 16);
    obj["threadId"] = threadId;
    obj["breakpointHit"] = breakpointHit;
    obj["breakpointId"] = breakpointId;
    obj["callStack"] = callStack;
    obj["threadState"] = threadState;
    
    QJsonObject regsObj;
    for (auto it = registers.begin(); it != registers.end(); ++it) {
        regsObj[it.key()] = QString::number(it.value(), 16);
    }
    obj["registers"] = regsObj;
    
    QJsonObject varsObj;
    for (auto it = localVariables.begin(); it != localVariables.end(); ++it) {
        varsObj[it.key()] = it.value();
    }
    obj["variables"] = varsObj;
    
    return obj;
}

DebugSnapshot DebugSnapshot::fromJson(const QJsonObject& obj) {
    DebugSnapshot snap;
    snap.id = obj["id"].toInt();
    snap.timestamp = obj["timestamp"].toVariant().toLongLong();
    snap.instructionPointer = obj["instructionPointer"].toString().toULongLong(nullptr, 16);
    snap.threadId = obj["threadId"].toInt();
    snap.breakpointHit = obj["breakpointHit"].toBool();
    snap.breakpointId = obj["breakpointId"].toInt();
    snap.callStack = obj["callStack"].toString();
    snap.threadState = obj["threadState"].toString();
    
    QJsonObject regsObj = obj["registers"].toObject();
    for (auto it = regsObj.begin(); it != regsObj.end(); ++it) {
        snap.registers[it.key()] = it.value().toString().toULongLong(nullptr, 16);
    }
    
    QJsonObject varsObj = obj["variables"].toObject();
    for (auto it = varsObj.begin(); it != varsObj.end(); ++it) {
        snap.localVariables[it.key()] = it.value().toString();
    }
    
    return snap;
}

size_t DebugSnapshot::getSizeBytes() const {
    size_t size = sizeof(DebugSnapshot);
    size += callStack.size() + threadState.size();
    for (const auto& var : localVariables) {
        size += var.size();
    }
    for (const auto& mem : memoryRegions) {
        size += mem.size();
    }
    return size;
}

// ============================================================================
// ExecutionStep Implementation
// ============================================================================

QJsonObject ExecutionStep::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["snapshotId"] = snapshotId;
    obj["eventType"] = eventType;
    obj["eventDescription"] = eventDescription;
    obj["timestamp"] = timestamp;
    obj["address"] = QString::number(address, 16);
    obj["mnemonic"] = mnemonic;
    
    QJsonArray operandsArray;
    for (const auto& op : operands) {
        operandsArray.append(op);
    }
    obj["operands"] = operandsArray;
    
    return obj;
}

ExecutionStep ExecutionStep::fromJson(const QJsonObject& obj) {
    ExecutionStep step;
    step.id = obj["id"].toInt();
    step.snapshotId = obj["snapshotId"].toInt();
    step.eventType = obj["eventType"].toString();
    step.eventDescription = obj["eventDescription"].toString();
    step.timestamp = obj["timestamp"].toVariant().toLongLong();
    step.address = obj["address"].toString().toULongLong(nullptr, 16);
    step.mnemonic = obj["mnemonic"].toString();
    
    QJsonArray operandsArray = obj["operands"].toArray();
    for (const auto& op : operandsArray) {
        step.operands.append(op.toString());
    }
    
    return step;
}

// ============================================================================
// DebugSessionRecorder Implementation
// ============================================================================

DebugSessionRecorder::DebugSessionRecorder(QObject* parent)
    : QObject(parent), m_state(RecorderState::Idle), m_nextSnapshotId(1), m_nextStepId(1),
      m_nextBookmarkId(1), m_currentSnapshotId(-1), m_playbackSpeed(1.0), m_compressionEnabled(false) {
}

void DebugSessionRecorder::startRecording(const QString& sessionName) {
    m_sessionName = sessionName;
    m_state = RecorderState::Recording;
    m_recordingStartTime = QDateTime::currentMSecsSinceEpoch();
    m_snapshots.clear();
    m_executionSteps.clear();
    m_variableCache.clear();
    m_memoryCache.clear();
    emit recordingStarted();
}

void DebugSessionRecorder::stopRecording() {
    if (m_state == RecorderState::Recording) {
        m_recordingEndTime = QDateTime::currentMSecsSinceEpoch();
        m_state = RecorderState::Stopped;
        emit recordingStopped();
    }
}

bool DebugSessionRecorder::isRecording() const {
    return m_state == RecorderState::Recording;
}

QString DebugSessionRecorder::getSessionName() const {
    return m_sessionName;
}

int DebugSessionRecorder::captureSnapshot(const DebugSnapshot& snapshot) {
    if (m_state != RecorderState::Recording) {
        return -1;
    }
    
    DebugSnapshot snap = snapshot;
    snap.id = m_nextSnapshotId++;
    snap.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_snapshots[snap.id] = snap;
    m_currentSnapshotId = snap.id;
    
    // Cache variables for quick lookup
    m_variableCache[snap.id] = snap.localVariables;
    
    emit snapshotCaptured(snap.id);
    return snap.id;
}

DebugSnapshot DebugSessionRecorder::getSnapshot(int id) const {
    return m_snapshots.value(id, DebugSnapshot());
}

QList<DebugSnapshot> DebugSessionRecorder::getAllSnapshots() const {
    return m_snapshots.values();
}

QList<DebugSnapshot> DebugSessionRecorder::getSnapshotsInTimeRange(qint64 startTime, qint64 endTime) const {
    QList<DebugSnapshot> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.timestamp >= startTime && snap.timestamp <= endTime) {
            result.append(snap);
        }
    }
    return result;
}

int DebugSessionRecorder::getSnapshotCount() const {
    return m_snapshots.size();
}

int DebugSessionRecorder::getTotalRecordedMemorySize() const {
    int total = 0;
    for (const auto& snap : m_snapshots.values()) {
        total += snap.getSizeBytes();
    }
    return total;
}

void DebugSessionRecorder::seekToSnapshot(int snapshotId) {
    if (!m_snapshots.contains(snapshotId)) {
        return;
    }
    
    m_currentSnapshotId = snapshotId;
    emit snapshotChanged(snapshotId);
}

void DebugSessionRecorder::seekToTime(qint64 timestamp) {
    for (auto it = m_snapshots.begin(); it != m_snapshots.end(); ++it) {
        if (it.value().timestamp >= timestamp) {
            seekToSnapshot(it.key());
            return;
        }
    }
}

void DebugSessionRecorder::stepForward() {
    if (m_currentSnapshotId >= 0) {
        int nextId = getNextSnapshotId(m_currentSnapshotId);
        if (nextId >= 0) {
            seekToSnapshot(nextId);
        }
    }
}

void DebugSessionRecorder::stepBackward() {
    if (m_currentSnapshotId >= 0) {
        int prevId = getPreviousSnapshotId(m_currentSnapshotId);
        if (prevId >= 0) {
            seekToSnapshot(prevId);
        }
    }
}

void DebugSessionRecorder::stepToBreakpoint() {
    if (m_currentSnapshotId < 0) {
        return;
    }
    
    int current = m_currentSnapshotId;
    while (true) {
        int next = getNextSnapshotId(current);
        if (next < 0) break;
        
        if (m_snapshots[next].breakpointHit) {
            seekToSnapshot(next);
            return;
        }
        current = next;
    }
}

void DebugSessionRecorder::stepOverBreakpoint() {
    if (m_currentSnapshotId < 0) {
        return;
    }
    
    if (m_snapshots[m_currentSnapshotId].breakpointHit) {
        stepForward();
    }
}

int DebugSessionRecorder::getCurrentSnapshotId() const {
    return m_currentSnapshotId;
}

int DebugSessionRecorder::getNextSnapshotId(int fromId) const {
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.begin(), ids.end());
    
    auto it = std::upper_bound(ids.begin(), ids.end(), fromId);
    return it != ids.end() ? *it : -1;
}

int DebugSessionRecorder::getPreviousSnapshotId(int fromId) const {
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.rbegin(), ids.rend());
    
    auto it = std::upper_bound(ids.begin(), ids.end(), fromId);
    return it != ids.end() ? *it : -1;
}

int DebugSessionRecorder::recordExecutionStep(const ExecutionStep& step) {
    ExecutionStep s = step;
    s.id = m_nextStepId++;
    m_executionSteps[s.id] = s;
    return s.id;
}

ExecutionStep DebugSessionRecorder::getExecutionStep(int stepId) const {
    return m_executionSteps.value(stepId, ExecutionStep());
}

QList<ExecutionStep> DebugSessionRecorder::getExecutionSteps(int fromSnapshot, int toSnapshot) const {
    QList<ExecutionStep> result;
    for (const auto& step : m_executionSteps.values()) {
        if (step.snapshotId >= fromSnapshot && step.snapshotId <= toSnapshot) {
            result.append(step);
        }
    }
    return result;
}

QList<ExecutionStep> DebugSessionRecorder::getExecutionStepsInTimeRange(qint64 startTime, qint64 endTime) const {
    QList<ExecutionStep> result;
    for (const auto& step : m_executionSteps.values()) {
        if (step.timestamp >= startTime && step.timestamp <= endTime) {
            result.append(step);
        }
    }
    return result;
}

int DebugSessionRecorder::getExecutionStepCount() const {
    return m_executionSteps.size();
}

void DebugSessionRecorder::startPlayback(int startSnapshotId) {
    if (m_snapshots.isEmpty()) {
        return;
    }
    
    m_state = RecorderState::Playing;
    m_currentSnapshotId = startSnapshotId >= 0 ? startSnapshotId : m_snapshots.keys().first();
    emit playbackStarted();
}

void DebugSessionRecorder::pausePlayback() {
    if (m_state == RecorderState::Playing) {
        m_state = RecorderState::Paused;
    }
}

void DebugSessionRecorder::resumePlayback() {
    if (m_state == RecorderState::Paused) {
        m_state = RecorderState::Playing;
    }
}

void DebugSessionRecorder::stopPlayback() {
    if (m_state == RecorderState::Playing || m_state == RecorderState::Paused) {
        m_state = RecorderState::Stopped;
        emit playbackStopped();
    }
}

void DebugSessionRecorder::setPlaybackSpeed(double speedMultiplier) {
    m_playbackSpeed = speedMultiplier;
}

bool DebugSessionRecorder::isPlayingBack() const {
    return m_state == RecorderState::Playing;
}

int DebugSessionRecorder::getPlaybackPosition() const {
    return m_currentSnapshotId;
}

QString DebugSessionRecorder::getVariableValueAtSnapshot(int snapshotId, const QString& varName) const {
    if (!m_snapshots.contains(snapshotId)) {
        return "";
    }
    return m_snapshots[snapshotId].localVariables.value(varName, "");
}

QMap<QString, QString> DebugSessionRecorder::getVariablesAtSnapshot(int snapshotId) const {
    if (!m_snapshots.contains(snapshotId)) {
        return QMap<QString, QString>();
    }
    return m_snapshots[snapshotId].localVariables;
}

QList<QString> DebugSessionRecorder::getVariableChanges(const QString& varName) const {
    QList<QString> changes;
    QString lastValue;
    
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        QString currentValue = m_snapshots[id].localVariables.value(varName, "");
        if (currentValue != lastValue) {
            changes.append(currentValue);
            lastValue = currentValue;
        }
    }
    
    return changes;
}

QList<int> DebugSessionRecorder::findSnapshotsWhereVariableChanged(const QString& varName) const {
    QList<int> result;
    QString lastValue;
    
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        QString currentValue = m_snapshots[id].localVariables.value(varName, "");
        if (currentValue != lastValue) {
            result.append(id);
            lastValue = currentValue;
        }
    }
    
    return result;
}

QByteArray DebugSessionRecorder::getMemoryAtSnapshot(int snapshotId, uintptr_t address, size_t size) const {
    if (!m_snapshots.contains(snapshotId)) {
        return QByteArray();
    }
    
    const auto& snap = m_snapshots[snapshotId];
    if (snap.memoryRegions.contains(address)) {
        return snap.memoryRegions[address];
    }
    
    return QByteArray(static_cast<int>(size), 0);
}

QList<uintptr_t> DebugSessionRecorder::getModifiedMemoryLocations(int fromSnapshot, int toSnapshot) const {
    QSet<uintptr_t> modified;
    
    if (!m_snapshots.contains(fromSnapshot) || !m_snapshots.contains(toSnapshot)) {
        return QList<uintptr_t>();
    }
    
    const auto& fromMem = m_snapshots[fromSnapshot].memoryRegions;
    const auto& toMem = m_snapshots[toSnapshot].memoryRegions;
    
    for (auto it = fromMem.begin(); it != fromMem.end(); ++it) {
        if (toMem.value(it.key()) != it.value()) {
            modified.insert(it.key());
        }
    }
    
    for (auto it = toMem.begin(); it != toMem.end(); ++it) {
        if (!fromMem.contains(it.key())) {
            modified.insert(it.key());
        }
    }
    
    return modified.values();
}

QMap<uintptr_t, QByteArray> DebugSessionRecorder::getMemoryDiffBetweenSnapshots(int fromSnapshot, int toSnapshot) const {
    QMap<uintptr_t, QByteArray> diff;
    
    if (!m_snapshots.contains(fromSnapshot) || !m_snapshots.contains(toSnapshot)) {
        return diff;
    }
    
    const auto& fromMem = m_snapshots[fromSnapshot].memoryRegions;
    const auto& toMem = m_snapshots[toSnapshot].memoryRegions;
    
    for (auto it = fromMem.begin(); it != fromMem.end(); ++it) {
        if (toMem.value(it.key()) != it.value()) {
            diff[it.key()] = toMem[it.key()];
        }
    }
    
    return diff;
}

QList<int> DebugSessionRecorder::getBreakpointsHitInTimeRange(qint64 startTime, qint64 endTime) const {
    QList<int> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.timestamp >= startTime && snap.timestamp <= endTime && snap.breakpointHit) {
            result.append(snap.breakpointId);
        }
    }
    return result;
}

int DebugSessionRecorder::getBreakpointHitCount(int breakpointId) const {
    int count = 0;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.breakpointHit && snap.breakpointId == breakpointId) {
            count++;
        }
    }
    return count;
}

int DebugSessionRecorder::getFirstBreakpointHit() const {
    for (auto snap : m_snapshots.values()) {
        if (snap.breakpointHit) {
            return snap.breakpointId;
        }
    }
    return -1;
}

int DebugSessionRecorder::getLastBreakpointHit() const {
    int last = -1;
    for (auto snap : m_snapshots.values()) {
        if (snap.breakpointHit) {
            last = snap.breakpointId;
        }
    }
    return last;
}

QList<int> DebugSessionRecorder::getThreadIds() const {
    QSet<int> threads;
    for (const auto& snap : m_snapshots.values()) {
        threads.insert(snap.threadId);
    }
    return threads.values();
}

QString DebugSessionRecorder::getThreadStateAtSnapshot(int snapshotId, int threadId) const {
    if (!m_snapshots.contains(snapshotId)) {
        return "";
    }
    
    const auto& snap = m_snapshots[snapshotId];
    if (snap.threadId == threadId) {
        return snap.threadState;
    }
    
    return "";
}

QList<int> DebugSessionRecorder::getSnapshotsForThread(int threadId) const {
    QList<int> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.threadId == threadId) {
            result.append(snap.id);
        }
    }
    return result;
}

QString DebugSessionRecorder::getCallStackAtSnapshot(int snapshotId) const {
    if (!m_snapshots.contains(snapshotId)) {
        return "";
    }
    return m_snapshots[snapshotId].callStack;
}

QList<QString> DebugSessionRecorder::getCallStackFramesAtSnapshot(int snapshotId) const {
    QString stack = getCallStackAtSnapshot(snapshotId);
    return stack.split('\n', Qt::SkipEmptyParts);
}

QList<int> DebugSessionRecorder::findSnapshotsInFunction(const QString& functionName) const {
    QList<int> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.callStack.contains(functionName)) {
            result.append(snap.id);
        }
    }
    return result;
}

qint64 DebugSessionRecorder::getTotalRecordingTime() const {
    if (m_recordingEndTime <= 0 || m_recordingStartTime <= 0) {
        return -1;
    }
    return m_recordingEndTime - m_recordingStartTime;
}

double DebugSessionRecorder::getAverageSnapshotInterval() const {
    if (m_snapshots.size() < 2) {
        return 0.0;
    }
    
    qint64 totalTime = 0;
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.begin(), ids.end());
    
    for (int i = 1; i < ids.size(); ++i) {
        totalTime += m_snapshots[ids[i]].timestamp - m_snapshots[ids[i-1]].timestamp;
    }
    
    return static_cast<double>(totalTime) / (ids.size() - 1);
}

int DebugSessionRecorder::getLongestUnchangedVariableSpan() const {
    int longest = 0;
    
    for (auto it = m_variableCache.begin(); it != m_variableCache.end(); ++it) {
        int span = 0;
        QString lastValue;
        
        for (const auto& var : it.value()) {
            if (var == lastValue) {
                span++;
                longest = std::max(longest, span);
            } else {
                span = 1;
                lastValue = var;
            }
        }
    }
    
    return longest;
}

QMap<QString, int> DebugSessionRecorder::getVariableChangeFrequency() const {
    QMap<QString, int> frequency;
    
    for (auto it = m_variableCache.begin(); it != m_variableCache.end(); ++it) {
        for (auto varIt = it.value().begin(); varIt != it.value().end(); ++varIt) {
            frequency[varIt.key()]++;
        }
    }
    
    return frequency;
}

QList<DebugSnapshot> DebugSessionRecorder::findSnapshotsWhere(const QString& varName, const QString& value) const {
    QList<DebugSnapshot> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.localVariables.value(varName) == value) {
            result.append(snap);
        }
    }
    return result;
}

QList<DebugSnapshot> DebugSessionRecorder::findSnapshotsAtAddress(uintptr_t address) const {
    QList<DebugSnapshot> result;
    for (const auto& snap : m_snapshots.values()) {
        if (snap.instructionPointer == address) {
            result.append(snap);
        }
    }
    return result;
}

QList<ExecutionStep> DebugSessionRecorder::findExecutionSteps(const QString& pattern) const {
    QList<ExecutionStep> result;
    for (const auto& step : m_executionSteps.values()) {
        if (step.mnemonic.contains(pattern) || step.eventDescription.contains(pattern)) {
            result.append(step);
        }
    }
    return result;
}

void DebugSessionRecorder::compareSnapshots(int snapshot1Id, int snapshot2Id) {
    // Implementation would compare two snapshots
}

QMap<QString, QPair<QString, QString>> DebugSessionRecorder::getVariableDifferences(int snapshot1Id, int snapshot2Id) const {
    QMap<QString, QPair<QString, QString>> diffs;
    
    if (!m_snapshots.contains(snapshot1Id) || !m_snapshots.contains(snapshot2Id)) {
        return diffs;
    }
    
    const auto& vars1 = m_snapshots[snapshot1Id].localVariables;
    const auto& vars2 = m_snapshots[snapshot2Id].localVariables;
    
    for (auto it = vars1.begin(); it != vars1.end(); ++it) {
        if (vars2.value(it.key()) != it.value()) {
            diffs[it.key()] = qMakePair(it.value(), vars2.value(it.key()));
        }
    }
    
    return diffs;
}

QString DebugSessionRecorder::generateDiffReport(int snapshot1Id, int snapshot2Id) const {
    QString report = QString("Diff Report: Snapshot %1 → %2\n").arg(snapshot1Id).arg(snapshot2Id);
    
    auto diffs = getVariableDifferences(snapshot1Id, snapshot2Id);
    for (auto it = diffs.begin(); it != diffs.end(); ++it) {
        report += QString("  %1: %2 → %3\n").arg(it.key(), it.value().first, it.value().second);
    }
    
    return report;
}

void DebugSessionRecorder::captureFullState() {
    // Implementation would capture full debugger state
}

bool DebugSessionRecorder::restoreToSnapshot(int snapshotId) {
    if (!m_snapshots.contains(snapshotId)) {
        return false;
    }
    
    seekToSnapshot(snapshotId);
    emit timeTravelCompleted(snapshotId);
    return true;
}

QList<StateTransition> DebugSessionRecorder::getTransitionHistory() const {
    return m_transitionHistory;
}

void DebugSessionRecorder::exportSession(const QString& filePath) const {
    QJsonObject root = toJson();
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void DebugSessionRecorder::importSession(const QString& filePath) {
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

QString DebugSessionRecorder::generateReport() const {
    QString report = QString("Debug Session Report: %1\n").arg(m_sessionName);
    report += QString("Total Snapshots: %1\n").arg(m_snapshots.size());
    report += QString("Total Execution Steps: %1\n").arg(m_executionSteps.size());
    report += QString("Recording Time: %1 ms\n").arg(getTotalRecordingTime());
    report += QString("Memory Used: %1 bytes\n").arg(getTotalRecordedMemorySize());
    
    return report;
}

QString DebugSessionRecorder::generateTimelineReport() const {
    QString report = "Timeline Report:\n";
    
    QList<int> ids = m_snapshots.keys();
    std::sort(ids.begin(), ids.end());
    
    for (int id : ids) {
        const auto& snap = m_snapshots[id];
        report += QString("  [%1] IP=0x%2 Thread=%3\n")
            .arg(id).arg(snap.instructionPointer, 0, 16).arg(snap.threadId);
    }
    
    return report;
}

void DebugSessionRecorder::saveSession(const QString& filePath) const {
    exportSession(filePath);
}

void DebugSessionRecorder::loadSession(const QString& filePath) {
    importSession(filePath);
}

QJsonObject DebugSessionRecorder::toJson() const {
    QJsonObject root;
    root["sessionName"] = m_sessionName;
    root["recordingTime"] = getTotalRecordingTime();
    
    QJsonArray snapshotArray;
    for (const auto& snap : m_snapshots.values()) {
        snapshotArray.append(snap.toJson());
    }
    root["snapshots"] = snapshotArray;
    
    QJsonArray stepArray;
    for (const auto& step : m_executionSteps.values()) {
        stepArray.append(step.toJson());
    }
    root["executionSteps"] = stepArray;
    
    return root;
}

void DebugSessionRecorder::fromJson(const QJsonObject& obj) {
    m_sessionName = obj["sessionName"].toString();
    
    QJsonArray snapshotArray = obj["snapshots"].toArray();
    for (const auto& snapJson : snapshotArray) {
        DebugSnapshot snap = DebugSnapshot::fromJson(snapJson.toObject());
        m_snapshots[snap.id] = snap;
        m_nextSnapshotId = std::max(m_nextSnapshotId, snap.id + 1);
    }
    
    QJsonArray stepArray = obj["executionSteps"].toArray();
    for (const auto& stepJson : stepArray) {
        ExecutionStep step = ExecutionStep::fromJson(stepJson.toObject());
        m_executionSteps[step.id] = step;
        m_nextStepId = std::max(m_nextStepId, step.id + 1);
    }
}

int DebugSessionRecorder::bookmarkCurrentSnapshot(const QString& label) {
    if (m_currentSnapshotId < 0) {
        return -1;
    }
    
    int id = m_nextBookmarkId++;
    m_bookmarks[id] = label;
    return id;
}

void DebugSessionRecorder::removeBookmark(int bookmarkId) {
    m_bookmarks.remove(bookmarkId);
}

QMap<int, QString> DebugSessionRecorder::getAllBookmarks() const {
    return m_bookmarks;
}

QList<int> DebugSessionRecorder::getSnapshotsByBookmark(const QString& label) const {
    QList<int> result;
    for (auto it = m_bookmarks.begin(); it != m_bookmarks.end(); ++it) {
        if (it.value() == label) {
            result.append(it.key());
        }
    }
    return result;
}

void DebugSessionRecorder::enableSnapshotCompression(bool enabled) {
    m_compressionEnabled = enabled;
}

void DebugSessionRecorder::pruneOldSnapshots(qint64 ageThreshold) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<int> toRemove;
    
    for (auto it = m_snapshots.begin(); it != m_snapshots.end(); ++it) {
        if ((now - it.value().timestamp) > ageThreshold) {
            toRemove.append(it.key());
        }
    }
    
    for (int id : toRemove) {
        m_snapshots.remove(id);
        m_variableCache.remove(id);
        m_memoryCache.remove(id);
    }
}

void DebugSessionRecorder::optimizeMemoryUsage() {
    // Implementation would compress snapshots or consolidate data
}

// ============================================================================
// TimeTravelDebugger Implementation
// ============================================================================

TimeTravelDebugger::TimeTravelDebugger(DebugSessionRecorder* recorder)
    : m_recorder(recorder) {
}

void TimeTravelDebugger::goToSnapshot(int id) {
    if (m_recorder) {
        m_recorder->seekToSnapshot(id);
    }
}

void TimeTravelDebugger::goToTime(qint64 time) {
    if (m_recorder) {
        m_recorder->seekToTime(time);
    }
}

void TimeTravelDebugger::goToBreakpoint(int breakpointId) {
    if (!m_recorder) {
        return;
    }
    
    for (const auto& snap : m_recorder->getAllSnapshots()) {
        if (snap.breakpointHit && snap.breakpointId == breakpointId) {
            m_recorder->seekToSnapshot(snap.id);
            return;
        }
    }
}

void TimeTravelDebugger::goToFunctionEntry(const QString& function) {
    if (!m_recorder) {
        return;
    }
    
    auto snapshots = m_recorder->findSnapshotsInFunction(function);
    if (!snapshots.isEmpty()) {
        m_recorder->seekToSnapshot(snapshots.first());
    }
}

void TimeTravelDebugger::goToFunctionExit(const QString& function) {
    if (!m_recorder) {
        return;
    }
    
    auto snapshots = m_recorder->findSnapshotsInFunction(function);
    if (!snapshots.isEmpty()) {
        m_recorder->seekToSnapshot(snapshots.last());
    }
}

QString TimeTravelDebugger::getExecutionPath() const {
    if (!m_recorder) {
        return "";
    }
    
    QString path;
    for (const auto& snap : m_recorder->getAllSnapshots()) {
        path += QString("0x%1 ").arg(snap.instructionPointer, 0, 16);
    }
    
    return path;
}

QList<QString> TimeTravelDebugger::getDataFlowForVariable(const QString& var) const {
    if (!m_recorder) {
        return QList<QString>();
    }
    
    return m_recorder->getVariableChanges(var);
}

QList<uintptr_t> TimeTravelDebugger::getControlFlowPath() const {
    if (!m_recorder) {
        return QList<uintptr_t>();
    }
    
    QList<uintptr_t> path;
    for (const auto& snap : m_recorder->getAllSnapshots()) {
        path.append(snap.instructionPointer);
    }
    
    return path;
}

void TimeTravelDebugger::watchVariable(const QString& var) {
    m_watchedVariables.append(var);
}

void TimeTravelDebugger::unwatchVariable(const QString& var) {
    m_watchedVariables.removeAll(var);
}

QList<QString> TimeTravelDebugger::getWatchedVariables() const {
    return m_watchedVariables;
}
