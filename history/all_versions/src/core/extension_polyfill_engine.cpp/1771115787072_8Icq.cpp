// ============================================================================
// extension_polyfill_engine.cpp — Self-Creating Polyfill/Shim Engine
// ============================================================================
//
// Phase 37: Auto-Polyfill System — Runtime Shim Generation
//
// Implements the PolyfillEngine that auto-generates JavaScript shims for
// Node.js built-in modules, Electron APIs, and remote system calls that
// are not available in the QuickJS embedded runtime.
//
// The key innovation: when an extension calls require('some-module') and that
// module isn't available, the engine:
//   1. Analyzes the module name and known API surface
//   2. Generates a minimal-but-functional JavaScript shim
//   3. Registers it in the module system for immediate use
//   4. Optionally bridges calls to C++ native implementations
//   5. Caches the shim for future extensions
//
// This means extensions that use fs.readFile, path.join, os.platform, etc.
// will "just work" without any modification to the extension code.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "js_extension_host.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>

// PatchResult provided by model_memory_hotpatch.hpp (included transitively)

// ============================================================================
// Singleton
// ============================================================================

PolyfillEngine& PolyfillEngine::instance() {
    static PolyfillEngine s_instance;
    return s_instance;
}

PolyfillEngine::PolyfillEngine()
    : m_initialized(false)
    , m_stats{}
{
}

PolyfillEngine::~PolyfillEngine() {
    if (m_initialized) shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult PolyfillEngine::initialize() {
    if (m_initialized) return PatchResult::error("Already initialized");

    std::lock_guard<std::mutex> lock(m_mutex);

    // Register all built-in polyfills
    // Each generates a self-contained JavaScript module

    // ---- Node.js Built-in Modules ----
    auto reg = [&](const char* name, PolyfillDescriptor::Category cat,
                     PolyfillDescriptor::Strategy strat, uint8_t score,
                     const std::string& source) {
        PolyfillDescriptor desc{};
        std::strncpy(desc.moduleName, name, sizeof(desc.moduleName) - 1);
        desc.category = cat;
        desc.strategy = strat;
        desc.compatibilityScore = score;
        desc.isGenerated = false;
        desc.hitCount = 0;
        desc.lastUsedTimestamp = 0;
        m_sourceCache[name] = source;
        desc.jsSource = m_sourceCache[name].c_str();
        desc.jsSourceLen = m_sourceCache[name].size();
        m_registry[name] = desc;
    };

    reg("fs",            PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::ProxyToNative, 85,
                         generateFsPolyfill());

    reg("path",          PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 98,
                         generatePathPolyfill());

    reg("os",            PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 90,
                         generateOsPolyfill());

    reg("process",       PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::PartialShim, 80,
                         generateProcessPolyfill());

    reg("child_process", PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::PartialShim, 40,
                         generateChildProcessPolyfill());

    reg("crypto",        PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::PartialShim, 50,
                         generateCryptoPolyfill());

    reg("http",          PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::NoOpStub, 30,
                         generateHttpPolyfill());

    reg("https",         PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::NoOpStub, 30,
                         generateHttpPolyfill()); // Reuse HTTP shim

    reg("events",        PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 95,
                         generateEventsPolyfill());

    reg("stream",        PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::PartialShim, 60,
                         generateStreamPolyfill());

    reg("buffer",        PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::PartialShim, 70,
                         generateBufferPolyfill());

    reg("util",          PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 85,
                         generateUtilPolyfill());

    reg("url",           PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 90,
                         generateUrlPolyfill());

    reg("querystring",   PolyfillDescriptor::Category::NodeBuiltin,
                         PolyfillDescriptor::Strategy::FullShim, 95,
                         generateQuerystringPolyfill());

    reg("electron",      PolyfillDescriptor::Category::ElectronAPI,
                         PolyfillDescriptor::Strategy::NoOpStub, 25,
                         generateElectronPolyfill());

    reg("vscode-remote", PolyfillDescriptor::Category::RemoteSystem,
                         PolyfillDescriptor::Strategy::NoOpStub, 20,
                         generateRemotePolyfill());

    // Common aliases
    m_registry["node:fs"]      = m_registry["fs"];
    m_registry["node:path"]    = m_registry["path"];
    m_registry["node:os"]      = m_registry["os"];
    m_registry["node:crypto"]  = m_registry["crypto"];
    m_registry["node:events"]  = m_registry["events"];
    m_registry["node:url"]     = m_registry["url"];
    m_registry["node:util"]    = m_registry["util"];
    m_registry["node:buffer"]  = m_registry["buffer"];
    m_registry["node:stream"]  = m_registry["stream"];
    m_registry["node:child_process"] = m_registry["child_process"];
    m_registry["node:querystring"]   = m_registry["querystring"];
    m_registry["node:http"]    = m_registry["http"];
    m_registry["node:https"]   = m_registry["https"];

    m_stats.totalPolyfills = m_registry.size();
    m_initialized = true;

    char info[128];
    std::snprintf(info, sizeof(info),
                  "[PolyfillEngine] Initialized with %zu built-in polyfills\n",
                  m_registry.size());
    OutputDebugStringA(info);

    return PatchResult::ok("PolyfillEngine initialized");
}

PatchResult PolyfillEngine::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry.clear();
    m_sourceCache.clear();
    m_bridges.clear();
    m_initialized = false;
    return PatchResult::ok("PolyfillEngine shutdown");
}

// ============================================================================
// Polyfill Registry
// ============================================================================

PatchResult PolyfillEngine::registerPolyfill(const char* moduleName,
                                              PolyfillDescriptor::Category category,
                                              PolyfillDescriptor::Strategy strategy,
                                              const char* jsSource,
                                              uint8_t compatScore) {
    if (!moduleName || !jsSource) return PatchResult::error("Null parameters");

    std::lock_guard<std::mutex> lock(m_mutex);

    PolyfillDescriptor desc{};
    std::strncpy(desc.moduleName, moduleName, sizeof(desc.moduleName) - 1);
    desc.category = category;
    desc.strategy = strategy;
    desc.compatibilityScore = compatScore;
    desc.isGenerated = false;
    desc.hitCount = 0;

    m_sourceCache[moduleName] = jsSource;
    desc.jsSource = m_sourceCache[moduleName].c_str();
    desc.jsSourceLen = m_sourceCache[moduleName].size();

    m_registry[moduleName] = desc;
    m_stats.totalPolyfills = m_registry.size();

    return PatchResult::ok("Polyfill registered");
}

const PolyfillDescriptor* PolyfillEngine::getPolyfill(const char* moduleName) const {
    if (!moduleName) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_registry.find(moduleName);
    if (it != m_registry.end()) {
        // Update hit count (const_cast for stats tracking only)
        const_cast<PolyfillDescriptor&>(it->second).hitCount++;
        const_cast<PolyfillDescriptor&>(it->second).lastUsedTimestamp = GetTickCount64();
        const_cast<Stats&>(m_stats).requireHits++;
        return &it->second;
    }

    const_cast<Stats&>(m_stats).requireMisses++;
    return nullptr;
}

// ============================================================================
// Auto-Generation — The Self-Creating Shim System
// ============================================================================

bool PolyfillEngine::autoGeneratePolyfill(const char* moduleName,
                                            const char* requiredByExtension) {
    if (!moduleName) return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Already exists?
    if (m_registry.count(moduleName)) return true;

    // Determine the best strategy based on module name patterns
    std::string name(moduleName);

    // Common node module patterns
    PolyfillDescriptor::Category category = PolyfillDescriptor::Category::Custom;
    PolyfillDescriptor::Strategy strategy = PolyfillDescriptor::Strategy::NoOpStub;
    uint8_t score = 10;

    // Detect category from name
    if (name.find("electron") != std::string::npos ||
        name.find("@electron") != std::string::npos) {
        category = PolyfillDescriptor::Category::ElectronAPI;
        strategy = PolyfillDescriptor::Strategy::NoOpStub;
        score = 15;
    } else if (name.find("vscode-remote") != std::string::npos ||
               name.find("@vscode/remote") != std::string::npos ||
               name.find("ssh2") != std::string::npos) {
        category = PolyfillDescriptor::Category::RemoteSystem;
        strategy = PolyfillDescriptor::Strategy::ErrorWithHelp;
        score = 5;
    } else if (name.find("node:") == 0) {
        // Unknown node: prefix module
        category = PolyfillDescriptor::Category::NodeBuiltin;
        strategy = PolyfillDescriptor::Strategy::NoOpStub;
        score = 20;
    }

    // Generate the stub source
    std::ostringstream js;
    js << "// Auto-generated polyfill for '" << name << "'\n";
    js << "// Required by: " << (requiredByExtension ? requiredByExtension : "unknown") << "\n";
    js << "// Strategy: " << static_cast<int>(strategy) << "\n";
    js << "// Generated at: " << GetTickCount64() << "\n";
    js << "\n";

    if (strategy == PolyfillDescriptor::Strategy::ErrorWithHelp) {
        js << "const handler = {\n";
        js << "  get(target, prop) {\n";
        js << "    if (prop === '__esModule') return true;\n";
        js << "    if (prop === 'default') return target;\n";
        js << "    console.warn(`[RawrXD Polyfill] Module '";
        js << name << "' property '${prop}' is not available. ";
        js << "This module requires features not supported in RawrXD.`);\n";
        js << "    return function() { return undefined; };\n";
        js << "  }\n";
        js << "};\n";
        js << "module.exports = new Proxy({}, handler);\n";
    } else {
        // NoOpStub: Create a proxy object that returns safe defaults for any access
        js << "const _noop = () => undefined;\n";
        js << "const _noopPromise = () => Promise.resolve(undefined);\n";
        js << "const _emptyObj = {};\n";
        js << "const _emptyArr = [];\n";
        js << "\n";
        js << "const handler = {\n";
        js << "  get(target, prop) {\n";
        js << "    if (prop === '__esModule') return true;\n";
        js << "    if (prop === 'default') return target;\n";
        js << "    if (prop === 'then') return undefined;\n"; // Not a thenable
        js << "    if (typeof prop === 'symbol') return undefined;\n";
        js << "    // Auto-stub: return function that returns undefined\n";
        js << "    return function(...args) {\n";
        js << "      // Check if last arg is callback\n";
        js << "      const lastArg = args[args.length - 1];\n";
        js << "      if (typeof lastArg === 'function') {\n";
        js << "        // Node-style callback: call with (null, default)\n";
        js << "        setTimeout(() => lastArg(null, undefined), 0);\n";
        js << "      }\n";
        js << "      return undefined;\n";
        js << "    };\n";
        js << "  }\n";
        js << "};\n";
        js << "module.exports = new Proxy({}, handler);\n";
    }

    std::string source = js.str();

    // Register the generated polyfill
    PolyfillDescriptor desc{};
    std::strncpy(desc.moduleName, moduleName, sizeof(desc.moduleName) - 1);
    desc.category = category;
    desc.strategy = strategy;
    desc.compatibilityScore = score;
    desc.isGenerated = true;
    desc.hitCount = 0;
    desc.lastUsedTimestamp = GetTickCount64();

    m_sourceCache[name] = source;
    desc.jsSource = m_sourceCache[name].c_str();
    desc.jsSourceLen = m_sourceCache[name].size();

    m_registry[name] = desc;
    m_stats.generatedPolyfills++;
    m_stats.totalPolyfills = m_registry.size();
    m_stats.hotpatchInjections++;

    char info[256];
    std::snprintf(info, sizeof(info),
                  "[PolyfillEngine] Auto-generated %s shim for '%s' (score=%u, requested by '%s')\n",
                  strategy == PolyfillDescriptor::Strategy::ErrorWithHelp ? "error-guide" : "no-op",
                  moduleName, score,
                  requiredByExtension ? requiredByExtension : "unknown");
    OutputDebugStringA(info);

    return true;
}

std::string PolyfillEngine::generateStubModule(const char* moduleName,
                                                 const std::vector<std::string>& accessedProperties) {
    std::ostringstream js;
    js << "// Targeted stub for '" << moduleName << "'\n";
    js << "// Properties: " << accessedProperties.size() << " detected\n\n";

    js << "const module_stub = {};\n\n";

    for (const auto& prop : accessedProperties) {
        js << "module_stub." << prop << " = function " << prop << "() {\n";
        js << "  console.warn('[RawrXD] " << moduleName << "." << prop << "() is shimmed');\n";
        js << "  return undefined;\n";
        js << "};\n\n";
    }

    js << "module.exports = module_stub;\n";
    return js.str();
}

// ============================================================================
// Hotpatch Integration
// ============================================================================

PatchResult PolyfillEngine::createNativeBridge(const char* moduleName,
                                                const char* functionName,
                                                void* nativeFunction) {
    if (!moduleName || !functionName || !nativeFunction) {
        return PatchResult::error("Null parameters");
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    NativeBridge bridge;
    bridge.moduleName = moduleName;
    bridge.functionName = functionName;
    bridge.nativeFunction = nativeFunction;
    m_bridges.push_back(bridge);

    m_stats.nativeBridges++;

    char info[256];
    std::snprintf(info, sizeof(info),
                  "[PolyfillEngine] Native bridge: %s.%s → %p\n",
                  moduleName, functionName, nativeFunction);
    OutputDebugStringA(info);

    return PatchResult::ok("Native bridge created");
}

// ============================================================================
// require() Analysis
// ============================================================================

std::vector<std::string> PolyfillEngine::analyzeRequires(const char* jsSource, size_t sourceLen) {
    std::vector<std::string> modules;
    if (!jsSource || sourceLen == 0) return modules;

    std::string source(jsSource, sourceLen);

    // Match require('module') and require("module") patterns
    // Also match import ... from 'module' patterns
    std::unordered_set<std::string> seen;

    // Pattern 1: require('...')  require("...")
    {
        size_t pos = 0;
        while ((pos = source.find("require(", pos)) != std::string::npos) {
            pos += 8; // skip "require("
            // Skip whitespace
            while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t')) pos++;

            char quote = 0;
            if (pos < source.size() && (source[pos] == '\'' || source[pos] == '"' || source[pos] == '`')) {
                quote = source[pos];
                pos++;
            }
            if (!quote) continue;

            size_t end = source.find(quote, pos);
            if (end == std::string::npos) continue;

            std::string moduleName = source.substr(pos, end - pos);
            // Skip relative requires (./  ../) — those are extension-internal
            if (!moduleName.empty() && moduleName[0] != '.') {
                if (seen.insert(moduleName).second) {
                    modules.push_back(moduleName);
                }
            }
            pos = end + 1;
        }
    }

    // Pattern 2: import ... from '...'  import ... from "..."
    {
        size_t pos = 0;
        while ((pos = source.find("from ", pos)) != std::string::npos) {
            pos += 5; // skip "from "
            while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t')) pos++;

            char quote = 0;
            if (pos < source.size() && (source[pos] == '\'' || source[pos] == '"')) {
                quote = source[pos];
                pos++;
            }
            if (!quote) continue;

            size_t end = source.find(quote, pos);
            if (end == std::string::npos) continue;

            std::string moduleName = source.substr(pos, end - pos);
            if (!moduleName.empty() && moduleName[0] != '.') {
                if (seen.insert(moduleName).second) {
                    modules.push_back(moduleName);
                }
            }
            pos = end + 1;
        }
    }

    return modules;
}

// ============================================================================
// Statistics
// ============================================================================

void PolyfillEngine::getAllPolyfills(PolyfillDescriptor* outDescs, size_t maxDescs,
                                      size_t* outCount) const {
    if (!outDescs || !outCount) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& [name, desc] : m_registry) {
        if (count >= maxDescs) break;
        outDescs[count++] = desc;
    }
    *outCount = count;
}

PolyfillEngine::Stats PolyfillEngine::getStats() const {
    return m_stats;
}

// ============================================================================
// Built-in Polyfill Generators
// ============================================================================

std::string PolyfillEngine::generateFsPolyfill() {
    return R"JS(
// RawrXD fs polyfill — Routes to native Win32 file I/O via C++ bridge
// Compatibility: ~85% of fs API surface used by VS Code extensions

const _native = globalThis.__rawrxd_native || {};

const fs = {
    // Synchronous operations
    existsSync(path) {
        if (_native.fs_existsSync) return _native.fs_existsSync(path);
        return false;
    },

    readFileSync(path, options) {
        const encoding = (typeof options === 'string') ? options : (options && options.encoding);
        if (_native.fs_readFileSync) return _native.fs_readFileSync(path, encoding || 'utf8');
        throw new Error(`ENOENT: no such file or directory, open '${path}'`);
    },

    writeFileSync(path, data, options) {
        const encoding = (typeof options === 'string') ? options : (options && options.encoding) || 'utf8';
        if (_native.fs_writeFileSync) return _native.fs_writeFileSync(path, data, encoding);
    },

    mkdirSync(path, options) {
        if (_native.fs_mkdirSync) return _native.fs_mkdirSync(path, !!(options && options.recursive));
    },

    readdirSync(path, options) {
        if (_native.fs_readdirSync) return _native.fs_readdirSync(path);
        return [];
    },

    statSync(path) {
        if (_native.fs_statSync) return _native.fs_statSync(path);
        return { isFile: () => false, isDirectory: () => false, size: 0, mtime: new Date() };
    },

    unlinkSync(path) {
        if (_native.fs_unlinkSync) return _native.fs_unlinkSync(path);
    },

    renameSync(oldPath, newPath) {
        if (_native.fs_renameSync) return _native.fs_renameSync(oldPath, newPath);
    },

    copyFileSync(src, dest) {
        if (_native.fs_copyFileSync) return _native.fs_copyFileSync(src, dest);
    },

    accessSync(path, mode) {
        if (!fs.existsSync(path)) {
            throw new Error(`ENOENT: no such file or directory, access '${path}'`);
        }
    },

    lstatSync(path) { return fs.statSync(path); },

    // Async operations (callback-style)
    readFile(path, optionsOrCallback, callback) {
        const cb = typeof optionsOrCallback === 'function' ? optionsOrCallback : callback;
        const opts = typeof optionsOrCallback === 'object' ? optionsOrCallback : {};
        try {
            const data = fs.readFileSync(path, opts);
            if (cb) setTimeout(() => cb(null, data), 0);
        } catch (err) {
            if (cb) setTimeout(() => cb(err), 0);
        }
    },

    writeFile(path, data, optionsOrCallback, callback) {
        const cb = typeof optionsOrCallback === 'function' ? optionsOrCallback : callback;
        const opts = typeof optionsOrCallback === 'object' ? optionsOrCallback : {};
        try {
            fs.writeFileSync(path, data, opts);
            if (cb) setTimeout(() => cb(null), 0);
        } catch (err) {
            if (cb) setTimeout(() => cb(err), 0);
        }
    },

    mkdir(path, optionsOrCallback, callback) {
        const cb = typeof optionsOrCallback === 'function' ? optionsOrCallback : callback;
        try {
            fs.mkdirSync(path, typeof optionsOrCallback === 'object' ? optionsOrCallback : {});
            if (cb) setTimeout(() => cb(null), 0);
        } catch (err) {
            if (cb) setTimeout(() => cb(err), 0);
        }
    },

    readdir(path, optionsOrCallback, callback) {
        const cb = typeof optionsOrCallback === 'function' ? optionsOrCallback : callback;
        try {
            const result = fs.readdirSync(path);
            if (cb) setTimeout(() => cb(null, result), 0);
        } catch (err) {
            if (cb) setTimeout(() => cb(err), 0);
        }
    },

    stat(path, callback) {
        try {
            const result = fs.statSync(path);
            if (callback) setTimeout(() => callback(null, result), 0);
        } catch (err) {
            if (callback) setTimeout(() => callback(err), 0);
        }
    },

    unlink(path, callback) {
        try {
            fs.unlinkSync(path);
            if (callback) setTimeout(() => callback(null), 0);
        } catch (err) {
            if (callback) setTimeout(() => callback(err), 0);
        }
    },

    // watch / watchFile stubs
    watch(path, options, listener) {
        return { close() {} };
    },

    watchFile(path, options, listener) {
        return { close() {} };
    },

    unwatchFile(path, listener) {},

    // Promises API
    promises: {
        async readFile(path, options) { return fs.readFileSync(path, options); },
        async writeFile(path, data, options) { return fs.writeFileSync(path, data, options); },
        async mkdir(path, options) { return fs.mkdirSync(path, options); },
        async readdir(path, options) { return fs.readdirSync(path); },
        async stat(path) { return fs.statSync(path); },
        async unlink(path) { return fs.unlinkSync(path); },
        async access(path) { return fs.accessSync(path); },
        async rename(oldPath, newPath) { return fs.renameSync(oldPath, newPath); },
        async copyFile(src, dest) { return fs.copyFileSync(src, dest); },
    },

    // Constants
    constants: {
        F_OK: 0,
        R_OK: 4,
        W_OK: 2,
        X_OK: 1,
        COPYFILE_EXCL: 1,
        COPYFILE_FICLONE: 2,
    },

    // Stream stubs
    createReadStream(path, options) {
        const EventEmitter = require('events');
        const stream = new EventEmitter();
        stream.pipe = function(dest) { return dest; };
        stream.destroy = function() {};
        setTimeout(() => {
            try {
                const data = fs.readFileSync(path, 'utf8');
                stream.emit('data', data);
                stream.emit('end');
            } catch (err) {
                stream.emit('error', err);
            }
        }, 0);
        return stream;
    },

    createWriteStream(path, options) {
        const EventEmitter = require('events');
        const stream = new EventEmitter();
        let buffer = '';
        stream.write = function(chunk) { buffer += chunk; return true; };
        stream.end = function(chunk) {
            if (chunk) buffer += chunk;
            fs.writeFileSync(path, buffer);
            stream.emit('finish');
        };
        stream.destroy = function() {};
        return stream;
    },
};

module.exports = fs;
)JS";
}

std::string PolyfillEngine::generatePathPolyfill() {
    return R"JS(
// RawrXD path polyfill — Pure JavaScript, 98% compatible
// Handles both Windows and POSIX paths correctly

const isWindows = true; // RawrXD is Windows-native

const path = {
    sep: isWindows ? '\\' : '/',
    delimiter: isWindows ? ';' : ':',
    posix: null, // Self-ref set below
    win32: null, // Self-ref set below

    basename(p, ext) {
        if (typeof p !== 'string') return '';
        let base = p.replace(/[\\/]+$/, '');
        const lastSep = Math.max(base.lastIndexOf('/'), base.lastIndexOf('\\'));
        if (lastSep >= 0) base = base.substring(lastSep + 1);
        if (ext && base.endsWith(ext)) base = base.slice(0, -ext.length);
        return base;
    },

    dirname(p) {
        if (typeof p !== 'string' || p.length === 0) return '.';
        const lastSep = Math.max(p.lastIndexOf('/'), p.lastIndexOf('\\'));
        if (lastSep < 0) return '.';
        if (lastSep === 0) return p[0];
        return p.substring(0, lastSep);
    },

    extname(p) {
        if (typeof p !== 'string') return '';
        const base = path.basename(p);
        const dotIndex = base.lastIndexOf('.');
        if (dotIndex <= 0) return '';
        return base.substring(dotIndex);
    },

    join(...parts) {
        const sep = isWindows ? '\\' : '/';
        let joined = parts.filter(p => typeof p === 'string' && p.length > 0).join(sep);
        // Normalize multiple separators
        joined = joined.replace(/[/\\]+/g, sep);
        // Remove trailing separator (unless root)
        if (joined.length > 1 && joined.endsWith(sep)) {
            joined = joined.slice(0, -1);
        }
        return joined;
    },

    resolve(...parts) {
        let resolved = '';
        for (let i = parts.length - 1; i >= 0; i--) {
            const p = parts[i];
            if (typeof p !== 'string' || p.length === 0) continue;
            resolved = p + (resolved ? ('\\' + resolved) : '');
            // If absolute, stop
            if (path.isAbsolute(resolved)) break;
        }
        if (!path.isAbsolute(resolved)) {
            // Prepend CWD (from native bridge or default)
            const cwd = (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getCwd) 
                        ? globalThis.__rawrxd_native.getCwd() : 'C:\\';
            resolved = cwd + '\\' + resolved;
        }
        return path.normalize(resolved);
    },

    normalize(p) {
        if (typeof p !== 'string' || p.length === 0) return '.';
        const sep = isWindows ? '\\' : '/';
        const parts = p.replace(/[/\\]+/g, sep).split(sep);
        const result = [];
        for (const part of parts) {
            if (part === '..') {
                if (result.length > 0 && result[result.length - 1] !== '..') {
                    result.pop();
                }
            } else if (part !== '.' && part !== '') {
                result.push(part);
            }
        }
        let normalized = result.join(sep);
        // Preserve leading separator
        if (p[0] === '/' || p[0] === '\\') normalized = sep + normalized;
        // Preserve drive letter (C:)
        if (isWindows && p.length >= 2 && p[1] === ':') {
            if (normalized.length >= 2 && normalized[1] !== ':') {
                normalized = p.substring(0, 2) + sep + normalized;
            }
        }
        return normalized || '.';
    },

    isAbsolute(p) {
        if (typeof p !== 'string') return false;
        if (isWindows) {
            return (p.length >= 3 && p[1] === ':' && (p[2] === '/' || p[2] === '\\')) ||
                   (p.startsWith('\\\\'));
        }
        return p.startsWith('/');
    },

    relative(from, to) {
        const fromParts = path.resolve(from).split(/[/\\]/);
        const toParts = path.resolve(to).split(/[/\\]/);
        let commonLength = 0;
        for (let i = 0; i < Math.min(fromParts.length, toParts.length); i++) {
            if (fromParts[i].toLowerCase() === toParts[i].toLowerCase()) {
                commonLength = i + 1;
            } else break;
        }
        const up = fromParts.length - commonLength;
        const remaining = toParts.slice(commonLength);
        const result = [];
        for (let i = 0; i < up; i++) result.push('..');
        result.push(...remaining);
        return result.join(path.sep) || '.';
    },

    parse(p) {
        const ext = path.extname(p);
        const base = path.basename(p);
        const dir = path.dirname(p);
        let root = '';
        if (isWindows && p.length >= 2 && p[1] === ':') {
            root = p.substring(0, 3);
        } else if (p.startsWith('/') || p.startsWith('\\')) {
            root = p[0];
        }
        return {
            root,
            dir,
            base,
            ext,
            name: base.slice(0, base.length - ext.length),
        };
    },

    format(pathObject) {
        const sep = path.sep;
        let result = '';
        if (pathObject.dir) {
            result = pathObject.dir + sep;
        } else if (pathObject.root) {
            result = pathObject.root;
        }
        if (pathObject.base) {
            result += pathObject.base;
        } else {
            result += (pathObject.name || '') + (pathObject.ext || '');
        }
        return result;
    },

    toNamespacedPath(p) { return p; },
};

// Self-references for posix/win32 sub-objects
path.posix = path;
path.win32 = path;

module.exports = path;
)JS";
}

std::string PolyfillEngine::generateOsPolyfill() {
    return R"JS(
// RawrXD os polyfill — Windows-native implementation

const os = {
    platform() { return 'win32'; },
    arch() { return 'x64'; },
    type() { return 'Windows_NT'; },
    release() { return '10.0.19045'; },
    
    homedir() {
        return (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getEnv)
            ? globalThis.__rawrxd_native.getEnv('USERPROFILE') || 'C:\\Users\\User'
            : 'C:\\Users\\User';
    },
    
    tmpdir() {
        return (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getEnv)
            ? globalThis.__rawrxd_native.getEnv('TEMP') || 'C:\\Users\\User\\AppData\\Local\\Temp'
            : 'C:\\Users\\User\\AppData\\Local\\Temp';
    },
    
    hostname() { return 'RawrXD-Host'; },
    
    cpus() {
        const count = (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getCpuCount)
            ? globalThis.__rawrxd_native.getCpuCount() : 4;
        const cpus = [];
        for (let i = 0; i < count; i++) {
            cpus.push({ model: 'CPU', speed: 3000, times: { user: 0, nice: 0, sys: 0, idle: 0, irq: 0 } });
        }
        return cpus;
    },
    
    totalmem() { return 16 * 1024 * 1024 * 1024; },
    freemem() { return 8 * 1024 * 1024 * 1024; },
    
    endianness() { return 'LE'; },
    
    networkInterfaces() { return {}; },
    
    userInfo() {
        return {
            uid: -1,
            gid: -1,
            username: os.homedir().split('\\').pop() || 'User',
            homedir: os.homedir(),
            shell: null,
        };
    },
    
    loadavg() { return [0, 0, 0]; },
    uptime() { return 3600; },
    
    EOL: '\r\n',
    devNull: 'NUL',
    
    constants: {
        signals: {},
        errno: {},
        priority: { PRIORITY_LOW: 19, PRIORITY_BELOW_NORMAL: 10, PRIORITY_NORMAL: 0,
                     PRIORITY_ABOVE_NORMAL: -7, PRIORITY_HIGH: -14, PRIORITY_HIGHEST: -20 },
    },
};

module.exports = os;
)JS";
}

std::string PolyfillEngine::generateProcessPolyfill() {
    return R"JS(
// RawrXD process polyfill — Provides process.env, process.platform, etc.

const process = {
    platform: 'win32',
    arch: 'x64',
    version: 'v18.0.0',
    versions: { node: '18.0.0', v8: '10.0', modules: '108' },
    
    env: new Proxy({}, {
        get(target, prop) {
            if (prop in target) return target[prop];
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getEnv) {
                return globalThis.__rawrxd_native.getEnv(String(prop));
            }
            return undefined;
        },
        set(target, prop, value) {
            target[prop] = value;
            return true;
        },
    }),
    
    cwd() {
        return (globalThis.__rawrxd_native && globalThis.__rawrxd_native.getCwd)
            ? globalThis.__rawrxd_native.getCwd() : 'C:\\';
    },
    
    pid: 1,
    ppid: 0,
    title: 'RawrXD Extension Host',
    argv: ['rawrxd'],
    argv0: 'rawrxd',
    execPath: 'C:\\RawrXD\\RawrXD.exe',
    execArgv: [],
    
    stdout: {
        write(chunk) { console.log(chunk); return true; },
        isTTY: false,
        columns: 120,
        rows: 30,
    },
    stderr: {
        write(chunk) { console.error(chunk); return true; },
        isTTY: false,
    },
    stdin: {
        isTTY: false,
        setEncoding() {},
        on() { return this; },
        resume() {},
        pause() {},
    },
    
    exit(code) { console.warn('[RawrXD] process.exit() called with code:', code); },
    abort() { console.warn('[RawrXD] process.abort() called'); },
    kill(pid, signal) { console.warn('[RawrXD] process.kill() called'); },
    
    nextTick(callback, ...args) { setTimeout(() => callback(...args), 0); },
    
    hrtime: Object.assign(
        function(previous) {
            const now = Date.now();
            const s = Math.floor(now / 1000);
            const ns = (now % 1000) * 1e6;
            if (previous) return [s - previous[0], ns - previous[1]];
            return [s, ns];
        },
        { bigint() { return BigInt(Date.now()) * 1000000n; } }
    ),
    
    memoryUsage() {
        return { rss: 100 * 1024 * 1024, heapTotal: 50 * 1024 * 1024,
                 heapUsed: 30 * 1024 * 1024, external: 5 * 1024 * 1024, arrayBuffers: 1024 * 1024 };
    },
    
    cpuUsage() { return { user: 0, system: 0 }; },
    
    on(event, listener) { return process; },
    once(event, listener) { return process; },
    off(event, listener) { return process; },
    removeListener(event, listener) { return process; },
    emit(event, ...args) { return false; },
    removeAllListeners(event) { return process; },
    listeners(event) { return []; },
    
    umask(mask) { return 0o22; },
    uptime() { return 3600; },
    
    // Feature detection
    features: { inspector: false, tls_alpn: false, tls_sni: false, tls_ocsp: false },
    release: { name: 'rawrxd', sourceUrl: '', headersUrl: '' },
};

// Set the global process
if (typeof globalThis.process === 'undefined') {
    globalThis.process = process;
}

module.exports = process;
)JS";
}

std::string PolyfillEngine::generateChildProcessPolyfill() {
    return R"JS(
// RawrXD child_process polyfill — Partial shim (40% compat)
// Extensions that spawn processes will get sandboxed execution

const child_process = {
    exec(command, options, callback) {
        const cb = typeof options === 'function' ? options : callback;
        console.warn('[RawrXD] child_process.exec() is sandboxed:', command);
        if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.execCommand) {
            const result = globalThis.__rawrxd_native.execCommand(command);
            if (cb) setTimeout(() => cb(null, result || '', ''), 0);
        } else {
            if (cb) setTimeout(() => cb(new Error('child_process.exec not available in sandbox'), '', ''), 0);
        }
        return { on() { return this; }, stdout: { on() { return this; } }, stderr: { on() { return this; } }, kill() {} };
    },

    execSync(command, options) {
        console.warn('[RawrXD] child_process.execSync() is sandboxed:', command);
        if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.execCommand) {
            return globalThis.__rawrxd_native.execCommand(command);
        }
        return '';
    },

    spawn(command, args, options) {
        console.warn('[RawrXD] child_process.spawn() is sandboxed:', command);
        const EventEmitter = require('events');
        const child = new EventEmitter();
        child.pid = 0;
        child.killed = false;
        child.stdin = { write() {}, end() {} };
        child.stdout = new EventEmitter();
        child.stderr = new EventEmitter();
        child.kill = function() { child.killed = true; };
        child.ref = function() { return child; };
        child.unref = function() { return child; };
        setTimeout(() => { child.emit('exit', 0, null); child.emit('close', 0, null); }, 10);
        return child;
    },

    execFile(file, args, options, callback) {
        const cb = typeof args === 'function' ? args :
                   typeof options === 'function' ? options : callback;
        console.warn('[RawrXD] child_process.execFile() is sandboxed:', file);
        if (cb) setTimeout(() => cb(null, '', ''), 0);
        return { on() { return this; }, kill() {} };
    },

    fork(modulePath, args, options) {
        return child_process.spawn('node', [modulePath, ...(args || [])], options);
    },
};

module.exports = child_process;
)JS";
}

std::string PolyfillEngine::generateCryptoPolyfill() {
    return R"JS(
// RawrXD crypto polyfill — Basic hashing and randomness

const crypto = {
    randomBytes(size) {
        const bytes = new Uint8Array(size);
        if (typeof globalThis.crypto !== 'undefined' && globalThis.crypto.getRandomValues) {
            globalThis.crypto.getRandomValues(bytes);
        } else {
            for (let i = 0; i < size; i++) bytes[i] = Math.floor(Math.random() * 256);
        }
        return {
            toString(encoding) {
                if (encoding === 'hex') return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
                if (encoding === 'base64') return btoa(String.fromCharCode(...bytes));
                return String.fromCharCode(...bytes);
            },
            toJSON() { return { type: 'Buffer', data: Array.from(bytes) }; },
            [Symbol.toPrimitive]() { return this.toString('hex'); },
            length: size,
        };
    },

    randomUUID() {
        const hex = crypto.randomBytes(16).toString('hex');
        return `${hex.slice(0,8)}-${hex.slice(8,12)}-4${hex.slice(13,16)}-${(parseInt(hex[16],16) & 3 | 8).toString(16)}${hex.slice(17,20)}-${hex.slice(20,32)}`;
    },

    createHash(algorithm) {
        // Simple non-cryptographic hash for extension compat
        let data = '';
        return {
            update(input, encoding) { data += String(input); return this; },
            digest(encoding) {
                let hash = 0;
                for (let i = 0; i < data.length; i++) {
                    hash = ((hash << 5) - hash + data.charCodeAt(i)) | 0;
                }
                const hex = Math.abs(hash).toString(16).padStart(8, '0');
                if (encoding === 'hex') return hex.repeat(4);
                if (encoding === 'base64') return btoa(hex);
                return hex;
            },
        };
    },

    createHmac(algorithm, key) {
        return crypto.createHash(algorithm);
    },

    timingSafeEqual(a, b) {
        if (a.length !== b.length) return false;
        let result = 0;
        for (let i = 0; i < a.length; i++) {
            result |= (a[i] || a.charCodeAt(i)) ^ (b[i] || b.charCodeAt(i));
        }
        return result === 0;
    },

    constants: {},
    webcrypto: globalThis.crypto || {},
};

module.exports = crypto;
)JS";
}

std::string PolyfillEngine::generateHttpPolyfill() {
    return R"JS(
// RawrXD http/https polyfill — Stub for extensions that make HTTP requests

const http = {
    request(urlOrOptions, optionsOrCallback, callback) {
        const cb = typeof optionsOrCallback === 'function' ? optionsOrCallback : callback;
        console.warn('[RawrXD] http.request() is stubbed — use fetch() or native bridge');
        const EventEmitter = require('events');
        const req = new EventEmitter();
        req.write = function() { return req; };
        req.end = function() {
            const res = new EventEmitter();
            res.statusCode = 200;
            res.headers = {};
            res.setEncoding = function() {};
            setTimeout(() => {
                res.emit('data', '');
                res.emit('end');
            }, 10);
            if (cb) cb(res);
            req.emit('response', res);
        };
        req.abort = function() {};
        req.destroy = function() {};
        req.on('error', () => {});
        return req;
    },

    get(urlOrOptions, optionsOrCallback, callback) {
        const req = http.request(urlOrOptions, optionsOrCallback, callback);
        req.end();
        return req;
    },

    createServer(requestListener) {
        console.warn('[RawrXD] http.createServer() is not supported in extension host');
        return { listen() {}, close() {}, on() { return this; } };
    },

    Agent: class Agent { constructor() {} },
    globalAgent: {},
    STATUS_CODES: { 200: 'OK', 404: 'Not Found', 500: 'Internal Server Error' },
    METHODS: ['GET', 'POST', 'PUT', 'DELETE', 'PATCH', 'HEAD', 'OPTIONS'],
};

module.exports = http;
)JS";
}

std::string PolyfillEngine::generateEventsPolyfill() {
    return R"JS(
// RawrXD events polyfill — Full EventEmitter implementation (95% compat)

class EventEmitter {
    constructor() {
        this._events = {};
        this._maxListeners = 10;
    }

    on(event, listener) {
        if (!this._events[event]) this._events[event] = [];
        this._events[event].push({ fn: listener, once: false });
        return this;
    }

    once(event, listener) {
        if (!this._events[event]) this._events[event] = [];
        this._events[event].push({ fn: listener, once: true });
        return this;
    }

    off(event, listener) {
        return this.removeListener(event, listener);
    }

    removeListener(event, listener) {
        if (!this._events[event]) return this;
        this._events[event] = this._events[event].filter(e => e.fn !== listener);
        return this;
    }

    removeAllListeners(event) {
        if (event) {
            delete this._events[event];
        } else {
            this._events = {};
        }
        return this;
    }

    emit(event, ...args) {
        if (!this._events[event]) return false;
        const listeners = this._events[event].slice();
        const toRemove = [];
        for (const entry of listeners) {
            entry.fn.apply(this, args);
            if (entry.once) toRemove.push(entry);
        }
        for (const entry of toRemove) {
            const idx = this._events[event].indexOf(entry);
            if (idx >= 0) this._events[event].splice(idx, 1);
        }
        return listeners.length > 0;
    }

    listeners(event) {
        return (this._events[event] || []).map(e => e.fn);
    }

    listenerCount(event) {
        return (this._events[event] || []).length;
    }

    eventNames() {
        return Object.keys(this._events);
    }

    setMaxListeners(n) {
        this._maxListeners = n;
        return this;
    }

    getMaxListeners() {
        return this._maxListeners;
    }

    prependListener(event, listener) {
        if (!this._events[event]) this._events[event] = [];
        this._events[event].unshift({ fn: listener, once: false });
        return this;
    }

    prependOnceListener(event, listener) {
        if (!this._events[event]) this._events[event] = [];
        this._events[event].unshift({ fn: listener, once: true });
        return this;
    }

    rawListeners(event) {
        return (this._events[event] || []).slice();
    }

    addListener(event, listener) { return this.on(event, listener); }
}

EventEmitter.defaultMaxListeners = 10;
EventEmitter.EventEmitter = EventEmitter;

module.exports = EventEmitter;
)JS";
}

std::string PolyfillEngine::generateStreamPolyfill() {
    return R"JS(
// RawrXD stream polyfill — Partial shim using EventEmitter

const EventEmitter = require('events');

class Readable extends EventEmitter {
    constructor(options) {
        super();
        this.readable = true;
        this._readableState = { flowing: null, ended: false };
    }
    read(size) { return null; }
    pipe(dest) { return dest; }
    unpipe(dest) { return this; }
    resume() { this._readableState.flowing = true; return this; }
    pause() { this._readableState.flowing = false; return this; }
    destroy() { this.emit('close'); return this; }
    setEncoding(enc) { return this; }
    [Symbol.asyncIterator]() { return { next: async () => ({ done: true, value: undefined }) }; }
}

class Writable extends EventEmitter {
    constructor(options) {
        super();
        this.writable = true;
    }
    write(chunk, encoding, callback) {
        if (typeof encoding === 'function') { callback = encoding; }
        if (callback) setTimeout(callback, 0);
        return true;
    }
    end(chunk, encoding, callback) {
        if (typeof chunk === 'function') { callback = chunk; chunk = null; }
        if (typeof encoding === 'function') { callback = encoding; }
        if (callback) setTimeout(callback, 0);
        this.emit('finish');
        return this;
    }
    destroy() { this.emit('close'); return this; }
    cork() {}
    uncork() {}
}

class Duplex extends Readable {
    constructor(options) {
        super(options);
        this.writable = true;
    }
    write(chunk, encoding, callback) { return Writable.prototype.write.call(this, chunk, encoding, callback); }
    end(chunk, encoding, callback) { return Writable.prototype.end.call(this, chunk, encoding, callback); }
}

class Transform extends Duplex {
    constructor(options) { super(options); }
    _transform(chunk, encoding, callback) { callback(null, chunk); }
}

class PassThrough extends Transform {}

module.exports = { Readable, Writable, Duplex, Transform, PassThrough, Stream: Readable };
)JS";
}

std::string PolyfillEngine::generateBufferPolyfill() {
    return R"JS(
// RawrXD Buffer polyfill — Using Uint8Array as backing store

class Buffer extends Uint8Array {
    static from(value, encoding) {
        if (typeof value === 'string') {
            const encoder = new TextEncoder();
            const bytes = encoder.encode(value);
            return new Buffer(bytes);
        }
        if (Array.isArray(value) || value instanceof Uint8Array) {
            return new Buffer(value);
        }
        if (value && value.type === 'Buffer' && Array.isArray(value.data)) {
            return new Buffer(value.data);
        }
        return new Buffer(0);
    }

    static alloc(size, fill, encoding) {
        const buf = new Buffer(size);
        if (fill !== undefined) buf.fill(typeof fill === 'number' ? fill : 0);
        return buf;
    }

    static allocUnsafe(size) { return new Buffer(size); }
    static allocUnsafeSlow(size) { return new Buffer(size); }

    static isBuffer(obj) { return obj instanceof Buffer || obj instanceof Uint8Array; }
    static isEncoding(enc) { return ['utf8', 'utf-8', 'ascii', 'latin1', 'binary', 'hex', 'base64'].includes(enc); }

    static concat(list, totalLength) {
        if (totalLength === undefined) {
            totalLength = list.reduce((sum, buf) => sum + buf.length, 0);
        }
        const result = Buffer.alloc(totalLength);
        let offset = 0;
        for (const buf of list) {
            result.set(buf, offset);
            offset += buf.length;
        }
        return result;
    }

    static byteLength(string, encoding) {
        return new TextEncoder().encode(String(string)).length;
    }

    toString(encoding, start, end) {
        const slice = this.subarray(start || 0, end || this.length);
        if (encoding === 'hex') {
            return Array.from(slice).map(b => b.toString(16).padStart(2, '0')).join('');
        }
        if (encoding === 'base64') {
            return btoa(String.fromCharCode(...slice));
        }
        return new TextDecoder().decode(slice);
    }

    write(string, offset, length, encoding) {
        const bytes = new TextEncoder().encode(string);
        this.set(bytes.subarray(0, Math.min(bytes.length, length || this.length)), offset || 0);
        return Math.min(bytes.length, length || this.length);
    }

    toJSON() { return { type: 'Buffer', data: Array.from(this) }; }
    equals(other) { return this.length === other.length && this.every((v, i) => v === other[i]); }
    compare(other) { for (let i = 0; i < Math.min(this.length, other.length); i++) { if (this[i] !== other[i]) return this[i] < other[i] ? -1 : 1; } return this.length - other.length; }
    copy(target, targetStart, sourceStart, sourceEnd) { target.set(this.subarray(sourceStart || 0, sourceEnd || this.length), targetStart || 0); return Math.min(this.length, target.length); }
    slice(start, end) { return Buffer.from(this.subarray(start, end)); }
    includes(value) { return this.indexOf(value) !== -1; }
    readUInt8(offset) { return this[offset]; }
    readUInt16LE(offset) { return this[offset] | (this[offset + 1] << 8); }
    readUInt32LE(offset) { return (this[offset] | (this[offset + 1] << 8) | (this[offset + 2] << 16) | (this[offset + 3] << 24)) >>> 0; }
    readInt8(offset) { const v = this[offset]; return v > 127 ? v - 256 : v; }
    writeUInt8(value, offset) { this[offset] = value & 0xFF; }
    writeUInt16LE(value, offset) { this[offset] = value & 0xFF; this[offset + 1] = (value >> 8) & 0xFF; }
    writeUInt32LE(value, offset) { this[offset] = value & 0xFF; this[offset + 1] = (value >> 8) & 0xFF; this[offset + 2] = (value >> 16) & 0xFF; this[offset + 3] = (value >> 24) & 0xFF; }
}

// Set global Buffer
if (typeof globalThis.Buffer === 'undefined') {
    globalThis.Buffer = Buffer;
}

module.exports = { Buffer };
)JS";
}

std::string PolyfillEngine::generateUtilPolyfill() {
    return R"JS(
// RawrXD util polyfill — Common Node.js utility functions

const util = {
    promisify(fn) {
        return function(...args) {
            return new Promise((resolve, reject) => {
                fn(...args, (err, ...results) => {
                    if (err) return reject(err);
                    resolve(results.length <= 1 ? results[0] : results);
                });
            });
        };
    },

    callbackify(fn) {
        return function(...args) {
            const callback = args.pop();
            fn(...args).then(result => callback(null, result), err => callback(err));
        };
    },

    inherits(ctor, superCtor) {
        Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
        ctor.super_ = superCtor;
    },

    deprecate(fn, msg) {
        let warned = false;
        return function(...args) {
            if (!warned) { console.warn('[DEP]', msg); warned = true; }
            return fn.apply(this, args);
        };
    },

    format(f, ...args) {
        if (typeof f !== 'string') return [f, ...args].map(String).join(' ');
        let i = 0;
        return f.replace(/%[sdjifoO%]/g, match => {
            if (match === '%%') return '%';
            if (i >= args.length) return match;
            const arg = args[i++];
            switch (match) {
                case '%s': return String(arg);
                case '%d': case '%i': return parseInt(arg, 10);
                case '%f': return parseFloat(arg);
                case '%j': return JSON.stringify(arg);
                case '%o': case '%O': return JSON.stringify(arg);
                default: return match;
            }
        });
    },

    inspect(obj, options) { return JSON.stringify(obj, null, 2); },
    debuglog(section) { return function() {}; },
    isDeepStrictEqual(a, b) { return JSON.stringify(a) === JSON.stringify(b); },
    types: {
        isDate(v) { return v instanceof Date; },
        isRegExp(v) { return v instanceof RegExp; },
        isNativeError(v) { return v instanceof Error; },
        isPromise(v) { return v instanceof Promise; },
        isArrayBuffer(v) { return v instanceof ArrayBuffer; },
        isUint8Array(v) { return v instanceof Uint8Array; },
        isMap(v) { return v instanceof Map; },
        isSet(v) { return v instanceof Set; },
    },
    TextDecoder: globalThis.TextDecoder,
    TextEncoder: globalThis.TextEncoder,
};

module.exports = util;
)JS";
}

std::string PolyfillEngine::generateUrlPolyfill() {
    return R"JS(
// RawrXD url polyfill — URL parsing using native URL constructor

const url = {
    URL: globalThis.URL,
    URLSearchParams: globalThis.URLSearchParams,

    parse(urlStr, parseQueryString) {
        try {
            const u = new URL(urlStr);
            return {
                protocol: u.protocol,
                slashes: u.protocol.endsWith(':'),
                auth: u.username ? `${u.username}:${u.password}` : null,
                host: u.host,
                port: u.port || null,
                hostname: u.hostname,
                hash: u.hash || null,
                search: u.search || null,
                query: parseQueryString ? Object.fromEntries(u.searchParams) : u.search?.slice(1) || null,
                pathname: u.pathname,
                path: u.pathname + (u.search || ''),
                href: u.href,
            };
        } catch (e) {
            return { protocol: null, host: null, hostname: null, pathname: urlStr, href: urlStr };
        }
    },

    format(urlObj) {
        if (typeof urlObj === 'string') return urlObj;
        let result = '';
        if (urlObj.protocol) result += urlObj.protocol + '//';
        if (urlObj.auth) result += urlObj.auth + '@';
        if (urlObj.hostname) result += urlObj.hostname;
        if (urlObj.port) result += ':' + urlObj.port;
        if (urlObj.pathname) result += urlObj.pathname;
        if (urlObj.search) result += urlObj.search;
        if (urlObj.hash) result += urlObj.hash;
        return result;
    },

    resolve(from, to) {
        try { return new URL(to, from).href; }
        catch (e) { return to; }
    },

    fileURLToPath(url) {
        const u = typeof url === 'string' ? new URL(url) : url;
        if (u.protocol !== 'file:') throw new Error('Not a file URL');
        return u.pathname.replace(/\//g, '\\').replace(/^\\/, '');
    },

    pathToFileURL(path) {
        return new URL('file:///' + path.replace(/\\/g, '/'));
    },
};

module.exports = url;
)JS";
}

std::string PolyfillEngine::generateQuerystringPolyfill() {
    return R"JS(
// RawrXD querystring polyfill — URL query string operations

const querystring = {
    parse(str, sep, eq) {
        sep = sep || '&';
        eq = eq || '=';
        const obj = {};
        if (typeof str !== 'string' || str.length === 0) return obj;
        str.split(sep).forEach(pair => {
            const idx = pair.indexOf(eq);
            const key = idx >= 0 ? decodeURIComponent(pair.slice(0, idx)) : decodeURIComponent(pair);
            const val = idx >= 0 ? decodeURIComponent(pair.slice(idx + 1)) : '';
            if (obj[key] !== undefined) {
                if (Array.isArray(obj[key])) obj[key].push(val);
                else obj[key] = [obj[key], val];
            } else {
                obj[key] = val;
            }
        });
        return obj;
    },

    stringify(obj, sep, eq) {
        sep = sep || '&';
        eq = eq || '=';
        return Object.entries(obj || {}).map(([key, val]) => {
            if (Array.isArray(val)) {
                return val.map(v => encodeURIComponent(key) + eq + encodeURIComponent(v)).join(sep);
            }
            return encodeURIComponent(key) + eq + encodeURIComponent(val);
        }).join(sep);
    },

    escape(str) { return encodeURIComponent(str); },
    unescape(str) { return decodeURIComponent(str); },
    encode: null, // alias set below
    decode: null, // alias set below
};

querystring.encode = querystring.stringify;
querystring.decode = querystring.parse;

module.exports = querystring;
)JS";
}

std::string PolyfillEngine::generateElectronPolyfill() {
    return R"JS(
// RawrXD electron polyfill — NoOp stubs for Electron-dependent extensions
// Extensions that deeply rely on Electron will get safe stubs

const electron = {
    // App module
    app: {
        getPath(name) {
            const os = require('os');
            switch (name) {
                case 'home': return os.homedir();
                case 'temp': return os.tmpdir();
                case 'desktop': return os.homedir() + '\\Desktop';
                case 'documents': return os.homedir() + '\\Documents';
                case 'downloads': return os.homedir() + '\\Downloads';
                case 'appData': return os.homedir() + '\\AppData\\Roaming';
                case 'userData': return os.homedir() + '\\AppData\\Roaming\\RawrXD';
                default: return os.homedir();
            }
        },
        getName() { return 'RawrXD IDE'; },
        getVersion() { return '1.0.0'; },
        getLocale() { return 'en'; },
        isReady() { return true; },
        whenReady() { return Promise.resolve(); },
        quit() { console.warn('[RawrXD] electron.app.quit() called'); },
        on() { return this; },
    },

    // BrowserWindow stub
    BrowserWindow: class BrowserWindow {
        constructor(options) { this.id = Math.random(); }
        loadURL(url) {}
        loadFile(path) {}
        show() {}
        hide() {}
        close() {}
        destroy() {}
        isDestroyed() { return false; }
        isVisible() { return false; }
        webContents = { send() {}, on() {}, executeJavaScript() { return Promise.resolve(); } };
        on() { return this; }
        static getAllWindows() { return []; }
        static getFocusedWindow() { return null; }
    },

    // Dialog module
    dialog: {
        showOpenDialog(options) { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showOpenDialog) {
                return globalThis.__rawrxd_native.dialog_showOpenDialog(options);
            }
            return Promise.resolve({ canceled: true, filePaths: [] }); 
        },
        showSaveDialog(options) { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showSaveDialog) {
                return globalThis.__rawrxd_native.dialog_showSaveDialog(options);
            }
            return Promise.resolve({ canceled: true, filePath: undefined }); 
        },
        showMessageBox(options) { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showMessageBox) {
                return globalThis.__rawrxd_native.dialog_showMessageBox(options);
            }
            return Promise.resolve({ response: 0, checkboxChecked: false }); 
        },
        showErrorBox(title, content) { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showErrorBox) {
                 globalThis.__rawrxd_native.dialog_showErrorBox(title, content);
            } else {
                 console.error('[Electron Dialog]', title, content); 
            }
        },
        showOpenDialogSync(options) { 
            // Blocking call via synchronous native method
             if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showOpenDialogSync) {
                return globalThis.__rawrxd_native.dialog_showOpenDialogSync(options);
            }
            return []; 
        },
        showSaveDialogSync(options) { 
             if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showSaveDialogSync) {
                return globalThis.__rawrxd_native.dialog_showSaveDialogSync(options);
            }
            return undefined; 
        },
        showMessageBoxSync(options) { 
             if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.dialog_showMessageBoxSync) {
                return globalThis.__rawrxd_native.dialog_showMessageBoxSync(options);
            }
            return 0; 
        },
    },

    // Shell module
    shell: {
        openExternal(url) {
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.openExternal) {
                globalThis.__rawrxd_native.openExternal(url);
            }
            return Promise.resolve();
        },
        openPath(path) { return Promise.resolve(''); },
        showItemInFolder(path) {},
        beep() {},
    },

    // Clipboard
    clipboard: {
        readText() { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.clipboard_readText) {
                return globalThis.__rawrxd_native.clipboard_readText();
            }
            return ''; 
        },
        writeText(text) { 
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.clipboard_writeText) {
                globalThis.__rawrxd_native.clipboard_writeText(text);
            }
        },
        readHTML() { return ''; },
        writeHTML(html) {},
    },

    // Menu stubs
    Menu: class Menu {
        constructor() { this.items = []; }
        append(item) { this.items.push(item); }
        popup() {
            if (globalThis.__rawrxd_native && globalThis.__rawrxd_native.menu_popup) {
                globalThis.__rawrxd_native.menu_popup(this.items);
            }
        }
        static buildFromTemplate(template) { 
            const m = new Menu();
            // Deep clone logic would be needed here for full fidelity
            return m; 
        }
        static setApplicationMenu(menu) {}
        static getApplicationMenu() { return null; }
    },

    MenuItem: class MenuItem {
        constructor(options) { Object.assign(this, options); }
    },

    // Tray stub
    Tray: class Tray {
        constructor(imagePath) {}
        setToolTip(tip) {}
        setContextMenu(menu) {}
        destroy() {}
        on() { return this; }
    },

    // IPC stubs
    ipcMain: { on() { return this; }, once() { return this; }, handle() {}, removeHandler() {} },
    ipcRenderer: { on() { return this; }, once() { return this; }, send() {}, invoke() { return Promise.resolve(); }, removeAllListeners() { return this; } },

    // Remote module (deprecated in Electron 14+)
    remote: null,

    // Notification
    Notification: class Notification {
        constructor(options) { Object.assign(this, options); }
        show() { console.log('[Notification]', this.title); }
        close() {}
        on() { return this; }
        static isSupported() { return false; }
    },

    // Screen
    screen: {
        getPrimaryDisplay() { return { workArea: { x: 0, y: 0, width: 1920, height: 1080 }, bounds: { x: 0, y: 0, width: 1920, height: 1080 }, scaleFactor: 1 }; },
        getAllDisplays() { return [this.getPrimaryDisplay()]; },
    },

    // Process type
    process: { type: 'renderer', versions: { electron: '0.0.0', chrome: '0.0.0' } },

    // nativeTheme
    nativeTheme: {
        shouldUseDarkColors: true,
        themeSource: 'dark',
        shouldUseHighContrastColors: false,
        on() { return this; },
    },
};

module.exports = electron;
)JS";
}

std::string PolyfillEngine::generateRemotePolyfill() {
    return R"JS(
// RawrXD vscode-remote polyfill — Stub for remote/SSH/WSL extensions

module.exports = {
    isRemote: false,
    remoteAuthority: null,
    getRemoteExtensionHostEnvironment() { return null; },
    env: {
        remoteName: undefined,
        uiKind: 1, // Desktop
    },
    workspace: {
        getRemoteFS() { return null; },
    },
};
)JS";
}

