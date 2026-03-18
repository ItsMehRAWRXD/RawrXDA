#include "Win32IDE.h"

#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

class EnterpriseStressTester {
public:
    explicit EnterpriseStressTester(Win32IDE* ide) : m_ide(ide) {}
    ~EnterpriseStressTester();

    bool initialize() { return m_ide != nullptr; }

    bool executeStressTestSuite(int durationSeconds, int threadCount) {
        if (!m_ide || durationSeconds <= 0 || threadCount <= 0) {
            return false;
        }

        m_ops.store(0);
        m_errors.store(0);
        m_totalLatencyUs.store(0);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(threadCount));

        for (int i = 0; i < threadCount; ++i) {
            workers.emplace_back([this, i, deadline]() { workerLoop(i, deadline); });
        }
        for (auto& t : workers) {
            t.join();
        }

        const uint64_t ops = m_ops.load();
        const uint64_t errors = m_errors.load();
        const double avg = ops > 0 ? static_cast<double>(m_totalLatencyUs.load()) / static_cast<double>(ops) : 0.0;

        m_last.operationsCompleted = ops;
        m_last.errorsEncountered = errors;
        m_last.avgResponseTimeUs = avg;
        m_last.operationsPerSecond = static_cast<double>(ops) / static_cast<double>(durationSeconds);
        m_last.passed = (errors == 0) && (ops >= static_cast<uint64_t>(threadCount) * 100ULL);
        return true;
    }

    StressTestResults results() const { return m_last; }

private:
    void workerLoop(int workerId, std::chrono::steady_clock::time_point deadline) {
        std::minstd_rand rng(static_cast<unsigned int>(GetTickCount64() + workerId * 997));
        std::uniform_int_distribution<int> pick(0, 2);

        while (std::chrono::steady_clock::now() < deadline) {
            const auto start = std::chrono::steady_clock::now();
            const bool ok = runOperation(workerId, pick(rng));
            const auto end = std::chrono::steady_clock::now();

            m_ops.fetch_add(1);
            m_totalLatencyUs.fetch_add(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()));
            if (!ok) {
                m_errors.fetch_add(1);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    bool runOperation(int workerId, int op) {
        switch (op) {
            case 0:
                return probeMainWindow();
            case 1:
                return probeEditor();
            case 2:
                return probeFilesystem(workerId);
            default:
                return true;
        }
    }

    bool probeMainWindow() {
        const HWND hwnd = m_ide->getMainWindow();
        if (!hwnd) {
            return true;
        }
        DWORD_PTR result = 0;
        const LRESULT sent = SendMessageTimeoutA(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 200, &result);
        return sent != 0;
    }

    bool probeEditor() {
        const HWND editor = m_ide->getEditor();
        if (!editor) {
            return true;
        }
        DWORD_PTR textLen = 0;
        const LRESULT sent = SendMessageTimeoutA(editor, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, 200, &textLen);
        return sent != 0;
    }

    bool probeFilesystem(int workerId) {
        char tempDir[MAX_PATH]{};
        if (!GetTempPathA(MAX_PATH, tempDir)) {
            return false;
        }
        std::string path = std::string(tempDir) + "rawrxd_stress_" + std::to_string(workerId) + ".tmp";
        HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }
        const char payload[] = "rawrxd-stress";
        DWORD written = 0;
        const BOOL wrote = WriteFile(h, payload, static_cast<DWORD>(sizeof(payload) - 1), &written, nullptr);
        CloseHandle(h);
        DeleteFileA(path.c_str());
        return wrote != 0 && written == sizeof(payload) - 1;
    }

    Win32IDE* m_ide = nullptr;
    std::atomic<uint64_t> m_ops{0};
    std::atomic<uint64_t> m_errors{0};
    std::atomic<uint64_t> m_totalLatencyUs{0};
    StressTestResults m_last{};
};

EnterpriseStressTester::~EnterpriseStressTester() = default;

void Win32IDE::initEnterpriseStressTests() {
    if (!m_enterpriseStressTester) {
        m_enterpriseStressTester = std::make_unique<EnterpriseStressTester>(this);
        if (!m_enterpriseStressTester->initialize()) {
            m_enterpriseStressTester.reset();
        }
    }
}

bool Win32IDE::executeEnterpriseStressTest(int durationSeconds, int threadCount) {
    if (!m_enterpriseStressTester) {
        initEnterpriseStressTests();
    }
    if (!m_enterpriseStressTester) {
        return false;
    }

    const bool ok = m_enterpriseStressTester->executeStressTestSuite(durationSeconds, threadCount);
    m_stressTestResults = m_enterpriseStressTester->results();
    return ok;
}

void Win32IDE::handleEnterpriseStressTestCommand() {
    if (!showStressTestDialog()) {
        return;
    }
    if (!executeEnterpriseStressTest(45, 4)) {
        MessageBoxA(getMainWindow(), "Enterprise stress test failed to start.", "Stress Test", MB_ICONERROR | MB_OK);
        return;
    }

    std::ostringstream msg;
    msg << "Operations: " << m_stressTestResults.operationsCompleted << "\n"
        << "Errors: " << m_stressTestResults.errorsEncountered << "\n"
        << "Avg latency (us): " << m_stressTestResults.avgResponseTimeUs << "\n"
        << "Ops/sec: " << m_stressTestResults.operationsPerSecond << "\n"
        << "Result: " << (m_stressTestResults.passed ? "PASS" : "FAIL");
    MessageBoxA(getMainWindow(), msg.str().c_str(), "Enterprise Stress Test", MB_OK | MB_ICONINFORMATION);
}

bool Win32IDE::showStressTestDialog() {
    const int rc = MessageBoxA(getMainWindow(),
                               "Run enterprise stress suite now?\n"
                               "Profile: 45 seconds, 4 workers, UI + FS probes.",
                               "Enterprise Stress Test",
                               MB_OKCANCEL | MB_ICONQUESTION);
    return rc == IDOK;
}
