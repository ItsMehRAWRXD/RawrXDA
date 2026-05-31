/**
 * @file quickjs_extension_host.cpp
 * @brief QuickJS Extension Host — Wires QuickJS to RawrXD Extension System
 *
 * Loads JS/TS extensions via QuickJS runtime, exposes VS Code API bridge
 * to JS context, and manages activation/deactivation lifecycle.
 */

#include "quickjs_extension_host.h"
#include "vscode_api_bridge.h"
#include "process_broker.h"
#include "os_sandbox.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>

// QuickJS headers (compiled as C, included in C++ with extern "C")
extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}

namespace RawrXD::Extensions {

// ============================================================================
// QuickJSExtensionHost Implementation
// ============================================================================

struct QuickJSRuntimeState {
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
    std::string extId;
    std::string extPath;
    bool active = false;
    VSCodeAPIBridge* apiBridge = nullptr;
    ProcessBroker* broker = nullptr;
    OSSandbox* sandbox = nullptr;
};

class QuickJSExtensionHost::Impl {
public:
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    VSCodeAPIBridge* m_apiBridge = nullptr;
    ProcessBroker* m_broker = nullptr;
    OSSandbox* m_sandbox = nullptr;
    std::map<std::string, std::unique_ptr<QuickJSRuntimeState>> m_runtimes;

    // JS C-function wrappers
    static JSValue js_vscode_executeCommand(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
        auto* state = static_cast<QuickJSRuntimeState*>(JS_GetContextOpaque(ctx));
        if (!state || !state->apiBridge || argc < 1) return JS_UNDEFINED;
        const char* cmd = JS_ToCString(ctx, argv[0]);
        if (!cmd) return JS_UNDEFINED;
        nlohmann::json args = nlohmann::json::object();
        if (argc >= 2) {
            const char* argStr = JS_ToCString(ctx, argv[1]);
            if (argStr) {
                try { args = nlohmann::json::parse(argStr); } catch (...) {}
                JS_FreeCString(ctx, argStr);
            }
        }
        auto result = state->apiBridge->executeCommand(cmd, args);
        JS_FreeCString(ctx, cmd);
        return JS_NewString(ctx, result.dump().c_str());
    }

    static JSValue js_vscode_getConfiguration(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
        auto* state = static_cast<QuickJSRuntimeState*>(JS_GetContextOpaque(ctx));
        if (!state || !state->apiBridge) return JS_UNDEFINED;
        std::string section;
        if (argc >= 1) {
            const char* s = JS_ToCString(ctx, argv[0]);
            if (s) { section = s; JS_FreeCString(ctx, s); }
        }
        auto cfg = state->apiBridge->getConfiguration(section);
        return JS_NewString(ctx, cfg.dump().c_str());
    }

    static JSValue js_vscode_showInformationMessage(JSContext* ctx, JSValueConst this_val,
                                                      int argc, JSValueConst* argv) {
        auto* state = static_cast<QuickJSRuntimeState*>(JS_GetContextOpaque(ctx));
        if (!state || !state->apiBridge || argc < 1) return JS_UNDEFINED;
        const char* msg = JS_ToCString(ctx, argv[0]);
        if (!msg) return JS_UNDEFINED;
        state->apiBridge->showInformationMessage(msg);
        JS_FreeCString(ctx, msg);
        return JS_TRUE;
    }

    static JSValue js_vscode_openTextDocument(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
        auto* state = static_cast<QuickJSRuntimeState*>(JS_GetContextOpaque(ctx));
        if (!state || !state->apiBridge || argc < 1) return JS_UNDEFINED;
        const char* path = JS_ToCString(ctx, argv[0]);
        if (!path) return JS_UNDEFINED;
        auto doc = state->apiBridge->openTextDocument(VSCodeUri::file(path));
        JS_FreeCString(ctx, path);
        return JS_NewString(ctx, doc.toJson().dump().c_str());
    }

    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
        for (int i = 0; i < argc; ++i) {
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                OutputDebugStringA(str);
                OutputDebugStringA(" ");
                JS_FreeCString(ctx, str);
            }
        }
        OutputDebugStringA("\n");
        return JS_UNDEFINED;
    }

    bool injectVSCodeAPI(JSContext* ctx, QuickJSRuntimeState* state) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue vscode = JS_NewObject(ctx);

        // vscode.commands
        JSValue commands = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, commands, "executeCommand",
            JS_NewCFunction(ctx, js_vscode_executeCommand, "executeCommand", 2));
        JS_SetPropertyStr(ctx, vscode, "commands", commands);

        // vscode.workspace
        JSValue workspace = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, workspace, "getConfiguration",
            JS_NewCFunction(ctx, js_vscode_getConfiguration, "getConfiguration", 1));
        JS_SetPropertyStr(ctx, workspace, "openTextDocument",
            JS_NewCFunction(ctx, js_vscode_openTextDocument, "openTextDocument", 1));
        JS_SetPropertyStr(ctx, vscode, "workspace", workspace);

        // vscode.window
        JSValue window = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, window, "showInformationMessage",
            JS_NewCFunction(ctx, js_vscode_showInformationMessage, "showInformationMessage", 1));
        JS_SetPropertyStr(ctx, vscode, "window", window);

        // console
        JSValue console = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, console, "log",
            JS_NewCFunction(ctx, js_console_log, "log", 1));
        JS_SetPropertyStr(ctx, global, "console", console);

        JS_SetPropertyStr(ctx, global, "vscode", vscode);
        JS_SetContextOpaque(ctx, state);

        JS_FreeValue(ctx, global);
        return true;
    }
};

// ============================================================================
// Public Interface
// ============================================================================

QuickJSExtensionHost::QuickJSExtensionHost()
    : m_impl(std::make_unique<Impl>()) {}

QuickJSExtensionHost::~QuickJSExtensionHost() = default;

bool QuickJSExtensionHost::initialize(VSCodeAPIBridge* apiBridge,
                                       ProcessBroker* broker,
                                       OSSandbox* sandbox) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (m_impl->m_initialized) return true;
    m_impl->m_apiBridge = apiBridge;
    m_impl->m_broker = broker;
    m_impl->m_sandbox = sandbox;
    m_impl->m_initialized = true;
    return true;
}

void QuickJSExtensionHost::shutdown() {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (!m_impl->m_initialized) return;
    for (auto& [_, state] : m_impl->m_runtimes) {
        if (state->ctx) JS_FreeContext(state->ctx);
        if (state->rt) JS_FreeRuntime(state->rt);
    }
    m_impl->m_runtimes.clear();
    m_impl->m_initialized = false;
}

bool QuickJSExtensionHost::loadExtension(const std::string& extId,
                                          const std::string& path) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    if (!m_impl->m_initialized) return false;
    if (m_impl->m_runtimes.count(extId)) return false;

    std::filesystem::path extPath(path);
    auto mainFile = extPath / "extension.js";
    if (!std::filesystem::exists(mainFile)) {
        mainFile = extPath / "out" / "extension.js";
    }
    if (!std::filesystem::exists(mainFile)) {
        mainFile = extPath / "dist" / "extension.js";
    }
    if (!std::filesystem::exists(mainFile)) return false;

    auto state = std::make_unique<QuickJSRuntimeState>();
    state->extId = extId;
    state->extPath = path;
    state->apiBridge = m_impl->m_apiBridge;
    state->broker = m_impl->m_broker;
    state->sandbox = m_impl->m_sandbox;

    state->rt = JS_NewRuntime();
    if (!state->rt) return false;

    JS_SetMemoryLimit(state->rt, 64 * 1024 * 1024); // 64 MB
    JS_SetMaxStackSize(state->rt, 1024 * 1024);     // 1 MB

    state->ctx = JS_NewContext(state->rt);
    if (!state->ctx) {
        JS_FreeRuntime(state->rt);
        return false;
    }

    js_std_init_handlers(state->rt);
    js_std_add_helpers(state->ctx, 0, nullptr);

    m_impl->injectVSCodeAPI(state->ctx, state.get());

    // Load extension.js
    try {
        std::ifstream f(mainFile, std::ios::binary);
        std::string code((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
        if (code.empty()) {
            JS_FreeContext(state->ctx);
            JS_FreeRuntime(state->rt);
            return false;
        }
        JSValue val = JS_Eval(state->ctx, code.c_str(), code.size(),
                                mainFile.string().c_str(), 0);
        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(state->ctx);
            const char* err = JS_ToCString(state->ctx, exc);
            if (err) {
                OutputDebugStringA("[QuickJS] Extension load error: ");
                OutputDebugStringA(err);
                OutputDebugStringA("\n");
                JS_FreeCString(state->ctx, err);
            }
            JS_FreeValue(state->ctx, exc);
            JS_FreeValue(state->ctx, val);
            JS_FreeContext(state->ctx);
            JS_FreeRuntime(state->rt);
            return false;
        }
        JS_FreeValue(state->ctx, val);
    } catch (...) {
        JS_FreeContext(state->ctx);
        JS_FreeRuntime(state->rt);
        return false;
    }

    m_impl->m_runtimes[extId] = std::move(state);
    return true;
}

bool QuickJSExtensionHost::activateExtension(const std::string& extId) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    auto it = m_impl->m_runtimes.find(extId);
    if (it == m_impl->m_runtimes.end()) return false;
    auto* state = it->second.get();
    if (state->active) return true;

    // Call activate() if defined
    JSValue global = JS_GetGlobalObject(state->ctx);
    JSValue activateFn = JS_GetPropertyStr(state->ctx, global, "activate");
    if (JS_IsFunction(state->ctx, activateFn)) {
        JSValue arg = JS_NewObject(state->ctx);
        JSValue result = JS_Call(state->ctx, activateFn, global, 1, &arg);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(state->ctx);
            const char* err = JS_ToCString(state->ctx, exc);
            if (err) {
                OutputDebugStringA("[QuickJS] activate() error: ");
                OutputDebugStringA(err);
                OutputDebugStringA("\n");
                JS_FreeCString(state->ctx, err);
            }
            JS_FreeValue(state->ctx, exc);
            JS_FreeValue(state->ctx, result);
            JS_FreeValue(state->ctx, arg);
            JS_FreeValue(state->ctx, activateFn);
            JS_FreeValue(state->ctx, global);
            return false;
        }
        JS_FreeValue(state->ctx, result);
        JS_FreeValue(state->ctx, arg);
    }
    JS_FreeValue(state->ctx, activateFn);
    JS_FreeValue(state->ctx, global);

    state->active = true;
    return true;
}

bool QuickJSExtensionHost::deactivateExtension(const std::string& extId) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    auto it = m_impl->m_runtimes.find(extId);
    if (it == m_impl->m_runtimes.end()) return false;
    auto* state = it->second.get();
    if (!state->active) return true;

    // Call deactivate() if defined
    JSValue global = JS_GetGlobalObject(state->ctx);
    JSValue deactivateFn = JS_GetPropertyStr(state->ctx, global, "deactivate");
    if (JS_IsFunction(state->ctx, deactivateFn)) {
        JSValue result = JS_Call(state->ctx, deactivateFn, global, 0, nullptr);
        JS_FreeValue(state->ctx, result);
    }
    JS_FreeValue(state->ctx, deactivateFn);
    JS_FreeValue(state->ctx, global);

    state->active = false;
    return true;
}

bool QuickJSExtensionHost::unloadExtension(const std::string& extId) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    auto it = m_impl->m_runtimes.find(extId);
    if (it == m_impl->m_runtimes.end()) return false;
    auto* state = it->second.get();
    if (state->active) deactivateExtension(extId);
    if (state->ctx) JS_FreeContext(state->ctx);
    if (state->rt) JS_FreeRuntime(state->rt);
    m_impl->m_runtimes.erase(it);
    return true;
}

bool QuickJSExtensionHost::isExtensionLoaded(const std::string& extId) const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    return m_impl->m_runtimes.count(extId) > 0;
}

bool QuickJSExtensionHost::isExtensionActive(const std::string& extId) const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    auto it = m_impl->m_runtimes.find(extId);
    if (it == m_impl->m_runtimes.end()) return false;
    return it->second->active;
}

size_t QuickJSExtensionHost::getLoadedCount() const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    return m_impl->m_runtimes.size();
}

size_t QuickJSExtensionHost::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    size_t count = 0;
    for (const auto& [_, state] : m_impl->m_runtimes) {
        if (state->active) ++count;
    }
    return count;
}

std::vector<std::string> QuickJSExtensionHost::listLoaded() const {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    std::vector<std::string> ids;
    for (const auto& [id, _] : m_impl->m_runtimes) ids.push_back(id);
    return ids;
}

} // namespace RawrXD::Extensions
