#include "ai_assistant_engine.h"
#include "../cpu_inference_engine.h"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <random>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {
namespace AI {

// ============================================================================
// AIAssistantEngine Implementation
// ============================================================================

AIAssistantEngine::AIAssistantEngine()
    : m_initialized(false)
    , m_shutdown_requested(false)
    , m_total_suggestions(0)
    , m_accepted_suggestions(0)
    , m_rejected_suggestions(0)
{
}

AIAssistantEngine::~AIAssistantEngine() {
    Shutdown();
}

bool AIAssistantEngine::Initialize(const ModelConfig& config) {
    if (m_initialized) return true;

    m_current_config = config;

    // Create appropriate backend based on provider
    switch (config.provider) {
        case ModelProvider::Local_GGUF:
            m_backend = std::make_unique<GGUFBackend>();
            break;
        case ModelProvider::Ollama:
            m_backend = std::make_unique<OllamaBackend>();
            break;
        case ModelProvider::OpenAI_Compatible:
        case ModelProvider::Anthropic:
            m_backend = std::make_unique<OpenAIBackend>();
            break;
        default:
            if (m_error_callback) {
                m_error_callback("Unsupported model provider");
            }
            return false;
    }

    if (!m_backend->Initialize(config)) {
        if (m_error_callback) {
            m_error_callback("Failed to initialize model backend");
        }
        return false;
    }

    // Start worker threads for async operations
    int num_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i) {
        m_worker_threads.emplace_back(&AIAssistantEngine::WorkerThread, this);
    }

    m_initialized = true;
    return true;
}

void AIAssistantEngine::Shutdown() {
    if (!m_initialized) return;

    m_shutdown_requested = true;
    m_queue_cv.notify_all();

    for (auto& thread : m_worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (m_backend) {
        m_backend->Shutdown();
    }

    m_initialized = false;
}

bool AIAssistantEngine::LoadModel(const std::string& model_path) {
    ModelConfig config = m_current_config;
    config.model_name = model_path;
    return SwitchModel(config);
}

bool AIAssistantEngine::SwitchModel(const ModelConfig& new_config) {
    if (m_backend) {
        m_backend->Shutdown();
    }

    return Initialize(new_config);
}

std::vector<std::string> AIAssistantEngine::ListAvailableModels() const {
    // TODO: Scan local models directory, query Ollama, etc.
    return {
        "codellama-7b-instruct.Q4_K_M.gguf",
        "deepseek-coder-6.7b-instruct.Q4_K_M.gguf",
        "qwen2.5-coder-7b-instruct.Q4_K_M.gguf",
        "ollama:codellama:7b",
        "ollama:deepseek-coder:6.7b"
    };
}

// ============================================================================
// Inline Completion (GitHub Copilot style)
// ============================================================================

void AIAssistantEngine::RequestInlineCompletion(const CodeContext& context,
                                                SuggestionCallback callback) {
    if (!m_initialized) {
        if (m_error_callback) {
            m_error_callback("AI engine not initialized");
        }
        return;
    }

    // Enqueue async task
    EnqueueTask([this, context, callback]() {
        ProcessInlineRequest(context, callback);
    });
}

void AIAssistantEngine::ProcessInlineRequest(const CodeContext& context,
                                             SuggestionCallback callback) {
    std::string prompt = BuildPrompt(context, AssistanceMode::InlineComplete);
    std::string response = CallModel(prompt, 150);  // Shorter for inline suggestions

    AISuggestion suggestion = ParseInlineSuggestion(response, context);
    
    if (callback) {
        callback(suggestion);
    }

    m_total_suggestions++;
}

AISuggestion AIAssistantEngine::ParseInlineSuggestion(const std::string& model_response,
                                                      const CodeContext& context) {
    AISuggestion suggestion;
    suggestion.insert_line = context.cursor_line;
    suggestion.insert_column = context.cursor_column;

    // Extract code from response (remove markdown, explanations)
    std::string clean_response = model_response;
    
    // Remove ```language and ``` markers
    size_t start = clean_response.find("```");
    if (start != std::string::npos) {
        size_t newline = clean_response.find('\n', start);
        if (newline != std::string::npos) {
            clean_response = clean_response.substr(newline + 1);
        }
        size_t end = clean_response.rfind("```");
        if (end != std::string::npos) {
            clean_response = clean_response.substr(0, end);
        }
    }

    // Trim whitespace
    clean_response.erase(0, clean_response.find_first_not_of(" \t\n\r"));
    clean_response.erase(clean_response.find_last_not_of(" \t\n\r") + 1);

    suggestion.suggestion_text = clean_response;
    suggestion.is_multiline = (std::count(clean_response.begin(), clean_response.end(), '\n') > 0);
    suggestion.confidence = 0.85f;  // TODO: Real confidence scoring
    suggestion.reasoning = "AI-generated code completion";

    return suggestion;
}

void AIAssistantEngine::AcceptSuggestion(const AISuggestion& suggestion) {
    m_accepted_suggestions++;
    // Log for future training/fine-tuning
    AddRecentEdit("Accepted: " + suggestion.suggestion_text.substr(0, 50));
}

void AIAssistantEngine::RejectSuggestion(const AISuggestion& suggestion) {
    m_rejected_suggestions++;
}

void AIAssistantEngine::CancelInlineCompletion() {
    // TODO: Implement cancellation mechanism
}

// ============================================================================
// Chat Mode (Cursor Chat / Copilot Chat)
// ============================================================================

std::string AIAssistantEngine::StartChatSession() {
    std::lock_guard<std::mutex> lock(m_chat_mutex);
    
    // Generate unique session ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::string session_id = "chat_" + std::to_string(timestamp);

    m_chat_sessions[session_id] = std::vector<ChatMessage>();

    // Add system message
    ChatMessage system_msg;
    system_msg.role = ChatMessage::Role::System;
    system_msg.content = "You are an expert coding assistant integrated into the RawrXD IDE. "
                        "Help the user with code questions, debugging, refactoring, and implementation. "
                        "Be concise and practical. Provide code examples when relevant.";
    system_msg.timestamp = GetCurrentTimestamp();
    m_chat_sessions[session_id].push_back(system_msg);

    return session_id;
}

void AIAssistantEngine::SendChatMessage(const std::string& session_id,
                                       const std::string& message,
                                       const CodeContext& context,
                                       ChatCallback callback) {
    if (!m_initialized) {
        if (m_error_callback) {
            m_error_callback("AI engine not initialized");
        }
        return;
    }

    EnqueueTask([this, session_id, message, context, callback]() {
        ProcessChatRequest(session_id, message, context, callback);
    });
}

void AIAssistantEngine::ProcessChatRequest(const std::string& session_id,
                                          const std::string& message,
                                          const CodeContext& context,
                                          ChatCallback callback) {
    std::lock_guard<std::mutex> lock(m_chat_mutex);

    // Add user message to history
    ChatMessage user_msg;
    user_msg.role = ChatMessage::Role::User;
    user_msg.content = message;
    user_msg.timestamp = GetCurrentTimestamp();
    m_chat_sessions[session_id].push_back(user_msg);

    // Build prompt with conversation history and context
    std::stringstream prompt_ss;
    
    // Add conversation history
    for (const auto& msg : m_chat_sessions[session_id]) {
        if (msg.role == ChatMessage::Role::System) {
            prompt_ss << "System: " << msg.content << "\n\n";
        } else if (msg.role == ChatMessage::Role::User) {
            prompt_ss << "User: " << msg.content << "\n\n";
        } else {
            prompt_ss << "Assistant: " << msg.content << "\n\n";
        }
    }

    // Add code context if available
    if (!context.selected_text.empty()) {
        prompt_ss << "\nSelected Code:\n```" << context.language << "\n"
                  << context.selected_text << "\n```\n\n";
    }

    prompt_ss << "Assistant: ";

    // Call model
    std::string response = CallModel(prompt_ss.str(), 1024);

    // Add assistant response to history
    ChatMessage assistant_msg;
    assistant_msg.role = ChatMessage::Role::Assistant;
    assistant_msg.content = response;
    assistant_msg.timestamp = GetCurrentTimestamp();
    m_chat_sessions[session_id].push_back(assistant_msg);

    if (callback) {
        callback(assistant_msg);
    }
}

std::vector<ChatMessage> AIAssistantEngine::GetChatHistory(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_chat_mutex));
    auto it = m_chat_sessions.find(session_id);
    if (it != m_chat_sessions.end()) {
        return it->second;
    }
    return {};
}

void AIAssistantEngine::ClearChatSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_chat_mutex);
    m_chat_sessions.erase(session_id);
}

// ============================================================================
// Inline Edit Mode (Cursor Cmd+K style)
// ============================================================================

void AIAssistantEngine::RequestEdit(const std::string& instruction,
                                   const CodeContext& context,
                                   EditCallback callback) {
    if (!m_initialized) {
        if (m_error_callback) {
            m_error_callback("AI engine not initialized");
        }
        return;
    }

    EnqueueTask([this, instruction, context, callback]() {
        ProcessEditRequest(instruction, context, callback);
    });
}

void AIAssistantEngine::ProcessEditRequest(const std::string& instruction,
                                          const CodeContext& context,
                                          EditCallback callback) {
    std::stringstream prompt_ss;
    prompt_ss << "You are a code editing assistant. The user has selected code and wants to edit it.\n\n";
    prompt_ss << "Original Code:\n```" << context.language << "\n"
              << context.selected_text << "\n```\n\n";
    prompt_ss << "User Instruction: " << instruction << "\n\n";
    prompt_ss << "Provide the edited code. Only output the code, no explanations:\n```" 
              << context.language << "\n";

    std::string response = CallModel(prompt_ss.str(), 512);
    EditOperation edit = ParseEditOperation(response, context);
    edit.instruction = instruction;

    if (callback) {
        callback(edit);
    }
}

EditOperation AIAssistantEngine::ParseEditOperation(const std::string& model_response,
                                                   const CodeContext& context) {
    EditOperation edit;
    edit.original_text = context.selected_text;
    edit.start_line = context.cursor_line;
    
    // Count lines in selected text
    int line_count = std::count(context.selected_text.begin(), context.selected_text.end(), '\n');
    edit.end_line = context.cursor_line + line_count;

    // Extract code from response
    std::string clean_response = model_response;
    size_t start = clean_response.find("```");
    if (start != std::string::npos) {
        size_t newline = clean_response.find('\n', start);
        if (newline != std::string::npos) {
            clean_response = clean_response.substr(newline + 1);
        }
        size_t end = clean_response.rfind("```");
        if (end != std::string::npos) {
            clean_response = clean_response.substr(0, end);
        }
    }

    clean_response.erase(0, clean_response.find_first_not_of(" \t\n\r"));
    clean_response.erase(clean_response.find_last_not_of(" \t\n\r") + 1);

    edit.new_text = clean_response;
    edit.confidence = 0.90f;

    return edit;
}

void AIAssistantEngine::ApplyEdit(const EditOperation& edit) {
    AddRecentEdit("Applied edit: " + edit.instruction);
}

void AIAssistantEngine::UndoLastEdit() {
    // TODO: Implement undo mechanism
}

// ============================================================================
// Agent Mode (Autonomous coding)
// ============================================================================

std::string AIAssistantEngine::CreateAgentTask(const std::string& task_description,
                                              const CodeContext& context) {
    std::lock_guard<std::mutex> lock(m_agent_mutex);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::string task_id = "agent_" + std::to_string(timestamp);

    AgentTask task;
    task.task_id = task_id;
    task.description = task_description;
    task.status = AgentTask::Status::Pending;
    task.current_step = 0;

    m_agent_tasks[task_id] = task;
    return task_id;
}

void AIAssistantEngine::StartAgent(const std::string& task_id, AgentCallback callback) {
    EnqueueTask([this, task_id, callback]() {
        ProcessAgentTask(task_id);
        if (callback) {
            callback(GetAgentStatus(task_id));
        }
    });
}

void AIAssistantEngine::ProcessAgentTask(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_agent_mutex);

    auto it = m_agent_tasks.find(task_id);
    if (it == m_agent_tasks.end()) return;

    AgentTask& task = it->second;
    task.status = AgentTask::Status::Running;

    // Step 1: Generate plan
    std::stringstream plan_prompt;
    plan_prompt << "You are an autonomous coding agent. Break down this task into steps:\n\n";
    plan_prompt << "Task: " << task.description << "\n\n";
    plan_prompt << "Provide a numbered list of concrete steps to complete this task:\n";

    std::string plan_response = CallModel(plan_prompt.str(), 512);
    task.plan = plan_response;

    // Parse steps from response
    std::istringstream iss(plan_response);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && (line[0] == '1' || line[0] == '2' || line[0] == '3' ||
                             line[0] == '4' || line[0] == '5')) {
            task.steps.push_back(line);
        }
    }

    // TODO: Execute each step, generate code, apply edits
    // For now, mark as completed
    task.status = AgentTask::Status::Completed;
}

void AIAssistantEngine::PauseAgent(const std::string& task_id) {
    // TODO: Implement pause mechanism
}

void AIAssistantEngine::StopAgent(const std::string& task_id) {
    std::lock_guard<std::mutex> lock(m_agent_mutex);
    auto it = m_agent_tasks.find(task_id);
    if (it != m_agent_tasks.end()) {
        it->second.status = AgentTask::Status::Failed;
        it->second.error_message = "Stopped by user";
    }
}

AgentTask AIAssistantEngine::GetAgentStatus(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_agent_mutex));
    auto it = m_agent_tasks.find(task_id);
    if (it != m_agent_tasks.end()) {
        return it->second;
    }
    return AgentTask{};
}

// ============================================================================
// Code Analysis
// ============================================================================

std::string AIAssistantEngine::ExplainCode(const std::string& code, const std::string& language) {
    std::stringstream prompt;
    prompt << "Explain this " << language << " code in clear, concise terms:\n\n```" 
           << language << "\n" << code << "\n```\n\nExplanation:\n";
    return CallModel(prompt.str(), 512);
}

std::vector<std::string> AIAssistantEngine::SuggestRefactorings(const std::string& code,
                                                                const std::string& language) {
    std::stringstream prompt;
    prompt << "Suggest specific refactorings for this " << language << " code:\n\n```"
           << language << "\n" << code << "\n```\n\n";
    prompt << "List 3-5 concrete refactoring suggestions, one per line:\n";
    
    std::string response = CallModel(prompt.str(), 512);
    
    // Parse line-separated suggestions
    std::vector<std::string> suggestions;
    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
            suggestions.push_back(line);
        }
    }
    return suggestions;
}

std::string AIAssistantEngine::GenerateTests(const std::string& code, const std::string& language) {
    std::stringstream prompt;
    prompt << "Generate comprehensive unit tests for this " << language << " code:\n\n```"
           << language << "\n" << code << "\n```\n\nTests:\n```" << language << "\n";
    return CallModel(prompt.str(), 1024);
}

std::string AIAssistantEngine::GenerateDocumentation(const std::string& code,
                                                    const std::string& language) {
    std::stringstream prompt;
    prompt << "Generate documentation comments for this " << language << " code:\n\n```"
           << language << "\n" << code << "\n```\n\nDocumented code:\n```" << language << "\n";
    return CallModel(prompt.str(), 1024);
}

std::vector<std::string> AIAssistantEngine::FindBugs(const std::string& code,
                                                     const std::string& language) {
    std::stringstream prompt;
    prompt << "Analyze this " << language << " code for bugs, security issues, and anti-patterns:\n\n```"
           << language << "\n" << code << "\n```\n\n";
    prompt << "List each issue found, one per line:\n";
    
    std::string response = CallModel(prompt.str(), 512);
    
    std::vector<std::string> bugs;
    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
            bugs.push_back(line);
        }
    }
    return bugs;
}

std::string AIAssistantEngine::OptimizeCode(const std::string& code, const std::string& language) {
    std::stringstream prompt;
    prompt << "Optimize this " << language << " code for performance:\n\n```"
           << language << "\n" << code << "\n```\n\nOptimized code:\n```" << language << "\n";
    return CallModel(prompt.str(), 1024);
}

// ============================================================================
// Context Management
// ============================================================================

void AIAssistantEngine::UpdateWorkspaceContext(const std::map<std::string, std::string>& files) {
    std::lock_guard<std::mutex> lock(m_context_mutex);
    m_workspace_context = files;
}

void AIAssistantEngine::AddRecentEdit(const std::string& edit_description) {
    std::lock_guard<std::mutex> lock(m_context_mutex);
    m_recent_edits.push_back(edit_description);
    if (m_recent_edits.size() > 50) {  // Keep last 50 edits
        m_recent_edits.erase(m_recent_edits.begin());
    }
}

void AIAssistantEngine::ClearContext() {
    std::lock_guard<std::mutex> lock(m_context_mutex);
    m_workspace_context.clear();
    m_recent_edits.clear();
}

// ============================================================================
// Internal Methods
// ============================================================================

std::string AIAssistantEngine::BuildPrompt(const CodeContext& context,
                                          AssistanceMode mode,
                                          const std::string& user_input) {
    std::stringstream prompt;

    switch (mode) {
        case AssistanceMode::InlineComplete:
            prompt << "Complete the following " << context.language << " code:\n\n";
            
            // Add file content before cursor
            if (!context.file_content.empty()) {
                std::istringstream iss(context.file_content);
                std::string line;
                int current_line = 0;
                while (std::getline(iss, line) && current_line < context.cursor_line) {
                    prompt << line << "\n";
                    current_line++;
                }
                // Add current line up to cursor
                if (current_line == context.cursor_line) {
                    prompt << line.substr(0, context.cursor_column);
                }
            }
            
            prompt << "<CURSOR>";  // Mark cursor position
            prompt << "\n\nComplete the code at <CURSOR> position. Only provide the completion:\n";
            break;

        case AssistanceMode::ChatMode:
            prompt << user_input;  // Chat mode handled separately
            break;

        case AssistanceMode::EditMode:
            prompt << user_input;  // Edit mode handled separately
            break;

        default:
            prompt << user_input;
            break;
    }

    return prompt.str();
}

std::string AIAssistantEngine::CallModel(const std::string& prompt, int max_tokens) {
    if (!m_backend || !m_backend->IsReady()) {
        return "Error: Model not ready";
    }

    try {
        return m_backend->Generate(prompt, max_tokens);
    } catch (const std::exception& e) {
        if (m_error_callback) {
            m_error_callback(std::string("Model generation error: ") + e.what());
        }
        return "";
    }
}

void AIAssistantEngine::WorkerThread() {
    while (!m_shutdown_requested) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_queue_cv.wait(lock, [this]() {
                return !m_task_queue.empty() || m_shutdown_requested;
            });

            if (m_shutdown_requested) break;

            if (!m_task_queue.empty()) {
                task = std::move(m_task_queue.front());
                m_task_queue.pop();
            }
        }

        if (task) {
            task();
        }
    }
}

void AIAssistantEngine::EnqueueTask(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.push(std::move(task));
    }
    m_queue_cv.notify_one();
}

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ============================================================================
// GGUFBackend Implementation
// ============================================================================

bool GGUFBackend::Initialize(const ModelConfig& config) {
    m_engine = std::make_unique<CPUInference::CPUInferenceEngine>();
    
    if (!m_engine->LoadModel(config.model_name)) {
        return false;
    }

    m_ready = true;
    return true;
}

std::string GGUFBackend::Generate(const std::string& prompt, int max_tokens) {
    if (!m_ready || !m_engine) {
        return "";
    }

    // Tokenize prompt
    std::vector<int32_t> tokens = m_engine->Tokenize(prompt);

    // Generate response
    std::string response;
    m_engine->GenerateStreaming(
        tokens,
        max_tokens,
        [&response](const std::string& token) {
            response += token;
        },
        []() {},  // Complete callback
        nullptr   // Token ID callback
    );

    return response;
}

void GGUFBackend::Shutdown() {
    m_engine.reset();
    m_ready = false;
}

// ============================================================================
// OllamaBackend Implementation
// ============================================================================

bool OllamaBackend::Initialize(const ModelConfig& config) {
    m_endpoint = config.api_endpoint.empty() ? "http://localhost:11434/api/generate" : config.api_endpoint;
    m_model_name = config.model_name;
    
    // Test connection
    // TODO: Ping Ollama to verify it's running
    
    m_ready = true;
    return true;
}

std::string OllamaBackend::Generate(const std::string& prompt, int max_tokens) {
    if (!m_ready) return "";

    // Build JSON payload
    std::stringstream json;
    json << "{"
         << "\"model\":\"" << m_model_name << "\","
         << "\"prompt\":\"" << EscapeJSON(prompt) << "\","
         << "\"stream\":false,"
         << "\"options\":{\"num_predict\":" << max_tokens << "}"
         << "}";

    std::string response_json = SendHTTPRequest(json.str());
    
    // Parse response (simple extraction, should use proper JSON parser)
    size_t response_pos = response_json.find("\"response\":\"");
    if (response_pos != std::string::npos) {
        size_t start = response_pos + 12;
        size_t end = response_json.find("\"", start);
        if (end != std::string::npos) {
            return response_json.substr(start, end - start);
        }
    }

    return "";
}

std::string OllamaBackend::SendHTTPRequest(const std::string& json_payload) {
    // TODO: Implement HTTP POST using WinHTTP
    return "";
}

void OllamaBackend::Shutdown() {
    m_ready = false;
}

// ============================================================================
// OpenAIBackend Implementation
// ============================================================================

bool OpenAIBackend::Initialize(const ModelConfig& config) {
    m_endpoint = config.api_endpoint.empty() ? "https://api.openai.com/v1/chat/completions" : config.api_endpoint;
    m_api_key = config.api_key;
    m_model_name = config.model_name.empty() ? "gpt-4" : config.model_name;
    
    m_ready = !m_api_key.empty();
    return m_ready;
}

std::string OpenAIBackend::Generate(const std::string& prompt, int max_tokens) {
    if (!m_ready) return "";

    // Build JSON payload (OpenAI Chat Completion format)
    std::stringstream json;
    json << "{"
         << "\"model\":\"" << m_model_name << "\","
         << "\"messages\":[{\"role\":\"user\",\"content\":\"" << EscapeJSON(prompt) << "\"}],"
         << "\"max_tokens\":" << max_tokens
         << "}";

    std::string response_json = SendHTTPRequest(json.str());
    
    // Parse response
    size_t content_pos = response_json.find("\"content\":\"");
    if (content_pos != std::string::npos) {
        size_t start = content_pos + 11;
        size_t end = response_json.find("\"", start);
        if (end != std::string::npos) {
            return response_json.substr(start, end - start);
        }
    }

    return "";
}

std::string OpenAIBackend::SendHTTPRequest(const std::string& json_payload) {
    // TODO: Implement HTTPS POST using WinHTTP
    return "";
}

void OpenAIBackend::Shutdown() {
    m_ready = false;
}

// Helper function
std::string EscapeJSON(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '"' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

} // namespace AI
} // namespace RawrXD
