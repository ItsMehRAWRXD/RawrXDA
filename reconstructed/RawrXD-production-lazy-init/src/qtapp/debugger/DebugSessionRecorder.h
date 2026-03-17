#ifndef DEBUGSESSIONRECORDER_H
#define DEBUGSESSIONRECORDER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QDateTime>
#include <QByteArray>
#include <cstdint>
#include <memory>

enum class RecorderState {
    Idle,
    Recording,
    Playing,
    Paused,
    Stopped
};

// Snapshot of debugger state at a specific point in time
struct DebugSnapshot {
    int id;
    qint64 timestamp;
    uintptr_t instructionPointer;
    QMap<QString, uintptr_t> registers;
    QMap<QString, QString> localVariables;
    QMap<uintptr_t, QByteArray> memoryRegions;
    QString callStack;
    QString threadState;
    int threadId;
    bool breakpointHit;
    int breakpointId;
    
    QJsonObject toJson() const;
    static DebugSnapshot fromJson(const QJsonObject& obj);
    size_t getSizeBytes() const;
};

// Represents a single step/event in execution
struct ExecutionStep {
    int id;
    int snapshotId;
    QString eventType;
    QString eventDescription;
    qint64 timestamp;
    uintptr_t address;
    QString mnemonic;
    QList<QString> operands;
    
    QJsonObject toJson() const;
    static ExecutionStep fromJson(const QJsonObject& obj);
};

// State transition when navigating through recorded history
struct StateTransition {
    int fromSnapshotId;
    int toSnapshotId;
    ExecutionStep step;
    QString transitionType;
    qint64 elapsedTime;
};

// Main session recorder for debugging
class DebugSessionRecorder : public QObject {
    Q_OBJECT

public:
    explicit DebugSessionRecorder(QObject* parent = nullptr);
    ~DebugSessionRecorder() override = default;

    // Recording control
    void startRecording(const QString& sessionName);
    void stopRecording();
    bool isRecording() const;
    QString getSessionName() const;
    
    // Snapshot management
    int captureSnapshot(const DebugSnapshot& snapshot);
    DebugSnapshot getSnapshot(int id) const;
    QList<DebugSnapshot> getAllSnapshots() const;
    QList<DebugSnapshot> getSnapshotsInTimeRange(qint64 startTime, qint64 endTime) const;
    int getSnapshotCount() const;
    int getTotalRecordedMemorySize() const;
    
    // Time-travel navigation
    void seekToSnapshot(int snapshotId);
    void seekToTime(qint64 timestamp);
    void stepForward();
    void stepBackward();
    void stepToBreakpoint();
    void stepOverBreakpoint();
    int getCurrentSnapshotId() const;
    int getNextSnapshotId(int fromId) const;
    int getPreviousSnapshotId(int fromId) const;
    
    // Execution step tracking
    int recordExecutionStep(const ExecutionStep& step);
    ExecutionStep getExecutionStep(int stepId) const;
    QList<ExecutionStep> getExecutionSteps(int fromSnapshot, int toSnapshot) const;
    QList<ExecutionStep> getExecutionStepsInTimeRange(qint64 startTime, qint64 endTime) const;
    int getExecutionStepCount() const;
    
    // Playback control
    void startPlayback(int startSnapshotId = 0);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();
    void setPlaybackSpeed(double speedMultiplier);
    bool isPlayingBack() const;
    int getPlaybackPosition() const;
    
    // Variable inspection across time
    QString getVariableValueAtSnapshot(int snapshotId, const QString& varName) const;
    QMap<QString, QString> getVariablesAtSnapshot(int snapshotId) const;
    QList<QString> getVariableChanges(const QString& varName) const;
    QList<int> findSnapshotsWhereVariableChanged(const QString& varName) const;
    
    // Memory inspection across time
    QByteArray getMemoryAtSnapshot(int snapshotId, uintptr_t address, size_t size) const;
    QList<uintptr_t> getModifiedMemoryLocations(int fromSnapshot, int toSnapshot) const;
    QMap<uintptr_t, QByteArray> getMemoryDiffBetweenSnapshots(int fromSnapshot, int toSnapshot) const;
    
    // Breakpoint tracking across time
    QList<int> getBreakpointsHitInTimeRange(qint64 startTime, qint64 endTime) const;
    int getBreakpointHitCount(int breakpointId) const;
    int getFirstBreakpointHit() const;
    int getLastBreakpointHit() const;
    
    // Thread state analysis
    QList<int> getThreadIds() const;
    QString getThreadStateAtSnapshot(int snapshotId, int threadId) const;
    QList<int> getSnapshotsForThread(int threadId) const;
    
    // Call stack analysis
    QString getCallStackAtSnapshot(int snapshotId) const;
    QList<QString> getCallStackFramesAtSnapshot(int snapshotId) const;
    QList<int> findSnapshotsInFunction(const QString& functionName) const;
    
    // Statistics
    qint64 getTotalRecordingTime() const;
    double getAverageSnapshotInterval() const;
    int getLongestUnchangedVariableSpan() const;
    QMap<QString, int> getVariableChangeFrequency() const;
    
    // Filtering and search
    QList<DebugSnapshot> findSnapshotsWhere(const QString& varName, const QString& value) const;
    QList<DebugSnapshot> findSnapshotsAtAddress(uintptr_t address) const;
    QList<ExecutionStep> findExecutionSteps(const QString& pattern) const;
    
    // Comparison and diff
    void compareSnapshots(int snapshot1Id, int snapshot2Id);
    QMap<QString, QPair<QString, QString>> getVariableDifferences(int snapshot1Id, int snapshot2Id) const;
    QString generateDiffReport(int snapshot1Id, int snapshot2Id) const;
    
    // State machine for debugging
    void captureFullState();
    bool restoreToSnapshot(int snapshotId);
    QList<StateTransition> getTransitionHistory() const;
    
    // Export and analysis
    void exportSession(const QString& filePath) const;
    void importSession(const QString& filePath);
    QString generateReport() const;
    QString generateTimelineReport() const;
    
    // Persistence
    void saveSession(const QString& filePath) const;
    void loadSession(const QString& filePath);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
    
    // Bookmarks for key moments
    int bookmarkCurrentSnapshot(const QString& label);
    void removeBookmark(int bookmarkId);
    QMap<int, QString> getAllBookmarks() const;
    QList<int> getSnapshotsByBookmark(const QString& label) const;
    
    // Memory optimization
    void enableSnapshotCompression(bool enabled);
    void pruneOldSnapshots(qint64 ageThreshold);
    void optimizeMemoryUsage();

signals:
    void snapshotCaptured(int snapshotId);
    void recordingStarted();
    void recordingStopped();
    void playbackStarted();
    void playbackStopped();
    void snapshotChanged(int snapshotId);
    void breakpointHit(int breakpointId, int snapshotId);
    void variableChanged(const QString& varName, int snapshotId);
    void timeTravelCompleted(int snapshotId);

private:
    RecorderState m_state;
    QString m_sessionName;
    qint64 m_recordingStartTime;
    qint64 m_recordingEndTime;
    int m_nextSnapshotId;
    int m_nextStepId;
    int m_nextBookmarkId;
    int m_currentSnapshotId;
    
    QMap<int, DebugSnapshot> m_snapshots;
    QMap<int, ExecutionStep> m_executionSteps;
    QMap<int, QString> m_bookmarks;
    QList<StateTransition> m_transitionHistory;
    
    double m_playbackSpeed;
    bool m_compressionEnabled;
    
    // Caching for optimization
    QMap<int, QMap<QString, QString>> m_variableCache;
    QMap<int, QMap<uintptr_t, QByteArray>> m_memoryCache;
};

// Time-travel debugger interface
class TimeTravelDebugger {
public:
    explicit TimeTravelDebugger(DebugSessionRecorder* recorder);
    
    // Navigation
    void goToSnapshot(int id);
    void goToTime(qint64 time);
    void goToBreakpoint(int breakpointId);
    void goToFunctionEntry(const QString& function);
    void goToFunctionExit(const QString& function);
    
    // Analysis
    QString getExecutionPath() const;
    QList<QString> getDataFlowForVariable(const QString& var) const;
    QList<uintptr_t> getControlFlowPath() const;
    
    // Watchpoints
    void watchVariable(const QString& var);
    void unwatchVariable(const QString& var);
    QList<QString> getWatchedVariables() const;
    
private:
    DebugSessionRecorder* m_recorder;
    QList<QString> m_watchedVariables;
};

#endif // DEBUGSESSIONRECORDER_H
