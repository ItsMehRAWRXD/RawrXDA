// ============================================================================
// agentic_composer_ux.cpp — Agentic Composer UX Polish Implementation
// ============================================================================
// Full implementation of Cursor-like agentic composer UX orchestration.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "agentic/agentic_composer_ux.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Utility
// ============================================================================
static uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
AgenticComposerUX::AgenticComposerUX() = default;
AgenticComposerUX::~AgenticComposerUX() { Shutdown(); }

void AgenticComposerUX::Initialize(const ComposerUICallbacks& callbacks) {
    if (m_initialized.load()) return;
    m_callbacks = callbacks;
    m_initialized.store(true);
}

void AgenticComposerUX::Shutdown() {
    if (!m_initialized.load()) return;
    m_initialized.store(false);
}

// ============================================================================
// Session Management
// ============================================================================
uint64_t AgenticComposerUX::StartSession(const std::string& prompt,
                                          const std::string& model) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);

    // Archive current session if exists
    if (m_currentSession.sessionId != 0) {
        m_sessionHistory.push_back(m_currentSession);
    }

    m_currentSession = {};
    m_currentSession.sessionId = m_nextSessionId.fetch_add(1);
    m_currentSession.title = prompt.substr(0, 80);
    m_currentSession.startedMs = NowMs();
    m_currentSession.lastActivityMs = m_currentSession.startedMs;
    m_currentSession.modelName = model;
    m_currentSession.currentStep = 0;
    m_currentSession.totalSteps = 0;
    m_currentSession.completedSteps = 0;
    m_currentSession.pendingApprovals = 0;
    m_currentSession.approvedChanges = 0;
    m_currentSession.rejectedChanges = 0;
    m_currentSession.totalTokensIn = 0;
    m_currentSession.totalTokensOut = 0;
    m_currentSession.avgLatencyMs = 0.0f;

    if (m_callbacks.onStatusChange) {
        m_callbacks.onStatusChange("Session started", prompt);
    }

    return m_currentSession.sessionId;
}

void AgenticComposerUX::EndSession(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    if (m_currentSession.sessionId == sessionId) {
        m_sessionHistory.push_back(m_currentSession);
        m_currentSession = {};
    }
}

ComposerSession* AgenticComposerUX::GetCurrentSession() {
    return &m_currentSession;
}

const ComposerSession* AgenticComposerUX::GetCurrentSession() const {
    return &m_currentSession;
}

std::vector<ComposerSession> AgenticComposerUX::GetSessionHistory(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    size_t start = (m_sessionHistory.size() > maxCount) 
        ? m_sessionHistory.size() - maxCount : 0;
    return std::vector<ComposerSession>(m_sessionHistory.begin() + start,
                                        m_sessionHistory.end());
}

// ============================================================================
// Step Tracking
// ============================================================================
void AgenticComposerUX::BeginStep(int stepIndex, const std::string& label) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.currentStep = stepIndex;
    m_currentSession.lastActivityMs = NowMs();
    notifyStepUpdate(stepIndex, StepState::InProgress, label);
}

void AgenticComposerUX::CompleteStep(int stepIndex, bool success,
                                      const std::string& result) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.completedSteps++;
    m_currentSession.lastActivityMs = NowMs();
    notifyStepUpdate(stepIndex,
                     success ? StepState::Completed : StepState::Failed,
                     result);
}

void AgenticComposerUX::FailStep(int stepIndex, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.lastActivityMs = NowMs();
    notifyStepUpdate(stepIndex, StepState::Failed, error);
    
    if (m_callbacks.onError) {
        m_callbacks.onError(error, "Check tool output for details");
    }
}

void AgenticComposerUX::SetTotalSteps(int total) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.totalSteps = total;
}

// ============================================================================
// Thinking Display
// ============================================================================
uint64_t AgenticComposerUX::BeginThinking(const std::string& title) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    ThinkingBlock block;
    block.id = m_nextThinkingId.fetch_add(1);
    block.title = title;
    block.startMs = NowMs();
    block.endMs = -1;
    block.collapsed = false;
    block.tokensGenerated = 0;
    block.tokensPerSecond = 0.0f;
    
    m_currentSession.thinkingBlocks.push_back(block);
    notifyThinkingUpdate(block.id, "", false);
    
    return block.id;
}

void AgenticComposerUX::AppendThinking(uint64_t blockId, const std::string& chunk) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    for (auto& block : m_currentSession.thinkingBlocks) {
        if (block.id == blockId) {
            block.content += chunk;
            block.tokensGenerated += static_cast<int>(chunk.size() / 4); // rough estimate
            
            int64_t elapsed = NowMs() - block.startMs;
            if (elapsed > 0) {
                block.tokensPerSecond = static_cast<float>(block.tokensGenerated) 
                    / (elapsed / 1000.0f);
            }
            
            notifyThinkingUpdate(blockId, block.content, false);
            break;
        }
    }
}

void AgenticComposerUX::EndThinking(uint64_t blockId) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    for (auto& block : m_currentSession.thinkingBlocks) {
        if (block.id == blockId) {
            block.endMs = NowMs();
            notifyThinkingUpdate(blockId, block.content, true);
            break;
        }
    }
}

void AgenticComposerUX::SetThinkingCollapsed(uint64_t blockId, bool collapsed) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& block : m_currentSession.thinkingBlocks) {
        if (block.id == blockId) {
            block.collapsed = collapsed;
            break;
        }
    }
}

// ============================================================================
// Tool Call Visualization
// ============================================================================
void AgenticComposerUX::RecordToolCall(const ToolCallDisplay& toolCall) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.toolCalls.push_back(toolCall);
    m_currentSession.lastActivityMs = NowMs();
    notifyToolCall(toolCall);
}

std::vector<ToolCallDisplay> AgenticComposerUX::GetToolCalls() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_currentSession.toolCalls;
}

// ============================================================================
// File Change Approval
// ============================================================================
void AgenticComposerUX::AddFileChange(const FileChangeEntry& change) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    // Check for existing entry for same file
    for (auto& existing : m_currentSession.fileChanges) {
        if (existing.filePath == change.filePath) {
            existing = change;
            m_currentSession.pendingApprovals++;
            notifyApprovalRequired();
            return;
        }
    }
    
    m_currentSession.fileChanges.push_back(change);
    m_currentSession.pendingApprovals++;
    notifyApprovalRequired();
}

void AgenticComposerUX::ApproveFileChange(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (fc.filePath == filePath) {
            fc.approved = true;
            fc.reviewed = true;
            for (auto& h : fc.hunkApprovals) {
                h.approved = true;
                h.reviewed = true;
            }
            m_currentSession.approvedChanges++;
            if (m_currentSession.pendingApprovals > 0)
                m_currentSession.pendingApprovals--;
            break;
        }
    }
}

void AgenticComposerUX::RejectFileChange(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (fc.filePath == filePath) {
            fc.approved = false;
            fc.reviewed = true;
            for (auto& h : fc.hunkApprovals) {
                h.approved = false;
                h.reviewed = true;
            }
            m_currentSession.rejectedChanges++;
            if (m_currentSession.pendingApprovals > 0)
                m_currentSession.pendingApprovals--;
            break;
        }
    }
}

void AgenticComposerUX::ApproveAllChanges() {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (!fc.reviewed) {
            fc.approved = true;
            fc.reviewed = true;
            for (auto& h : fc.hunkApprovals) {
                h.approved = true;
                h.reviewed = true;
            }
            m_currentSession.approvedChanges++;
        }
    }
    m_currentSession.pendingApprovals = 0;
}

void AgenticComposerUX::RejectAllChanges() {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (!fc.reviewed) {
            fc.approved = false;
            fc.reviewed = true;
            for (auto& h : fc.hunkApprovals) {
                h.approved = false;
                h.reviewed = true;
            }
            m_currentSession.rejectedChanges++;
        }
    }
    m_currentSession.pendingApprovals = 0;
}

void AgenticComposerUX::ApproveHunk(const std::string& filePath, int hunkIndex) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (fc.filePath == filePath) {
            for (auto& h : fc.hunkApprovals) {
                if (h.hunkIndex == hunkIndex) {
                    h.approved = true;
                    h.reviewed = true;
                    break;
                }
            }
            break;
        }
    }
}

void AgenticComposerUX::RejectHunk(const std::string& filePath, int hunkIndex) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& fc : m_currentSession.fileChanges) {
        if (fc.filePath == filePath) {
            for (auto& h : fc.hunkApprovals) {
                if (h.hunkIndex == hunkIndex) {
                    h.approved = false;
                    h.reviewed = true;
                    break;
                }
            }
            break;
        }
    }
}

bool AgenticComposerUX::AllChangesReviewed() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (const auto& fc : m_currentSession.fileChanges) {
        if (!fc.reviewed) return false;
    }
    return true;
}

std::vector<FileChangeEntry> AgenticComposerUX::GetPendingChanges() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    std::vector<FileChangeEntry> pending;
    for (const auto& fc : m_currentSession.fileChanges) {
        if (!fc.reviewed) pending.push_back(fc);
    }
    return pending;
}

// ============================================================================
// Context Chips
// ============================================================================
void AgenticComposerUX::AddContextChip(const ContextChip& chip) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    // Deduplicate by label
    for (const auto& existing : m_currentSession.contextChips) {
        if (existing.label == chip.label) return;
    }
    m_currentSession.contextChips.push_back(chip);
    notifyContextUpdate();
}

void AgenticComposerUX::RemoveContextChip(const std::string& label) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto& chips = m_currentSession.contextChips;
    chips.erase(
        std::remove_if(chips.begin(), chips.end(),
                       [&](const ContextChip& c) { return c.label == label; }),
        chips.end());
    notifyContextUpdate();
}

void AgenticComposerUX::ClearContextChips() {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_currentSession.contextChips.clear();
    notifyContextUpdate();
}

std::vector<ContextChip> AgenticComposerUX::GetContextChips() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    return m_currentSession.contextChips;
}

// ============================================================================
// Progress
// ============================================================================
void AgenticComposerUX::UpdateProgress(int current, int total,
                                        const std::string& label) {
    notifyProgress(current, total, label);
}

void AgenticComposerUX::SetBusy(bool busy) {
    m_busy.store(busy);
}

// ============================================================================
// Status
// ============================================================================
void AgenticComposerUX::SetStatus(const std::string& status,
                                   const std::string& detail) {
    if (m_callbacks.onStatusChange) {
        m_callbacks.onStatusChange(status, detail);
    }
}

void AgenticComposerUX::ShowError(const std::string& error,
                                   const std::string& suggestion) {
    if (m_callbacks.onError) {
        m_callbacks.onError(error, suggestion);
    }
}

// ============================================================================
// Render Metrics
// ============================================================================
AgenticComposerUX::RenderMetrics AgenticComposerUX::CalculateRenderMetrics(
    int availableWidth) const {
    (void)availableWidth;
    
    RenderMetrics m;
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    
    // Step panel: 28px per step + 8px padding
    m.stepPanelHeight = (m_currentSession.totalSteps * 28) + 16;
    if (m.stepPanelHeight < 60) m.stepPanelHeight = 60;
    
    // Thinking panel: 100px per visible block
    int visibleThinking = 0;
    for (const auto& tb : m_currentSession.thinkingBlocks) {
        if (!tb.collapsed) visibleThinking++;
    }
    m.thinkingPanelHeight = visibleThinking * 100;
    
    // Approval panel: 40px per pending file
    int pending = 0;
    for (const auto& fc : m_currentSession.fileChanges) {
        if (!fc.reviewed) pending++;
    }
    m.approvalPanelHeight = pending * 40;
    if (pending > 0) m.approvalPanelHeight += 50; // buttons
    
    // Context bar: 32px if chips exist
    m.contextBarHeight = m_currentSession.contextChips.empty() ? 0 : 32;
    
    m.totalHeight = m.stepPanelHeight + m.thinkingPanelHeight 
                  + m.approvalPanelHeight + m.contextBarHeight;
    
    return m;
}

// ============================================================================
// Serialization
// ============================================================================
std::string AgenticComposerUX::SerializeSession() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    const auto& s = m_currentSession;
    
    std::ostringstream ss;
    ss << "{";
    ss << "\"sessionId\":" << s.sessionId;
    ss << ",\"title\":\"" << s.title << "\"";
    ss << ",\"model\":\"" << s.modelName << "\"";
    ss << ",\"currentStep\":" << s.currentStep;
    ss << ",\"totalSteps\":" << s.totalSteps;
    ss << ",\"completedSteps\":" << s.completedSteps;
    ss << ",\"pendingApprovals\":" << s.pendingApprovals;
    ss << ",\"toolCalls\":" << s.toolCalls.size();
    ss << ",\"fileChanges\":" << s.fileChanges.size();
    ss << ",\"contextChips\":" << s.contextChips.size();
    ss << ",\"tokensIn\":" << s.totalTokensIn;
    ss << ",\"tokensOut\":" << s.totalTokensOut;
    ss << "}";
    
    return ss.str();
}

bool AgenticComposerUX::DeserializeSession(const std::string& json) {
    // JSON deserialization — parse session state for replay/restore
    if (json.empty() || json[0] != '{') return false;

    auto extractString = [&json](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":\"";
        auto pos = json.find(pattern);
        if (pos == std::string::npos) return "";
        auto start = pos + pattern.size();
        auto end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    };

    auto extractInt = [&json](const std::string& key) -> int64_t {
        std::string pattern = "\"" + key + "\":";
        auto pos = json.find(pattern);
        if (pos == std::string::npos) return 0;
        auto start = pos + pattern.size();
        // Skip whitespace
        while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
        char* end = nullptr;
        long long val = strtoll(json.c_str() + start, &end, 10);
        return static_cast<int64_t>(val);
    };

    std::lock_guard<std::mutex> lock(m_sessionMutex);

    m_currentSession.sessionId = static_cast<uint64_t>(extractInt("sessionId"));
    m_currentSession.title = extractString("title");
    m_currentSession.modelName = extractString("model");
    m_currentSession.currentStep = static_cast<int>(extractInt("currentStep"));
    m_currentSession.totalSteps = static_cast<int>(extractInt("totalSteps"));
    m_currentSession.completedSteps = static_cast<int>(extractInt("completedSteps"));
    m_currentSession.pendingApprovals = static_cast<int>(extractInt("pendingApprovals"));
    m_currentSession.totalTokensIn = static_cast<uint64_t>(extractInt("tokensIn"));
    m_currentSession.totalTokensOut = static_cast<uint64_t>(extractInt("tokensOut"));

    // Validate that we got at least a session ID
    if (m_currentSession.sessionId == 0) return false;

    m_currentSession.lastActivityMs = NowMs();
    return true;
}

// ============================================================================
// Internal notification helpers
// ============================================================================
void AgenticComposerUX::notifyStepUpdate(int stepIndex, StepState state,
                                          const std::string& label) {
    if (m_callbacks.onStepUpdate) {
        m_callbacks.onStepUpdate(stepIndex, state, label);
    }
}

void AgenticComposerUX::notifyThinkingUpdate(uint64_t blockId,
                                               const std::string& content,
                                               bool complete) {
    if (m_callbacks.onThinkingUpdate) {
        m_callbacks.onThinkingUpdate(blockId, content, complete);
    }
}

void AgenticComposerUX::notifyToolCall(const ToolCallDisplay& tc) {
    if (m_callbacks.onToolCallComplete) {
        m_callbacks.onToolCallComplete(tc);
    }
}

void AgenticComposerUX::notifyApprovalRequired() {
    if (m_callbacks.onApprovalRequired) {
        auto pending = GetPendingChanges();
        m_callbacks.onApprovalRequired(pending);
    }
}

void AgenticComposerUX::notifyContextUpdate() {
    if (m_callbacks.onContextUpdate) {
        m_callbacks.onContextUpdate(m_currentSession.contextChips);
    }
}

void AgenticComposerUX::notifyProgress(int current, int total,
                                         const std::string& label) {
    if (m_callbacks.onProgressUpdate) {
        m_callbacks.onProgressUpdate(current, total, label);
    }
}

} // namespace Agentic
} // namespace RawrXD
