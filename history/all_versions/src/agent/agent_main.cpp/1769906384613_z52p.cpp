#include "planner.hpp"
#include "self_patch.hpp"
#include "self_code.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "self_test_gate.hpp"
#include "ide_agent_bridge_hot_patching_integration.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <thread>

using json = nlohmann::json;

// Global bridge instance to keep proxy alive
std::unique_ptr<IDEAgentBridgeWithHotPatching> g_bridge;

int main(int argc, char *argv[]) {
    // Initialize Hot Patching Bridge
    g_bridge = std::make_unique<IDEAgentBridgeWithHotPatching>();
    g_bridge->initializeWithHotPatching();
    g_bridge->startHotPatchingProxy();
    
    // Allow proxy to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (argc < 2) {
        
        return 1;
    }

    std::string wish;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            wish += " ";
        }
        wish += argv[i];
    }

    // ============================================================================
    // Step 1: Plan
    // ============================================================================
    Planner planner;
    json tasks = planner.plan(wish);

    if (!tasks.is_array() || tasks.empty()) {
        
        return 1;
    }

    // ============================================================================
    // Step 2: Execute
    // ============================================================================
    SelfPatch patch;
    ReleaseAgent rel;
    MetaLearn ml;

    bool success = true;
    size_t taskCount = 0;
    size_t failureCount = 0;

    for (const auto& task : tasks) {
        if (!task.is_object()) {
            continue;
        }
        std::string type = task.value("type", "");
        ++taskCount;

        if (type == "add_kernel") {
            success = patch.addKernel(task.value("target", ""), task.value("template", ""));
        } else if (type == "add_cpp") {
            std::string deps;
            if (task.contains("deps") && task["deps"].is_array()) {
                for (const auto& val : task["deps"]) {
                    if (!deps.empty()) {
                        deps += ",";
                    }
                    deps += val.get<std::string>();
                }
            } else {
                deps = task.value("deps", "");
            }
            success = patch.addCpp(task.value("target", ""), deps);
        } else if (type == "build") {
            std::string cmd = "cmake --build build --config Release";
            std::string target = task.value("target", "");
            if (!target.empty()) {
                cmd += " --target " + target;
            }
            int rc = std::system(cmd.c_str());
            success = (rc == 0);
        } else if (type == "hot_reload") {
            success = patch.hotReload();
        } else if (type == "bump_version") {
            success = rel.bumpVersion(task.value("part", ""));
        } else if (type == "tag") {
            success = rel.tagAndUpload();
        } else if (type == "tweet") {
            success = rel.tweet(task.value("text", ""));
        } else if (type == "meta_learn") {
            success = ml.record(
                task.value("quant", ""),
                task.value("kernel", ""),
                task.value("gpu", ""),
                task.value("tps", 0.0),
                task.value("ppl", 0.0)
            );
        } else if (type == "bench" || type == "bench_all") {
            success = true;
        }

        if (!success) {
            failureCount++;
            return 1;
        }
    }

    // ============================================================================
    // Summary
    // ============================================================================

    std::string suggested = ml.suggestQuant();
    (void)suggested;
    (void)taskCount;
    (void)failureCount;

    return 0;
}
