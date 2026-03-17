#include "MemoryProfiler.h"
#include <QDebug>
#include "logging/structured_logger.h"

using namespace RawrXD;

MemoryProfiler::MemoryProfiler(QObject *parent)
    : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &MemoryProfiler::periodicSnapshot);
}

void MemoryProfiler::startProfiling() {
    if (m_isProfiling) return;
    m_isProfiling = true;
    m_latencyTimer.start();
    emit profilingStarted();
    m_timer.start(1000); // snapshot every second
}

void MemoryProfiler::stopProfiling() {
    if (!m_isProfiling) return;
    m_timer.stop();
    m_isProfiling = false;
    emit profilingStopped();
}

void MemoryProfiler::recordAllocation(quint64 address, size_t size, const QString &allocator, const QStringList &stackSymbols) {
    if (!m_trackAllocs) return;
    if (size < m_minAllocationSize) return;
    MemoryAllocation alloc;
    alloc.address = address;
    alloc.size = size;
    alloc.allocator = allocator;
    alloc.timestamp = QDateTime::currentDateTimeUtc();
    alloc.callStack = stackSymbols;
    m_liveAllocations[address] = alloc;
    m_totalAllocatedBytes += size;
    m_peakUsageBytes = qMax(m_peakUsageBytes, currentProcessUsageBytes());
}

void MemoryProfiler::recordFree(quint64 address) {
    if (!m_trackFrees) return;
    m_liveAllocations.remove(address);
}

MemorySnapshot MemoryProfiler::takeSnapshot() const {
    MemorySnapshot snap;
    snap.timestampUs = m_latencyTimer.nsecsElapsed() / 1000ULL;
    snap.totalAllocatedBytes = m_totalAllocatedBytes;
    snap.currentUsageBytes = currentProcessUsageBytes();
    snap.peakUsageBytes = m_peakUsageBytes;
    snap.allocations = m_liveAllocations.values();
    // Build histogram
    for (const auto &a : snap.allocations) {
        snap.allocationSizes[a.size] += 1;
    }
    return snap;
}

QList<MemoryAllocation> MemoryProfiler::getLeaks() const {
    return m_liveAllocations.values();
}

void MemoryProfiler::periodicSnapshot() {
    if (!m_isProfiling) return;
    MemorySnapshot snap = takeSnapshot();
    emit snapshotTaken(snap);
    // Structured logging: snapshot latency
    qint64 latencyUs = m_latencyTimer.nsecsElapsed() / 1000;
    qDebug() << "MemoryProfiler snapshot latency(us)=" << latencyUs
             << " currentUsageBytes=" << snap.currentUsageBytes
             << " liveAllocations=" << m_liveAllocations.size();
    m_latencyTimer.restart();
    RawrXD::StructuredLogger::instance().recordMetric("profiler.phase7.snapshot.latency_us", static_cast<double>(latencyUs));
    RawrXD::StructuredLogger::instance().recordMetric("profiler.phase7.memory.rss_bytes", static_cast<double>(snap.currentUsageBytes));
    RawrXD::StructuredLogger::instance().recordMetric("profiler.phase7.memory.live_allocations", static_cast<double>(m_liveAllocations.size()));
}

quint64 MemoryProfiler::currentProcessUsageBytes() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#elif defined(__linux__)
    long rss = 0L;
    FILE* fp = nullptr;
    if ((fp = fopen("/proc/self/statm", "r")) == nullptr) return 0;
    long pages = 0;
    if (fscanf(fp, "%*s %ld", &pages) != 1) { fclose(fp); return 0; }
    fclose(fp);
    long page_size = sysconf(_SC_PAGESIZE);
    rss = pages * page_size;
    return (quint64)rss;
#else
    return 0;
#endif
}
