// ============================================================================
// MOE TELEMETRY UI (Flight Recorder)
// Real-time diagnostics overlay for Win32IDE
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>

namespace RawrXD {
namespace Telemetry {

// ----------------------------------------------------------------------------
// Diagnostic Frame (Per-Generation Metrics)
// ----------------------------------------------------------------------------
struct DiagnosticFrame {
    float acceptance_rate;      // α: percentage of draft tokens kept
    int tokens_produced;        // Total tokens in this burst
    double draft_latency_ms;    // Time spent on draft model
    double verify_latency_ms;   // Time spent on target verification
    double total_ms;            // End-to-end latency
    int expert_id;              // Which expert was used
    float expert_score;         // Current EMA score for this expert
    int expert_trials;          // How many times this expert was used
    float reward;               // Computed reward for this generation
    uint64_t timestamp_ms;      // Generation timestamp
    
    DiagnosticFrame() 
        : acceptance_rate(0.0f)
        , tokens_produced(0)
        , draft_latency_ms(0.0)
        , verify_latency_ms(0.0)
        , total_ms(0.0)
        , expert_id(0)
        , expert_score(0.5f)
        , expert_trials(0)
        , reward(0.0f)
        , timestamp_ms(0)
    {}
};

// ----------------------------------------------------------------------------
// Rolling Statistics
// ----------------------------------------------------------------------------
template<typename T, size_t N>
class RollingBuffer {
    std::array<T, N> data_;
    size_t head_ = 0;
    size_t count_ = 0;
    std::mutex mtx_;
    
public:
    void Push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        data_[head_] = value;
        head_ = (head_ + 1) % N;
        if (count_ < N) count_++;
    }
    
    T Mean() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        if (count_ == 0) return T{};
        T sum = {};
        for (size_t i = 0; i < count_; i++) {
            sum = sum + data_[(head_ + N - count_ + i) % N];
        }
        return sum / static_cast<T>(count_);
    }
    
    T Min() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        if (count_ == 0) return T{};
        T min_val = data_[(head_ + N - count_) % N];
        for (size_t i = 1; i < count_; i++) {
            T val = data_[(head_ + N - count_ + i) % N];
            if (val < min_val) min_val = val;
        }
        return min_val;
    }
    
    T Max() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        if (count_ == 0) return T{};
        T max_val = data_[(head_ + N - count_) % N];
        for (size_t i = 1; i < count_; i++) {
            T val = data_[(head_ + N - count_ + i) % N];
            if (val > max_val) max_val = val;
        }
        return max_val;
    }
    
    size_t Count() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx_));
        return count_;
    }
};

// Specialization for DiagnosticFrame
inline DiagnosticFrame operator+(const DiagnosticFrame& a, const DiagnosticFrame& b) {
    DiagnosticFrame r;
    r.acceptance_rate = a.acceptance_rate + b.acceptance_rate;
    r.tokens_produced = a.tokens_produced + b.tokens_produced;
    r.draft_latency_ms = a.draft_latency_ms + b.draft_latency_ms;
    r.verify_latency_ms = a.verify_latency_ms + b.verify_latency_ms;
    r.total_ms = a.total_ms + b.total_ms;
    r.reward = a.reward + b.reward;
    return r;
}

inline DiagnosticFrame operator/(const DiagnosticFrame& a, float n) {
    DiagnosticFrame r;
    r.acceptance_rate = a.acceptance_rate / n;
    r.tokens_produced = static_cast<int>(a.tokens_produced / n);
    r.draft_latency_ms = a.draft_latency_ms / n;
    r.verify_latency_ms = a.verify_latency_ms / n;
    r.total_ms = a.total_ms / n;
    r.reward = a.reward / n;
    return r;
}

// ----------------------------------------------------------------------------
// Telemetry Aggregator
// ----------------------------------------------------------------------------
class TelemetryAggregator {
    static constexpr size_t HISTORY_SIZE = 100;
    
    RollingBuffer<float, HISTORY_SIZE> acceptance_rates_;
    RollingBuffer<float, HISTORY_SIZE> rewards_;
    RollingBuffer<double, HISTORY_SIZE> latencies_;
    RollingBuffer<DiagnosticFrame, HISTORY_SIZE> frames_;
    
    std::atomic<uint64_t> total_tokens_{0};
    std::atomic<uint64_t> total_generations_{0};
    std::atomic<double> total_time_ms_{0.0};
    
    std::array<float, 8> expert_scores_;
    std::array<int, 8> expert_trials_;
    std::mutex expert_mtx_;
    
public:
    TelemetryAggregator() {
        expert_scores_.fill(0.5f);
        expert_trials_.fill(0);
    }
    
    void RecordFrame(const DiagnosticFrame& frame) {
        acceptance_rates_.Push(frame.acceptance_rate);
        rewards_.Push(frame.reward);
        latencies_.Push(frame.total_ms);
        frames_.Push(frame);
        
        total_tokens_ += frame.tokens_produced;
        total_generations_++;
        total_time_ms_.store(total_time_ms_.load() + frame.total_ms);
        
        {
            std::lock_guard<std::mutex> lock(expert_mtx_);
            if (frame.expert_id >= 0 && frame.expert_id < 8) {
                expert_scores_[frame.expert_id] = frame.expert_score;
                expert_trials_[frame.expert_id] = frame.expert_trials;
            }
        }
    }
    
    struct Summary {
        float avg_acceptance_rate;
        float min_acceptance_rate;
        float max_acceptance_rate;
        float avg_reward;
        double avg_latency_ms;
        double min_latency_ms;
        double max_latency_ms;
        uint64_t total_tokens;
        uint64_t total_generations;
        double overall_tps;
        float roi;  // Return on investment: speedup vs baseline
        
        // Per-expert stats
        std::array<float, 8> expert_scores;
        std::array<int, 8> expert_trials;
    };
    
    Summary GetSummary(float baseline_tps = 10.0f) const {
        Summary s = {};
        
        s.avg_acceptance_rate = acceptance_rates_.Mean();
        s.min_acceptance_rate = acceptance_rates_.Min();
        s.max_acceptance_rate = acceptance_rates_.Max();
        s.avg_reward = rewards_.Mean();
        s.avg_latency_ms = latencies_.Mean();
        s.min_latency_ms = latencies_.Min();
        s.max_latency_ms = latencies_.Max();
        
        s.total_tokens = total_tokens_.load();
        s.total_generations = total_generations_.load();
        
        double total_time = total_time_ms_.load();
        s.overall_tps = total_time > 0 ? (s.total_tokens / (total_time / 1000.0)) : 0.0;
        
        // ROI: current TPS / baseline TPS
        s.roi = baseline_tps > 0 ? static_cast<float>(s.overall_tps / baseline_tps) : 0.0f;
        
        {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(expert_mtx_));
            s.expert_scores = expert_scores_;
            s.expert_trials = expert_trials_;
        }
        
        return s;
    }
    
    // Get recent frames for real-time display
    std::vector<DiagnosticFrame> GetRecentFrames(size_t count) const {
        // Simplified: return empty for now
        // In real impl, maintain circular buffer of frames
        return {};
    }
};

// ----------------------------------------------------------------------------
// UI Renderer (Win32 GDI)
// ----------------------------------------------------------------------------
#ifdef _WIN32
#include <windows.h>

class TelemetryOverlay {
    HWND hwnd_ = nullptr;
    TelemetryAggregator* aggregator_ = nullptr;
    
    // Colors
    static constexpr COLORREF COLOR_BG = RGB(30, 30, 30);
    static constexpr COLORREF COLOR_TEXT = RGB(200, 200, 200);
    static constexpr COLORREF COLOR_GOOD = RGB(0, 255, 0);
    static constexpr COLORREF COLOR_WARN = RGB(255, 255, 0);
    static constexpr COLORREF COLOR_BAD = RGB(255, 0, 0);
    
public:
    void Initialize(HWND parent) {
        // Create overlay window as child of IDE
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = TEXT("RawrTelemetryOverlay");
        RegisterClass(&wc);
        
        hwnd_ = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TRANSPARENT,
            TEXT("RawrTelemetryOverlay"),
            TEXT("Telemetry"),
            WS_CHILD | WS_VISIBLE,
            10, 10, 400, 200,
            parent, nullptr, GetModuleHandle(nullptr), nullptr
        );
        
        SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 200, LWA_ALPHA);
    }
    
    void SetAggregator(TelemetryAggregator* agg) {
        aggregator_ = agg;
    }
    
    void Render() {
        if (!hwnd_ || !aggregator_) return;
        
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        
        // Clear background
        RECT rc;
        GetClientRect(hwnd_, &rc);
        FillRect(hdc, &rc, CreateSolidBrush(COLOR_BG));
        
        // Get summary
        auto summary = aggregator_->GetSummary();
        
        // Draw metrics
        SetTextColor(hdc, COLOR_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        
        HFONT font = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TEXT("Consolas"));
        SelectObject(hdc, font);
        
        int y = 10;
        int line_height = 18;
        
        // Title
        TextOut(hdc, 10, y, TEXT("=== RAWR TELEMETRY ==="), 22);
        y += line_height * 2;
        
        // Acceptance rate
        COLORREF acc_color = summary.avg_acceptance_rate > 0.7f ? COLOR_GOOD :
                            summary.avg_acceptance_rate > 0.4f ? COLOR_WARN : COLOR_BAD;
        SetTextColor(hdc, acc_color);
        char buf[256];
        snprintf(buf, sizeof(buf), "Acceptance: %.1f%% (%.1f%% - %.1f%%)",
                summary.avg_acceptance_rate * 100,
                summary.min_acceptance_rate * 100,
                summary.max_acceptance_rate * 100);
        TextOutA(hdc, 10, y, buf, strlen(buf));
        y += line_height;
        
        // ROI
        SetTextColor(hdc, summary.roi >= 1.0f ? COLOR_GOOD : COLOR_BAD);
        snprintf(buf, sizeof(buf), "ROI: %.2fx %s", summary.roi,
                summary.roi >= 1.0f ? "(SPEEDUP)" : "(REGRESSION)");
        TextOutA(hdc, 10, y, buf, strlen(buf));
        y += line_height;
        
        // Latency
        SetTextColor(hdc, COLOR_TEXT);
        snprintf(buf, sizeof(buf), "Latency: %.1fms (%.1f - %.1f)",
                summary.avg_latency_ms,
                summary.min_latency_ms,
                summary.max_latency_ms);
        TextOutA(hdc, 10, y, buf, strlen(buf));
        y += line_height;
        
        // TPS
        snprintf(buf, sizeof(buf), "Throughput: %.1f tok/s | Total: %llu",
                summary.overall_tps,
                summary.total_tokens);
        TextOutA(hdc, 10, y, buf, strlen(buf));
        y += line_height * 2;
        
        // Expert scores
        TextOut(hdc, 10, y, TEXT("Expert Scores:"), 14);
        y += line_height;
        
        for (int i = 0; i < 8; i++) {
            COLORREF exp_color = summary.expert_scores[i] > 0.7f ? COLOR_GOOD :
                                summary.expert_scores[i] > 0.4f ? COLOR_WARN : COLOR_BAD;
            SetTextColor(hdc, exp_color);
            snprintf(buf, sizeof(buf), "  E%d: %.2f (%d trials)",
                    i, summary.expert_scores[i], summary.expert_trials[i]);
            TextOutA(hdc, 10, y, buf, strlen(buf));
            y += line_height;
        }
        
        DeleteObject(font);
        EndPaint(hwnd_, &ps);
    }
    
    void Update() {
        if (hwnd_) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
};

#endif // _WIN32

} // namespace Telemetry
} // namespace RawrXD
