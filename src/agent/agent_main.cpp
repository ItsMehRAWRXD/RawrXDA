/**
 * @file agent_main.cpp
 * @brief RawrXD Autonomous Agent — Zero-touch IDE automation entry point
 *
 * Architecture: C++20, no Qt, no exceptions
 * Process execution: CreateProcessA (Windows) / fork+exec (POSIX)
 */

#include "planner.hpp"
#include "self_patch.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "self_test_gate.hpp"
#include "simple_json.hpp"
#include "agent_self_healing_orchestrator.hpp"
#include "../gpu_enforcement.h"
#include "agentic_hotpatch_orchestrator.hpp"

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Process Execution Helper
// ═══════════════════════════════════════════════════════════════════════════

#ifdef _WIN32
/**
 * @brief Execute process and return exit code (Windows)
 */
static int executeProcess(const std::string& program, const std::vector<std::string>& args)
{
    // Build command line
    std::string cmdLine = program;
    for (const auto& arg : args) {
        cmdLine += " ";
        if (arg.find(' ') != std::string::npos) {
            cmdLine += "\"" + arg + "\"";
        } else {
            cmdLine += arg;
        }
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return static_cast<int>(exitCode);
}
#else
/**
 * @brief Execute process and return exit code (POSIX)
 */
static int executeProcess(const std::string& program, const std::vector<std::string>& args)
{
    std::string cmdLine = program;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }
    return system(cmdLine.c_str());
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Command Line Parsing
// ═══════════════════════════════════════════════════════════════════════════

static void printHelp()
{
    // Help text output disabled
}

static void printVersion()
{
    // Version output disabled
}

// ═══════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[])
{
    // Mandatory GPU gate — the agent CLI refuses to start without a GPU.
    rxd::gpu::require();

    // ============================================================================
    // Zero-Touch Initialization: Counter-Strike Mode
    // ============================================================================
    PatchResult initRes = AgentSelfHealingOrchestrator::instance().initialize();
    if (!initRes.success) {
        // Self-healing orchestrator offline
    } else {
        // Autonomous stabilization active
    }

    // Parse arguments manually (replaces QCommandLineParser)
    std::string wish;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }

        // First non-flag argument is the wish
        if (wish.empty() && !arg.starts_with("-")) {
            wish = arg;
        }
    }

    if (wish.empty()) {
        return 1;
    }

    // ============================================================================
    // Step 1: Plan
    // ============================================================================
    Planner planner;
    JsonValue tasks = planner.plan(wish);

    if (tasks.size() == 0) {
        return 1;
    }

    // ============================================================================
    // Step 2: Execute
    // ============================================================================
    SelfPatch patch;
    ReleaseAgent rel;
    MetaLearn ml;

    bool success = true;
    int taskCount = 0;
    int failureCount = 0;

    for (size_t idx = 0; idx < tasks.size(); ++idx) {
        JsonValue task = tasks[idx];
        std::string type = task.value("type").toString();

        if (type == "add_kernel") {
            success = patch.addKernel(task.value("target").toString(),
                                       task.value("template").toString());
        } else if (type == "add_cpp") {
            std::string deps;
            if (task.value("deps").isArray()) {
                std::vector<std::string> parts;
                for (const auto& val : task.value("deps").toArray()) {
                    parts.push_back(val.toString());
                }
                deps = strutil::join(parts, ",");
            } else {
                deps = task.value("deps").toString();
            }
            success = patch.addCpp(task.value("target").toString(), deps);
        } else if (type == "build") {
            std::string target = task.value("target").toString();
            std::vector<std::string> args = {"--build", "build", "--config", "Release"};
            if (!target.empty()) {
                args.push_back("--target");
                args.push_back(target);
            }
            int rc = executeProcess("cmake", args);
            success = (rc == 0);
        } else if (type == "hot_reload") {
            success = patch.hotReload();
        } else if (type == "bump_version") {
            success = rel.bumpVersion(task.value("part").toString());
        } else if (type == "tag") {
            success = rel.tagAndUpload();
        } else if (type == "tweet") {
            success = rel.tweet(task.value("text").toString());
        } else if (type == "meta_learn") {
            success = ml.record(
                task.value("quant").toString(),
                task.value("kernel").toString(),
                task.value("gpu").toString(),
                task.value("tps").toDouble(),
                task.value("ppl").toDouble()
            );
        } else if (type == "bench" || type == "bench_all") {
            // Benchmark handled by build
        }

        if (!success) {
            failureCount++;

            // Autonomous repair on task failure
            SelfHealReport report = AgentSelfHealingOrchestrator::instance().runHealingCycle();
            if (report.bugsFixed > 0) {
                // Fixed degradation issues, retry task
            } else {
                // No degradation found, rolling back
            }
            return 1;
        }
    }

    // ============================================================================
    // Summary
    // ============================================================================

    std::string suggested = ml.suggestQuant();

    double successRate = (taskCount > 0) ? (100.0 * (taskCount - failureCount) / taskCount) : 0.0;

    return 0;
}
