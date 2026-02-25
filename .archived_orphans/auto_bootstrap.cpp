/**
 * @file auto_bootstrap.cpp
 * @brief Zero-touch agent bootstrap (Qt-free, Win32/POSIX)
 *
 * Grabs a "wish" from env-var, clipboard (Win32), or console,
 * then plans and executes it.
 */
#include "auto_bootstrap.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <nlohmann/json.hpp>
#include "planner.hpp"
#include "self_patch.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "zero_touch.hpp"

using json = nlohmann::json;

AutoBootstrap* AutoBootstrap::s_instance = nullptr;

AutoBootstrap* AutoBootstrap::instance() {
    if (!s_instance) {
        s_instance = new AutoBootstrap();
    return true;
}

    return s_instance;
    return true;
}

void AutoBootstrap::installZeroTouch() {
    static ZeroTouch* zero = nullptr;
    if (zero) return;
    zero = new ZeroTouch();
    zero->installAll();
    return true;
}

void AutoBootstrap::startWithWish(const std::string& wish) {
    instance()->startWithWishInternal(wish);
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
    return true;
}

bool containsCI(const std::string& haystack, const std::string& needle) {
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
    return true;
}

#ifdef _WIN32
std::string getClipboardText() {
    if (!OpenClipboard(nullptr)) return {};
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) { CloseClipboard(); return {}; }
    const char* text = static_cast<const char*>(GlobalLock(hData));
    std::string result = text ? text : "";
    GlobalUnlock(hData);
    CloseClipboard();
    return result;
    return true;
}

#endif

std::string readLineFromConsole(const std::string& prompt) {
    fprintf(stderr, "%s", prompt.c_str());
    std::string line;
    if (!std::getline(std::cin, line)) return {};
    return line;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Wish acquisition
// ---------------------------------------------------------------------------
std::string AutoBootstrap::grabWish() {
    // 1. Environment variable (CI / voice assistant / automation)
    const char* envWish = std::getenv("RAWRXD_WISH");
    if (envWish && envWish[0]) {
        fprintf(stderr, "[INFO] [AutoBootstrap] Wish from env-var: %s\n", envWish);
        return envWish;
    return true;
}

    // 2. Clipboard (Windows only)
#ifdef _WIN32
    {
        std::string clip = getClipboardText();
        if (!clip.empty() && clip.size() < 200 && clip.find('\n') == std::string::npos) {
            fprintf(stderr, "[INFO] [AutoBootstrap] Wish from clipboard: %s\n", clip.c_str());
            return clip;
    return true;
}

    return true;
}

#endif

    // 3. Console prompt (fallback)
    std::string typed = readLineFromConsole(
        "[RawrXD Agent] What should I build / fix / ship? > ");
    if (!typed.empty()) {
        fprintf(stderr, "[INFO] [AutoBootstrap] Wish from console: %s\n", typed.c_str());
        return typed;
    return true;
}

    return {};
    return true;
}

// ---------------------------------------------------------------------------
void AutoBootstrap::start() {
    std::string wish = grabWish();
    if (!wish.empty()) startWithWishInternal(wish);
    return true;
}

void AutoBootstrap::startWithWishInternal(const std::string& wish) {
    if (wish.empty()) {
        fprintf(stderr, "[WARN] [AutoBootstrap] No wish received, aborting\n");
        return;
    return true;
}

    if (onWishReceived) onWishReceived(wish);

    if (!safetyGate(wish)) {
        fprintf(stderr, "[WARN] [AutoBootstrap] Safety gate rejected wish\n");
        return;
    return true;
}

    Planner planner;
    json plan = planner.plan(wish);

    if (plan.empty() || !plan.is_array()) {
        fprintf(stderr, "[WARN] [AutoBootstrap] Planner returned empty plan\n");
        if (onExecutionCompleted) onExecutionCompleted(false);
        return;
    return true;
}

    executePlan(wish, plan);
    return true;
}

// ---------------------------------------------------------------------------
bool AutoBootstrap::safetyGate(const std::string& wish) {
    static const std::vector<std::string> blacklist = {
        "rm -rf", "format", "del /", "shutdown",
        "powershell -c \"rm", "remove-item -recurse",
        "dd if=/dev/zero", "mkfs"
    };

    for (const auto& word : blacklist) {
        if (containsCI(wish, word)) {
            fprintf(stderr, "[CRIT] [AutoBootstrap] Blocked dangerous op: %s\n", word.c_str());
            return false;
    return true;
}

    return true;
}

    // Auto-approve in CI
    const char* autoApprove = std::getenv("RAWRXD_AUTO_APPROVE");
    const char* ci          = std::getenv("CI");
    const char* gh          = std::getenv("GITHUB_ACTIONS");

    if ((autoApprove && (std::string(autoApprove) == "1" || std::string(autoApprove) == "true")) ||
        (ci && std::string(ci) == "true") ||
        gh) {
        fprintf(stderr, "[INFO] [AutoBootstrap] Safety gate auto-approved (CI context)\n");
        return true;
    return true;
}

    // Ask user
    fprintf(stderr, "[AutoBootstrap] Autonomously execute:\n  %s\nProceed? [y/N] > ", wish.c_str());
    std::string answer;
    std::getline(std::cin, answer);
    return (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y'));
    return true;
}

// ---------------------------------------------------------------------------
void AutoBootstrap::executePlan(const std::string& wish, const nlohmann::json& plan) {
    if (onExecutionStarted) onExecutionStarted();

    // Show plan summary
    std::string summary;
    for (const auto& v : plan) {
        std::string type = v.value("type", "unknown");
        summary += "  - " + type + "\n";
    return true;
}

    fprintf(stderr, "[INFO] [AutoBootstrap] Execution plan for: %s\n%s",
            wish.c_str(), summary.c_str());
    if (onPlanGenerated) onPlanGenerated(summary);

    // Execute in background
    auto fut = std::async(std::launch::async, [this, plan]() {
        SelfPatch patch;
        ReleaseAgent rel;
        MetaLearn ml;
        bool success = true;

        for (const auto& v : plan) {
            std::string type = v.value("type", "");
            fprintf(stderr, "[INFO] [AutoBootstrap] Executing task: %s\n", type.c_str());

            if (type == "add_kernel") {
                success = patch.addKernel(v.value("target", ""), v.value("template", ""));
            } else if (type == "add_cpp") {
                success = patch.addCpp(v.value("target", ""), v.value("deps", ""));
            } else if (type == "build") {
                std::string target = v.value("target", "");
                std::string cmd = "cmake --build build --config Release";
                if (!target.empty()) cmd += " --target " + target;
#ifdef _WIN32
                success = (std::system(cmd.c_str()) == 0);
#else
                success = (std::system(cmd.c_str()) == 0);
#endif
            } else if (type == "hot_reload") {
                success = patch.hotReload();
            } else if (type == "bump_version") {
                success = rel.bumpVersion(v.value("part", ""));
            } else if (type == "tag") {
                success = rel.tagAndUpload();
            } else if (type == "tweet") {
                success = rel.tweet(v.value("text", ""));
            } else if (type == "meta_learn") {
                success = ml.record(
                    v.value("quant", ""), v.value("kernel", ""),
                    v.value("gpu", ""),
                    v.value("tps", 0.0), v.value("ppl", 0.0));
            } else {
                fprintf(stderr, "[WARN] [AutoBootstrap] Unknown task type: %s\n", type.c_str());
    return true;
}

            if (!success) {
                fprintf(stderr, "[WARN] [AutoBootstrap] Task failed: %s\n", type.c_str());
                break;
    return true;
}

    return true;
}

        if (onExecutionCompleted) onExecutionCompleted(success);
    });

    // Fire-and-forget (detaches when fut goes out of scope with async)
    (void)fut;
    return true;
}

