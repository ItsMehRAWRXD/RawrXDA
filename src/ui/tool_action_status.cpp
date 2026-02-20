// ============================================================================
// tool_action_status.cpp — Tool Action Status Rendering Implementation
// ============================================================================
// Three rendering modes: PlainText, HTML, JSON
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "tool_action_status.h"

#include <sstream>
#include <cstdio>

namespace RawrXD {
namespace UI {

// ============================================================================
// Icon Lookups
// ============================================================================

const char* ToolActionStatusFormatter::iconForKind(ToolActionKind kind) {
    switch (kind) {
        case ToolActionKind::ReadFile:         return "\xF0\x9F\x93\x96"; // 📖
        case ToolActionKind::EditFile:         return "\xE2\x9C\x8F\xEF\xB8\x8F"; // ✏️
        case ToolActionKind::CreateFile:       return "\xF0\x9F\x93\x84"; // 📄
        case ToolActionKind::DeleteFile:       return "\xF0\x9F\x97\x91\xEF\xB8\x8F"; // 🗑️
        case ToolActionKind::RunTerminal:      return "\xE2\x9A\xA1";     // ⚡
        case ToolActionKind::SearchGrep:       return "\xF0\x9F\x94\x8D"; // 🔍
        case ToolActionKind::SearchSemantic:   return "\xF0\x9F\xA7\xA0"; // 🧠
        case ToolActionKind::SearchFiles:      return "\xF0\x9F\x93\x82"; // 📂
        case ToolActionKind::ListDirectory:    return "\xF0\x9F\x93\x81"; // 📁
        case ToolActionKind::ManagedTodoList:  return "\xF0\x9F\x93\x8B"; // 📋
        case ToolActionKind::ReviewedChanges:  return "\xF0\x9F\x93\x82"; // 📂
        case ToolActionKind::ListCodeUsages:   return "\xF0\x9F\x94\x97"; // 🔗
        case ToolActionKind::GetErrors:        return "\xE2\x9A\xA0\xEF\xB8\x8F"; // ⚠️
        case ToolActionKind::FetchWebpage:     return "\xF0\x9F\x8C\x90"; // 🌐
        case ToolActionKind::RunSubagent:      return "\xF0\x9F\xA4\x96"; // 🤖
        case ToolActionKind::NotebookRun:      return "\xF0\x9F\x93\x93"; // 📓
        case ToolActionKind::NotebookEdit:     return "\xF0\x9F\x93\x93"; // 📓
        case ToolActionKind::GitOperation:     return "\xF0\x9F\x94\x80"; // 🔀
        case ToolActionKind::BuildProject:     return "\xF0\x9F\x94\xA7"; // 🔧
        case ToolActionKind::FinishedStep:     return "\xE2\x9C\x85";     // ✅
        case ToolActionKind::MultiReplace:     return "\xF0\x9F\x94\x84"; // 🔄
        case ToolActionKind::OpenBrowser:      return "\xF0\x9F\x96\xA5\xEF\xB8\x8F"; // 🖥️
        case ToolActionKind::AgentThinking:    return "\xF0\x9F\x92\xAD"; // 💭
        case ToolActionKind::Custom:
        default:                               return "\xE2\x96\xB6\xEF\xB8\x8F"; // ▶️
    }
}

const char* ToolActionStatusFormatter::stateIcon(ToolActionState state) {
    switch (state) {
        case ToolActionState::Pending:    return "\xE2\xAC\x9C"; // ⬜
        case ToolActionState::Running:    return "\xF0\x9F\x94\x84"; // 🔄
        case ToolActionState::Completed:  return "\xE2\x9C\x85"; // ✅
        case ToolActionState::Failed:     return "\xE2\x9D\x8C"; // ❌
        case ToolActionState::Skipped:    return "\xE2\x8F\xAD\xEF\xB8\x8F"; // ⏭️
        default:                          return "\xE2\x97\xBB\xEF\xB8\x8F"; // ◻️
    }
}

const char* ToolActionStatusFormatter::cssClassForState(ToolActionState state) {
    switch (state) {
        case ToolActionState::Pending:    return "pending";
        case ToolActionState::Running:    return "running";
        case ToolActionState::Completed:  return "done";
        case ToolActionState::Failed:     return "error";
        case ToolActionState::Skipped:    return "skipped";
        default:                          return "pending";
    }
}

// ============================================================================
// Plain Text Formatting (Win32 EDIT controls)
// ============================================================================

std::string ToolActionStatusFormatter::formatPlainText(const ToolActionStatus& action) {
    std::string line;

    // Icon + summary
    line += iconForKind(action.kind);
    line += " ";
    line += action.summary;

    // Duration (if > 0)
    if (action.durationMs > 0) {
        if (action.durationMs < 1000) {
            line += " (" + std::to_string(action.durationMs) + "ms)";
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%.1fs)", action.durationMs / 1000.0);
            line += buf;
        }
    }

    // State indicator for non-completed states
    if (action.state == ToolActionState::Running) {
        line += " [running]";
    } else if (action.state == ToolActionState::Failed) {
        line += " [FAILED]";
    } else if (action.state == ToolActionState::Skipped) {
        line += " [skipped]";
    }

    line += "\r\n";
    return line;
}

std::string ToolActionStatusFormatter::formatPlainTextBlock(
    const std::vector<ToolActionStatus>& actions, int totalSteps)
{
    if (actions.empty()) return "";

    std::string block;
    block += "--- Agent Actions ---\r\n";

    for (const auto& action : actions) {
        block += formatPlainText(action);
    }

    if (totalSteps > 0) {
        block += "\r\n";
        block += iconForKind(ToolActionKind::FinishedStep);
        block += " Finished with ";
        block += std::to_string(totalSteps);
        block += (totalSteps == 1 ? " step" : " steps");
        block += "\r\n";
    }

    block += "---------------------\r\n";
    return block;
}

// ============================================================================
// HTML Formatting (ChatMessageRenderer / WebView2)
// ============================================================================

std::string ToolActionStatusFormatter::formatHtml(const ToolActionStatus& action) {
    std::ostringstream html;

    html << "<div class=\"tool-action-step " << cssClassForState(action.state) << "\">";

    // Icon
    html << "<span class=\"tool-action-icon\">" << iconForKind(action.kind) << "</span>";

    // Summary text
    html << "<span class=\"tool-action-summary\">" << action.summary << "</span>";

    // Duration badge
    if (action.durationMs > 0) {
        html << "<span class=\"tool-action-duration\">";
        if (action.durationMs < 1000) {
            html << action.durationMs << "ms";
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1fs", action.durationMs / 1000.0);
            html << buf;
        }
        html << "</span>";
    }

    // State badge for non-completed
    if (action.state == ToolActionState::Running) {
        html << "<span class=\"tool-action-state running\">running</span>";
    } else if (action.state == ToolActionState::Failed) {
        html << "<span class=\"tool-action-state error\">failed</span>";
    } else if (action.state == ToolActionState::Skipped) {
        html << "<span class=\"tool-action-state skipped\">skipped</span>";
    }

    html << "</div>";
    return html.str();
}

std::string ToolActionStatusFormatter::formatHtmlBlock(
    const std::vector<ToolActionStatus>& actions, int totalSteps)
{
    if (actions.empty()) return "";

    std::ostringstream html;

    // Collapsible container
    html << "<details class=\"tool-action-block\" open>";
    html << "<summary class=\"tool-action-header\">";
    html << "<span class=\"tool-action-header-icon\">"
         << iconForKind(ToolActionKind::FinishedStep) << "</span>";

    if (totalSteps > 0) {
        html << "Finished with " << totalSteps
             << (totalSteps == 1 ? " step" : " steps");
    } else {
        html << actions.size() << (actions.size() == 1 ? " action" : " actions");
    }
    html << "</summary>";

    html << "<div class=\"tool-action-steps\">";
    for (const auto& action : actions) {
        html << formatHtml(action);
    }
    html << "</div>"; // .tool-action-steps
    html << "</details>"; // .tool-action-block

    return html.str();
}

// ============================================================================
// CSS for tool action steps
// ============================================================================

std::string ToolActionStatusFormatter::generateCSS() {
    return R"CSS(
/* ── Tool Action Status Steps ── */
.tool-action-block {
  margin: 8px 0;
  border: 1px solid var(--border, rgba(255,255,255,0.08));
  border-radius: 6px;
  background: var(--bg-secondary, #1e1e1e);
  overflow: hidden;
}
.tool-action-block[open] {
  border-color: var(--border-active, rgba(0,122,204,0.3));
}
.tool-action-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  cursor: pointer;
  font-size: 13px;
  font-weight: 500;
  color: var(--text-primary, #cccccc);
  background: var(--bg-tertiary, #252526);
  user-select: none;
}
.tool-action-header:hover {
  background: var(--bg-hover, #2a2d2e);
}
.tool-action-header-icon {
  font-size: 14px;
}
.tool-action-steps {
  display: flex;
  flex-direction: column;
  gap: 1px;
  background: var(--border, rgba(255,255,255,0.04));
}
.tool-action-step {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 12px 6px 20px;
  font-size: 12px;
  color: var(--text-primary, #cccccc);
  background: var(--bg-secondary, #1e1e1e);
  transition: background 0.15s ease;
}
.tool-action-step:hover {
  background: var(--bg-hover, #2a2d2e);
}
.tool-action-icon {
  font-size: 14px;
  flex-shrink: 0;
  width: 20px;
  text-align: center;
}
.tool-action-summary {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.tool-action-duration {
  font-size: 11px;
  color: var(--accent-green, #4caf50);
  font-family: var(--font-mono, 'Consolas', monospace);
  flex-shrink: 0;
}
.tool-action-state {
  font-size: 10px;
  padding: 1px 6px;
  border-radius: 3px;
  font-weight: 600;
  text-transform: uppercase;
  flex-shrink: 0;
}
.tool-action-state.running {
  background: rgba(0,122,204,0.2);
  color: var(--accent, #007acc);
  animation: tool-action-pulse 1.5s ease-in-out infinite;
}
.tool-action-state.error {
  background: rgba(244,67,54,0.2);
  color: #f44336;
}
.tool-action-state.skipped {
  background: rgba(255,255,255,0.06);
  color: var(--text-muted, #808080);
  text-decoration: line-through;
}
.tool-action-step.done .tool-action-icon {
  opacity: 0.8;
}
.tool-action-step.running .tool-action-icon {
  animation: tool-action-pulse 1.5s ease-in-out infinite;
}
.tool-action-step.error .tool-action-summary {
  color: #f44336;
}
@keyframes tool-action-pulse {
  0%, 100% { opacity: 1; }
  50%      { opacity: 0.4; }
}

/* ── Working Bubbles ── */
.working-bubble {
  margin: 10px 0;
  border: 1px solid var(--border, rgba(255,255,255,0.08));
  border-radius: 8px;
  background: var(--bg-secondary, #1e1e1e);
  overflow: hidden;
  transition: border-color 0.3s ease, box-shadow 0.3s ease;
}
.working-bubble.running {
  border-color: var(--accent, #007acc);
  box-shadow: 0 0 12px rgba(0,122,204,0.15);
}
.working-bubble.completed {
  border-color: var(--accent-green, #4caf50);
}
.working-bubble.failed {
  border-color: #f44336;
}
.working-bubble-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 14px;
  cursor: pointer;
  user-select: none;
  background: var(--bg-tertiary, #252526);
  border-bottom: 1px solid var(--border, rgba(255,255,255,0.06));
}
.working-bubble-header:hover {
  background: var(--bg-hover, #2a2d2e);
}
.working-bubble-spinner {
  display: inline-block;
  width: 14px; height: 14px;
  border: 2px solid rgba(255,255,255,0.1);
  border-top-color: var(--accent, #007acc);
  border-radius: 50%;
  animation: working-spin 0.8s linear infinite;
  flex-shrink: 0;
}
.working-bubble.completed .working-bubble-spinner,
.working-bubble.failed .working-bubble-spinner {
  display: none;
}
.working-bubble-done-icon {
  display: none;
  font-size: 14px;
  flex-shrink: 0;
}
.working-bubble.completed .working-bubble-done-icon {
  display: inline;
}
.working-bubble.failed .working-bubble-done-icon {
  display: inline;
  color: #f44336;
}
.working-bubble-title {
  font-weight: 600;
  font-size: 13px;
  color: var(--text-primary, #cccccc);
  flex: 1;
}
.working-bubble-subtitle {
  font-size: 11px;
  color: var(--text-muted, #808080);
  flex-shrink: 0;
}
.working-bubble-chevron {
  font-size: 10px;
  color: var(--text-muted, #808080);
  transition: transform 0.2s ease;
  flex-shrink: 0;
}
.working-bubble.expanded .working-bubble-chevron {
  transform: rotate(90deg);
}
.working-bubble-elapsed {
  font-size: 10px;
  color: var(--accent-green, #4caf50);
  font-family: var(--font-mono, 'Consolas', monospace);
  flex-shrink: 0;
}
.working-bubble-body {
  display: none;
  padding: 0;
}
.working-bubble.expanded .working-bubble-body {
  display: block;
}
@keyframes working-spin {
  to { transform: rotate(360deg); }
}

/* ── Subagent Nested Bubbles ── */
.subagent-bubble {
  margin: 4px 8px 4px 16px;
  border: 1px solid var(--border, rgba(255,255,255,0.06));
  border-left: 3px solid var(--accent-purple, #9b59b6);
  border-radius: 6px;
  background: rgba(155,89,182,0.03);
  overflow: hidden;
}
.subagent-bubble.running {
  border-left-color: var(--accent, #007acc);
  animation: tool-action-pulse 2s ease-in-out infinite;
}
.subagent-bubble.completed {
  border-left-color: var(--accent-green, #4caf50);
}
.subagent-bubble.failed {
  border-left-color: #f44336;
}
.subagent-header {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 6px 10px;
  cursor: pointer;
  user-select: none;
  font-size: 12px;
}
.subagent-header:hover {
  background: rgba(255,255,255,0.02);
}
.subagent-icon {
  font-size: 13px;
  flex-shrink: 0;
}
.subagent-label {
  color: var(--accent-purple, #9b59b6);
  font-weight: 600;
  font-size: 10px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  flex-shrink: 0;
}
.subagent-description {
  flex: 1;
  color: var(--text-primary, #cccccc);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.subagent-duration {
  font-size: 10px;
  color: var(--accent-green, #4caf50);
  font-family: var(--font-mono, 'Consolas', monospace);
  flex-shrink: 0;
}
.subagent-chevron {
  font-size: 9px;
  color: var(--text-muted, #808080);
  transition: transform 0.2s ease;
}
.subagent-bubble.expanded .subagent-chevron {
  transform: rotate(90deg);
}
.subagent-body {
  display: none;
  padding: 0 4px 4px 12px;
}
.subagent-bubble.expanded .subagent-body {
  display: block;
}
.subagent-prompt {
  padding: 4px 8px;
  margin: 2px 0 4px 0;
  font-size: 11px;
  color: var(--text-muted, #808080);
  font-style: italic;
  border-left: 2px solid var(--border, rgba(255,255,255,0.08));
  max-height: 60px;
  overflow: hidden;
}

/* ── Code Block Keep/Undo Buttons ── */
.code-block-actions {
  display: flex;
  gap: 6px;
  align-items: center;
}
.code-btn-keep, .code-btn-undo {
  padding: 3px 10px;
  border-radius: 4px;
  border: 1px solid var(--border, rgba(255,255,255,0.1));
  font-size: 11px;
  font-family: var(--font-mono, 'Consolas', monospace);
  cursor: pointer;
  transition: all 0.15s ease;
}
.code-btn-keep {
  background: rgba(76,175,80,0.15);
  color: var(--accent-green, #4caf50);
  border-color: rgba(76,175,80,0.3);
}
.code-btn-keep:hover {
  background: rgba(76,175,80,0.3);
}
.code-btn-undo {
  background: rgba(244,67,54,0.10);
  color: #f44336;
  border-color: rgba(244,67,54,0.2);
}
.code-btn-undo:hover {
  background: rgba(244,67,54,0.25);
}
.code-btn-keep.applied {
  background: var(--accent-green, #4caf50);
  color: #fff;
  cursor: default;
}
.code-btn-undo.applied {
  background: #f44336;
  color: #fff;
  cursor: default;
}
)CSS";
}

// ============================================================================
// JSON Formatting (AgentTranscript)
// ============================================================================

std::string ToolActionStatusFormatter::formatJson(const ToolActionStatus& action) {
    std::ostringstream json;

    json << "{";
    json << "\"kind\":" << static_cast<int>(action.kind) << ",";
    json << "\"state\":" << static_cast<int>(action.state) << ",";
    json << "\"summary\":\"" << action.summary << "\"";

    if (!action.detail.empty())
        json << ",\"detail\":\"" << action.detail << "\"";
    if (action.durationMs > 0)
        json << ",\"duration_ms\":" << action.durationMs;
    if (action.stepNumber > 0)
        json << ",\"step\":" << action.stepNumber;
    if (!action.filePath.empty())
        json << ",\"file\":\"" << action.filePath << "\"";
    if (action.lineStart > 0)
        json << ",\"line_start\":" << action.lineStart << ",\"line_end\":" << action.lineEnd;
    if (!action.command.empty())
        json << ",\"command\":\"" << action.command << "\"";
    if (!action.searchQuery.empty())
        json << ",\"query\":\"" << action.searchQuery << "\"";
    if (action.matchCount > 0)
        json << ",\"match_count\":" << action.matchCount;

    json << "}";
    return json.str();
}

// ============================================================================
// ToolActionAccumulator
// ============================================================================

void ToolActionAccumulator::addAction(const ToolActionStatus& action) {
    m_actions.push_back(action);
}

void ToolActionAccumulator::clear() {
    m_actions.clear();
}

int ToolActionAccumulator::completedActions() const {
    int count = 0;
    for (const auto& a : m_actions) {
        if (a.state == ToolActionState::Completed) ++count;
    }
    return count;
}

int ToolActionAccumulator::failedActions() const {
    int count = 0;
    for (const auto& a : m_actions) {
        if (a.state == ToolActionState::Failed) ++count;
    }
    return count;
}

int64_t ToolActionAccumulator::totalDurationMs() const {
    int64_t total = 0;
    for (const auto& a : m_actions) {
        total += a.durationMs;
    }
    return total;
}

std::string ToolActionAccumulator::renderPlainText() const {
    return ToolActionStatusFormatter::formatPlainTextBlock(m_actions, totalActions());
}

std::string ToolActionAccumulator::renderHtml() const {
    return ToolActionStatusFormatter::formatHtmlBlock(m_actions, totalActions());
}

// ============================================================================
// Working Bubble Management
// ============================================================================

void ToolActionAccumulator::beginWorkingBubble(const std::string& title, const std::string& subtitle) {
    m_bubbles.push_back(WorkingBubble::Begin(title, subtitle));
    m_activeBubbleIdx = static_cast<int>(m_bubbles.size()) - 1;
    m_activeSubagentIdx = -1;
}

void ToolActionAccumulator::endWorkingBubble(bool success) {
    if (m_activeBubbleIdx >= 0 && m_activeBubbleIdx < static_cast<int>(m_bubbles.size())) {
        m_bubbles[m_activeBubbleIdx].finish(success);
    }
    m_activeBubbleIdx = -1;
    m_activeSubagentIdx = -1;
}

void ToolActionAccumulator::beginSubagentBubble(const std::string& description, const std::string& prompt) {
    if (m_activeBubbleIdx < 0 || m_activeBubbleIdx >= static_cast<int>(m_bubbles.size())) return;
    auto& bubble = m_bubbles[m_activeBubbleIdx];
    WorkingBubble::SubagentBubble sub;
    sub.agentDescription = description;
    sub.agentPrompt = prompt;
    sub.state = ToolActionState::Running;
    bubble.subagents.push_back(std::move(sub));
    m_activeSubagentIdx = static_cast<int>(bubble.subagents.size()) - 1;
}

void ToolActionAccumulator::endSubagentBubble(bool success, int64_t durationMs) {
    if (m_activeBubbleIdx < 0 || m_activeBubbleIdx >= static_cast<int>(m_bubbles.size())) return;
    auto& bubble = m_bubbles[m_activeBubbleIdx];
    if (m_activeSubagentIdx >= 0 && m_activeSubagentIdx < static_cast<int>(bubble.subagents.size())) {
        auto& sub = bubble.subagents[m_activeSubagentIdx];
        sub.state = success ? ToolActionState::Completed : ToolActionState::Failed;
        sub.durationMs = durationMs;
        sub.isCollapsed = true;
    }
    m_activeSubagentIdx = -1;
}

std::string ToolActionAccumulator::renderBubblesPlainText() const {
    std::string result;
    for (const auto& bubble : m_bubbles) {
        result += ToolActionStatusFormatter::formatWorkingBubblePlainText(bubble);
    }
    return result;
}

std::string ToolActionAccumulator::renderBubblesHtml() const {
    std::string result;
    for (const auto& bubble : m_bubbles) {
        result += ToolActionStatusFormatter::formatWorkingBubbleHtml(bubble);
    }
    return result;
}

void ToolActionAccumulator::trackCodeBlockAction(const CodeBlockAction& action) {
    m_codeActions.push_back(action);
}

// ============================================================================
// Working Bubble Rendering — Plain Text
// ============================================================================

std::string ToolActionStatusFormatter::formatWorkingBubblePlainText(const WorkingBubble& bubble) {
    std::string result;

    // Bubble header
    const char* stateMarker = "";
    switch (bubble.state) {
        case ToolActionState::Running:   stateMarker = "[working] "; break;
        case ToolActionState::Completed: stateMarker = "[done] "; break;
        case ToolActionState::Failed:    stateMarker = "[FAILED] "; break;
        default: break;
    }

    result += "--- ";
    result += stateMarker;
    result += bubble.title;
    if (!bubble.subtitle.empty()) {
        result += " (" + bubble.subtitle + ")";
    }
    result += " ---\r\n";

    // Steps inside bubble
    for (const auto& step : bubble.steps) {
        result += "  " + formatPlainText(step);
    }

    // Subagent bubbles
    for (const auto& sub : bubble.subagents) {
        result += formatSubagentBubblePlainText(sub);
    }

    // Duration
    if (bubble.endTimeMs > 0 && bubble.startTimeMs > 0) {
        int64_t elapsed = bubble.endTimeMs - bubble.startTimeMs;
        char buf[64];
        if (elapsed < 1000)
            std::snprintf(buf, sizeof(buf), "  Duration: %lldms\r\n", (long long)elapsed);
        else
            std::snprintf(buf, sizeof(buf), "  Duration: %.1fs\r\n", elapsed / 1000.0);
        result += buf;
    }

    result += "---\r\n";
    return result;
}

std::string ToolActionStatusFormatter::formatSubagentBubblePlainText(
    const WorkingBubble::SubagentBubble& sub)
{
    std::string result;
    const char* marker = sub.state == ToolActionState::Completed ? "[done]"
                       : sub.state == ToolActionState::Failed ? "[FAILED]"
                       : "[working]";

    result += "  \xF0\x9F\xA4\x96 Subagent: " + sub.agentDescription + " " + marker + "\r\n";

    if (!sub.agentPrompt.empty()) {
        std::string prompt = sub.agentPrompt;
        if (prompt.size() > 120) prompt = prompt.substr(0, 117) + "...";
        result += "    Prompt: " + prompt + "\r\n";
    }

    for (const auto& step : sub.innerSteps) {
        result += "    " + formatPlainText(step);
    }

    if (sub.durationMs > 0) {
        char buf[64];
        if (sub.durationMs < 1000)
            std::snprintf(buf, sizeof(buf), "    Duration: %lldms\r\n", (long long)sub.durationMs);
        else
            std::snprintf(buf, sizeof(buf), "    Duration: %.1fs\r\n", sub.durationMs / 1000.0);
        result += buf;
    }

    return result;
}

// ============================================================================
// Working Bubble Rendering — HTML
// ============================================================================

std::string ToolActionStatusFormatter::formatWorkingBubbleHtml(const WorkingBubble& bubble) {
    std::ostringstream html;

    std::string stateClass = cssClassForState(bubble.state);

    html << "<div class=\"working-bubble " << stateClass;
    if (!bubble.isCollapsed) html << " expanded";
    html << "\">";

    // Header
    html << "<div class=\"working-bubble-header\" onclick=\"this.parentElement.classList.toggle('expanded')\">";
    html << "<span class=\"working-bubble-spinner\"></span>";

    // Done icon (shown when completed)
    if (bubble.state == ToolActionState::Completed)
        html << "<span class=\"working-bubble-done-icon\">\xE2\x9C\x85</span>";
    else if (bubble.state == ToolActionState::Failed)
        html << "<span class=\"working-bubble-done-icon\">\xE2\x9D\x8C</span>";

    html << "<span class=\"working-bubble-title\">" << bubble.title << "</span>";

    if (!bubble.subtitle.empty()) {
        html << "<span class=\"working-bubble-subtitle\">" << bubble.subtitle << "</span>";
    }

    // Elapsed time
    int64_t elapsed = bubble.elapsedMs();
    if (elapsed > 0) {
        html << "<span class=\"working-bubble-elapsed\">";
        if (elapsed < 1000)
            html << elapsed << "ms";
        else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1fs", elapsed / 1000.0);
            html << buf;
        }
        html << "</span>";
    }

    html << "<span class=\"working-bubble-chevron\">\xE2\x96\xB6</span>";
    html << "</div>"; // .working-bubble-header

    // Body
    html << "<div class=\"working-bubble-body\">";

    // Steps
    for (const auto& step : bubble.steps) {
        html << formatHtml(step);
    }

    // Subagent bubbles
    for (const auto& sub : bubble.subagents) {
        html << formatSubagentBubbleHtml(sub);
    }

    html << "</div>"; // .working-bubble-body
    html << "</div>"; // .working-bubble

    return html.str();
}

std::string ToolActionStatusFormatter::formatSubagentBubbleHtml(
    const WorkingBubble::SubagentBubble& sub)
{
    std::ostringstream html;

    std::string stateClass = cssClassForState(sub.state);

    html << "<div class=\"subagent-bubble " << stateClass;
    if (!sub.isCollapsed) html << " expanded";
    html << "\">";

    // Header
    html << "<div class=\"subagent-header\" onclick=\"this.parentElement.classList.toggle('expanded')\">";
    html << "<span class=\"subagent-icon\">\xF0\x9F\xA4\x96</span>";
    html << "<span class=\"subagent-label\">Subagent</span>";
    html << "<span class=\"subagent-description\">" << sub.agentDescription << "</span>";

    if (sub.durationMs > 0) {
        html << "<span class=\"subagent-duration\">";
        if (sub.durationMs < 1000)
            html << sub.durationMs << "ms";
        else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1fs", sub.durationMs / 1000.0);
            html << buf;
        }
        html << "</span>";
    }

    html << "<span class=\"subagent-chevron\">\xE2\x96\xB6</span>";
    html << "</div>"; // .subagent-header

    // Body
    html << "<div class=\"subagent-body\">";

    // Prompt snippet
    if (!sub.agentPrompt.empty()) {
        std::string prompt = sub.agentPrompt;
        if (prompt.size() > 300) prompt = prompt.substr(0, 297) + "...";
        html << "<div class=\"subagent-prompt\">" << prompt << "</div>";
    }

    // Inner steps
    for (const auto& step : sub.innerSteps) {
        html << formatHtml(step);
    }

    html << "</div>"; // .subagent-body
    html << "</div>"; // .subagent-bubble

    return html.str();
}

// ============================================================================
// Code Block Keep/Undo Button HTML
// ============================================================================

std::string ToolActionStatusFormatter::generateCodeBlockActionsHtml(
    const std::string& messageId, int blockIndex, const std::string& language)
{
    std::ostringstream html;

    html << "<div class=\"code-block-actions\">";

    // Copy button (always present)
    html << "<button class=\"code-btn\" onclick=\"copyCodeBlock('"
         << messageId << "', " << blockIndex << ")\">Copy</button>";

    // Keep button (for code edits)
    html << "<button class=\"code-btn-keep\" data-msg=\"" << messageId
         << "\" data-block=\"" << blockIndex
         << "\" onclick=\"keepCodeBlock(this, '"
         << messageId << "', " << blockIndex << ")\">Keep</button>";

    // Undo button
    html << "<button class=\"code-btn-undo\" data-msg=\"" << messageId
         << "\" data-block=\"" << blockIndex
         << "\" onclick=\"undoCodeBlock(this, '"
         << messageId << "', " << blockIndex << ")\">Undo</button>";

    html << "</div>";
    return html.str();
}

} // namespace UI
} // namespace RawrXD
