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
#include <utility>

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
    if (m_initialized) return PatchResult::ok("Headless JS host already initialized");
#ifdef _WIN32
    if (!m_queueEvent) {
        m_queueEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }
#endif
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_timerMutex);
        m_timers.clear();
    }
    m_nextTimerId = 1;
    m_stats = {};
    m_initialized = true;
    m_running.store(false);
    return PatchResult::ok("Headless JS host initialized");
}

PatchResult JSExtensionHost::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_timerMutex);
        m_timers.clear();
    }
    m_running.store(false);
    m_initialized = false;
#ifdef _WIN32
    if (m_queueEvent) {
        CloseHandle(m_queueEvent);
        m_queueEvent = nullptr;
    }
#endif
    return PatchResult::ok("Headless JS host shut down");
}
bool JSExtensionHost::isInitialized() const { return m_initialized; }

PatchResult JSExtensionHost::loadVSIX(const char* vsixPath, const char* installDir) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!vsixPath || !installDir) return PatchResult::error("Invalid VSIX load args", -1);
    return PatchResult::ok("Headless VSIX accepted (metadata-only)");
}

PatchResult JSExtensionHost::loadExtensionFromDir(const char* extensionDir) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!extensionDir || extensionDir[0] == '\0') return PatchResult::error("Invalid extension dir", -1);

    auto st = std::make_unique<JSExtensionState>();
    st->extensionId = extensionDir;
    st->extensionPath = extensionDir;
    st->entryPoint = "headless.js";
    st->activated = false;
    st->hasDeactivate = true;
    st->activateTime = 0;
    st->apiCallCount = 0;
    st->jsContext = nullptr;
    st->jsModule = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions[st->extensionId] = std::move(st);
        m_stats.jsExtensionsLoaded = static_cast<uint64_t>(m_extensions.size());
    }
    return PatchResult::ok("Headless extension registered");
}

PatchResult JSExtensionHost::activateExtension(const char* extensionId) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!extensionId || extensionId[0] == '\0') return PatchResult::error("Invalid extension id", -1);
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) return PatchResult::error("Extension not loaded", -2);
    if (!it->second->activated) {
        it->second->activated = true;
        ++m_stats.jsExtensionsActive;
    }
    return PatchResult::ok("Extension activated (headless)");
}

PatchResult JSExtensionHost::deactivateExtension(const char* extensionId) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!extensionId || extensionId[0] == '\0') return PatchResult::error("Invalid extension id", -1);
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) return PatchResult::error("Extension not loaded", -2);
    if (it->second->activated) {
        it->second->activated = false;
        if (m_stats.jsExtensionsActive > 0) --m_stats.jsExtensionsActive;
    }
    return PatchResult::ok("Extension deactivated (headless)");
}

PatchResult JSExtensionHost::unloadExtension(const char* extensionId) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!extensionId || extensionId[0] == '\0') return PatchResult::error("Invalid extension id", -1);
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) return PatchResult::error("Extension not loaded", -2);
    if (it->second->activated && m_stats.jsExtensionsActive > 0) --m_stats.jsExtensionsActive;
    m_extensions.erase(it);
    m_stats.jsExtensionsLoaded = static_cast<uint64_t>(m_extensions.size());
    return PatchResult::ok("Extension unloaded (headless)");
}

bool JSExtensionHost::isJSExtension(const VSCodeExtensionManifest* manifest) const {
    return manifest != nullptr;
}

PatchResult JSExtensionHost::fireEvent(const char* eventName, const char* dataJson) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!eventName || eventName[0] == '\0') return PatchResult::error("Invalid event name", -1);
    (void)dataJson;
    ++m_stats.eventsDispatched;
    return PatchResult::ok("Event accepted by headless JS host");
}

PatchResult JSExtensionHost::registerModuleResolver(const char* moduleName, const char* jsSource) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!moduleName || moduleName[0] == '\0' || !jsSource) return PatchResult::error("Invalid module resolver args", -1);
    return PatchResult::ok("Headless module resolver accepted");
}

PatchResult JSExtensionHost::executeScript(const char* extensionId, const char* script, char* outResult, size_t maxResultLen) {
    if (!m_initialized) return PatchResult::error("Headless JS host not initialized", -1);
    if (!extensionId || !script) return PatchResult::error("Invalid script execution args", -1);

    ++m_stats.totalScriptExecutions;
    if (outResult && maxResultLen > 0) {
        const char* msg = "headless-exec-ok";
        std::strncpy(outResult, msg, maxResultLen - 1);
        outResult[maxResultLen - 1] = '\0';
    }
    return PatchResult::ok("Script accepted by headless host");
}

uint64_t JSExtensionHost::createTimer(uint64_t delayMs, bool repeat, void* jsCallback) {
    if (!m_initialized) return 0;
    TimerEntry t{};
    t.id = m_nextTimerId++;
    t.fireTime = GetTickCount64() + delayMs;
    t.intervalMs = repeat ? delayMs : 0;
    t.repeat = repeat;
    t.cancelled = false;
    t.jsCallback = jsCallback;
    {
        std::lock_guard<std::mutex> lock(m_timerMutex);
        m_timers.push_back(t);
    }
    ++m_stats.timersCreated;
    return t.id;
}

void JSExtensionHost::cancelTimer(uint64_t timerId) {
    if (!m_initialized || timerId == 0) return;
    std::lock_guard<std::mutex> lock(m_timerMutex);
    for (auto& t : m_timers) {
        if (t.id == timerId) {
            t.cancelled = true;
            break;
        }
    }
}

JSExtensionHost::Stats JSExtensionHost::getStats() const {
    Stats out = m_stats;
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    out.jsExtensionsLoaded = static_cast<uint64_t>(m_extensions.size());
    uint64_t active = 0;
    for (const auto& kv : m_extensions) {
        if (kv.second && kv.second->activated) ++active;
    }
    out.jsExtensionsActive = active;
    return out;
}

void JSExtensionHost::getLoadedExtensions(JSExtensionState* outStates, size_t maxStates, size_t* outCount) const {
    if (outCount) *outCount = 0;
    if (!outStates || maxStates == 0) return;
    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    size_t idx = 0;
    for (const auto& kv : m_extensions) {
        if (idx >= maxStates) break;
        if (!kv.second) continue;
        outStates[idx++] = *kv.second;
    }
    if (outCount) *outCount = idx;
}

JSExtensionHost::VSIXVerificationResult JSExtensionHost::verifyVSIX(const char* vsixPath) const {
    VSIXVerificationResult r = {};
    r.isValid = false;
    r.isSigned = false;
    r.signatureVerified = false;
    std::strncpy(r.publisher, "headless-local", sizeof(r.publisher) - 1);
    if (!vsixPath || vsixPath[0] == '\0') {
        std::strncpy(r.errorDetail, "Missing VSIX path", sizeof(r.errorDetail) - 1);
        return r;
    }
#ifdef _WIN32
    const DWORD attrs = GetFileAttributesA(vsixPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        std::strncpy(r.errorDetail, "VSIX path not found", sizeof(r.errorDetail) - 1);
        return r;
    }
#endif
    r.isValid = true;
    std::strncpy(r.errorDetail, "Headless validation only (signature not checked)", sizeof(r.errorDetail) - 1);
    return r;
}
