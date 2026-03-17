// masm_hotpatch_bridge.cpp
// Thin extern "C" bridge so MASM shims can drive the C++ UnifiedHotpatchManager.
// Exposes a minimal surface: initialize, enable/disable layers, optimize, and apply safety filters.

#include "unified_hotpatch_manager.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QDateTime>

namespace {
QMutex& bridgeMutex()
{
    static QMutex m;
    return m;
}

UnifiedHotpatchManager*& managerRef()
{
    static UnifiedHotpatchManager* mgr = nullptr;
    return mgr;
}

void logBridge(const QString& line)
{
    QFile f("terminal_diagnostics.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
           << " [MASM-BRIDGE] " << line << "\n";
    }
}

UnifiedHotpatchManager* ensureManager()
{
    QMutexLocker lock(&bridgeMutex());
    auto& mgr = managerRef();
    if (!mgr) {
        QObject* parent = QCoreApplication::instance();
        mgr = new UnifiedHotpatchManager(parent);
    }
    if (!mgr->isInitialized()) {
        UnifiedResult res = mgr->initialize();
        if (!res.success) {
            logBridge(QString("initialize failed: %1").arg(res.errorDetail));
            return nullptr;
        }
        logBridge("UnifiedHotpatchManager initialized");
    }
    return mgr;
}

int boolToInt(bool v) { return v ? 1 : 0; }
}

extern "C" __declspec(dllexport) int hotpatch_manager_init()
{
    return boolToInt(ensureManager() != nullptr);
}

extern "C" __declspec(dllexport) int hotpatch_manager_enable_layers(int enableMemory, int enableByte, int enableServer)
{
    UnifiedHotpatchManager* mgr = ensureManager();
    if (!mgr) return 0;
    mgr->setMemoryHotpatchEnabled(enableMemory != 0);
    mgr->setByteHotpatchEnabled(enableByte != 0);
    mgr->setServerHotpatchEnabled(enableServer != 0);
    logBridge(QString("layers set mem=%1 byte=%2 server=%3")
              .arg(enableMemory).arg(enableByte).arg(enableServer));
    return 1;
}

extern "C" __declspec(dllexport) int hotpatch_manager_optimize()
{
    UnifiedHotpatchManager* mgr = ensureManager();
    if (!mgr) return 0;
    QList<UnifiedResult> results = mgr->optimizeModel();
    bool ok = !results.isEmpty();
    for (const auto& r : results) {
        ok = ok && r.success;
        if (!r.success) {
            logBridge(QString("optimize fail layer=%1 op=%2 err=%3")
                      .arg((int)r.layer).arg(r.operationName).arg(r.errorDetail));
        }
    }
    return boolToInt(ok);
}

extern "C" __declspec(dllexport) int hotpatch_manager_apply_safety()
{
    UnifiedHotpatchManager* mgr = ensureManager();
    if (!mgr) return 0;
    QList<UnifiedResult> results = mgr->applySafetyFilters();
    bool ok = !results.isEmpty();
    for (const auto& r : results) {
        ok = ok && r.success;
        if (!r.success) {
            logBridge(QString("safety fail layer=%1 op=%2 err=%3")
                      .arg((int)r.layer).arg(r.operationName).arg(r.errorDetail));
        }
    }
    return boolToInt(ok);
}

extern "C" __declspec(dllexport) int hotpatch_manager_boost_speed()
{
    UnifiedHotpatchManager* mgr = ensureManager();
    if (!mgr) return 0;
    QList<UnifiedResult> results = mgr->boostInferenceSpeed();
    bool ok = !results.isEmpty();
    for (const auto& r : results) {
        ok = ok && r.success;
        if (!r.success) {
            logBridge(QString("speed fail layer=%1 op=%2 err=%3")
                      .arg((int)r.layer).arg(r.operationName).arg(r.errorDetail));
        }
    }
    return boolToInt(ok);
}
