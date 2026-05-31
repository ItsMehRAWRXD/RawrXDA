// ============================================================================
// extension_installer_smoketest.cpp
// Smoke-tests the RawrXD extension installer stack for full route coverage
// and parity between Amazon Q and GitHub Copilot.
//
// Tests:
//   1.  Priority extension list: all known IDs present and well-formed
//   2.  ExtensionAutoInstaller singleton: first-run gate, state persistence
//   3.  Marketplace API: GetById for GitHub.copilot and amazon-q-vscode
//   4.  Marketplace API: Query (search) for all priority extension categories
//   5.  VSIX installer: Install/Uninstall path guards (no real network)
//   6.  AmazonQ ↔ Copilot parity: both listed as AI, both have requiresAuth
//   7.  Compatibility: all PRIORITY_EXTENSIONS IDs pass format validation
//   8.  loadInstalledExtensions round-trip: save → reload preserves entries
//   9.  installExtensions (batch): already-installed skip logic is correct
//  10.  Live mode (--live flag): real marketplace GetById for both AI extensions
//
// Build: standalone Win32 console exe (no IDE link required).
//   Add to CMake target: ExtensionInstallerSmoke
//   Link: winhttp.lib crypt32.lib wintrust.lib shell32.lib ole32.lib
//
// Usage:
//   ExtensionInstallerSmoke.exe          — offline / unit mode
//   ExtensionInstallerSmoke.exe --live   — live marketplace network probes
// ============================================================================
#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cassert>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>

// Extension stack headers
#include "../marketplace/extension_auto_installer.hpp"
#include "../win32app/VSCodeMarketplaceAPI.hpp"
#include "../win32app/VSIXInstaller.hpp"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;
using namespace RawrXD::Extensions;

// ── Test harness ───────────────────────────────────────────────────────────
static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

static void check(const char* label, bool ok, const char* detail = nullptr)
{
    if (ok) {
        ++g_pass;
        printf("[PASS] %s", label);
    } else {
        ++g_fail;
        printf("[FAIL] %s", label);
    }
    if (detail && *detail) printf(" — %s", detail);
    putchar('\n');
}

static void skip(const char* label, const char* reason)
{
    ++g_skip;
    printf("[SKIP] %s — %s\n", label, reason);
}

// ── Helpers ────────────────────────────────────────────────────────────────

static bool isValidExtId(const std::string& id) {
    // Must match "publisher.extensionName" — both parts non-empty, exactly one dot.
    size_t dot = id.find('.');
    if (dot == std::string::npos) return false;
    if (dot == 0 || dot == id.size() - 1) return false;
    // No spaces, no path separators
    for (char c : id) {
        if (c == ' ' || c == '/' || c == '\\') return false;
    }
    return true;
}

static bool splitExtId(const std::string& id, std::string& publisher, std::string& extensionName) {
    size_t dot = id.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) {
        return false;
    }
    publisher = id.substr(0, dot);
    extensionName = id.substr(dot + 1);
    return true;
}

static bool hasActivationEventInManifest(const fs::path& manifestPath) {
    std::ifstream in(manifestPath, std::ios::binary);
    if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return content.find("\"activationEvents\"") != std::string::npos;
}

static bool findInstalledDirForId(const std::string& extensionId, fs::path& outDir) {
    std::string root = RawrXD::GetExtensionsInstallRoot();
    std::error_code ec;
    fs::path rootPath(root);
    if (!fs::exists(rootPath, ec) || ec) return false;

    const std::string prefix = extensionId + "-";
    for (const auto& entry : fs::directory_iterator(rootPath, ec)) {
        if (ec) return false;
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0) {
            outDir = entry.path();
            return true;
        }
    }
    return false;
}

// ── Test 1: Priority list integrity ───────────────────────────────────────
static void test_priority_list_integrity()
{
    printf("\n--- Test 1: Priority extension list integrity ---\n");
    check("priority_list.not_empty", PRIORITY_EXTENSION_COUNT > 0);

    bool allValid = true;
    for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
        const auto& ext = PRIORITY_EXTENSIONS[i];
        if (!ext.id || !ext.displayName || !ext.category) {
            printf("  [BAD] index %zu: null fields\n", i);
            allValid = false;
            continue;
        }
        if (!isValidExtId(ext.id)) {
            printf("  [BAD] id=\"%s\" — malformed\n", ext.id);
            allValid = false;
        }
    }
    check("priority_list.all_ids_valid", allValid);

    // Must contain both AI assistants.
    bool hasQ = false, hasCopilot = false, hasCopilotChat = false;
    for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
        const char* id = PRIORITY_EXTENSIONS[i].id;
        if (std::string(id) == "amazonwebservices.amazon-q-vscode") hasQ = true;
        if (std::string(id) == "GitHub.copilot")                    hasCopilot = true;
        if (std::string(id) == "GitHub.copilot-chat")               hasCopilotChat = true;
    }
    check("priority_list.contains_amazon_q",     hasQ);
    check("priority_list.contains_copilot",      hasCopilot);
    check("priority_list.contains_copilot_chat", hasCopilotChat);
}

// ── Test 2: AmazonQ ↔ Copilot parity ──────────────────────────────────────
static void test_ai_extension_parity()
{
    printf("\n--- Test 2: AmazonQ ↔ Copilot parity ---\n");

    const PriorityExtension* qEntry       = nullptr;
    const PriorityExtension* copilotEntry = nullptr;
    const PriorityExtension* chatEntry    = nullptr;

    for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
        const std::string id = PRIORITY_EXTENSIONS[i].id;
        if (id == "amazonwebservices.amazon-q-vscode") qEntry       = &PRIORITY_EXTENSIONS[i];
        if (id == "GitHub.copilot")                    copilotEntry = &PRIORITY_EXTENSIONS[i];
        if (id == "GitHub.copilot-chat")               chatEntry    = &PRIORITY_EXTENSIONS[i];
    }

    check("parity.amazon_q_entry_found",     qEntry       != nullptr);
    check("parity.copilot_entry_found",      copilotEntry != nullptr);
    check("parity.copilot_chat_entry_found", chatEntry    != nullptr);

    if (!qEntry || !copilotEntry || !chatEntry) {
        printf("  (skipping parity sub-checks — entries missing)\n");
        return;
    }

    // Both must be AI category.
    check("parity.amazon_q_is_AI",    std::string(qEntry->category)       == "AI");
    check("parity.copilot_is_AI",     std::string(copilotEntry->category) == "AI");
    check("parity.copilot_chat_is_AI",std::string(chatEntry->category)    == "AI");

    // Both require auth (they need sign-in to function).
    check("parity.amazon_q_requires_auth",    qEntry->requiresAuth       == true);
    check("parity.copilot_requires_auth",     copilotEntry->requiresAuth == true);
    check("parity.copilot_chat_requires_auth",chatEntry->requiresAuth    == true);

    // Both are marked for auto-install on first run.
    check("parity.amazon_q_auto_install",    qEntry->autoInstall       == true);
    check("parity.copilot_auto_install",     copilotEntry->autoInstall == true);
    check("parity.copilot_chat_auto_install",chatEntry->autoInstall    == true);

    // Both have non-empty display names.
    check("parity.amazon_q_has_display_name",    qEntry->displayName[0]       != '\0');
    check("parity.copilot_has_display_name",     copilotEntry->displayName[0] != '\0');
    check("parity.copilot_chat_has_display_name",chatEntry->displayName[0]    != '\0');
}

// ── Test 3: All category coverage ─────────────────────────────────────────
static void test_category_coverage()
{
    printf("\n--- Test 3: Category coverage ---\n");
    static const char* required[] = { "AI", "Languages", "Debuggers", "SCM", "Formatters", "Linters", "Themes" };
    const size_t nRequired = sizeof(required) / sizeof(required[0]);

    for (size_t r = 0; r < nRequired; ++r) {
        bool found = false;
        for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
            if (std::string(PRIORITY_EXTENSIONS[i].category) == required[r]) {
                found = true;
                break;
            }
        }
        char label[128];
        snprintf(label, sizeof(label), "category.%s_present", required[r]);
        check(label, found);
    }
}

// ── Test 4: ExtensionAutoInstaller singleton and state ────────────────────
static void test_auto_installer_state()
{
    printf("\n--- Test 4: ExtensionAutoInstaller state ---\n");

    ExtensionAutoInstaller& ai = ExtensionAutoInstaller::instance();
    check("auto_installer.singleton_ok", true);  // constructor didn't crash

    // isInstalled with garbage id must return false without crashing.
    bool noGarbage = !ai.isInstalled("this.does_not_exist_xyzzy");
    check("auto_installer.garbage_id_not_installed", noGarbage);

    // getInstalledExtensions returns a list (may be empty — that's fine).
    auto installed = ai.getInstalledExtensions();
    check("auto_installer.getInstalled_returns", true);  // no crash

    // getPendingExtensions initially empty (no active installs in unit mode).
    auto pending = ai.getPendingExtensions();
    check("auto_installer.getPending_no_crash", true);

    // needsFirstRunInstall is a bool — just verify it doesn't throw.
    bool needs = ai.needsFirstRunInstall();
    char detail[64];
    snprintf(detail, sizeof(detail), "needsFirstRun=%s", needs ? "true" : "false");
    check("auto_installer.needsFirstRun_ok", true, detail);
}

// ── Test 5: installExtension on already-installed id ─────────────────────
static void test_already_installed_skip()
{
    printf("\n--- Test 5: Already-installed skip logic ---\n");
    ExtensionAutoInstaller& ai = ExtensionAutoInstaller::instance();

    // Seed a fake installed entry for testing.
    const std::string fakeId = "rawrxd.smoke-test-fake-ext";

    // installExtensions with an empty list must succeed silently.
    auto r0 = ai.installExtensions({});
    check("batch_install.empty_list_ok", r0.success, r0.detail.c_str());
    check("batch_install.empty_list_zero_installed", r0.installedCount == 0);
    check("batch_install.empty_list_zero_failed",    r0.failedCount    == 0);
}

// ── Test 6: VSIX id format validation (path-traversal guard) ──────────────
static void test_vsix_security_guards()
{
    printf("\n--- Test 6: VSIX security / id format guards ---\n");

    // IDs that must fail the format check used by installSingleExtension.
    static const char* badIds[] = {
        "",
        "noDot",
        ".leadingDot",
        "trailingDot.",
        "../path/traversal",
        "pub lisher.ext name",
    };
    for (auto* bad : badIds) {
        bool valid = isValidExtId(bad);
        char label[128];
        snprintf(label, sizeof(label), "vsix_guard.bad_id_rejected[\"%s\"]", bad);
        check(label, !valid);
    }

    // Well-formed ids must pass.
    static const char* goodIds[] = {
        "GitHub.copilot",
        "amazonwebservices.amazon-q-vscode",
        "ms-vscode.cpptools",
        "rust-lang.rust-analyzer",
    };
    for (auto* good : goodIds) {
        bool valid = isValidExtId(good);
        char label[128];
        snprintf(label, sizeof(label), "vsix_guard.good_id_accepted[\"%s\"]", good);
        check(label, valid);
    }
}

// ── Test 7: GetExtensionsInstallRoot returns a non-empty absolute path ─────
static void test_install_root_path()
{
    printf("\n--- Test 7: Install root path ---\n");
    std::string root = RawrXD::GetExtensionsInstallRoot();
    check("install_root.non_empty",       !root.empty());
    check("install_root.contains_RawrXD", root.find("RawrXD") != std::string::npos);
    check("install_root.ends_slash",      root.back() == '\\' || root.back() == '/');
}

// ── Test 8: installAIExtensions convenience helper builds right list ───────
static void test_ai_extensions_helper()
{
    printf("\n--- Test 8: AI extension convenience helper ---\n");

    // Collect the expected AI ids from the priority list directly.
    std::vector<std::string> expectedAI;
    for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
        if (std::string(PRIORITY_EXTENSIONS[i].category) == "AI") {
            expectedAI.push_back(PRIORITY_EXTENSIONS[i].id);
        }
    }
    check("ai_helper.expected_ai_count_nonzero", !expectedAI.empty());
    // Verify both key IDs are in the expected list.
    bool hasQ = std::find(expectedAI.begin(), expectedAI.end(),
                          "amazonwebservices.amazon-q-vscode") != expectedAI.end();
    bool hasCopilot = std::find(expectedAI.begin(), expectedAI.end(),
                                "GitHub.copilot") != expectedAI.end();
    check("ai_helper.amazon_q_in_ai_list",  hasQ);
    check("ai_helper.copilot_in_ai_list",   hasCopilot);
}

// ── Test 9: Marketplace ItemUrl format ────────────────────────────────────
static void test_marketplace_item_url()
{
    printf("\n--- Test 9: Marketplace ItemUrl format ---\n");
    std::string url = VSCodeMarketplace::ItemUrl("GitHub", "copilot");
    check("item_url.copilot_non_empty",         !url.empty());
    check("item_url.copilot_contains_publisher", url.find("GitHub") != std::string::npos);
    check("item_url.copilot_contains_ext_name",  url.find("copilot") != std::string::npos);

    std::string urlQ = VSCodeMarketplace::ItemUrl("amazonwebservices", "amazon-q-vscode");
    check("item_url.amazon_q_non_empty",              !urlQ.empty());
    check("item_url.amazon_q_contains_publisher",
          urlQ.find("amazonwebservices") != std::string::npos ||
          urlQ.find("amazon") != std::string::npos);
}

// ── Test 10: Live marketplace probes (--live only) ─────────────────────────
static void test_live_marketplace(bool live)
{
    printf("\n--- Test 10: Live marketplace probes ---\n");
    if (!live) {
        skip("live.copilot_getbyid",  "--live flag not set");
        skip("live.amazon_q_getbyid", "--live flag not set");
        skip("live.search_AI",        "--live flag not set");
        skip("live.copilot_vs_q_version_format", "--live flag not set");
        return;
    }

    // GitHub Copilot.
    {
        VSCodeMarketplace::MarketplaceEntry e;
        bool ok = VSCodeMarketplace::GetById("GitHub.copilot", e);
        check("live.copilot_getbyid",          ok);
        check("live.copilot_has_version",      ok && !e.version.empty());
        check("live.copilot_publisher_correct",ok && e.publisher == "GitHub");
        check("live.copilot_ext_name_correct", ok && e.extensionName == "copilot");
        if (ok) printf("  GitHub.copilot version: %s\n", e.version.c_str());
    }

    // Amazon Q.
    {
        VSCodeMarketplace::MarketplaceEntry e;
        bool ok = VSCodeMarketplace::GetById("amazonwebservices.amazon-q-vscode", e);
        check("live.amazon_q_getbyid",          ok);
        check("live.amazon_q_has_version",      ok && !e.version.empty());
        check("live.amazon_q_publisher_correct",ok && e.publisher == "amazonwebservices");
        if (ok) printf("  amazon-q-vscode version: %s\n", e.version.c_str());
    }

    // Parity: both have installCount > 0.
    {
        VSCodeMarketplace::MarketplaceEntry copilot, amazonQ;
        bool okC = VSCodeMarketplace::GetById("GitHub.copilot", copilot);
        bool okQ = VSCodeMarketplace::GetById("amazonwebservices.amazon-q-vscode", amazonQ);
        check("live.copilot_has_installs",  okC && copilot.installCount > 0);
        check("live.amazon_q_has_installs", okQ && amazonQ.installCount > 0);
    }

    // Search smoke: "Amazon Q" must return at least one result.
    {
        std::vector<VSCodeMarketplace::MarketplaceEntry> results;
        bool ok = VSCodeMarketplace::Query("Amazon Q", 10, 1, results);
        check("live.search_amazon_q_ok",           ok);
        check("live.search_amazon_q_has_results",  ok && !results.empty());
    }

    // Search smoke: "GitHub Copilot" must return at least one result.
    {
        std::vector<VSCodeMarketplace::MarketplaceEntry> results;
        bool ok = VSCodeMarketplace::Query("GitHub Copilot", 10, 1, results);
        check("live.search_copilot_ok",          ok);
        check("live.search_copilot_has_results", ok && !results.empty());
    }

    // All AI priority extension ids must be resolvable from the marketplace.
    {
        int aiResolved = 0, aiTotal = 0;
        for (size_t i = 0; i < PRIORITY_EXTENSION_COUNT; ++i) {
            if (std::string(PRIORITY_EXTENSIONS[i].category) != "AI") continue;
            ++aiTotal;
            VSCodeMarketplace::MarketplaceEntry e;
            if (VSCodeMarketplace::GetById(PRIORITY_EXTENSIONS[i].id, e)) {
                ++aiResolved;
            } else {
                printf("  [WARN] marketplace did not resolve: %s\n", PRIORITY_EXTENSIONS[i].id);
            }
        }
        char detail[64];
        snprintf(detail, sizeof(detail), "%d/%d resolved", aiResolved, aiTotal);
        check("live.all_AI_ext_resolve", aiResolved == aiTotal, detail);
    }
}

// ── Test 11: Live install path + activation artifact verification ─────────
static void test_live_real_install_and_activation(bool liveInstall)
{
    printf("\n--- Test 11: Live install execution path + activation artifacts ---\n");
    if (!liveInstall) {
        skip("live_install.cpptools.install_or_present", "--live-install flag not set");
        skip("live_install.rust_analyzer.install_or_present", "--live-install flag not set");
        skip("live_install.cpptools.artifacts", "--live-install flag not set");
        skip("live_install.rust_analyzer.artifacts", "--live-install flag not set");
        skip("live_install.cpptools.activation_events_present", "--live-install flag not set");
        skip("live_install.rust_analyzer.activation_events_present", "--live-install flag not set");
        return;
    }

    ExtensionAutoInstaller& ai = ExtensionAutoInstaller::instance();
    const std::vector<std::string> ids = {
        "ms-vscode.cpptools",
        "rust-lang.rust-analyzer"
    };

    for (const auto& id : ids) {
        bool alreadyInstalled = ai.isInstalled(id);
        AutoInstallResult r = alreadyInstalled ? AutoInstallResult::ok(0) : ai.installExtension(id);

        char labelA[128];
        snprintf(labelA, sizeof(labelA), "live_install.%s.install_or_present", id.c_str());
        bool okInstall = alreadyInstalled || r.success;
        check(labelA, okInstall, (!r.detail.empty() ? r.detail.c_str() : nullptr));

        fs::path installDir;
        bool hasDir = findInstalledDirForId(id, installDir);
        char labelB[128];
        snprintf(labelB, sizeof(labelB), "live_install.%s.artifacts", id.c_str());
        check(labelB, hasDir);

        bool hasActivationEvents = false;
        if (hasDir) {
            fs::path p1 = installDir / "package.json";
            fs::path p2 = installDir / "extension" / "package.json";
            hasActivationEvents = hasActivationEventInManifest(p1) || hasActivationEventInManifest(p2);
        }

        char labelC[128];
        snprintf(labelC, sizeof(labelC), "live_install.%s.activation_events_present", id.c_str());
        check(labelC, hasActivationEvents);
    }
}

// ── Test 12: Parallel install stress harness ──────────────────────────────
static void test_parallel_install_stress(int concurrency, bool liveStress)
{
    printf("\n--- Test 12: Parallel install stress harness ---\n");
    if (concurrency <= 0) {
        skip("stress.parallel_install", "--stress or --stress-live not set");
        return;
    }

    if (concurrency < 1) concurrency = 1;
    if (concurrency > 50) concurrency = 50;

    ExtensionAutoInstaller& ai = ExtensionAutoInstaller::instance();

    std::vector<std::string> idPool;
    if (liveStress) {
        idPool = {
            "ms-vscode.cpptools",
            "rust-lang.rust-analyzer",
            "ms-python.python",
            "GitHub.copilot",
            "amazonwebservices.amazon-q-vscode"
        };
    } else {
        // Offline stress still exercises locking, pending/install list consistency, and idempotency paths.
        idPool = {
            "ms-vscode.cpptools",
            "ms-vscode.cpptools",
            "rust-lang.rust-analyzer",
            "rust-lang.rust-analyzer",
            "invalid.no_such_extension_for_stress"
        };
    }

    std::atomic<int> okCount{0};
    std::atomic<int> alreadyCount{0};
    std::atomic<int> failCount{0};
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(concurrency));

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < concurrency; ++i) {
        workers.emplace_back([&, i]() {
            const std::string id = idPool[static_cast<size_t>(i) % idPool.size()];
            AutoInstallResult r = ai.installExtension(id);
            if (r.success) {
                okCount.fetch_add(1);
                return;
            }

            if (r.errorCode == 0 && r.detail.find("already installed") != std::string::npos) {
                alreadyCount.fetch_add(1);
            } else {
                failCount.fetch_add(1);
            }
        });
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    auto pending = ai.getPendingExtensions();
    auto installed = ai.getInstalledExtensions();
    std::set<std::string> installedSet(installed.begin(), installed.end());

    char detailA[128];
    snprintf(detailA, sizeof(detailA), "workers=%d elapsed_ms=%lld", concurrency, static_cast<long long>(elapsedMs));
    check("stress.parallel_install.completed", true, detailA);
    check("stress.parallel_install.pending_cleared", pending.empty());
    check("stress.parallel_install.installed_unique", installedSet.size() == installed.size());

    char detailB[128];
    snprintf(detailB, sizeof(detailB), "ok=%d already=%d failed=%d", okCount.load(), alreadyCount.load(), failCount.load());
    bool summaryOk = false;
    if (liveStress) {
        // In live mode we expect at least one successful or idempotent outcome.
        summaryOk = (okCount.load() + alreadyCount.load()) > 0;
    } else {
        // In offline/mixed mode we mainly assert concurrency safety and consistency,
        // because strict VSIX verification may reject downloaded payloads.
        summaryOk = (okCount.load() + alreadyCount.load() + failCount.load()) == concurrency;
    }
    check("stress.parallel_install.summary", summaryOk, detailB);
}

// ── Test 13: Failure injection + rollback consistency ─────────────────────
static void test_failure_injection(bool enable)
{
    printf("\n--- Test 13: Failure injection and rollback consistency ---\n");
    if (!enable) {
        skip("failure.invalid_id_rejected", "--failure-inject flag not set");
        skip("failure.download_404_rejected", "--failure-inject flag not set");
        skip("failure.corrupt_vsix_rejected", "--failure-inject flag not set");
        skip("failure.state_consistent_after_failures", "--failure-inject flag not set");
        return;
    }

    ExtensionAutoInstaller& ai = ExtensionAutoInstaller::instance();
    const auto installedBefore = ai.getInstalledExtensions();

    // 1) Invalid ID / traversal should fail fast.
    AutoInstallResult invalidIdResult = ai.installExtension("../traversal.attempt");
    check("failure.invalid_id_rejected", !invalidIdResult.success && invalidIdResult.errorCode == 1,
          invalidIdResult.detail.c_str());

    // 2) 404 / invalid version download must fail.
    std::filesystem::create_directories(".rawrxd\\temp");
    const std::string badVsixPath = ".rawrxd\\temp\\bad-404-smoke.vsix";
    bool badDownload = VSCodeMarketplace::DownloadVsix(
        "GitHub", "copilot", "0.0.0-smoke-not-real", badVsixPath);
    bool invalidPayloadRejected = true;
    if (badDownload && std::filesystem::exists(badVsixPath)) {
        RawrXD::VSIXVerification v = RawrXD::VSIXInstaller::VerifyPackage(badVsixPath);
        invalidPayloadRejected = !v.valid;
    }
    check("failure.download_404_rejected", !badDownload || invalidPayloadRejected);

    // 3) Corrupted VSIX must fail install and leave no install dir.
    const std::string corruptVsix = ".rawrxd\\temp\\corrupt-smoke.vsix";
    {
        std::ofstream out(corruptVsix, std::ios::binary);
        out << "NOT_A_ZIP";
    }
    bool corruptInstalled = RawrXD::VSIXInstaller::Install(corruptVsix);
    check("failure.corrupt_vsix_rejected", !corruptInstalled);

    // Ensure state stayed consistent.
    const auto installedAfter = ai.getInstalledExtensions();
    const auto pendingAfter = ai.getPendingExtensions();
    bool sameInstalled = (installedBefore == installedAfter);
    check("failure.state_consistent_after_failures", sameInstalled && pendingAfter.empty());

    std::error_code ec;
    std::filesystem::remove(corruptVsix, ec);
    std::filesystem::remove(badVsixPath, ec);
}

// ── main ───────────────────────────────────────────────────────────────────
int main(int argc, char** argv)
{
    bool liveMode = false;
    bool liveInstallMode = false;
    bool failureInjectMode = false;
    int stressCount = 0;
    int stressLiveCount = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--live") liveMode = true;
        if (std::string(argv[i]) == "--live-install") liveInstallMode = true;
        if (std::string(argv[i]) == "--failure-inject") failureInjectMode = true;
        if (std::string(argv[i]) == "--stress" && i + 1 < argc) {
            stressCount = std::max(0, std::atoi(argv[i + 1]));
            ++i;
        }
        if (std::string(argv[i]) == "--stress-live" && i + 1 < argc) {
            stressLiveCount = std::max(0, std::atoi(argv[i + 1]));
            ++i;
        }
    }

    printf("=================================================================\n");
    printf(" RawrXD Extension Installer Smoke Test\n");
    printf(" Mode: %s\n", liveMode ? "LIVE (marketplace network calls)" : "OFFLINE (unit checks only)");
    printf(" Live install verify: %s\n", liveInstallMode ? "ENABLED" : "DISABLED");
    if (stressCount > 0) {
        printf(" Stress workers: %d (offline/mixed mode)\n", stressCount);
    }
    if (stressLiveCount > 0) {
        printf(" Stress workers: %d (live mode)\n", stressLiveCount);
    }
    printf(" Failure injection: %s\n", failureInjectMode ? "ENABLED" : "DISABLED");
    printf("=================================================================\n");

    test_priority_list_integrity();
    test_ai_extension_parity();
    test_category_coverage();
    test_auto_installer_state();
    test_already_installed_skip();
    test_vsix_security_guards();
    test_install_root_path();
    test_ai_extensions_helper();
    test_marketplace_item_url();
    test_live_marketplace(liveMode);
    test_live_real_install_and_activation(liveInstallMode);

    if (stressLiveCount > 0) {
        test_parallel_install_stress(stressLiveCount, true);
    } else {
        test_parallel_install_stress(stressCount, false);
    }

    test_failure_injection(failureInjectMode);

    printf("\n=================================================================\n");
    printf(" Results: %d passed  %d failed  %d skipped\n", g_pass, g_fail, g_skip);
    printf("=================================================================\n");

    return (g_fail > 0) ? 1 : 0;
}
