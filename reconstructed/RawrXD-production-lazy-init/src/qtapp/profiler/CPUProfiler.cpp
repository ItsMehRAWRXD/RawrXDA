#include "CPUProfiler.h"
#include <QDebug>
#include "logging/structured_logger.h"

using namespace RawrXD;

CPUProfiler::CPUProfiler(QObject *parent)
    : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &CPUProfiler::collectSample);
}

CPUProfiler::~CPUProfiler() {
    stopProfiling();
}

void CPUProfiler::startProfiling(const QString &processName, SamplingRate rate) {
    if (m_isProfiling) return;
    m_session = new ProfileSession(this);
    m_session->setProcessName(processName);
    setSamplingRate(rate);
    m_totalSamples = 0;
    m_isProfiling = true;
    m_latencyTimer.start();
#ifdef _WIN32
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif
    emit profilingStarted();
    m_timer.start(1000 / m_rateHz);
}

void CPUProfiler::stopProfiling() {
    if (!m_isProfiling) return;
    m_timer.stop();
#ifdef _WIN32
    SymCleanup(GetCurrentProcess());
#endif
    m_isProfiling = false;
    emit profilingStopped();
}

void CPUProfiler::setSamplingRate(SamplingRate rate) {
    m_rate = rate;
    m_rateHz = static_cast<int>(rate);
    if (m_isProfiling) {
        m_timer.start(1000 / m_rateHz);
    }
}

void CPUProfiler::setSamplingRate(int rateHz) {
    m_rateHz = qMax(1, rateHz);
    if (m_isProfiling) {
        m_timer.start(1000 / m_rateHz);
    }
}

void CPUProfiler::startProfilingHz(const QString &processName, int rateHz) {
    if (m_isProfiling) return;
    m_session = new ProfileSession(this);
    m_session->setProcessName(processName);
    setSamplingRate(rateHz);
    m_totalSamples = 0;
    m_isProfiling = true;
    m_latencyTimer.start();
#ifdef _WIN32
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif
    emit profilingStarted();
    m_timer.start(1000 / m_rateHz);
}

void CPUProfiler::collectSample() {
    if (!m_isProfiling || !m_session) return;
    QMutexLocker locker(&m_mutex);

    QList<StackFrame> frames = unwindCurrentStack();
    if (frames.isEmpty()) return;

    // Limit depth
    if (frames.size() > m_maxStackDepth) frames.resize(m_maxStackDepth);

    // Calculate times (assign nominal 1/rate per sample per frame to inclusive time)
    quint64 sampleFrameTimeUs = 1000000ULL / static_cast<quint64>(m_rateHz);
    for (auto &f : frames) {
        f.timeSpentUs += sampleFrameTimeUs;
        f.selfTimeUs = f.timeSpentUs; // adjusted later in aggregation
        f.callCount += 1;
    }

    // Apply filters
    frames = applyFilters(frames);

    CallStack stack;
    stack.frames = frames;
    stack.timestampUs = m_session->totalRuntimeUs();
    stack.memoryUsageBytes = currentMemoryUsageBytes();

    m_session->addCallStack(stack);
    ++m_totalSamples;
    emit sampleCollected(stack);
    emit profileUpdated();

    // Structured logging: sample latency
    qint64 latencyUs = m_latencyTimer.nsecsElapsed() / 1000;
    qDebug() << "CPUProfiler sample latency(us)=" << latencyUs << " totalSamples=" << m_totalSamples;
    RawrXD::StructuredLogger::instance().recordMetric("profiler.phase7.sample.latency_us", static_cast<double>(latencyUs));
    RawrXD::StructuredLogger::instance().incrementCounter("profiler.phase7.sample.count");
    m_latencyTimer.restart();
}

QList<StackFrame> CPUProfiler::unwindCurrentStack() {
#ifdef _WIN32
    return unwindWindowsStack();
#elif defined(__linux__)
    return unwindPOSIXStack();
#else
    QList<StackFrame> frames;
    StackFrame f;
    f.functionName = QStringLiteral("unknown_function");
    f.fileName = QStringLiteral("unknown_file");
    f.lineNumber = -1;
    frames.append(f);
    return frames;
#endif
}

#ifdef _WIN32
QList<StackFrame> CPUProfiler::unwindWindowsStack() {
    QList<StackFrame> frames;

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();

    CONTEXT context = {0};
    RtlCaptureContext(&context);

#if defined(_M_X64)
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#endif

    while (StackWalk64(machineType, hProcess, hThread, &frame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        DWORD64 addr = frame.AddrPC.Offset;
        char buffer[sizeof(SYMBOL_INFO) + 256];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;

        StackFrame f;
        if (SymFromAddr(hProcess, addr, 0, symbol)) {
            f.functionName = QString::fromLatin1(symbol->Name);
        } else {
            f.functionName = QStringLiteral("0x%1").arg(QString::number(addr, 16));
        }

        IMAGEHLP_LINE64 line;
        DWORD displacement = 0;
        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        if (SymGetLineFromAddr64(hProcess, addr, &displacement, &line)) {
            f.fileName = QString::fromLatin1(line.FileName);
            f.lineNumber = static_cast<int>(line.LineNumber);
        } else {
            f.fileName = QStringLiteral("<unknown>");
            f.lineNumber = -1;
        }
        frames.append(f);
    }
    return frames;
}
#endif

#ifdef __linux__
QList<StackFrame> CPUProfiler::unwindPOSIXStack() {
    QList<StackFrame> frames;
    const int maxFrames = 256;
    void *addrs[maxFrames];
    int num = ::backtrace(addrs, maxFrames);
    char **syms = ::backtrace_symbols(addrs, num);

    for (int i = 0; i < num; ++i) {
        Dl_info info;
        memset(&info, 0, sizeof(info));
        dladdr(addrs[i], &info);
        StackFrame f;
        if (info.dli_sname) {
            f.functionName = QString::fromLatin1(info.dli_sname);
        } else if (syms && syms[i]) {
            f.functionName = QString::fromLatin1(syms[i]);
        } else {
            f.functionName = QStringLiteral("frame_%1").arg(i);
        }
        if (info.dli_fname) f.fileName = QString::fromLatin1(info.dli_fname);
        frames.append(f);
    }
    if (syms) free(syms);
    return frames;
}
#endif

QList<StackFrame> CPUProfiler::applyFilters(const QList<StackFrame> &stack) const {
    if (m_filterPatterns.isEmpty()) return stack;
    QList<StackFrame> out;
    for (const auto &f : stack) {
        bool match = true;
        for (const auto &pat : m_filterPatterns) {
            QRegularExpression re(pat);
            if (re.isValid() && f.functionName.contains(re)) {
                match = false;
                break;
            }
        }
        if (match) out.append(f);
    }
    return out;
}

quint64 CPUProfiler::currentMemoryUsageBytes() const {
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
