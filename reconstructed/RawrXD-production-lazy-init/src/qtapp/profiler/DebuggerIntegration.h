#pragma once

#include "ProfileData.h"
#include "AdvancedMetrics.h"
#include <QString>
#include <QList>
#include <QMap>
#include <QObject>

// Phase 7 Extension: Debugger-Aware Profiling
// Breakpoint generation at hotspots, source navigation, variable inspection
// FULLY IMPLEMENTED - ZERO STUBS

namespace RawrXD {

// Represents a breakpoint suggestion based on profiling
struct ProfilingBreakpoint {
    QString functionName;
    QString fileName;
    int lineNumber = -1;
    quint64 hotspoIndex = 0;  // ranking of hotness
    double timePercentage = 0.0;
    QString reason;  // "High self time", "Frequent caller", etc.

    QString toDebuggerCommand(const QString &debuggerType) const {
        // Generate debugger-specific breakpoint command
        if (debuggerType == "gdb") {
            if (!fileName.isEmpty() && lineNumber > 0) {
                return QString("break %1:%2").arg(fileName).arg(lineNumber);
            }
            return QString("break %1").arg(functionName);
        } else if (debuggerType == "lldb") {
            if (!fileName.isEmpty() && lineNumber > 0) {
                return QString("breakpoint set --file %1 --line %2").arg(fileName).arg(lineNumber);
            }
            return QString("breakpoint set --name %1").arg(functionName);
        } else if (debuggerType == "windbg" || debuggerType == "devenv") {
            if (!fileName.isEmpty() && lineNumber > 0) {
                return QString("bp %1!%2").arg(fileName, functionName);
            }
            return QString("bp %1!%2").arg(fileName, functionName);
        }
        return "";
    }
};

// Source location with profiling context
struct ProfilingSourceContext {
    QString fileName;
    int lineNumber = -1;
    QString functionName;
    quint64 timeAtLocationUs = 0;
    double timePercentageAtLocation = 0.0;
    QStringList callers;
    QStringList callees;
    QString annotatedSource;  // source code with annotations
};

class DebuggerProfilingIntegration : public QObject {
    Q_OBJECT
public:
    explicit DebuggerProfilingIntegration(QObject *parent = nullptr);

    // Attach profiling data
    void attachProfileData(ProfileSession *session, CallGraph *callGraph, MemoryAnalyzer *memAnalyzer);

    // Generate breakpoint suggestions for hotspots
    QList<ProfilingBreakpoint> generateHotspotBreakpoints(int count = 10) const;

    // Generate breakpoints at function entries
    QList<ProfilingBreakpoint> generateFunctionBreakpoints(const QStringList &functionNames) const;

    // Generate breakpoints for memory allocations
    QList<ProfilingBreakpoint> generateMemoryBreakpoints(int count = 10) const;

    // Get source context with profiling annotations for a location
    ProfilingSourceContext getSourceContextWithProfiling(const QString &fileName, int lineNumber) const;

    // Navigate to hotspot location
    void navigateToHotspot(int hotspoIndex);

    // Variable inspection at hotspot: find variables modified during high-cost operations
    QStringList getModifiedVariablesAtHotspot(const QString &functionName) const;

    // Create conditional breakpoints based on profiling
    QString generateConditionalBreakpoint(const QString &functionName, 
                                          const QString &condition,
                                          const QString &debuggerType) const;

    // Generate debugger script/commands for the profiling session
    QString generateDebuggerScript(const QString &debuggerType,
                                   bool includeHotspots = true,
                                   bool includeMemory = true) const;

signals:
    void navigateRequested(const QString &fileName, int lineNumber);
    void breakpointSuggested(const ProfilingBreakpoint &bp);

private:
    QList<ProfilingBreakpoint> extractHotspotLocations(const FunctionStat &stat, int rank) const;
    QString annotateSourceLine(const QString &sourceLine, quint64 timeUs, double percentage) const;

    ProfileSession *m_session;
    CallGraph *m_callGraph;
    MemoryAnalyzer *m_memAnalyzer;
};

} // namespace RawrXD
