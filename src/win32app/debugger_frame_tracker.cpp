// Win32IDE_Types.h must come before debugger_frame_tracker.h: STL (e.g. <functional>) before
// WinSDK commctrl/prsht breaks UINT/CALLBACK parsing on MSVC (/std:c++latest).
#include "Win32IDE_Types.h"
#include "debugger_frame_tracker.h"
#include <sstream>
#include <algorithm>
#include <cstring>

// =============================================================================
// EnhancedStackFrame Implementation
// =============================================================================

bool EnhancedStackFrame::isValid() const noexcept {
    // Frame is valid if:
    // 1. Has valid state
    // 2. Has non-empty function or file
    // 3. Line number is reasonable (or instructionPtr for low-level debugging)
    
    if (state != FrameState::Valid) {
        return false;
    }
    
    if (function.empty() && file.empty() && instructionPtr == 0) {
        return false;
    }
    
    if (line < 0 || line > 100000) {  // Reasonable upper bound
        return false;
    }
    
    return true;
}

std::string EnhancedStackFrame::getDisplayName() const noexcept {
    try {
        std::ostringstream oss;

        if (!function.empty()) {
            oss << function;
        } else if (instructionPtr > 0) {
            oss << "0x" << std::hex << instructionPtr;
        } else {
            oss << "<unknown>";
        }

        if (!file.empty()) {
            oss << " (" << file << ":" << line << ")";
        }

        return oss.str();
    } catch (...) {
        if (!function.empty()) return function;
        if (instructionPtr > 0) return "<addr>";
        return "<unknown>";
    }
}

// =============================================================================
// FrameHistory Implementation
// =============================================================================

namespace {

bool sameFrameIdentity(const EnhancedStackFrame& lhs, const EnhancedStackFrame& rhs) noexcept {
    return lhs.function == rhs.function &&
           lhs.file == rhs.file &&
           lhs.line == rhs.line &&
           lhs.dapFrameId == rhs.dapFrameId &&
           lhs.instructionPtr == rhs.instructionPtr;
}

}  // namespace

FrameHistory::FrameHistory() : currentIndex_(0) {}

void FrameHistory::push(const EnhancedStackFrame& frame) {
    if (!history_.empty() && currentIndex_ < history_.size() &&
        sameFrameIdentity(history_[currentIndex_], frame)) {
        return;
    }

    // Trim forward history when adding new entry
    if (!history_.empty() && currentIndex_ + 1 < history_.size()) {
        history_.erase(history_.begin() + currentIndex_ + 1, history_.end());
    }
    
    // Maintain max history size
    if (history_.size() >= MAX_HISTORY) {
        history_.erase(history_.begin());
        if (currentIndex_ > 0) {
            --currentIndex_;
        }
    }
    
    history_.push_back(frame);
    currentIndex_ = history_.size() - 1;
}

std::optional<EnhancedStackFrame> FrameHistory::previous() {
    if (!canGoBack()) {
        return std::nullopt;
    }
    --currentIndex_;
    return history_[currentIndex_];
}

std::optional<EnhancedStackFrame> FrameHistory::next() {
    if (!canGoForward()) {
        return std::nullopt;
    }
    ++currentIndex_;
    return history_[currentIndex_];
}

bool FrameHistory::canGoBack() const noexcept {
    return currentIndex_ > 0;
}

bool FrameHistory::canGoForward() const noexcept {
    return !history_.empty() && (currentIndex_ + 1 < history_.size());
}

void FrameHistory::clear() noexcept {
    history_.clear();
    currentIndex_ = 0;
}

size_t FrameHistory::currentIndex() const noexcept {
    return currentIndex_;
}

size_t FrameHistory::size() const noexcept {
    return history_.size();
}

// =============================================================================
// DebuggerFrameTracker Implementation
// =============================================================================

DebuggerFrameTracker::DebuggerFrameTracker()
        : currentFrameIndex_(0), isValid_(false), validationErrorCount_(0), droppedFrameCount_(0),
            debugRing_(RawrXD::ZCCF::DebugRing::kDefaultCapacity) {
    lastUpdateTime_ = std::chrono::steady_clock::now();
        ringStartTime_ = lastUpdateTime_;
}

namespace {

bool matchesFrameIdentity(const EnhancedStackFrame& candidate,
                          const EnhancedStackFrame& reference) noexcept {
    return candidate.function == reference.function &&
           candidate.file == reference.file &&
           candidate.line == reference.line &&
           candidate.instructionPtr == reference.instructionPtr &&
           candidate.dapFrameId == reference.dapFrameId;
}

bool matchesFrameFallbackIdentity(const EnhancedStackFrame& candidate,
                      const EnhancedStackFrame& reference) noexcept {
    return candidate.function == reference.function &&
        candidate.file == reference.file;
}

} // namespace

size_t DebuggerFrameTracker::updateFromNativeFrames(
    const std::vector<NativeStackFrame>& nativeFrames) {
    try {
        droppedFrameCount_ = 0;
        EnhancedStackFrame previousSelection;
        bool hadPreviousSelection = false;
        if (currentFrameIndex_ < frames_.size()) {
            try {
                previousSelection = frames_[currentFrameIndex_];
                hadPreviousSelection = true;
            } catch (...) {
                hadPreviousSelection = false;
            }
        }

        frames_.clear();
        currentFrameIndex_ = 0;
        isValid_ = false;
        validationErrorCount_ = 0;
        size_t validFrameCount = 0;
        
        if (nativeFrames.empty()) {
            reportTrackerError(
                DebuggerErrorType::FrameStackWalkFailed,
                ErrorSeverity::Warning,
                "Native frame update returned an empty stack",
                "DebuggerFrameTracker::updateFromNativeFrames");
            return 0;
        }

        const size_t frameLimit = std::min(nativeFrames.size(), MAX_TRACKED_FRAMES);
        if (nativeFrames.size() > frameLimit) {
            droppedFrameCount_ = nativeFrames.size() - frameLimit;
            reportTrackerError(
                DebuggerErrorType::FrameStackWalkFailed,
                ErrorSeverity::Warning,
                "Native frame update exceeded tracking limit; some frames were dropped",
                "DebuggerFrameTracker::updateFromNativeFrames");
        }
        frames_.reserve(frameLimit);
        
        // Convert native frames to enhanced frames
        for (size_t i = 0; i < frameLimit; ++i) {
            const auto& nf = nativeFrames[i];
            auto ef = convertFromNativeFrame(nf);
            
            if (validateFrame(ef)) {
                frames_.push_back(ef);
                ++validFrameCount;
            } else {
                // Even invalid frames are stored, but marked as such
                frames_.push_back(ef);
                ++validationErrorCount_;
            }
        }
        
        if (hadPreviousSelection) {
            for (size_t i = 0; i < frames_.size(); ++i) {
                if (matchesFrameIdentity(frames_[i], previousSelection)) {
                    currentFrameIndex_ = i;
                    break;
                }
            }
        }

        // If selection landed on an invalid frame after refresh, prefer the first
        // valid frame so dependent UI panes (locals/watch) keep a usable context.
        if (!frames_.empty() &&
            (currentFrameIndex_ >= frames_.size() ||
             frames_[currentFrameIndex_].state != FrameState::Valid)) {
            for (size_t i = 0; i < frames_.size(); ++i) {
                if (frames_[i].state == FrameState::Valid) {
                    currentFrameIndex_ = i;
                    break;
                }
            }
        }

        updateDisplayIndices();
        lastUpdateTime_ = std::chrono::steady_clock::now();
        recomputeValidationState();

        if (const auto* cur = getCurrentFrame()) {
            emitPayloadFromFrame(*cur, RawrXD::ZCCF::CaptureMode::Stopped);
        }

        if (validFrameCount == 0 && !frames_.empty()) {
            reportTrackerError(
                DebuggerErrorType::FrameDataCorrupted,
                ErrorSeverity::Error,
                "Native frame update produced no valid frames",
                "DebuggerFrameTracker::updateFromNativeFrames");
        }
        
        return validFrameCount;
    } catch (...) {
        frames_.clear();
        currentFrameIndex_ = 0;
        isValid_ = false;
        validationErrorCount_ = 0;
        reportTrackerError(
            DebuggerErrorType::FrameStackWalkFailed,
            ErrorSeverity::Error,
            "Native frame update failed due to unexpected exception",
            "DebuggerFrameTracker::updateFromNativeFrames");
        return 0;
    }
}

size_t DebuggerFrameTracker::updateFromDapFrames(
    const std::vector<EnhancedStackFrame>& dapFrames) {
    try {
        droppedFrameCount_ = 0;
        EnhancedStackFrame previousSelection;
        bool hadPreviousSelection = false;
        if (currentFrameIndex_ < frames_.size()) {
            try {
                previousSelection = frames_[currentFrameIndex_];
                hadPreviousSelection = true;
            } catch (...) {
                hadPreviousSelection = false;
            }
        }

        frames_.clear();
        currentFrameIndex_ = 0;
        isValid_ = false;
        validationErrorCount_ = 0;
        size_t validFrameCount = 0;
        
        if (dapFrames.empty()) {
            reportTrackerError(
                DebuggerErrorType::FrameStackWalkFailed,
                ErrorSeverity::Warning,
                "DAP frame update returned an empty stack",
                "DebuggerFrameTracker::updateFromDapFrames");
            return 0;
        }

        const size_t frameLimit = std::min(dapFrames.size(), MAX_TRACKED_FRAMES);
        if (dapFrames.size() > frameLimit) {
            droppedFrameCount_ = dapFrames.size() - frameLimit;
            reportTrackerError(
                DebuggerErrorType::FrameStackWalkFailed,
                ErrorSeverity::Warning,
                "DAP frame update exceeded tracking limit; some frames were dropped",
                "DebuggerFrameTracker::updateFromDapFrames");
        }
        frames_.reserve(frameLimit);
        
        // Copy and validate DAP frames
        for (size_t i = 0; i < frameLimit; ++i) {
            auto ef = dapFrames[i];
            
            if (validateFrame(ef)) {
                frames_.push_back(ef);
                ++validFrameCount;
            } else {
                frames_.push_back(ef);
                ++validationErrorCount_;
            }
        }
        
        if (hadPreviousSelection) {
            for (size_t i = 0; i < frames_.size(); ++i) {
                if (matchesFrameIdentity(frames_[i], previousSelection)) {
                    currentFrameIndex_ = i;
                    break;
                }
            }
        }

        // If selection landed on an invalid frame after refresh, prefer the first
        // valid frame so dependent UI panes (locals/watch) keep a usable context.
        if (!frames_.empty() &&
            (currentFrameIndex_ >= frames_.size() ||
             frames_[currentFrameIndex_].state != FrameState::Valid)) {
            for (size_t i = 0; i < frames_.size(); ++i) {
                if (frames_[i].state == FrameState::Valid) {
                    currentFrameIndex_ = i;
                    break;
                }
            }
        }

        updateDisplayIndices();
        lastUpdateTime_ = std::chrono::steady_clock::now();
        recomputeValidationState();

        if (const auto* cur = getCurrentFrame()) {
            emitPayloadFromFrame(*cur, RawrXD::ZCCF::CaptureMode::SoftSample);
        }

        if (validFrameCount == 0 && !frames_.empty()) {
            reportTrackerError(
                DebuggerErrorType::FrameDataCorrupted,
                ErrorSeverity::Error,
                "DAP frame update produced no valid frames",
                "DebuggerFrameTracker::updateFromDapFrames");
        }
        
        return validFrameCount;
    } catch (...) {
        frames_.clear();
        currentFrameIndex_ = 0;
        isValid_ = false;
        validationErrorCount_ = 0;
        reportTrackerError(
            DebuggerErrorType::FrameStackWalkFailed,
            ErrorSeverity::Error,
            "DAP frame update failed due to unexpected exception",
            "DebuggerFrameTracker::updateFromDapFrames");
        return 0;
    }
}

bool DebuggerFrameTracker::validateStack() noexcept {
    if (frames_.empty()) {
        isValid_ = false;
        validationErrorCount_ = 0;
        reportTrackerError(
            DebuggerErrorType::FrameStackWalkFailed,
            ErrorSeverity::Warning,
            "Frame validation failed: stack is empty",
            "DebuggerFrameTracker::validateStack");
        return false;
    }

    validationErrorCount_ = 0;
    for (auto& frame : frames_) {
        if (!validateFrame(frame)) {
            ++validationErrorCount_;
        }
    }

    recomputeValidationState();
    return validationErrorCount_ == 0;
}

bool DebuggerFrameTracker::attemptFrameRecovery(size_t index) noexcept {
    try {
        if (index >= frames_.size()) {
            reportTrackerError(
                DebuggerErrorType::FrameIndexOutOfRange,
                ErrorSeverity::Error,
                "Frame recovery failed: index out of range",
                "DebuggerFrameTracker::attemptFrameRecovery");
            return false;
        }

        auto& frame = frames_[index];

        // Recovery strategy 1: If function is empty but we have instruction ptr,
        // format it as hex address
        if (frame.function.empty() && frame.instructionPtr > 0) {
            std::ostringstream oss;
            oss << "0x" << std::hex << frame.instructionPtr;
            frame.function = oss.str();
            frame.state = FrameState::Valid;
            frame.stateReason.clear();
            recomputeValidationState();
            return true;
        }

        // Recovery strategy 2: If we have file but no line, mark line as 0
        if (!frame.file.empty() && frame.line < 0) {
            frame.line = 0;
            frame.state = FrameState::Valid;
            frame.stateReason.clear();
            recomputeValidationState();
            return true;
        }

        // Recovery strategy 3: If state is marked invalid but data is present,
        // try to salvage it
        if (frame.state != FrameState::Valid &&
            (!frame.function.empty() || !frame.file.empty())) {
            frame.state = FrameState::Valid;
            frame.stateReason.clear();
            recomputeValidationState();
            return true;
        }

        reportTrackerError(
            DebuggerErrorType::FrameDataCorrupted,
            ErrorSeverity::Warning,
            "Frame recovery failed: no applicable recovery strategy",
            "DebuggerFrameTracker::attemptFrameRecovery");
        return false;
    } catch (...) {
        reportTrackerError(
            DebuggerErrorType::FrameDataCorrupted,
            ErrorSeverity::Error,
            "Frame recovery failed due to unexpected exception",
            "DebuggerFrameTracker::attemptFrameRecovery");
        return false;
    }
}

size_t DebuggerFrameTracker::frameCount() const noexcept {
    return frames_.size();
}

const EnhancedStackFrame* DebuggerFrameTracker::getFrame(size_t index) const noexcept {
    if (index >= frames_.size()) {
        return nullptr;
    }
    return &frames_[index];
}

EnhancedStackFrame* DebuggerFrameTracker::getFrameMutable(size_t index) noexcept {
    if (index >= frames_.size()) {
        return nullptr;
    }
    return &frames_[index];
}

const EnhancedStackFrame* DebuggerFrameTracker::getCurrentFrame() const noexcept {
    if (currentFrameIndex_ >= frames_.size()) {
        return nullptr;
    }

    const auto& frame = frames_[currentFrameIndex_];
    if (frame.state != FrameState::Valid) {
        return nullptr;
    }

    return &frame;
}

bool DebuggerFrameTracker::selectFrameInternal(size_t index, bool recordHistory) noexcept {
    if (index >= frames_.size()) {
        reportTrackerError(
            DebuggerErrorType::FrameIndexOutOfRange,
            ErrorSeverity::Error,
            "Frame selection index out of range",
            "DebuggerFrameTracker::selectFrameInternal");
        return false;
    }

    if (frames_[index].state != FrameState::Valid && !attemptFrameRecovery(index)) {
        reportTrackerError(
            DebuggerErrorType::FrameDataCorrupted,
            ErrorSeverity::Error,
            "Frame selection failed: unrecoverable frame state",
            "DebuggerFrameTracker::selectFrameInternal");
        return false;
    }

    if (frames_[index].state != FrameState::Valid) {
        reportTrackerError(
            DebuggerErrorType::FrameNavigationFailed,
            ErrorSeverity::Error,
            "Frame selection failed: target frame is not valid",
            "DebuggerFrameTracker::selectFrameInternal");
        return false;
    }

    const size_t previousIndex = currentFrameIndex_;
    const bool selectionChanged = (currentFrameIndex_ != index);
    currentFrameIndex_ = index;
    updateDisplayIndices();

    if (recordHistory && selectionChanged) {
        try {
            // Seed history with the previous valid selection once so the first
            // user frame change can navigate back to the original frame.
            if (frameHistory_.size() == 0 &&
                previousIndex < frames_.size() &&
                frames_[previousIndex].state == FrameState::Valid) {
                frameHistory_.push(frames_[previousIndex]);
            }
            frameHistory_.push(frames_[index]);
        } catch (...) {
            // Best-effort only; selection itself should still succeed.
        }
    }
    
    if (frameContextCallback_) {
        try {
            frameContextCallback_(frames_[index]);
        } catch (...) {
            // Swallow callback exceptions
        }
    }

    emitPayloadFromFrame(frames_[index], RawrXD::ZCCF::CaptureMode::Breakpoint);
    
    return true;
}

bool DebuggerFrameTracker::selectFrame(size_t index) noexcept {
    return selectFrameInternal(index, true);
}

size_t DebuggerFrameTracker::getCurrentFrameIndex() const noexcept {
    return currentFrameIndex_;
}

bool DebuggerFrameTracker::navigateFrameBack() noexcept {
    bool movedHistoryCursor = false;
    try {
        auto prevFrame = frameHistory_.previous();
        if (!prevFrame.has_value()) {
            return false;
        }
        movedHistoryCursor = true;

        // Find frame in frames_ vector by strongest identity first.
        for (size_t i = 0; i < frames_.size(); ++i) {
            if (matchesFrameIdentity(frames_[i], *prevFrame)) {
                if (selectFrameInternal(i, false)) {
                    return true;
                }
                (void)frameHistory_.next();
                reportTrackerError(
                    DebuggerErrorType::FrameNavigationFailed,
                    ErrorSeverity::Warning,
                    "Frame back navigation failed while selecting matched frame",
                    "DebuggerFrameTracker::navigateFrameBack");
                return false;
            }
        }

        // Fallback identity match for adapters/symbolizers that omit some fields.
        for (size_t i = 0; i < frames_.size(); ++i) {
            if (matchesFrameFallbackIdentity(frames_[i], *prevFrame)) {
                if (selectFrameInternal(i, false)) {
                    return true;
                }
                (void)frameHistory_.next();
                reportTrackerError(
                    DebuggerErrorType::FrameNavigationFailed,
                    ErrorSeverity::Warning,
                    "Frame back navigation failed while selecting fallback-matched frame",
                    "DebuggerFrameTracker::navigateFrameBack");
                return false;
            }
        }

        // Restore history position when navigation target cannot be resolved.
        (void)frameHistory_.next();
        reportTrackerError(
            DebuggerErrorType::FrameNavigationFailed,
            ErrorSeverity::Warning,
            "Frame back navigation failed: no matching target in current stack",
            "DebuggerFrameTracker::navigateFrameBack");
        return false;
    } catch (...) {
        if (movedHistoryCursor) {
            (void)frameHistory_.next();
        }
        reportTrackerError(
            DebuggerErrorType::FrameNavigationFailed,
            ErrorSeverity::Error,
            "Frame back navigation failed due to unexpected exception",
            "DebuggerFrameTracker::navigateFrameBack");
        return false;
    }
}

bool DebuggerFrameTracker::navigateFrameForward() noexcept {
    bool movedHistoryCursor = false;
    try {
        auto nextFrame = frameHistory_.next();
        if (!nextFrame.has_value()) {
            return false;
        }
        movedHistoryCursor = true;

        for (size_t i = 0; i < frames_.size(); ++i) {
            if (matchesFrameIdentity(frames_[i], *nextFrame)) {
                if (selectFrameInternal(i, false)) {
                    return true;
                }
                (void)frameHistory_.previous();
                reportTrackerError(
                    DebuggerErrorType::FrameNavigationFailed,
                    ErrorSeverity::Warning,
                    "Frame forward navigation failed while selecting matched frame",
                    "DebuggerFrameTracker::navigateFrameForward");
                return false;
            }
        }

        for (size_t i = 0; i < frames_.size(); ++i) {
            if (matchesFrameFallbackIdentity(frames_[i], *nextFrame)) {
                if (selectFrameInternal(i, false)) {
                    return true;
                }
                (void)frameHistory_.previous();
                reportTrackerError(
                    DebuggerErrorType::FrameNavigationFailed,
                    ErrorSeverity::Warning,
                    "Frame forward navigation failed while selecting fallback-matched frame",
                    "DebuggerFrameTracker::navigateFrameForward");
                return false;
            }
        }

        // Restore history position when navigation target cannot be resolved.
        (void)frameHistory_.previous();
        reportTrackerError(
            DebuggerErrorType::FrameNavigationFailed,
            ErrorSeverity::Warning,
            "Frame forward navigation failed: no matching target in current stack",
            "DebuggerFrameTracker::navigateFrameForward");
        return false;
    } catch (...) {
        if (movedHistoryCursor) {
            (void)frameHistory_.previous();
        }
        reportTrackerError(
            DebuggerErrorType::FrameNavigationFailed,
            ErrorSeverity::Error,
            "Frame forward navigation failed due to unexpected exception",
            "DebuggerFrameTracker::navigateFrameForward");
        return false;
    }
}

bool DebuggerFrameTracker::canNavigateBack() const noexcept {
    return frameHistory_.canGoBack();
}

bool DebuggerFrameTracker::canNavigateForward() const noexcept {
    return frameHistory_.canGoForward();
}

bool DebuggerFrameTracker::getFrameLocals(
    std::map<std::string, std::string>& outLocals) noexcept {
    try {
        const auto* frame = getCurrentFrame();
        if (!frame || frame->state != FrameState::Valid) {
            reportTrackerError(
                DebuggerErrorType::FrameLocalsUnavailable,
                ErrorSeverity::Warning,
                "Failed to retrieve locals: no valid current frame",
                "DebuggerFrameTracker::getFrameLocals");
            return false;
        }

        outLocals = frame->locals;
        return true;
    } catch (...) {
        reportTrackerError(
            DebuggerErrorType::FrameLocalsUnavailable,
            ErrorSeverity::Error,
            "Failed to retrieve locals due to unexpected exception",
            "DebuggerFrameTracker::getFrameLocals");
        return false;
    }
}

bool DebuggerFrameTracker::getFrameStatics(
    std::map<std::string, std::string>& outStatics) noexcept {
    try {
        const auto* frame = getCurrentFrame();
        if (!frame || frame->state != FrameState::Valid) {
            reportTrackerError(
                DebuggerErrorType::FrameLocalsUnavailable,
                ErrorSeverity::Warning,
                "Failed to retrieve statics: no valid current frame",
                "DebuggerFrameTracker::getFrameStatics");
            return false;
        }

        outStatics = frame->statics;
        return true;
    } catch (...) {
        reportTrackerError(
            DebuggerErrorType::FrameLocalsUnavailable,
            ErrorSeverity::Error,
            "Failed to retrieve statics due to unexpected exception",
            "DebuggerFrameTracker::getFrameStatics");
        return false;
    }
}

bool DebuggerFrameTracker::updateFrameVariable(
    const std::string& name, const std::string& value) noexcept {
    try {
        auto* frame = getFrameMutable(currentFrameIndex_);
        if (!frame || frame->state != FrameState::Valid) {
            reportTrackerError(
                DebuggerErrorType::FrameLocalsUnavailable,
                ErrorSeverity::Warning,
                "Failed to update frame variable: no valid current frame",
                "DebuggerFrameTracker::updateFrameVariable");
            return false;
        }

        frame->locals[name] = value;
        return true;
    } catch (...) {
        reportTrackerError(
            DebuggerErrorType::FrameLocalsUnavailable,
            ErrorSeverity::Error,
            "Failed to update frame variable due to unexpected exception",
            "DebuggerFrameTracker::updateFrameVariable");
        return false;
    }
}

void DebuggerFrameTracker::setFrameContextCallback(
    FrameContextCallback callback) noexcept {
    try {
        frameContextCallback_ = std::move(callback);
    } catch (...) {
        // Preserve previous callback if assignment fails.
    }
}

void DebuggerFrameTracker::setErrorReportCallback(
    ErrorReportCallback callback) noexcept {
    try {
        errorReportCallback_ = std::move(callback);
    } catch (...) {
        // Preserve previous callback if assignment fails.
    }
}

void DebuggerFrameTracker::reportTrackerError(
    DebuggerErrorType type,
    ErrorSeverity severity,
    const std::string& message,
    const std::string& context) noexcept {
    if (!errorReportCallback_) {
        return;
    }
    try {
        errorReportCallback_(type, severity, message, context);
    } catch (...) {
        // Best effort only; tracker must remain non-throwing.
    }
}

std::vector<EnhancedStackFrame> DebuggerFrameTracker::getDisplayFrames() const noexcept {
    try {
        return frames_;
    } catch (...) {
        return {};
    }
}

DebuggerFrameTracker::FrameStats DebuggerFrameTracker::getStatistics() const noexcept {
    try {
        FrameStats stats;
        stats.totalFrames = frames_.size();
        stats.droppedFrames = droppedFrameCount_;
        for (const auto& frame : frames_) {
            if (frame.state == FrameState::Valid) {
                ++stats.validFrames;
            } else {
                ++stats.invalidFrames;
                if (frame.state == FrameState::Corrupted) {
                    ++stats.corruptedFrames;
                }
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - lastUpdateTime_;
        stats.lastUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

        return stats;
    } catch (...) {
        FrameStats fallback;
        fallback.totalFrames = frames_.size();
        fallback.droppedFrames = droppedFrameCount_;
        fallback.validFrames = 0;
        fallback.invalidFrames = 0;
        fallback.corruptedFrames = 0;
        fallback.lastUpdateTime = std::chrono::milliseconds(0);
        return fallback;
    }
}

std::string DebuggerFrameTracker::getDiagnostics(size_t index) const noexcept {
    try {
        if (index >= frames_.size()) {
            return "Frame index out of range";
        }

        const auto& frame = frames_[index];
        std::ostringstream oss;

        oss << "Frame [" << index << "]\n"
            << "  Function: " << (frame.function.empty() ? "<unknown>" : frame.function) << "\n"
            << "  File: " << (frame.file.empty() ? "<unknown>" : frame.file) << "\n"
            << "  Line: " << frame.line << "\n"
            << "  State: ";

        switch (frame.state) {
            case FrameState::Valid:
                oss << "Valid\n";
                break;
            case FrameState::InvalidAddress:
                oss << "Invalid Address\n";
                break;
            case FrameState::SymbolResolutionFailed:
                oss << "Symbol Resolution Failed\n";
                break;
            case FrameState::LocalsRetrievalFailed:
                oss << "Locals Retrieval Failed\n";
                break;
            case FrameState::Corrupted:
                oss << "Corrupted\n";
                break;
            default:
                oss << "Unknown\n";
                break;
        }

        if (!frame.stateReason.empty()) {
            oss << "  Reason: " << frame.stateReason << "\n";
        }

        oss << "  InstructionPtr: 0x" << std::hex << frame.instructionPtr << "\n"
            << "  Locals: " << std::dec << frame.locals.size() << "\n";

        return oss.str();
    } catch (...) {
        return "Frame diagnostics unavailable";
    }
}

std::string DebuggerFrameTracker::getDiagnosticsReport() const noexcept {
    try {
        std::ostringstream oss;

        oss << "=== Debugger Frame Diagnostics Report ===\n"
            << "Total Frames: " << frames_.size() << "\n"
            << "Dropped Frames: " << droppedFrameCount_ << "\n"
            << "Current Frame Index: " << currentFrameIndex_ << "\n"
            << "Valid State: " << (isValid_ ? "YES" : "NO") << "\n"
            << "Validation Errors: " << validationErrorCount_ << "\n"
            << "\n";

        for (size_t i = 0; i < frames_.size(); ++i) {
            oss << getDiagnostics(i) << "\n";
        }

        return oss.str();
    } catch (...) {
        return "=== Debugger Frame Diagnostics Report ===\n<unavailable>\n";
    }
}

std::optional<RawrXD::ZCCF::DebuggerFramePayload>
DebuggerFrameTracker::latestDebugPayload() const noexcept {
    return debugRing_.Latest();
}

std::vector<RawrXD::ZCCF::DebuggerFramePayload>
DebuggerFrameTracker::debugPayloadsSince(uint64_t afterSeq) const {
    return debugRing_.Since(afterSeq);
}

void DebuggerFrameTracker::clear() noexcept {
    try {
        frames_.clear();
        currentFrameIndex_ = 0;
        frameHistory_.clear();
        isValid_ = false;
        validationErrorCount_ = 0;
        droppedFrameCount_ = 0;
        frameContextCallback_ = nullptr;
        errorReportCallback_ = nullptr;
        lastUpdateTime_ = std::chrono::steady_clock::now();
    } catch (...) {
        // Preserve noexcept contract; leave object in best-effort cleared state.
        currentFrameIndex_ = 0;
        isValid_ = false;
        validationErrorCount_ = 0;
    }
}

bool DebuggerFrameTracker::isValid() const noexcept {
    return isValid_;
}

void DebuggerFrameTracker::recomputeValidationState() noexcept {
    try {
        size_t validFrameCount = 0;
        validationErrorCount_ = 0;
        size_t firstValidFrameIndex = 0;
        bool foundValidFrame = false;

        for (size_t i = 0; i < frames_.size(); ++i) {
            const auto& frame = frames_[i];
            if (frame.state == FrameState::Valid) {
                ++validFrameCount;
                if (!foundValidFrame) {
                    firstValidFrameIndex = i;
                    foundValidFrame = true;
                }
            } else {
                ++validationErrorCount_;
            }
        }

        if (!frames_.empty() && currentFrameIndex_ >= frames_.size()) {
            currentFrameIndex_ = frames_.size() - 1;
        }

        if (foundValidFrame &&
            (frames_.empty() || frames_[currentFrameIndex_].state != FrameState::Valid)) {
            currentFrameIndex_ = firstValidFrameIndex;
        }

        isValid_ = validFrameCount > 0;
        updateDisplayIndices();
    } catch (...) {
        isValid_ = false;
        validationErrorCount_ = static_cast<int>(frames_.size());
        currentFrameIndex_ = 0;
    }
}

EnhancedStackFrame DebuggerFrameTracker::convertFromNativeFrame(
    const NativeStackFrame& nf) noexcept {
    EnhancedStackFrame ef;
    try {
        ef.function = nf.function;
        ef.file = nf.sourceFile;
        ef.line = nf.sourceLine;
        ef.instructionPtr = nf.instructionPtr;
        ef.stackPtr = nf.stackPointer;
        ef.framePtr = nf.framePointer;
        ef.locals = nf.locals;
        ef.captureTime = std::chrono::steady_clock::now();
        ef.state = FrameState::Unknown;
    } catch (...) {
        ef.function.clear();
        ef.file.clear();
        ef.line = 0;
        ef.instructionPtr = 0;
        ef.stackPtr = 0;
        ef.framePtr = 0;
        ef.locals.clear();
        ef.captureTime = std::chrono::steady_clock::now();
        ef.state = FrameState::Corrupted;
        ef.stateReason = "Allocation/conversion failure";
    }

    return ef;
}

bool DebuggerFrameTracker::validateFrame(EnhancedStackFrame& frame) noexcept {
    try {
        // Basic validation checks

        // Check 1: Must have some identifying information
        if (frame.function.empty() && frame.file.empty() && frame.instructionPtr == 0) {
            frame.state = FrameState::Corrupted;
            frame.stateReason = "No identifying information";
            return false;
        }

        // Check 2: Function name reasonable length
        if (frame.function.size() > 512) {
            frame.state = FrameState::Corrupted;
            frame.stateReason = "Function name too long";
            return false;
        }

        // Check 3: File path reasonable length
        if (frame.file.size() > 1024) {
            frame.state = FrameState::Corrupted;
            frame.stateReason = "File path too long";
            return false;
        }

        // Check 4: Line number in reasonable range
        if (frame.line < 0 || frame.line > 100000) {
            if (frame.instructionPtr == 0) {
                frame.state = FrameState::InvalidAddress;
                frame.stateReason = "Invalid line number and no instruction pointer";
                return false;
            }
            frame.line = 0;  // Reset to 0 for safety
        }

        frame.state = FrameState::Valid;
        frame.stateReason.clear();
        return true;
    } catch (...) {
        frame.state = FrameState::Corrupted;
        frame.stateReason.clear();
        return false;
    }
}

void DebuggerFrameTracker::updateDisplayIndices() noexcept {
    try {
        for (size_t i = 0; i < frames_.size(); ++i) {
            frames_[i].displayIndex = static_cast<int>(i);
            frames_[i].isSelected = (i == currentFrameIndex_);
        }
    } catch (...) {
        // Best effort only; preserve noexcept contract.
    }
}

void DebuggerFrameTracker::emitPayloadFromFrame(
    const EnhancedStackFrame& frame,
    RawrXD::ZCCF::CaptureMode mode) noexcept {
    try {
        RawrXD::ZCCF::DebuggerFramePayload p{};
        p.rip = frame.instructionPtr;
        p.rsp = frame.stackPtr;
        p.rbp = frame.framePtr;
        p.returnIps[0] = frame.instructionPtr;
        p.stackPeekBytes = 0;
        p.moduleHandle = 0;
        p.symbolHandle = 0;
        p.confidence = (frame.state == FrameState::Valid) ? 1.0f : 0.35f;
        p.captureMode = mode;
        p.timestampUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - ringStartTime_).count());
        p.threadId = static_cast<uint32_t>(frame.threadId);
        p.processId = ::GetCurrentProcessId();

        debugRing_.Push(std::move(p));
    } catch (...) {
        // Best effort only; frame tracker remains non-throwing.
    }
}

// =============================================================================
// Singleton Implementation
// =============================================================================

static DebuggerFrameTracker g_frameTracker;

DebuggerFrameTracker& DebuggerFrameTrackerInstance::instance() noexcept {
    return g_frameTracker;
}
