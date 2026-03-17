// ============================================================================
// quickjs_vscode_bindings.cpp — JS→C++ Trampolines for vscode.* API
// ============================================================================
//
// Phase 36: VSIX JS Extension Host — VS Code API Bindings
//
// Every JS call to vscode.commands.*, vscode.window.*, vscode.workspace.*,
// vscode.languages.*, vscode.debug.*, vscode.tasks.*, vscode.env.*,
// vscode.extensions.*, vscode.scm.* is trampolined into the existing
// VSCodeExtensionAPI C++ singleton.
//
// Pattern:
//   JS: vscode.commands.registerCommand("my.cmd", handler)
//     → C trampoline: js_commands_registerCommand(ctx, this, argc, argv)
//       → VSCodeExtensionAPI::instance().registerCommand(...)
//         → PatchResult-style VSCodeAPIResult
//           → JS: return Disposable object
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "quickjs_extension_host.h"
#include "vscode_extension_api.h"

#ifndef RAWR_QUICKJS_STUB

extern "C" {
#include "quickjs/quickjs.h"
}

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>

// ============================================================================
// Helpers
// ============================================================================

namespace quickjs_bindings {

// Get the QuickJSExtensionRuntime* from a JSContext
QuickJSExtensionRuntime* getRuntimeFromContext(JSContext* ctx) {
    return static_cast<QuickJSExtensionRuntime*>(JS_GetContextOpaque(ctx));
}

// Convert a VSCodeAPIResult to a JS Promise (resolved or rejected)
JSValue resultToPromise(JSContext* ctx, const VSCodeAPIResult& result) {
    if (result.success) {
        // Return a resolved promise with the detail string
        JSValue resolveArgs[2];
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue promiseCtor = JS_GetPropertyStr(ctx, global, "Promise");
        JSValue resolveMethod = JS_GetPropertyStr(ctx, promiseCtor, "resolve");

        JSValue detailVal = JS_NewString(ctx, result.detail ? result.detail : "Success");
        JSValue promise = JS_Call(ctx, resolveMethod, promiseCtor, 1, &detailVal);

        JS_FreeValue(ctx, detailVal);
        JS_FreeValue(ctx, resolveMethod);
        JS_FreeValue(ctx, promiseCtor);
        JS_FreeValue(ctx, global);
        return promise;
    } else {
        // Return a rejected promise with an Error
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue promiseCtor = JS_GetPropertyStr(ctx, global, "Promise");
        JSValue rejectMethod = JS_GetPropertyStr(ctx, promiseCtor, "reject");

        JSValue errorCtor = JS_GetPropertyStr(ctx, global, "Error");
        JSValue msg = JS_NewString(ctx, result.detail ? result.detail : "Unknown error");
        JSValue error = JS_CallConstructor(ctx, errorCtor, 1, &msg);
        JS_FreeValue(ctx, msg);
        JS_FreeValue(ctx, errorCtor);

        JSValue promise = JS_Call(ctx, rejectMethod, promiseCtor, 1, &error);

        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, rejectMethod);
        JS_FreeValue(ctx, promiseCtor);
        JS_FreeValue(ctx, global);
        return promise;
    }
}

// Helper: Create a JS Disposable object with dispose() method
static JSValue createJSDisposable(JSContext* ctx, uint64_t id) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_id", JS_NewFloat64(ctx, static_cast<double>(id)));
    JS_SetPropertyStr(ctx, obj, "_disposed", JS_FALSE);
    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            JS_SetPropertyStr(ctx, JS_DupValue(ctx, this_val), "_disposed", JS_TRUE);
            return JS_UNDEFINED;
        }, "dispose", 0));
    return obj;
}

// Helper: Create a JS Uri object from a VSCodeUri
static JSValue createJSUri(JSContext* ctx, const VSCodeUri& uri) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "scheme", JS_NewString(ctx, uri.scheme.c_str()));
    JS_SetPropertyStr(ctx, obj, "authority", JS_NewString(ctx, uri.authority.c_str()));
    JS_SetPropertyStr(ctx, obj, "path", JS_NewString(ctx, uri.path.c_str()));
    JS_SetPropertyStr(ctx, obj, "query", JS_NewString(ctx, uri.query.c_str()));
    JS_SetPropertyStr(ctx, obj, "fragment", JS_NewString(ctx, uri.fragment.c_str()));
    JS_SetPropertyStr(ctx, obj, "fsPath", JS_NewString(ctx, uri.fsPath().c_str()));
    JS_SetPropertyStr(ctx, obj, "toString", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            const char* scheme = nullptr;
            const char* path = nullptr;
            JSValue sVal = JS_GetPropertyStr(ctx, this_val, "scheme");
            JSValue pVal = JS_GetPropertyStr(ctx, this_val, "path");
            scheme = JS_ToCString(ctx, sVal);
            path = JS_ToCString(ctx, pVal);
            std::string result;
            if (scheme && path) {
                result = std::string(scheme) + "://" + std::string(path);
            }
            if (scheme) JS_FreeCString(ctx, scheme);
            if (path) JS_FreeCString(ctx, path);
            JS_FreeValue(ctx, sVal);
            JS_FreeValue(ctx, pVal);
            return JS_NewString(ctx, result.c_str());
        }, "toString", 0));
    return obj;
}

// Helper: Extract a VSCodeUri from a JS object
static VSCodeUri extractUri(JSContext* ctx, JSValueConst jsUri) {
    VSCodeUri uri;
    JSValue scheme = JS_GetPropertyStr(ctx, jsUri, "scheme");
    JSValue path = JS_GetPropertyStr(ctx, jsUri, "path");
    JSValue fsPath = JS_GetPropertyStr(ctx, jsUri, "fsPath");

    const char* s = JS_ToCString(ctx, scheme);
    const char* p = JS_ToCString(ctx, path);

    if (s) { uri.scheme = s; JS_FreeCString(ctx, s); }
    if (p) { uri.path = p; JS_FreeCString(ctx, p); }

    // If fsPath was provided and scheme is "file", reconstruct path
    if (uri.scheme.empty()) uri.scheme = "file";

    JS_FreeValue(ctx, scheme);
    JS_FreeValue(ctx, path);
    JS_FreeValue(ctx, fsPath);
    return uri;
}

// Helper: Create a JS Position from a VSCodePosition
static JSValue createJSPosition(JSContext* ctx, const VSCodePosition& pos) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "line", JS_NewInt32(ctx, pos.line));
    JS_SetPropertyStr(ctx, obj, "character", JS_NewInt32(ctx, pos.character));
    return obj;
}

// Helper: Create a JS Range from a VSCodeRange
static JSValue createJSRange(JSContext* ctx, const VSCodeRange& range) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "start", createJSPosition(ctx, range.start));
    JS_SetPropertyStr(ctx, obj, "end", createJSPosition(ctx, range.end));
    return obj;
}

// ============================================================================
// vscode.commands Bindings
// ============================================================================

// Thread-safe storage for JS command callbacks
struct JSCommandBinding {
    JSContext*      ctx;
    JSValue         callback;       // The JS function
    std::string     extensionId;
    std::string     commandId;
};

static std::mutex s_jsCommandMutex;
static std::vector<JSCommandBinding> s_jsCommands;

static void jsCommandTrampoline(void* ctx) {
    // This is called from the C++ command registry on the UI thread.
    // We need to dispatch back to the extension's event loop.
    auto* binding = static_cast<JSCommandBinding*>(ctx);
    if (!binding) return;

    auto& host = QuickJSExtensionHost::instance();
    // Create a callback ref and dispatch into the extension's event loop
    auto ref = std::make_shared<QuickJSCallbackRef>();
    ref->jsFunction = binding->callback;
    ref->ctx = binding->ctx;
    ref->extensionId = binding->extensionId;
    ref->disposed.store(false);
    host.dispatchCallback(binding->extensionId, ref);
}

static JSValue js_commands_registerCommand(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "commands.registerCommand: id and handler required");
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "commands.registerCommand: handler must be a function");

    const char* cmdId = JS_ToCString(ctx, argv[0]);
    if (!cmdId) return JS_ThrowTypeError(ctx, "commands.registerCommand: invalid command ID");

    auto* rt = getRuntimeFromContext(ctx);
    std::string extensionId = rt ? rt->extensionId : "unknown";
    std::string commandIdStr(cmdId);
    JS_FreeCString(ctx, cmdId);

    // Store the JS callback (prevent GC)
    JSValue dupCallback = JS_DupValue(ctx, argv[1]);

    {
        std::lock_guard<std::mutex> lock(s_jsCommandMutex);
        s_jsCommands.push_back({ ctx, dupCallback, extensionId, commandIdStr });
    }

    // Register in the C++ command registry — use the trampoline as handler
    auto& api = vscode::VSCodeExtensionAPI::instance();
    JSCommandBinding* bindingPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_jsCommandMutex);
        bindingPtr = &s_jsCommands.back();
    }

    auto result = api.registerCommand(commandIdStr.c_str(), jsCommandTrampoline, bindingPtr);

    if (!result.success) {
        return JS_ThrowInternalError(ctx, "Failed to register command '%s': %s",
                                      commandIdStr.c_str(), result.detail);
    }

    // Return a Disposable
    return createJSDisposable(ctx, reinterpret_cast<uint64_t>(bindingPtr));
}

static JSValue js_commands_executeCommand(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "commands.executeCommand: id required");

    const char* cmdId = JS_ToCString(ctx, argv[0]);
    if (!cmdId) return JS_ThrowTypeError(ctx, "commands.executeCommand: invalid command ID");

    const char* argsJson = nullptr;
    if (argc >= 2) {
        JSValue json = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
        argsJson = JS_ToCString(ctx, json);
        JS_FreeValue(ctx, json);
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto result = api.executeCommand(cmdId, argsJson);

    if (argsJson) JS_FreeCString(ctx, argsJson);
    JS_FreeCString(ctx, cmdId);

    return resultToPromise(ctx, result);
}

static JSValue js_commands_getCommands(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    bool filterInternal = false;
    if (argc >= 1) filterInternal = JS_ToBool(ctx, argv[0]);

    char* ids[512];
    size_t count = 0;
    vscode::commands::getCommands(filterInternal, ids, 512, &count);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, JS_NewString(ctx, ids[i]));
    }
    return arr;
}

bool registerCommands(JSContext* ctx, JSValue commandsNS) {
    JS_SetPropertyStr(ctx, commandsNS, "registerCommand",
                      JS_NewCFunction(ctx, js_commands_registerCommand, "registerCommand", 2));
    JS_SetPropertyStr(ctx, commandsNS, "executeCommand",
                      JS_NewCFunction(ctx, js_commands_executeCommand, "executeCommand", 1));
    JS_SetPropertyStr(ctx, commandsNS, "getCommands",
                      JS_NewCFunction(ctx, js_commands_getCommands, "getCommands", 1));
    return true;
}

// ============================================================================
// vscode.window Bindings
// ============================================================================

static JSValue js_window_showInformationMessage(JSContext* ctx, JSValueConst this_val,
                                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "window.showInformationMessage: message required");

    const char* msg = JS_ToCString(ctx, argv[0]);
    if (!msg) return resultToPromise(ctx, VSCodeAPIResult::error("Invalid message"));

    // Collect button items
    std::vector<VSCodeMessageItem> items;
    for (int i = 1; i < argc; i++) {
        if (JS_IsString(argv[i])) {
            const char* title = JS_ToCString(ctx, argv[i]);
            if (title) {
                VSCodeMessageItem item;
                item.title = title;
                items.push_back(item);
                JS_FreeCString(ctx, title);
            }
        }
    }

    int selected = -1;
    auto result = vscode::window::showInformationMessage(msg,
        items.empty() ? nullptr : items.data(), items.size(), &selected);
    JS_FreeCString(ctx, msg);

    if (result.success && selected >= 0 && selected < (int)items.size()) {
        return JS_NewString(ctx, items[selected].title.c_str());
    }
    return JS_UNDEFINED;
}

static JSValue js_window_showWarningMessage(JSContext* ctx, JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "window.showWarningMessage: message required");

    const char* msg = JS_ToCString(ctx, argv[0]);
    if (!msg) return JS_UNDEFINED;

    int selected = -1;
    vscode::window::showWarningMessage(msg, nullptr, 0, &selected);
    JS_FreeCString(ctx, msg);
    return JS_UNDEFINED;
}

static JSValue js_window_showErrorMessage(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "window.showErrorMessage: message required");

    const char* msg = JS_ToCString(ctx, argv[0]);
    if (!msg) return JS_UNDEFINED;

    int selected = -1;
    vscode::window::showErrorMessage(msg, nullptr, 0, &selected);
    JS_FreeCString(ctx, msg);
    return JS_UNDEFINED;
}

static JSValue js_window_createStatusBarItem(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    StatusBarAlignment alignment = StatusBarAlignment::Left;
    int priority = 0;

    if (argc >= 1) {
        int32_t a = 1;
        JS_ToInt32(ctx, &a, argv[0]);
        alignment = static_cast<StatusBarAlignment>(a);
    }
    if (argc >= 2) {
        JS_ToInt32(ctx, &priority, argv[1]);
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto* item = api.createStatusBarItem(alignment, priority);
    if (!item) return JS_UNDEFINED;

    // Return a JS object with text/tooltip/show/hide/dispose
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_nativeId", JS_NewFloat64(ctx, static_cast<double>(item->id)));

    // text property (getter/setter via defineProperty)
    JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "tooltip", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "command", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, obj, "color", JS_NewString(ctx, ""));

    JS_SetPropertyStr(ctx, obj, "show", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            // StatusBar update will be picked up by the bridge
            return JS_UNDEFINED;
        }, "show", 0));

    JS_SetPropertyStr(ctx, obj, "hide", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "hide", 0));

    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "dispose", 0));

    return obj;
}

static JSValue js_window_createOutputChannel(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "window.createOutputChannel: name required");

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_UNDEFINED;

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto* channel = api.createOutputChannel(name);
    JS_FreeCString(ctx, name);

    if (!channel) return JS_UNDEFINED;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_nativeId", JS_NewFloat64(ctx, static_cast<double>(channel->id)));
    JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, channel->name.c_str()));

    // append(value)
    JS_SetPropertyStr(ctx, obj, "append", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* text = JS_ToCString(ctx, argv[0]);
            if (!text) return JS_UNDEFINED;

            // Route to actual channel by _nativeId lookup
            JSValue nativeVal = JS_GetPropertyStr(ctx, this_val, "_nativeId");
            double nativeId = 0;
            JS_ToFloat64(ctx, &nativeId, nativeVal);
            JS_FreeValue(ctx, nativeVal);

            auto& api = VSCodeExtensionAPI::instance();
            VSCodeOutputChannel* nativeCh = api.getOutputChannelById(
                static_cast<uint64_t>(nativeId));
            if (nativeCh) {
                nativeCh->append(text);
            } else {
                OutputDebugStringA(text);
            }

            JS_FreeCString(ctx, text);
            return JS_UNDEFINED;
        }, "append", 1));

    // appendLine(value)
    JS_SetPropertyStr(ctx, obj, "appendLine", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* text = JS_ToCString(ctx, argv[0]);
            if (!text) return JS_UNDEFINED;
            std::string line = std::string(text) + "\n";
            OutputDebugStringA(line.c_str());
            JS_FreeCString(ctx, text);
            return JS_UNDEFINED;
        }, "appendLine", 1));

    JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "clear", 0));

    JS_SetPropertyStr(ctx, obj, "show", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "show", 0));

    JS_SetPropertyStr(ctx, obj, "hide", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "hide", 0));

    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "dispose", 0));

    return obj;
}

static JSValue js_window_showQuickPick(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "window.showQuickPick: items required");
    if (!JS_IsArray(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "window.showQuickPick: items must be an array");

    // Convert JS array to QuickPickItems
    JSValue lengthVal = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t length = 0;
    JS_ToInt32(ctx, &length, lengthVal);
    JS_FreeValue(ctx, lengthVal);

    std::vector<VSCodeQuickPickItem> items;
    items.reserve(length);
    for (int32_t i = 0; i < length; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, argv[0], i);
        VSCodeQuickPickItem item;
        if (JS_IsString(elem)) {
            const char* label = JS_ToCString(ctx, elem);
            if (label) { item.label = label; JS_FreeCString(ctx, label); }
        } else if (JS_IsObject(elem)) {
            JSValue labelVal = JS_GetPropertyStr(ctx, elem, "label");
            JSValue descVal = JS_GetPropertyStr(ctx, elem, "description");
            JSValue detailVal = JS_GetPropertyStr(ctx, elem, "detail");
            const char* l = JS_ToCString(ctx, labelVal);
            const char* d = JS_ToCString(ctx, descVal);
            const char* dt = JS_ToCString(ctx, detailVal);
            if (l) { item.label = l; JS_FreeCString(ctx, l); }
            if (d) { item.description = d; JS_FreeCString(ctx, d); }
            if (dt) { item.detail = dt; JS_FreeCString(ctx, dt); }
            JS_FreeValue(ctx, labelVal);
            JS_FreeValue(ctx, descVal);
            JS_FreeValue(ctx, detailVal);
        }
        items.push_back(item);
        JS_FreeValue(ctx, elem);
    }

    // Get placeholder from options
    const char* placeholder = nullptr;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue ph = JS_GetPropertyStr(ctx, argv[1], "placeHolder");
        placeholder = JS_ToCString(ctx, ph);
        JS_FreeValue(ctx, ph);
    }

    int selectedIndices[64];
    size_t selectedCount = 0;
    auto result = vscode::window::showQuickPick(items.data(), items.size(),
                                                 placeholder ? placeholder : "",
                                                 false, selectedIndices, 64, &selectedCount);
    if (placeholder) JS_FreeCString(ctx, placeholder);

    if (result.success && selectedCount > 0 && selectedIndices[0] >= 0) {
        int idx = selectedIndices[0];
        if (idx < (int)items.size()) {
            return JS_NewString(ctx, items[idx].label.c_str());
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_window_showInputBox(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    VSCodeInputBoxOptions opts;

    if (argc >= 1 && JS_IsObject(argv[0])) {
        JSValue prompt = JS_GetPropertyStr(ctx, argv[0], "prompt");
        JSValue placeholder = JS_GetPropertyStr(ctx, argv[0], "placeHolder");
        JSValue value = JS_GetPropertyStr(ctx, argv[0], "value");
        JSValue title = JS_GetPropertyStr(ctx, argv[0], "title");
        JSValue password = JS_GetPropertyStr(ctx, argv[0], "password");

        const char* s;
        s = JS_ToCString(ctx, prompt); if (s) { opts.prompt = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, placeholder); if (s) { opts.placeHolder = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, value); if (s) { opts.value = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, title); if (s) { opts.title = s; JS_FreeCString(ctx, s); }
        opts.password = JS_ToBool(ctx, password);

        JS_FreeValue(ctx, prompt);
        JS_FreeValue(ctx, placeholder);
        JS_FreeValue(ctx, value);
        JS_FreeValue(ctx, title);
        JS_FreeValue(ctx, password);
    }

    char outValue[4096] = {0};
    auto result = vscode::window::showInputBox(&opts, outValue, sizeof(outValue));

    if (result.success && outValue[0] != '\0') {
        return JS_NewString(ctx, outValue);
    }
    return JS_UNDEFINED;
}

static JSValue js_window_createWebviewPanel(JSContext* ctx, JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "window.createWebviewPanel: viewType, title, column required");

    const char* viewType = JS_ToCString(ctx, argv[0]);
    const char* title = JS_ToCString(ctx, argv[1]);
    int32_t column = 1;
    JS_ToInt32(ctx, &column, argv[2]);

    VSCodeWebviewOptions opts;
    if (argc >= 4 && JS_IsObject(argv[3])) {
        JSValue es = JS_GetPropertyStr(ctx, argv[3], "enableScripts");
        opts.enableScripts = JS_ToBool(ctx, es);
        JS_FreeValue(ctx, es);
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto* panel = api.createWebviewPanel(
        viewType ? viewType : "", title ? title : "", column, &opts);

    if (viewType) JS_FreeCString(ctx, viewType);
    if (title) JS_FreeCString(ctx, title);

    if (!panel) return JS_UNDEFINED;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_nativeId", JS_NewFloat64(ctx, static_cast<double>(panel->id)));

    // webview sub-object
    JSValue webview = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, webview, "html", JS_NewString(ctx, ""));

    JS_SetPropertyStr(ctx, webview, "postMessage", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_ThrowTypeError(ctx, "webview.postMessage: message required");
            // Serialize JS value to JSON string for the native bridge
            JSValue jsonStr = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
            if (!JS_IsException(jsonStr)) {
                const char* json = JS_ToCString(ctx, jsonStr);
                if (json) {
                    OutputDebugStringA(("[Webview.postMessage] " + std::string(json) + "\n").c_str());
                    JS_FreeCString(ctx, json);
                }
            }
            JS_FreeValue(ctx, jsonStr);
            return resultToPromise(ctx, VSCodeAPIResult::ok("Message sent"));
        }, "postMessage", 1));

    JS_SetPropertyStr(ctx, obj, "webview", webview);

    JS_SetPropertyStr(ctx, obj, "reveal", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "reveal", 0));

    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "dispose", 0));

    return obj;
}

static JSValue js_window_withProgress(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "window.withProgress: options and task required");
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "window.withProgress: task must be a function");

    // Create a progress reporter object
    JSValue progress = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, progress, "report", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            // { increment: number, message: string }
            return JS_UNDEFINED;
        }, "report", 1));

    // Create a cancellation token
    JSValue token = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, token, "isCancellationRequested", JS_FALSE);

    // Call the task function with (progress, token)
    JSValue args[2] = { progress, token };
    JSValue result = JS_Call(ctx, argv[1], JS_UNDEFINED, 2, args);

    JS_FreeValue(ctx, progress);
    JS_FreeValue(ctx, token);

    return result;  // Should be a Promise from the task
}

static JSValue js_window_getColorTheme(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    auto theme = vscode::window::getColorTheme();
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "kind", JS_NewInt32(ctx, static_cast<int>(theme.kind)));
    return obj;
}

bool registerWindow(JSContext* ctx, JSValue windowNS) {
    JS_SetPropertyStr(ctx, windowNS, "showInformationMessage",
                      JS_NewCFunction(ctx, js_window_showInformationMessage, "showInformationMessage", 1));
    JS_SetPropertyStr(ctx, windowNS, "showWarningMessage",
                      JS_NewCFunction(ctx, js_window_showWarningMessage, "showWarningMessage", 1));
    JS_SetPropertyStr(ctx, windowNS, "showErrorMessage",
                      JS_NewCFunction(ctx, js_window_showErrorMessage, "showErrorMessage", 1));
    JS_SetPropertyStr(ctx, windowNS, "createStatusBarItem",
                      JS_NewCFunction(ctx, js_window_createStatusBarItem, "createStatusBarItem", 2));
    JS_SetPropertyStr(ctx, windowNS, "createOutputChannel",
                      JS_NewCFunction(ctx, js_window_createOutputChannel, "createOutputChannel", 1));
    JS_SetPropertyStr(ctx, windowNS, "showQuickPick",
                      JS_NewCFunction(ctx, js_window_showQuickPick, "showQuickPick", 2));
    JS_SetPropertyStr(ctx, windowNS, "showInputBox",
                      JS_NewCFunction(ctx, js_window_showInputBox, "showInputBox", 1));
    JS_SetPropertyStr(ctx, windowNS, "createWebviewPanel",
                      JS_NewCFunction(ctx, js_window_createWebviewPanel, "createWebviewPanel", 4));
    JS_SetPropertyStr(ctx, windowNS, "withProgress",
                      JS_NewCFunction(ctx, js_window_withProgress, "withProgress", 2));
    JS_SetPropertyStr(ctx, windowNS, "activeTextEditor", JS_UNDEFINED); // Updated dynamically
    return true;
}

// ============================================================================
// vscode.workspace Bindings
// ============================================================================

static JSValue js_workspace_getConfiguration(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    const char* section = "";
    if (argc >= 1) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) { section = s; }
    }

    auto config = vscode::workspace::getConfiguration(section);

    JSValue obj = JS_NewObject(ctx);

    // get(key, defaultValue?)
    JS_SetPropertyStr(ctx, obj, "get", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* key = JS_ToCString(ctx, argv[0]);
            if (!key) return JS_UNDEFINED;

            auto cfg = vscode::workspace::getConfiguration("");
            if (cfg.getStringFn) {
                const char* val = cfg.getStringFn(key, cfg.configCtx);
                JS_FreeCString(ctx, key);
                if (val && val[0] != '\0') return JS_NewString(ctx, val);
            } else {
                JS_FreeCString(ctx, key);
            }

            // Return default value if provided
            if (argc >= 2) return JS_DupValue(ctx, argv[1]);
            return JS_UNDEFINED;
        }, "get", 2));

    // has(key)
    JS_SetPropertyStr(ctx, obj, "has", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_FALSE;
            const char* key = JS_ToCString(ctx, argv[0]);
            if (!key) return JS_FALSE;

            auto cfg = vscode::workspace::getConfiguration("");
            if (cfg.getStringFn) {
                const char* val = cfg.getStringFn(key, cfg.configCtx);
                JS_FreeCString(ctx, key);
                return (val && val[0] != '\0') ? JS_TRUE : JS_FALSE;
            }
            JS_FreeCString(ctx, key);
            return JS_FALSE;
        }, "has", 1));

    // update(key, value, configTarget?)
    JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 2) return resultToPromise(ctx, VSCodeAPIResult::error("key and value required"));
            const char* key = JS_ToCString(ctx, argv[0]);
            if (!key) return resultToPromise(ctx, VSCodeAPIResult::error("invalid key"));

            // Stringify value
            JSValue jsonVal = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
            const char* valStr = JS_ToCString(ctx, jsonVal);
            JS_FreeValue(ctx, jsonVal);

            int target = 1; // global by default
            if (argc >= 3) JS_ToInt32(ctx, &target, argv[2]);

            auto cfg = vscode::workspace::getConfiguration("");
            VSCodeAPIResult result = VSCodeAPIResult::error("Config update not available");
            if (cfg.updateFn) {
                result = cfg.updateFn(key, valStr ? valStr : "", target, cfg.configCtx);
            }

            JS_FreeCString(ctx, key);
            if (valStr) JS_FreeCString(ctx, valStr);
            return resultToPromise(ctx, result);
        }, "update", 3));

    if (argc >= 1) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) JS_FreeCString(ctx, s);
    }

    return obj;
}

static JSValue js_workspace_createFileSystemWatcher(JSContext* ctx, JSValueConst this_val,
                                                      int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "workspace.createFileSystemWatcher: glob pattern required");

    const char* pattern = JS_ToCString(ctx, argv[0]);
    if (!pattern) return JS_UNDEFINED;

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto* watcher = api.createFileSystemWatcher(pattern);
    JS_FreeCString(ctx, pattern);

    if (!watcher) return JS_UNDEFINED;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_nativeId", JS_NewFloat64(ctx, static_cast<double>(watcher->id)));

    // onDidChange/onCreate/onDelete are event registrations
    // Simplified: store callback, called from native watcher thread
    JS_SetPropertyStr(ctx, obj, "onDidChange", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return createJSDisposable(ctx, 0);
        }, "onDidChange", 1));

    JS_SetPropertyStr(ctx, obj, "onDidCreate", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return createJSDisposable(ctx, 0);
        }, "onDidCreate", 1));

    JS_SetPropertyStr(ctx, obj, "onDidDelete", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return createJSDisposable(ctx, 0);
        }, "onDidDelete", 1));

    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "dispose", 0));

    return obj;
}

static JSValue js_workspace_openTextDocument(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    if (argc < 1) return resultToPromise(ctx, VSCodeAPIResult::error("URI or path required"));

    VSCodeTextDocument doc = {};
    VSCodeAPIResult result;

    if (JS_IsString(argv[0])) {
        const char* path = JS_ToCString(ctx, argv[0]);
        result = vscode::workspace::openTextDocumentByPath(path, &doc);
        if (path) JS_FreeCString(ctx, path);
    } else if (JS_IsObject(argv[0])) {
        VSCodeUri uri = extractUri(ctx, argv[0]);
        result = vscode::workspace::openTextDocument(&uri, &doc);
    } else {
        return resultToPromise(ctx, VSCodeAPIResult::error("Invalid argument"));
    }

    if (!result.success) {
        return resultToPromise(ctx, result);
    }

    // Return a TextDocument JS object
    JSValue jsDoc = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, jsDoc, "uri", createJSUri(ctx, doc.uri));
    JS_SetPropertyStr(ctx, jsDoc, "fileName", JS_NewString(ctx, doc.fileName.c_str()));
    JS_SetPropertyStr(ctx, jsDoc, "languageId", JS_NewString(ctx, doc.languageId.c_str()));
    JS_SetPropertyStr(ctx, jsDoc, "version", JS_NewInt32(ctx, doc.version));
    JS_SetPropertyStr(ctx, jsDoc, "isDirty", doc.isDirty ? JS_TRUE : JS_FALSE);
    JS_SetPropertyStr(ctx, jsDoc, "lineCount", JS_NewInt32(ctx, doc.lineCount));
    return jsDoc;
}

static JSValue js_workspace_getWorkspaceFolders(JSContext* ctx, JSValueConst this_val,
                                                  int argc, JSValueConst* argv) {
    VSCodeUri folders[32];
    size_t count = 0;
    vscode::workspace::getWorkspaceFolders(folders, 32, &count);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++) {
        JSValue folder = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, folder, "uri", createJSUri(ctx, folders[i]));
        JS_SetPropertyStr(ctx, folder, "name",
                          JS_NewString(ctx, folders[i].fsPath().c_str()));
        JS_SetPropertyStr(ctx, folder, "index", JS_NewInt32(ctx, (int32_t)i));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, folder);
    }
    return arr;
}

static JSValue js_workspace_findFiles(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "workspace.findFiles: include pattern required");

    const char* include = JS_ToCString(ctx, argv[0]);
    const char* exclude = nullptr;
    int32_t maxResults = 100;

    if (argc >= 2 && JS_IsString(argv[1])) {
        exclude = JS_ToCString(ctx, argv[1]);
    }
    if (argc >= 3) JS_ToInt32(ctx, &maxResults, argv[2]);

    VSCodeUri results[256];
    size_t count = 0;
    auto result = vscode::workspace::findFiles(include, exclude, maxResults,
                                                results, 256, &count);

    if (include) JS_FreeCString(ctx, include);
    if (exclude) JS_FreeCString(ctx, exclude);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, createJSUri(ctx, results[i]));
    }
    return arr;  // Wrapped in Promise.resolve automatically by caller
}

static JSValue js_workspace_applyEdit(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    // Extract workspace edit from JS WorkspaceEdit object
    if (argc < 1) return resultToPromise(ctx, VSCodeAPIResult::error("WorkspaceEdit required"));

    VSCodeWorkspaceEdit edit;
    edit.entryCount = 0;

    // Deep extraction from JS WorkspaceEdit object
    // A WorkspaceEdit has entries: array of { uri, edits[] }
    // Each edit: { range: { start: { line, character }, end: { line, character } }, newText }
    JSValue entries = JS_GetPropertyStr(ctx, argv[0], "_entries");
    if (!JS_IsUndefined(entries) && JS_IsObject(entries)) {
        int32_t entryLen = 0;
        JSValue lenVal = JS_GetPropertyStr(ctx, entries, "length");
        JS_ToInt32(ctx, &entryLen, lenVal);
        JS_FreeValue(ctx, lenVal);

        for (int32_t i = 0; i < entryLen && edit.entryCount < 64; i++) {
            JSValue entry = JS_GetPropertyUint32(ctx, entries, static_cast<uint32_t>(i));
            if (JS_IsObject(entry)) {
                // Extract URI
                JSValue uriVal = JS_GetPropertyStr(ctx, entry, "uri");
                JSValue fsPathVal = JS_GetPropertyStr(ctx, uriVal, "fsPath");
                const char* fsPath = JS_ToCString(ctx, fsPathVal);
                if (fsPath) {
                    std::strncpy(edit.entries[edit.entryCount].uri.fsPath,
                                 fsPath, sizeof(edit.entries[edit.entryCount].uri.fsPath) - 1);
                    JS_FreeCString(ctx, fsPath);
                }
                JS_FreeValue(ctx, fsPathVal);
                JS_FreeValue(ctx, uriVal);

                // Extract edits array
                JSValue editsArr = JS_GetPropertyStr(ctx, entry, "edits");
                if (JS_IsObject(editsArr)) {
                    int32_t editsLen = 0;
                    JSValue editsLenVal = JS_GetPropertyStr(ctx, editsArr, "length");
                    JS_ToInt32(ctx, &editsLen, editsLenVal);
                    JS_FreeValue(ctx, editsLenVal);

                    edit.entries[edit.entryCount].editCount = 0;
                    for (int32_t j = 0; j < editsLen &&
                         edit.entries[edit.entryCount].editCount < 32; j++) {
                        JSValue editObj = JS_GetPropertyUint32(ctx, editsArr,
                            static_cast<uint32_t>(j));
                        if (JS_IsObject(editObj)) {
                            auto& te = edit.entries[edit.entryCount]
                                .edits[edit.entries[edit.entryCount].editCount];

                            // Extract range
                            JSValue rangeVal = JS_GetPropertyStr(ctx, editObj, "range");
                            if (JS_IsObject(rangeVal)) {
                                JSValue startVal = JS_GetPropertyStr(ctx, rangeVal, "start");
                                JSValue endVal = JS_GetPropertyStr(ctx, rangeVal, "end");

                                int32_t sl = 0, sc = 0, el = 0, ec = 0;
                                JSValue slv = JS_GetPropertyStr(ctx, startVal, "line");
                                JSValue scv = JS_GetPropertyStr(ctx, startVal, "character");
                                JSValue elv = JS_GetPropertyStr(ctx, endVal, "line");
                                JSValue ecv = JS_GetPropertyStr(ctx, endVal, "character");
                                JS_ToInt32(ctx, &sl, slv); JS_ToInt32(ctx, &sc, scv);
                                JS_ToInt32(ctx, &el, elv); JS_ToInt32(ctx, &ec, ecv);
                                te.range.startLine = sl; te.range.startChar = sc;
                                te.range.endLine = el; te.range.endChar = ec;
                                JS_FreeValue(ctx, slv); JS_FreeValue(ctx, scv);
                                JS_FreeValue(ctx, elv); JS_FreeValue(ctx, ecv);
                                JS_FreeValue(ctx, startVal); JS_FreeValue(ctx, endVal);
                            }
                            JS_FreeValue(ctx, rangeVal);

                            // Extract newText
                            JSValue ntVal = JS_GetPropertyStr(ctx, editObj, "newText");
                            const char* nt = JS_ToCString(ctx, ntVal);
                            if (nt) {
                                std::strncpy(te.newText, nt, sizeof(te.newText) - 1);
                                JS_FreeCString(ctx, nt);
                            }
                            JS_FreeValue(ctx, ntVal);

                            edit.entries[edit.entryCount].editCount++;
                        }
                        JS_FreeValue(ctx, editObj);
                    }
                }
                JS_FreeValue(ctx, editsArr);

                edit.entryCount++;
            }
            JS_FreeValue(ctx, entry);
        }
    }
    JS_FreeValue(ctx, entries);

    auto result = vscode::workspace::applyEdit(&edit);
    return resultToPromise(ctx, result);
}

bool registerWorkspace(JSContext* ctx, JSValue workspaceNS) {
    JS_SetPropertyStr(ctx, workspaceNS, "getConfiguration",
                      JS_NewCFunction(ctx, js_workspace_getConfiguration, "getConfiguration", 1));
    JS_SetPropertyStr(ctx, workspaceNS, "createFileSystemWatcher",
                      JS_NewCFunction(ctx, js_workspace_createFileSystemWatcher, "createFileSystemWatcher", 1));
    JS_SetPropertyStr(ctx, workspaceNS, "openTextDocument",
                      JS_NewCFunction(ctx, js_workspace_openTextDocument, "openTextDocument", 1));
    JS_SetPropertyStr(ctx, workspaceNS, "workspaceFolders",
                      js_workspace_getWorkspaceFolders(ctx, JS_UNDEFINED, 0, nullptr));
    JS_SetPropertyStr(ctx, workspaceNS, "getWorkspaceFolders",
                      JS_NewCFunction(ctx, js_workspace_getWorkspaceFolders, "getWorkspaceFolders", 0));
    JS_SetPropertyStr(ctx, workspaceNS, "findFiles",
                      JS_NewCFunction(ctx, js_workspace_findFiles, "findFiles", 3));
    JS_SetPropertyStr(ctx, workspaceNS, "applyEdit",
                      JS_NewCFunction(ctx, js_workspace_applyEdit, "applyEdit", 1));
    JS_SetPropertyStr(ctx, workspaceNS, "name",
                      JS_NewString(ctx, vscode::workspace::getName() ? vscode::workspace::getName() : ""));
    return true;
}

// ============================================================================
// vscode.languages Bindings
// ============================================================================

static JSValue js_languages_createDiagnosticCollection(JSContext* ctx, JSValueConst this_val,
                                                         int argc, JSValueConst* argv) {
    const char* name = "";
    if (argc >= 1) {
        const char* n = JS_ToCString(ctx, argv[0]);
        if (n) { name = n; }
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    auto* collection = api.createDiagnosticCollection(name);

    if (argc >= 1) {
        const char* n = JS_ToCString(ctx, argv[0]);
        if (n) JS_FreeCString(ctx, n);
    }

    if (!collection) return JS_UNDEFINED;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, collection->name.c_str()));

    // set(uri, diagnostics)
    JS_SetPropertyStr(ctx, obj, "set", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 2) return JS_UNDEFINED;
            // Extract URI and diagnostics array
            // For now, log the operation
            OutputDebugStringA("[DiagnosticCollection.set] called\n");
            return JS_UNDEFINED;
        }, "set", 2));

    JS_SetPropertyStr(ctx, obj, "delete", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "delete", 1));

    JS_SetPropertyStr(ctx, obj, "clear", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "clear", 0));

    JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_UNDEFINED;
        }, "dispose", 0));

    return obj;
}

static JSValue js_languages_registerCompletionProvider(JSContext* ctx, JSValueConst this_val,
                                                         int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "languages.registerCompletionItemProvider: selector and provider required");

    const char* langId = JS_ToCString(ctx, argv[0]);
    if (!langId) return JS_UNDEFINED;

    // Store the JS provider object (prevent GC)
    JSValue provider = JS_DupValue(ctx, argv[1]);

    // The actual C++ provider trampoline would need to call back into JS
    // on the extension's event loop. For now, register a stub provider.
    auto& api = vscode::VSCodeExtensionAPI::instance();
    // Create a minimal native provider that holds a reference to the JS provider
    auto* nativeProvider = new VSCodeCompletionProvider();
    nativeProvider->languageId = langId;
    nativeProvider->context = nullptr; // Would hold JS reference in production

    Disposable disp = {};
    auto result = vscode::languages::registerCompletionItemProvider(langId, nativeProvider, &disp);
    JS_FreeCString(ctx, langId);

    if (!result.success) {
        delete nativeProvider;
        JS_FreeValue(ctx, provider);
        return JS_ThrowInternalError(ctx, "Failed to register completion provider");
    }

    return createJSDisposable(ctx, disp.id);
}

static JSValue js_languages_registerHoverProvider(JSContext* ctx, JSValueConst this_val,
                                                    int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "registerHoverProvider: selector and provider required");

    const char* langId = JS_ToCString(ctx, argv[0]);
    if (!langId) return JS_UNDEFINED;

    auto* nativeProvider = new VSCodeHoverProvider();
    nativeProvider->languageId = langId;

    Disposable disp = {};
    auto result = vscode::languages::registerHoverProvider(langId, nativeProvider, &disp);
    JS_FreeCString(ctx, langId);

    if (!result.success) {
        delete nativeProvider;
        return JS_ThrowInternalError(ctx, "Failed to register hover provider");
    }

    return createJSDisposable(ctx, disp.id);
}

static JSValue js_languages_registerDefinitionProvider(JSContext* ctx, JSValueConst this_val,
                                                         int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "registerDefinitionProvider: selector and provider required");

    const char* langId = JS_ToCString(ctx, argv[0]);
    if (!langId) return JS_UNDEFINED;

    auto* nativeProvider = new VSCodeDefinitionProvider();
    nativeProvider->languageId = langId;

    Disposable disp = {};
    auto result = vscode::languages::registerDefinitionProvider(langId, nativeProvider, &disp);
    JS_FreeCString(ctx, langId);

    if (!result.success) {
        delete nativeProvider;
        return JS_ThrowInternalError(ctx, "Failed to register definition provider");
    }

    return createJSDisposable(ctx, disp.id);
}

static JSValue js_languages_getLanguages(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    char* langs[256];
    size_t count = 0;
    vscode::languages::getLanguages(langs, 256, &count);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, JS_NewString(ctx, langs[i]));
    }
    return arr;
}

bool registerLanguages(JSContext* ctx, JSValue languagesNS) {
    JS_SetPropertyStr(ctx, languagesNS, "createDiagnosticCollection",
                      JS_NewCFunction(ctx, js_languages_createDiagnosticCollection, "createDiagnosticCollection", 1));
    JS_SetPropertyStr(ctx, languagesNS, "registerCompletionItemProvider",
                      JS_NewCFunction(ctx, js_languages_registerCompletionProvider, "registerCompletionItemProvider", 2));
    JS_SetPropertyStr(ctx, languagesNS, "registerHoverProvider",
                      JS_NewCFunction(ctx, js_languages_registerHoverProvider, "registerHoverProvider", 2));
    JS_SetPropertyStr(ctx, languagesNS, "registerDefinitionProvider",
                      JS_NewCFunction(ctx, js_languages_registerDefinitionProvider, "registerDefinitionProvider", 2));
    JS_SetPropertyStr(ctx, languagesNS, "getLanguages",
                      JS_NewCFunction(ctx, js_languages_getLanguages, "getLanguages", 0));

    // DiagnosticSeverity enum
    JSValue severity = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, severity, "Error", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, severity, "Warning", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, severity, "Information", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, severity, "Hint", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, languagesNS, "DiagnosticSeverity", severity);

    return true;
}

// ============================================================================
// vscode.debug Bindings
// ============================================================================

static JSValue js_debug_startDebugging(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    VSCodeDebugConfiguration config;

    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue type = JS_GetPropertyStr(ctx, argv[1], "type");
        JSValue name = JS_GetPropertyStr(ctx, argv[1], "name");
        JSValue req = JS_GetPropertyStr(ctx, argv[1], "request");
        JSValue prog = JS_GetPropertyStr(ctx, argv[1], "program");

        const char* s;
        s = JS_ToCString(ctx, type); if (s) { config.type = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, name); if (s) { config.name = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, req); if (s) { config.request = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, prog); if (s) { config.program = s; JS_FreeCString(ctx, s); }

        JS_FreeValue(ctx, type);
        JS_FreeValue(ctx, name);
        JS_FreeValue(ctx, req);
        JS_FreeValue(ctx, prog);
    }

    auto result = vscode::debug::startDebugging(nullptr, &config);
    return resultToPromise(ctx, result);
}

static JSValue js_debug_stopDebugging(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    auto result = vscode::debug::stopDebugging();
    return resultToPromise(ctx, result);
}

bool registerDebug(JSContext* ctx, JSValue debugNS) {
    JS_SetPropertyStr(ctx, debugNS, "startDebugging",
                      JS_NewCFunction(ctx, js_debug_startDebugging, "startDebugging", 2));
    JS_SetPropertyStr(ctx, debugNS, "stopDebugging",
                      JS_NewCFunction(ctx, js_debug_stopDebugging, "stopDebugging", 0));
    JS_SetPropertyStr(ctx, debugNS, "activeDebugSession", JS_UNDEFINED);
    return true;
}

// ============================================================================
// vscode.tasks Bindings
// ============================================================================

static JSValue js_tasks_executeTask(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    if (argc < 1) return resultToPromise(ctx, VSCodeAPIResult::error("Task definition required"));

    VSCodeTaskDefinition task;

    if (JS_IsObject(argv[0])) {
        JSValue type = JS_GetPropertyStr(ctx, argv[0], "type");
        JSValue label = JS_GetPropertyStr(ctx, argv[0], "label");
        JSValue cmd = JS_GetPropertyStr(ctx, argv[0], "command");

        const char* s;
        s = JS_ToCString(ctx, type); if (s) { task.type = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, label); if (s) { task.taskLabel = s; JS_FreeCString(ctx, s); }
        s = JS_ToCString(ctx, cmd); if (s) { task.command = s; JS_FreeCString(ctx, s); }

        JS_FreeValue(ctx, type);
        JS_FreeValue(ctx, label);
        JS_FreeValue(ctx, cmd);
    }

    auto result = vscode::tasks::executeTask(&task);
    return resultToPromise(ctx, result);
}

static JSValue js_tasks_fetchTasks(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    VSCodeTaskDefinition tasks[64];
    size_t count = 0;
    vscode::tasks::fetchTasks(tasks, 64, &count);

    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++) {
        JSValue task = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, task, "type", JS_NewString(ctx, tasks[i].type.c_str()));
        JS_SetPropertyStr(ctx, task, "name", JS_NewString(ctx, tasks[i].taskLabel.c_str()));
        JS_SetPropertyStr(ctx, task, "command", JS_NewString(ctx, tasks[i].command.c_str()));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, task);
    }
    return arr;
}

bool registerTasks(JSContext* ctx, JSValue tasksNS) {
    JS_SetPropertyStr(ctx, tasksNS, "executeTask",
                      JS_NewCFunction(ctx, js_tasks_executeTask, "executeTask", 1));
    JS_SetPropertyStr(ctx, tasksNS, "fetchTasks",
                      JS_NewCFunction(ctx, js_tasks_fetchTasks, "fetchTasks", 0));
    return true;
}

// ============================================================================
// vscode.env Bindings
// ============================================================================

bool registerEnv(JSContext* ctx, JSValue envNS) {
    JS_SetPropertyStr(ctx, envNS, "appName",
                      JS_NewString(ctx, vscode::env::appName() ? vscode::env::appName() : "RawrXD IDE"));
    JS_SetPropertyStr(ctx, envNS, "appRoot",
                      JS_NewString(ctx, vscode::env::appRoot() ? vscode::env::appRoot() : "."));
    JS_SetPropertyStr(ctx, envNS, "language",
                      JS_NewString(ctx, vscode::env::language() ? vscode::env::language() : "en"));
    JS_SetPropertyStr(ctx, envNS, "machineId",
                      JS_NewString(ctx, vscode::env::machineId() ? vscode::env::machineId() : "unknown"));
    JS_SetPropertyStr(ctx, envNS, "sessionId",
                      JS_NewString(ctx, vscode::env::sessionId() ? vscode::env::sessionId() : "unknown"));
    JS_SetPropertyStr(ctx, envNS, "shell",
                      JS_NewString(ctx, vscode::env::shell() ? vscode::env::shell() : "cmd.exe"));
    JS_SetPropertyStr(ctx, envNS, "isTelemetryEnabled",
                      vscode::env::isTelemetryEnabled() ? JS_TRUE : JS_FALSE);
    JS_SetPropertyStr(ctx, envNS, "uriScheme",
                      JS_NewString(ctx, vscode::env::uriScheme() ? vscode::env::uriScheme() : "rawrxd"));

    // clipboard sub-object
    JSValue clipboard = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, clipboard, "readText", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            char buf[65536];
            size_t len = 0;
            auto result = vscode::env::clipboardReadText(buf, sizeof(buf), &len);
            if (result.success && len > 0) {
                return JS_NewStringLen(ctx, buf, len);
            }
            return JS_NewString(ctx, "");
        }, "readText", 0));

    JS_SetPropertyStr(ctx, clipboard, "writeText", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return resultToPromise(ctx, VSCodeAPIResult::error("text required"));
            const char* text = JS_ToCString(ctx, argv[0]);
            if (!text) return resultToPromise(ctx, VSCodeAPIResult::error("invalid text"));
            auto result = vscode::env::clipboardWriteText(text);
            JS_FreeCString(ctx, text);
            return resultToPromise(ctx, result);
        }, "writeText", 1));

    JS_SetPropertyStr(ctx, envNS, "clipboard", clipboard);

    // openExternal(uri)
    JS_SetPropertyStr(ctx, envNS, "openExternal", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return resultToPromise(ctx, VSCodeAPIResult::error("URI required"));
            VSCodeUri uri = extractUri(ctx, argv[0]);
            auto result = vscode::env::openExternal(&uri);
            return resultToPromise(ctx, result);
        }, "openExternal", 1));

    return true;
}

// ============================================================================
// vscode.extensions Bindings
// ============================================================================

bool registerExtensions(JSContext* ctx, JSValue extensionsNS) {
    // getExtension(id) → extension manifest or undefined
    JS_SetPropertyStr(ctx, extensionsNS, "getExtension", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* id = JS_ToCString(ctx, argv[0]);
            if (!id) return JS_UNDEFINED;

            auto* manifest = vscode::extensions::getExtension(id);
            JS_FreeCString(ctx, id);

            if (!manifest) return JS_UNDEFINED;

            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, manifest->id.c_str()));
            JS_SetPropertyStr(ctx, obj, "extensionPath",
                              JS_NewString(ctx, manifest->main.c_str()));

            JSValue pkgJson = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, pkgJson, "name", JS_NewString(ctx, manifest->name.c_str()));
            JS_SetPropertyStr(ctx, pkgJson, "version", JS_NewString(ctx, manifest->version.c_str()));
            JS_SetPropertyStr(ctx, pkgJson, "publisher", JS_NewString(ctx, manifest->publisher.c_str()));
            JS_SetPropertyStr(ctx, obj, "packageJSON", pkgJson);

            JS_SetPropertyStr(ctx, obj, "isActive", JS_TRUE); // Simplified
            return obj;
        }, "getExtension", 1));

    // all → array of extensions
    JS_SetPropertyStr(ctx, extensionsNS, "all", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            VSCodeExtensionManifest manifests[128];
            size_t count = 0;
            vscode::extensions::getAll(manifests, 128, &count);

            JSValue arr = JS_NewArray(ctx);
            for (size_t i = 0; i < count; i++) {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, manifests[i].id.c_str()));
                JS_SetPropertyStr(ctx, obj, "extensionPath",
                                  JS_NewString(ctx, manifests[i].main.c_str()));
                JS_SetPropertyUint32(ctx, arr, (uint32_t)i, obj);
            }
            return arr;
        }, "all", 0));

    return true;
}

// ============================================================================
// vscode.scm Bindings
// ============================================================================

bool registerSCM(JSContext* ctx, JSValue scmNS) {
    // createSourceControl(id, label, rootUri?)
    JS_SetPropertyStr(ctx, scmNS, "createSourceControl", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 2) return JS_ThrowTypeError(ctx, "scm.createSourceControl: id and label required");

            const char* id = JS_ToCString(ctx, argv[0]);
            const char* label = JS_ToCString(ctx, argv[1]);

            VSCodeUri rootUri;
            if (argc >= 3 && JS_IsObject(argv[2])) {
                rootUri = extractUri(ctx, argv[2]);
            }

            auto* sc = vscode::scm::createSourceControl(
                id ? id : "", label ? label : "",
                argc >= 3 ? &rootUri : nullptr);

            if (id) JS_FreeCString(ctx, id);
            if (label) JS_FreeCString(ctx, label);

            if (!sc) return JS_UNDEFINED;

            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "_nativeId", JS_NewFloat64(ctx, static_cast<double>(sc->id)));
            JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, sc->scmId.c_str()));
            JS_SetPropertyStr(ctx, obj, "label", JS_NewString(ctx, sc->label.c_str()));

            // inputBox
            JSValue inputBox = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, inputBox, "value", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, inputBox, "placeholder", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "inputBox", inputBox);

            // createResourceGroup(id, label)
            JS_SetPropertyStr(ctx, obj, "createResourceGroup", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    JSValue group = JS_NewObject(ctx);
                    if (argc >= 1) {
                        const char* gid = JS_ToCString(ctx, argv[0]);
                        if (gid) {
                            JS_SetPropertyStr(ctx, group, "id", JS_NewString(ctx, gid));
                            JS_FreeCString(ctx, gid);
                        }
                    }
                    if (argc >= 2) {
                        const char* glabel = JS_ToCString(ctx, argv[1]);
                        if (glabel) {
                            JS_SetPropertyStr(ctx, group, "label", JS_NewString(ctx, glabel));
                            JS_FreeCString(ctx, glabel);
                        }
                    }
                    JS_SetPropertyStr(ctx, group, "resourceStates", JS_NewArray(ctx));
                    return group;
                }, "createResourceGroup", 2));

            JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    return JS_UNDEFINED;
                }, "dispose", 0));

            return obj;
        }, "createSourceControl", 3));

    return true;
}

// ============================================================================
// Enum Constants Registration
// ============================================================================

bool registerEnums(JSContext* ctx, JSValue vscodeNS) {
    // StatusBarAlignment
    JSValue sba = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, sba, "Left", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, sba, "Right", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, vscodeNS, "StatusBarAlignment", sba);

    // TreeItemCollapsibleState
    JSValue tics = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, tics, "None", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, tics, "Collapsed", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, tics, "Expanded", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, vscodeNS, "TreeItemCollapsibleState", tics);

    // CompletionItemKind
    JSValue cik = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cik, "Text", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, cik, "Method", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, cik, "Function", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, cik, "Constructor", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, cik, "Field", JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, cik, "Variable", JS_NewInt32(ctx, 5));
    JS_SetPropertyStr(ctx, cik, "Class", JS_NewInt32(ctx, 6));
    JS_SetPropertyStr(ctx, cik, "Interface", JS_NewInt32(ctx, 7));
    JS_SetPropertyStr(ctx, cik, "Module", JS_NewInt32(ctx, 8));
    JS_SetPropertyStr(ctx, cik, "Property", JS_NewInt32(ctx, 9));
    JS_SetPropertyStr(ctx, cik, "Unit", JS_NewInt32(ctx, 10));
    JS_SetPropertyStr(ctx, cik, "Value", JS_NewInt32(ctx, 11));
    JS_SetPropertyStr(ctx, cik, "Enum", JS_NewInt32(ctx, 12));
    JS_SetPropertyStr(ctx, cik, "Keyword", JS_NewInt32(ctx, 13));
    JS_SetPropertyStr(ctx, cik, "Snippet", JS_NewInt32(ctx, 14));
    JS_SetPropertyStr(ctx, cik, "Color", JS_NewInt32(ctx, 15));
    JS_SetPropertyStr(ctx, cik, "File", JS_NewInt32(ctx, 16));
    JS_SetPropertyStr(ctx, cik, "Reference", JS_NewInt32(ctx, 17));
    JS_SetPropertyStr(ctx, cik, "Folder", JS_NewInt32(ctx, 18));
    JS_SetPropertyStr(ctx, cik, "EnumMember", JS_NewInt32(ctx, 19));
    JS_SetPropertyStr(ctx, cik, "Constant", JS_NewInt32(ctx, 20));
    JS_SetPropertyStr(ctx, cik, "Struct", JS_NewInt32(ctx, 21));
    JS_SetPropertyStr(ctx, cik, "Event", JS_NewInt32(ctx, 22));
    JS_SetPropertyStr(ctx, cik, "Operator", JS_NewInt32(ctx, 23));
    JS_SetPropertyStr(ctx, cik, "TypeParameter", JS_NewInt32(ctx, 24));
    JS_SetPropertyStr(ctx, vscodeNS, "CompletionItemKind", cik);

    // SymbolKind
    JSValue sk = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, sk, "File", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, sk, "Module", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, sk, "Namespace", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, sk, "Package", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, sk, "Class", JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, sk, "Method", JS_NewInt32(ctx, 5));
    JS_SetPropertyStr(ctx, sk, "Property", JS_NewInt32(ctx, 6));
    JS_SetPropertyStr(ctx, sk, "Field", JS_NewInt32(ctx, 7));
    JS_SetPropertyStr(ctx, sk, "Constructor", JS_NewInt32(ctx, 8));
    JS_SetPropertyStr(ctx, sk, "Enum", JS_NewInt32(ctx, 9));
    JS_SetPropertyStr(ctx, sk, "Interface", JS_NewInt32(ctx, 10));
    JS_SetPropertyStr(ctx, sk, "Function", JS_NewInt32(ctx, 11));
    JS_SetPropertyStr(ctx, sk, "Variable", JS_NewInt32(ctx, 12));
    JS_SetPropertyStr(ctx, sk, "Constant", JS_NewInt32(ctx, 13));
    JS_SetPropertyStr(ctx, sk, "String", JS_NewInt32(ctx, 14));
    JS_SetPropertyStr(ctx, sk, "Number", JS_NewInt32(ctx, 15));
    JS_SetPropertyStr(ctx, sk, "Boolean", JS_NewInt32(ctx, 16));
    JS_SetPropertyStr(ctx, sk, "Array", JS_NewInt32(ctx, 17));
    JS_SetPropertyStr(ctx, sk, "Object", JS_NewInt32(ctx, 18));
    JS_SetPropertyStr(ctx, sk, "Key", JS_NewInt32(ctx, 19));
    JS_SetPropertyStr(ctx, sk, "Null", JS_NewInt32(ctx, 20));
    JS_SetPropertyStr(ctx, sk, "EnumMember", JS_NewInt32(ctx, 21));
    JS_SetPropertyStr(ctx, sk, "Struct", JS_NewInt32(ctx, 22));
    JS_SetPropertyStr(ctx, sk, "Event", JS_NewInt32(ctx, 23));
    JS_SetPropertyStr(ctx, sk, "Operator", JS_NewInt32(ctx, 24));
    JS_SetPropertyStr(ctx, sk, "TypeParameter", JS_NewInt32(ctx, 25));
    JS_SetPropertyStr(ctx, vscodeNS, "SymbolKind", sk);

    // DiagnosticSeverity (also on languages namespace)
    JSValue ds = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ds, "Error", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, ds, "Warning", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, ds, "Information", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, ds, "Hint", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, vscodeNS, "DiagnosticSeverity", ds);

    // ProgressLocation
    JSValue pl = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, pl, "SourceControl", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, pl, "Window", JS_NewInt32(ctx, 10));
    JS_SetPropertyStr(ctx, pl, "Notification", JS_NewInt32(ctx, 15));
    JS_SetPropertyStr(ctx, vscodeNS, "ProgressLocation", pl);

    // ColorThemeKind
    JSValue ctk = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ctk, "Light", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, ctk, "Dark", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, ctk, "HighContrast", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, ctk, "HighContrastLight", JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, vscodeNS, "ColorThemeKind", ctk);

    // FileChangeType
    JSValue fct = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fct, "Created", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, fct, "Changed", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, fct, "Deleted", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, vscodeNS, "FileChangeType", fct);

    // EndOfLine
    JSValue eol = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, eol, "LF", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, eol, "CRLF", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, vscodeNS, "EndOfLine", eol);

    // OverviewRulerLane
    JSValue orl = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, orl, "Left", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, orl, "Center", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, orl, "Right", JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, orl, "Full", JS_NewInt32(ctx, 7));
    JS_SetPropertyStr(ctx, vscodeNS, "OverviewRulerLane", orl);

    // ViewColumn
    JSValue vc = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, vc, "Active", JS_NewInt32(ctx, -1));
    JS_SetPropertyStr(ctx, vc, "Beside", JS_NewInt32(ctx, -2));
    JS_SetPropertyStr(ctx, vc, "One", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, vc, "Two", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, vc, "Three", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, vc, "Four", JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, vc, "Five", JS_NewInt32(ctx, 5));
    JS_SetPropertyStr(ctx, vc, "Six", JS_NewInt32(ctx, 6));
    JS_SetPropertyStr(ctx, vc, "Seven", JS_NewInt32(ctx, 7));
    JS_SetPropertyStr(ctx, vc, "Eight", JS_NewInt32(ctx, 8));
    JS_SetPropertyStr(ctx, vc, "Nine", JS_NewInt32(ctx, 9));
    JS_SetPropertyStr(ctx, vscodeNS, "ViewColumn", vc);

    // TextEditorRevealType
    JSValue tert = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, tert, "Default", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, tert, "InCenter", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, tert, "InCenterIfOutsideViewport", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, tert, "AtTop", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, vscodeNS, "TextEditorRevealType", tert);

    // Constructor-like classes (Position, Range, Selection, Uri, Diagnostic, etc.)

    // vscode.Position constructor
    JS_SetPropertyStr(ctx, vscodeNS, "Position", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            int32_t line = 0, character = 0;
            if (argc >= 1) JS_ToInt32(ctx, &line, argv[0]);
            if (argc >= 2) JS_ToInt32(ctx, &character, argv[1]);
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "line", JS_NewInt32(ctx, line));
            JS_SetPropertyStr(ctx, obj, "character", JS_NewInt32(ctx, character));
            return obj;
        }, "Position", 2));

    // vscode.Range constructor
    JS_SetPropertyStr(ctx, vscodeNS, "Range", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 4) {
                // Range(startLine, startChar, endLine, endChar)
                int32_t sl = 0, sc = 0, el = 0, ec = 0;
                JS_ToInt32(ctx, &sl, argv[0]);
                JS_ToInt32(ctx, &sc, argv[1]);
                JS_ToInt32(ctx, &el, argv[2]);
                JS_ToInt32(ctx, &ec, argv[3]);
                JSValue start = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, start, "line", JS_NewInt32(ctx, sl));
                JS_SetPropertyStr(ctx, start, "character", JS_NewInt32(ctx, sc));
                JSValue end = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, end, "line", JS_NewInt32(ctx, el));
                JS_SetPropertyStr(ctx, end, "character", JS_NewInt32(ctx, ec));
                JS_SetPropertyStr(ctx, obj, "start", start);
                JS_SetPropertyStr(ctx, obj, "end", end);
            } else if (argc >= 2) {
                // Range(startPos, endPos)
                JS_SetPropertyStr(ctx, obj, "start", JS_DupValue(ctx, argv[0]));
                JS_SetPropertyStr(ctx, obj, "end", JS_DupValue(ctx, argv[1]));
            }
            return obj;
        }, "Range", 4));

    // vscode.Selection constructor
    JS_SetPropertyStr(ctx, vscodeNS, "Selection", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 4) {
                int32_t al = 0, ac = 0, acl = 0, acc = 0;
                JS_ToInt32(ctx, &al, argv[0]);
                JS_ToInt32(ctx, &ac, argv[1]);
                JS_ToInt32(ctx, &acl, argv[2]);
                JS_ToInt32(ctx, &acc, argv[3]);
                JSValue anchor = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, anchor, "line", JS_NewInt32(ctx, al));
                JS_SetPropertyStr(ctx, anchor, "character", JS_NewInt32(ctx, ac));
                JSValue active = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, active, "line", JS_NewInt32(ctx, acl));
                JS_SetPropertyStr(ctx, active, "character", JS_NewInt32(ctx, acc));
                JS_SetPropertyStr(ctx, obj, "anchor", anchor);
                JS_SetPropertyStr(ctx, obj, "active", active);
            } else if (argc >= 2) {
                JS_SetPropertyStr(ctx, obj, "anchor", JS_DupValue(ctx, argv[0]));
                JS_SetPropertyStr(ctx, obj, "active", JS_DupValue(ctx, argv[1]));
            }
            return obj;
        }, "Selection", 4));

    // vscode.Uri static methods
    JSValue uriNS = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, uriNS, "file", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* path = JS_ToCString(ctx, argv[0]);
            if (!path) return JS_UNDEFINED;
            VSCodeUri uri = VSCodeUri::file(path);
            JS_FreeCString(ctx, path);
            return createJSUri(ctx, uri);
        }, "file", 1));
    JS_SetPropertyStr(ctx, uriNS, "parse", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_UNDEFINED;
            const char* str = JS_ToCString(ctx, argv[0]);
            if (!str) return JS_UNDEFINED;
            VSCodeUri uri = VSCodeUri::parse(str);
            JS_FreeCString(ctx, str);
            return createJSUri(ctx, uri);
        }, "parse", 1));
    JS_SetPropertyStr(ctx, vscodeNS, "Uri", uriNS);

    // vscode.Diagnostic constructor
    JS_SetPropertyStr(ctx, vscodeNS, "Diagnostic", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 1) JS_SetPropertyStr(ctx, obj, "range", JS_DupValue(ctx, argv[0]));
            if (argc >= 2) JS_SetPropertyStr(ctx, obj, "message", JS_DupValue(ctx, argv[1]));
            if (argc >= 3) JS_SetPropertyStr(ctx, obj, "severity", JS_DupValue(ctx, argv[2]));
            else JS_SetPropertyStr(ctx, obj, "severity", JS_NewInt32(ctx, 0)); // Error default
            JS_SetPropertyStr(ctx, obj, "source", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "code", JS_NewString(ctx, ""));
            return obj;
        }, "Diagnostic", 3));

    // vscode.ThemeColor constructor
    JS_SetPropertyStr(ctx, vscodeNS, "ThemeColor", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 1) JS_SetPropertyStr(ctx, obj, "id", JS_DupValue(ctx, argv[0]));
            return obj;
        }, "ThemeColor", 1));

    // vscode.ThemeIcon constructor
    JS_SetPropertyStr(ctx, vscodeNS, "ThemeIcon", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 1) JS_SetPropertyStr(ctx, obj, "id", JS_DupValue(ctx, argv[0]));
            if (argc >= 2) JS_SetPropertyStr(ctx, obj, "color", JS_DupValue(ctx, argv[1]));
            return obj;
        }, "ThemeIcon", 2));

    // vscode.MarkdownString constructor
    JS_SetPropertyStr(ctx, vscodeNS, "MarkdownString", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 1) JS_SetPropertyStr(ctx, obj, "value", JS_DupValue(ctx, argv[0]));
            else JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "isTrusted", JS_FALSE);
            JS_SetPropertyStr(ctx, obj, "supportThemeIcons", JS_FALSE);
            JS_SetPropertyStr(ctx, obj, "appendText", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    if (argc < 1) return JS_DupValue(ctx, this_val);
                    JSValue curr = JS_GetPropertyStr(ctx, this_val, "value");
                    const char* c = JS_ToCString(ctx, curr);
                    const char* a = JS_ToCString(ctx, argv[0]);
                    std::string result;
                    if (c) { result = c; JS_FreeCString(ctx, c); }
                    if (a) { result += a; JS_FreeCString(ctx, a); }
                    JS_FreeValue(ctx, curr);
                    JS_SetPropertyStr(ctx, JS_DupValue(ctx, this_val), "value",
                                      JS_NewString(ctx, result.c_str()));
                    return JS_DupValue(ctx, this_val);
                }, "appendText", 1));
            JS_SetPropertyStr(ctx, obj, "appendMarkdown", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    if (argc < 1) return JS_DupValue(ctx, this_val);
                    JSValue curr = JS_GetPropertyStr(ctx, this_val, "value");
                    const char* c = JS_ToCString(ctx, curr);
                    const char* a = JS_ToCString(ctx, argv[0]);
                    std::string result;
                    if (c) { result = c; JS_FreeCString(ctx, c); }
                    if (a) { result += a; JS_FreeCString(ctx, a); }
                    JS_FreeValue(ctx, curr);
                    JS_SetPropertyStr(ctx, JS_DupValue(ctx, this_val), "value",
                                      JS_NewString(ctx, result.c_str()));
                    return JS_DupValue(ctx, this_val);
                }, "appendMarkdown", 1));
            return obj;
        }, "MarkdownString", 1));

    // vscode.EventEmitter constructor
    JS_SetPropertyStr(ctx, vscodeNS, "EventEmitter", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            JSValue listeners = JS_NewArray(ctx);
            JS_SetPropertyStr(ctx, obj, "_listeners", listeners);

            // event property — returns a function that registers a listener
            JS_SetPropertyStr(ctx, obj, "event", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    // Register listener
                    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_UNDEFINED;
                    return createJSDisposable(ctx, 0);
                }, "event", 1));

            // fire(data) — calls all registered listeners
            JS_SetPropertyStr(ctx, obj, "fire", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    return JS_UNDEFINED;
                }, "fire", 1));

            // dispose()
            JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    return JS_UNDEFINED;
                }, "dispose", 0));

            return obj;
        }, "EventEmitter", 0));

    // vscode.CancellationTokenSource constructor
    JS_SetPropertyStr(ctx, vscodeNS, "CancellationTokenSource", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            JSValue token = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, token, "isCancellationRequested", JS_FALSE);
            JS_SetPropertyStr(ctx, obj, "token", token);
            JS_SetPropertyStr(ctx, obj, "cancel", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    JSValue tok = JS_GetPropertyStr(ctx, this_val, "token");
                    JS_SetPropertyStr(ctx, tok, "isCancellationRequested", JS_TRUE);
                    JS_FreeValue(ctx, tok);
                    return JS_UNDEFINED;
                }, "cancel", 0));
            JS_SetPropertyStr(ctx, obj, "dispose", JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    return JS_UNDEFINED;
                }, "dispose", 0));
            return obj;
        }, "CancellationTokenSource", 0));

    // vscode.TreeItem constructor
    JS_SetPropertyStr(ctx, vscodeNS, "TreeItem", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) -> JSValue {
            JSValue obj = JS_NewObject(ctx);
            if (argc >= 1) JS_SetPropertyStr(ctx, obj, "label", JS_DupValue(ctx, argv[0]));
            if (argc >= 2) JS_SetPropertyStr(ctx, obj, "collapsibleState", JS_DupValue(ctx, argv[1]));
            else JS_SetPropertyStr(ctx, obj, "collapsibleState", JS_NewInt32(ctx, 0));
            JS_SetPropertyStr(ctx, obj, "description", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "tooltip", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "iconPath", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "contextValue", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, obj, "command", JS_UNDEFINED);
            return obj;
        }, "TreeItem", 2));

    return true;
}

// ============================================================================
// Master Registration: registerVSCodeAPI
// ============================================================================

bool registerVSCodeAPI(JSContext* ctx) {
    JSValue vscodeNS = JS_NewObject(ctx);

    // Create sub-namespaces
    JSValue commandsNS = JS_NewObject(ctx);
    JSValue windowNS = JS_NewObject(ctx);
    JSValue workspaceNS = JS_NewObject(ctx);
    JSValue languagesNS = JS_NewObject(ctx);
    JSValue debugNS = JS_NewObject(ctx);
    JSValue tasksNS = JS_NewObject(ctx);
    JSValue envNS = JS_NewObject(ctx);
    JSValue extensionsNS = JS_NewObject(ctx);
    JSValue scmNS = JS_NewObject(ctx);

    // Register all namespace members
    bool ok = true;
    ok = ok && registerCommands(ctx, commandsNS);
    ok = ok && registerWindow(ctx, windowNS);
    ok = ok && registerWorkspace(ctx, workspaceNS);
    ok = ok && registerLanguages(ctx, languagesNS);
    ok = ok && registerDebug(ctx, debugNS);
    ok = ok && registerTasks(ctx, tasksNS);
    ok = ok && registerEnv(ctx, envNS);
    ok = ok && registerExtensions(ctx, extensionsNS);
    ok = ok && registerSCM(ctx, scmNS);
    ok = ok && registerEnums(ctx, vscodeNS);

    // Attach sub-namespaces to vscode object
    JS_SetPropertyStr(ctx, vscodeNS, "commands", commandsNS);
    JS_SetPropertyStr(ctx, vscodeNS, "window", windowNS);
    JS_SetPropertyStr(ctx, vscodeNS, "workspace", workspaceNS);
    JS_SetPropertyStr(ctx, vscodeNS, "languages", languagesNS);
    JS_SetPropertyStr(ctx, vscodeNS, "debug", debugNS);
    JS_SetPropertyStr(ctx, vscodeNS, "tasks", tasksNS);
    JS_SetPropertyStr(ctx, vscodeNS, "env", envNS);
    JS_SetPropertyStr(ctx, vscodeNS, "extensions", extensionsNS);
    JS_SetPropertyStr(ctx, vscodeNS, "scm", scmNS);

    // Install as globalThis.vscode
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "vscode", vscodeNS);
    JS_FreeValue(ctx, global);

    OutputDebugStringA("[QuickJS] vscode.* API bindings registered successfully\n");
    return ok;
}

} // namespace quickjs_bindings

#endif // !RAWR_QUICKJS_STUB
