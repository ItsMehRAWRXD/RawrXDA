// ============================================================================
// context_fusion_harness.cpp — Headless validation of ContextFusionEngine
// ============================================================================
// Validates:
//   • Event propagation under load
//   • Frame integrity (multi-source merge)
//   • Latency bounds (P50/P95)
//   • No feedback loops or runaway updates
//
// Run: context_fusion_harness.exe
// Exit: 0 = PASS, 1 = FAIL
// ============================================================================

#include "core/ContextFusionEngine.h"
#include "win32app/GhostTextContextSubscriber.h"
#include "win32app/Win32IDE_GhostText.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <thread>

namespace RawrXD {

// ── Mock GhostText (no UI, just records calls) ───────────────────────────────

class MockGhostText : public Win32IDE_GhostText {
public:
    struct Call {
        std::string method;
        std::string prefix;
        std::string suffix;
        std::chrono::steady_clock::time_point time;
    };
    std::vector<Call> calls;
    bool showing = false;
    bool enabled = true;

    void ShowSuggestion(const GhostTextSuggestion&) override { showing = true; }
    void HideSuggestion() override { showing = false; }
    void Hide() override { showing = false; }
    void Clear() override { showing = false; }
    bool IsShowing() const override { return showing; }
    void AcceptSuggestion() override {}
    void DismissSuggestion() override {}
    void SetEnabled(bool e) override { enabled = e; }
    bool IsEnabled() const override { return enabled; }
    void SetDelayMs(int) override {}
    std::string GetCurrentLine() const override { return ""; }
    int GetCursorPosition() const override { return 0; }
    std::string GetRecentLines(int) const override { return ""; }
    void RequestSuggestion(
        const std::string& prefix,
        const std::string& suffix,
        const std::string&,
        const std::string&,
        const std::vector<std::string>&,
        const std::vector<DiagnosticInfo>&
    ) override {
        Call c;
        c.method = "RequestSuggestion";
        c.prefix = prefix;
        c.suffix = suffix;
        c.time = std::chrono::steady_clock::now();
        calls.push_back(c);
    }
    void SetSuggestionProvider(SuggestionCallback) override {}
};

// ── Test Reporter ───────────────────────────────────────────────────────────

struct TestResult {
    std::string name;
    bool passed = false;
    std::string detail;
    int64_t latencyUs = 0;
};

static std::vector<TestResult> g_results;

static void Report(const std::string& name, bool pass, const std::string& detail = "", int64_t latencyUs = 0) {
    TestResult r;
    r.name = name;
    r.passed = pass;
    r.detail = detail;
    r.latencyUs = latencyUs;
    g_results.push_back(r);
    std::cout << (pass ? "[PASS] " : "[FAIL] ") << name;
    if (!detail.empty()) std::cout << " — " << detail;
    if (latencyUs > 0) std::cout << " (" << latencyUs << " us)";
    std::cout << "\n";
}

// ── Tests ─────────────────────────────────────────────────────────────────────

static bool Test_FrameIntegrity() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    // Build a valid frame
    ContextFrameBuilder builder;
    builder.WithEditorState("test.cpp", "int main() {\n    return 0;\n}\n", {1, 4}, {});
    auto frame = builder.Build();
    frame.languageId = "cpp";   // Builder doesn't auto-detect in test
    frame.version = 1;          // Must be non-zero for non-empty buffer

    bool valid = engine.ValidateFrameIntegrity(frame);
    std::string diag = engine.GetFrameDiagnostics(frame);

    engine.Shutdown();
    Report("FrameIntegrity", valid, diag);
    return valid;
}

static bool Test_InvalidFrameDetection() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    // Invalid: cursor out of bounds
    ContextFrame bad;
    bad.bufferText = "one line\n";
    bad.cursor.line = 100; // out of bounds
    bad.cursor.column = 0;
    bad.version = 1;

    bool caught = !engine.ValidateFrameIntegrity(bad);
    std::string diag = engine.GetFrameDiagnostics(bad);

    engine.Shutdown();
    Report("InvalidFrameDetection", caught, diag);
    return caught;
}

static bool Test_EventPropagation() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    auto mock = std::make_unique<MockGhostText>();
    auto sub = std::make_unique<GhostTextContextSubscriber>(mock.get());
    sub->SetDebounceMs(0); // No debounce for test
    engine.Subscribe(sub.get());

    // Emit file opened event first (sets filePath and languageId)
    std::string path = "test.cpp";
    ContextEvent evFile(ContextEvent::FILE_OPENED, "test", &path);
    engine.EmitEvent(evFile);

    // Emit editor changed event
    std::string text = "void foo() {}";
    ContextEvent ev(ContextEvent::EDITOR_CHANGED, "test", &text);
    engine.EmitEvent(ev);

    // Give dispatch a moment
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    bool received = !mock->calls.empty();
    engine.Shutdown();
    Report("EventPropagation", received,
        "calls=" + std::to_string(mock->calls.size()));
    return received;
}

static bool Test_LatencyBounds() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    auto mock = std::make_unique<MockGhostText>();
    auto sub = std::make_unique<GhostTextContextSubscriber>(mock.get());
    sub->SetDebounceMs(0);
    engine.Subscribe(sub.get());

    std::vector<int64_t> latencies;
    const int N = 50;

    for (int i = 0; i < N; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        std::string text = "line " + std::to_string(i) + "\n";
        ContextEvent ev(ContextEvent::EDITOR_CHANGED, "test", &text);
        engine.EmitEvent(ev);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto t1 = std::chrono::steady_clock::now();
        latencies.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
        );
    }

    // Compute P50 and P95
    std::sort(latencies.begin(), latencies.end());
    int64_t p50 = latencies[latencies.size() / 2];
    int64_t p95 = latencies[static_cast<size_t>(latencies.size() * 0.95)];

    bool pass = p95 < 50000; // 50ms ceiling for headless test
    std::ostringstream detail;
    detail << "P50=" << p50 << "us P95=" << p95 << "us";

    engine.Shutdown();
    Report("LatencyBounds", pass, detail.str(), p95);
    return pass;
}

static bool Test_NoFeedbackLoop() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    auto mock = std::make_unique<MockGhostText>();
    auto sub = std::make_unique<GhostTextContextSubscriber>(mock.get());
    sub->SetDebounceMs(0);
    engine.Subscribe(sub.get());

    // Emit same event 10 times rapidly
    std::string text = "static void no_loop() {}";
    for (int i = 0; i < 10; ++i) {
        ContextEvent ev(ContextEvent::EDITOR_CHANGED, "test", &text);
        engine.EmitEvent(ev);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Should NOT have 10 calls (would indicate feedback loop)
    // Debounce + version check should coalesce
    bool pass = mock->calls.size() <= 2;

    engine.Shutdown();
    Report("NoFeedbackLoop", pass,
        "calls=" + std::to_string(mock->calls.size()) + " (expected <=2)");
    return pass;
}

static bool Test_MultiSourceMerge() {
    auto& engine = ContextFusionEngine::Get();
    engine.Initialize();

    // Emit editor event
    std::string text = "int x = 42;\n";
    ContextEvent ev1(ContextEvent::EDITOR_CHANGED, "editor", &text);
    engine.EmitEvent(ev1);

    // Emit LSP event
    std::vector<SymbolInfo> symbols;
    SymbolInfo sym; sym.name = "x"; sym.kind = "variable"; sym.line = 0;
    symbols.push_back(sym);
    ContextEvent ev2(ContextEvent::LSP_UPDATED, "lsp", &symbols);
    engine.EmitEvent(ev2);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto frame = engine.GetFrameCopy();
    bool merged = (frame.bufferText == text) && (frame.symbols.size() == 1);

    engine.Shutdown();
    Report("MultiSourceMerge", merged,
        "buffer=" + std::to_string(frame.bufferText.size()) +
        " symbols=" + std::to_string(frame.symbols.size()));
    return merged;
}

} // namespace RawrXD

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    using namespace RawrXD;

    std::cout << "=== Context Fusion Harness ===\n";
    std::cout << "Testing: FrameIntegrity, InvalidFrameDetection, EventPropagation,\n";
    std::cout << "         LatencyBounds, NoFeedbackLoop, MultiSourceMerge\n\n";

    bool allPass = true;
    allPass &= Test_FrameIntegrity();
    allPass &= Test_InvalidFrameDetection();
    allPass &= Test_EventPropagation();
    allPass &= Test_LatencyBounds();
    allPass &= Test_NoFeedbackLoop();
    allPass &= Test_MultiSourceMerge();

    std::cout << "\n=== Summary ===\n";
    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) passed++; else failed++;
    }
    std::cout << "Passed: " << passed << " / " << g_results.size() << "\n";
    std::cout << "Failed: " << failed << "\n";

    if (allPass) {
        std::cout << "\n[HARNESS] PASS\n";
        return 0;
    } else {
        std::cout << "\n[HARNESS] FAIL\n";
        return 1;
    }
}
