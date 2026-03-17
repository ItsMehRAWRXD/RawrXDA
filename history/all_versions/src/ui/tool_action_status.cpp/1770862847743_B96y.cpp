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

} // namespace UI
} // namespace RawrXD
