#pragma once
/*==========================================================================
 * RawrXD Toolchain Bridge — IDE ↔ From-Scratch Toolchain Integration
 *
 * Bridges the IDE's build system to the custom from-scratch toolchain
 * (phase1: assembler, phase2: linker, phase3: imports, phase4: resources,
 *  phase5: debuginfo, phase6: libmanager).
 *
 * Also wraps MSVC (cl/ml64/link) as a fallback backend, providing a
 * unified compile→assemble→link→run pipeline for both IDEWindow and
 * Win32IDE variants.
 *
 * Thread model: Build runs on a worker thread, progress/diagnostics
 * pushed to the UI via callback.
 *=========================================================================*/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <memory>
#include <unordered_map>

namespace RawrXD::Compiler {

// =========================================================================
// Enums & Config
// =========================================================================

enum class ToolchainBackend {
    Auto,           // Detect best available
    MSVC,           // cl.exe + ml64.exe + link.exe (external)
    FromScratch,    // Custom assembler + linker (in-process)
    Hybrid          // MSVC compile → custom link
};

enum class BuildConfig {
    Debug,
    Release,
    RelWithDebInfo
};

enum class Subsystem {
    Console,
    Windows
};

enum class SourceLang {
    Unknown,
    Cpp,
    C,
    Asm,
    Header
};

enum class DiagLevel {
    Info,
    Warning,
    Error,
    Fatal
};

// =========================================================================
// Diagnostic — single compile/link message
// =========================================================================

struct BuildDiagnostic {
    DiagLevel    level;
    std::string  file;
    int          line    = 0;
    int          column  = 0;
    std::string  code;      // e.g. "C2065", "A1000", "LNK2019"
    std::string  message;
};

// =========================================================================
// Build Target — what to produce
// =========================================================================

struct BuildTarget {
    std::string              name;        // e.g. "test.exe"
    Subsystem                subsystem     = Subsystem::Console;
    std::string              entry_point;  // e.g. "main", "WinMainCRTStartup"
    std::vector<std::string> source_files; // .cpp, .c, .asm
    std::vector<std::string> include_dirs;
    std::vector<std::string> defines;
    std::vector<std::string> link_libs;    // kernel32.lib, etc.
    std::string              output_dir    = ".\\bin";
    std::string              obj_dir       = ".\\obj";
};

// =========================================================================
// Build Progress — pushed to UI callbacks
// =========================================================================

struct BuildProgress {
    int         files_total    = 0;
    int         files_done     = 0;
    std::string current_file;
    std::string phase;          // "Compile", "Assemble", "Link"
    bool        finished       = false;
    bool        success        = false;
    uint64_t    elapsed_ms     = 0;
    std::string output_path;
};

// Callback types
using DiagnosticCallback = std::function<void(const BuildDiagnostic&)>;
using ProgressCallback   = std::function<void(const BuildProgress&)>;
using OutputCallback     = std::function<void(const std::string&)>;

// =========================================================================
// ToolchainBridge — Main class
// =========================================================================

class ToolchainBridge {
public:
    ToolchainBridge();
    ~ToolchainBridge();

    // ---- Configuration ----
    void setBackend(ToolchainBackend backend)   { backend_ = backend; }
    void setConfig(BuildConfig config)          { config_ = config; }
    void setProjectRoot(const std::string& root){ project_root_ = root; }
    void setVerbose(bool v)                     { verbose_ = v; }

    // ---- Callbacks (set before build) ----
    void onDiagnostic(DiagnosticCallback cb) { on_diag_ = std::move(cb); }
    void onProgress(ProgressCallback cb)     { on_prog_ = std::move(cb); }
    void onOutput(OutputCallback cb)         { on_out_  = std::move(cb); }

    // ---- Toolchain status ----
    bool detectToolchain();
    bool isMSVCAvailable()       const { return !msvc_cl_.empty(); }
    bool isFromScratchAvailable() const { return from_scratch_ready_; }
    std::string getMl64Path()    const { return msvc_ml64_; }
    std::string getClPath()      const { return msvc_cl_; }
    std::string getLinkPath()    const { return msvc_link_; }

    // ---- Build operations (async) ----
    bool buildAsync(const BuildTarget& target);
    bool isBuilding() const { return building_.load(); }
    void cancelBuild()      { cancel_.store(true); }

    // ---- Build operations (sync, blocks) ----
    bool buildSync(const BuildTarget& target);

    // ---- Single-file operations ----
    bool compileFile(const std::string& path, const BuildTarget& target);
    bool assembleFile(const std::string& path, const BuildTarget& target);

    // ---- Source discovery ----
    static SourceLang detectLanguage(const std::string& path);
    static std::vector<std::string> discoverSources(
        const std::vector<std::string>& dirs,
        bool recursive = true);

    // ---- Diagnostics access ----
    const std::vector<BuildDiagnostic>& diagnostics() const { return diags_; }
    int errorCount()   const { return error_count_; }
    int warningCount() const { return warning_count_; }

private:
    // Toolchain detection
    bool findMSVC();
    bool findFromScratch();
    std::string findVsWhere();
    std::string queryVsPath();
    void importVcVars(const std::string& vcvars_path);

    // Compilation
    bool compileCpp(const std::string& src, const BuildTarget& tgt);
    bool compileAsm(const std::string& src, const BuildTarget& tgt);
    bool linkObjects(const std::vector<std::string>& objs,
                     const BuildTarget& tgt);

    // Custom toolchain (in-process)
    bool assembleCustom(const std::string& src, const BuildTarget& tgt);
    bool linkCustom(const std::vector<std::string>& objs,
                    const BuildTarget& tgt);

    // Process execution
    struct ProcessResult {
        int         exit_code;
        std::string stdout_text;
        std::string stderr_text;
        uint64_t    duration_ms;
    };
    ProcessResult runProcess(const std::string& cmdline,
                             const std::string& working_dir = "");

    // Diagnostic parsing
    void parseMsvcOutput(const std::string& output);
    void parseMl64Output(const std::string& output);
    void parseLinkOutput(const std::string& output);

    // Helpers
    void emit(const std::string& msg);
    void emitDiag(DiagLevel lvl, const std::string& file,
                  int line, const std::string& code, const std::string& msg);
    std::string objPath(const std::string& src, const BuildTarget& tgt);
    bool ensureDir(const std::string& dir);
    bool needsRebuild(const std::string& src, const std::string& obj);

    // State
    ToolchainBackend  backend_   = ToolchainBackend::Auto;
    BuildConfig       config_    = BuildConfig::Release;
    std::string       project_root_;
    bool              verbose_   = false;

    // MSVC paths
    std::string msvc_cl_;
    std::string msvc_ml64_;
    std::string msvc_link_;
    std::string msvc_lib_;
    std::string sdk_include_um_;
    std::string sdk_include_shared_;
    std::string sdk_include_ucrt_;
    std::string vc_include_;
    std::string sdk_lib_um_;
    std::string sdk_lib_ucrt_;
    std::string vc_lib_;

    // Custom toolchain
    bool from_scratch_ready_ = false;

    // Build state
    std::atomic<bool> building_{false};
    std::atomic<bool> cancel_{false};
    std::thread       build_thread_;
    std::mutex        diag_mutex_;

    // Diagnostics
    std::vector<BuildDiagnostic> diags_;
    int error_count_   = 0;
    int warning_count_ = 0;

    // Callbacks
    DiagnosticCallback on_diag_;
    ProgressCallback   on_prog_;
    OutputCallback     on_out_;
};

} // namespace RawrXD::Compiler
