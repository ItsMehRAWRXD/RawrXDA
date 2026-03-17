// ============================================================================
// quickjs_node_shims.cpp — Node.js Compatibility Shims for QuickJS Extensions
// ============================================================================
//
// Phase 36: VSIX JS Extension Host — Node.js Module Shims
//
// Provides sandboxed implementations of:
//   fs      → readFileSync, writeFileSync, existsSync, readdirSync, statSync,
//             mkdirSync, unlinkSync, readFile (async wrapper)
//   path    → join, resolve, dirname, basename, extname, sep, delimiter,
//             normalize, isAbsolute, relative, parse, format
//   os      → platform(), homedir(), tmpdir(), EOL, arch(), hostname()
//   process → env, cwd(), argv, version, exit(), platform, arch, pid
//
// All filesystem operations are sandboxed to the extension's allowed paths.
// Hard-fail on any rejected module (child_process, net, http, etc.).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "quickjs_extension_host.h"

extern "C" {
#include "quickjs/quickjs.h"
}

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

// ============================================================================
// Rejected Modules — Hard-fail on require()
// ============================================================================

namespace quickjs_shims {

const char* const REJECTED_MODULES[] = {
    "child_process",
    "node:child_process",
    "net",
    "node:net",
    "http",
    "node:http",
    "https",
    "node:https",
    "http2",
    "node:http2",
    "dgram",
    "node:dgram",
    "tls",
    "node:tls",
    "cluster",
    "node:cluster",
    "worker_threads",
    "node:worker_threads",
    "vm",
    "node:vm",
    "v8",
    "node:v8",
    "async_hooks",
    "node:async_hooks",
    "perf_hooks",
    "node:perf_hooks",
    "inspector",
    "node:inspector",
    "dns",
    "node:dns",
    "readline",
    "node:readline",
    "repl",
    "node:repl",
    "stream",
    "node:stream",
    "zlib",
    "node:zlib",
    "crypto",       // Could be shimmed in future with a pure-JS crypto lib
    "node:crypto"
};

const size_t REJECTED_MODULES_COUNT = sizeof(REJECTED_MODULES) / sizeof(REJECTED_MODULES[0]);

// ============================================================================
// Path Sandbox Validation
// ============================================================================

bool isPathAllowed(const std::filesystem::path& candidate,
                   const std::vector<std::filesystem::path>& allowedPaths) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) return false;

    for (const auto& allowed : allowedPaths) {
        auto canonAllowed = std::filesystem::weakly_canonical(allowed, ec);
        if (ec) continue;

        auto rel = std::filesystem::relative(canonical, canonAllowed, ec);
        if (ec) continue;

        std::string relStr = rel.string();
        if (!relStr.empty() && relStr.substr(0, 2) != "..") {
            return true;
        }
    }

    return false;
}

// Helper: Get sandbox config from context opaque
static const QuickJSSandboxConfig* getSandbox(JSContext* ctx) {
    auto* rt = static_cast<QuickJSExtensionRuntime*>(JS_GetContextOpaque(ctx));
    return rt ? &rt->sandbox : nullptr;
}

static const std::string& getExtensionPath(JSContext* ctx) {
    static const std::string empty;
    auto* rt = static_cast<QuickJSExtensionRuntime*>(JS_GetContextOpaque(ctx));
    return rt ? rt->extensionPath : empty;
}

// ============================================================================
// fs Module — Sandboxed File System Operations
// ============================================================================

static JSValue js_fs_readFileSync(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.readFileSync: path required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.readFileSync: invalid path");

    std::filesystem::path filePath(pathStr);
    JS_FreeCString(ctx, pathStr);

    // Resolve relative paths against extension path
    if (filePath.is_relative()) {
        filePath = std::filesystem::path(getExtensionPath(ctx)) / filePath;
    }

    // Sandbox check
    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(filePath, sandbox->allowedReadPaths)) {
        return JS_ThrowTypeError(ctx,
            "fs.readFileSync: path '%s' is outside sandbox boundaries",
            filePath.string().c_str());
    }

    std::ifstream infile(filePath, std::ios::binary);
    if (!infile.is_open()) {
        return JS_ThrowInternalError(ctx,
            "ENOENT: no such file or directory, open '%s'", filePath.string().c_str());
    }

    std::string content((std::istreambuf_iterator<char>(infile)),
                         std::istreambuf_iterator<char>());
    infile.close();

    // Check encoding option
    std::string encoding;
    if (argc >= 2) {
        if (JS_IsString(argv[1])) {
            const char* enc = JS_ToCString(ctx, argv[1]);
            if (enc) { encoding = enc; JS_FreeCString(ctx, enc); }
        } else if (JS_IsObject(argv[1])) {
            JSValue encVal = JS_GetPropertyStr(ctx, argv[1], "encoding");
            if (JS_IsString(encVal)) {
                const char* enc = JS_ToCString(ctx, encVal);
                if (enc) { encoding = enc; JS_FreeCString(ctx, enc); }
            }
            JS_FreeValue(ctx, encVal);
        }
    }

    if (encoding == "utf8" || encoding == "utf-8" || !encoding.empty()) {
        return JS_NewStringLen(ctx, content.c_str(), content.length());
    }

    // Return as Buffer (Uint8Array)
    JSValue arrayBuffer = JS_NewArrayBufferCopy(ctx,
        reinterpret_cast<const uint8_t*>(content.data()), content.size());
    return arrayBuffer;
}

static JSValue js_fs_writeFileSync(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "fs.writeFileSync: path and data required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.writeFileSync: invalid path");

    std::filesystem::path filePath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (filePath.is_relative()) {
        filePath = std::filesystem::path(getExtensionPath(ctx)) / filePath;
    }

    // Sandbox check — write paths
    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(filePath, sandbox->allowedWritePaths)) {
        return JS_ThrowTypeError(ctx,
            "fs.writeFileSync: path '%s' is outside writable sandbox",
            filePath.string().c_str());
    }

    // Get data as string or buffer
    const char* dataStr = JS_ToCString(ctx, argv[1]);
    if (!dataStr) return JS_ThrowTypeError(ctx, "fs.writeFileSync: invalid data");

    std::string data(dataStr);
    JS_FreeCString(ctx, dataStr);

    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(filePath.parent_path(), ec);

    std::ofstream outfile(filePath, std::ios::binary);
    if (!outfile.is_open()) {
        return JS_ThrowInternalError(ctx,
            "EACCES: permission denied, open '%s'", filePath.string().c_str());
    }

    outfile.write(data.c_str(), data.length());
    outfile.close();

    return JS_UNDEFINED;
}

static JSValue js_fs_existsSync(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    if (argc < 1) return JS_FALSE;

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_FALSE;

    std::filesystem::path filePath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (filePath.is_relative()) {
        filePath = std::filesystem::path(getExtensionPath(ctx)) / filePath;
    }

    // Sandbox check (read)
    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(filePath, sandbox->allowedReadPaths)) {
        return JS_FALSE; // Silently deny — matches Node.js behavior (returns false)
    }

    return std::filesystem::exists(filePath) ? JS_TRUE : JS_FALSE;
}

static JSValue js_fs_readdirSync(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.readdirSync: path required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.readdirSync: invalid path");

    std::filesystem::path dirPath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (dirPath.is_relative()) {
        dirPath = std::filesystem::path(getExtensionPath(ctx)) / dirPath;
    }

    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(dirPath, sandbox->allowedReadPaths)) {
        return JS_ThrowTypeError(ctx,
            "fs.readdirSync: path '%s' is outside sandbox", dirPath.string().c_str());
    }

    if (!std::filesystem::is_directory(dirPath)) {
        return JS_ThrowInternalError(ctx,
            "ENOTDIR: not a directory, scandir '%s'", dirPath.string().c_str());
    }

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec)) {
        std::string name = entry.path().filename().string();
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, name.c_str()));
    }

    return arr;
}

static JSValue js_fs_statSync(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.statSync: path required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.statSync: invalid path");

    std::filesystem::path filePath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (filePath.is_relative()) {
        filePath = std::filesystem::path(getExtensionPath(ctx)) / filePath;
    }

    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(filePath, sandbox->allowedReadPaths)) {
        return JS_ThrowTypeError(ctx, "fs.statSync: outside sandbox");
    }

    std::error_code ec;
    auto status = std::filesystem::status(filePath, ec);
    if (ec) {
        return JS_ThrowInternalError(ctx,
            "ENOENT: no such file or directory, stat '%s'", filePath.string().c_str());
    }

    JSValue stat = JS_NewObject(ctx);

    bool isFile = std::filesystem::is_regular_file(status);
    bool isDir  = std::filesystem::is_directory(status);

    JS_SetPropertyStr(ctx, stat, "isFile", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            JSValue val = JS_GetPropertyStr(ctx, this_val, "_isFile");
            return val;
        }, "isFile", 0));
    JS_SetPropertyStr(ctx, stat, "_isFile", isFile ? JS_TRUE : JS_FALSE);

    JS_SetPropertyStr(ctx, stat, "isDirectory", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            JSValue val = JS_GetPropertyStr(ctx, this_val, "_isDir");
            return val;
        }, "isDirectory", 0));
    JS_SetPropertyStr(ctx, stat, "_isDir", isDir ? JS_TRUE : JS_FALSE);

    // File size
    if (isFile) {
        auto fileSize = std::filesystem::file_size(filePath, ec);
        JS_SetPropertyStr(ctx, stat, "size",
                          JS_NewFloat64(ctx, static_cast<double>(fileSize)));
    } else {
        JS_SetPropertyStr(ctx, stat, "size", JS_NewInt32(ctx, 0));
    }

    return stat;
}

static JSValue js_fs_mkdirSync(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.mkdirSync: path required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.mkdirSync: invalid path");

    std::filesystem::path dirPath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (dirPath.is_relative()) {
        dirPath = std::filesystem::path(getExtensionPath(ctx)) / dirPath;
    }

    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(dirPath, sandbox->allowedWritePaths)) {
        return JS_ThrowTypeError(ctx, "fs.mkdirSync: outside writable sandbox");
    }

    // Check for { recursive: true } option
    bool recursive = false;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue rec = JS_GetPropertyStr(ctx, argv[1], "recursive");
        recursive = JS_ToBool(ctx, rec);
        JS_FreeValue(ctx, rec);
    }

    std::error_code ec;
    if (recursive) {
        std::filesystem::create_directories(dirPath, ec);
    } else {
        std::filesystem::create_directory(dirPath, ec);
    }

    if (ec) {
        return JS_ThrowInternalError(ctx, "EACCES: mkdir '%s' failed: %s",
                                      dirPath.string().c_str(), ec.message().c_str());
    }

    return JS_UNDEFINED;
}

static JSValue js_fs_unlinkSync(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.unlinkSync: path required");

    const char* pathStr = JS_ToCString(ctx, argv[0]);
    if (!pathStr) return JS_ThrowTypeError(ctx, "fs.unlinkSync: invalid path");

    std::filesystem::path filePath(pathStr);
    JS_FreeCString(ctx, pathStr);

    if (filePath.is_relative()) {
        filePath = std::filesystem::path(getExtensionPath(ctx)) / filePath;
    }

    const auto* sandbox = getSandbox(ctx);
    if (sandbox && !isPathAllowed(filePath, sandbox->allowedWritePaths)) {
        return JS_ThrowTypeError(ctx, "fs.unlinkSync: outside writable sandbox");
    }

    std::error_code ec;
    if (!std::filesystem::remove(filePath, ec)) {
        return JS_ThrowInternalError(ctx, "ENOENT: unlink '%s' failed",
                                      filePath.string().c_str());
    }

    return JS_UNDEFINED;
}

bool registerFS(JSContext* ctx, const QuickJSSandboxConfig& sandbox,
                const std::string& extensionPath) {
    JSValue fsModule = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, fsModule, "readFileSync",
                      JS_NewCFunction(ctx, js_fs_readFileSync, "readFileSync", 2));
    JS_SetPropertyStr(ctx, fsModule, "writeFileSync",
                      JS_NewCFunction(ctx, js_fs_writeFileSync, "writeFileSync", 3));
    JS_SetPropertyStr(ctx, fsModule, "existsSync",
                      JS_NewCFunction(ctx, js_fs_existsSync, "existsSync", 1));
    JS_SetPropertyStr(ctx, fsModule, "readdirSync",
                      JS_NewCFunction(ctx, js_fs_readdirSync, "readdirSync", 2));
    JS_SetPropertyStr(ctx, fsModule, "statSync",
                      JS_NewCFunction(ctx, js_fs_statSync, "statSync", 1));
    JS_SetPropertyStr(ctx, fsModule, "mkdirSync",
                      JS_NewCFunction(ctx, js_fs_mkdirSync, "mkdirSync", 2));
    JS_SetPropertyStr(ctx, fsModule, "unlinkSync",
                      JS_NewCFunction(ctx, js_fs_unlinkSync, "unlinkSync", 1));

    // Async wrapper: readFile(path, opts, callback) → calls readFileSync internally
    JS_SetPropertyStr(ctx, fsModule, "readFile", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            // Simplified async: call sync version and resolve via callback/promise
            if (argc < 2) return JS_ThrowTypeError(ctx, "fs.readFile: path and callback required");

            // If last arg is a function, use callback pattern
            JSValue callback = argc >= 3 ? argv[2] : (argc >= 2 && JS_IsFunction(ctx, argv[1]) ? argv[1] : JS_UNDEFINED);
            if (JS_IsFunction(ctx, callback)) {
                JSValue result = js_fs_readFileSync(ctx, this_val, argc - 1, argv);
                if (JS_IsException(result)) {
                    JSValue exc = JS_GetException(ctx);
                    JSValue args[2] = { exc, JS_UNDEFINED };
                    JS_Call(ctx, callback, JS_UNDEFINED, 2, args);
                    JS_FreeValue(ctx, exc);
                } else {
                    JSValue args[2] = { JS_NULL, result };
                    JS_Call(ctx, callback, JS_UNDEFINED, 2, args);
                }
                JS_FreeValue(ctx, result);
            }
            return JS_UNDEFINED;
        }, "readFile", 3));

    // Store in global for require()
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__rawrxd_shim_fs", fsModule);
    JS_FreeValue(ctx, global);

    return true;
}

// ============================================================================
// path Module — Pure Computation (No Sandbox Needed)
// ============================================================================

bool registerPath(JSContext* ctx) {
    JSValue pathModule = JS_NewObject(ctx);

    // path.sep
    JS_SetPropertyStr(ctx, pathModule, "sep", JS_NewString(ctx, "\\"));

    // path.delimiter
    JS_SetPropertyStr(ctx, pathModule, "delimiter", JS_NewString(ctx, ";"));

    // path.posix.sep / path.win32.sep
    JSValue posix = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, posix, "sep", JS_NewString(ctx, "/"));
    JS_SetPropertyStr(ctx, posix, "delimiter", JS_NewString(ctx, ":"));
    JS_SetPropertyStr(ctx, pathModule, "posix", posix);

    JSValue win32 = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, win32, "sep", JS_NewString(ctx, "\\"));
    JS_SetPropertyStr(ctx, win32, "delimiter", JS_NewString(ctx, ";"));
    JS_SetPropertyStr(ctx, pathModule, "win32", win32);

    // path.join(...args) → string
    JS_SetPropertyStr(ctx, pathModule, "join", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            std::filesystem::path result;
            for (int i = 0; i < argc; i++) {
                const char* seg = JS_ToCString(ctx, argv[i]);
                if (seg) {
                    if (i == 0) result = seg;
                    else result /= seg;
                    JS_FreeCString(ctx, seg);
                }
            }
            std::string normalized = result.lexically_normal().string();
            return JS_NewString(ctx, normalized.c_str());
        }, "join", 0));

    // path.resolve(...args) → absolute string
    JS_SetPropertyStr(ctx, pathModule, "resolve", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            std::filesystem::path result = std::filesystem::current_path();
            for (int i = 0; i < argc; i++) {
                const char* seg = JS_ToCString(ctx, argv[i]);
                if (seg) {
                    std::filesystem::path p(seg);
                    if (p.is_absolute()) {
                        result = p;
                    } else {
                        result /= p;
                    }
                    JS_FreeCString(ctx, seg);
                }
            }
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(result, ec);
            if (!ec) result = canonical;
            return JS_NewString(ctx, result.string().c_str());
        }, "resolve", 0));

    // path.dirname(p) → string
    JS_SetPropertyStr(ctx, pathModule, "dirname", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_NewString(ctx, ".");
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_NewString(ctx, ".");
            std::string result = std::filesystem::path(p).parent_path().string();
            JS_FreeCString(ctx, p);
            if (result.empty()) result = ".";
            return JS_NewString(ctx, result.c_str());
        }, "dirname", 1));

    // path.basename(p, ext?) → string
    JS_SetPropertyStr(ctx, pathModule, "basename", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_NewString(ctx, "");
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_NewString(ctx, "");
            std::string filename = std::filesystem::path(p).filename().string();
            JS_FreeCString(ctx, p);

            // If ext argument provided, strip it from the result
            if (argc >= 2) {
                const char* ext = JS_ToCString(ctx, argv[1]);
                if (ext) {
                    std::string extStr(ext);
                    JS_FreeCString(ctx, ext);
                    if (filename.length() > extStr.length() &&
                        filename.substr(filename.length() - extStr.length()) == extStr) {
                        filename = filename.substr(0, filename.length() - extStr.length());
                    }
                }
            }

            return JS_NewString(ctx, filename.c_str());
        }, "basename", 2));

    // path.extname(p) → string
    JS_SetPropertyStr(ctx, pathModule, "extname", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_NewString(ctx, "");
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_NewString(ctx, "");
            std::string ext = std::filesystem::path(p).extension().string();
            JS_FreeCString(ctx, p);
            return JS_NewString(ctx, ext.c_str());
        }, "extname", 1));

    // path.normalize(p) → string
    JS_SetPropertyStr(ctx, pathModule, "normalize", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_NewString(ctx, ".");
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_NewString(ctx, ".");
            std::string result = std::filesystem::path(p).lexically_normal().string();
            JS_FreeCString(ctx, p);
            return JS_NewString(ctx, result.c_str());
        }, "normalize", 1));

    // path.isAbsolute(p) → bool
    JS_SetPropertyStr(ctx, pathModule, "isAbsolute", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_FALSE;
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_FALSE;
            bool abs = std::filesystem::path(p).is_absolute();
            JS_FreeCString(ctx, p);
            return abs ? JS_TRUE : JS_FALSE;
        }, "isAbsolute", 1));

    // path.relative(from, to) → string
    JS_SetPropertyStr(ctx, pathModule, "relative", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 2) return JS_NewString(ctx, "");
            const char* from = JS_ToCString(ctx, argv[0]);
            const char* to   = JS_ToCString(ctx, argv[1]);
            if (!from || !to) {
                if (from) JS_FreeCString(ctx, from);
                if (to) JS_FreeCString(ctx, to);
                return JS_NewString(ctx, "");
            }
            std::error_code ec;
            auto result = std::filesystem::relative(
                std::filesystem::path(to), std::filesystem::path(from), ec);
            JS_FreeCString(ctx, from);
            JS_FreeCString(ctx, to);
            return JS_NewString(ctx, result.string().c_str());
        }, "relative", 2));

    // path.parse(p) → { root, dir, base, ext, name }
    JS_SetPropertyStr(ctx, pathModule, "parse", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1) return JS_NewObject(ctx);
            const char* p = JS_ToCString(ctx, argv[0]);
            if (!p) return JS_NewObject(ctx);

            std::filesystem::path fp(p);
            JS_FreeCString(ctx, p);

            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "root", JS_NewString(ctx, fp.root_path().string().c_str()));
            JS_SetPropertyStr(ctx, obj, "dir", JS_NewString(ctx, fp.parent_path().string().c_str()));
            JS_SetPropertyStr(ctx, obj, "base", JS_NewString(ctx, fp.filename().string().c_str()));
            JS_SetPropertyStr(ctx, obj, "ext", JS_NewString(ctx, fp.extension().string().c_str()));
            JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, fp.stem().string().c_str()));
            return obj;
        }, "parse", 1));

    // Store in global for require()
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__rawrxd_shim_path", pathModule);
    JS_FreeValue(ctx, global);

    return true;
}

// ============================================================================
// os Module — System Information (Read-Only, Minimal)
// ============================================================================

bool registerOS(JSContext* ctx) {
    JSValue osModule = JS_NewObject(ctx);

    // os.platform() → "win32"
    JS_SetPropertyStr(ctx, osModule, "platform", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_NewString(ctx, "win32");
        }, "platform", 0));

    // os.arch() → "x64"
    JS_SetPropertyStr(ctx, osModule, "arch", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
#ifdef _M_X64
            return JS_NewString(ctx, "x64");
#elif defined(_M_ARM64)
            return JS_NewString(ctx, "arm64");
#else
            return JS_NewString(ctx, "ia32");
#endif
        }, "arch", 0));

    // os.homedir() → user home directory
    JS_SetPropertyStr(ctx, osModule, "homedir", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            const char* home = getenv("USERPROFILE");
            if (!home) home = getenv("HOME");
            if (!home) home = "C:\\Users\\Default";
            return JS_NewString(ctx, home);
        }, "homedir", 0));

    // os.tmpdir() → temp directory
    JS_SetPropertyStr(ctx, osModule, "tmpdir", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            wchar_t tmpPath[MAX_PATH];
            DWORD len = GetTempPathW(MAX_PATH, tmpPath);
            if (len > 0) {
                int utf8Len = WideCharToMultiByte(CP_UTF8, 0, tmpPath, len, nullptr, 0, nullptr, nullptr);
                std::string tmp(utf8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, tmpPath, len, tmp.data(), utf8Len, nullptr, nullptr);
                // Remove trailing backslash
                while (!tmp.empty() && (tmp.back() == '\\' || tmp.back() == '\0')) tmp.pop_back();
                return JS_NewString(ctx, tmp.c_str());
            }
            return JS_NewString(ctx, "C:\\Temp");
        }, "tmpdir", 0));

    // os.hostname()
    JS_SetPropertyStr(ctx, osModule, "hostname", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            char name[256];
            DWORD size = sizeof(name);
            if (GetComputerNameA(name, &size)) {
                return JS_NewString(ctx, name);
            }
            return JS_NewString(ctx, "localhost");
        }, "hostname", 0));

    // os.EOL
    JS_SetPropertyStr(ctx, osModule, "EOL", JS_NewString(ctx, "\r\n"));

    // os.type()
    JS_SetPropertyStr(ctx, osModule, "type", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_NewString(ctx, "Windows_NT");
        }, "type", 0));

    // os.release()
    JS_SetPropertyStr(ctx, osModule, "release", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            return JS_NewString(ctx, "10.0.0"); // Approximate
        }, "release", 0));

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__rawrxd_shim_os", osModule);
    JS_FreeValue(ctx, global);

    return true;
}

// ============================================================================
// process Module — Process Information & Environment
// ============================================================================

bool registerProcess(JSContext* ctx, const std::string& extensionPath) {
    JSValue processModule = JS_NewObject(ctx);

    // process.platform
    JS_SetPropertyStr(ctx, processModule, "platform", JS_NewString(ctx, "win32"));

    // process.arch
#ifdef _M_X64
    JS_SetPropertyStr(ctx, processModule, "arch", JS_NewString(ctx, "x64"));
#elif defined(_M_ARM64)
    JS_SetPropertyStr(ctx, processModule, "arch", JS_NewString(ctx, "arm64"));
#else
    JS_SetPropertyStr(ctx, processModule, "arch", JS_NewString(ctx, "ia32"));
#endif

    // process.version (fake — we're QuickJS, not Node)
    JS_SetPropertyStr(ctx, processModule, "version", JS_NewString(ctx, "v18.0.0"));

    // process.versions (minimal)
    JSValue versions = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, versions, "node", JS_NewString(ctx, "18.0.0"));
    JS_SetPropertyStr(ctx, versions, "v8", JS_NewString(ctx, "0.0.0"));
    JS_SetPropertyStr(ctx, versions, "quickjs", JS_NewString(ctx, "2024.1.0"));
    JS_SetPropertyStr(ctx, processModule, "versions", versions);

    // process.pid
    JS_SetPropertyStr(ctx, processModule, "pid",
                      JS_NewInt32(ctx, static_cast<int32_t>(GetCurrentProcessId())));

    // process.argv (fake — extensions shouldn't depend on this)
    JSValue argv = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, argv, 0, JS_NewString(ctx, "rawrxd"));
    JS_SetPropertyStr(ctx, processModule, "argv", argv);

    // process.cwd()
    JS_SetPropertyStr(ctx, processModule, "cwd", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            auto* rt = static_cast<QuickJSExtensionRuntime*>(JS_GetContextOpaque(ctx));
            if (rt && !rt->extensionPath.empty()) {
                return JS_NewString(ctx, rt->extensionPath.c_str());
            }
            return JS_NewString(ctx, std::filesystem::current_path().string().c_str());
        }, "cwd", 0));

    // process.env — Expose a subset of environment variables
    // Security: Only expose safe environment variables, not secrets
    JSValue env = JS_NewObject(ctx);

    auto safeEnvVar = [&](const char* name) {
        const char* val = getenv(name);
        if (val) {
            JS_SetPropertyStr(ctx, env, name, JS_NewString(ctx, val));
        }
    };

    safeEnvVar("HOME");
    safeEnvVar("USERPROFILE");
    safeEnvVar("TEMP");
    safeEnvVar("TMP");
    safeEnvVar("PATH");
    safeEnvVar("LANG");
    safeEnvVar("LC_ALL");
    safeEnvVar("SHELL");
    safeEnvVar("ComSpec");
    safeEnvVar("SystemRoot");
    safeEnvVar("ProgramFiles");
    safeEnvVar("APPDATA");
    safeEnvVar("LOCALAPPDATA");
    // Expose extension-specific vars
    JS_SetPropertyStr(ctx, env, "RAWRXD_EXTENSION_HOST", JS_NewString(ctx, "quickjs"));
    JS_SetPropertyStr(ctx, env, "RAWRXD_EXTENSION_PATH",
                      JS_NewString(ctx, extensionPath.c_str()));
    JS_SetPropertyStr(ctx, processModule, "env", env);

    // process.exit() → triggers extension shutdown (does NOT exit the IDE)
    JS_SetPropertyStr(ctx, processModule, "exit", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            auto* rt = static_cast<QuickJSExtensionRuntime*>(JS_GetContextOpaque(ctx));
            if (rt) {
                rt->running.store(false, std::memory_order_release);
                OutputDebugStringA(("[JS:process.exit] Extension '" +
                                    rt->extensionId + "' called process.exit()\n").c_str());
            }
            return JS_ThrowInternalError(ctx, "Extension shutdown via process.exit()");
        }, "exit", 1));

    // process.nextTick(fn) → execute fn after current microtask
    JS_SetPropertyStr(ctx, processModule, "nextTick", JS_NewCFunction(ctx,
        [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
            if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
                return JS_ThrowTypeError(ctx, "process.nextTick requires a function");
            }
            // Enqueue as a resolved promise to execute in the next microtask
            JSValue resolve = JS_NewCFunction(ctx,
                [](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
                    return JS_UNDEFINED;
                }, "resolve", 0);

            JSValue promise = JS_CallConstructor(ctx,
                JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Promise"),
                1, &resolve);
            if (!JS_IsException(promise)) {
                JSValue then = JS_GetPropertyStr(ctx, promise, "then");
                JS_Call(ctx, then, promise, 1, const_cast<JSValueConst*>(argv));
                JS_FreeValue(ctx, then);
            }
            JS_FreeValue(ctx, promise);
            JS_FreeValue(ctx, resolve);
            return JS_UNDEFINED;
        }, "nextTick", 1));

    // Store in global for require() AND as globalThis.process
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__rawrxd_shim_process", JS_DupValue(ctx, processModule));
    JS_SetPropertyStr(ctx, global, "process", processModule);
    JS_FreeValue(ctx, global);

    return true;
}

// ============================================================================
// Register All Shims
// ============================================================================

bool registerAllShims(JSContext* ctx, const QuickJSSandboxConfig& sandbox,
                      const std::string& extensionPath) {
    bool ok = true;

    ok = ok && registerFS(ctx, sandbox, extensionPath);
    ok = ok && registerPath(ctx);
    ok = ok && registerOS(ctx);
    ok = ok && registerProcess(ctx, extensionPath);

    return ok;
}

} // namespace quickjs_shims
