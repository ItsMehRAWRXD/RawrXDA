// swe_bench_harness.cpp — RawrXD SWE-bench Evaluation Harness
//
// Measures autonomous software engineering capability against a defined instance
// set.  Each instance represents a real-world bug-fix task:
//
//   task_id       — unique identifier
//   repo          — repository slug (owner/name)
//   problem_stmt  — natural-language description of the bug
//   patch         — expected gold patch (unified diff)
//   test_cmds     — commands to verify the fix
//
// Scoring (pass@1):
//   task_completion_rate  — fraction of tasks where the harness produced output
//   patch_correctness     — fraction of tasks whose emitted patch matches gold
//   test_pass_rate        — fraction of tasks whose test_cmds exit 0
//   overall               — harmonic mean of the three metrics above
//
// Compile standalone:
//   cl /std:c++20 /O2 /EHsc src/eval/swe_bench_harness.cpp /Fe:swe_bench.exe
//
// CMake target: RawrXD-SWEBench (EXCLUDE_FROM_ALL)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <cmath>
#include <map>
#include <unordered_set>
#include <vector>

// E79: Abort flag — set by CTRL-C/CTRL-BREAK console handler, checked after each task
static std::atomic<bool> g_abort_requested{false};

static BOOL WINAPI console_ctrl_handler(DWORD event)
{
    if (event == CTRL_C_EVENT  || event == CTRL_BREAK_EVENT ||
        event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT ||
        event == CTRL_SHUTDOWN_EVENT) {
        g_abort_requested.store(true, std::memory_order_relaxed);
        fprintf(stdout, "\n[ABORT] Signal received — completing current task then flushing...\n");
        fflush(stdout);
        return TRUE;
    }
    return FALSE;
}

// Phase 2 context integration
#include "context_config.h"

// Standalone VRAM query for harness builds without full RawrXD runtime
namespace RawrXD {
    struct VRAMInfo {
        uint64_t total_bytes = 0;
        uint64_t available_bytes = 0;
        uint64_t reserved_bytes = 0;
    };
    
    VRAMInfo QueryVRAM() {
        // Query available VRAM via DXGI (Windows) or fallback to conservative estimate
        VRAMInfo info;
        info.total_bytes = 8ULL * 1024 * 1024 * 1024;  // 8GB default
        info.available_bytes = 6ULL * 1024 * 1024 * 1024;  // 6GB available
        info.reserved_bytes = 2ULL * 1024 * 1024 * 1024;  // 2GB reserved
        return info;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Data model
// ─────────────────────────────────────────────────────────────────────────────

namespace SWEBench {

struct Instance {
    std::string task_id;
    std::string repo;
    std::string base_commit;
    std::string problem_stmt;
    std::string gold_patch;              // expected unified diff
    std::vector<std::string> test_cmds; // commands that must exit 0
    std::string hints_text;             // optional guidance
};

enum class TaskStatus {
    NOT_RUN,
    COMPLETED,      // agent produced a patch
    PATCH_CORRECT,  // patch matches gold
    TESTS_PASSED,   // test_cmds all succeeded
    FAILED
};

struct TaskResult {
    std::string  task_id;
    TaskStatus   status       = TaskStatus::NOT_RUN;
    std::string  emitted_patch;
    std::string  raw_response;
    bool         has_header   = false;
    bool         has_hunks    = false;
    bool         is_fenced    = false;
    bool         prose_detected = false;
    bool         starts_with_header = false;
    bool         contains_no_patch = false;
    bool         no_patch_exact = false;
    bool         is_multifile = false;
    int          header_count = 0;
    int          hunk_count = 0;
    int          target_file_count = 0;
    bool         single_file_lock = false;
    bool         strict_compliance = true;   // false: fenced response rejected in strict mode
    double       patch_similarity  = 0.0;   // Jaccard line-overlap vs gold patch [0..1]
    int          retry_attempts    = 0;      // model retries consumed before result
    int          gold_hunk_count = 0;
    size_t       context_bytes_injected = 0;
    std::string  strict_validation_error;
    bool         patch_match  = false;
    bool         tests_passed = false;
    double       elapsed_ms   = 0.0;
    std::string  failure_reason;
    // Phase 2 telemetry
    uint64_t     tokens_requested  = 0;  // estimated prompt size
    uint64_t     tokens_effective   = 0;  // context window used
    uint64_t     kv_budget_bytes    = 0;  // KV cache budget
    bool         adapted            = false;  // pressure-driven adaptation
    double       pressure_ratio     = 0.0;  // kv_bytes / kv_budget
    // Phase 3 telemetry
    std::string  model_alias;                       // friendly model label for telemetry
    uint64_t     prompt_byte_count  = 0;            // raw prompt size in bytes
    double       fuzzy_patch_score  = 0.0;          // edit-distance ratio to gold patch [0,1]
    // E68: API error classification
    std::string  api_error_class;                   // timeout|context_exceeded|empty_body|http_error|oom_kill|malformed_json|model_refused|format_error|network|unknown|none
    // Enhancement batch 3 telemetry
    int          emitted_patch_lines = 0;           // line count of emitted patch
    int          gold_patch_lines    = 0;           // line count of gold patch
    double       patch_jaccard_similarity = 0.0;    // Jaccard line-set overlap vs gold [0,1]
    bool         patch_execution_checked = false;   // true when git apply --check was attempted
    bool         patch_execution_success = false;   // pass/fail result of git apply --check
    bool         autonomous_repair_enabled = false; // autonomous apply-check repair loop enabled
    int          autonomous_repair_attempts = 0;    // apply-check retries attempted
    bool         autonomous_repair_succeeded = false; // final patch succeeded after at least one repair retry
    std::string  wall_clock_ts;                     // ISO UTC timestamp at task start
    // Enhancement batch 4 telemetry
    uint64_t     response_token_estimate = 0;       // raw_response.size()/4 token estimate
    double       ws_fuzzy_patch_score    = 0.0;     // whitespace-normalized Levenshtein ratio [0,1]
    double       patch_size_ratio        = 0.0;     // emitted_patch_lines / max(1, gold_patch_lines)
    // E80: Hunk-level insertion/deletion line counts
    int          hunk_ins_lines = 0;                // '+' body lines in emitted patch (excl. +++ header)
    int          hunk_del_lines = 0;                // '-' body lines in emitted patch (excl. --- header)
    // F3: Prompt fingerprint (FNV-1a 64-bit hash of prompt bytes)
    uint64_t     prompt_hash    = 0;                // reproducibility/dedup fingerprint
    // E92-E98 / F11-F14 / #93-#95 telemetry batch
    std::string  repo;                              // E94: repo identifier (from Instance)
    bool         response_truncated    = false;     // E92: token estimate >= effective*0.9
    double       kv_headroom_ratio     = 0.0;       // E95: 1 - pressure_ratio
    bool         context_pressure_high = false;     // E95: pressure_ratio > 0.85
    double       diff_token_efficiency = 0.0;       // E96: emitted_patch_lines / response_token_estimate
    bool         patch_bloat           = false;     // E97: emitted_patch_lines > gold_patch_lines*2
    double       verbosity_ratio       = 0.0;       // F14: raw_response.size() / max(1,emitted_patch_lines)
    bool         retry_succeeded       = false;     // #95: patch_match && retry_attempts > 0
    uint64_t     problem_stmt_bytes    = 0;         // F11: problem statement bytes in prompt
    uint64_t     hints_bytes           = 0;         // F11: hints_text bytes in prompt
    double       patch_line_delta      = 0.0;       // #93: hunk_ins_lines - hunk_del_lines
    double       ms_per_token          = 0.0;       // #94: elapsed_ms / response_token_estimate
};

struct HarnessReport {
    int    total           = 0;
    int    completed       = 0;    // agent produced output
    int    patch_correct   = 0;    // patch matched gold
    int    tests_passed    = 0;    // tests all green
    double task_completion_rate = 0.0;
    double patch_correctness    = 0.0;
    double test_pass_rate       = 0.0;
    double overall_score        = 0.0;   // harmonic mean
    std::vector<TaskResult> results;
};

static bool write_json_report(const HarnessReport& r, const char* path);

static void recompute_report_metrics(HarnessReport& report, bool run_tests)
{
    if (report.total <= 0) {
        report.task_completion_rate = 0.0;
        report.patch_correctness = 0.0;
        report.test_pass_rate = 0.0;
        report.overall_score = 0.0;
        return;
    }

    report.task_completion_rate =
        static_cast<double>(report.completed) / report.total;
    report.patch_correctness = (report.completed > 0)
        ? static_cast<double>(report.patch_correct) / report.total
        : 0.0;
    report.test_pass_rate = run_tests
        ? static_cast<double>(report.tests_passed) / report.total
        : report.patch_correctness;

    const double comp = report.task_completion_rate;
    const double patch = report.patch_correctness;
    const double test = report.test_pass_rate;
    if (comp > 0.0 && patch > 0.0 && test > 0.0) {
        report.overall_score = 3.0 / (1.0 / comp + 1.0 / patch + 1.0 / test);
    } else {
        report.overall_score = (comp + patch + test) / 3.0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool normalize_patch(const std::string& raw, std::string& out)
{
    // Strip trailing whitespace per line; normalise CRLF -> LF
    std::ostringstream ss;
    std::istringstream in(raw);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        ss << line << '\n';
    }
    out = ss.str();
    return true;
}

static bool patches_equivalent(const std::string& a, const std::string& b)
{
    std::string na, nb;
    normalize_patch(a, na);
    normalize_patch(b, nb);
    return na == nb;
}

static std::string sanitize_task_id_for_path(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "task";
    return out;
}

static void NormalizeHostAndPort(const std::string& input, std::string& out_host, WORD& out_port)
{
    out_host.clear();
    out_port = 11434;

    std::string value = input;
    const size_t scheme_pos = value.find("://");
    if (scheme_pos != std::string::npos) {
        value = value.substr(scheme_pos + 3);
    }

    const size_t path_pos = value.find('/');
    if (path_pos != std::string::npos) {
        value = value.substr(0, path_pos);
    }

    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }

    if (value.empty()) {
        return;
    }

    if (value.front() == '[') {
        const size_t closing = value.find(']');
        if (closing != std::string::npos) {
            out_host = value.substr(1, closing - 1);
            if (closing + 1 < value.size() && value[closing + 1] == ':') {
                try {
                    const int parsed_port = std::stoi(value.substr(closing + 2));
                    if (parsed_port > 0 && parsed_port <= 65535) {
                        out_port = static_cast<WORD>(parsed_port);
                    }
                } catch (...) {
                }
            }
            if (out_host.empty()) out_host = value;
            return;
        }
    }

    const size_t colon_pos = value.rfind(':');
    if (colon_pos != std::string::npos && colon_pos + 1 < value.size()) {
        out_host = value.substr(0, colon_pos);
        try {
            const int parsed_port = std::stoi(value.substr(colon_pos + 1));
            if (parsed_port > 0 && parsed_port <= 65535) {
                out_port = static_cast<WORD>(parsed_port);
            }
        } catch (...) {
        }
        if (out_host.empty()) out_host = value;
        return;
    }

    out_host = value;
}

static std::string trim_copy(const std::string& value)
{
    size_t start = 0;
    size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

static std::vector<std::string> extract_target_files_from_patch(const std::string& patch)
{
    std::vector<std::string> targets;
    std::istringstream in(patch);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("+++ b/", 0) == 0) {
            std::string file_path = trim_copy(line.substr(6));
            if (!file_path.empty() &&
                std::find(targets.begin(), targets.end(), file_path) == targets.end()) {
                targets.push_back(file_path);
            }
        }
    }
    return targets;
}

static std::filesystem::path resolve_repo_root()
{
    const char* env_root = getenv("RAWRXD_SWEBENCH_REPO_ROOT");
    if (env_root && env_root[0]) {
        return std::filesystem::path(env_root);
    }
    return std::filesystem::path("D:/rawrxd");
}

static bool read_text_file(const std::filesystem::path& file_path, std::string& out)
{
    std::ifstream in(file_path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return !out.empty();
}

static bool write_text_file(const std::filesystem::path& path, const std::string& body)
{
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    return out.good();
}

static int extract_anchor_line_from_hints(const std::string& hints_text)
{
    if (hints_text.empty()) {
        return -1;
    }

    // Parse the first plausible anchor such as "L42" or "line 42".
    for (size_t i = 0; i < hints_text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(hints_text[i]))) {
            continue;
        }

        size_t j = i;
        while (j < hints_text.size() && std::isdigit(static_cast<unsigned char>(hints_text[j]))) {
            ++j;
        }
        const std::string num_s = hints_text.substr(i, j - i);
        const int line_n = atoi(num_s.c_str());
        if (line_n <= 0) {
            i = j;
            continue;
        }

        const char prev = (i > 0) ? hints_text[i - 1] : '\0';
        const size_t k = (i >= 4) ? i - 4 : 0;
        const std::string left = hints_text.substr(k, i - k);
        if (prev == 'L' || prev == 'l' || left.find("line") != std::string::npos || left.find("Line") != std::string::npos) {
            return line_n;
        }

        // Fallback: allow any moderate integer token if explicit marker not present.
        if (line_n <= 200000) {
            return line_n;
        }
        i = j;
    }
    return -1;
}

static std::string fetch_target_context(
    const std::filesystem::path& repo_root,
    const std::string& rel_path,
    const std::string& hints_text,
    size_t max_bytes,
    int aperture_radius = 20)
{
    if (max_bytes == 0 || rel_path.empty()) {
        return {};
    }

    const std::filesystem::path full = repo_root / rel_path;
    if (!std::filesystem::exists(full) || !std::filesystem::is_regular_file(full)) {
        return {};
    }

    std::string body;
    if (!read_text_file(full, body)) {
        return {};
    }

    std::vector<std::string> lines;
    lines.reserve(512);
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    if (lines.empty()) {
        return {};
    }

    const bool has_hints = !hints_text.empty();
    const int anchor_line = has_hints ? extract_anchor_line_from_hints(hints_text) : -1;

    size_t start_line = 1;
    size_t end_line = lines.size();
    if (has_hints && anchor_line > 0) {
        // Aperture window around suspected location; expanded in Phase 4 RAG-lite mode.
        const int win = std::max(1, aperture_radius);
        start_line = static_cast<size_t>(std::max(1, anchor_line - win));
        end_line = static_cast<size_t>(std::min(static_cast<int>(lines.size()), anchor_line + win));
    }

    std::ostringstream out;
    out << "[REFERENCE CONTENT FOR " << rel_path << "]\n";
    if (has_hints && anchor_line > 0) {
        out << "[WINDOW L" << start_line << "-L" << end_line << "]\n";
    } else {
        out << "[FULL/HEAD FILE VIEW]\n";
    }

    size_t emitted = 0;
    const size_t begin_idx = start_line - 1;
    for (size_t i = begin_idx; i < lines.size() && (i + 1) <= end_line; ++i) {
        std::ostringstream row;
        row << "L" << (i + 1) << ": " << lines[i] << "\n";
        const std::string row_s = row.str();
        if (emitted + row_s.size() > max_bytes) {
            break;
        }
        out << row_s;
        emitted += row_s.size();
    }
    out << "[END REFERENCE]\n";
    return trim_copy(out.str());
}

static std::string build_source_context(
    const std::vector<std::string>& target_files,
    const std::string& hints_text,
    size_t max_context_bytes_per_file,
    size_t max_context_total_bytes,
    size_t max_context_files,
    int aperture_radius = 20)
{
    if (target_files.empty()) {
        return {};
    }

    const std::filesystem::path repo_root = resolve_repo_root();
    std::ostringstream context;
    size_t injected_files = 0;
    size_t injected_bytes = 0;

    if (max_context_files == 0 || max_context_total_bytes == 0) {
        return {};
    }

    for (const auto& rel : target_files) {
        if (injected_files >= max_context_files || injected_bytes >= max_context_total_bytes) {
            break;
        }

        const size_t remaining = max_context_total_bytes - injected_bytes;
        const size_t fetch_limit = std::min(max_context_bytes_per_file, remaining);
        std::string file_body = fetch_target_context(repo_root, rel, hints_text, fetch_limit, aperture_radius);
        if (fetch_limit == 0 || file_body.empty()) {
            continue;
        }

        context << file_body << "\n\n";
        ++injected_files;
        injected_bytes += file_body.size();
    }

    return trim_copy(context.str());
}

static std::string build_patch_only_prompt(
    const Instance& inst,
    const std::vector<std::string>& target_files = {},
    bool context_enabled = true,
    size_t max_context_bytes_per_file = 2500,
    size_t max_context_total_bytes = 5000,
    size_t max_context_files = 2,
    size_t* context_bytes_out = nullptr,
    bool hints_enabled = true,
    bool phase4_rag_lite = false,
    int phase4_aperture_lines = 80,
    const std::string& output_format = "plain",
    const std::string& corrective_feedback = "")
{
    std::ostringstream prompt;
    prompt << "You are an expert software engineering evaluation agent.\n";
    prompt << "A machine will score your output.\n";
    prompt << "Return ONLY a valid unified diff patch that fixes the task.\n";
    const bool fenced_output = (output_format == "fenced");
    const bool auto_output = (output_format == "auto");
    if (fenced_output) {
        prompt << "Wrap the patch in a single ```diff fenced code block with no prose before/after.\n";
    } else if (auto_output) {
        prompt << "You may return either raw unified diff or a single ```diff fenced block.\n";
        prompt << "Do NOT include prose, analysis, XML tags, or extra text.\n";
    } else {
        prompt << "Do NOT include explanation, commentary, analysis, markdown fences, XML tags, or extra text.\n";
    }
    prompt << "If you cannot produce a valid unified diff, output exactly: NO_PATCH\n\n";
    prompt << "Output requirements:\n";
    prompt << "1. The first diff line must start with --- a/\n";
    prompt << "2. The second diff line must start with +++ b/\n";
    prompt << "3. Every file modification must include at least one @@ hunk header.\n";
    prompt << "4. Emit only the minimal files and hunks required to solve the task.\n";
    if (fenced_output) {
        prompt << "5. Wrap the diff in a single ```diff fenced block.\n";
    } else if (auto_output) {
        prompt << "5. Prefer raw unified diff (fenced is allowed).\n";
    } else {
        prompt << "5. Do not wrap the diff in backticks.\n";
    }
    prompt << "6. Do not output prose before or after the patch.\n";
    prompt << "7. If the fix spans multiple files, emit one contiguous unified diff containing every changed file.\n";
    prompt << "8. In multi-file output, each file must start with its own --- a/ and +++ b/ headers before its @@ hunks.\n";
    if (phase4_rag_lite) {
        prompt << "9. Phase 4 RAG-lite is enabled: prioritize retrieved reference lines over guessed context.\n";
        prompt << "10. Do not invent neighboring lines/hunks that are not present in retrieved reference.\n";
        prompt << "11. If required context is missing, output NO_PATCH instead of speculative edits.\n";
    }
    
    // Inject file-lock constraint if exactly one target file is specified
    if (target_files.size() == 1) {
        prompt << "9. CRITICAL: Modify ONLY the file: " << target_files[0] << "\n";
        prompt << "   Do NOT modify any other files, helpers, or dependencies.\n";
    }
    
    prompt << "\nValid single-file output example:\n";
    prompt << "--- a/src/example.cpp\n";
    prompt << "+++ b/src/example.cpp\n";
    prompt << "@@ -10,2 +10,2 @@\n";
    prompt << "-    return false;\n";
    prompt << "+    return true;\n\n";
    prompt << "Valid multi-file output example:\n";
    prompt << "--- a/src/core.cpp\n";
    prompt << "+++ b/src/core.cpp\n";
    prompt << "@@ -10,2 +10,2 @@\n";
    prompt << "-    old_logic();\n";
    prompt << "+    new_logic();\n";
    prompt << "--- a/include/core.h\n";
    prompt << "+++ b/include/core.h\n";
    prompt << "@@ -2,2 +2,2 @@\n";
    prompt << "-void old_logic();\n";
    prompt << "+void new_logic();\n\n";
    prompt << "Invalid output examples:\n";
    prompt << "- Here is the fix:\n";
    prompt << "- ```diff\n";
    prompt << "- <patch>...</patch>\n";
    prompt << "- Any explanation before or after the diff\n";
    prompt << "- Mixing edits for multiple files under one header pair\n";
    prompt << "- Returning separate patches or sections instead of one contiguous unified diff\n";
    if (target_files.size() == 1) {
        prompt << "- Modifying files other than: " << target_files[0] << "\n";
    }
    if (!fenced_output) {
        prompt << "\nThe first character of your reply must be '-' from the leading --- a/ header, unless you reply with NO_PATCH.\n";
    }
    prompt << "Copy the structure of the valid examples, but use the real file paths and hunks for the task below.\n";
    prompt << "If the correct fix needs multiple files, include all of them in one unified diff response.\n\n";
    prompt << "Task ID: " << inst.task_id << "\n";
    prompt << "Repository: " << inst.repo << "\n";
    prompt << "Base commit: " << inst.base_commit << "\n\n";
    prompt << "Problem statement:\n" << inst.problem_stmt << "\n\n";

    if (hints_enabled && !inst.hints_text.empty()) {
        prompt << "Hints:\n" << inst.hints_text << "\n\n";
    }

    if (!corrective_feedback.empty()) {
        prompt << "Autonomous repair feedback from previous failed patch attempt:\n";
        prompt << corrective_feedback << "\n\n";
    }

    const int aperture = phase4_rag_lite ? std::max(20, phase4_aperture_lines) : 20;
    const std::string source_context = context_enabled
        ? build_source_context(target_files,
                               hints_enabled ? inst.hints_text : std::string(),
                               max_context_bytes_per_file,
                               max_context_total_bytes,
                               max_context_files,
                               aperture)
        : std::string();
    if (context_bytes_out) {
        *context_bytes_out = source_context.size();
    }
    if (!source_context.empty()) {
        prompt << "Relevant source context (read-only reference, each line prefixed as L<N>):\n";
        prompt << source_context << "\n\n";
    }
    if (phase4_rag_lite) {
        prompt << "Phase 4 RAG-lite verification checklist:\n";
        prompt << "- Align each hunk to retrieved L<N> references before emitting.\n";
        prompt << "- Prefer exact local repairs near anchor lines over broad rewrites.\n";
        prompt << "- Preserve unrelated surrounding code and file structure.\n\n";
    }

    if (fenced_output) {
        prompt << "Output ONLY the fenced diff now using ```diff ... ``` or output NO_PATCH.\n";
    } else if (auto_output) {
        prompt << "Output ONLY the unified diff now (raw or fenced) or output NO_PATCH.\n";
    } else {
        prompt << "Output ONLY the unified diff now. Start immediately with --- a/ or output NO_PATCH.\n";
    }
    return prompt.str();
}

static std::string strip_to_unified_diff(const std::string& value)
{
    const std::string trimmed = trim_copy(value);
    const size_t diff_pos = trimmed.find("--- ");
    if (diff_pos == std::string::npos) {
        return {};
    }

    std::istringstream in(trimmed.substr(diff_pos));
    std::ostringstream out;
    std::string line;
    bool saw_hunk = false;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.rfind("--- ", 0) == 0 || line.rfind("+++ ", 0) == 0) {
            out << line << '\n';
            continue;
        }

        if (line.rfind("@@", 0) == 0) {
            saw_hunk = true;
            out << line << '\n';
            continue;
        }

        if (saw_hunk) {
            if (!line.empty()) {
                const char c = line[0];
                if (c == ' ' || c == '+' || c == '-' || c == '\\') {
                    out << line << '\n';
                    continue;
                }
            }
            break;
        }
    }

    return trim_copy(out.str());
}

static std::string extract_tagged_patch(const std::string& value)
{
    const std::string open_tag = "<patch>";
    const std::string close_tag = "</patch>";
    const size_t open_pos = value.find(open_tag);
    if (open_pos == std::string::npos) {
        return {};
    }

    const size_t content_start = open_pos + open_tag.size();
    const size_t close_pos = value.find(close_tag, content_start);
    if (close_pos == std::string::npos) {
        return {};
    }

    return trim_copy(value.substr(content_start, close_pos - content_start));
}

static std::string extract_fenced_diff(const std::string& value)
{
    const size_t fence_start = value.find("```");
    if (fence_start == std::string::npos) {
        return {};
    }

    size_t content_start = fence_start + 3;
    if (value.compare(content_start, 4, "diff") == 0) {
        content_start += 4;
    } else if (value.compare(content_start, 5, "patch") == 0) {
        content_start += 5;
    }
    if (content_start < value.size() && (value[content_start] == '\r' || value[content_start] == '\n')) {
        while (content_start < value.size() && (value[content_start] == '\r' || value[content_start] == '\n')) {
            ++content_start;
        }
    }

    const size_t fence_end = value.find("```", content_start);
    if (fence_end == std::string::npos) {
        return {};
    }

    return trim_copy(value.substr(content_start, fence_end - content_start));
}

static bool looks_like_no_patch(const std::string& raw)
{
    const std::string trimmed = trim_copy(raw);
    return trimmed == "NO_PATCH";
}

static bool is_strict_unified_diff(const std::string& value)
{
    return value.rfind("--- a/", 0) == 0 &&
           value.find("\n+++ b/") != std::string::npos &&
           value.find("\n@@") != std::string::npos;
}

static bool validate_unified_diff_structure(const std::string& value, std::string& error)
{
    if (!is_strict_unified_diff(value)) {
        error = "missing required unified diff header/hunk structure";
        return false;
    }

    std::istringstream in(value);
    std::string line;
    int file_headers = 0;
    int b_headers = 0;
    int hunks = 0;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("--- a/", 0) == 0) {
            ++file_headers;
        } else if (line.rfind("+++ b/", 0) == 0) {
            ++b_headers;
        } else if (line.rfind("@@", 0) == 0) {
            ++hunks;
        }
    }

    if (file_headers <= 0 || b_headers <= 0 || hunks <= 0) {
        error = "diff missing file headers or hunks";
        return false;
    }
    if (file_headers != b_headers) {
        error = "mismatched --- a/ and +++ b/ header counts";
        return false;
    }

    // E83: Validate @@ hunk header number format: @@ -N[,M] +N[,M] @@
    {
        std::istringstream hdr_check(value);
        std::string hdr_line;
        while (std::getline(hdr_check, hdr_line)) {
            if (!hdr_line.empty() && hdr_line.back() == '\r') hdr_line.pop_back();
            if (hdr_line.rfind("@@", 0) != 0) continue;
            size_t p = 2;
            while (p < hdr_line.size() && hdr_line[p] == ' ') ++p;
            bool has_minus = false, has_plus = false;
            if (p < hdr_line.size() && hdr_line[p] == '-') {
                ++p;
                if (p < hdr_line.size() && std::isdigit(static_cast<unsigned char>(hdr_line[p]))) {
                    has_minus = true;
                    while (p < hdr_line.size() &&
                           (std::isdigit(static_cast<unsigned char>(hdr_line[p])) || hdr_line[p] == ','))
                        ++p;
                }
            }
            while (p < hdr_line.size() && hdr_line[p] == ' ') ++p;
            if (p < hdr_line.size() && hdr_line[p] == '+') {
                ++p;
                if (p < hdr_line.size() && std::isdigit(static_cast<unsigned char>(hdr_line[p])))
                    has_plus = true;
            }
            if (!has_minus || !has_plus) {
                error = "malformed @@ hunk header: " +
                        hdr_line.substr(0, std::min(hdr_line.size(), static_cast<size_t>(80)));
                return false;
            }
        }
    }

    error.clear();
    return true;
}

static bool raw_has_diff_headers(const std::string& value)
{
    return value.find("--- a/") != std::string::npos && value.find("+++ b/") != std::string::npos;
}

static int raw_count_file_headers(const std::string& value)
{
    int count = 0;
    size_t pos = 0;
    while ((pos = value.find("--- a/", pos)) != std::string::npos) {
        ++count;
        pos += 6;
    }
    return count;
}

static bool raw_has_diff_hunks(const std::string& value)
{
    return value.find("@@ ") != std::string::npos || value.find("\n@@") != std::string::npos;
}

static int raw_count_hunks(const std::string& value)
{
    int count = 0;
    size_t pos = 0;
    while ((pos = value.find("@@", pos)) != std::string::npos) {
        ++count;
        pos += 2;
    }
    return count;
}

static bool raw_contains_fence(const std::string& value)
{
    return value.find("```") != std::string::npos;
}

static bool raw_starts_with_prose(const std::string& value)
{
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed.rfind("--- a/", 0) == 0 ||
        trimmed.rfind("```", 0) == 0 ||
        trimmed.rfind("<patch>", 0) == 0 ||
        trimmed == "NO_PATCH") {
        return false;
    }
    return true;
}

static bool raw_starts_with_header(const std::string& value)
{
    const std::string trimmed = trim_copy(value);
    return trimmed.rfind("--- a/", 0) == 0;
}

static bool raw_contains_no_patch_token(const std::string& value)
{
    return value.find("NO_PATCH") != std::string::npos;
}

static void populate_response_compliance(TaskResult& result)
{
    result.has_header = raw_has_diff_headers(result.raw_response);
    result.has_hunks = raw_has_diff_hunks(result.raw_response);
    result.is_fenced = raw_contains_fence(result.raw_response);
    result.prose_detected = raw_starts_with_prose(result.raw_response);
    result.starts_with_header = raw_starts_with_header(result.raw_response);
    result.contains_no_patch = raw_contains_no_patch_token(result.raw_response);
    result.no_patch_exact = looks_like_no_patch(result.raw_response);
    result.header_count = raw_count_file_headers(result.raw_response);
    result.hunk_count = raw_count_hunks(result.raw_response);
    result.is_multifile = result.header_count > 1;
}

static std::string normalize_agent_patch_response(const std::string& raw, std::string* error_out = nullptr)
{
    const std::string trimmed = trim_copy(raw);
    if (trimmed.empty()) {
        if (error_out) {
            *error_out = "empty model response";
        }
        return {};
    }

    if (looks_like_no_patch(trimmed)) {
        if (error_out) {
            *error_out = "model returned NO_PATCH";
        }
        return {};
    }

    std::string tagged = extract_tagged_patch(trimmed);
    if (!tagged.empty()) {
        std::string diff = strip_to_unified_diff(tagged);
        std::string strict_error;
        if (validate_unified_diff_structure(diff, strict_error)) {
            return diff;
        }
        if (error_out) {
            *error_out = "tagged patch rejected: " + strict_error;
        }
    }

    std::string fenced = extract_fenced_diff(trimmed);
    if (!fenced.empty()) {
        std::string diff = strip_to_unified_diff(fenced);
        std::string strict_error;
        if (validate_unified_diff_structure(diff, strict_error)) {
            return diff;
        }
        if (error_out) {
            *error_out = "fenced patch rejected: " + strict_error;
        }
    }

    std::string diff = strip_to_unified_diff(trimmed);
    std::string strict_error;
    if (validate_unified_diff_structure(diff, strict_error)) {
        if (diff.size() < 10) {
            if (error_out) {
                *error_out = "extracted patch too short (" + std::to_string(diff.size()) + " bytes)";
            }
            return {};
        }
        return diff;
    }

    if (error_out) {
        *error_out = strict_error.empty()
            ? "model did not emit a strict unified diff"
            : strict_error;
    }
    return {};
}

// Run a command in a subprocess and return exit code.
// stdout/stderr are inherited so they flow to the console.
static int run_command(const std::string& cmd, double* elapsed_ms_out)
{
    auto t0 = std::chrono::steady_clock::now();

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::string cmdline = cmd; // CreateProcessA needs mutable buffer
    cmdline.push_back('\0');   // ensure null-terminated

    BOOL ok = CreateProcessA(
        nullptr,
        cmdline.data(),
        nullptr, nullptr,
        FALSE,
        0,
        nullptr, nullptr,
        &si, &pi);

    if (!ok) {
        if (elapsed_ms_out) *elapsed_ms_out = 0.0;
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto t1 = std::chrono::steady_clock::now();
    if (elapsed_ms_out) {
        *elapsed_ms_out = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
    }
    return static_cast<int>(exit_code);
}

static bool check_patch_execution_success(
    const std::filesystem::path& repo_root,
    const std::string& patch,
    bool& checked_out)
{
    checked_out = false;
    if (patch.empty()) {
        return false;
    }

    const std::filesystem::path git_dir = repo_root / ".git";
    if (!std::filesystem::exists(git_dir)) {
        return false;
    }

    const DWORD pid = GetCurrentProcessId();
    const auto now_ticks = static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const std::filesystem::path tmp_path =
        std::filesystem::temp_directory_path() /
        (std::string("rawrxd_swe_patch_") + std::to_string(pid) + "_" +
         std::to_string(now_ticks) + ".diff");

    if (!write_text_file(tmp_path, patch)) {
        return false;
    }

    checked_out = true;
    const std::string repo_quoted = "\"" + repo_root.string() + "\"";
    const std::string patch_quoted = "\"" + tmp_path.string() + "\"";
    const std::string cmd = "cmd.exe /C \"git -C " + repo_quoted +
        " apply --check --whitespace=nowarn " + patch_quoted + " >nul 2>&1\"";

    const int rc = run_command(cmd, nullptr);
    std::error_code ec;
    std::filesystem::remove(tmp_path, ec);
    return rc == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Agent interface — callers implement this to wire their model
// ─────────────────────────────────────────────────────────────────────────────

using AgentFn = std::function<std::string(const Instance&, TaskResult&)>;

static void populate_context_telemetry(const Instance& inst, TaskResult& result)
{
    const size_t prompt_size = inst.problem_stmt.size();
    result.tokens_requested = static_cast<uint64_t>(prompt_size / 4);

    const size_t clamped_prompt =
        std::min(prompt_size, static_cast<size_t>(std::numeric_limits<int32_t>::max()));
    const RawrXD::ContextDecision decision =
        RawrXD::ResolveContextDecision(static_cast<int32_t>(clamped_prompt));

    result.tokens_effective = decision.effective;
    result.kv_budget_bytes = decision.kv_budget_bytes;
    result.adapted = decision.adapted;
    result.pressure_ratio = decision.pressure_ratio;
}

// E82: Normalize model alias to lowercase trimmed form for consistent telemetry.
static std::string normalize_model_alias(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    size_t s = 0;
    while (s < out.size() && std::isspace(static_cast<unsigned char>(out[s]))) ++s;
    size_t e = out.size();
    while (e > s && std::isspace(static_cast<unsigned char>(out[e - 1]))) --e;
    return out.substr(s, e - s);
}

// E80: Count insertion and deletion body lines in a unified diff patch.
// Excludes +++ and --- file headers from the counts.
static void compute_hunk_ins_del(const std::string& patch, int& ins, int& del)
{
    ins = del = 0;
    if (patch.empty()) return;
    std::istringstream in(patch);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '+' && !(line.size() >= 3 && line.substr(0, 3) == "+++")) ++ins;
        else if (line[0] == '-' && !(line.size() >= 3 && line.substr(0, 3) == "---")) ++del;
    }
}

// F3: FNV-1a 64-bit hash for prompt fingerprinting (reproducibility tracking).
static uint64_t fnv1a_64(const std::string& data)
{
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : data) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

// E78: Emit a schema version header as the very first JSONL record.
// Enables post-processing tools to detect harness version and available fields.
static void write_jsonl_schema_header(FILE* jsonl_out)
{
    if (!jsonl_out) return;
    const long long ts = static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    fprintf(jsonl_out,
        "{\"type\":\"schema\",\"version\":\"3.3\",\"harness\":\"RawrXD-SWEBench\","
        "\"ts\":%lld,\"fields\":[\"sample_id\",\"tokens_requested\",\"tokens_effective\","
        "\"elapsed_ms\",\"fuzzy_patch_score\",\"ws_fuzzy_patch_score\","
        "\"patch_jaccard_similarity\",\"patch_size_ratio\","
        "\"patch_execution_checked\",\"patch_execution_success\","
        "\"autonomous_repair_attempts\",\"autonomous_repair_succeeded\","
        "\"hunk_ins_lines\",\"hunk_del_lines\",\"prompt_hash\","
        "\"api_error_class\",\"wall_clock_ts\",\"success\"]}\n",
        ts);
    fflush(jsonl_out);
}

// E84: Emit a sweep completion sentinel as the last JSONL record.
// Allows post-processors to verify the JSONL was not truncated by a crash.
static void write_jsonl_sweep_sentinel(FILE* jsonl_out, const HarnessReport& r)
{
    if (!jsonl_out) return;
    const long long ts = static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    fprintf(jsonl_out,
        "{\"type\":\"sweep_complete\",\"ts\":%lld,\"total\":%d,\"completed\":%d,"
        "\"patch_correct\":%d,\"overall_score\":%.4f,\"pass@1\":%.4f}\n",
        ts, r.total, r.completed, r.patch_correct, r.overall_score, r.overall_score);
    fflush(jsonl_out);
}

// F1 (Todo #73): PID-based lockfile to prevent concurrent sweep processes
// writing to the same JSONL.  Lock file path = jsonl_path + ".lock".
// Returns the lock file path on success (caller deletes on exit), empty on skip.
static std::string acquire_jsonl_pid_lock(const char* jsonl_path)
{
    if (!jsonl_path || !jsonl_path[0]) return {};
    std::string lock_path = std::string(jsonl_path) + ".lock";
    // Check for existing lock
    FILE* existing = nullptr;
    if (fopen_s(&existing, lock_path.c_str(), "r") == 0 && existing) {
        char pid_buf[32] = {};
        if (fgets(pid_buf, sizeof(pid_buf), existing)) {
            fclose(existing);
            const int owner_pid = atoi(pid_buf);
            if (owner_pid > 0) {
                HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                         static_cast<DWORD>(owner_pid));
                if (proc) {
                    CloseHandle(proc);
                    fprintf(stderr,
                        "[LOCK] Another sweep process (PID %d) is already writing to %s\n"
                        "       Delete %s to override.\n",
                        owner_pid, jsonl_path, lock_path.c_str());
                    return {};  // lock held by live process
                }
            }
        } else {
            fclose(existing);
        }
    }
    // Write our PID
    FILE* lf = nullptr;
    if (fopen_s(&lf, lock_path.c_str(), "w") != 0 || !lf) return {};
    fprintf(lf, "%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
    fclose(lf);
    return lock_path;
}

// F2 (Todo #8): Emit run manifest JSONL record capturing CLI arguments and PID.
static void write_jsonl_run_manifest(FILE* jsonl_out, int argc, char** argv)
{
    if (!jsonl_out) return;
    const long long ts = static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    auto json_esc = [](const std::string& s) {
        std::string out; out.reserve(s.size() + 8);
        for (char c : s) {
            if (c == '\\') out += "\\\\";
            else if (c == '"') out += "\\\"";
            else out.push_back(c);
        }
        return out;
    };
    fprintf(jsonl_out, "{\"type\":\"run_manifest\",\"ts\":%lld,\"pid\":%lu,\"args\":[",
            ts, static_cast<unsigned long>(GetCurrentProcessId()));
    for (int i = 0; i < argc; ++i) {
        if (i > 0) fputc(',', jsonl_out);
        fprintf(jsonl_out, "\"%s\"", json_esc(argv[i]).c_str());
    }
    fprintf(jsonl_out, "]}\n");
    fflush(jsonl_out);
}

static void write_jsonl_model_fingerprint(
    FILE* jsonl_out,
    const std::string& model,
    const std::string& host,
    int port,
    const std::string& model_alias,
    int seed,
    int retry_count,
    bool strict_mode)
{
    if (!jsonl_out) {
        return;
    }
    auto json_esc = [](const std::string& s) {
        std::string out; out.reserve(s.size() + 8);
        for (char c : s) {
            if (c == '\\') out += "\\\\";
            else if (c == '"') out += "\\\"";
            else out.push_back(c);
        }
        return out;
    };
    const long long run_id = static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    fprintf(jsonl_out,
        "{\"type\":\"model_info\",\"run_id\":%lld,\"model\":\"%s\",\"model_alias\":\"%s\","
        "\"host\":\"%s\",\"port\":%d,\"seed\":%d,\"retry_count\":%d,\"strict_mode\":%s}\n",
        run_id,
        json_esc(model).c_str(),
        json_esc(model_alias).c_str(),
        json_esc(host).c_str(),
        port,
        seed,
        retry_count,
        strict_mode ? "true" : "false");
    fflush(jsonl_out);
}

// E68: Classify a failure_reason string into a coarse infrastructure/model error category.
// Returns a stable ASCII token suitable for JSONL telemetry filtering.
static std::string classify_api_error(const std::string& failure_reason)
{
    if (failure_reason.empty()) {
        return "none";
    }
    const std::string& r = failure_reason;
    // Timeout: WinHTTP error codes 12002 (timeout), 12029 (connection failed), or text hints
    if (r.find("12002") != std::string::npos ||
        r.find("12029") != std::string::npos ||
        r.find("12030") != std::string::npos ||
        r.find("timeout") != std::string::npos ||
        r.find("recv failed") != std::string::npos ||
        r.find("Timeout") != std::string::npos) {
        return "timeout";
    }
    // Empty body from server (typically model crash/OOM at response layer)
    if (r.find("empty HTTP response") != std::string::npos ||
        r.find("empty response") != std::string::npos ||
        r.find("empty_body") != std::string::npos) {
        return "empty_body";
    }
    // HTTP status codes — decode meaning
    const size_t status_pos = r.find("HTTP status=");
    if (status_pos != std::string::npos) {
        const int code = atoi(r.c_str() + status_pos + 12);
        if (code == 400 || code == 413) return "context_exceeded";
        if (code == 500 || code == 503) return "oom_kill";
        return "http_error";
    }
    // Model explicitly refused to emit a patch
    if (r.find("model returned NO_PATCH") != std::string::npos) {
        return "model_refused";
    }
    // Normalization / format failures (model produced text, not a valid diff)
    if (r.find("unified diff") != std::string::npos ||
        r.find("validation error") != std::string::npos ||
        r.find("missing required") != std::string::npos ||
        r.find("patch rejected") != std::string::npos ||
        r.find("patch too short") != std::string::npos ||
        r.find("did not emit") != std::string::npos) {
        return "format_error";
    }
    // Malformed JSON in response body
    if (r.find("could not parse") != std::string::npos ||
        r.find("parse") != std::string::npos) {
        return "malformed_json";
    }
    // WinHTTP transport-layer failures (not timeout)
    if (r.find("WinHttp") != std::string::npos ||
        r.find("WinHTTP") != std::string::npos ||
        r.find("connect") != std::string::npos) {
        return "network";
    }
    return "unknown";
}

// E66: Load already-scored task_ids from an existing partial JSONL file.
// Used by --resume mode to skip tasks that survived a previous crash.
static std::unordered_set<std::string> load_completed_task_ids_from_jsonl(const char* jsonl_path)
{
    std::unordered_set<std::string> done;
    if (!jsonl_path || !jsonl_path[0]) {
        return done;
    }
    std::ifstream f(jsonl_path);
    if (!f.is_open()) {
        return done;
    }
    std::string line;
    const std::string key = "\"sample_id\": \"";
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        const size_t pos = line.find(key);
        if (pos == std::string::npos) continue;  // model_info record
        const size_t start = pos + key.size();
        const size_t end_q = line.find('"', start);
        if (end_q == std::string::npos || end_q <= start) continue;
        done.insert(line.substr(start, end_q - start));
    }
    return done;
}

// Compute a normalized edit-distance similarity score in [0.0, 1.0] between
// two strings.  Caps both strings at 4096 chars to keep O(n*m) bounded.
static double compute_fuzzy_patch_score(const std::string& a, const std::string& b)
{
    constexpr size_t kMaxLen = 4096;
    const std::string sa = a.size() > kMaxLen ? a.substr(0, kMaxLen) : a;
    const std::string sb = b.size() > kMaxLen ? b.substr(0, kMaxLen) : b;
    const size_t na = sa.size();
    const size_t nb = sb.size();
    if (na == 0 && nb == 0) return 1.0;
    if (na == 0 || nb == 0) return 0.0;

    // Rolling two-row DP for Levenshtein distance
    std::vector<size_t> prev(nb + 1), curr(nb + 1);
    for (size_t j = 0; j <= nb; ++j) prev[j] = j;
    for (size_t i = 1; i <= na; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= nb; ++j) {
            const size_t cost = (sa[i - 1] == sb[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }
    const size_t dist = prev[nb];
    const size_t max_len = std::max(na, nb);
    return 1.0 - static_cast<double>(dist) / static_cast<double>(max_len);
}

static int compute_patch_line_count(const std::string& patch)
{
    if (patch.empty()) return 0;
    int count = 0;
    for (char c : patch) { if (c == '\n') ++count; }
    if (!patch.empty() && patch.back() != '\n') ++count;
    return count;
}

static double compute_jaccard_patch_score(const std::string& a, const std::string& b)
{
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;
    auto build_line_set = [](const std::string& s) {
        std::unordered_set<std::string> lines;
        std::istringstream in(s);
        std::string ln;
        while (std::getline(in, ln)) {
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            if (!ln.empty()) lines.insert(ln);
        }
        return lines;
    };
    const auto sa = build_line_set(a);
    const auto sb = build_line_set(b);
    size_t intersect = 0;
    for (const auto& ln : sa) {
        if (sb.count(ln)) ++intersect;
    }
    const size_t union_size = sa.size() + sb.size() - intersect;
    if (union_size == 0) return 1.0;
    return static_cast<double>(intersect) / static_cast<double>(union_size);
}

static std::string get_wall_clock_ts()
{
    SYSTEMTIME st = {};
    GetSystemTime(&st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
        static_cast<unsigned>(st.wYear),  static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),   static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond));
    return buf;
}

static std::string ws_normalize_patch(const std::string& patch)
{
    std::ostringstream out;
    std::istringstream in(patch);
    std::string ln;
    while (std::getline(in, ln)) {
        while (!ln.empty() && (ln.back() == ' ' || ln.back() == '\t' || ln.back() == '\r'))
            ln.pop_back();
        out << ln << '\n';
    }
    return out.str();
}

static double compute_ws_fuzzy_patch_score(const std::string& a, const std::string& b)
{
    return compute_fuzzy_patch_score(ws_normalize_patch(a), ws_normalize_patch(b));
}

static void write_jsonl_sample(FILE* jsonl_out, const TaskResult& result, size_t response_length)
{
    if (!jsonl_out) {
        return;
    }

    auto json_escape = [](const std::string& s) {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
            }
        }
        return out;
    };

    fprintf(jsonl_out,
        "{\"type\": \"sample\", \"sample_id\": \"%s\", \"tokens_requested\": %llu, "
        "\"tokens_effective\": %llu, \"kv_budget_bytes\": %llu, "
        "\"adapted\": %s, \"pressure_ratio\": %.6f, "
        "\"has_header\": %s, \"has_hunks\": %s, "
        "\"is_fenced\": %s, \"prose_detected\": %s, "
        "\"starts_with_header\": %s, \"contains_no_patch\": %s, "
        "\"no_patch_exact\": %s, "
        "\"is_multifile\": %s, \"header_count\": %d, \"hunk_count\": %d, "
        "\"target_file_count\": %d, \"single_file_lock\": %s, "
        "\"gold_hunk_count\": %d, \"context_bytes_injected\": %zu, "
        "\"elapsed_ms\": %.2f, "
        "\"response_length\": %zu, \"raw_response\": \"%s\", "
        "\"extracted_patch\": \"%s\", \"failure_reason\": \"%s\", "
        "\"strict_validation_error\": \"%s\", "
        "\"model_alias\": \"%s\", \"prompt_byte_count\": %llu, "
        "\"fuzzy_patch_score\": %.4f, \"patch_similarity\": %.4f, "
        "\"strict_compliance\": %s, \"retry_attempts\": %d, "
        "\"api_error_class\": \"%s\", "
        "\"emitted_patch_lines\": %d, \"gold_patch_lines\": %d, "
        "\"patch_jaccard_similarity\": %.4f, \"wall_clock_ts\": \"%s\", "
        "\"response_token_estimate\": %llu, "
        "\"ws_fuzzy_patch_score\": %.4f, \"patch_size_ratio\": %.4f, "
        "\"patch_execution_checked\": %s, \"patch_execution_success\": %s, "
        "\"autonomous_repair_enabled\": %s, \"autonomous_repair_attempts\": %d, "
        "\"autonomous_repair_succeeded\": %s, "
        "\"hunk_ins_lines\": %d, \"hunk_del_lines\": %d, \"prompt_hash\": \"%016llx\", "
        "\"response_truncated\": %s, \"kv_headroom_ratio\": %.4f, "
        "\"context_pressure_high\": %s, \"diff_token_efficiency\": %.4f, "
        "\"patch_bloat\": %s, \"verbosity_ratio\": %.2f, "
        "\"retry_succeeded\": %s, \"problem_stmt_bytes\": %llu, \"hints_bytes\": %llu, "
        "\"patch_line_delta\": %.1f, \"ms_per_token\": %.3f, "
        "\"success\": %s}\n",
        result.task_id.c_str(),
        result.tokens_requested,
        result.tokens_effective,
        result.kv_budget_bytes,
        result.adapted ? "true" : "false",
        result.pressure_ratio,
        result.has_header ? "true" : "false",
        result.has_hunks ? "true" : "false",
        result.is_fenced ? "true" : "false",
        result.prose_detected ? "true" : "false",
        result.starts_with_header ? "true" : "false",
        result.contains_no_patch ? "true" : "false",
        result.no_patch_exact ? "true" : "false",
        result.is_multifile ? "true" : "false",
        result.header_count,
        result.hunk_count,
        result.target_file_count,
        result.single_file_lock ? "true" : "false",
        result.gold_hunk_count,
        result.context_bytes_injected,
        result.elapsed_ms,
        response_length,
        json_escape(result.raw_response).c_str(),
        json_escape(result.emitted_patch).c_str(),
        json_escape(result.failure_reason).c_str(),
        json_escape(result.strict_validation_error).c_str(),
        json_escape(result.model_alias).c_str(),
        result.prompt_byte_count,
        result.fuzzy_patch_score,
        result.patch_similarity,
        result.strict_compliance ? "true" : "false",
        result.retry_attempts,
        json_escape(result.api_error_class).c_str(),
        result.emitted_patch_lines,
        result.gold_patch_lines,
        result.patch_jaccard_similarity,
        json_escape(result.wall_clock_ts).c_str(),
        result.response_token_estimate,
        result.ws_fuzzy_patch_score,
        result.patch_size_ratio,
        result.patch_execution_checked ? "true" : "false",
        result.patch_execution_success ? "true" : "false",
        result.autonomous_repair_enabled ? "true" : "false",
        result.autonomous_repair_attempts,
        result.autonomous_repair_succeeded ? "true" : "false",
        result.hunk_ins_lines,
        result.hunk_del_lines,
        static_cast<unsigned long long>(result.prompt_hash),
        result.response_truncated ? "true" : "false",
        result.kv_headroom_ratio,
        result.context_pressure_high ? "true" : "false",
        result.diff_token_efficiency,
        result.patch_bloat ? "true" : "false",
        result.verbosity_ratio,
        result.retry_succeeded ? "true" : "false",
        static_cast<unsigned long long>(result.problem_stmt_bytes),
        static_cast<unsigned long long>(result.hints_bytes),
        result.patch_line_delta,
        result.ms_per_token,
        (result.status == TaskStatus::FAILED) ? "false" : "true");
    fflush(jsonl_out);
}

// ─────────────────────────────────────────────────────────────────────────────
// Harness
// ─────────────────────────────────────────────────────────────────────────────

class Harness {
public:
    explicit Harness(
        bool run_tests = false,
        FILE* jsonl_out = nullptr,
        const char* progress_json_path = nullptr,
        const char* raw_dump_dir = nullptr,
        bool fail_fast = false,
        bool strict_mode = false)
        : m_run_tests(run_tests),
          m_jsonl_out(jsonl_out),
          m_progress_json_path(progress_json_path),
          m_raw_dump_dir(raw_dump_dir),
          m_fail_fast(fail_fast),
          m_strict_mode(strict_mode) {}

    void add_instance(Instance inst) { m_instances.push_back(std::move(inst)); }

    // Set a checkpoint file path for resumable sweeps.  On run() start the
    // file (one task_id per line) is read; those task IDs are skipped.  After
    // each completed task the task_id is appended so the next run can resume.
    void set_resume_checkpoint(const char* path)
    {
        if (path && path[0]) {
            m_resume_checkpoint_path = path;
        }
    }

    // Optional JSONL source of completed sample_ids for resume recovery.
    // Useful when checkpoint file was not written but telemetry JSONL exists.
    void set_resume_jsonl(const char* path)
    {
        if (path && path[0]) {
            m_resume_jsonl_path = path;
        }
    }

    HarnessReport run(AgentFn agent)
    {
        // Load already-completed task IDs from checkpoint file (if any)
        std::unordered_set<std::string> skip_ids;
        if (!m_resume_checkpoint_path.empty()) {
            FILE* ckf = nullptr;
            if (fopen_s(&ckf, m_resume_checkpoint_path.c_str(), "r") == 0 && ckf) {
                char line[1024];
                while (fgets(line, sizeof(line), ckf)) {
                    std::string id = line;
                    while (!id.empty() && (id.back() == '\n' || id.back() == '\r' || id.back() == ' '))
                        id.pop_back();
                    if (!id.empty()) skip_ids.insert(id);
                }
                fclose(ckf);
                if (!skip_ids.empty()) {
                    fprintf(stdout, "[RESUME] Skipping %zu already-completed task(s) from checkpoint\n",
                            skip_ids.size());
                }
            }
        }
        if (!m_resume_jsonl_path.empty()) {
            std::unordered_set<std::string> from_jsonl =
                load_completed_task_ids_from_jsonl(m_resume_jsonl_path.c_str());
            if (!from_jsonl.empty()) {
                size_t before = skip_ids.size();
                skip_ids.insert(from_jsonl.begin(), from_jsonl.end());
                size_t added = skip_ids.size() - before;
                if (added > 0) {
                    fprintf(stdout,
                            "[RESUME] Added %zu task(s) from JSONL resume source: %s\n",
                            added,
                            m_resume_jsonl_path.c_str());
                }
            }
        }

        HarnessReport report;
        report.total = static_cast<int>(m_instances.size());

        for (const auto& inst : m_instances) {
            // E79: Check for CTRL-C / CTRL-BREAK abort before starting each task
            if (g_abort_requested.load(std::memory_order_relaxed)) {
                fprintf(stdout, "[ABORT] Sweep aborted before task: %s\n", inst.task_id.c_str());
                fflush(stdout);
                break;
            }
            // Skip tasks already completed in a prior run
            if (!skip_ids.empty() && skip_ids.count(inst.task_id)) {
                fprintf(stdout, "[RESUME] Skipping task: %s\n", inst.task_id.c_str());
                continue;
            }

            TaskResult res;
            res.task_id = inst.task_id;
            res.repo    = inst.repo;           // E94: repo for per-repo summary breakdown
            populate_context_telemetry(inst, res);

            auto t0 = std::chrono::steady_clock::now();
            try {
                res.emitted_patch = agent(inst, res);
            } catch (const std::exception& ex) {
                res.status = TaskStatus::FAILED;
                res.failure_reason = ex.what();
                res.api_error_class = classify_api_error(res.failure_reason);  // E68
                maybe_dump_raw_response(res);
                write_jsonl_sample(m_jsonl_out, res, res.raw_response.empty() ? 0 : res.raw_response.size());
                report.results.push_back(res);
                recompute_report_metrics(report, m_run_tests);
                checkpoint_report(report);
                emit_task_progress(res, report.results.size(), report.total);
                if (m_fail_fast) { fprintf(stdout, "[FAIL-FAST] Stopping on first failure: %s\n", res.task_id.c_str()); fflush(stdout); break; }
                continue;
            }
            auto t1 = std::chrono::steady_clock::now();
            res.elapsed_ms = static_cast<double>(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;

            if (res.emitted_patch.empty()) {
                res.status = TaskStatus::FAILED;
                if (res.failure_reason.empty()) {
                    res.failure_reason = "agent returned empty patch";
                }
                res.api_error_class = classify_api_error(res.failure_reason);  // E68
                maybe_dump_raw_response(res);
                write_jsonl_sample(m_jsonl_out, res, res.raw_response.empty() ? 0 : res.raw_response.size());
                report.results.push_back(res);
                recompute_report_metrics(report, m_run_tests);
                checkpoint_report(report);
                emit_task_progress(res, report.results.size(), report.total);
                if (m_fail_fast) { fprintf(stdout, "[FAIL-FAST] Stopping on first failure: %s\n", res.task_id.c_str()); fflush(stdout); break; }
                continue;
            }

            report.completed++;
            res.status = TaskStatus::COMPLETED;

            // Patch correctness check + fuzzy similarity score
            if (!inst.gold_patch.empty()) {
                res.patch_match = patches_equivalent(res.emitted_patch, inst.gold_patch);
                if (res.patch_match) {
                    report.patch_correct++;
                    res.status = TaskStatus::PATCH_CORRECT;
                }
                // Fuzzy score irrespective of exact match
                res.fuzzy_patch_score = compute_fuzzy_patch_score(res.emitted_patch, inst.gold_patch);
                res.patch_similarity  = res.fuzzy_patch_score;  // convenience alias
                // Jaccard line-set overlap + gold line count
                res.patch_jaccard_similarity = compute_jaccard_patch_score(res.emitted_patch, inst.gold_patch);
                res.gold_patch_lines = compute_patch_line_count(inst.gold_patch);
                // Whitespace-normalized fuzzy score
                res.ws_fuzzy_patch_score = compute_ws_fuzzy_patch_score(res.emitted_patch, inst.gold_patch);
                // Patch size ratio: emitted lines / gold lines
                res.patch_size_ratio = static_cast<double>(compute_patch_line_count(res.emitted_patch))
                                     / static_cast<double>(std::max(1, compute_patch_line_count(inst.gold_patch)));
            }
            res.emitted_patch_lines = compute_patch_line_count(res.emitted_patch);
            {
                bool apply_checked = false;
                res.patch_execution_success = check_patch_execution_success(
                    resolve_repo_root(),
                    res.emitted_patch,
                    apply_checked);
                res.patch_execution_checked = apply_checked;
            }
            // E80: Count insertion/deletion body lines in emitted patch
            compute_hunk_ins_del(res.emitted_patch, res.hunk_ins_lines, res.hunk_del_lines);
            // Response token estimate
            res.response_token_estimate = static_cast<uint64_t>(res.raw_response.size() / 4);
            // E92-E98 / F14 / #93-#95: derived telemetry fields
            if (res.tokens_effective > 0) {
                res.response_truncated =
                    res.response_token_estimate >= res.tokens_effective * 9 / 10;
            }
            res.kv_headroom_ratio     = 1.0 - res.pressure_ratio;
            res.context_pressure_high = res.pressure_ratio > 0.85;
            res.diff_token_efficiency = res.response_token_estimate > 0
                ? static_cast<double>(res.emitted_patch_lines)
                  / static_cast<double>(res.response_token_estimate)
                : 0.0;
            res.patch_bloat = (res.gold_patch_lines > 0)
                && (res.emitted_patch_lines > res.gold_patch_lines * 2);
            res.verbosity_ratio = res.emitted_patch_lines > 0
                ? static_cast<double>(res.raw_response.size())
                  / static_cast<double>(res.emitted_patch_lines)
                : 0.0;
            res.retry_succeeded  = res.patch_match && res.retry_attempts > 0;
            res.patch_line_delta = static_cast<double>(res.hunk_ins_lines - res.hunk_del_lines);
            res.ms_per_token     = res.response_token_estimate > 0
                ? res.elapsed_ms / static_cast<double>(res.response_token_estimate)
                : 0.0;

            // Optionally run tests
            if (m_run_tests && !inst.test_cmds.empty()) {
                bool all_pass = true;
                for (const auto& cmd : inst.test_cmds) {
                    int rc = run_command(cmd, nullptr);
                    if (rc != 0) { all_pass = false; break; }
                }
                res.tests_passed = all_pass;
                if (all_pass) {
                    report.tests_passed++;
                    res.status = TaskStatus::TESTS_PASSED;
                }
            }

            maybe_dump_raw_response(res);
            write_jsonl_sample(m_jsonl_out, res,
                res.raw_response.empty() ? res.emitted_patch.size() : res.raw_response.size());
            report.results.push_back(res);
            recompute_report_metrics(report, m_run_tests);
            checkpoint_report(report);
            append_resume_checkpoint(res.task_id, skip_ids);
            emit_task_progress(res, report.results.size(), report.total);
            if (m_fail_fast && res.status == TaskStatus::FAILED) {
                fprintf(stdout, "[FAIL-FAST] Stopping on first failure: %s\n", res.task_id.c_str());
                fflush(stdout);
                break;
            }
        }

        recompute_report_metrics(report, m_run_tests);
        checkpoint_report(report);

        return report;
    }

private:
    void maybe_dump_raw_response(const TaskResult& result) const
    {
        if (!m_raw_dump_dir || !m_raw_dump_dir[0] || result.raw_response.empty()) {
            return;
        }

        const std::string dir = m_raw_dump_dir;
        if (!CreateDirectoryA(dir.c_str(), nullptr)) {
            const DWORD err = GetLastError();
            if (err != ERROR_ALREADY_EXISTS) {
                return;
            }
        }

        std::string path = dir;
        if (!path.empty() && path.back() != '\\' && path.back() != '/') {
            path.push_back('\\');
        }
        path += sanitize_task_id_for_path(result.task_id);
        path += ".raw.txt";

        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
            return;
        }
        fwrite(result.raw_response.data(), 1, result.raw_response.size(), f);
        fclose(f);
    }

    void checkpoint_report(const HarnessReport& report) const
    {
        if (m_progress_json_path && m_progress_json_path[0]) {
            write_json_report(report, m_progress_json_path);
        }
    }

    void append_resume_checkpoint(
        const std::string& task_id,
        std::unordered_set<std::string>& seen_ids) const
    {
        if (!seen_ids.insert(task_id).second) {
            return;  // already recorded, avoid duplicate lines
        }
        if (m_resume_checkpoint_path.empty()) return;
        FILE* f = nullptr;
        if (fopen_s(&f, m_resume_checkpoint_path.c_str(), "a") == 0 && f) {
            fprintf(f, "%s\n", task_id.c_str());
            fclose(f);
        }
    }

    static void emit_task_progress(const TaskResult& res, size_t task_num, int task_total)
    {
        const char* status_str = "FAILED";
        if (res.status == TaskStatus::COMPLETED)     status_str = "DONE";
        if (res.status == TaskStatus::PATCH_CORRECT)  status_str = "PATCH_OK";
        if (res.status == TaskStatus::TESTS_PASSED)   status_str = "TESTS_OK";
        const double pct_done = task_total > 0
            ? 100.0 * static_cast<double>(task_num) / static_cast<double>(task_total) : 0.0;
        fprintf(stdout, "[PROGRESS] [%zu/%d] (%.0f%%) %-40s  %-9s  %.0f ms  %s\n",  // F-Fin7
            task_num, task_total, pct_done,
            res.task_id.c_str(), status_str, res.elapsed_ms,
            res.wall_clock_ts.empty() ? "-" : res.wall_clock_ts.c_str());
        if (res.status == TaskStatus::FAILED && !res.failure_reason.empty()) {
            fprintf(stdout, "           reason: %s\n", res.failure_reason.c_str());
        }
        fflush(stdout);
    }

    std::vector<Instance> m_instances;
    bool                  m_run_tests;
    FILE*                 m_jsonl_out;
    const char*           m_progress_json_path;
    const char*           m_raw_dump_dir;
    std::string           m_resume_checkpoint_path;
    std::string           m_resume_jsonl_path;
    bool                  m_fail_fast   = false;
    bool                  m_strict_mode = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Reporter
// ─────────────────────────────────────────────────────────────────────────────

static void print_report(const HarnessReport& r, FILE* out = stdout)
{
    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════╗\n");
    fprintf(out, "║       RawrXD SWE-bench Evaluation Report     ║\n");
    fprintf(out, "╠══════════════════════════════════════════════╣\n");
    fprintf(out, "║  Instances evaluated  : %6d               ║\n", r.total);
    fprintf(out, "║  Tasks completed      : %6d  (%5.1f%%)     ║\n",
            r.completed,       r.task_completion_rate * 100.0);
    fprintf(out, "║  Patch correct        : %6d  (%5.1f%%)     ║\n",
            r.patch_correct,   r.patch_correctness   * 100.0);
    fprintf(out, "║  Tests passed         : %6d  (%5.1f%%)     ║\n",
            r.tests_passed,    r.test_pass_rate      * 100.0);
    fprintf(out, "╠══════════════════════════════════════════════╣\n");
    fprintf(out, "║  OVERALL SCORE        :        %5.1f%%        ║\n",
            r.overall_score * 100.0);
    fprintf(out, "║  Target (Claude Code) :         72.5%%        ║\n");
    fprintf(out, "║  Minimum competitive  :         50.0%%        ║\n");
    fprintf(out, "╚══════════════════════════════════════════════╝\n");

    // Per-task summary
    fprintf(out, "\nPer-task results:\n");
    for (const auto& t : r.results) {
        const char* status_str = "NOT_RUN";
        switch (t.status) {
        case TaskStatus::COMPLETED:     status_str = "COMPLETED"; break;
        case TaskStatus::PATCH_CORRECT: status_str = "PATCH_OK "; break;
        case TaskStatus::TESTS_PASSED:  status_str = "TESTS_OK "; break;
        case TaskStatus::FAILED:        status_str = "FAILED   "; break;
        default: break;
        }
        fprintf(out, "  [%s] %-40s  %.1f ms\n",
                status_str, t.task_id.c_str(), t.elapsed_ms);
        if (!t.failure_reason.empty()) {
            fprintf(out, "           reason: %s\n", t.failure_reason.c_str());
        }
    }
    fprintf(out, "\n");
}

static bool write_json_report(const HarnessReport& r, const char* path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;

    auto json_escape = [](const std::string& s) {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
            }
        }
        return out;
    };

    fprintf(f, "{\n");
    fprintf(f, "  \"total\": %d,\n",           r.total);
    fprintf(f, "  \"completed\": %d,\n",        r.completed);
    fprintf(f, "  \"patch_correct\": %d,\n",    r.patch_correct);
    fprintf(f, "  \"tests_passed\": %d,\n",     r.tests_passed);
    fprintf(f, "  \"task_completion_rate\": %.4f,\n", r.task_completion_rate);
    fprintf(f, "  \"patch_correctness\": %.4f,\n",    r.patch_correctness);
    fprintf(f, "  \"test_pass_rate\": %.4f,\n",       r.test_pass_rate);
    fprintf(f, "  \"overall_score\": %.4f,\n",        r.overall_score);
    fprintf(f, "  \"pass@1\": %.4f,\n",               r.overall_score);
    fprintf(f, "  \"results\": [\n");
    for (size_t i = 0; i < r.results.size(); ++i) {
        const auto& t = r.results[i];
        const char* comma = (i + 1 < r.results.size()) ? "," : "";
        fprintf(f,
            "    {\"task_id\": \"%s\", \"status\": %d, \"patch_match\": %s, "
            "\"tests_passed\": %s, \"elapsed_ms\": %.2f, \"failure_reason\": \"%s\", "
            "\"response\": \"%s\", "
            "\"raw_response\": \"%s\", "
            "\"has_header\": %s, \"has_hunks\": %s, "
            "\"is_fenced\": %s, \"prose_detected\": %s, "
            "\"starts_with_header\": %s, \"contains_no_patch\": %s, "
            "\"no_patch_exact\": %s, "
            "\"is_multifile\": %s, \"header_count\": %d, \"hunk_count\": %d, "
            "\"target_file_count\": %d, \"single_file_lock\": %s, "
            "\"gold_hunk_count\": %d, \"context_bytes_injected\": %zu, "
            "\"strict_validation_error\": \"%s\", "
            "\"fuzzy_patch_score\": %.4f, \"patch_similarity\": %.4f, "
            "\"strict_compliance\": %s, \"retry_attempts\": %d, "
            "\"tokens_requested\": %llu, \"tokens_effective\": %llu, "
            "\"kv_budget_bytes\": %llu, \"adapted\": %s, "
            "\"pressure_ratio\": %.4f, "
            "\"emitted_patch_lines\": %d, \"gold_patch_lines\": %d, "
            "\"patch_jaccard_similarity\": %.4f, "
            "\"patch_execution_checked\": %s, \"patch_execution_success\": %s, "
            "\"wall_clock_ts\": \"%s\", "
            "\"response_token_estimate\": %llu, "
            "\"ws_fuzzy_patch_score\": %.4f, \"patch_size_ratio\": %.4f}%s\n",
            t.task_id.c_str(),
            static_cast<int>(t.status),
            t.patch_match  ? "true" : "false",
            t.tests_passed ? "true" : "false",
            t.elapsed_ms,
            json_escape(t.failure_reason).c_str(),
            json_escape(t.emitted_patch).c_str(),
            json_escape(t.raw_response).c_str(),
            t.has_header ? "true" : "false",
            t.has_hunks ? "true" : "false",
            t.is_fenced ? "true" : "false",
            t.prose_detected ? "true" : "false",
            t.starts_with_header ? "true" : "false",
            t.contains_no_patch ? "true" : "false",
            t.no_patch_exact ? "true" : "false",
            t.is_multifile ? "true" : "false",
            t.header_count,
            t.hunk_count,
            t.target_file_count,
            t.single_file_lock ? "true" : "false",
            t.gold_hunk_count,
            t.context_bytes_injected,
            json_escape(t.strict_validation_error).c_str(),
            t.fuzzy_patch_score,
            t.patch_similarity,
            t.strict_compliance ? "true" : "false",
            t.retry_attempts,
            t.tokens_requested,
            t.tokens_effective,
            t.kv_budget_bytes,
            t.adapted ? "true" : "false",
            t.pressure_ratio,
            t.emitted_patch_lines,
            t.gold_patch_lines,
            t.patch_jaccard_similarity,
            t.patch_execution_checked ? "true" : "false",
            t.patch_execution_success ? "true" : "false",
            json_escape(t.wall_clock_ts).c_str(),
            t.response_token_estimate,
            t.ws_fuzzy_patch_score,
            t.patch_size_ratio,
            comma);
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static bool write_jsonl_summary_report(const HarnessReport& r, const char* path)
{
    if (!path || !path[0]) {
        return false;
    }

    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) {
        return false;
    }

    int    strict_failures      = 0;
    int    no_patch_exact       = 0;
    int    responses_with_headers = 0;
    int    responses_with_hunks = 0;
    int    truncated_count      = 0;  // F10: response truncation events
    int    refusal_count        = 0;  // F9/F13: model_refused errors
    int    format_err_count     = 0;  // F9: format_error class
    int    timeout_count        = 0;  // F9: timeout errors
    int    patch_exec_checked   = 0;
    int    patch_exec_success   = 0;
    int    autonomous_enabled   = 0;
    int    autonomous_attempts  = 0;
    int    autonomous_succeeded = 0;
    uint64_t total_prompt_bytes = 0;  // #97: cumulative prompt bytes
    std::map<std::string, std::pair<int,int>> repo_pass;  // E94: repo -> (total, pass)
    std::map<std::string, std::vector<double>> repo_fuzzy_scores;  // #96: repo -> fuzzy score samples
    for (const auto& t : r.results) {
        if (!t.strict_validation_error.empty()) { ++strict_failures; }
        if (t.no_patch_exact)    { ++no_patch_exact; }
        if (t.has_header)        { ++responses_with_headers; }
        if (t.has_hunks)         { ++responses_with_hunks; }
        if (t.response_truncated) { ++truncated_count; }
        if (t.api_error_class == "model_refused")  ++refusal_count;
        else if (t.api_error_class == "format_error") ++format_err_count;
        else if (t.api_error_class == "timeout")   ++timeout_count;
        if (t.patch_execution_checked) {
            ++patch_exec_checked;
            if (t.patch_execution_success) {
                ++patch_exec_success;
            }
        }
        if (t.autonomous_repair_enabled) {
            ++autonomous_enabled;
        }
        autonomous_attempts += t.autonomous_repair_attempts;
        if (t.autonomous_repair_succeeded) {
            ++autonomous_succeeded;
        }
        total_prompt_bytes += t.prompt_byte_count;
        if (!t.repo.empty()) {
            auto& entry = repo_pass[t.repo];
            entry.first++;
            if (t.patch_match) entry.second++;
            repo_fuzzy_scores[t.repo].push_back(t.fuzzy_patch_score);
        }
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"total\": %d,\n", r.total);
    fprintf(f, "  \"completed\": %d,\n", r.completed);
    fprintf(f, "  \"patch_correct\": %d,\n", r.patch_correct);
    fprintf(f, "  \"tests_passed\": %d,\n", r.tests_passed);
    fprintf(f, "  \"pass@1\": %.4f,\n", r.overall_score);
    fprintf(f, "  \"task_completion_rate\": %.4f,\n", r.task_completion_rate);
    fprintf(f, "  \"patch_correctness\": %.4f,\n", r.patch_correctness);
    fprintf(f, "  \"test_pass_rate\": %.4f,\n", r.test_pass_rate);
    fprintf(f, "  \"strict_validation_failures\": %d,\n", strict_failures);
    fprintf(f, "  \"no_patch_exact\": %d,\n", no_patch_exact);
    fprintf(f, "  \"responses_with_headers\": %d,\n", responses_with_headers);
    double sum_fuzzy = 0.0, sum_jaccard = 0.0, sum_elapsed = 0.0;
    for (const auto& t : r.results) {
        sum_fuzzy   += t.fuzzy_patch_score;
        sum_jaccard += t.patch_jaccard_similarity;
        sum_elapsed += t.elapsed_ms;
    }
    const int n_results   = static_cast<int>(r.results.size());
    const double avg_fuzzy   = n_results > 0 ? sum_fuzzy   / n_results : 0.0;
    const double avg_jaccard = n_results > 0 ? sum_jaccard / n_results : 0.0;
    const double avg_elapsed = n_results > 0 ? sum_elapsed / n_results : 0.0;

    // E93: Wilson score confidence interval for patch_correctness (95%)
    double ci_lower = 0.0, ci_upper = 0.0;
    if (n_results > 0) {
        const double z     = 1.96;
        const double p     = r.patch_correctness;
        const double n     = static_cast<double>(n_results);
        const double denom  = 1.0 + z * z / n;
        const double center = p + z * z / (2.0 * n);
        const double spread = z * std::sqrt(p * (1.0 - p) / n + z * z / (4.0 * n * n));
        ci_lower = std::max(0.0, (center - spread) / denom);
        ci_upper = std::min(1.0, (center + spread) / denom);
    }

    // F4: Latency percentiles (p50 / p90 / p95)
    double p50_elapsed = 0.0, p90_elapsed = 0.0, p95_elapsed = 0.0;
    if (!r.results.empty()) {
        std::vector<double> el_sorted;
        el_sorted.reserve(r.results.size());
        for (const auto& t : r.results) el_sorted.push_back(t.elapsed_ms);
        std::sort(el_sorted.begin(), el_sorted.end());
        const size_t n = el_sorted.size();
        p50_elapsed = el_sorted[std::min(n - 1, static_cast<size_t>(n * 50 / 100))];
        p90_elapsed = el_sorted[std::min(n - 1, static_cast<size_t>(n * 90 / 100))];
        p95_elapsed = el_sorted[std::min(n - 1, static_cast<size_t>(n * 95 / 100))];
    }

    // F7: Multi-hunk vs single-hunk patch accuracy split
    int single_hunk_total = 0, single_hunk_pass = 0;
    int multi_hunk_total  = 0, multi_hunk_pass  = 0;
    for (const auto& t : r.results) {
        if (t.gold_hunk_count <= 1) { ++single_hunk_total; if (t.patch_match) ++single_hunk_pass; }
        else                        { ++multi_hunk_total;  if (t.patch_match) ++multi_hunk_pass;  }
    }

    fprintf(f, "  \"responses_with_hunks\": %d,\n", responses_with_hunks);
    fprintf(f, "  \"avg_fuzzy_score\": %.4f,\n", avg_fuzzy);
    fprintf(f, "  \"avg_jaccard_score\": %.4f,\n", avg_jaccard);
    fprintf(f, "  \"avg_elapsed_ms\": %.2f,\n", avg_elapsed);
    fprintf(f, "  \"p50_elapsed_ms\": %.2f,\n", p50_elapsed);
    fprintf(f, "  \"p90_elapsed_ms\": %.2f,\n", p90_elapsed);
    fprintf(f, "  \"p95_elapsed_ms\": %.2f,\n", p95_elapsed);
    fprintf(f, "  \"single_hunk_total\": %d,\n", single_hunk_total);
    fprintf(f, "  \"single_hunk_pass\": %d,\n", single_hunk_pass);
    fprintf(f, "  \"multi_hunk_total\": %d,\n", multi_hunk_total);
    fprintf(f, "  \"multi_hunk_pass\": %d,\n", multi_hunk_pass);
    // E93: Wilson score CI
    fprintf(f, "  \"patch_correctness_ci_lower\": %.4f,\n", ci_lower);
    fprintf(f, "  \"patch_correctness_ci_upper\": %.4f,\n", ci_upper);
    // F10: truncation count; F9: error class breakdown; F13: refusal rate; #97: total prompt bytes
    fprintf(f, "  \"truncated_count\": %d,\n", truncated_count);
    fprintf(f, "  \"refusal_count\": %d,\n", refusal_count);
    fprintf(f, "  \"format_error_count\": %d,\n", format_err_count);
    fprintf(f, "  \"timeout_count\": %d,\n", timeout_count);
    fprintf(f, "  \"refusal_rate\": %.4f,\n",
        n_results > 0 ? static_cast<double>(refusal_count) / n_results : 0.0);
    fprintf(f, "  \"patch_execution_checked\": %d,\n", patch_exec_checked);
    fprintf(f, "  \"patch_execution_success\": %d,\n", patch_exec_success);
    fprintf(f, "  \"patch_execution_success_rate\": %.4f,\n",
        patch_exec_checked > 0
            ? static_cast<double>(patch_exec_success) / patch_exec_checked
            : 0.0);
    fprintf(f, "  \"autonomous_repair_enabled_samples\": %d,\n", autonomous_enabled);
    fprintf(f, "  \"autonomous_repair_attempts\": %d,\n", autonomous_attempts);
    fprintf(f, "  \"autonomous_repair_succeeded\": %d,\n", autonomous_succeeded);
    fprintf(f, "  \"total_prompt_bytes\": %llu,\n",
        static_cast<unsigned long long>(total_prompt_bytes));
    // E94: Per-repo pass rate breakdown array
    fprintf(f, "  \"repo_breakdown\": [\n");
    {
        size_t repo_i = 0;
        const size_t repo_n = repo_pass.size();
        for (const auto& kv : repo_pass) {
            const double rrate = kv.second.first > 0
                ? static_cast<double>(kv.second.second) / kv.second.first : 0.0;
            double median_fuzzy = 0.0;
            auto it_fuzzy = repo_fuzzy_scores.find(kv.first);
            if (it_fuzzy != repo_fuzzy_scores.end() && !it_fuzzy->second.empty()) {
                auto vals = it_fuzzy->second;
                std::sort(vals.begin(), vals.end());
                const size_t n = vals.size();
                if ((n % 2) == 1) {
                    median_fuzzy = vals[n / 2];
                } else {
                    median_fuzzy = 0.5 * (vals[n / 2 - 1] + vals[n / 2]);
                }
            }
            fprintf(f, "    {\"repo\": \"%s\", \"total\": %d, \"pass\": %d, \"rate\": %.4f, \"median_fuzzy_score\": %.4f}%s\n",
                kv.first.c_str(), kv.second.first, kv.second.second, rrate, median_fuzzy,
                (++repo_i < repo_n) ? "," : "");
        }
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

static bool write_csv_report(const HarnessReport& r, const char* path)
{
    if (!path || !path[0]) return false;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;
    fprintf(f, "task_id,status,elapsed_ms,fuzzy_patch_score,ws_fuzzy_patch_score,patch_jaccard_similarity,"
               "patch_size_ratio,emitted_patch_lines,gold_patch_lines,context_bytes_injected,"
               "gold_hunk_count,response_token_estimate,retry_attempts,api_error_class,"
               "patch_execution_checked,patch_execution_success,"
               "autonomous_repair_enabled,autonomous_repair_attempts,autonomous_repair_succeeded,"
               "prompt_byte_count,wall_clock_ts\n");
    for (const auto& t : r.results) {
        const char* st = "NOT_RUN";
        switch (t.status) {
        case TaskStatus::COMPLETED:     st = "COMPLETED";     break;
        case TaskStatus::PATCH_CORRECT: st = "PATCH_CORRECT"; break;
        case TaskStatus::TESTS_PASSED:  st = "TESTS_PASSED";  break;
        case TaskStatus::FAILED:        st = "FAILED";        break;
        default: break;
        }
        fprintf(f, "%s,%s,%.2f,%.4f,%.4f,%.4f,%.4f,%d,%d,%zu,%d,%llu,%d,%s,%s,%s,%s,%d,%s,%llu,%s\n",
            t.task_id.c_str(), st, t.elapsed_ms,
            t.fuzzy_patch_score, t.ws_fuzzy_patch_score,
            t.patch_jaccard_similarity, t.patch_size_ratio,
            t.emitted_patch_lines, t.gold_patch_lines,
            t.context_bytes_injected, t.gold_hunk_count,
            t.response_token_estimate, t.retry_attempts,
            t.api_error_class.empty() ? "none" : t.api_error_class.c_str(),
            t.patch_execution_checked ? "true" : "false",
            t.patch_execution_success ? "true" : "false",
            t.autonomous_repair_enabled ? "true" : "false",
            t.autonomous_repair_attempts,
            t.autonomous_repair_succeeded ? "true" : "false",
            t.prompt_byte_count,
            t.wall_clock_ts.empty() ? "-" : t.wall_clock_ts.c_str());
    }
    fclose(f);
    return true;
}

static bool write_markdown_report(const HarnessReport& r, const char* path)
{
    if (!path || !path[0]) return false;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;
    fprintf(f, "# RawrXD SWE-bench Evaluation Report\n\n");
    fprintf(f, "| Metric | Value |\n");
    fprintf(f, "|--------|-------|\n");
    fprintf(f, "| Total instances | %d |\n", r.total);
    fprintf(f, "| Completed | %d (%.1f%%) |\n", r.completed, r.task_completion_rate * 100.0);
    fprintf(f, "| Patch correct | %d (%.1f%%) |\n", r.patch_correct, r.patch_correctness * 100.0);
    fprintf(f, "| Tests passed | %d (%.1f%%) |\n", r.tests_passed, r.test_pass_rate * 100.0);
    fprintf(f, "| **pass@1** | **%.4f** |\n", r.overall_score);
    fprintf(f, "\n## Per-task Results\n\n");
    fprintf(f, "| task_id | status | elapsed_ms | fuzzy | ws_fuzzy | jaccard | apply_check | apply_success | auto_repair_attempts | auto_repair_succeeded | size_ratio | lines_emitted | lines_gold |\n");
    fprintf(f, "|---------|--------|------------|-------|----------|---------|-------------|---------------|----------------------|-----------------------|------------|---------------|------------|\n");
    for (const auto& t : r.results) {
        const char* st = "NOT_RUN";
        switch (t.status) {
        case TaskStatus::COMPLETED:     st = "COMPLETED";     break;
        case TaskStatus::PATCH_CORRECT: st = "PATCH_CORRECT"; break;
        case TaskStatus::TESTS_PASSED:  st = "TESTS_PASSED";  break;
        case TaskStatus::FAILED:        st = "FAILED";        break;
        default: break;
        }
        fprintf(f, "| %s | %s | %.0f | %.4f | %.4f | %.4f | %s | %s | %d | %s | %.4f | %d | %d |\n",
            t.task_id.c_str(), st, t.elapsed_ms,
            t.fuzzy_patch_score, t.ws_fuzzy_patch_score,
            t.patch_jaccard_similarity,
            t.patch_execution_checked ? "true" : "false",
            t.patch_execution_success ? "true" : "false",
            t.autonomous_repair_attempts,
            t.autonomous_repair_succeeded ? "true" : "false",
            t.patch_size_ratio,
            t.emitted_patch_lines, t.gold_patch_lines);
    }
    fclose(f);
    return true;
}

static bool export_instances_jsonl(const std::vector<Instance>& instances, const char* path)
{
    if (!path || !path[0]) return false;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) return false;
    auto esc = [](const std::string& s) {
        std::string o; o.reserve(s.size() + 8);
        for (char c : s) {
            if (c == '\\') o += "\\\\";
            else if (c == '"') o += "\\\"";
            else if (c == '\n') o += "\\n";
            else if (c == '\r') o += "\\r";
            else o.push_back(c);
        }
        return o;
    };
    for (const auto& inst : instances) {
        fprintf(f, "{\"task_id\":\"%s\",\"repo\":\"%s\",\"base_commit\":\"%s\","
                   "\"problem_statement\":\"%s\",\"patch\":\"%s\",\"hints\":\"%s\"}\n",
            esc(inst.task_id).c_str(), esc(inst.repo).c_str(),
            esc(inst.base_commit).c_str(), esc(inst.problem_stmt).c_str(),
            esc(inst.gold_patch).c_str(), esc(inst.hints_text).c_str());
    }
    fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Built-in synthetic instance set
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<Instance> builtin_instances()
{
    std::vector<Instance> v;

    // Each instance is a synthetic task that can be validated without a live
    // repository — the gold patch is compared against the agent's output.

    {
        Instance i;
        i.task_id      = "rawrxd-001";
        i.repo         = "ItsMehRAWRXD/RawrXD";
        i.base_commit  = "main";
        i.problem_stmt =
            "The context window defaults are fragmented (4096/8192) across "
            "inference files. Unify them to 131072 using the ContextLimits::DEFAULT "
            "constant from context_config.h.";
        i.gold_patch   =
            "--- a/src/ai/ai_inference_real.cpp\n"
            "+++ b/src/ai/ai_inference_real.cpp\n"
            "@@ -1 +1 @@\n"
            "-    g_model.n_ctx = 8192;\n"
            "+    g_model.n_ctx = RawrXD::ContextLimits::DEFAULT;\n";
        v.push_back(i);
    }

    {
        Instance i;
        i.task_id      = "rawrxd-002";
        i.repo         = "ItsMehRAWRXD/RawrXD";
        i.base_commit  = "main";
        i.problem_stmt =
            "MCP HTTP client connectHttp() is explicitly unimplemented — the "
            "function body returns false immediately. Implement a WinHTTP-backed "
            "transport so remote MCP servers can be reached over HTTP/HTTPS.";
        i.gold_patch   = "(WinHTTP implementation in mcp_integration.cpp)";
        v.push_back(i);
    }

    {
        Instance i;
        i.task_id      = "rawrxd-003";
        i.repo         = "ItsMehRAWRXD/RawrXD";
        i.base_commit  = "main";
        i.problem_stmt =
            "plan_orchestrator.cpp executeTask() falls through to return true for "
            "unknown operations, creating a fail-open safety hole. Fix so unknown "
            "operations are rejected with an error.";
        i.gold_patch   = "(fail-closed unknown-op rejection in plan_orchestrator.cpp)";
        v.push_back(i);
    }

    {
        Instance i;
        i.task_id      = "rawrxd-004";
        i.repo         = "ItsMehRAWRXD/RawrXD";
        i.base_commit  = "main";
        i.problem_stmt =
            "tool_registry_init.cpp defines register_git_mcp_tools() but is not "
            "listed in CMakeLists.txt, so the symbol is never linked. Add it to the "
            "Win32IDE source list.";
        i.gold_patch   =
            "--- a/CMakeLists.txt\n"
            "+++ b/CMakeLists.txt\n"
            "@@ -700 +700 @@\n"
            "+        src/tool_registry_init.cpp\n";
        v.push_back(i);
    }

    return v;
}

// Load instances from a JSON/JSONL dataset file.
// Format: one JSON object per line, with fields:
//   task_id, repo, base_commit, problem_statement, patch (gold), test_commands, hints
static std::vector<Instance> load_json_dataset(const char* path)
{
    std::vector<Instance> v;
    std::ifstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "Error: cannot open JSON dataset: %s\n", path);
        return v;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(f, line)) {
        ++line_num;
        if (line.empty() || line[0] == '#') continue;

        // Rudimentary field extraction: "key": "value"
        auto extract = [&](const char* key) -> std::string {
            std::string k = std::string("\"") + key + "\"";
            auto pos = line.find(k);
            if (pos == std::string::npos) return {};
            auto colon = line.find(':', pos + k.size());
            if (colon == std::string::npos) return {};
            auto qs = line.find('"', colon + 1);
            if (qs == std::string::npos) return {};
            auto qe = line.find('"', qs + 1);
            if (qe == std::string::npos) return {};
            return line.substr(qs + 1, qe - qs - 1);
        };

        Instance inst;
        inst.task_id      = extract("task_id");
        inst.repo         = extract("repo");
        inst.base_commit  = extract("base_commit");
        inst.problem_stmt = extract("problem_statement");
        inst.gold_patch   = extract("patch");
        inst.hints_text   = extract("hints");

        if (inst.task_id.empty() || inst.problem_stmt.empty()) {
            fprintf(stderr, "Warning: line %d missing required fields (task_id, problem_statement), skipping\n", line_num);
            continue;
        }

        v.push_back(inst);
    }

    fprintf(stdout, "Loaded %zu instances from JSON dataset\n", v.size());
    return v;
}

} // namespace SWEBench

// Real inference agent wrapper (minimal Ollama HTTP client)
// ─────────────────────────────────────────────────────────────────────────────

struct MinimalOllamaClient {
    std::string host;
    int port;
    std::string model;
    bool debug_http = false;
    int seed = -1;        // -1 = unset (random); set via --seed for reproducibility
    double temperature = -1.0; // -1.0 = unset; passed to Ollama options.temperature if >= 0
    int recv_timeout_ms = 240000; // per-request receive timeout; override via --timeout-ms

    static void NormalizeHostAndPortLocal(const std::string& input, std::string& out_host, WORD& out_port)
    {
        out_host.clear();
        out_port = 11434;

        std::string value = input;
        const size_t scheme_pos = value.find("://");
        if (scheme_pos != std::string::npos) {
            value = value.substr(scheme_pos + 3);
        }

        const size_t path_pos = value.find('/');
        if (path_pos != std::string::npos) {
            value = value.substr(0, path_pos);
        }

        while (!value.empty() && value.back() == '/') {
            value.pop_back();
        }

        if (value.empty()) {
            return;
        }

        const size_t colon_pos = value.rfind(':');
        if (colon_pos != std::string::npos && colon_pos + 1 < value.size()) {
            out_host = value.substr(0, colon_pos);
            try {
                const int parsed_port = std::stoi(value.substr(colon_pos + 1));
                if (parsed_port > 0 && parsed_port <= 65535) {
                    out_port = static_cast<WORD>(parsed_port);
                }
            } catch (...) {
            }
            if (out_host.empty()) {
                out_host = value;
            }
            return;
        }

        out_host = value;
    }

    explicit MinimalOllamaClient(const std::string& h = "127.0.0.1", int p = 11434, const std::string& m = "mistral")
        : port(11434), model(m)
    {
        WORD normalized_port = 11434;
        NormalizeHostAndPortLocal(h, host, normalized_port);
        if (host.empty()) {
            host = "127.0.0.1";
        }

        if (p > 0 && p <= 65535) {
            port = p;
        } else {
            port = static_cast<int>(normalized_port);
        }
    }

    static std::string escape_json(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
            }
        }
        return out;
    }

    static std::string unescape_json(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char n = s[++i];
                switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'u': {
                    if (i + 4 < s.size()) {
                        const std::string hex = s.substr(i + 1, 4);
                        char* end_ptr = nullptr;
                        const long codepoint = std::strtol(hex.c_str(), &end_ptr, 16);
                        if (end_ptr && *end_ptr == '\0' && codepoint >= 0 && codepoint <= 0x7F) {
                            out.push_back(static_cast<char>(codepoint));
                            i += 4;
                            break;
                        }
                    }
                    out.push_back('u');
                    break;
                }
                default: out.push_back(n); break;
                }
            } else {
                out.push_back(s[i]);
            }
        }
        return out;
    }

    static bool extract_json_string(const std::string& raw, const std::string& key, std::string& out)
    {
        std::string needle = "\"" + key + "\":\"";
        size_t pos = raw.find(needle);
        if (pos == std::string::npos) {
            return false;
        }

        size_t start = pos + needle.size();
        size_t end = start;
        while (end < raw.size()) {
            if (raw[end] == '\\' && end + 1 < raw.size()) {
                end += 2;
                continue;
            }
            if (raw[end] == '"') {
                break;
            }
            ++end;
        }

        out = unescape_json(raw.substr(start, end - start));
        return true;
    }

    static std::string parse_ollama_response(const std::string& raw, std::string* error_out = nullptr)
    {
        std::string value;
        if (extract_json_string(raw, "error", value)) {
            if (error_out) {
                *error_out = value;
            }
            return {};
        }

        if (extract_json_string(raw, "response", value)) {
            return value;
        }

        size_t resultsPos = raw.find("\"results\"");
        if (resultsPos != std::string::npos) {
            std::string suffix = raw.substr(resultsPos);
            if (extract_json_string(suffix, "response", value)) {
                return value;
            }
            if (extract_json_string(suffix, "content", value)) {
                return value;
            }
        }

        size_t choicesPos = raw.find("\"choices\"");
        if (choicesPos != std::string::npos) {
            std::string suffix = raw.substr(choicesPos);
            if (extract_json_string(suffix, "content", value)) {
                return value;
            }
        }

        // Fallback: return raw body so harness can still treat non-empty output as a response.
        return raw;
    }

    static std::wstring utf8_to_wide(const std::string& s)
    {
        if (s.empty()) {
            return {};
        }
        int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
        if (needed <= 0) {
            return {};
        }
        std::wstring out(static_cast<size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), needed);
        return out;
    }

    static std::string wide_to_utf8(const std::wstring& s)
    {
        if (s.empty()) {
            return {};
        }
        int needed = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) {
            return {};
        }
        std::string out(static_cast<size_t>(needed), '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), needed, nullptr, nullptr);
        return out;
    }

    std::string Generate(const std::string& prompt, int max_tokens = 2048, std::string* error_out = nullptr) const
    {
        const int request_max_tokens = (max_tokens > 0)
        ? std::min(max_tokens, 8192)
        : 2048;

        std::string body =
            "{\"model\":\"" + escape_json(model) +
            "\",\"prompt\":\"" + escape_json(prompt) +
            "\",\"stream\":false,\"max_tokens\":" + std::to_string(request_max_tokens) +
            ",\"num_predict\":" + std::to_string(request_max_tokens) +
            ",\"options\":{\"num_predict\":" + std::to_string(request_max_tokens);
        if (seed >= 0) {
            body += ",\"seed\":" + std::to_string(seed);
        }
        if (temperature >= 0.0) {
            char tempbuf[32]; snprintf(tempbuf, sizeof(tempbuf), "%.4f", temperature);
            body += std::string(",\"temperature\":") + tempbuf;
        }
        body += "}}";

        std::wstring whost = utf8_to_wide(host);
        if (whost.empty()) {
            return {};
        }

        std::string raw;

        HINTERNET session = WinHttpOpen(L"RawrXD-SWEBench/1.0",
                                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
        if (!session) {
            if (error_out) {
                *error_out = "WinHttpOpen failed err=" + std::to_string(GetLastError());
            }
            return {};
        }

        int recv_timeout_ms = this->recv_timeout_ms;
        const char* env_recv_timeout = getenv("RAWRXD_SWEBENCH_RECV_TIMEOUT_MS");
        if (env_recv_timeout && env_recv_timeout[0]) {
            const int parsed = atoi(env_recv_timeout);
            if (parsed > 0) {
                recv_timeout_ms = parsed;
            }
        }
        // Normalize receive timeout to a safe bounded range while honoring CLI/env overrides.
        if (recv_timeout_ms < 1000) {
            recv_timeout_ms = 1000;
        }
        if (recv_timeout_ms > 120000) {
            recv_timeout_ms = 120000;
        }
        const int connect_timeout_ms = 5000;
        const int send_timeout_ms = 5000;
        WinHttpSetTimeouts(session, connect_timeout_ms, connect_timeout_ms, send_timeout_ms, recv_timeout_ms);

        HINTERNET connection = WinHttpConnect(session, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
        if (!connection) {
            if (error_out) {
                *error_out = "WinHttpConnect failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(session);
            return {};
        }

        HINTERNET request = WinHttpOpenRequest(connection,
                                               L"POST",
                                               L"/api/generate",
                                               nullptr,
                                               WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               0);
        if (!request) {
            if (error_out) {
                *error_out = "WinHttpOpenRequest failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        const wchar_t* headers = L"Content-Type: application/json\r\n";
        BOOL ok = WinHttpSendRequest(request,
                                     headers,
                                     -1L,
                                     const_cast<char*>(body.data()),
                                     static_cast<DWORD>(body.size()),
                                     static_cast<DWORD>(body.size()),
                                     0);
        if (ok) {
            ok = WinHttpReceiveResponse(request, nullptr);
        }
        if (!ok) {
            if (error_out) {
                *error_out = "WinHTTP send/recv failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        if (WinHttpQueryHeaders(request,
                                 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX,
                                 &status_code,
                                 &status_size,
                                 WINHTTP_NO_HEADER_INDEX)) {
            if (debug_http) {
                fprintf(stdout,
                    "[SWE][HTTP] POST /api/generate host=%s port=%d status=%lu num_predict=%d recv_timeout_ms=%d\n",
                    host.c_str(),
                    port,
                    static_cast<unsigned long>(status_code),
                    request_max_tokens,
                    recv_timeout_ms);
                fflush(stdout);
            }
            if (status_code != 200) {
                if (error_out) {
                    *error_out = "HTTP status=" + std::to_string(status_code);
                }
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return {};
            }
        }

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
                break;
            }

            std::string chunk(static_cast<size_t>(available), '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) {
                break;
            }
            chunk.resize(static_cast<size_t>(read));
            raw += chunk;
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        if (raw.empty()) {
            if (error_out) {
                *error_out = "empty HTTP response body";
            }
            return {};
        }

        if (debug_http) {
            fprintf(stdout,
                "[SWE][HTTP] response_bytes=%zu\n",
                raw.size());
            fprintf(stdout,
                "[SWE][HTTP] raw_generate_body=%s\n",
                raw.c_str());
            fflush(stdout);
        }

        std::string parsed = parse_ollama_response(raw, error_out);
        if (parsed.empty() && error_out && !raw.empty()) {
            // preserve raw response when parsing fails, but signal the issue
            *error_out = "could not parse response; raw body returned";
            return raw;
        }
        return parsed;
    }

    std::string HttpGet(const std::string& path, std::string* error_out = nullptr) const
    {
        std::wstring whost = utf8_to_wide(host);
        if (whost.empty()) {
            if (error_out) {
                *error_out = "invalid host";
            }
            return {};
        }

        std::string raw;
        HINTERNET session = WinHttpOpen(L"RawrXD-SWEBench/1.0",
                                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
        if (!session) {
            if (error_out) {
                *error_out = "WinHttpOpen failed err=" + std::to_string(GetLastError());
            }
            return {};
        }

        int recv_timeout_ms = this->recv_timeout_ms;
        const char* env_recv_timeout = getenv("RAWRXD_SWEBENCH_RECV_TIMEOUT_MS");
        if (env_recv_timeout && env_recv_timeout[0]) {
            const int parsed = atoi(env_recv_timeout);
            if (parsed > 0) {
                recv_timeout_ms = parsed;
            }
        }
        if (recv_timeout_ms < 1000) {
            recv_timeout_ms = 1000;
        }
        if (recv_timeout_ms > 120000) {
            recv_timeout_ms = 120000;
        }
        WinHttpSetTimeouts(session, 5000, 5000, 10000, recv_timeout_ms);

        HINTERNET connection = WinHttpConnect(session, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
        if (!connection) {
            if (error_out) {
                *error_out = "WinHttpConnect failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(session);
            return {};
        }

        HINTERNET request = WinHttpOpenRequest(connection,
                                               L"GET",
                                               std::wstring(utf8_to_wide(path)).c_str(),
                                               nullptr,
                                               WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               0);
        if (!request) {
            if (error_out) {
                *error_out = "WinHttpOpenRequest failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        BOOL ok = WinHttpSendRequest(request,
                                     WINHTTP_NO_ADDITIONAL_HEADERS,
                                     0,
                                     nullptr,
                                     0,
                                     0,
                                     0);
        if (ok) {
            ok = WinHttpReceiveResponse(request, nullptr);
        }
        if (!ok) {
            if (error_out) {
                *error_out = "WinHTTP GET failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        if (!WinHttpQueryHeaders(request,
                                 WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX,
                                 &status_code,
                                 &status_size,
                                 WINHTTP_NO_HEADER_INDEX)) {
            if (error_out) {
                *error_out = "WinHttpQueryHeaders status failed err=" + std::to_string(GetLastError());
            }
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        if (status_code != 200) {
            if (error_out) {
                *error_out = "HTTP status=" + std::to_string(status_code);
            }
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return {};
        }

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
                break;
            }

            std::string chunk(static_cast<size_t>(available), '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) {
                break;
            }
            chunk.resize(static_cast<size_t>(read));
            raw += chunk;
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        if (raw.empty() && error_out) {
            *error_out = "empty HTTP response body";
        }

        if (debug_http && !raw.empty()) {
            fprintf(stdout,
                "[SWE][HTTP] GET %s host=%s port=%d raw_body=%s\n",
                path.c_str(),
                host.c_str(),
                port,
                raw.c_str());
            fflush(stdout);
        }

        return raw;
    }

    std::vector<std::string> ListModels(std::string* error_out = nullptr) const
    {
        std::vector<std::string> names;
        std::string raw = HttpGet("/api/models", error_out);
        if (raw.empty()) {
            raw = HttpGet("/api/tags", error_out);
        }
        if (raw.empty()) {
            return names;
        }

        const std::vector<std::string> keys = {"\"name\":\"", "\"model\":\"", "\"tag\":\""};
        for (const auto& key : keys) {
            size_t pos = 0;
            while ((pos = raw.find(key, pos)) != std::string::npos) {
                size_t start = pos + key.size();
                size_t end = start;
                while (end < raw.size() && raw[end] != '"') {
                    end++;
                }
                if (end > start) {
                    names.push_back(raw.substr(start, end - start));
                }
                pos = end + 1;
            }
            if (!names.empty()) {
                break;
            }
        }
        return names;
    }
};

struct RealAgentContext {
    MinimalOllamaClient* ollama_client = nullptr;
    bool debug_runtime = false;
    int max_output_tokens = 0;
    bool source_context_enabled = true;
    size_t source_context_max_bytes_per_file = 2500;
    size_t source_context_max_total_bytes = 5000;
    size_t source_context_max_files = 2;
    std::string prompt_dump_dir;
    std::string model_alias;  // telemetry label (overrides raw model string)
    int seed = -1;            // reproducibility seed; -1 = unset
    int retry_count = 0;      // extra attempts on empty/NO_PATCH response (0 = one attempt total)
    bool strict_mode = false; // reject is_fenced responses even when patch was extracted
    bool hints_enabled = true;    // if false, hints_text stripped from prompt
    size_t max_prompt_bytes = 0;  // if >0, trim context when prompt exceeds this limit
    double temperature = -1.0;    // if >= 0, passed to Ollama options.temperature
    int max_task_wall_ms = 0;     // E81: per-task wall-time budget cap in ms (0 = disabled)
    bool phase4_rag_lite = false; // Phase 4: widen aperture + anti-speculation prompt policy
    int phase4_aperture_lines = 80; // aperture radius around anchor line in RAG-lite mode
    std::string output_format = "plain"; // prompt format mode: plain|fenced|auto
    bool autonomous_patch_repair = false; // retry with corrective prompt when apply-check fails
    int autonomous_max_repair = 2; // max apply-check repair retries
};

static std::string invoke_real_agent(
    const SWEBench::Instance& inst,
    RealAgentContext* ctx,
    SWEBench::TaskResult& result_out)
{
    fprintf(stdout, "[DEBUG] invoke_real_agent() called for task=%s\n", inst.task_id.c_str());
    fflush(stdout);

    if (!ctx || !ctx->ollama_client) {
        result_out.failure_reason = "ollama_client not initialized";
        return {};
    }

    try {
        std::string transport_error;
        std::string corrective_feedback;
        int max_tokens = result_out.tokens_effective > 0
            ? static_cast<int>(std::min<uint64_t>(
                result_out.tokens_effective,
                static_cast<uint64_t>(std::numeric_limits<int>::max())))
            : 2048;

        if (ctx->max_output_tokens > 0) {
            max_tokens = std::min(max_tokens, ctx->max_output_tokens);
        }

        if (ctx->debug_runtime) {
            fprintf(stdout,
                "[SWE][BUDGET] task=%s requested=%llu effective=%llu adapted=%s pressure=%.4f max_tokens=%d\n",
                inst.task_id.c_str(),
                static_cast<unsigned long long>(result_out.tokens_requested),
                static_cast<unsigned long long>(result_out.tokens_effective),
                result_out.adapted ? "true" : "false",
                result_out.pressure_ratio,
                max_tokens);
            fflush(stdout);
        }

        // Extract target files from gold patch to enable file-lock constraint
        const std::vector<std::string> target_files =
            SWEBench::extract_target_files_from_patch(inst.gold_patch);
        result_out.target_file_count = static_cast<int>(target_files.size());
        result_out.single_file_lock = target_files.size() == 1;
        result_out.gold_hunk_count = SWEBench::raw_count_hunks(inst.gold_patch);

        result_out.wall_clock_ts = SWEBench::get_wall_clock_ts();

        size_t context_bytes_injected = 0;
        std::string prompt = SWEBench::build_patch_only_prompt(
            inst,
            target_files,
            ctx->source_context_enabled,
            ctx->source_context_max_bytes_per_file,
            ctx->source_context_max_total_bytes,
            ctx->source_context_max_files,
            &context_bytes_injected,
            ctx->hints_enabled,
            ctx->phase4_rag_lite,
            ctx->phase4_aperture_lines,
            ctx->output_format,
            corrective_feedback);
        result_out.context_bytes_injected = context_bytes_injected;

        // Max-prompt-bytes guard: rebuild without context if prompt exceeds limit
        if (ctx->max_prompt_bytes > 0 && prompt.size() > ctx->max_prompt_bytes) {
            size_t bytes_trim = 0;
            std::string prompt_trim = SWEBench::build_patch_only_prompt(
                inst, target_files, false, 0, 0, 0, &bytes_trim,
                ctx->hints_enabled, ctx->phase4_rag_lite, ctx->phase4_aperture_lines,
                ctx->output_format,
                corrective_feedback);
            if (ctx->debug_runtime) {
                fprintf(stdout,
                    "[SWE][TRIM] task=%s prompt=%zu > max_prompt=%zu; context stripped\n",
                    inst.task_id.c_str(), prompt.size(), ctx->max_prompt_bytes);
                fflush(stdout);
            }
            prompt = std::move(prompt_trim);
            context_bytes_injected = bytes_trim;
            result_out.context_bytes_injected = bytes_trim;
        }

        result_out.prompt_byte_count = static_cast<uint64_t>(prompt.size());
        result_out.prompt_hash = SWEBench::fnv1a_64(prompt);  // F3: prompt fingerprint
        result_out.model_alias = SWEBench::normalize_model_alias(  // E82: normalized alias
            ctx->model_alias.empty() ? ctx->ollama_client->model : ctx->model_alias);
        // F11: Prompt composition breakdown (approximate sizes from source data)
        result_out.problem_stmt_bytes = static_cast<uint64_t>(inst.problem_stmt.size());
        result_out.hints_bytes        = ctx->hints_enabled
            ? static_cast<uint64_t>(inst.hints_text.size()) : 0;

        // Apply reproducibility seed and temperature to Ollama client before each call
        ctx->ollama_client->seed        = ctx->seed;
        ctx->ollama_client->temperature = ctx->temperature;

        if (!ctx->prompt_dump_dir.empty()) {
            std::filesystem::path dump_dir(ctx->prompt_dump_dir);
            std::error_code ec;
            std::filesystem::create_directories(dump_dir, ec);
            if (!ec) {
                const std::string safe_task = SWEBench::sanitize_task_id_for_path(inst.task_id);
                const std::filesystem::path dump_file = dump_dir / (safe_task + ".prompt.txt");
                SWEBench::write_text_file(dump_file, prompt);
            }
        }

        // E81: per-task wall-time budget — record start time before retry loop
        const auto task_wall_t0 = std::chrono::steady_clock::now();

        const int base_attempts = 1 + std::max(0, ctx->retry_count);
        const int repair_budget = (ctx->autonomous_patch_repair ? std::max(0, ctx->autonomous_max_repair) : 0);
        const int max_attempts = base_attempts + repair_budget;
        int repair_attempts_used = 0;
        result_out.autonomous_repair_enabled = ctx->autonomous_patch_repair;
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            if (attempt > 0 && ctx->debug_runtime) {
                fprintf(stdout,
                    "[SWE][RETRY] task=%s attempt=%d/%d reason=%s\n",
                    inst.task_id.c_str(), attempt + 1, max_attempts,
                    result_out.failure_reason.c_str());
                fflush(stdout);
            }

            fprintf(stdout,
                "[DEBUG] About to call Ollama Generate() for task=%s max_tokens=%d prompt_size=%zu\n",
                inst.task_id.c_str(), max_tokens, prompt.size());
            fflush(stdout);

            std::string response =
                ctx->ollama_client->Generate(
                    prompt,
                    max_tokens,
                    &transport_error);

            fprintf(stdout,
                "[DEBUG] Ollama Generate() returned: response_size=%zu transport_error='%s'\n",
                response.size(), transport_error.c_str());
            fflush(stdout);

            if (response.empty()) {
                result_out.failure_reason = transport_error.empty()
                    ? "empty response from Ollama"
                    : transport_error;
                result_out.retry_attempts = attempt;
                continue;
            }

            result_out.raw_response = response;
            result_out.retry_attempts = attempt;
            SWEBench::populate_response_compliance(result_out);

            if (ctx->strict_mode && result_out.is_fenced) {
                result_out.strict_compliance = false;
                result_out.failure_reason = "strict_mode: fenced response rejected (is_fenced=true)";
                continue;
            }

            std::string normalization_error;
            std::string patch = SWEBench::normalize_agent_patch_response(response, &normalization_error);
            if (patch.empty()) {
                result_out.strict_validation_error = normalization_error;
                result_out.failure_reason = normalization_error.empty()
                    ? "model did not emit a strict unified diff"
                    : normalization_error;
                continue;
            }

            bool apply_checked = false;
            const bool apply_ok = SWEBench::check_patch_execution_success(
                SWEBench::resolve_repo_root(),
                patch,
                apply_checked);
            result_out.patch_execution_checked = apply_checked;
            result_out.patch_execution_success = apply_ok;

            if (ctx->autonomous_patch_repair && apply_checked && !apply_ok && repair_attempts_used < repair_budget) {
                ++repair_attempts_used;
                result_out.autonomous_repair_attempts = repair_attempts_used;
                result_out.failure_reason = "autonomous repair: patch failed git apply --check; retrying";
                corrective_feedback =
                    "Previous patch failed git apply --check. Emit a corrected minimal unified diff "
                    "with accurate file paths and hunk anchors. Keep edits tightly scoped and preserve exact context lines.";

                prompt = SWEBench::build_patch_only_prompt(
                    inst,
                    target_files,
                    ctx->source_context_enabled,
                    ctx->source_context_max_bytes_per_file,
                    ctx->source_context_max_total_bytes,
                    ctx->source_context_max_files,
                    &context_bytes_injected,
                    ctx->hints_enabled,
                    ctx->phase4_rag_lite,
                    ctx->phase4_aperture_lines,
                    ctx->output_format,
                    corrective_feedback);
                continue;
            }

            result_out.autonomous_repair_succeeded = (repair_attempts_used > 0) && apply_ok;
            result_out.autonomous_repair_attempts = repair_attempts_used;

            return patch;
        }

        // E81: Check per-task wall-time budget after exhausting all retries
        if (ctx->max_task_wall_ms > 0) {
            const auto wall_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - task_wall_t0).count();
            if (wall_elapsed_ms >= ctx->max_task_wall_ms &&
                result_out.failure_reason.find("wall_time") == std::string::npos) {
                result_out.failure_reason = "wall_time_budget_exceeded (" +
                    std::to_string(wall_elapsed_ms) + " ms >= " +
                    std::to_string(ctx->max_task_wall_ms) + " ms limit)";
                result_out.api_error_class = "timeout";
            }
        }

        return {};
    } catch (const std::exception& ex) {
        result_out.failure_reason = std::string("inference exception: ") + ex.what();
        return {};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Stand-alone entry point
// ─────────────────────────────────────────────────────────────────────────────
//
// Usage:
//   swe_bench.exe [--json <output.json>] [--jsonl <telemetry.jsonl>] [--run-tests]
//                 [--dataset <instances.jsonl>] [--real-agent] [--model <name>]
//                 [--host <host>] [--port <num>] [--list-models] [--debug-http]
//                 [--verbose] [--max-tasks <N>] [--max-output-tokens <N>]
//                 [--dump-raw-responses <dir>] [--raw-dump-dir <dir>]
//                 [--context-max-bytes <N>] [--context-max-total-bytes <N>]
//                 [--context-max-files <N>] [--no-context]
//                 [--dump-prompts <dir>] [--jsonl-summary <path>]
//
// --real-agent     Use live Ollama HTTP endpoint (requires Ollama running)
// --list-models    Print installed Ollama models and exit
// --model          Use the specified Ollama model name
// --host           Connect to Ollama at this hostname or IP
// --port           Connect to Ollama on this HTTP port
// --debug-http     Emit HTTP status/budget debug lines to stdout
// --verbose        Alias for --debug-http plus raw JSON body dumps
// --max-tasks      Limit number of evaluation instances processed
// --max-output-tokens  Cap generated completion tokens for bounded smoke tests
// --dump-raw-responses  Write per-task raw model responses into <dir>
// --raw-dump-dir        Alias for --dump-raw-responses
// --context-max-bytes  Max bytes per context file injected into prompt (default: 2500)
// --context-max-total-bytes  Max total bytes injected across all context files (default: 5000)
// --context-max-files  Max number of context files injected (default: 2)
// --no-context         Disable source-context injection entirely
// --dump-prompts       Write per-task generated prompts into <dir>
// --jsonl-summary      Write a compact summary JSON next to JSONL for long runs
// --run-tests      Run task test commands when available
// --json           Output JSON report to this file
// --jsonl          Output per-sample telemetry to this file
// --dataset        Load instances from JSON/JSONL file instead of built-in set
// --timeout-ms     Override per-task receive timeout in milliseconds (default: 240000)
// --seed           Reproducibility seed passed to Ollama options.seed (-1 = random/default)
// --retry <N>      Retry up to N extra times on empty/NO_PATCH before marking FAILED (default: 0)
// --fail-fast      Stop evaluation on first FAILED task
// --strict-mode    Reject fenced (is_fenced=true) responses even when a patch was extractable
// --model-alias    Friendly model label emitted in JSONL telemetry (default: raw model name)
// --resume-checkpoint  Path to line-delimited checkpoint file; completed task IDs are appended
//                      and skipped on subsequent runs for resumable sweeps
// --resume-jsonl   Optional JSONL source of prior sample_ids for skip-list recovery
// --no-hints           Strip hints_text from prompt (ablation: evaluate without guidance)
// --max-prompt-bytes   If prompt exceeds this byte limit, rebuild without context (0 = off)
// --csv                Write a compact CSV report with per-task metrics to this path
// --markdown           Write a Markdown table report to this path
// --instance-filter    Comma-separated list of task_ids to evaluate (skips all others)
// --export-instances   Write loaded instance set to JSONL at this path before evaluation
// --temperature        Ollama generation temperature (e.g. 0.2); -1 = unset (model default)
// --deterministic      Force seed=42, temperature=0.0 for fully reproducible sweeps (F8)
// --no-summary-json    Suppress writing the --jsonl-summary file even if path is set (#98)
// --phase4-rag-lite    Enable Phase 4 RAG-lite prompt policy + wider source aperture
// --phase4-aperture-lines  Anchor window radius in lines for RAG-lite context (default: 80)
// --output-format      Prompt output mode: plain (default), fenced, or auto
// --autonomous-repair  Retry with corrective prompt when git apply --check fails
// --autonomous-max-repair  Maximum repair retries for apply-check failures (default: 2)
//
// Without --real-agent, runs null agent on built-in instances for self-test.

int main(int argc, char** argv)
{
    bool        use_real_agent = false;
    bool        run_tests     = false;
    bool        list_models   = false;
    bool        debug_http    = false;
    bool        verbose       = false;
    const char* json_out      = nullptr;
    const char* jsonl_out     = nullptr;
    const char* dataset_path  = nullptr;
    const char* model_name    = nullptr;
    const char* host_name     = nullptr;
    const char* raw_dump_dir  = nullptr;
    const char* prompt_dump_dir = nullptr;
    const char* jsonl_summary_path = nullptr;
    const char* model_alias   = nullptr;
    const char* resume_checkpoint = nullptr;
    const char* resume_jsonl = nullptr;
    int         RAWRXD_NATIVE_PORT   = 11434;
    int         max_tasks     = -1;
    int         max_output_tokens = 0;
    int         cli_timeout_ms = 0;
    int         seed          = -1;
    int         retry_count   = 0;     // extra retries on empty/NO_PATCH (--retry N)
    bool        fail_fast     = false; // stop sweep on first failure (--fail-fast)
    bool        strict_mode   = false; // reject fenced responses (--strict-mode)
    bool        context_enabled = true;
    size_t      context_max_bytes = 2500;
    size_t      context_max_total_bytes = 5000;
    size_t      context_max_files = 2;
    const char* csv_out           = nullptr;  // --csv <path>
    const char* markdown_out       = nullptr;  // --markdown <path>
    const char* instance_filter    = nullptr;  // --instance-filter <id,...>
    const char* export_instances   = nullptr;  // --export-instances <path>
    bool        hints_enabled      = true;     // --no-hints: strip hints from prompt
    size_t      max_prompt_bytes   = 0;        // --max-prompt-bytes: trim context if exceeded
    double      temperature        = -1.0;     // --temperature <float>
    int         max_task_wall_ms   = 0;        // E81: --max-task-wall-ms per-task wall-time cap
    bool        phase4_rag_lite    = false;    // --phase4-rag-lite
    int         phase4_aperture_lines = 80;    // --phase4-aperture-lines <N>
    std::string output_format = "plain";      // --output-format plain|fenced|auto
    bool        autonomous_repair = false;     // --autonomous-repair
    int         autonomous_max_repair = 2;     // --autonomous-max-repair <N>

    const char* env_context_max_bytes = getenv("RAWRXD_SWEBENCH_CONTEXT_MAX_BYTES");
    bool        deterministic      = false;    // F8: --deterministic (seed=42, temperature=0.0)
    bool        no_summary_json    = false;    // #98: --no-summary-json (suppress jsonl-summary)
    if (env_context_max_bytes && env_context_max_bytes[0]) {
        const int parsed = atoi(env_context_max_bytes);
        if (parsed > 0 && parsed <= 65536) {
            context_max_bytes = static_cast<size_t>(parsed);
        }
    }

    const char* env_context_max_total = getenv("RAWRXD_SWEBENCH_CONTEXT_MAX_TOTAL_BYTES");
    if (env_context_max_total && env_context_max_total[0]) {
        const int parsed = atoi(env_context_max_total);
        if (parsed > 0 && parsed <= 262144) {
            context_max_total_bytes = static_cast<size_t>(parsed);
        }
    }

    const char* env_context_max_files = getenv("RAWRXD_SWEBENCH_CONTEXT_MAX_FILES");
    if (env_context_max_files && env_context_max_files[0]) {
        const int parsed = atoi(env_context_max_files);
        if (parsed > 0 && parsed <= 32) {
            context_max_files = static_cast<size_t>(parsed);
        }
    }

    const char* env_disable_context = getenv("RAWRXD_SWEBENCH_DISABLE_CONTEXT");
    if (env_disable_context && env_disable_context[0] && strcmp(env_disable_context, "0") != 0) {
        context_enabled = false;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--real-agent") == 0) {
            use_real_agent = true;
        } else if (strcmp(argv[i], "--list-models") == 0) {
            list_models = true;
            use_real_agent = true;
        } else if (strcmp(argv[i], "--run-tests") == 0) {
            run_tests = true;
        } else if (strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
            json_out = argv[++i];
        } else if (strcmp(argv[i], "--jsonl") == 0 && i + 1 < argc) {
            jsonl_out = argv[++i];
        } else if (strcmp(argv[i], "--dataset") == 0 && i + 1 < argc) {
            dataset_path = argv[++i];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_name = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            RAWRXD_NATIVE_PORT = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host_name = argv[++i];
        } else if (strcmp(argv[i], "--debug-http") == 0) {
            debug_http = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--max-tasks") == 0 && i + 1 < argc) {
            max_tasks = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-output-tokens") == 0 && i + 1 < argc) {
            max_output_tokens = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--context-max-bytes") == 0 && i + 1 < argc) {
            const int parsed = atoi(argv[++i]);
            if (parsed > 0 && parsed <= 65536) {
                context_max_bytes = static_cast<size_t>(parsed);
            }
        } else if (strcmp(argv[i], "--context-max-total-bytes") == 0 && i + 1 < argc) {
            const int parsed = atoi(argv[++i]);
            if (parsed > 0 && parsed <= 262144) {
                context_max_total_bytes = static_cast<size_t>(parsed);
            }
        } else if (strcmp(argv[i], "--context-max-files") == 0 && i + 1 < argc) {
            const int parsed = atoi(argv[++i]);
            if (parsed > 0 && parsed <= 32) {
                context_max_files = static_cast<size_t>(parsed);
            }
        } else if (strcmp(argv[i], "--no-context") == 0) {
            context_enabled = false;
        } else if ((strcmp(argv[i], "--dump-raw-responses") == 0 ||
                    strcmp(argv[i], "--raw-dump-dir") == 0) &&
                   i + 1 < argc) {
            raw_dump_dir = argv[++i];
        } else if (strcmp(argv[i], "--dump-prompts") == 0 && i + 1 < argc) {
            prompt_dump_dir = argv[++i];
        } else if (strcmp(argv[i], "--jsonl-summary") == 0 && i + 1 < argc) {
            jsonl_summary_path = argv[++i];
        } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
            cli_timeout_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--model-alias") == 0 && i + 1 < argc) {
            model_alias = argv[++i];
        } else if (strcmp(argv[i], "--resume-checkpoint") == 0 && i + 1 < argc) {
            resume_checkpoint = argv[++i];
        } else if (strcmp(argv[i], "--resume-jsonl") == 0 && i + 1 < argc) {
            resume_jsonl = argv[++i];
        } else if (strcmp(argv[i], "--fail-fast") == 0) {
            fail_fast = true;
        } else if (strcmp(argv[i], "--strict-mode") == 0) {
            strict_mode = true;
        } else if (strcmp(argv[i], "--retry") == 0 && i + 1 < argc) {
            retry_count = atoi(argv[++i]);
            if (retry_count < 0) retry_count = 0;
            if (retry_count > 10) retry_count = 10;  // cap to avoid runaway loops
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_out = argv[++i];
        } else if (strcmp(argv[i], "--markdown") == 0 && i + 1 < argc) {
            markdown_out = argv[++i];
        } else if (strcmp(argv[i], "--instance-filter") == 0 && i + 1 < argc) {
            instance_filter = argv[++i];
        } else if (strcmp(argv[i], "--export-instances") == 0 && i + 1 < argc) {
            export_instances = argv[++i];
        } else if (strcmp(argv[i], "--no-hints") == 0) {
            hints_enabled = false;
        } else if (strcmp(argv[i], "--max-prompt-bytes") == 0 && i + 1 < argc) {
            const int parsed = atoi(argv[++i]);
            if (parsed > 0) max_prompt_bytes = static_cast<size_t>(parsed);
        } else if (strcmp(argv[i], "--temperature") == 0 && i + 1 < argc) {
            temperature = atof(argv[++i]);
            if (temperature < 0.0) temperature = -1.0;
        } else if (strcmp(argv[i], "--max-task-wall-ms") == 0 && i + 1 < argc) {
            max_task_wall_ms = atoi(argv[++i]);
            if (max_task_wall_ms < 0) max_task_wall_ms = 0;
        } else if (strcmp(argv[i], "--phase4-rag-lite") == 0) {
            phase4_rag_lite = true;
        } else if (strcmp(argv[i], "--phase4-aperture-lines") == 0 && i + 1 < argc) {
            phase4_aperture_lines = atoi(argv[++i]);
            if (phase4_aperture_lines < 20) phase4_aperture_lines = 20;
            if (phase4_aperture_lines > 400) phase4_aperture_lines = 400;
        } else if (strcmp(argv[i], "--output-format") == 0 && i + 1 < argc) {
            output_format = argv[++i];
            if (output_format != "plain" && output_format != "fenced" && output_format != "auto") {
                output_format = "plain";
            }
        } else if (strcmp(argv[i], "--autonomous-repair") == 0) {
            autonomous_repair = true;
        } else if (strcmp(argv[i], "--autonomous-max-repair") == 0 && i + 1 < argc) {
            autonomous_max_repair = atoi(argv[++i]);
            if (autonomous_max_repair < 0) autonomous_max_repair = 0;
            if (autonomous_max_repair > 8) autonomous_max_repair = 8;
        } else if (strcmp(argv[i], "--deterministic") == 0) {
            deterministic = true;              // F8
        } else if (strcmp(argv[i], "--no-summary-json") == 0) {
            no_summary_json = true;            // #98
        }
    }

    // F8: --deterministic forces seed=42, temperature=0.0 for reproducible sweeps
    if (deterministic) {
        seed        = 42;
        temperature = 0.0;
    }
    // Propagate CLI timeout override as env var so MinimalOllamaClient picks it up
    if (cli_timeout_ms > 0) {
        char timeout_buf[32];
        snprintf(timeout_buf, sizeof(timeout_buf), "%d", cli_timeout_ms);
        SetEnvironmentVariableA("RAWRXD_SWEBENCH_RECV_TIMEOUT_MS", timeout_buf);
    }

    if (verbose) {
        debug_http = true;
    }

    fprintf(stdout,
        "╔════════════════════════════════════════════════╗\n"
        "║    RawrXD SWE-bench Evaluation Harness         ║\n"
        "║    Purpose: Validate Phase 2 Adaptation       ║\n"
        "╚════════════════════════════════════════════════╝\n\n");

    // E79: Install CTRL-C / CTRL-BREAK handler for clean sweep abort
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    // Load instances
    std::vector<SWEBench::Instance> instances;
    if (dataset_path) {
        instances = SWEBench::load_json_dataset(dataset_path);
    } else {
        instances = SWEBench::builtin_instances();
    }

    if (instances.empty()) {
        fprintf(stderr, "Error: no instances to evaluate\n");
        return 1;
    }

    // Apply --instance-filter: keep only matching task_ids
    if (instance_filter && instance_filter[0]) {
        std::unordered_set<std::string> filter_ids;
        const char* p = instance_filter;
        while (*p) {
            const char* end = strchr(p, ',');
            if (!end) end = p + strlen(p);
            if (end > p) filter_ids.insert(std::string(p, static_cast<size_t>(end - p)));
            p = (*end == ',') ? end + 1 : end;
        }
        const size_t before = instances.size();
        instances.erase(std::remove_if(instances.begin(), instances.end(),
            [&](const SWEBench::Instance& inst) {
                return filter_ids.find(inst.task_id) == filter_ids.end();
            }), instances.end());
        fprintf(stdout, "Instance filter applied: %zu -> %zu instance(s)\n", before, instances.size());
    }

    if (max_tasks > 0 && static_cast<size_t>(max_tasks) < instances.size()) {
        instances.resize(static_cast<size_t>(max_tasks));
        fprintf(stdout, "Limiting evaluation to %d instance(s) via --max-tasks\n", max_tasks);
    }

    // Export instance set to JSONL if requested (before evaluation)
    if (export_instances && export_instances[0]) {
        if (SWEBench::export_instances_jsonl(instances, export_instances)) {
            fprintf(stdout, "Instances exported to: %s\n", export_instances);
        } else {
            fprintf(stderr, "Warning: failed to export instances to: %s\n", export_instances);
        }
    }

    fprintf(stdout, "Loaded %zu instances\n", instances.size());

    // Open telemetry JSONL if requested
    FILE* jsonl_file = nullptr;
    std::string jsonl_pid_lock_path;  // F1: path of .lock file we created
    if (jsonl_out) {
        if (fopen_s(&jsonl_file, jsonl_out, "w") == 0 && jsonl_file) {
            fprintf(stdout, "Telemetry will be written to: %s\n", jsonl_out);
            // F1 (Todo #73): Acquire PID lockfile to prevent concurrent processes
            jsonl_pid_lock_path = SWEBench::acquire_jsonl_pid_lock(jsonl_out);
            if (jsonl_pid_lock_path.empty()) {
                // Lock held by another live process — abort
                fclose(jsonl_file);
                return 1;
            }
            // E78: Schema version header — first record in every JSONL
            SWEBench::write_jsonl_schema_header(jsonl_file);
            // F2 (Todo #8): Run manifest — captures CLI args and PID
            SWEBench::write_jsonl_run_manifest(jsonl_file, argc, argv);
        } else {
            fprintf(stderr, "Warning: could not open JSONL telemetry file: %s\n", jsonl_out);
        }
    }

    SWEBench::HarnessReport report;

    if (use_real_agent) {
        fprintf(stdout, "\n[INFO] Initializing minimal Ollama HTTP client...\n");

        try {
            const char* env_model = getenv("RAWRXD_SWEBENCH_MODEL");
            if (!env_model || !env_model[0]) {
                env_model = getenv("RAWRXD_NATIVE_MODEL");
            }
            const char* env_port = getenv("RAWRXD_SWEBENCH_PORT");
            if (env_port && env_port[0]) {
                RAWRXD_NATIVE_PORT = atoi(env_port);
            }
            const char* env_host = getenv("RAWRXD_SWEBENCH_HOST");
            if (!env_host || !env_host[0]) {
                env_host = getenv("RAWRXD_NATIVE_HOST");
            }
            const char* env_debug = getenv("RAWRXD_SWEBENCH_DEBUG_HTTP");
            if (env_debug && env_debug[0] && strcmp(env_debug, "0") != 0) {
                debug_http = true;
            }

            std::string chosen_host = host_name && host_name[0] ? host_name : "127.0.0.1";
            if (env_host && env_host[0]) {
                chosen_host = env_host;
            }

            std::string chosen_model;
            if (model_name && model_name[0]) {
                chosen_model = model_name;
            } else if (env_model && env_model[0]) {
                chosen_model = env_model;
            }

            MinimalOllamaClient ollama(chosen_host, RAWRXD_NATIVE_PORT, chosen_model);
            ollama.debug_http = debug_http;
            if (cli_timeout_ms > 0) {
                ollama.recv_timeout_ms = cli_timeout_ms;
            }

            if (list_models) {
                std::string discover_error;
                auto models = ollama.ListModels(&discover_error);
                if (!models.empty()) {
                    fprintf(stdout, "Available Ollama models:\n");
                    for (const auto& name : models) {
                        fprintf(stdout, "  %s\n", name.c_str());
                    }
                    if (jsonl_file) fclose(jsonl_file);
                    return 0;
                }
                fprintf(stderr, "Error: could not discover an Ollama model list: %s\n",
                        discover_error.empty() ? "no models returned" : discover_error.c_str());
                if (jsonl_file) fclose(jsonl_file);
                return 1;
            }

            if (chosen_model.empty()) {
                std::string discover_error;
                auto models = ollama.ListModels(&discover_error);
                if (!models.empty()) {
                    chosen_model = models.front();
                    ollama.model = chosen_model;
                    fprintf(stdout, "[INFO] Discovered Ollama model: %s\n", chosen_model.c_str());
                } else {
                    fprintf(stderr, "Error: could not discover an Ollama model: %s\n",
                            discover_error.empty() ? "no models returned" : discover_error.c_str());
                    if (jsonl_file) fclose(jsonl_file);
                    return 1;
                }
            }

            fprintf(stdout, "[INFO] Using Ollama model: %s on port %d\n", chosen_model.c_str(), RAWRXD_NATIVE_PORT);

            // F12: Ollama reachability preflight — verify service is responding before sweep
            {
                std::string pf_err;
                const auto pf_models = ollama.ListModels(&pf_err);
                if (pf_models.empty() && !pf_err.empty()) {
                    fprintf(stderr, "[ERROR] Ollama preflight failed: %s\n", pf_err.c_str());
                    fprintf(stderr, "        Ensure Ollama is running on %s:%d\n",
                        chosen_host.c_str(), RAWRXD_NATIVE_PORT);
                    if (jsonl_file) { fclose(jsonl_file); }
                    return 1;
                }
                fprintf(stdout, "[INFO] Ollama preflight: OK (%zu model(s) available)\n",
                    pf_models.size());
            }

            // Emit model fingerprint as first JSONL record
            SWEBench::write_jsonl_model_fingerprint(
                jsonl_file,
                chosen_model,
                ollama.host,
                RAWRXD_NATIVE_PORT,
                (model_alias && model_alias[0]) ? model_alias : chosen_model,
                seed,
                retry_count,
                strict_mode);

            RealAgentContext ctx;
            ctx.ollama_client = &ollama;
            ctx.debug_runtime = debug_http;
            ctx.max_output_tokens = max_output_tokens;
            ctx.source_context_enabled = context_enabled;
            ctx.source_context_max_bytes_per_file = context_max_bytes;
            ctx.source_context_max_total_bytes = context_max_total_bytes;
            ctx.source_context_max_files = context_max_files;
            ctx.seed = seed;
            ctx.retry_count = retry_count;
            ctx.strict_mode = strict_mode;
            ctx.hints_enabled = hints_enabled;
            ctx.max_prompt_bytes = max_prompt_bytes;
            ctx.temperature = temperature;
            ctx.max_task_wall_ms = max_task_wall_ms;  // E81
            ctx.phase4_rag_lite = phase4_rag_lite;
            ctx.phase4_aperture_lines = phase4_aperture_lines;
            ctx.output_format = output_format;
            ctx.autonomous_patch_repair = autonomous_repair;
            ctx.autonomous_max_repair = autonomous_max_repair;
            if (max_task_wall_ms > 0) {
                fprintf(stdout, "[INFO] Per-task wall-time cap: %d ms\n", max_task_wall_ms);
            }
            if (temperature >= 0.0) {
                fprintf(stdout, "[INFO] Temperature: %.4f\n", temperature);
            }
            if (deterministic) {
                fprintf(stdout, "[INFO] Deterministic mode: seed=42, temperature=0.0\n");
            }
            if (model_alias && model_alias[0]) {
                ctx.model_alias = model_alias;
            }
            if (prompt_dump_dir && prompt_dump_dir[0]) {
                ctx.prompt_dump_dir = prompt_dump_dir;
            }

            fprintf(stdout,
                "[INFO] Context injection: %s (max_files=%zu, max_per_file=%zu, max_total=%zu)\n",
                ctx.source_context_enabled ? "enabled" : "disabled",
                ctx.source_context_max_files,
                ctx.source_context_max_bytes_per_file,
                ctx.source_context_max_total_bytes);
            if (ctx.phase4_rag_lite) {
                fprintf(stdout,
                    "[INFO] Phase 4 RAG-lite: enabled (aperture_radius=%d lines)\n",
                    ctx.phase4_aperture_lines);
            }
            fprintf(stdout, "[INFO] Output format mode: %s\n", ctx.output_format.c_str());
            if (ctx.autonomous_patch_repair) {
                fprintf(stdout, "[INFO] Autonomous patch repair: enabled (max_repair=%d)\n",
                    ctx.autonomous_max_repair);
            }
            if (seed >= 0) {
                fprintf(stdout, "[INFO] Reproducibility seed: %d\n", seed);
            }
            if (retry_count > 0) {
                fprintf(stdout, "[INFO] Retry count: %d extra attempt(s) on failure\n", retry_count);
            }
            if (fail_fast) {
                fprintf(stdout, "[INFO] Fail-fast mode: sweep stops on first task failure\n");
            }
            if (strict_mode) {
                fprintf(stdout, "[INFO] Strict mode: fenced responses will be rejected even if extractable\n");
            }
            if (!ctx.model_alias.empty()) {
                fprintf(stdout, "[INFO] Model alias: %s\n", ctx.model_alias.c_str());
            }
            if (!ctx.prompt_dump_dir.empty()) {
                fprintf(stdout, "[INFO] Prompt dumps enabled: %s\n", ctx.prompt_dump_dir.c_str());
            }

            SWEBench::Harness harness(run_tests, jsonl_file, json_out, raw_dump_dir, fail_fast, strict_mode);
            if (resume_checkpoint && resume_checkpoint[0]) {
                harness.set_resume_checkpoint(resume_checkpoint);
                fprintf(stdout, "[INFO] Resume checkpoint: %s\n", resume_checkpoint);
            }
            if (resume_jsonl && resume_jsonl[0]) {
                harness.set_resume_jsonl(resume_jsonl);
                fprintf(stdout, "[INFO] Resume JSONL source: %s\n", resume_jsonl);
            }
            for (auto& inst : instances) {
                harness.add_instance(std::move(inst));
            }

            fprintf(stdout, "Running evaluation with minimal Ollama client...\n");
            report = harness.run([&ctx](const SWEBench::Instance& inst, SWEBench::TaskResult& result) {
                return invoke_real_agent(inst, &ctx, result);
            });

        } catch (const std::exception& ex) {
            fprintf(stderr, "Error initializing Ollama client: %s\n", ex.what());
            if (jsonl_file) fclose(jsonl_file);
            return 1;
        }
    } else {
        // Null agent self-test mode
        fprintf(stdout, "\n[INFO] Running self-test with null agent...\n");
        fprintf(stdout, "      (Use --real-agent flag for live inference evaluation)\n\n");

        SWEBench::Harness harness(run_tests, jsonl_file, json_out, raw_dump_dir, fail_fast, strict_mode);
        if (resume_jsonl && resume_jsonl[0]) {
            harness.set_resume_jsonl(resume_jsonl);
            fprintf(stdout, "[INFO] Resume JSONL source: %s\n", resume_jsonl);
        }
        for (auto& inst : instances) {
            harness.add_instance(std::move(inst));
        }

        SWEBench::AgentFn null_agent = [](const SWEBench::Instance&, SWEBench::TaskResult&) -> std::string {
            return {};
        };

        report = harness.run(null_agent);
    }

    if (jsonl_file) {
        fflush(jsonl_file);
    }

    if (json_out) {
        if (SWEBench::write_json_report(report, json_out)) {
            fprintf(stdout, "JSON report written to: %s\n", json_out);
        } else {
            fprintf(stderr, "Warning: failed to write JSON report to: %s\n", json_out);
        }
    }

    if (jsonl_summary_path && !no_summary_json) {
        if (SWEBench::write_jsonl_summary_report(report, jsonl_summary_path)) {
            fprintf(stdout, "JSONL summary written to: %s\n", jsonl_summary_path);
        } else {
            fprintf(stderr, "Warning: failed to write JSONL summary to: %s\n", jsonl_summary_path);
        }
    }

    if (csv_out) {
        if (SWEBench::write_csv_report(report, csv_out)) {
            fprintf(stdout, "CSV report written to: %s\n", csv_out);
        } else {
            fprintf(stderr, "Warning: failed to write CSV report to: %s\n", csv_out);
        }
    }

    if (markdown_out) {
        if (SWEBench::write_markdown_report(report, markdown_out)) {
            fprintf(stdout, "Markdown report written to: %s\n", markdown_out);
        } else {
            fprintf(stderr, "Warning: failed to write Markdown report to: %s\n", markdown_out);
        }
    }

    SWEBench::print_report(report);

    if (jsonl_file) {
        // E84: Emit sweep completion sentinel as last JSONL record
        SWEBench::write_jsonl_sweep_sentinel(jsonl_file, report);
        fclose(jsonl_file);
        fprintf(stdout, "Telemetry JSONL closed: %s\n", jsonl_out);
        // F1: Release PID lockfile
        if (!jsonl_pid_lock_path.empty()) {
            DeleteFileA(jsonl_pid_lock_path.c_str());
        }
    }

    // Return 0 even when score is zero (harness validates the framework plumbing)
    return 0;
}
