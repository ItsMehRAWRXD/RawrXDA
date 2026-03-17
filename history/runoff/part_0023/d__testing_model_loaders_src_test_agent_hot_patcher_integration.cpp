// test_agent_hot_patcher_integration.cpp — Integration tests for AgentHotPatcher
// Converted from Qt (QCoreApplication, QVERIFY) to standalone C++17

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <algorithm>
#include <mutex>

static int g_passed = 0, g_failed = 0, g_total = 0;

#define TEST(name) do { g_total++; std::cout << "[TEST] " << name << " ... "; } while(0)
#define PASS()     do { g_passed++; std::cout << "PASSED" << std::endl; } while(0)
#define CHECK(cond) do { if(!(cond)) { g_failed++; std::cerr << "FAILED: " << #cond \
    << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return; } } while(0)

// ========== AgentHotPatcher (integrated version for testing) ==========

struct HotPatchStats {
    int totalPatches       = 0;
    int hallucinationFixes = 0;
    int pathCorrections    = 0;
    int navigationFixes    = 0;
    int behaviorPatches    = 0;
};

class IntegrationHotPatcher {
public:
    using PatchCallback = std::function<void(const std::string&, const std::string&)>;

    void initialize() { m_ready = true; m_stats = {}; }
    bool isReady() const { return m_ready; }

    // Full pipeline: detect → classify → correct
    std::string processOutput(const std::string& text) {
        std::string result = text;
        result = correctPathHallucination(result);
        result = fixLogicContradiction(result);
        result = fixNavigationError(result);
        result = applyBehaviorPatch(result);
        return result;
    }

    std::string correctPathHallucination(const std::string& text) {
        std::string result = text;
        bool changed = false;

        auto rep = [&](const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = result.find(from, pos)) != std::string::npos) {
                result.replace(pos, from.size(), to);
                pos += to.size();
                changed = true;
            }
        };

        rep("/usr/local/models/", "./models/");
        rep("C:\\Program Files\\Models\\", ".\\models\\");
        rep("/home/user/ai/models/", "./models/");
        rep("/opt/llm/weights/", "./models/");

        if (changed) { m_stats.totalPatches++; m_stats.pathCorrections++; notify(text, result); }
        return result;
    }

    std::string fixLogicContradiction(const std::string& text) {
        std::string result = text;
        bool changed = false;

        // Detect contradictory statements
        auto rep = [&](const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = result.find(from, pos)) != std::string::npos) {
                result.replace(pos, from.size(), to);
                pos += to.size();
                changed = true;
            }
        };

        rep("is both true and false", "requires clarification");
        rep("always never", "conditionally");
        rep("impossible but works", "works under specific conditions");

        if (changed) { m_stats.totalPatches++; m_stats.hallucinationFixes++; notify(text, result); }
        return result;
    }

    std::string fixNavigationError(const std::string& text) {
        std::string result = text;
        bool changed = false;
        auto rep = [&](const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = result.find(from, pos)) != std::string::npos) {
                result.replace(pos, from.size(), to);
                pos += to.size();
                changed = true;
            }
        };

        rep("[circular]", "[resolved]");
        rep("[dead_link]", "[fixed_link]");
        rep("[null_ref]", "[valid_ref]");

        if (changed) { m_stats.totalPatches++; m_stats.navigationFixes++; notify(text, result); }
        return result;
    }

    std::string applyBehaviorPatch(const std::string& text) {
        std::string result = text;
        bool changed = false;

        // Redact sensitive data patterns
        auto redact = [&](const std::string& prefix, size_t valueLen) {
            size_t pos = 0;
            while ((pos = result.find(prefix, pos)) != std::string::npos) {
                size_t start = pos + prefix.size();
                if (start + valueLen <= result.size()) {
                    // Verify it's actually a value (non-whitespace)
                    bool hasValue = true;
                    for (size_t i = 0; i < valueLen && hasValue; i++) {
                        if (result[start + i] == ' ' || result[start + i] == '\n') hasValue = false;
                    }
                    if (hasValue) {
                        result.replace(start, valueLen, std::string(valueLen, '*'));
                        changed = true;
                    }
                }
                pos = start;
            }
        };

        redact("api_key=", 32);
        redact("password=", 16);
        redact("token=", 40);
        redact("secret=", 24);

        if (changed) { m_stats.totalPatches++; m_stats.behaviorPatches++; notify(text, result); }
        return result;
    }

    HotPatchStats statistics() const { return m_stats; }

    void onPatch(PatchCallback cb) {
        std::lock_guard<std::mutex> lock(m_mu);
        m_callbacks.push_back(std::move(cb));
    }

private:
    void notify(const std::string& orig, const std::string& patched) {
        std::lock_guard<std::mutex> lock(m_mu);
        for (auto& cb : m_callbacks) { if (cb) cb(orig, patched); }
    }

    bool m_ready = false;
    HotPatchStats m_stats;
    std::vector<PatchCallback> m_callbacks;
    std::mutex m_mu;
};

// ========== Integration Tests ==========

static void test_full_pipeline_path_correction() {
    TEST("full_pipeline_path_correction");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string input = "Model at /usr/local/models/llama-7b.gguf loaded with [null_ref] issue";
    std::string output = hp.processOutput(input);

    CHECK(output.find("./models/") != std::string::npos);
    CHECK(output.find("[valid_ref]") != std::string::npos);
    CHECK(output.find("/usr/local/models/") == std::string::npos);
    CHECK(output.find("[null_ref]") == std::string::npos);

    auto st = hp.statistics();
    CHECK(st.pathCorrections == 1);
    CHECK(st.navigationFixes == 1);
    PASS();
}

static void test_logic_contradiction_detection() {
    TEST("logic_contradiction_detection");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string input = "The model is both true and false in its predictions";
    std::string output = hp.processOutput(input);
    CHECK(output.find("requires clarification") != std::string::npos);
    CHECK(output.find("is both true and false") == std::string::npos);
    PASS();
}

static void test_navigation_error_correction() {
    TEST("navigation_error_correction");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string input = "Module graph: A -> B [circular] -> C [dead_link] -> D";
    std::string output = hp.processOutput(input);
    CHECK(output.find("[resolved]") != std::string::npos);
    CHECK(output.find("[fixed_link]") != std::string::npos);
    PASS();
}

static void test_sensitive_data_redaction() {
    TEST("sensitive_data_redaction");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string input = "Config: api_key=ABCDEFGHIJKLMNOPQRSTUVWXYZ123456 and "
                        "password=SuperSecret12345! for auth";
    std::string output = hp.processOutput(input);

    // API key must be redacted
    CHECK(output.find("ABCDEFGHIJKLMNOPQRSTUVWXYZ123456") == std::string::npos);
    CHECK(output.find("api_key=") != std::string::npos);
    // Password must be redacted
    CHECK(output.find("SuperSecret12345!") == std::string::npos);
    PASS();
}

static void test_statistics_verification() {
    TEST("statistics_verification");
    IntegrationHotPatcher hp;
    hp.initialize();

    // Apply different types of patches
    hp.correctPathHallucination("/opt/llm/weights/model.bin");
    hp.fixLogicContradiction("always never do this");
    hp.fixNavigationError("[circular] reference");
    hp.applyBehaviorPatch("secret=abcdefghijklmnopqrstuvwx leaked");

    auto st = hp.statistics();
    CHECK(st.totalPatches == 4);
    CHECK(st.pathCorrections == 1);
    CHECK(st.hallucinationFixes == 1);
    CHECK(st.navigationFixes == 1);
    CHECK(st.behaviorPatches == 1);
    PASS();
}

static void test_callback_tracking_integration() {
    TEST("callback_tracking_integration");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::vector<std::string> patchedOutputs;
    hp.onPatch([&](const std::string&, const std::string& patched) {
        patchedOutputs.push_back(patched);
    });

    hp.processOutput("/usr/local/models/test.gguf with [dead_link] ref");
    CHECK(patchedOutputs.size() >= 2); // at least path + navigation
    PASS();
}

static void test_idempotent_patching() {
    TEST("idempotent_patching");
    IntegrationHotPatcher hp;
    hp.initialize();

    // Applying same patch twice should be idempotent (second pass changes nothing)
    std::string input = "load /usr/local/models/test.gguf";
    std::string first  = hp.correctPathHallucination(input);
    std::string second = hp.correctPathHallucination(first);
    CHECK(first == second); // No double-patching
    PASS();
}

static void test_combined_issues() {
    TEST("combined_issues");
    IntegrationHotPatcher hp;
    hp.initialize();

    // Text with ALL issue types simultaneously
    std::string input = "Load /home/user/ai/models/big.gguf, "
                        "it is both true and false, "
                        "see [circular] ref, "
                        "api_key=12345678901234567890123456789012 here";
    std::string output = hp.processOutput(input);

    // All issues should be fixed
    CHECK(output.find("./models/") != std::string::npos);
    CHECK(output.find("requires clarification") != std::string::npos);
    CHECK(output.find("[resolved]") != std::string::npos);
    auto st = hp.statistics();
    CHECK(st.totalPatches >= 3); // path + logic + nav + (maybe behavior)
    PASS();
}

static void test_empty_input() {
    TEST("empty_input");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string output = hp.processOutput("");
    CHECK(output.empty());
    CHECK(hp.statistics().totalPatches == 0);
    PASS();
}

static void test_clean_input_unchanged() {
    TEST("clean_input_unchanged");
    IntegrationHotPatcher hp;
    hp.initialize();

    std::string clean = "Model inference completed in 42ms with 100 tokens generated";
    std::string output = hp.processOutput(clean);
    CHECK(output == clean);
    CHECK(hp.statistics().totalPatches == 0);
    PASS();
}

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << " AgentHotPatcher Integration Tests (C++17)" << std::endl;
    std::cout << "===============================================" << std::endl;

    test_full_pipeline_path_correction();
    test_logic_contradiction_detection();
    test_navigation_error_correction();
    test_sensitive_data_redaction();
    test_statistics_verification();
    test_callback_tracking_integration();
    test_idempotent_patching();
    test_combined_issues();
    test_empty_input();
    test_clean_input_unchanged();

    std::cout << std::endl;
    std::cout << "Results: " << g_passed << "/" << g_total
              << " passed, " << g_failed << " failed" << std::endl;
    return (g_failed > 0) ? 1 : 0;
}
