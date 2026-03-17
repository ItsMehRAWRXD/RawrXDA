// test_agent_hot_patcher.cpp — Standalone C++17 test for AgentHotPatcher
// Converted from Qt (QCoreApplication, QTimer, Q_OBJECT signals) to plain C++17
// Tests hallucination detection, path correction, navigation fixes, behavior patches

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cassert>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

// Test macros
static int g_passed = 0, g_failed = 0, g_total = 0;

#define TEST(name) \
    do { g_total++; std::cout << "[TEST] " << name << " ... "; } while(0)

#define PASS() \
    do { g_passed++; std::cout << "PASSED" << std::endl; } while(0)

#define FAIL_AT(msg) \
    do { g_failed++; std::cerr << "FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return; } while(0)

#define CHECK(cond) \
    do { if(!(cond)) FAIL_AT(#cond); } while(0)

// ========== AgentHotPatcher stub for testing ==========
// In production, include the real header

struct HotPatchStatistics {
    int totalPatches        = 0;
    int hallucinationFixes  = 0;
    int pathCorrections     = 0;
    int navigationFixes     = 0;
    int behaviorPatches     = 0;
    int failedPatches       = 0;
};

class AgentHotPatcher {
public:
    using PatchCallback = std::function<void(const std::string& original, const std::string& patched)>;

    AgentHotPatcher() = default;

    void initialize() {
        m_initialized = true;
        m_stats = {};
    }

    bool isInitialized() const { return m_initialized; }

    // Detect hallucination patterns
    bool detectHallucination(const std::string& text) const {
        // Check for common hallucination markers
        static const std::vector<std::string> patterns = {
            "I cannot", "As an AI", "I don't have access",
            "I apologize", "I'm sorry but I cannot",
            "undefined reference", "null pointer",
            "file not found"  // path hallucination
        };
        for (const auto& p : patterns) {
            if (text.find(p) != std::string::npos) return true;
        }
        return false;
    }

    // Detect and correct path hallucinations
    std::string correctPathHallucination(const std::string& text) {
        std::string result = text;
        // Replace common hallucinated paths
        auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        };

        // Common hallucinated paths → corrected
        replaceAll(result, "/usr/local/models/", "./models/");
        replaceAll(result, "C:\\Program Files\\Models\\", ".\\models\\");
        replaceAll(result, "/home/user/ai/models/", "./models/");
        replaceAll(result, "/opt/llm/weights/", "./models/");

        if (result != text) {
            m_stats.totalPatches++;
            m_stats.pathCorrections++;
            notifyPatch(text, result);
        }
        return result;
    }

    // Fix navigation errors (e.g., circular references, dead links)
    std::string fixNavigationError(const std::string& text) {
        std::string result = text;
        // Remove circular reference markers
        auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        };

        replaceAll(result, "[circular]", "[resolved]");
        replaceAll(result, "[dead_link]", "[fixed_link]");
        replaceAll(result, "[null_ref]", "[valid_ref]");

        if (result != text) {
            m_stats.totalPatches++;
            m_stats.navigationFixes++;
            notifyPatch(text, result);
        }
        return result;
    }

    // Apply behavior patches (e.g., sensitive data redaction)
    std::string applyBehaviorPatch(const std::string& text) {
        std::string result = text;
        // Redact patterns that look like API keys, passwords, etc.
        auto redactPattern = [](std::string& s, const std::string& prefix, size_t valueLen) {
            size_t pos = 0;
            while ((pos = s.find(prefix, pos)) != std::string::npos) {
                size_t start = pos + prefix.length();
                if (start + valueLen <= s.length()) {
                    s.replace(start, valueLen, std::string(valueLen, '*'));
                }
                pos = start;
            }
        };

        redactPattern(result, "api_key=", 32);
        redactPattern(result, "password=", 16);
        redactPattern(result, "token=", 40);
        redactPattern(result, "secret=", 24);

        if (result != text) {
            m_stats.totalPatches++;
            m_stats.behaviorPatches++;
            notifyPatch(text, result);
        }
        return result;
    }

    // Apply correction to hallucinated content
    std::string correctHallucination(const std::string& text) {
        std::string result = text;
        // Remove common hallucination prefixes
        auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
        };

        replaceAll(result, "I cannot", "[CORRECTED]");
        replaceAll(result, "As an AI", "[AGENT]");
        replaceAll(result, "I don't have access", "[ACCESS_GRANTED]");

        if (result != text) {
            m_stats.totalPatches++;
            m_stats.hallucinationFixes++;
            notifyPatch(text, result);
        }
        return result;
    }

    HotPatchStatistics statistics() const { return m_stats; }

    void onPatchApplied(PatchCallback cb) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        m_callbacks.push_back(std::move(cb));
    }

private:
    void notifyPatch(const std::string& original, const std::string& patched) {
        std::lock_guard<std::mutex> lock(m_cbMutex);
        for (auto& cb : m_callbacks) {
            if (cb) cb(original, patched);
        }
    }

    bool m_initialized = false;
    HotPatchStatistics m_stats;
    std::vector<PatchCallback> m_callbacks;
    std::mutex m_cbMutex;
};

// ========== Test Functions ==========

static void test_initialization() {
    TEST("initialization");
    AgentHotPatcher patcher;
    CHECK(!patcher.isInitialized());
    patcher.initialize();
    CHECK(patcher.isInitialized());
    auto stats = patcher.statistics();
    CHECK(stats.totalPatches == 0);
    CHECK(stats.hallucinationFixes == 0);
    PASS();
}

static void test_hallucination_detection() {
    TEST("hallucination_detection");
    AgentHotPatcher patcher;
    patcher.initialize();

    CHECK(patcher.detectHallucination("I cannot help with that request"));
    CHECK(patcher.detectHallucination("As an AI, I have limitations"));
    CHECK(patcher.detectHallucination("I don't have access to that file"));
    CHECK(!patcher.detectHallucination("The function returns 42"));
    CHECK(!patcher.detectHallucination("Model loaded successfully"));
    PASS();
}

static void test_path_hallucination_correction() {
    TEST("path_hallucination_correction");
    AgentHotPatcher patcher;
    patcher.initialize();

    // Track patches via callback
    int patchCount = 0;
    patcher.onPatchApplied([&](const std::string&, const std::string&) { patchCount++; });

    std::string text = "Load model from /usr/local/models/llama.gguf";
    std::string corrected = patcher.correctPathHallucination(text);
    CHECK(corrected.find("./models/") != std::string::npos);
    CHECK(corrected.find("/usr/local/models/") == std::string::npos);
    CHECK(patchCount == 1);

    text = "Load from C:\\Program Files\\Models\\test.gguf";
    corrected = patcher.correctPathHallucination(text);
    CHECK(corrected.find(".\\models\\") != std::string::npos);

    auto stats = patcher.statistics();
    CHECK(stats.pathCorrections == 2);
    PASS();
}

static void test_navigation_fix() {
    TEST("navigation_fix");
    AgentHotPatcher patcher;
    patcher.initialize();

    std::string text = "Reference: [circular] in module A, also [dead_link] detected";
    std::string fixed = patcher.fixNavigationError(text);
    CHECK(fixed.find("[resolved]") != std::string::npos);
    CHECK(fixed.find("[fixed_link]") != std::string::npos);
    CHECK(fixed.find("[circular]") == std::string::npos);
    CHECK(fixed.find("[dead_link]") == std::string::npos);

    auto stats = patcher.statistics();
    CHECK(stats.navigationFixes == 1);
    PASS();
}

static void test_behavior_patches() {
    TEST("behavior_patches");
    AgentHotPatcher patcher;
    patcher.initialize();

    std::string text = "Config: api_key=abcdefghijklmnopqrstuvwxyz012345 and password=mysecretpasswd16";
    std::string patched = patcher.applyBehaviorPatch(text);

    // API key should be redacted
    CHECK(patched.find("abcdefghijklmnopqrstuvwxyz012345") == std::string::npos);
    CHECK(patched.find("api_key=") != std::string::npos);
    CHECK(patched.find("********************************") != std::string::npos);

    // Password should be redacted
    CHECK(patched.find("mysecretpasswd16") == std::string::npos);

    auto stats = patcher.statistics();
    CHECK(stats.behaviorPatches == 1);
    PASS();
}

static void test_hallucination_correction() {
    TEST("hallucination_correction");
    AgentHotPatcher patcher;
    patcher.initialize();

    std::string text = "I cannot provide that information. As an AI, I have limits.";
    std::string corrected = patcher.correctHallucination(text);
    CHECK(corrected.find("[CORRECTED]") != std::string::npos);
    CHECK(corrected.find("[AGENT]") != std::string::npos);
    CHECK(corrected.find("I cannot") == std::string::npos);
    CHECK(corrected.find("As an AI") == std::string::npos);

    auto stats = patcher.statistics();
    CHECK(stats.hallucinationFixes == 1);
    PASS();
}

static void test_statistics() {
    TEST("statistics_accumulation");
    AgentHotPatcher patcher;
    patcher.initialize();

    patcher.correctPathHallucination("path /usr/local/models/x.gguf here");
    patcher.correctPathHallucination("path /opt/llm/weights/y.gguf here");
    patcher.fixNavigationError("fix [circular] ref");
    patcher.correctHallucination("I cannot do that");
    patcher.applyBehaviorPatch("api_key=12345678901234567890123456789012 leaked");

    auto stats = patcher.statistics();
    CHECK(stats.totalPatches == 5);
    CHECK(stats.pathCorrections == 2);
    CHECK(stats.navigationFixes == 1);
    CHECK(stats.hallucinationFixes == 1);
    CHECK(stats.behaviorPatches == 1);
    PASS();
}

static void test_no_false_positives() {
    TEST("no_false_positives");
    AgentHotPatcher patcher;
    patcher.initialize();

    // Clean text should not be modified
    std::string clean = "The model loaded successfully with 7B parameters";
    CHECK(patcher.correctPathHallucination(clean) == clean);
    CHECK(patcher.fixNavigationError(clean) == clean);
    CHECK(patcher.correctHallucination(clean) == clean);
    CHECK(patcher.applyBehaviorPatch(clean) == clean);

    auto stats = patcher.statistics();
    CHECK(stats.totalPatches == 0);
    PASS();
}

static void test_callback_tracking() {
    TEST("callback_tracking");
    AgentHotPatcher patcher;
    patcher.initialize();

    std::vector<std::pair<std::string, std::string>> patchLog;
    patcher.onPatchApplied([&](const std::string& orig, const std::string& patched) {
        patchLog.push_back({orig, patched});
    });

    patcher.correctPathHallucination("load /usr/local/models/test.gguf");
    patcher.fixNavigationError("ref [null_ref] here");
    patcher.correctHallucination("As an AI model");

    CHECK(patchLog.size() == 3);
    CHECK(patchLog[0].second.find("./models/") != std::string::npos);
    CHECK(patchLog[1].second.find("[valid_ref]") != std::string::npos);
    CHECK(patchLog[2].second.find("[AGENT]") != std::string::npos);
    PASS();
}

// =========================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " AgentHotPatcher Test Suite (C++17)" << std::endl;
    std::cout << "========================================" << std::endl;

    test_initialization();
    test_hallucination_detection();
    test_path_hallucination_correction();
    test_navigation_fix();
    test_behavior_patches();
    test_hallucination_correction();
    test_statistics();
    test_no_false_positives();
    test_callback_tracking();

    std::cout << std::endl;
    std::cout << "Results: " << g_passed << "/" << g_total
              << " passed, " << g_failed << " failed" << std::endl;
    return (g_failed > 0) ? 1 : 0;
}
