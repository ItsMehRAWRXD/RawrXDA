// Headless link-closure stubs for JSExtensionHost.
// RawrEngine is a console/headless target; the full QuickJS+VSCode bridge is wired in Win32IDE lanes.
// These stubs preserve SSOT handler link closure without pulling in Win32IDE dependencies.

#include "js_extension_host.hpp"

// Anchor symbol to prevent certain toolchains/link modes from treating this TU as discardable.
// This also gives a stable "/include:" hook if needed from consuming targets.
extern "C" volatile int __rawr_headless_stub_anchor = 0x545542;  // "TUB" (stub)

JSExtensionHost& JSExtensionHost::instance()
{
    static JSExtensionHost host;
    return host;
}

JSExtensionHost::JSExtensionHost() : m_jsRuntime(nullptr), m_jsContext(nullptr), m_initialized(false) {}

JSExtensionHost::~JSExtensionHost() = default;

PatchResult JSExtensionHost::initialize()
{
    m_initialized = true;
    return PatchResult::ok("JSExtensionHost stub initialized");
}

PatchResult JSExtensionHost::shutdown()
{
    m_initialized = false;
    return PatchResult::ok("JSExtensionHost stub shutdown");
}

bool JSExtensionHost::isInitialized() const
{
    return m_initialized;
}

PatchResult JSExtensionHost::activateExtension(const char* extensionId)
{
    (void)extensionId;
    return PatchResult::ok("activateExtension (stub)");
}

PatchResult JSExtensionHost::deactivateExtension(const char* extensionId)
{
    (void)extensionId;
    return PatchResult::ok("deactivateExtension (stub)");
}

void JSExtensionHost::getLoadedExtensions(JSExtensionState* outStates, size_t maxStates, size_t* outCount) const
{
    (void)outStates;
    (void)maxStates;
    if (outCount)
        *outCount = 0;
}

PatchResult JSExtensionHost::loadVSIX(const char* vsixPath, const char* installDir)
{
    (void)vsixPath;
    (void)installDir;
    return PatchResult::error("VSIX loading not available in RawrEngine headless lane");
}

PatchResult JSExtensionHost::loadExtensionFromDir(const char* extensionDir)
{
    (void)extensionDir;
    return PatchResult::error("Extension loading not available in RawrEngine headless lane");
}

PatchResult JSExtensionHost::unloadExtension(const char* extensionId)
{
    (void)extensionId;
    return PatchResult::ok("unloadExtension (stub)");
}

bool JSExtensionHost::isJSExtension(const VSCodeExtensionManifest* manifest) const
{
    (void)manifest;
    return false;
}

JSModuleDef* JSExtensionHost::moduleLoaderWrapper(JSContext* ctx, const char* moduleName, void* opaque)
{
    (void)ctx;
    (void)moduleName;
    (void)opaque;
    return nullptr;
}

PatchResult JSExtensionHost::fireEvent(const char* eventName, const char* dataJson)
{
    (void)eventName;
    (void)dataJson;
    return PatchResult::ok("fireEvent (stub)");
}

PatchResult JSExtensionHost::registerModuleResolver(const char* moduleName, const char* jsSource)
{
    (void)moduleName;
    (void)jsSource;
    return PatchResult::ok("registerModuleResolver (stub)");
}

PatchResult JSExtensionHost::executeScript(const char* extensionId, const char* script, char* outResult,
                                           size_t maxResultLen)
{
    (void)extensionId;
    (void)script;
    if (outResult && maxResultLen)
        outResult[0] = '\0';
    return PatchResult::error("executeScript not available in RawrEngine headless lane");
}

uint64_t JSExtensionHost::createTimer(uint64_t delayMs, bool repeat, void* jsCallback)
{
    (void)delayMs;
    (void)repeat;
    (void)jsCallback;
    return 0;
}

void JSExtensionHost::cancelTimer(uint64_t timerId)
{
    (void)timerId;
}

JSExtensionHost::Stats JSExtensionHost::getStats() const
{
    Stats s{};
    return s;
}

JSExtensionHost::VSIXVerificationResult JSExtensionHost::verifyVSIX(const char* vsixPath) const
{
    (void)vsixPath;
    VSIXVerificationResult r{};
    r.isValid = false;
    r.isSigned = false;
    r.signatureVerified = false;
    std::strncpy(r.errorDetail, "verifyVSIX not available in RawrEngine headless lane", sizeof(r.errorDetail) - 1);
    return r;
}

void JSExtensionHost::bindVSCodeCommands(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeWindow(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeWorkspace(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeLanguages(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeDebug(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeTasks(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeEnv(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeExtensions(void* ctx)
{
    (void)ctx;
}
void JSExtensionHost::bindVSCodeAPI(void* ctx)
{
    (void)ctx;
}
