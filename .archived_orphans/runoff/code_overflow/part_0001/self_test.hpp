#pragma once
// self_test.hpp – Qt-free SelfTest (C++20 / Win32)
#include <cstdint>
#include <string>
#include <vector>

class SelfTest {
public:
    SelfTest();
    ~SelfTest() = default;

    bool runAll();               // unit + integration + perf
    bool runUnitTests();         // build/bin/*_test.exe
    bool runIntegrationTests();  // deflate_50mb, flash_attn, etc.
    bool runLint();              // cl.exe /analyze
    bool runBenchmarkBaseline(); // tokens/sec vs. stored baseline

    const std::string& lastOutput() const { return m_output; }
    const std::string& lastError()  const { return m_error; }

    // Callback for log lines (replaces Qt signal)
    using LogCallback = void(*)(void* ctx, const char* line);
    void setLogCb(LogCallback cb, void* ctx) { m_logCb = cb; m_logCtx = ctx; }

private:
    bool   runProcess(const std::string& prog, const std::vector<std::string>& args, int timeoutMs = 60000);
    double parseTPS(const std::string& log) const;
    bool   checkBenchmarkRegression(const std::string& name, double current, double baseline);
    void   log(const std::string& line);

    std::string m_output;
    std::string m_error;

    LogCallback m_logCb  = nullptr;
    void*       m_logCtx = nullptr;
};
