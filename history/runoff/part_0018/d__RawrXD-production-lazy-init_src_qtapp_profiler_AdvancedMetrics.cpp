#include "AdvancedMetrics.h"
#include <QMap>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace RawrXD {

// ============================================================================
// CallGraph Implementation
// ============================================================================

CallGraph::CallGraph(ProfileSession *session, QObject *parent)
    : QObject(parent), m_session(session) {
}

void CallGraph::analyzeSession() {
    if (!m_session) {
        qWarning() << "CallGraph: No session attached";
        return;
    }

    m_callEdges.clear();
    m_overheads.clear();

    extractCallEdges();
    computeOverhead();
}

void CallGraph::extractCallEdges() {
    QMap<QString, QMap<QString, CallEdge>> edgeMap;

    // Traverse all call stacks to build edges
    for (const auto &stack : m_session->callStacks()) {
        // Process consecutive frames as caller->callee relationships
        for (int i = 0; i < static_cast<int>(stack.frames.size()) - 1; ++i) {
            const auto &caller = stack.frames[i];
            const auto &callee = stack.frames[i + 1];

            auto &edge = edgeMap[caller.functionName][callee.functionName];
            if (edge.caller.isEmpty()) {
                edge.caller = caller.functionName;
                edge.callee = callee.functionName;
            }
            edge.callCount++;
            edge.totalTimeUs += callee.timeSpentUs;
        }
    }

    // Flatten map to list and calculate averages
    for (auto callerIt = edgeMap.begin(); callerIt != edgeMap.end(); ++callerIt) {
        for (auto calleeIt = callerIt.value().begin(); calleeIt != callerIt.value().end(); ++calleeIt) {
            CallEdge edge = calleeIt.value();
            edge.averageTimeUs = edge.callCount > 0 ? edge.totalTimeUs / edge.callCount : 0;
            m_callEdges.append(edge);
        }
    }

    // Sort by frequency
    std::sort(m_callEdges.begin(), m_callEdges.end(),
              [](const CallEdge &a, const CallEdge &b) {
                  return a.callCount > b.callCount;
              });
}

void CallGraph::computeOverhead() {
    const auto &stats = m_session->functionStats();

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FunctionOverhead oh;
        oh.functionName = it.value().functionName;
        oh.selfTimeUs = it.value().selfTimeUs;
        oh.totalTimeUs = it.value().totalTimeUs;
        oh.callCount = it.value().callCount;
        oh.childrenTimeUs = it.value().totalTimeUs > it.value().selfTimeUs
                                ? it.value().totalTimeUs - it.value().selfTimeUs
                                : 0;

        // Calculate percentages relative to total runtime
        quint64 totalRuntime = m_session->totalRuntimeUs();
        if (totalRuntime > 0) {
            oh.selfTimePercent = (double)oh.selfTimeUs / totalRuntime * 100.0;
            oh.childrenTimePercent = (double)oh.childrenTimeUs / totalRuntime * 100.0;
        }

        // Get callees
        for (const auto &edge : m_callEdges) {
            if (edge.caller == oh.functionName) {
                oh.callees.append(edge.callee);
            }
        }

        m_overheads[oh.functionName] = oh;
    }
}

QList<CallEdge> CallGraph::getCallersOf(const QString &function) const {
    QList<CallEdge> result;
    for (const auto &edge : m_callEdges) {
        if (edge.callee == function) {
            result.append(edge);
        }
    }
    return result;
}

QList<CallEdge> CallGraph::getCalleesOf(const QString &function) const {
    QList<CallEdge> result;
    for (const auto &edge : m_callEdges) {
        if (edge.caller == function) {
            result.append(edge);
        }
    }
    return result;
}

QMap<QString, FunctionOverhead> CallGraph::getFunctionOverheadAnalysis() const {
    return m_overheads;
}

QList<QStringList> CallGraph::getCriticalPaths(int topCount) const {
    // Simple heuristic: find longest chains by total time
    QList<QStringList> paths;

    // Start from highest-cost leaf functions
    QMap<QString, quint64> funcTimes;
    for (const auto &stat : m_session->functionStats()) {
        funcTimes[stat.functionName] = stat.totalTimeUs;
    }

    // Build chains recursively (simplified: just top-level expensive functions)
    QList<QString> sorted;
    for (auto it = funcTimes.begin(); it != funcTimes.end(); ++it) {
        sorted.append(it.key());
    }
    std::sort(sorted.begin(), sorted.end(),
              [&funcTimes](const QString &a, const QString &b) {
                  return funcTimes[a] > funcTimes[b];
              });

    for (int i = 0; i < std::min(topCount, static_cast<int>(sorted.size())); ++i) {
        paths.append({sorted[i]});
    }

    return paths;
}

QMap<QString, quint64> CallGraph::getCallFrequency() const {
    QMap<QString, quint64> freq;
    for (const auto &edge : m_callEdges) {
        freq[edge.callee] += edge.callCount;
    }
    return freq;
}

QJsonObject CallGraph::toJson() const {
    QJsonObject obj;

    QJsonArray edgesArray;
    for (const auto &edge : m_callEdges) {
        edgesArray.append(edge.toJson());
    }
    obj["callEdges"] = edgesArray;

    QJsonObject overheadMap;
    for (auto it = m_overheads.begin(); it != m_overheads.end(); ++it) {
        overheadMap[it.key()] = it.value().toJson();
    }
    obj["overheads"] = overheadMap;

    return obj;
}

// ============================================================================
// MemoryAnalyzer Implementation
// ============================================================================

MemoryAnalyzer::MemoryAnalyzer(ProfileSession *session, QObject *parent)
    : QObject(parent), m_session(session) {
}

void MemoryAnalyzer::analyzeForLeaks() {
    if (!m_session) {
        qWarning() << "MemoryAnalyzer: No session attached";
        return;
    }

    m_leakReport.totalLeakedBytes = 0;
    m_leakReport.leakCount = 0;
    m_leakReport.suspectedLeaks.clear();
    m_leakReport.leaksByFunction.clear();
    m_memoryTimeline.clear();

    // Reconstruct memory timeline from snapshots and identify unfreed allocations
    for (const auto &alloc : m_allocations) {
        if (!alloc.isFreed) {
            m_leakReport.suspectedLeaks.append(alloc);
            m_leakReport.totalLeakedBytes += alloc.sizeBytes;
            m_leakReport.leakCount++;

            // Accumulate by function
            m_leakReport.leaksByFunction[alloc.allocatingFunction] += alloc.sizeBytes;
        }
    }

    if (m_leakReport.leakCount > 0) {
        emit leaksDetected(m_leakReport.leakCount, m_leakReport.totalLeakedBytes);
    }

    emit allocationAnalysisComplete();
}

void MemoryAnalyzer::trackAllocations(const MemoryAllocation &alloc) {
    // Convert to extended allocation
    MemoryAllocationExtended ext;
    ext.timestamp = alloc.timestamp.toMSecsSinceEpoch() * 1000;
    ext.sizeBytes = alloc.size;
    ext.allocatingFunction = alloc.allocator;
    ext.callStack = alloc.callStack.join(" <- ");
    ext.address = alloc.address;
    ext.isFreed = false;
    ext.freedTimestamp = 0;
    ext.lifetimeUs = 0;
    
    m_allocations.append(ext);
    m_totalAllocated += alloc.size;

    // Update timeline
    quint64 timestampUs = alloc.timestamp.toMSecsSinceEpoch() * 1000;
    if (m_memoryTimeline.find(timestampUs) == m_memoryTimeline.end()) {
        m_memoryTimeline[timestampUs] = 0;
    }
    m_memoryTimeline[timestampUs] += alloc.size;
}

void MemoryAnalyzer::trackDeallocation(const QString &allocationId, quint64 timestamp) {
    if (m_allocationMap.contains(allocationId)) {
        auto &alloc = m_allocationMap[allocationId];
        alloc.isFreed = true;
        alloc.freedTimestamp = timestamp;
        alloc.lifetimeUs = timestamp - alloc.timestamp;
        m_totalFreed += alloc.sizeBytes;

        // Update timeline
        if (m_memoryTimeline.find(timestamp) == m_memoryTimeline.end()) {
            m_memoryTimeline[timestamp] = 0;
        }
        m_memoryTimeline[timestamp] -= alloc.sizeBytes;
    }
}

QList<AllocationHotspot> MemoryAnalyzer::getAllocationHotspots(int topCount) const {
    QMap<QString, AllocationHotspot> hotspots;

    // m_allocations is QList<MemoryAllocationExtended> - use allocatingFunction, sizeBytes
    for (const auto &alloc : m_allocations) {
        QString funcName = alloc.allocatingFunction.isEmpty() ? QStringLiteral("unknown") : alloc.allocatingFunction;
        auto &hs = hotspots[funcName];
        if (hs.functionName.isEmpty()) {
            hs.functionName = funcName;
        }
        hs.totalAllocatedBytes += static_cast<qint64>(alloc.sizeBytes);
        hs.allocationCount++;
        hs.peakAllocationBytes = qMax<qint64>(hs.peakAllocationBytes, static_cast<qint64>(alloc.sizeBytes));
    }

    // Calculate averages
    for (auto &hs : hotspots) {
        hs.averageAllocationBytes = hs.allocationCount > 0
                                        ? hs.totalAllocatedBytes / hs.allocationCount
                                        : 0;
    }

    // Convert to list and sort by total
    QList<AllocationHotspot> result = hotspots.values();
    std::sort(result.begin(), result.end(),
              [](const AllocationHotspot &a, const AllocationHotspot &b) {
                  return a.totalAllocatedBytes > b.totalAllocatedBytes;
              });

    if (topCount > 0 && result.size() > topCount) {
        result.resize(topCount);
    }

    return result;
}

QJsonObject MemoryAnalyzer::toJson() const {
    QJsonObject obj;
    obj["totalAllocated"] = static_cast<double>(m_totalAllocated);
    obj["totalFreed"] = static_cast<double>(m_totalFreed);
    obj["leakReport"] = m_leakReport.toJson();

    QJsonObject timelineObj;
    for (auto it = m_memoryTimeline.begin(); it != m_memoryTimeline.end(); ++it) {
        timelineObj[QString::number(it.key())] = static_cast<double>(it.value());
    }
    obj["memoryTimeline"] = timelineObj;

    return obj;
}

} // namespace RawrXD
