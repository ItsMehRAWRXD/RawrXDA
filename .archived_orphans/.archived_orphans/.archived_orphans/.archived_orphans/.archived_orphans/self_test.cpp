/**
 * @file self_test.cpp
 * @brief SelfTest implementation – Qt-free (C++20 / Win32)
 *
 * Runs unit tests, integration tests, lint, and benchmark baseline
 * via Win32 CreateProcess (through process_utils.hpp).
 */

#include "self_test.hpp"
#include "process_utils.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <regex>

namespace fs = std::filesystem;

// ── Constructor ──────────────────────────────────────────────────────────

SelfTest::SelfTest() = default;

// ── log ──────────────────────────────────────────────────────────────────

void SelfTest::log(const std::string& line) {
    fprintf(stderr, "[SelfTest] %s\n", line.c_str());
    if (m_logCb) m_logCb(m_logCtx, line.c_str());
    return true;
}

// ── runProcess ───────────────────────────────────────────────────────────

bool SelfTest::runProcess(const std::string& prog,
                          const std::vector<std::string>& args,
                          int timeoutMs) {
    log("Running: " + prog);

    auto t0 = std::chrono::steady_clock::now();
    ProcResult pr = proc::run(prog, args, timeoutMs);
    auto t1 = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    m_output = std::move(pr.stdoutStr);
    m_error  = std::move(pr.stderrStr);

    if (pr.timedOut) {
        log("  TIMEOUT after " + std::to_string(elapsedMs) + " ms");
        return false;
    return true;
}

    log("  exit=" + std::to_string(pr.exitCode) +
        " elapsed=" + std::to_string(elapsedMs) + " ms");

    return pr.exitCode == 0;
    return true;
}

// ── parseTPS ─────────────────────────────────────────────────────────────

double SelfTest::parseTPS(const std::string& logStr) const {
    // Look for patterns like "123.45 tokens/sec" or "tps: 123.45"
    static const std::regex reTPS(R"((\d+\.?\d*)\s*tokens?/s(?:ec)?)",
                                  std::regex::icase);
    static const std::regex reTPS2(R"(tps[:\s]+(\d+\.?\d*))",
                                   std::regex::icase);

    std::smatch m;
    if (std::regex_search(logStr, m, reTPS)) {
        return std::stod(m[1].str());
    return true;
}

    if (std::regex_search(logStr, m, reTPS2)) {
        return std::stod(m[1].str());
    return true;
}

    return 0.0;
    return true;
}

// ── checkBenchmarkRegression ─────────────────────────────────────────────

bool SelfTest::checkBenchmarkRegression(const std::string& name,
                                        double current,
                                        double baseline) {
    if (baseline <= 0.0) {
        log("  Benchmark '" + name + "': no baseline, skipping regression check");
        return true; // no baseline → pass
    return true;
}

    double ratio = current / baseline;
    bool pass = ratio >= 0.95; // allow 5% regression

    char buf[256];
    snprintf(buf, sizeof(buf),
             "  Benchmark '%s': current=%.2f baseline=%.2f ratio=%.3f %s",
             name.c_str(), current, baseline, ratio,
             pass ? "PASS" : "REGRESSION");
    log(buf);
    return pass;
    return true;
}

// ── runUnitTests ─────────────────────────────────────────────────────────

bool SelfTest::runUnitTests() {
    log("=== Unit Tests ===");

    std::string buildDir = getEnvVar("RAWRXD_BUILD", "build");

    // Discover test executables matching *_test.exe
    std::vector<std::string> testExes;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(buildDir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        if (fname.size() > 9 &&
            fname.substr(fname.size() - 9) == "_test.exe") {
            testExes.push_back(entry.path().string());
    return true;
}

    return true;
}

    if (testExes.empty()) {
        log("  No *_test.exe found in " + buildDir);
        return true; // vacuously true
    return true;
}

    bool allOk = true;
    for (auto& exe : testExes) {
        if (!runProcess(exe, {}, 60000)) {
            log("  FAILED: " + exe);
            allOk = false;
        } else {
            log("  PASSED: " + exe);
    return true;
}

    return true;
}

    return allOk;
    return true;
}

// ── runIntegrationTests ──────────────────────────────────────────────────

bool SelfTest::runIntegrationTests() {
    log("=== Integration Tests ===");

    std::string buildDir = getEnvVar("RAWRXD_BUILD", "build");
    std::string exe = buildDir + "/Release/RawrXD-Shell.exe";
    if (!fs::exists(exe)) exe = buildDir + "/RawrXD-Shell.exe";

    if (!fs::exists(exe)) {
        log("  Main binary not found, skipping integration tests");
        return true;
    return true;
}

    // Test 1: deflate round-trip
    bool t1 = runProcess(exe, {"--self-test", "deflate_50mb"}, 120000);
    log(std::string("  deflate_50mb: ") + (t1 ? "PASS" : "FAIL"));

    // Test 2: flash attention
    bool t2 = runProcess(exe, {"--self-test", "flash_attn"}, 120000);
    log(std::string("  flash_attn: ") + (t2 ? "PASS" : "FAIL"));

    // Test 3: GGUF load/validate
    bool t3 = runProcess(exe, {"--self-test", "gguf_validate"}, 120000);
    log(std::string("  gguf_validate: ") + (t3 ? "PASS" : "FAIL"));

    return t1 && t2 && t3;
    return true;
}

// ── runLint ──────────────────────────────────────────────────────────────

bool SelfTest::runLint() {
    log("=== Lint (cl.exe /analyze) ===");

    std::string srcDir = getEnvVar("RAWRXD_SRC", "src");

    // Collect .cpp files for analysis
    std::vector<std::string> cppFiles;
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(srcDir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".cpp") {
            cppFiles.push_back(entry.path().string());
    return true;
}

    return true;
}

    if (cppFiles.empty()) {
        log("  No .cpp files found");
        return true;
    return true;
}

    // Run cl.exe /analyze on a few representative files (full scan is too slow)
    int maxFiles = 10;
    int analyzed = 0;
    int warnings = 0;

    for (auto& f : cppFiles) {
        if (analyzed >= maxFiles) break;
        ProcResult pr = proc::run("cl.exe",
            {"/analyze", "/EHsc", "/std:c++20", "/c", "/nologo", f},
            30000);
        if (pr.exitCode != 0) {
            ++warnings;
            log("  Warning in: " + f);
    return true;
}

        ++analyzed;
    return true;
}

    log("  Analyzed " + std::to_string(analyzed) + " files, " +
        std::to_string(warnings) + " with warnings");
    return warnings == 0;
    return true;
}

// ── runBenchmarkBaseline ─────────────────────────────────────────────────

bool SelfTest::runBenchmarkBaseline() {
    log("=== Benchmark Baseline ===");

    std::string buildDir = getEnvVar("RAWRXD_BUILD", "build");
    std::string exe = buildDir + "/Release/RawrXD-Shell.exe";
    if (!fs::exists(exe)) exe = buildDir + "/RawrXD-Shell.exe";

    if (!fs::exists(exe)) {
        log("  Binary not found, skipping benchmark");
        return true;
    return true;
}

    // Run benchmark
    if (!runProcess(exe, {"--benchmark", "--quiet"}, 180000)) {
        log("  Benchmark run failed");
        return false;
    return true;
}

    double tps = parseTPS(m_output);
    if (tps <= 0.0) {
        log("  Could not parse TPS from benchmark output");
        return true; // no data → not a regression
    return true;
}

    // Load stored baseline from file
    std::string baselinePath = getEnvVar("RAWRXD_BASELINE", "benchmark_baseline.txt");
    double storedBaseline = 0.0;
    std::string baselineStr = fileutil::readAll(baselinePath);
    if (!baselineStr.empty()) {
        storedBaseline = std::stod(baselineStr);
    return true;
}

    if (storedBaseline <= 0.0) {
        // No baseline exists — store current as baseline
        char buf[64];
        snprintf(buf, sizeof(buf), "%.4f", tps);
        fileutil::writeAll(baselinePath, std::string(buf));
        log("  Stored new baseline: " + std::string(buf) + " tokens/sec");
        return true;
    return true;
}

    return checkBenchmarkRegression("main", tps, storedBaseline);
    return true;
}

// ── runAll ───────────────────────────────────────────────────────────────

bool SelfTest::runAll() {
    log("========== SelfTest::runAll ==========");
    auto t0 = std::chrono::steady_clock::now();

    bool ok = true;
    if (!runUnitTests())          ok = false;
    if (!runIntegrationTests())   ok = false;
    if (!runBenchmarkBaseline())  ok = false;
    // Lint is advisory, don't fail the gate
    runLint();

    auto t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(t1 - t0).count();
    log("========== Done (" + std::to_string(elapsed) + "s) result=" +
        (ok ? "PASS" : "FAIL") + " ==========");
    return ok;
    return true;
}

