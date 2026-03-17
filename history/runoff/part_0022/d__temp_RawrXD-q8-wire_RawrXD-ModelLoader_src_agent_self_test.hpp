#pragma once

#include <string>
#include <vector>
#include <functional>

namespace RawrXD {

class SelfTest {
public:
    explicit SelfTest();
    ~SelfTest() = default;

    bool runAll();               // unit + integration + perf
    bool runUnitTests();         // build/bin/*_test.exe
    bool runIntegrationTests();  // deflate_50mb, flash_attn, etc.
    bool runLint();              // cl.exe /analyze
    bool runBenchmarkBaseline(); // tokens/sec vs. stored baseline

    std::string lastOutput() const { return m_output; }
    std::string lastError() const { return m_error; }

    using LogCallback = std::function<void(const std::string&)>;
    void setLogCallback(LogCallback cb) { m_logCallback = std::move(cb); }

private:
    bool runProcess(const std::string& prog, const std::vector<std::string>& args, int timeoutMs = 60000);
    double parseTPS(const std::string& log) const;
    bool checkBenchmarkRegression(const std::string& name, double current, double baseline);

    std::string m_output;
    std::string m_error;
    LogCallback m_logCallback;

    void log(const std::string& line) {
        if (m_logCallback) m_logCallback(line);
    }
};

} // namespace RawrXD
