#pragma once
// Helper utilities for generated stub scaffolding. These functions provide a
// logging-enabled no-op body for MASM stub entry points so we can surface call
// counts and latency without altering existing linkage expectations.

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QTextStream>
#include <QtCore/QString>
#include <atomic>

namespace RawrXD {
namespace StubScaffold {

inline QMutex &logMutex()
{
    static QMutex mutex;
    return mutex;
}

inline bool loggingEnabled()
{
    static bool enabled = qEnvironmentVariableIntValue("RAWRXD_STUB_LOG") != 0;
    return enabled;
}

inline quint64 logAndReturnZero(const char *name)
{
    static std::atomic<quint64> callCounter{0};
    const quint64 callIndex = ++callCounter;

    if (!loggingEnabled()) {
        return 0;
    }

    QElapsedTimer timer;
    timer.start();

    QMutexLocker locker(&logMutex());
    QFile f("terminal_diagnostics.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        const QString tsStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        const qint64 elapsedMs = timer.elapsed();
        ts << tsStr << " [STUB-SCAFFOLD] " << name << " call#" << callIndex
           << " latency=" << elapsedMs << "ms" << "\n";
    }

    return 0;
}

} // namespace StubScaffold
} // namespace RawrXD

#define RAWRXD_DEFINE_STUB(fn) extern "C" __declspec(dllexport) quint64 fn() { return RawrXD::StubScaffold::logAndReturnZero(#fn); }
