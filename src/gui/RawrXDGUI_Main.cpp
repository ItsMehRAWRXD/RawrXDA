#include <windows.h>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include "inference/MLInferenceEngine.hpp"
#include "sovereign/SovereignCoreWrapper.hpp"
#include "gui/TokenStreamDisplay.hpp"

using json = nlohmann::json;

namespace RawrXD::GUI {

// Global state
static TokenStreamDisplay* g_tokenDisplay = nullptr;
static TelemetryDisplay* g_telemetryDisplay = nullptr;
static std::thread* g_inferenceThread = nullptr;

// Main window procedure
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class RawrXDGUI {
public:
    int run(HINSTANCE hInstance) {
        m_hInstance = hInstance;

        // Register main window class
        WNDCLASSA wc{};
        wc.lpfnWndProc = MainWndProc;
        wc.lpszClassName = "RawrXD_MainWindowClass";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);

        if (!RegisterClassA(&wc)) {
            return 1;
        }

        // Create main window
        m_hMainWindow = CreateWindowExA(
            0,
            "RawrXD_MainWindowClass",
            "RawrXD IDE — Real ML Inference + Autonomous Core",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1400, 800,
            nullptr, nullptr, hInstance, this
        );

        if (!m_hMainWindow) {
            return 2;
        }

        ShowWindow(m_hMainWindow, SW_SHOW);
        UpdateWindow(m_hMainWindow);

        // Message loop
        MSG msg{};
        while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        return (int)msg.wParam;
    }

    void initialize() {
        // Initialize inference engine
        auto& mlEngine = RawrXD::ML::MLInferenceEngine::getInstance();
        if (mlEngine.initialize()) {
            OutputDebugStringA("✓ ML engine initialized\n");
        } else {
            OutputDebugStringA("✗ ML engine failed to connect\n");
        }

        // Initialize sovereign core
        auto& sovCore = RawrXD::Sovereign::SovereignCore::getInstance();
        try {
            sovCore.initialize(1);
            OutputDebugStringA("✓ Sovereign core initialized\n");
        } catch (...) {
            OutputDebugStringA("✗ Sovereign core failed\n");
        }

        // Create UI components
        g_tokenDisplay = new TokenStreamDisplay();
        g_tokenDisplay->create(m_hMainWindow, 10, 10, 680, 400);

        g_telemetryDisplay = new TelemetryDisplay();
        g_telemetryDisplay->create(m_hMainWindow, 700, 10, 680, 400);

        // Start autonomous loop
        sovCore.startAutonomousLoop();
        SetTimer(m_hMainWindow, 1, 200, nullptr);
    }

    void onInferenceClicked() {
        const char* testPrompt = "What is the role of MASM .ENDPROLOG and stack alignment in x64?";

        // Disable button
        EnableWindow(m_hInferButton, FALSE);
        g_tokenDisplay->clear();

        // Start inference in background thread
        if (g_inferenceThread) delete g_inferenceThread;
        g_inferenceThread = new std::thread([&, testPrompt]() {
            auto& mlEngine = RawrXD::ML::MLInferenceEngine::getInstance();
            auto& sovCore = RawrXD::Sovereign::SovereignCore::getInstance();

            auto result = mlEngine.query(testPrompt, [&](const std::string& token) {
                // Append token to display (thread-safe via PostMessage)
                PostMessageA(m_hMainWindow, WM_APP_TOKEN, 0, (LPARAM)new std::string(token));
            });

            // Trigger autonomous cycle
            try {
                sovCore.runCycle();
                PostMessageA(m_hMainWindow, WM_APP_CYCLE_COMPLETE, 0, 0);
            } catch (...) {
            }

            // Save telemetry
            json telemetry = json::parse(mlEngine.telemetryToJSON());
            std::ofstream file("d:\\rawrxd\\inference_telemetry.json");
            file << telemetry.dump(4);
            file.close();

            PostMessageA(m_hMainWindow, WM_APP_INFERENCE_COMPLETE, 0, 0);
            EnableWindow(m_hInferButton, TRUE);
        });

        g_inferenceThread->detach();
    }

    void onTokenReceived(std::string* pToken) {
        if (g_tokenDisplay && pToken) {
            g_tokenDisplay->appendToken(*pToken);
            delete pToken;
        }
    }

    void onCycleComplete() {
        auto& sovCore = RawrXD::Sovereign::SovereignCore::getInstance();
        auto stats = sovCore.getStats();

        if (g_telemetryDisplay) {
            TelemetryDisplay::Metrics metrics{
                .totalTokens = stats.cycleCount * 100,  // Estimate
                .totalLatencyMs = stats.elapsed,
                .avgTokensPerSecond = (stats.elapsed > 0) ? (stats.cycleCount * 100.0f * 1000.0f / stats.elapsed) : 0.0f,
                .engineStatus = "ready"
            };
            g_telemetryDisplay->updateMetrics(metrics);
        }
    }

    HWND getMainWindow() const { return m_hMainWindow; }
    void setInferButton(HWND h) { m_hInferButton = h; }

private:
    HINSTANCE m_hInstance{nullptr};
    HWND m_hMainWindow{nullptr};
    HWND m_hInferButton{nullptr};
};

static RawrXDGUI* g_pAppInstance = nullptr;

#define WM_APP_TOKEN (WM_APP + 1)
#define WM_APP_CYCLE_COMPLETE (WM_APP + 2)
#define WM_APP_INFERENCE_COMPLETE (WM_APP + 3)

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        g_pAppInstance = reinterpret_cast<RawrXDGUI*>(pCreate->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)g_pAppInstance);
        g_pAppInstance->initialize();
        return 0;
    }

    RawrXDGUI* pThis = reinterpret_cast<RawrXDGUI*>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    if (pThis) {
        switch (msg) {
        case WM_CREATE: {
            // Create inference button
            HWND hButton = CreateWindowExA(
                0, "BUTTON", "Run Real Inference",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 420, 200, 30,
                hwnd, (HMENU)1001, GetModuleHandleA(nullptr), nullptr
            );
            pThis->setInferButton(hButton);
            return 0;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                pThis->onInferenceClicked();
            }
            return 0;

        case WM_TIMER:
            if (g_telemetryDisplay) {
                pThis->onCycleComplete();
            }
            return 0;

        case WM_APP_TOKEN: {
            std::string* pToken = reinterpret_cast<std::string*>(lParam);
            pThis->onTokenReceived(pToken);
            return 0;
        }

        case WM_APP_CYCLE_COMPLETE:
            pThis->onCycleComplete();
            return 0;

        case WM_APP_INFERENCE_COMPLETE:
            MessageBoxA(hwnd, "Inference complete! Check telemetry.json", "Done", MB_OK | MB_ICONINFORMATION);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    try {
        RawrXD::GUI::RawrXDGUI gui;
        return gui.run(hInstance);
    } catch (const std::exception& e) {
        OutputDebugStringA("FATAL: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return 1;
    }
}
