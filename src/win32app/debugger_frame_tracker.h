#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <chrono>
#include <optional>
#include <thread>
#include <mutex>
#include <functional>
#include "../core/native_debugger_types.h"
#include "../zccf/debug_ring.h"
#include "debugger_error_handler.h"

using RawrXD::Debugger::NativeStackFrame;

// =============================================================================
// Frame Tracking System
// =============================================================================

/**
 * @brief Frame state enumeration for tracking debugging state
 */
enum class FrameState {
    Unknown = 0,
    Valid = 1,
    InvalidAddress = 2,
    SymbolResolutionFailed = 3,
    LocalsRetrievalFailed = 4,
    Corrupted = 5
};

/**
 * @brief Enhanced frame information with validation and metadata
 */
struct EnhancedStackFrame {
    // Core frame data
    std::string function;
    std::string file;
    int line = 0;
    int dapFrameId = 0;
    
    // Frame validation state
    FrameState state = FrameState::Unknown;
    std::string stateReason;  // Bounded to 256 chars
    
    // Address information (for low-level debugging)
    uint64_t instructionPtr = 0;
    uint64_t stackPtr = 0;
    uint64_t framePtr = 0;
    
    // Locals and watch data
    std::map<std::string, std::string> locals;
    std::map<std::string, std::string> statics;
    bool localsRetrieved = false;
    
    // Metadata for frame selection
    std::chrono::steady_clock::time_point captureTime;
    int displayIndex = 0;  // Position in UI (0-based)
    bool isSelected = false;
    
    // DAP session info (if DAP-based debugging)
    int threadId = 0;
    bool isInlineFrame = false;
    
    /**
     * @brief Validate frame integrity
     * @return true if frame appears valid for navigation
     */
    bool isValid() const noexcept;
    
    /**
     * @brief Get human-readable frame name for UI
     */
    std::string getDisplayName() const noexcept;
};

/**
 * @brief Frame history tracker for undo/redo and navigation
 */
class FrameHistory {
public:
    FrameHistory();
    ~FrameHistory() = default;
    
    /// Add a frame snapshot to history
    void push(const EnhancedStackFrame& frame);
    
    /// Get previous frame in history
    std::optional<EnhancedStackFrame> previous();
    
    /// Get next frame in history
    std::optional<EnhancedStackFrame> next();
    
    /// Check if we can go backward
    bool canGoBack() const noexcept;
    
    /// Check if we can go forward
    bool canGoForward() const noexcept;
    
    /// Clear history and start fresh
    void clear() noexcept;
    
    /// Get current position in history
    size_t currentIndex() const noexcept;
    
    /// Get total history size
    size_t size() const noexcept;

private:
    std::vector<EnhancedStackFrame> history_;
    size_t currentIndex_ = 0;
    static constexpr size_t MAX_HISTORY = 100;
};

/**
 * @brief Main frame tracking system for call stack management
 */
class DebuggerFrameTracker {
public:
    DebuggerFrameTracker();
    ~DebuggerFrameTracker() = default;
    
    // =========================================================================
    // Frame Collection & Validation
    // =========================================================================
    
    /**
     * @brief Update frame list from native engine
     * @param nativeFrames Raw frames from debugger engine
     * @return Number of valid frames collected
     */
    size_t updateFromNativeFrames(const std::vector<NativeStackFrame>& nativeFrames);
    
    /**
     * @brief Update frame list from DAP adapter
     * @param dapFrames Stack frames from DAP protocol
     * @return Number of valid frames collected
     */
    size_t updateFromDapFrames(const std::vector<EnhancedStackFrame>& dapFrames);
    
    /**
     * @brief Validate current frame stack for integrity
     * @return true if all frames pass basic validation
     */
    bool validateStack() noexcept;
    
    /**
     * @brief Try to recover corrupted frame at index
     * @param index Frame index to recover
     * @return true if recovery successful
     */
    bool attemptFrameRecovery(size_t index) noexcept;
    
    // =========================================================================
    // Frame Access & Navigation
    // =========================================================================
    
    /**
     * @brief Get frame count
     */
    size_t frameCount() const noexcept;
    
    /**
     * @brief Get frame at index
     * @param index Frame index (0 = topmost)
     * @return Frame pointer or nullptr if invalid
     */
    const EnhancedStackFrame* getFrame(size_t index) const noexcept;
    
    /**
     * @brief Get mutable frame at index
     */
    EnhancedStackFrame* getFrameMutable(size_t index) noexcept;
    
    /**
     * @brief Get currently selected frame
     */
    const EnhancedStackFrame* getCurrentFrame() const noexcept;
    
    /**
     * @brief Select frame by index
     * @param index Frame index to select
     * @return true if selection succeeded
     */
    bool selectFrame(size_t index) noexcept;
    
    /**
     * @brief Get current frame index
     */
    size_t getCurrentFrameIndex() const noexcept;
    
    // =========================================================================
    // Frame History (for back/forward navigation)
    // =========================================================================
    
    /**
     * @brief Navigate to previous frame in history
     * @return true if navigation successful
     */
    bool navigateFrameBack() noexcept;
    
    /**
     * @brief Navigate to next frame in history
     * @return true if navigation successful
     */
    bool navigateFrameForward() noexcept;
    
    /**
     * @brief Check if back navigation available
     */
    bool canNavigateBack() const noexcept;
    
    /**
     * @brief Check if forward navigation available
     */
    bool canNavigateForward() const noexcept;
    
    // =========================================================================
    // Frame Locals & Variables
    // =========================================================================
    
    /**
     * @brief Retrieve locals for current frame
     * @param outLocals Map to fill with variable names and values
     * @return true if retrieval successful
     */
    bool getFrameLocals(std::map<std::string, std::string>& outLocals) noexcept;
    
    /**
     * @brief Retrieve statics for current frame
     * @param outStatics Map to fill with static variables
     * @return true if retrieval successful
     */
    bool getFrameStatics(std::map<std::string, std::string>& outStatics) noexcept;
    
    /**
     * @brief Update variable value in current frame
     * @param name Variable name
     * @param value New value
     * @return true if update successful
     */
    bool updateFrameVariable(const std::string& name, const std::string& value) noexcept;
    
    // =========================================================================
    // Frame Metadata & Context
    // =========================================================================
    
    /**
     * @brief Set context callback (called when frame changes)
     * @param callback Function called with new frame context
     */
    using FrameContextCallback = std::function<void(const EnhancedStackFrame&)>;
    void setFrameContextCallback(FrameContextCallback callback) noexcept;

    /**
     * @brief Optional callback for routing frame-tracker failures to debugger error handling
     */
    using ErrorReportCallback = std::function<void(
        DebuggerErrorType,
        ErrorSeverity,
        const std::string&,
        const std::string&)>;
    void setErrorReportCallback(ErrorReportCallback callback) noexcept;
    
    /**
     * @brief Get all frames as display-ready list
     * @return Vector of frames suitable for UI rendering
     */
    std::vector<EnhancedStackFrame> getDisplayFrames() const noexcept;
    
    /**
     * @brief Get statistics about frame state
     */
    struct FrameStats {
        size_t totalFrames = 0;
        size_t droppedFrames = 0;
        size_t validFrames = 0;
        size_t invalidFrames = 0;
        size_t corruptedFrames = 0;
        std::chrono::milliseconds lastUpdateTime = std::chrono::milliseconds(0);
    };
    FrameStats getStatistics() const noexcept;
    
    // =========================================================================
    // Frame Debugging/Diagnostics
    // =========================================================================
    
    /**
     * @brief Get diagnostic info for a frame
     * @param index Frame index
     * @return Formatted diagnostic string
     */
    std::string getDiagnostics(size_t index) const noexcept;
    
    /**
     * @brief Get full diagnostic report for entire stack
     */
    std::string getDiagnosticsReport() const noexcept;

    // =========================================================================
    // ZCCF Debug Ring Integration (P1.2)
    // =========================================================================

    /// Latest structured debugger payload for agent consumers.
    std::optional<RawrXD::ZCCF::DebuggerFramePayload> latestDebugPayload() const noexcept;

    /// Structured debugger payloads since sequence (exclusive).
    std::vector<RawrXD::ZCCF::DebuggerFramePayload> debugPayloadsSince(uint64_t afterSeq) const;
    
    /**
     * @brief Clear all frame data
     */
    void clear() noexcept;
    
    /**
     * @brief Check if frame tracker is in valid state
     */
    bool isValid() const noexcept;

private:
    static constexpr size_t MAX_TRACKED_FRAMES = 500;
    std::vector<EnhancedStackFrame> frames_;
    size_t currentFrameIndex_ = 0;
    FrameHistory frameHistory_;
    FrameContextCallback frameContextCallback_;
    std::chrono::steady_clock::time_point lastUpdateTime_;
    
    // State tracking
    bool isValid_ = false;
    int validationErrorCount_ = 0;
    size_t droppedFrameCount_ = 0;
    static constexpr int MAX_VALIDATION_ERRORS = 10;
    
    // Conversion utilities
    void recomputeValidationState() noexcept;
    void reportTrackerError(
        DebuggerErrorType type,
        ErrorSeverity severity,
        const std::string& message,
        const std::string& context) noexcept;
    bool selectFrameInternal(size_t index, bool recordHistory) noexcept;
    EnhancedStackFrame convertFromNativeFrame(const NativeStackFrame& nf) noexcept;
    bool validateFrame(EnhancedStackFrame& frame) noexcept;
    void updateDisplayIndices() noexcept;
    void emitPayloadFromFrame(const EnhancedStackFrame& frame,
                              RawrXD::ZCCF::CaptureMode mode) noexcept;

    ErrorReportCallback errorReportCallback_;
    RawrXD::ZCCF::DebugRing debugRing_;
    std::chrono::steady_clock::time_point ringStartTime_;
};

// =============================================================================
// Frame Tracker Singleton
// =============================================================================

class DebuggerFrameTrackerInstance {
public:
    static DebuggerFrameTracker& instance() noexcept;
    
private:
    DebuggerFrameTrackerInstance() = delete;
};
