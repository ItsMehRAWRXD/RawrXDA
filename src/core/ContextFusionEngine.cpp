// ContextFusionEngine.cpp — Implementation of unified semantic state machine
// This collapses all IDE intelligence loops into one shared runtime state.

#include "ContextFusionEngine.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {

// ─────────────────────────────────────────────────────────────────────────────
// ContextFrame Helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string ContextFrame::CurrentLine() const {
    std::istringstream stream(bufferText);
    std::string line;
    int currentLine = 0;
    while (std::getline(stream, line)) {
        if (currentLine == cursor.line) {
            return line;
        }
        currentLine++;
    }
    return "";
}

std::string ContextFrame::CurrentWord() const {
    std::string line = CurrentLine();
    if (line.empty() || cursor.column < 0 || cursor.column >= (int)line.length()) {
        return "";
    }
    
    // Find word boundaries
    int start = cursor.column;
    int end = cursor.column;
    
    // Walk backward
    while (start > 0 && (std::isalnum(line[start - 1]) || line[start - 1] == '_')) {
        start--;
    }
    
    // Walk forward
    while (end < (int)line.length() && (std::isalnum(line[end]) || line[end] == '_')) {
        end++;
    }
    
    if (start < end) {
        return line.substr(start, end - start);
    }
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

ContextFusionEngine& ContextFusionEngine::Get() {
    static ContextFusionEngine instance;
    return instance;
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void ContextFusionEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame = ContextFrame();
    m_currentVersion = 0;
    m_eventQueue.clear();
    m_initialized = true;
}

void ContextFusionEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_subscribers.clear();
    m_eventQueue.clear();
    m_initialized = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event Ingestion
// ─────────────────────────────────────────────────────────────────────────────

void ContextFusionEngine::EmitEvent(const ContextEvent& event) {
    if (!m_initialized) return;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_eventQueue.push_back(event);
    }
    
    // Process immediately (can be made async if needed)
    ProcessEvent(event);
    OnEventReceived.emit(event);
    
    // Broadcast updated frame to all subscribers
    BroadcastFrame();
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame Access
// ─────────────────────────────────────────────────────────────────────────────

ContextFrame ContextFusionEngine::GetCurrentFrame() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentFrame;
}

ContextFrame ContextFusionEngine::GetFrameCopy() const {
    return GetCurrentFrame();
}

// ─────────────────────────────────────────────────────────────────────────────
// Subscription
// ─────────────────────────────────────────────────────────────────────────────

void ContextFusionEngine::Subscribe(IContextSubscriber* subscriber) {
    if (!subscriber) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already subscribed
    auto it = std::find(m_subscribers.begin(), m_subscribers.end(), subscriber);
    if (it == m_subscribers.end()) {
        m_subscribers.push_back(subscriber);
    }
}

void ContextFusionEngine::Unsubscribe(IContextSubscriber* subscriber) {
    if (!subscriber) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::find(m_subscribers.begin(), m_subscribers.end(), subscriber);
    if (it != m_subscribers.end()) {
        m_subscribers.erase(it);
    }
}

size_t ContextFusionEngine::GetSubscriberCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_subscribers.size();
}

// ─────────────────────────────────────────────────────────────────────────────
// Direct Frame Mutation (for testing/debugging)
// ─────────────────────────────────────────────────────────────────────────────

void ContextFusionEngine::InjectFrame(const ContextFrame& frame) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFrame = frame;
        m_currentFrame.version = ++m_currentVersion;
        m_currentFrame.timestamp = GetTickCount64();
    }
    
    BroadcastFrame();
    OnFrameUpdated.emit(m_currentFrame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Merge Logic
// ─────────────────────────────────────────────────────────────────────────────

void ContextFusionEngine::ProcessEvent(const ContextEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (event.type) {
        case ContextEvent::EDITOR_CHANGED:
            // Editor state is always source of truth
            if (event.payload) {
                auto* text = static_cast<std::string*>(event.payload);
                m_currentFrame.bufferText = *text;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::CURSOR_MOVED:
            if (event.payload) {
                auto* pos = static_cast<EditorPosition*>(event.payload);
                m_currentFrame.cursor = *pos;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::SELECTION_CHANGED:
            if (event.payload) {
                auto* sel = static_cast<SelectionRange*>(event.payload);
                m_currentFrame.selection = *sel;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::VISIBLE_RANGE_CHANGED:
            if (event.payload) {
                auto* vis = static_cast<VisibleRange*>(event.payload);
                m_currentFrame.visible = *vis;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::LSP_UPDATED:
            // LSP enriches but doesn't override editor
            if (event.payload) {
                auto* symbols = static_cast<std::vector<SymbolInfo>*>(event.payload);
                m_currentFrame.symbols = *symbols;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::AI_RESPONSE:
            // AI state is secondary
            if (event.payload) {
                auto* interaction = static_cast<AIInteraction*>(event.payload);
                m_currentFrame.recentInteractions.push_back(*interaction);
                // Keep only last 10 interactions
                if (m_currentFrame.recentInteractions.size() > 10) {
                    m_currentFrame.recentInteractions.erase(
                        m_currentFrame.recentInteractions.begin()
                    );
                }
                m_currentFrame.lastGhostText = interaction->response;
                m_currentFrame.lastConfidence = interaction->confidence;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::AI_ACCEPTED:
            // Clear ghost text on accept
            m_currentFrame.lastGhostText.clear();
            m_currentFrame.lastConfidence = 0.0f;
            m_currentFrame.version = ++m_currentVersion;
            break;
            
        case ContextEvent::AI_REJECTED:
            // Clear ghost text on reject
            m_currentFrame.lastGhostText.clear();
            m_currentFrame.lastConfidence = 0.0f;
            m_currentFrame.version = ++m_currentVersion;
            break;
            
        case ContextEvent::GIT_UPDATED:
            if (event.payload) {
                auto* git = static_cast<GitState*>(event.payload);
                m_currentFrame.git = *git;
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::FILE_OPENED:
            if (event.payload) {
                auto* path = static_cast<std::string*>(event.payload);
                m_currentFrame.filePath = *path;
                // Detect language from extension
                size_t dot = path->find_last_of('.');
                if (dot != std::string::npos) {
                    std::string ext = path->substr(dot + 1);
                    if (ext == "cpp" || ext == "h" || ext == "hpp") m_currentFrame.languageId = "cpp";
                    else if (ext == "c") m_currentFrame.languageId = "c";
                    else if (ext == "py") m_currentFrame.languageId = "python";
                    else if (ext == "js" || ext == "ts") m_currentFrame.languageId = "javascript";
                    else if (ext == "rs") m_currentFrame.languageId = "rust";
                    else if (ext == "go") m_currentFrame.languageId = "go";
                    else m_currentFrame.languageId = "plaintext";
                }
                m_currentFrame.version = ++m_currentVersion;
            }
            break;
            
        case ContextEvent::FILE_SAVED:
            // Just update timestamp
            m_currentFrame.version = ++m_currentVersion;
            break;
            
        case ContextEvent::AGENT_ACTION:
        case ContextEvent::TOOL_EXECUTED:
            // These affect context but don't directly mutate frame
            m_currentFrame.version = ++m_currentVersion;
            break;
    }
    
    m_currentFrame.timestamp = GetTickCount64();
}

void ContextFusionEngine::RebuildFrame() {
    // Full rebuild from all sources
    // This is called when a major state change happens
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentFrame.version = ++m_currentVersion;
    m_currentFrame.timestamp = GetTickCount64();
}

void ContextFusionEngine::BroadcastFrame() {
    auto subscribers = GetOrderedSubscribers();
    
    for (auto* sub : subscribers) {
        if (sub) {
            sub->OnContextUpdate(m_currentFrame);
        }
    }
}

std::vector<IContextSubscriber*> ContextFusionEngine::GetOrderedSubscribers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto result = m_subscribers;
    std::sort(result.begin(), result.end(), [](IContextSubscriber* a, IContextSubscriber* b) {
        if (!a || !b) return false;
        return a->GetPriority() < b->GetPriority();
    });
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame Validation
// ─────────────────────────────────────────────────────────────────────────────

bool ContextFusionEngine::ValidateFrameIntegrity(const ContextFrame& frame) const {
    // 1. Cursor must be within buffer bounds
    int lineCount = 0;
    {
        std::istringstream stream(frame.bufferText);
        std::string line;
        while (std::getline(stream, line)) lineCount++;
    }
    if (frame.cursor.line < 0 || frame.cursor.line >= lineCount) return false;
    
    // 2. Selection must be valid
    if (!frame.selection.isEmpty()) {
        if (frame.selection.start.line < 0 || frame.selection.end.line < 0) return false;
        if (frame.selection.start.line > frame.selection.end.line) return false;
    }
    
    // 3. Visible range must be within total lines
    if (frame.visible.startLine < 0 || frame.visible.endLine < frame.visible.startLine) return false;
    if (frame.visible.endLine > lineCount) return false;
    
    // 4. Symbols must reference valid lines
    for (const auto& sym : frame.symbols) {
        if (sym.line < 0 || sym.line >= lineCount) return false;
    }
    
    // 5. Diagnostics must reference valid lines
    for (const auto& diag : frame.diagnostics) {
        if (diag.line < 0 || diag.line >= lineCount) return false;
    }
    
    // 6. Language ID must not be empty if file path is set
    if (!frame.filePath.empty() && frame.languageId.empty()) return false;
    
    // 7. Version must be monotonic
    if (frame.version == 0 && !frame.bufferText.empty()) return false;
    
    return true;
}

std::string ContextFusionEngine::GetFrameDiagnostics(const ContextFrame& frame) const {
    std::ostringstream oss;
    int issues = 0;
    
    int lineCount = 0;
    {
        std::istringstream stream(frame.bufferText);
        std::string line;
        while (std::getline(stream, line)) lineCount++;
    }
    
    if (frame.cursor.line < 0 || frame.cursor.line >= lineCount) {
        oss << "[INVALID] cursor.line=" << frame.cursor.line << " out of bounds (0-" << (lineCount-1) << ")\n";
        issues++;
    }
    if (!frame.selection.isEmpty() && frame.selection.start.line > frame.selection.end.line) {
        oss << "[INVALID] selection start > end\n";
        issues++;
    }
    if (frame.visible.startLine < 0 || frame.visible.endLine > lineCount) {
        oss << "[INVALID] visible range out of bounds\n";
        issues++;
    }
    if (!frame.filePath.empty() && frame.languageId.empty()) {
        oss << "[INVALID] filePath set but languageId empty\n";
        issues++;
    }
    if (frame.version == 0 && !frame.bufferText.empty()) {
        oss << "[INVALID] version=0 with non-empty buffer\n";
        issues++;
    }
    
    if (issues == 0) {
        oss << "[VALID] Frame integrity OK. Lines=" << lineCount
            << " Symbols=" << frame.symbols.size()
            << " Diagnostics=" << frame.diagnostics.size()
            << " Version=" << frame.version << "\n";
    }
    
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// ContextFrameBuilder
// ─────────────────────────────────────────────────────────────────────────────

ContextFrameBuilder& ContextFrameBuilder::WithEditorState(
    const std::string& filePath,
    const std::string& bufferText,
    const EditorPosition& cursor,
    const SelectionRange& selection
) {
    m_frame.filePath = filePath;
    m_frame.bufferText = bufferText;
    m_frame.cursor = cursor;
    m_frame.selection = selection;
    return *this;
}

ContextFrameBuilder& ContextFrameBuilder::WithLSPData(
    const std::vector<SymbolInfo>& symbols,
    const std::vector<DiagnosticInfo>& diagnostics
) {
    m_frame.symbols = symbols;
    m_frame.diagnostics = diagnostics;
    return *this;
}

ContextFrameBuilder& ContextFrameBuilder::WithAIState(
    const std::string& lastCompletion,
    float confidence
) {
    m_frame.lastGhostText = lastCompletion;
    m_frame.lastConfidence = confidence;
    return *this;
}

ContextFrameBuilder& ContextFrameBuilder::WithGitState(const GitState& git) {
    m_frame.git = git;
    return *this;
}

ContextFrame ContextFrameBuilder::Build() {
    m_frame.timestamp = GetTickCount64();
    return m_frame;
}

} // namespace RawrXD