// ============================================================================
// SovereignCursor.cpp — AI Editor Brain Implementation
// ============================================================================
// The full loop: Capture → Embed → Retrieve → Prompt → Infer → Parse → Apply
//
// Pattern: Worker thread for async ops, fail-closed on all paths.
// ============================================================================

#include "SovereignCursor.h"
#include "../engine/global_runtime_orchestrator.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

// Forward declarations for embedding C API
extern "C" {
    bool RawrXD_AI_InitEmbeddingProvider(const char* modelPath, int dimensions);
    bool RawrXD_AI_Embed(const char* text, float* output, int outputSize);
}

namespace RawrXD {
namespace AI {

// ============================================================================
// Construction / Destruction
// ============================================================================

SovereignCursor::SovereignCursor(const SovereignCursorConfig& cfg)
    : config_(cfg)
{
}

SovereignCursor::~SovereignCursor() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool SovereignCursor::Initialize() {
    if (llm_) return true; // Already initialized

    ReportProgress("Initializing Sovereign Cursor...");

    // 1. Initialize LLM
    Agent::SovereignModelConfig llmCfg;
    llmCfg.model_path = config_.modelPath;
    llmCfg.context_size = config_.contextSize;
    llmCfg.temperature = config_.temperature;
    llmCfg.max_tokens = config_.maxTokens;
    llmCfg.enable_speculative = config_.speculativeDecode;
    llmCfg.draft_tokens = config_.draftTokens;

    llm_ = std::make_unique<Agent::SovereignInferenceClient>(llmCfg);
    if (!llm_->LoadModel(config_.modelPath)) {
        ReportProgress("Failed to load model: " + config_.modelPath);
        llm_.reset();
        return false;
    }

    // 2. Initialize vector store
    vectorStore_ = std::make_unique<SovereignVectorStore>(
        config_.embeddingDim,
        100000,  // max chunks
        512 * 1024 * 1024  // 512MB R15 reserve
    );

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

    if (llm_) {
        llm_->UnloadModel();
        llm_.reset();
    }

    vectorStore_.reset();
    buffer_ = nullptr;
}

bool SovereignCursor::IsReady() const {
    return llm_ && llm_->IsLoaded() && running_.load();
}

// ============================================================================
// Editor binding
// ============================================================================

void SovereignCursor::BindBuffer(Editor::SovereignGapBuffer* buffer) {
    buffer_ = buffer;
}

void SovereignCursor::UnbindBuffer() {
    buffer_ = nullptr;
}

// ============================================================================
// AI operations (enqueue async tasks)
// ============================================================================

void SovereignCursor::RequestCompletion() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::Completion, "", CaptureContext()});
    queueCv_.notify_one();
}

void SovereignCursor::RequestInlineSuggestion() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::InlineSuggestion, "", CaptureContext()});
    queueCv_.notify_one();
}

void SovereignCursor::RequestRefactoring() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::Refactoring, "", CaptureContext()});
    queueCv_.notify_one();
}

void SovereignCursor::RequestExplanation() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::Explanation, "", CaptureContext()});
    queueCv_.notify_one();
}

void SovereignCursor::RequestFix() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::Fix, "", CaptureContext()});
    queueCv_.notify_one();
}

void SovereignCursor::RequestDiff(const std::string& instruction) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    taskQueue_.push_back({TaskType::Diff, instruction, CaptureContext()});
    queueCv_.notify_one();
}

// ============================================================================
// Synchronous helpers
// ============================================================================

AISuggestion SovereignCursor::SuggestCompletionSync() {
    Task task{TaskType::Completion, "", CaptureContext()};
    ProcessTask(task);
    // Note: In a real implementation, we'd capture the result from the callback.
    // For sync, we could use a promise/future. This is a simplified version.
    return AISuggestion{};
}

std::string SovereignCursor::ExplainSelectionSync() {
    Task task{TaskType::Explanation, "", CaptureContext()};
    ProcessTask(task);
    return "";
}

// ============================================================================
// RAG indexing
// ============================================================================

void SovereignCursor::IndexWorkspace(const std::string& path) {
    ReportProgress("Indexing workspace: " + path);
    // TODO: Walk directory, parse files, extract functions, generate embeddings
    // For now, stub
    (void)path;
}

void SovereignCursor::IndexFile(const std::string& path,
                                 const std::string& content) {
    auto& orch = GlobalRuntimeOrchestrator::Get();
    float risk = orch.AssessRisk();
    
    // Safety Gate: Skip indexing if system is under high pressure (risk > 0.4)
    // and wait for a "Safety Pulse" during lower risk moments.
    if (risk > 0.40f) {
        // Log telemetry that indexing was throttled for safety
        return;
    }

    // Initialize embedding provider if needed
    static bool provider_init = false;
    if (!provider_init) {
        RawrXD_AI_InitEmbeddingProvider(config_.modelPath.c_str(), config_.embeddingDim);
        provider_init = true;
    }

    std::vector<float> embedding(config_.embeddingDim);
    if (!RawrXD_AI_Embed(content.c_str(), embedding.data(), (int)config_.embeddingDim)) {
        return;
    }

    if (vectorStore_) {
        EmbeddingChunk chunk;
        chunk.id = std::hash<std::string>{}(path + content);
        chunk.fileId = std::hash<std::string>{}(path);
        chunk.name = path;
        chunk.codeLength = content.length();
        chunk.dim = config_.embeddingDim;
        
        vectorStore_->AddChunk(chunk, embedding.data());
    }
}

void SovereignCursor::ClearIndex() {
    if (vectorStore_) {
        vectorStore_->Clear();
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void SovereignCursor::SetSuggestionCallback(SuggestionCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    suggestionCb_ = cb;
}

void SovereignCursor::SetProgressCallback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    progressCb_ = cb;
}

// ============================================================================
// Stats
// ============================================================================

SovereignCursor::Stats SovereignCursor::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    Stats s = stats_;
    if (vectorStore_) {
        s.indexChunkCount = vectorStore_->GetChunkCount();
        s.indexMemoryBytes = vectorStore_->GetMemoryBytes();
    }
    return s;
}

// ============================================================================
// Worker thread
// ============================================================================

void SovereignCursor::WorkerLoop() {
    while (running_.load()) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait(lock, [this] {
                return !taskQueue_.empty() || !running_.load();
            });

            if (!running_.load()) break;
            if (taskQueue_.empty()) continue;

            task = taskQueue_.front();
            taskQueue_.erase(taskQueue_.begin());
        }

        ProcessTask(task);
    }
}

// ============================================================================
// Task processing — THE AI LOOP
// ============================================================================

void SovereignCursor::ProcessTask(const Task& task) {
    if (!llm_ || !llm_->IsLoaded()) {
        ReportProgress("LLM not loaded");
        return;
    }

    auto t0 = std::chrono::high_resolution_clock::now();

    // 1. Build prompt with context
    std::string prompt = BuildPrompt(task);
    if (prompt.empty()) {
        ReportProgress("Empty prompt");
        return;
    }

    ReportProgress("Inferring...");

    // 2. Run inference
    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"system", BuildSystemPrompt()});
    messages.push_back({"user", prompt});

    auto result = llm_->ChatSync(messages);

    if (!result.error_message.empty()) {
        ReportProgress("Inference error: " + result.error_message);
        return;
    }

    // 3. Parse response
    AISuggestion suggestion = ParseResponse(result.response, task.type);

    auto t1 = std::chrono::high_resolution_clock::now();
    double latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    UpdateStats(result.completion_tokens, latencyMs);

    ReportProgress("Suggestion ready (" +
                   std::to_string(static_cast<int>(latencyMs)) + "ms)");

    // 4. Deliver to editor
    {
        std::lock_guard<std::mutex> lock(cbMutex_);
        if (suggestionCb_) {
            suggestionCb_(suggestion);
        }
    }

    // 5. Auto-apply if diff
    if (suggestion.isDiff && buffer_) {
        ApplySuggestion(suggestion);
    }
}

// ============================================================================
// Prompt engineering
// ============================================================================

std::string SovereignCursor::BuildSystemPrompt() {
    return R"(You are SovereignCursor, an AI coding assistant embedded in the RawrXD IDE.
You have direct access to the editor's memory and can suggest code changes.

Rules:
1. Respond with ONLY code or diffs. No explanations unless asked.
2. For completions: continue the code naturally from the cursor.
3. For diffs: use unified diff format (@@ -line,count +line,count @@).
4. For fixes: provide minimal changes that resolve the issue.
5. Respect the existing code style and conventions.
6. Never output markdown code blocks unless specifically requested.

Language detection is automatic. Context is retrieved from the workspace index.
)";
}

std::string SovereignCursor::BuildPrompt(const Task& task) {
    std::ostringstream oss;

    // File context
    if (!task.context.fileName.empty()) {
        oss << "File: " << task.context.fileName << "\n";
    }
    if (!task.context.language.empty()) {
        oss << "Language: " << task.context.language << "\n";
    }
    oss << "\n";

    // RAG context
    std::string ragContext = RetrieveRAGContext(
        task.context.beforeCursor + " " + task.context.selectedText
    );
    if (!ragContext.empty()) {
        oss << "Relevant workspace context:\n";
        oss << ragContext;
        oss << "\n---\n\n";
    }

    // Editor context
    oss << "Code before cursor:\n";
    oss << "```\n";
    // Limit to last N lines
    std::string before = task.context.beforeCursor;
    size_t maxLines = 50;
    size_t lineCount = 0;
    size_t pos = before.size();
    while (pos > 0 && lineCount < maxLines) {
        pos = before.rfind('\n', pos - 1);
        if (pos == std::string::npos) {
            pos = 0;
            break;
        }
        ++lineCount;
    }
    oss << before.substr(pos);
    oss << "\n```\n\n";

    if (!task.context.afterCursor.empty()) {
        oss << "Code after cursor:\n";
        oss << "```\n";
        oss << task.context.afterCursor.substr(0, 500);
        oss << "\n```\n\n";
    }

    if (!task.context.selectedText.empty()) {
        oss << "Selected code:\n";
        oss << "```\n";
        oss << task.context.selectedText;
        oss << "\n```\n\n";
    }

    // Task-specific instruction
    switch (task.type) {
        case TaskType::Completion:
            oss << "Complete the code at the cursor position. "
                  << "Continue naturally from where the code ends.\n";
            break;
        case TaskType::InlineSuggestion:
            oss << "Suggest the next few tokens of code. "
                  << "Output ONLY the suggested text, no explanations.\n";
            break;
        case TaskType::Refactoring:
            oss << "Refactor the selected code to improve clarity, "
                  << "performance, or maintainability.\n";
            break;
        case TaskType::Explanation:
            oss << "Explain what the selected code does in 1-2 sentences.\n";
            break;
        case TaskType::Fix:
            oss << "Fix any bugs or issues in the selected code. "
                  << "Provide a unified diff of the changes.\n";
            break;
        case TaskType::Diff:
            oss << "Apply the following instruction to the code:\n";
            oss << task.instruction << "\n";
            oss << "Provide a unified diff of the changes.\n";
            break;
    }

    return oss.str();
}

// ============================================================================
// Context capture
// ============================================================================

CursorContext SovereignCursor::CaptureContext() {
    CursorContext ctx;
    if (!buffer_) return ctx;

    ctx.beforeCursor = std::string(buffer_->BeforeCursor());
    ctx.afterCursor = std::string(buffer_->AfterCursor());
    ctx.lineNumber = buffer_->GetLineFromOffset(buffer_->GetCursor());

    // Detect language from file extension
    if (!ctx.fileName.empty()) {
        size_t dot = ctx.fileName.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = ctx.fileName.substr(dot + 1);
            if (ext == "cpp" || ext == "hpp" || ext == "h") ctx.language = "cpp";
            else if (ext == "c") ctx.language = "c";
            else if (ext == "py") ctx.language = "python";
            else if (ext == "js" || ext == "ts") ctx.language = "javascript";
            else if (ext == "rs") ctx.language = "rust";
            else if (ext == "go") ctx.language = "go";
            else if (ext == "java") ctx.language = "java";
            else if (ext == "asm") ctx.language = "asm";
        }
    }

    return ctx;
}

// ============================================================================
// RAG retrieval
// ============================================================================

std::string SovereignCursor::RetrieveRAGContext(const std::string& query) {
    if (!vectorStore_ || query.empty()) return "";

    // TODO: Generate embedding for query using local embedding model
    // For now, return empty (will be implemented when embedding model is ready)
    (void)query;
    return "";
}

std::vector<float> SovereignCursor::EmbedText(const std::string& text) {
    (void)text;
    // TODO: Call local embedding model (e.g., MiniLM via ONNX or custom)
    return {};
}

// ============================================================================
// Response parsing
// ============================================================================

AISuggestion SovereignCursor::ParseResponse(const std::string& response,
                                             TaskType type) {
    AISuggestion suggestion;
    suggestion.text = response;
    suggestion.confidence = 0.85f;

    // Check if response looks like a unified diff
    if (response.find("@@ -") != std::string::npos &&
        response.find("@@ +") != std::string::npos) {
        suggestion.isDiff = true;

        // Parse diff using DiffEngine
        // For now, mark as diff and let editor handle application
        suggestion.reasoning = "Diff-based suggestion";
    } else {
        suggestion.isDiff = false;
        switch (type) {
            case TaskType::Completion:
            case TaskType::InlineSuggestion:
                suggestion.reasoning = "Code completion";
                break;
            case TaskType::Refactoring:
                suggestion.reasoning = "Refactoring suggestion";
                break;
            case TaskType::Explanation:
                suggestion.reasoning = "Code explanation";
                break;
            case TaskType::Fix:
                suggestion.reasoning = "Bug fix suggestion";
                break;
            case TaskType::Diff:
                suggestion.reasoning = "Instruction-based edit";
                break;
        }
    }

    return suggestion;
}

// ============================================================================
// Patch application
// ============================================================================

bool SovereignCursor::ApplySuggestion(const AISuggestion& suggestion) {
    if (!buffer_ || !suggestion.isDiff) return false;

    // TODO: Parse unified diff and apply to gap buffer
    // This would use DiffEngine::ComputeDiff + ApplyHunk
    (void)suggestion;
    return false;
}

// ============================================================================
// Stats / progress
// ============================================================================

void SovereignCursor::UpdateStats(uint64_t tokens, double latencyMs) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.totalRequests++;
    stats_.totalTokensGenerated += tokens;
    if (stats_.totalRequests == 1) {
        stats_.avgLatencyMs = latencyMs;
    } else {
        stats_.avgLatencyMs = 0.9 * stats_.avgLatencyMs + 0.1 * latencyMs;
    }
}

void SovereignCursor::ReportProgress(const std::string& msg) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    if (progressCb_) {
        progressCb_(msg);
    }
}

} // namespace AI
} // namespace RawrXD
