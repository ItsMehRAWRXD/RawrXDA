// ============================================================================
// pipeline_dashboard.cpp - Real-time TUI for RawrXD Pipeline Telemetry
// ============================================================================
// ANSI-based terminal dashboard showing:
//   - Live sparkline of throughput (elements/sec)
//   - Governor "clench" level (minCredits bar)
//   - Credit reservoir level (stall proximity)
//   - PID error convergence
//
// Polls at 60Hz (16ms), renders at 30Hz (33ms)
// ============================================================================

#include "kernels/fp8_quantizer_avx512.hpp"
#include "flow_control/credit_governor.hpp"
#include "flow_control/credit_based_flow_control.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>
#include <cmath>
#include <windows.h>
#include <conio.h>

using namespace RawrXD::Kernels;
using namespace RawrXD::FlowControl;

// ANSI escape codes
#define ESC "\x1B"
#define CLEAR_SCREEN ESC "[2J"
#define CURSOR_HOME ESC "[H"
#define CURSOR_HIDE ESC "[?25l"
#define CURSOR_SHOW ESC "[?25h"
#define RESET ESC "[0m"
#define BOLD ESC "[1m"
#define DIM ESC "[2m"
#define RED ESC "[31m"
#define GREEN ESC "[32m"
#define YELLOW ESC "[33m"
#define BLUE ESC "[34m"
#define MAGENTA ESC "[35m"
#define CYAN ESC "[36m"
#define WHITE ESC "[37m"
#define BG_RED ESC "[41m"
#define BG_GREEN ESC "[42m"
#define BG_YELLOW ESC "[43m"
#define BG_BLUE ESC "[44m"

// Dashboard dimensions
constexpr int SPARKLINE_WIDTH = 60;
constexpr int BAR_WIDTH = 40;
constexpr int HISTORY_SIZE = 120;  // 2 seconds at 60Hz

struct TelemetryHistory {
    std::vector<double> throughput;
    std::vector<double> error;
    std::vector<uint32_t> minCredits;
    std::vector<uint32_t> availableCredits;
    size_t writePos = 0;
    bool wrapped = false;
    
    void push(double tp, double err, uint32_t minC, uint32_t availC) {
        if (throughput.size() < HISTORY_SIZE) {
            throughput.push_back(tp);
            error.push_back(err);
            minCredits.push_back(minC);
            availableCredits.push_back(availC);
        } else {
            throughput[writePos] = tp;
            error[writePos] = err;
            minCredits[writePos] = minC;
            availableCredits[writePos] = availC;
            writePos = (writePos + 1) % HISTORY_SIZE;
            wrapped = true;
        }
    }
    
    size_t size() const {
        return wrapped ? HISTORY_SIZE : writePos;
    }
    
    double getThroughput(size_t i) const {
        size_t idx = wrapped ? (writePos + i) % HISTORY_SIZE : i;
        return throughput[idx];
    }
    
    double getError(size_t i) const {
        size_t idx = wrapped ? (writePos + i) % HISTORY_SIZE : i;
        return error[idx];
    }
    
    uint32_t getMinCredits(size_t i) const {
        size_t idx = wrapped ? (writePos + i) % HISTORY_SIZE : i;
        return minCredits[idx];
    }
    
    uint32_t getAvailableCredits(size_t i) const {
        size_t idx = wrapped ? (writePos + i) % HISTORY_SIZE : i;
        return availableCredits[idx];
    }
};

// Sparkline characters (Unicode block elements)
static const char* SPARK_CHARS[] = {" ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"};

void drawSparkline(const TelemetryHistory& hist, double minVal, double maxVal, int row, int col) {
    printf(ESC "[%d;%dH", row, col);
    
    size_t count = hist.size();
    size_t start = count > SPARKLINE_WIDTH ? count - SPARKLINE_WIDTH : 0;
    size_t displayCount = count > SPARKLINE_WIDTH ? SPARKLINE_WIDTH : count;
    
    double range = maxVal - minVal;
    if (range < 1e-9) range = 1.0;
    
    for (size_t i = 0; i < displayCount; i++) {
        double val = hist.getThroughput(start + i);
        int level = static_cast<int>(((val - minVal) / range) * 8.0);
        if (level < 0) level = 0;
        if (level > 8) level = 8;
        
        // Color based on value
        if (val > maxVal * 0.8) printf(GREEN);
        else if (val > maxVal * 0.5) printf(YELLOW);
        else printf(RED);
        
        printf("%s", SPARK_CHARS[level]);
    }
    printf(RESET);
}

void drawBar(int value, int max, int width, int row, int col, const char* label, const char* color) {
    printf(ESC "[%d;%dH%s%s%s", row, col, BOLD, color, label);
    printf(RESET " [");
    
    int filled = (value * width) / max;
    if (filled > width) filled = width;
    
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            if (i < width * 0.3) printf(GREEN "\u2588");
            else if (i < width * 0.7) printf(YELLOW "\u2588");
            else printf(RED "\u2588");
        } else {
            printf(DIM "\u2591");
        }
    }
    printf(RESET "] %d/%d", value, max);
}

void drawHeader(int row) {
    printf(ESC "[%d;1H" BOLD CYAN, row);
    printf("╔══════════════════════════════════════════════════════════════════════════════╗");
    printf(ESC "[%d;1H", row + 1);
    printf("║" BOLD WHITE "  RAWRXD v1.1.0-dev — Pipeline Telemetry Dashboard" RESET CYAN "                          ║");
    printf(ESC "[%d;1H", row + 2);
    printf("╠══════════════════════════════════════════════════════════════════════════════╣");
    printf(RESET);
}

void drawFooter(int row) {
    printf(ESC "[%d;1H" CYAN, row);
    printf("╚══════════════════════════════════════════════════════════════════════════════╝");
    printf(RESET);
}

void drawMetric(int row, int col, const char* label, double value, const char* unit, const char* color) {
    printf(ESC "[%d;%dH" BOLD "%s%-20s" RESET "%s%10.2f %s", 
           row, col, color, label, WHITE, value, unit);
}

void drawMetricInt(int row, int col, const char* label, int value, const char* unit, const char* color) {
    printf(ESC "[%d;%dH" BOLD "%s%-20s" RESET "%s%10d %s", 
           row, col, color, label, WHITE, value, unit);
}

void drawStatus(int row, int col, const char* label, bool active, const char* activeColor) {
    printf(ESC "[%d;%dH" BOLD "%-20s", row, col, label);
    if (active) {
        printf("%s[ ACTIVE ]" RESET, activeColor);
    } else {
        printf(DIM "[ STANDBY ]" RESET);
    }
}

// Simulate workload with varying computational weight
class WorkloadSimulator {
public:
    FP8QuantizerAVX512 quantizer;
    CreditCounter counter;
    CreditGovernor governor;
    
    std::vector<float> inputBuffer;
    std::vector<uint8_t> outputBuffer;
    size_t elementCount = 65536;
    
    bool Initialize() {
        quantizer.SetSilent(true);  // Suppress BEFORE init
        if (!quantizer.Initialize(QuantizeStrategy::Auto)) return false;
        
        CreditConfig creditCfg;
        creditCfg.initialCredits = 10000;
        creditCfg.maxCredits = 10000;
        creditCfg.minCredits = 100;
        creditCfg.reserveForPartial = false;
        creditCfg.silent = true;  // Suppress printf for clean TUI
        counter.Initialize(creditCfg);
        
        GovernorConfig govCfg;
        govCfg.targetThroughput = 5.0e9;
        govCfg.kp = 0.5;
        govCfg.ki = 0.1;
        govCfg.kd = 0.1;
        govCfg.updateIntervalMs = 50;
        govCfg.minCreditsFloor = 10;
        govCfg.minCreditsCeiling = 500;
        govCfg.emaAlpha = 0.3;
        govCfg.silent = true;  // Suppress printf for clean TUI
        governor.Initialize(creditCfg, govCfg);
        
        inputBuffer.resize(elementCount);
        outputBuffer.resize(elementCount);
        for (size_t i = 0; i < elementCount; i++) {
            inputBuffer[i] = static_cast<float>(i % 100) * 0.1f;
        }
        
        return true;
    }
    
    double RunIteration() {
        auto result = counter.TryAcquire(1000);
        if (result != CreditResult::Success) {
            return 0.0;  // Blocked
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        quantizer.Quantize(inputBuffer.data(), outputBuffer.data(), elementCount, 1.0f);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        double throughput = elementCount / (nanos / 1e9);
        
        counter.ReturnCredits(1000);
        
        GovernorTelemetry tel;
        tel.throughputElemPerSec = throughput;
        tel.timestampMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        governor.RecordTelemetry(tel);
        
        return throughput;
    }
};

int main() {
    // Enable ANSI on Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    
    printf(CLEAR_SCREEN CURSOR_HIDE);
    
    WorkloadSimulator sim;
    if (!sim.Initialize()) {
        printf(CURSOR_SHOW "Failed to initialize simulator\n");
        return 1;
    }
    
    TelemetryHistory history;
    
    auto startTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    double peakThroughput = 0.0;
    double avgThroughput = 0.0;
    int iterationCount = 0;
    
    bool running = true;
    auto lastRender = std::chrono::steady_clock::now();
    auto lastPoll = std::chrono::steady_clock::now();
    
    while (running) {
        auto now = std::chrono::steady_clock::now();
        
        // Poll at ~60Hz (16ms)
        auto pollElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastPoll).count();
        if (pollElapsed >= 16) {
            double tp = sim.RunIteration();
            
            auto cfg = sim.governor.GetCurrentConfig();
            auto stats = sim.counter.GetStats();
            
            history.push(tp, sim.governor.GetLastError(), cfg.minCredits, 
                        sim.counter.GetAvailableCredits());
            
            if (tp > peakThroughput) peakThroughput = tp;
            avgThroughput = (avgThroughput * iterationCount + tp) / (iterationCount + 1);
            iterationCount++;
            
            lastPoll = now;
        }
        
        // Render at ~30Hz (33ms)
        auto renderElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastRender).count();
        if (renderElapsed >= 33) {
            printf(CURSOR_HOME);
            
            // Header
            drawHeader(1);
            
            // Runtime info
            auto runtime = std::chrono::duration_cast<std::chrono::seconds>(
                now - startTime).count();
            printf(ESC "[4;1H" BOLD WHITE "  Runtime: %02lld:%02lld:%02lld  " RESET "|  " BOLD "Iterations: %d  " RESET "|  " BOLD "Frame: %d",
                   runtime / 3600, (runtime % 3600) / 60, runtime % 60,
                   iterationCount, frameCount);
            
            // Separator
            printf(ESC "[5;1H" CYAN "╠══════════════════════════════════════════════════════════════════════════════╣" RESET);
            
            // Throughput section
            printf(ESC "[6;1H" BOLD BLUE "  THROUGHPUT" RESET);
            drawMetric(7, 3, "Current:", history.size() > 0 ? history.getThroughput(history.size() - 1) / 1e6 : 0, "M elem/s", CYAN);
            drawMetric(8, 3, "Peak:", peakThroughput / 1e6, "M elem/s", GREEN);
            drawMetric(9, 3, "Average:", avgThroughput / 1e6, "M elem/s", YELLOW);
            drawMetric(10, 3, "Target:", 5000.0, "M elem/s", WHITE);
            
            // Sparkline
            printf(ESC "[11;1H" BOLD "  History (last 2s):" RESET);
            drawSparkline(history, 0, peakThroughput > 0 ? peakThroughput : 1e9, 12, 3);
            
            // Governor section
            printf(ESC "[14;1H" CYAN "╠══════════════════════════════════════════════════════════════════════════════╣" RESET);
            printf(ESC "[15;1H" BOLD MAGENTA "  GOVERNOR STATUS" RESET);
            
            auto cfg = sim.governor.GetCurrentConfig();
            double lastErr = sim.governor.GetLastError();
            double lastAdj = sim.governor.GetLastAdjustment();
            
            drawMetric(16, 3, "minCredits:", cfg.minCredits, "", 
                      cfg.minCredits < 50 ? GREEN : (cfg.minCredits < 200 ? YELLOW : RED));
            drawMetric(17, 3, "Last Error:", lastErr, "", 
                      std::abs(lastErr) < 0.1 ? GREEN : (std::abs(lastErr) < 0.5 ? YELLOW : RED));
            drawMetric(18, 3, "Adjustment:", lastAdj, "", 
                      std::abs(lastAdj) < 0.5 ? GREEN : YELLOW);
            
            // Clench bar
            printf(ESC "[19;1H" BOLD "  Clench Level:" RESET);
            drawBar(cfg.minCredits, 500, BAR_WIDTH, 20, 3, "", MAGENTA);
            
            // Credit reservoir
            printf(ESC "[22;1H" CYAN "╠══════════════════════════════════════════════════════════════════════════════╣" RESET);
            printf(ESC "[23;1H" BOLD GREEN "  CREDIT RESERVOIR" RESET);
            
            uint32_t avail = sim.counter.GetAvailableCredits();
            uint32_t maxC = cfg.maxCredits;
            auto stats = sim.counter.GetStats();
            
            drawMetricInt(24, 3, "Available:", avail, "credits", 
                         avail > maxC * 0.5 ? GREEN : (avail > maxC * 0.2 ? YELLOW : RED));
            drawMetricInt(25, 3, "Max:", maxC, "credits", WHITE);
            drawMetricInt(26, 3, "Blocked Ops:", static_cast<int>(stats.acquireBlocked), "", RED);
            
            // Reservoir bar
            printf(ESC "[27;1H" BOLD "  Reservoir Level:" RESET);
            drawBar(avail, maxC, BAR_WIDTH, 28, 3, "", GREEN);
            
            // Status indicators
            printf(ESC "[30;1H" CYAN "╠══════════════════════════════════════════════════════════════════════════════╣" RESET);
            printf(ESC "[31;1H" BOLD "  SYSTEM STATUS" RESET);
            
            drawStatus(32, 3, "Auto-Tuning:", sim.governor.IsAutoTuning(), GREEN);
            drawStatus(33, 3, "Backpressure:", avail <= cfg.minCredits * 2, YELLOW);
            drawStatus(34, 3, "Quantizer:", true, GREEN);
            
            // Footer
            drawFooter(36);
            
            // Instructions
            printf(ESC "[37;1H" DIM "  Press Ctrl+C to exit  |  PID: Kp=0.5 Ki=0.1 Kd=0.1  |  EMA=0.3" RESET);
            
            fflush(stdout);
            frameCount++;
            lastRender = now;
        }
        
        // Check for keypress (non-blocking)
        if (_kbhit()) {
            char c = _getch();
            if (c == 3 || c == 'q' || c == 'Q') {  // Ctrl+C or q
                running = false;
            }
        }
        
        // Small sleep to prevent CPU spinning
        Sleep(1);
    }
    
    printf(CLEAR_SCREEN CURSOR_SHOW CURSOR_HOME);
    printf("Dashboard exited.\n");
    printf("Peak throughput: %.2f M elements/sec\n", peakThroughput / 1e6);
    printf("Average throughput: %.2f M elements/sec\n", avgThroughput / 1e6);
    printf("Total iterations: %d\n", iterationCount);
    
    return 0;
}
