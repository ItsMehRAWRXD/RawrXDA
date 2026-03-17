#pragma once

#include <QObject>
#include <QDateTime>
#include <QTimer>
#include <QMap>
#include <QList>
#include <QElapsedTimer>
#include "ProfileData.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

#ifdef __linux__
#include <unistd.h>
#include <sys/resource.h>
#endif

namespace RawrXD {

struct MemoryAllocation {
    quint64 address = 0;    // pointer address
    size_t size = 0;        // bytes
    QString allocator;      // malloc/new/new[]/custom
    QDateTime timestamp;
    QStringList callStack;  // symbolic stack at allocation time
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["address"] = QString::number(address, 16);
        obj["size"] = static_cast<double>(size);
        obj["allocator"] = allocator;
        obj["timestamp"] = timestamp.toString(Qt::ISODate);
        QJsonArray stack;
        for (const auto &s : callStack) stack.append(s);
        obj["callStack"] = stack;
        return obj;
    }
};

struct MemorySnapshot {
    quint64 timestampUs = 0;
    quint64 totalAllocatedBytes = 0;
    quint64 currentUsageBytes = 0;
    quint64 peakUsageBytes = 0;
    QMap<size_t, int> allocationSizes; // histogram of allocation sizes
    QList<MemoryAllocation> allocations; // tracked allocations at snapshot

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["timestampUs"] = static_cast<double>(timestampUs);
        obj["totalAllocatedBytes"] = static_cast<double>(totalAllocatedBytes);
        obj["currentUsageBytes"] = static_cast<double>(currentUsageBytes);
        obj["peakUsageBytes"] = static_cast<double>(peakUsageBytes);
        QJsonArray allocArr;
        for (const auto &a : allocations) allocArr.append(a.toJson());
        obj["allocations"] = allocArr;
        return obj;
    }
};

class MemoryProfiler : public QObject {
    Q_OBJECT
public:
    explicit MemoryProfiler(QObject *parent = nullptr);

    void startProfiling();
    void stopProfiling();
    bool isProfiling() const { return m_isProfiling; }

    void setTrackingMode(bool trackAllocs, bool trackFrees) { m_trackAllocs = trackAllocs; m_trackFrees = trackFrees; }
    void setMinAllocationSize(size_t minSize) { m_minAllocationSize = minSize; }

    MemorySnapshot takeSnapshot() const;
    QList<MemoryAllocation> getLeaks() const; // retained allocations that were not freed
    quint64 getTotalAllocated() const { return m_totalAllocatedBytes; }
    quint64 getCurrentUsage() const { return currentProcessUsageBytes(); }

    // Manual tracking APIs (for instrumented regions)
    void recordAllocation(quint64 address, size_t size, const QString &allocator, const QStringList &stackSymbols);
    void recordFree(quint64 address);

signals:
    void profilingStarted();
    void profilingStopped();
    void snapshotTaken(const MemorySnapshot &snapshot);
    void leakDetected(const MemoryAllocation &alloc);

private slots:
    void periodicSnapshot();

private:
    quint64 currentProcessUsageBytes() const;

private:
    bool m_isProfiling = false;
    bool m_trackAllocs = true;
    bool m_trackFrees = true;
    size_t m_minAllocationSize = 0;

    QTimer m_timer;
    quint64 m_totalAllocatedBytes = 0;
    quint64 m_peakUsageBytes = 0;

    QMap<quint64, MemoryAllocation> m_liveAllocations; // address -> allocation
    mutable QElapsedTimer m_latencyTimer;
};

} // namespace RawrXD
