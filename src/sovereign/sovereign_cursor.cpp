// ============================================================================
// sovereign_cursor.cpp — The "Digital Cortex" Implementation
// ============================================================================
// This is the AI loop that makes RawrXD a Cursor-like IDE:
//
//   Typing → Debounce → Context Assembly → RAG Retrieval → Inference
//   → Diff Generation → Suggestion → User Accept/Reject → Apply
//
// Zero HTTP. All operations use direct memory access.
// ============================================================================

#include "sovereign_cursor.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {
namespace Sovereign {

// ============================================================================
// Construction / Destruction
// ============================================================================

SovereignCursor::SovereignCursor(const CursorAIConfig& cfg) : config_(cfg) {}

SovereignCursor::~SovereignCursor() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SovereignCursor::Initialize(
    const std::string& modelPath,
    const std::string& embeddingModelPath,
    uint32_t contextSize) {
    
    ReportProgress("Initializing Sovereign Cursor...");
    
    // 1. Initialize inference engine
    Agent::SovereignModelConfig infCfg;
    infCfg.model_path = modelPath;
    infCfg.context_size = contextSize;
    infCfg.temperature = config_.temperature;
    infCfg.max_tokens = config_.maxTokens;
    infCfg.enable_speculative = config_.useSpeculative;
    infCfg.n_batch = 512;
    
    inference_ = std::make_unique<Agent::SovereignInferenceClient>(infCfg);
    
    if (!inference_->LoadModel(modelPath)) {
        ReportProgress("ERROR: Failed to load inference model: " + modelPath);
        return false;
    }
    
    ReportProgress("Inference model loaded: " + modelPath);
    
    // 2. Initialize embedding engine (if path provided)
    if (!embeddingModelPath.empty()) {
        Embeddings::EmbeddingModelConfig embCfg;
        embCfg.modelPath = embeddingModelPath;
        embCfg.dimensions = 384;  // MiniLM default
        embCfg.maxTokens = 512;
        embCfg.batchSize = 32;
        embCfg.numThreads = config_.inferenceThreads;
        embCfg.useMASMTokenizer = true;
        embCfg.normalizeOutput = true;
        
        embeddings_ = std::make_unique<Embeddings::EmbeddingEngine>(embCfg);
        
        auto result = embeddings_->Initialize();
        if (!result.success) {
            ReportProgress("WARNING: Embedding engine failed: " + std::string(result.detail));
            // Non-fatal: RAG is optional
            embeddings_.reset();
        } else {
            ReportProgress("Embedding model loaded: " + embeddingModelPath);
        }
    }
    
    // 3. Start worker thread
    running_ = true;
    workerThread_ = std::thread(&SovereignCursor::WorkerLoop, this);
    
    ReportProgress("Sovereign Cursor ready.");
    return true;
}

void SovereignCursor::Shutdown() {
    running_ = false;
    queueCv_.notify_all();
    
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    
    if (inference_) {
        inference_->UnloadModel();
        inference_.reset();
    }
    
    if (embeddings_) {
        embeddings_.reset();
    }
    
    buffer_ = nullptr;
}

bool SovereignCursor::IsReady() const {
    return inference_ && inference_->IsLoaded();
}

// ============================================================================
// Editor Binding
// ============================================================================

void SovereignCursor::AttachBuffer(TextBuffer* buffer) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    buffer_ = buffer;
    ClearContext();
}

void SovereignCursor::DetachBuffer() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    buffer_ = nullptr;
}

void SovereignCursor::SetCursorPosition(int pos) {
    cursorPos_.store(pos);
}

int SovereignCursor::GetCursorPosition() const {
    return cursorPos_.load();
}

// ============================================================================
// Trigger Points
// ============================================================================

void SovereignCursor::OnTyping(int pos, const std::wstring& text) {
    cursorPos_.store(pos + (int)text.length());
    lastTyping_ = std::chrono::steady_clock::now();
    debounceActive_.store(true);
    
    // Queue a completion request (will be debounced in worker)
    std::lock_guard<std::mutex> lock(queueMutex_);
    workQueue_.push({WorkItem::Complete, L"", cursorPos_.load()});
    queueCv_.notify_one();
}

void SovereignCursor::OnSelection(int start, int end) {
    selectionStart_.store(start);
    selectionEnd_.store(end);
}

void SovereignCursor::OnIdle() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    workQueue_.push({WorkItem::Idle, L"", cursorPos_.load()});
    queueCv_.notify_one();
}

// ============================================================================
// Explicit Requests
// ============================================================================

AISuggestion SovereignCursor::RequestCompletion() {
    if (!buffer_ || !IsReady()) {
        return AISuggestion{.type = AISuggestion::Type::None};
    }
    
    return GenerateCompletion(cursorPos_.load());
}

AISuggestion SovereignCursor::RequestEdit(const std::wstring& instruction) {
    if (!buffer_ || !IsReady()) {
        return AISuggestion{.type = AISuggestion::Type::None};
    }
    
    return GenerateEdit(cursorPos_.load(), instruction);
}

std::vector<AISuggestion> SovereignCursor::RequestAlternatives() {
    std::vector<AISuggestion> results;
    
    // Generate 3 alternatives with different temperatures
    float temps[] = {0.0f, 0.3f, 0.7f};
    
    for (float temp : temps) {
        auto saved = config_.temperature;
        config_.temperature = temp;
        auto sug = RequestCompletion();
        config_.temperature = saved;
        
        if (sug.type != AISuggestion::Type::None) {
            results.push_back(sug);
        }
    }
    
    return results;
}

// ============================================================================
// Suggestion Management
// ============================================================================

bool SovereignCursor::AcceptSuggestion(const AISuggestion& sug) {
    if (!buffer_) return false;
    
    if (sug.type == AISuggestion::Type::Completion ||
        sug.type == AISuggestion::Type::Edit) {
        
        // Simple text insertion/replacement
        if (sug.replaceLen > 0) {
            buffer_->remove(sug.insertPos, sug.replaceLen);
        }
        
        auto text = Utf8ToWstring(sug.text);
        buffer_->insert(sug.insertPos, String(text.c_str()));
        
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.suggestionsAccepted++;
        }
        
        return true;
    }
    
    if (sug.type == AISuggestion::Type::Diff) {
        return ApplyDiffToBuffer(sug.diff);
    }
    
    return false;
}

bool SovereignCursor::RejectSuggestion(const AISuggestion& sug) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.suggestionsRejected++;
    return true;
}

bool SovereignCursor::PreviewSuggestion(const AISuggestion& sug) {
    // TODO: Render ghost text in editor
    // This would integrate with the editor's rendering pipeline
    return true;
}

void SovereignCursor::ClearSuggestions() {
    std::lock_guard<std::mutex> lock(suggestionMutex_);
    activeSuggestions_.clear();
}

// ============================================================================
// Context Management
// ============================================================================

void SovereignCursor::IndexWorkspace(const std::string& path) {
    if (!embeddings_) return;
    
    ReportProgress("Indexing workspace: " + path);
    
    // TODO: Walk directory, chunk files, generate embeddings
    // This is a background operation that populates the vector store
    
    std::lock_guard<std::mutex> lock(queueMutex_);
    workQueue_.push({WorkItem::Index, L"", 0});
    queueCv_.notify_one();
}

void SovereignCursor::IndexCurrentFile() {
    if (!buffer_ || !embeddings_) return;
    
    // Re-index the current buffer content
    auto text = buffer_->text();
    // TODO: Chunk and embed
}

void SovereignCursor::ClearContext() {
    if (inference_) {
        inference_->ClearKVCache();
    }
    
    if (embeddings_) {
        // TODO: Clear embedding cache
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void SovereignCursor::SetSuggestionCallback(SuggestionCallback cb) {
    onSuggestion_ = cb;
}

void SovereignCursor::SetProgressCallback(ProgressCallback cb) {
    onProgress_ = cb;
}

// ============================================================================
// Statistics
// ============================================================================

SovereignCursor::Stats SovereignCursor::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

// ============================================================================
// Worker Loop
// ============================================================================

void SovereignCursor::WorkerLoop() {
    while (running_) {
        WorkItem item;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] { return !workQueue_.empty() || !running_; });
            
            if (!running_) break;
            
            item = workQueue_.front();
            workQueue_.pop();
        }
        
        // Debounce: if this is a completion request, wait for typing to stop
        if (item.type == WorkItem::Complete && debounceActive_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTyping_).count();
            
            if (elapsed < (int)config_.debounceMs) {
                // Re-queue with delay
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.debounceMs - elapsed));
                
                std::lock_guard<std::mutex> lock(queueMutex_);
                workQueue_.push(item);
                continue;
            }
            
            debounceActive_ = false;
        }
        
        ProcessWorkItem(item);
    }
}

void SovereignCursor::ProcessWorkItem(const WorkItem& item) {
    if (!buffer_ || !IsReady()) return;
    
    switch (item.type) {
        case WorkItem::Complete: {
            auto sug = GenerateCompletion(item.cursorPos);
            if (sug.type != AISuggestion::Type::None && onSuggestion_) {
                onSuggestion_(sug);
            }
            break;
        }
        
        case WorkItem::Edit: {
            auto sug = GenerateEdit(item.cursorPos, item.instruction);
            if (sug.type != AISuggestion::Type::None && onSuggestion_) {
                onSuggestion_(sug);
            }
            break;
        }
        
        case WorkItem::Index: {
            // Background indexing
            // TODO: Implement workspace indexing
            break;
        }
        
        case WorkItem::Idle: {
            // Could trigger predictive indexing or cache warming
            break;
        }
    }
}

// ============================================================================
// Context Assembly
// ============================================================================

std::string SovereignCursor::BuildContextPrompt(int cursorPos) {
    // 1. Get current file context
    std::string fileContext = GetBufferContext(cursorPos, config_.maxContextChars);
    
    // 2. Retrieve relevant code chunks from workspace
    std::string ragContext;
    if (embeddings_) {
        ragContext = RetrieveRelevantContext(fileContext);
    }
    
    // 3. Build the prompt
    std::ostringstream prompt;
    prompt << "You are an expert programmer. Complete the code at the cursor position.\n\n";
    
    if (!ragContext.empty()) {
        prompt << "Relevant context from the workspace:\n";
        prompt << "```\n" << ragContext << "\n```\n\n";
    }
    
    prompt << "Current file:\n";
    prompt << "```\n" << fileContext << "\n```\n\n";
    prompt << "Continue from the cursor position. Output only the code, no explanations.";
    
    return prompt.str();
}

std::string SovereignCursor::RetrieveRelevantContext(const std::string& query) {
    if (!embeddings_) return "";
    
    // TODO: Generate embedding for query, search HNSW index, return top-k chunks
    // This is a placeholder - real implementation would use the embedding engine
    
    return "";
}

std::string SovereignCursor::GetBufferContext(int cursorPos, int maxChars) {
    if (!buffer_) return "";
    
    // Get text around cursor
    int start = std::max(0, cursorPos - maxChars / 2);
    int len = std::min(maxChars, buffer_->length() - start);
    
    auto text = buffer_->substring(start, len);
    auto ws = text.toStdWString();
    
    // Mark cursor position
    int cursorInContext = cursorPos - start;
    if (cursorInContext >= 0 && cursorInContext <= (int)ws.length()) {
        ws.insert(cursorInContext, L"<|cursor|>");
    }
    
    return WstringToUtf8(ws);
}

// ============================================================================
// Suggestion Generation
// ============================================================================

AISuggestion SovereignCursor::GenerateCompletion(int cursorPos) {
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Build prompt
    std::string prompt = BuildContextPrompt(cursorPos);
    
    // Run inference
    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"user", prompt});
    
    auto result = inference_->ChatSync(messages);
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    
    if (!result.success || result.response.empty()) {
        return AISuggestion{.type = AISuggestion::Type::None};
    }
    
    // Parse the response
    AISuggestion sug;
    sug.type = AISuggestion::Type::Completion;
    sug.text = result.response;
    sug.confidence = 0.85f;  // Could compute from logprobs
    sug.insertPos = cursorPos;
    sug.replaceLen = 0;
    sug.latencyUs = latency;
    sug.tokensGenerated = result.tokens_generated;
    sug.explanation = "AI completion";
    
    UpdateStats(sug);
    
    return sug;
}

AISuggestion SovereignCursor::GenerateEdit(int cursorPos, const std::wstring& instruction) {
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Get selected text or context around cursor
    int selStart = selectionStart_.load();
    int selEnd = selectionEnd_.load();
    
    std::string selectedText;
    if (selEnd > selStart) {
        selectedText = WstringToUtf8(
            buffer_->substring(selStart, selEnd - selStart).toStdWString()
        );
    }
    
    // Build edit prompt
    std::ostringstream prompt;
    prompt << "You are an expert programmer. " << WstringToUtf8(instruction) << "\n\n";
    
    if (!selectedText.empty()) {
        prompt << "Selected code:\n```\n" << selectedText << "\n```\n\n";
    }
    
    prompt << "Output only the modified code, no explanations.";
    
    // Run inference
    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"user", prompt.str()});
    
    auto result = inference_->ChatSync(messages);
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    
    if (!result.success || result.response.empty()) {
        return AISuggestion{.type = AISuggestion::Type::None};
    }
    
    // Generate diff between original and suggested
    AISuggestion sug;
    sug.type = AISuggestion::Type::Diff;
    sug.text = result.response;
    sug.confidence = 0.80f;
    sug.insertPos = selStart > 0 ? selStart : cursorPos;
    sug.replaceLen = selEnd > selStart ? (selEnd - selStart) : 0;
    sug.latencyUs = latency;
    sug.tokensGenerated = result.tokens_generated;
    sug.explanation = WstringToUtf8(instruction);
    
    // Compute diff for preview
    auto oldText = selectedText.empty() ? "" : selectedText;
    sug.diff = diffEngine_.diffLines(oldText, result.response);
    
    UpdateStats(sug);
    
    return sug;
}

// ============================================================================
// Diff Application
// ============================================================================

bool SovereignCursor::ApplyDiffToBuffer(const Core::Diff::DiffResult& diff) {
    if (!buffer_) return false;
    
    // Apply hunks in reverse order (so line numbers stay valid)
    for (auto it = diff.hunks.rbegin(); it != diff.hunks.rend(); ++it) {
        const auto& hunk = *it;
        
        // Calculate position in buffer
        int pos = buffer_->getLineStart((int)hunk.oldStart);
        
        // Apply edits within hunk
        for (const auto& edit : hunk.edits) {
            switch (edit.op) {
                case Core::Diff::DiffOp::Delete: {
                    buffer_->remove(pos, (int)edit.oldLen);
                    break;
                }
                case Core::Diff::DiffOp::Insert: {
                    auto text = Utf8ToWstring(edit.text);
                    buffer_->insert(pos, String(text.c_str()));
                    pos += (int)text.length();
                    break;
                }
                case Core::Diff::DiffOp::Replace: {
                    buffer_->remove(pos, (int)edit.oldLen);
                    auto text = Utf8ToWstring(edit.text);
                    buffer_->insert(pos, String(text.c_str()));
                    pos += (int)text.length();
                    break;
                }
                default:
                    pos += (int)edit.oldLen;
                    break;
            }
        }
    }
    
    return true;
}

// ============================================================================
// Helpers
// ============================================================================

std::string SovereignCursor::WstringToUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(),
                                    nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring SovereignCursor::Utf8ToWstring(const std::string& s) {
    if (s.empty()) return L"";
    
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(),
                                    nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(),
                        result.data(), size);
    return result;
}

void SovereignCursor::UpdateStats(const AISuggestion& sug) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    if (sug.type == AISuggestion::Type::Completion) {
        stats_.totalCompletions++;
    } else if (sug.type == AISuggestion::Type::Edit ||
               sug.type == AISuggestion::Type::Diff) {
        stats_.totalEdits++;
    }
    
    stats_.totalTokensGenerated += sug.tokensGenerated;
    
    // Update EMA latency
    double latencyMs = sug.latencyUs / 1000.0;
    if (stats_.avgLatencyMs == 0.0) {
        stats_.avgLatencyMs = latencyMs;
    } else {
        stats_.avgLatencyMs = 0.9 * stats_.avgLatencyMs + 0.1 * latencyMs;
    }
}

void SovereignCursor::ReportProgress(const std::string& msg) {
    if (onProgress_) {
        onProgress_(msg);
    }
}

} // namespace Sovereign
} // namespace RawrXD
