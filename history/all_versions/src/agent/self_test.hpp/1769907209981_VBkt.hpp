#pragma once


class SelfTest {

public:
    explicit SelfTest(void* parent = nullptr);

    bool runAll();               // unit + integration + perf
    bool runUnitTests();         // build/bin/*_test.exe
    bool runIntegrationTests();  // deflate_50mb, flash_attn, etc.
    bool runInferenceTests();    // ModelInvoker check
    bool runLint();              // cl.exe /analyze
    bool runBenchmarkBaseline(); // tokens/sec vs. stored baseline

    std::string lastOutput() const { return m_output; }
    std::string lastError() const { return m_error; }

    void log(const std::string& line);

private:
    bool runProcess(const std::string& prog, const std::vector<std::string>& args, int timeoutMs = 60000);
    double parseTPS(const std::string& log) const;
    bool checkBenchmarkRegression(const std::string& name, double current, double baseline);

    std::string m_output;
    std::string m_error;
};

