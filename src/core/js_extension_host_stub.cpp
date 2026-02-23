// ============================================================================
// js_extension_host_stub.cpp — Headless JSExtensionHost (headless build variant) for RawrEngine
// ============================================================================
// RawrEngine has no QuickJS/VSCode extension stack. This provides minimal
// implementations so ssot_handlers_ext (handleVscExt*) link without the
// full js_extension_host + PolyfillEngine + vscode API chain.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "js_extension_host.hpp"
#include "model_memory_hotpatch.hpp"
#include <cstring>

// SCAFFOLD_190: Extension host and JS (if present)


// ============================================================================
// JSExtensionHost implementation (headless build variant)
// ============================================================================

JSExtensionHost& JSExtensionHost::instance() {
    static JSExtensionHost s_instance;
    return s_instance;
}

JSExtensionHost::JSExtensionHost()
    : m_jsRuntime(nullptr)
    , m_jsContext(nullptr)
    , m_nextTimerId(0)
    , m_initialized(false)
    , m_hostThread(nullptr)
    , m_running(false)
{
#ifdef _WIN32
    m_queueEvent = nullptr;
#endif
}

JSExtensionHost::~JSExtensionHost() {}

PatchResult JSExtensionHost::initialize() {
    m_initialized = false;
    return PatchResult::ok("Headless: extension host disabled");
}

PatchResult JSExtensionHost::shutdown() { return PatchResult::ok(); }
bool JSExtensionHost::isInitialized() const { return false; }

PatchResult JSExtensionHost::loadVSIX(const char*, const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::loadExtensionFromDir(const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::activateExtension(const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::deactivateExtension(const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::unloadExtension(const char*) { return PatchResult::ok(); }
bool JSExtensionHost::isJSExtension(const VSCodeExtensionManifest*) const { return false; }
PatchResult JSExtensionHost::fireEvent(const char*, const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::registerModuleResolver(const char*, const char*) { return PatchResult::ok(); }
PatchResult JSExtensionHost::executeScript(const char*, const char*, char*, size_t) { return PatchResult::ok(); }
uint64_t JSExtensionHost::createTimer(uint64_t, bool, void*) { return 0; }
void JSExtensionHost::cancelTimer(uint64_t) {}

JSExtensionHost::Stats JSExtensionHost::getStats() const { return {}; }

void JSExtensionHost::getLoadedExtensions(JSExtensionState* outStates, size_t maxStates, size_t* outCount) const {
    (void)outStates;
    (void)maxStates;
    *outCount = 0;
}

JSExtensionHost::VSIXVerificationResult JSExtensionHost::verifyVSIX(const char*) const {
    VSIXVerificationResult r = {};
    r.isValid = false;
    r.isSigned = false;
    r.signatureVerified = false;
    std::strncpy(r.errorDetail, "Headless stub", sizeof(r.errorDetail) - 1);
    return r;
}
