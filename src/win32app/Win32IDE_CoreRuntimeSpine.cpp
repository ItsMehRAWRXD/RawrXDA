#include "Win32IDE.h"
#include "Win32IDE_ComponentManagers.h"
#include "../../include/plugin_signature.h"

bool Win32IDE::initializeCoreRuntimeSpine()
{
    if (m_coreRuntimeSpineInitialized)
        return true;

    // Deterministic order: signature policy gate -> persistence -> export.
    // Each init is designed to be idempotent.
    initPluginSignatureVerifier();
    initSQLite3Core();
    initTelemetryExport();

    m_coreRuntimeSpineInitialized = true;
    return true;
}

void Win32IDE::shutdownCoreRuntimeSpine()
{
    if (!m_coreRuntimeSpineInitialized)
        return;

    // Shutdown in reverse order.
    shutdownTelemetryExport();
    shutdownSQLite3Core();

    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (verifier.isInitialized())
        verifier.shutdown();

    m_coreRuntimeSpineInitialized = false;
}
