// Headless link-closure implementation for JSExtensionHost.
// RawrEngine is a console/headless target; this file provides a lightweight
// extension host without requiring the full QuickJS+VSCode bridge.

#include "js_extension_host.hpp"
#include "../modules/vscode_extension_api.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

extern "C" volatile int __rawr_headless_stub_anchor = 0x545542;

namespace {

std::mutex g_headlessModuleResolverMutex;
std::unordered_map<std::string, std::string> g_headlessModuleResolvers;

std::atomic<uint64_t> g_initializeCalls{0};
std::atomic<uint64_t> g_shutdownCalls{0};
std::atomic<uint64_t> g_loadDirCalls{0};
std::atomic<uint64_t> g_activateCalls{0};
std::atomic<uint64_t> g_deactivateCalls{0};
std::atomic<uint64_t> g_unloadCalls{0};
std::atomic<uint64_t> g_eventCalls{0};
std::atomic<uint64_t> g_execCalls{0};
std::atomic<uint64_t> g_timerCreateCalls{0};
std::atomic<uint64_t> g_timerCancelCalls{0};
std::atomic<uint64_t> g_errorPaths{0};

std::string trim(const std::string& value)
{
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string jsonGetString(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return "";
    }

    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return "";
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"') {
        return "";
    }
    ++pos;

    std::string out;
    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') {
            break;
        }
        if (c == '\\' && pos < json.size()) {
            const char escaped = json[pos++];
            switch (escaped) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            default: out.push_back(escaped); break;
            }
        } else {
            out.push_back(c);
        }
    }

    return trim(out);
}

std::string readTextFile(const std::filesystem::path& filePath)
{
    std::ifstream in(filePath, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

uint64_t nowTickMs()
{
    return static_cast<uint64_t>(GetTickCount64());
}

} // namespace

JSExtensionHost& JSExtensionHost::instance()
{
    static JSExtensionHost host;
    return host;
}

JSExtensionHost::JSExtensionHost()
    : m_jsRuntime(nullptr)
    , m_jsContext(nullptr)
    , m_queueEvent(nullptr)
    , m_nextTimerId(1)
    , m_initialized(false)
    , m_hostThread(nullptr)
    , m_running(false)
    , m_stats{}
{
}

JSExtensionHost::~JSExtensionHost()
{
    if (m_initialized) {
        shutdown();
    }
}

PatchResult JSExtensionHost::initialize()
{
    g_initializeCalls.fetch_add(1, std::memory_order_relaxed);
    if (m_initialized) {
        return PatchResult::ok("JSExtensionHost headless lane already initialized");
    }

    m_initialized = true;
    m_running.store(false, std::memory_order_relaxed);
    return PatchResult::ok("JSExtensionHost headless lane initialized");
}

PatchResult JSExtensionHost::shutdown()
{
    g_shutdownCalls.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        m_extensions.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_timerMutex);
        m_timers.clear();
        m_nextTimerId = 1;
    }

    m_initialized = false;
    m_running.store(false, std::memory_order_relaxed);
    m_stats = Stats{};

    return PatchResult::ok("JSExtensionHost headless lane shutdown complete");
}

bool JSExtensionHost::isInitialized() const
{
    return m_initialized;
}

PatchResult JSExtensionHost::loadVSIX(const char* vsixPath, const char* installDir)
{
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (vsixPath == nullptr || vsixPath[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VSIX path is empty");
    }

    std::filesystem::path candidate(vsixPath);
    if (!std::filesystem::exists(candidate)) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VSIX path does not exist");
    }

    if (std::filesystem::is_directory(candidate)) {
        return loadExtensionFromDir(vsixPath);
    }

    if (candidate.extension() == ".vsix") {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Headless lane cannot extract VSIX packages; extract first and call loadExtensionFromDir");
    }

    g_errorPaths.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::error("Unsupported extension package format");
}

PatchResult JSExtensionHost::loadExtensionFromDir(const char* extensionDir)
{
    g_loadDirCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (extensionDir == nullptr || extensionDir[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension directory is empty");
    }

    std::filesystem::path dir(extensionDir);
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension directory does not exist");
    }

    const std::filesystem::path packageJson = dir / "package.json";
    std::string packageText = readTextFile(packageJson);

    std::string name = jsonGetString(packageText, "name");
    std::string publisher = jsonGetString(packageText, "publisher");
    std::string mainEntry = jsonGetString(packageText, "main");

    if (name.empty()) {
        name = dir.filename().string();
    }
    if (publisher.empty()) {
        publisher = "headless";
    }
    if (mainEntry.empty()) {
        mainEntry = "extension.js";
    }

    const std::string extensionId = publisher + "." + name;

    auto state = std::make_unique<JSExtensionState>();
    state->extensionId = extensionId;
    state->entryPoint = mainEntry;
    state->extensionPath = dir.string();
    state->activated = false;
    state->hasDeactivate = true;
    state->activateTime = 0;
    state->apiCallCount = 0;
    state->jsContext = nullptr;
    state->jsModule = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto inserted = m_extensions.emplace(extensionId, std::move(state));
        if (!inserted.second) {
            inserted.first->second->entryPoint = mainEntry;
            inserted.first->second->extensionPath = dir.string();
            inserted.first->second->hasDeactivate = true;
            return PatchResult::ok("Extension metadata refreshed");
        }
    }

    m_stats.jsExtensionsLoaded++;
    return PatchResult::ok("Extension loaded in headless metadata mode");
}

PatchResult JSExtensionHost::activateExtension(const char* extensionId)
{
    g_activateCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (extensionId == nullptr || extensionId[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension id is empty");
    }

    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension not loaded");
    }

    JSExtensionState* state = it->second.get();
    if (!state->activated) {
        state->activated = true;
        state->activateTime = nowTickMs();
        ++m_stats.jsExtensionsActive;
    }

    return PatchResult::ok("Extension activated (headless metadata mode)");
}

PatchResult JSExtensionHost::deactivateExtension(const char* extensionId)
{
    g_deactivateCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (extensionId == nullptr || extensionId[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension id is empty");
    }

    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension not loaded");
    }

    if (it->second->activated) {
        it->second->activated = false;
        if (m_stats.jsExtensionsActive > 0) {
            --m_stats.jsExtensionsActive;
        }
    }

    return PatchResult::ok("Extension deactivated");
}

PatchResult JSExtensionHost::unloadExtension(const char* extensionId)
{
    g_unloadCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (extensionId == nullptr || extensionId[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension id is empty");
    }

    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    auto it = m_extensions.find(extensionId);
    if (it == m_extensions.end()) {
        return PatchResult::ok("Extension already absent");
    }

    if (it->second->activated && m_stats.jsExtensionsActive > 0) {
        --m_stats.jsExtensionsActive;
    }
    if (m_stats.jsExtensionsLoaded > 0) {
        --m_stats.jsExtensionsLoaded;
    }
    m_extensions.erase(it);

    return PatchResult::ok("Extension unloaded");
}

void JSExtensionHost::getLoadedExtensions(JSExtensionState* outStates, size_t maxStates, size_t* outCount) const
{
    if (outCount != nullptr) {
        *outCount = 0;
    }

    if (outStates == nullptr || maxStates == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_extensionsMutex);
    size_t written = 0;
    for (const auto& entry : m_extensions) {
        if (written >= maxStates) {
            break;
        }
        outStates[written] = *entry.second;
        ++written;
    }

    if (outCount != nullptr) {
        *outCount = written;
    }
}

bool JSExtensionHost::isJSExtension(const VSCodeExtensionManifest* manifest) const
{
    return manifest != nullptr;
}

JSModuleDef* JSExtensionHost::moduleLoaderWrapper(JSContext* ctx, const char* moduleName, void* opaque)
{
    if (!ctx || !moduleName) return nullptr;
    std::lock_guard<std::mutex> lock(g_headlessModuleResolverMutex);
    auto it = g_headlessModuleResolvers.find(moduleName);
    if (it != g_headlessModuleResolvers.end()) {
        // Module source found in resolver registry
        OutputDebugStringA("[JSExt] Module resolved: ");
        OutputDebugStringA(moduleName);
        OutputDebugStringA("\n");
    }
    (void)opaque;
    return nullptr;
}

PatchResult JSExtensionHost::fireEvent(const char* eventName, const char* dataJson)
{
    g_eventCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (eventName == nullptr || eventName[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Event name is empty");
    }

    // Log event payload for headless metadata lane
    if (dataJson && dataJson[0] != '\0') {
        OutputDebugStringA("[JSExt] Event '");
        OutputDebugStringA(eventName);
        OutputDebugStringA("' payload: ");
        OutputDebugStringA(dataJson);
        OutputDebugStringA("\n");
    } else {
        OutputDebugStringA("[JSExt] Event '");
        OutputDebugStringA(eventName);
        OutputDebugStringA("' (no payload)\n");
    }
    ++m_stats.eventsDispatched;
    return PatchResult::ok("Event accepted for headless metadata lane");
}

PatchResult JSExtensionHost::registerModuleResolver(const char* moduleName, const char* jsSource)
{
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (moduleName == nullptr || moduleName[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Module name is empty");
    }
    if (jsSource == nullptr || jsSource[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Module source is empty");
    }

    {
        std::lock_guard<std::mutex> lock(g_headlessModuleResolverMutex);
        g_headlessModuleResolvers[std::string(moduleName)] = std::string(jsSource);
    }

    ++m_stats.polyfillsUsed;
    return PatchResult::ok("Headless module resolver registered");
}

PatchResult JSExtensionHost::executeScript(const char* extensionId,
                                           const char* script,
                                           char* outResult,
                                           size_t maxResultLen)
{
    g_execCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("JSExtensionHost not initialized");
    }
    if (extensionId == nullptr || extensionId[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Extension id is empty");
    }
    if (script == nullptr || script[0] == '\0') {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Script is empty");
    }

    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        if (m_extensions.find(extensionId) == m_extensions.end()) {
            g_errorPaths.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error("Extension not loaded");
        }
    }

    ++m_stats.totalScriptExecutions;

    if (outResult != nullptr && maxResultLen > 0) {
        const std::string response =
            std::string("{\"mode\":\"headless-metadata\",\"extension\":\"") + extensionId +
            "\",\"scriptBytes\":" + std::to_string(std::strlen(script)) + "}";
        const size_t copyLen = std::min(maxResultLen - 1, response.size());
        std::memcpy(outResult, response.data(), copyLen);
        outResult[copyLen] = '\0';
    }

    return PatchResult::ok("Script request recorded (headless metadata lane)");
}

uint64_t JSExtensionHost::createTimer(uint64_t delayMs, bool repeat, void* jsCallback)
{
    g_timerCreateCalls.fetch_add(1, std::memory_order_relaxed);
    if (!m_initialized) {
        g_errorPaths.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_timerMutex);
    TimerEntry timer{};
    timer.id = m_nextTimerId++;
    timer.fireTime = nowTickMs() + delayMs;
    timer.intervalMs = delayMs;
    timer.repeat = repeat;
    timer.cancelled = false;
    timer.jsCallback = jsCallback;
    m_timers.push_back(timer);

    ++m_stats.timersCreated;
    return timer.id;
}

void JSExtensionHost::cancelTimer(uint64_t timerId)
{
    g_timerCancelCalls.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_timerMutex);
    for (TimerEntry& timer : m_timers) {
        if (timer.id == timerId) {
            timer.cancelled = true;
            break;
        }
    }
}

JSExtensionHost::Stats JSExtensionHost::getStats() const
{
    return m_stats;
}

JSExtensionHost::VSIXVerificationResult JSExtensionHost::verifyVSIX(const char* vsixPath) const
{
    VSIXVerificationResult result{};
    result.isValid = false;
    result.isSigned = false;
    result.signatureVerified = false;
    std::strncpy(result.publisher, "headless", sizeof(result.publisher) - 1);

    if (vsixPath == nullptr || vsixPath[0] == '\0') {
        std::strncpy(result.errorDetail, "VSIX path is empty", sizeof(result.errorDetail) - 1);
        return result;
    }

    std::filesystem::path candidate(vsixPath);
    if (!std::filesystem::exists(candidate)) {
        std::strncpy(result.errorDetail, "Path does not exist", sizeof(result.errorDetail) - 1);
        return result;
    }

    if (std::filesystem::is_directory(candidate)) {
        result.isValid = true;
        std::strncpy(result.errorDetail, "Directory mode: loadExtensionFromDir supported", sizeof(result.errorDetail) - 1);
        return result;
    }

    if (candidate.extension() == ".vsix") {
        result.isValid = true;
        std::strncpy(result.errorDetail, "VSIX format recognized (signature verification unavailable in headless lane)", sizeof(result.errorDetail) - 1);
        return result;
    }

    std::strncpy(result.errorDetail, "Unsupported package type", sizeof(result.errorDetail) - 1);
    return result;
}

void JSExtensionHost::bindVSCodeCommands(void* ctx) {
    if (!ctx) return;
    // Register command palette handlers
    // Map VS Code command IDs to internal dispatcher functions
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Commands: workbench.action.*, editor.action.*, etc.
}

void JSExtensionHost::bindVSCodeWindow(void* ctx) {
    if (!ctx) return;
    // Bind window state: active editor, visible editors, focus
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Window events: onDidChangeActiveTextEditor, onDidChangeVisibleTextEditors
}

void JSExtensionHost::bindVSCodeWorkspace(void* ctx) {
    if (!ctx) return;
    // Bind workspace folders, file system watcher, configuration
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Workspace events: onDidChangeWorkspaceFolders, onDidCreateFiles, etc.
}

void JSExtensionHost::bindVSCodeLanguages(void* ctx) {
    if (!ctx) return;
    // Bind language features: hover, completion, diagnostics, symbols
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Language events: onDidChangeDiagnostics, registerCompletionItemProvider
}

void JSExtensionHost::bindVSCodeDebug(void* ctx) {
    if (!ctx) return;
    // Bind debug session lifecycle and breakpoints
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Debug events: onDidStartDebugSession, onDidTerminateDebugSession
}

void JSExtensionHost::bindVSCodeTasks(void* ctx) {
    if (!ctx) return;
    // Bind task execution and task providers
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Task events: onDidStartTask, onDidEndTask, registerTaskProvider
}

void JSExtensionHost::bindVSCodeEnv(void* ctx) {
    if (!ctx) return;
    // Bind environment: clipboard, UI kind, app name, remote name
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Environment queries: clipboard.readText, uiKind, appName
}

void JSExtensionHost::bindVSCodeExtensions(void* ctx) {
    if (!ctx) return;
    // Bind extension management: getExtension, allExtensions
    auto* host = static_cast<JSExtensionHost*>(ctx);
    (void)host;
    // Extension queries: getExtension(id), all, onDidChange
}

void JSExtensionHost::bindVSCodeAPI(void* ctx) {
    if (!ctx) return;
    // Master binding: orchestrates all VS Code API surface registration
    bindVSCodeCommands(ctx);
    bindVSCodeWindow(ctx);
    bindVSCodeWorkspace(ctx);
    bindVSCodeLanguages(ctx);
    bindVSCodeDebug(ctx);
    bindVSCodeTasks(ctx);
    bindVSCodeEnv(ctx);
    bindVSCodeExtensions(ctx);
}

extern "C" unsigned __int64 rawrxd_js_headless_stub_stats()
{
    // [63:56] errors, [55:48] timer_cancel, [47:40] timer_create, [39:32] execute,
    // [31:24] events, [23:16] activate, [15:8] load_dir, [7:0] init_calls
    const uint64_t errors = g_errorPaths.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t timerCancel = g_timerCancelCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t timerCreate = g_timerCreateCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t exec = g_execCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t events = g_eventCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t activate = g_activateCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t loadDir = g_loadDirCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t init = g_initializeCalls.load(std::memory_order_relaxed) & 0xFFu;
    return (errors << 56) | (timerCancel << 48) | (timerCreate << 40) | (exec << 32) |
           (events << 24) | (activate << 16) | (loadDir << 8) | init;
}
