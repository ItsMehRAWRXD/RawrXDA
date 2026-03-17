// build_self_heal_loop.hpp
// Self-Healing Build Loop — SOURCE-LEVEL compile-time error repair.
//
// Pipeline:
//   run() → runBuild() → parseErrors() → buildLLMPrompt()
//         → callOllama()  → applyFix() → runBuild() ... (N retries)
//
// Zero external dependencies beyond Win32 + WinHTTP (already linked).
// No exceptions thrown; every call returns a result value.
//
// All WinHTTP calls target Ollama at 127.0.0.1:11434/api/generate.
// -----------------------------------------------------------------
#pragma once

#ifndef _WIN32
#  error "BuildSelfHealLoop requires Win32"
#endif

#include <string>
#include <vector>
#include <cstdint>

namespace rawrxd {

// ─── Error record emitted by the compiler / linker ────────────────────────
struct BuildError {
    std::string file;     // absolute or relative source path
    int         line{0};  // 1-based; 0 = linker / global error
    std::string code;     // e.g. "C2143", "LNK2005", "A2006"
    std::string message;  // raw error text after the code
};

// ─── Result returned by a single heal attempt ─────────────────────────────
struct HealResult {
    bool        success{false};
    int         attemptsUsed{0};
    int         errorsOnEntry{0};   // # errors before healing started
    std::string buildOutput;        // final stdout+stderr from build
    std::string llmResponse;        // last raw LLM answer
    std::string detail;             // summary / failure reason
};

// ─── Patch record extracted from LLM response ─────────────────────────────
struct FilePatch {
    std::string relPath;        // relative path as emitted by LLM
    int         startLine{0};   // 1-based inclusive
    int         endLine{0};     // 1-based inclusive (lines to replace)
    std::string replacement;    // new lines (may be empty = delete)
};

// ─────────────────────────────────────────────────────────────────────────
// BuildSelfHealer
//   Drives the compile → diagnose → LLM-fix → recompile loop.
// ─────────────────────────────────────────────────────────────────────────
class BuildSelfHealer {
public:
    // Configuration ---------------------------------------------------------
    struct Config {
        std::string ollamaModel{"llama3"};      // Ollama model name
        std::string ollamaHost{"127.0.0.1"};    // Ollama HTTP host
        uint16_t    ollamaPort{11434};           // Ollama HTTP port
        int         maxRetries{5};               // max heal iterations
        int         contextLines{12};            // ±N source lines in prompt
        uint32_t    buildTimeoutMs{120'000};     // ms to wait for build proc
        uint32_t    ollamaTimeoutMs{60'000};     // ms to wait for LLM response
        bool        verbose{true};               // print progress to stdout
    };

    explicit BuildSelfHealer(Config cfg = {});
    ~BuildSelfHealer() = default;

    // Not copyable (owns HANDLE resources during run)
    BuildSelfHealer(const BuildSelfHealer&)            = delete;
    BuildSelfHealer& operator=(const BuildSelfHealer&) = delete;

    // ── Main entry point ───────────────────────────────────────────────────
    // buildCmd   : full shell command to build  (e.g. "powershell -File Build.ps1")
    // srcRoot    : root directory for resolving relative source paths
    // Returns HealResult.success == true when the build exits cleanly.
    HealResult run(const std::string& buildCmd,
                   const std::string& srcRoot);

    // ── Sub-steps (public for unit-level testing) ──────────────────────────
    // Execute buildCmd, capture combined stdout+stderr, return exit code.
    int runBuild(const std::string& buildCmd,
                 const std::string& workDir,
                 std::string&       outputOut);

    // Parse MSVC / ML64 / LINK error lines → BuildError vector.
    std::vector<BuildError> parseErrors(const std::string& buildOutput);

    // Build the LLM repair prompt from errors + surrounding source context.
    std::string buildLLMPrompt(const std::vector<BuildError>& errors,
                               const std::string& srcRoot);

    // POST prompt to Ollama, return raw LLM text (empty on failure).
    std::string callOllama(const std::string& prompt);

    // Extract FilePatch records from LLM response.
    std::vector<FilePatch> extractPatches(const std::string& llmResponse);

    // Apply a list of patches to the filesystem (atomic per-file).
    // Returns true if at least one patch was applied.
    bool applyPatches(const std::vector<FilePatch>& patches,
                      const std::string& srcRoot);

private:
    Config m_cfg;

    // Read ±contextLines lines around errorLine from file.
    // Returns annotated snippet string.
    std::string readContext(const std::string& absPath,
                            int errorLine,
                            int contextLines);

    // Resolve a potentially relative path against srcRoot.
    std::string resolvePath(const std::string& path,
                            const std::string& srcRoot);

    void log(const std::string& msg);
};

} // namespace rawrxd
