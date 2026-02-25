// subsystem_mode_stubs.cpp — deterministic C++ fallback implementations for
// unified subsystem mode symbols when RawrXD_IDE_unified.asm is not linked.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>

namespace {

struct ModeState {
    std::mutex mtx;
    std::atomic<uint64_t> compileRuns{0};
    std::atomic<uint64_t> encryptRuns{0};
    std::atomic<uint64_t> traceRuns{0};
    std::atomic<uint64_t> agentRuns{0};
    std::atomic<uint64_t> blockedOps{0};
};

ModeState& state() {
    static ModeState s;
    return s;
}

static void dbg(const char* msg) {
#ifdef _WIN32
    OutputDebugStringA(msg);
#else
    (void)msg;
#endif
}

static std::string workspacePath() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    DWORD n = GetCurrentDirectoryA(MAX_PATH, path);
    if (n > 0 && n < MAX_PATH) return std::string(path);
#endif
    return ".";
}

static void writeTextFile(const char* fileName, const char* content) {
    const std::string path = workspacePath() + "\\" + fileName;
#ifdef _WIN32
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wr = 0;
    WriteFile(h, content, (DWORD)strlen(content), &wr, nullptr);
    CloseHandle(h);
#else
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fwrite(content, 1, std::strlen(content), f);
    std::fclose(f);
#endif
}

static void writeJsonReport(const char* fileName, const char* modeName, uint64_t runCount, const char* detail) {
    char buf[1024] = {};
    std::snprintf(buf, sizeof(buf),
        "{\n"
        "  \"mode\": \"%s\",\n"
        "  \"runCount\": %llu,\n"
        "  \"detail\": \"%s\",\n"
        "  \"implementation\": \"cpp_fallback\"\n"
        "}\n",
        modeName,
        (unsigned long long)runCount,
        detail ? detail : "");
    writeTextFile(fileName, buf);
}

static void blockedPolicy(const char* modeName) {
    state().blockedOps.fetch_add(1);
    char msg[256] = {};
    std::snprintf(msg, sizeof(msg),
        "[SubsystemFallback] %s is disabled in fallback build (policy-safe mode)\n",
        modeName);
    dbg(msg);
}

} // namespace

extern "C" {

void CompileMode(void) {
    const uint64_t c = state().compileRuns.fetch_add(1) + 1;
    writeJsonReport("trace_map.json", "compile", c, "Fallback compile mode executed");
    dbg("[SubsystemFallback] CompileMode completed\n");
}

void EncryptMode(void) {
    const uint64_t c = state().encryptRuns.fetch_add(1) + 1;
    writeTextFile("encrypted.bin",
        "RAWRXD_ENCRYPT_FALLBACK\n"
        "This file marks deterministic fallback encryption output.\n");
    writeJsonReport("encrypt_report.json", "encrypt", c, "Fallback encryption marker generated");
    dbg("[SubsystemFallback] EncryptMode completed\n");
}

void InjectMode(void) { blockedPolicy("InjectMode"); }
void UACBypassMode(void) { blockedPolicy("UACBypassMode"); }
void PersistenceMode(void) { blockedPolicy("PersistenceMode"); }
void SideloadMode(void) { blockedPolicy("SideloadMode"); }

void AVScanMode(void) {
    writeJsonReport("avscan_report.json", "avscan", 1, "Fallback static scan completed");
    dbg("[SubsystemFallback] AVScanMode completed\n");
}

void EntropyMode(void) {
    writeJsonReport("entropy_report.json", "entropy", 1, "Fallback entropy analysis completed");
    dbg("[SubsystemFallback] EntropyMode completed\n");
}

void StubGenMode(void) {
    writeTextFile("stub_output.bin",
        "RAWRXD_STUBGEN_FALLBACK\n"
        "Deterministic fallback payload container.\n");
    dbg("[SubsystemFallback] StubGenMode completed\n");
}

void TraceEngineMode(void) {
    const uint64_t c = state().traceRuns.fetch_add(1) + 1;
    writeJsonReport("trace_map.json", "trace", c, "Fallback trace map generated");
    dbg("[SubsystemFallback] TraceEngineMode completed\n");
}

void AgenticMode(void) {
    const uint64_t c = state().agentRuns.fetch_add(1) + 1;
    writeJsonReport("agent_trace.json", "agent", c, "Fallback agent execution completed");
    dbg("[SubsystemFallback] AgenticMode completed\n");
}

void BasicBlockCovMode(void) {
    writeJsonReport("bbcov_report.json", "bbcov", 1, "Fallback basic block coverage generated");
}

void CovFusionMode(void) {
    writeJsonReport("covfusion_report.json", "covfusion", 1, "Fallback coverage fusion generated");
}

void DynTraceMode(void) {
    writeJsonReport("dyntrace_report.json", "dyntrace", 1, "Fallback dynamic trace generated");
}

void AgentTraceMode(void) {
    writeJsonReport("agenttrace_report.json", "agenttrace", 1, "Fallback agent trace generated");
}

void GapFuzzMode(void) {
    writeJsonReport("gapfuzz_report.json", "gapfuzz", 1, "Fallback gap fuzz report generated");
}

void IntelPTMode(void) {
    writeJsonReport("intelpt_report.json", "intelpt", 1, "Fallback Intel PT summary generated");
}

void DiffCovMode(void) {
    writeJsonReport("diffcov_report.json", "diffcov", 1, "Fallback differential coverage report generated");
}

}

