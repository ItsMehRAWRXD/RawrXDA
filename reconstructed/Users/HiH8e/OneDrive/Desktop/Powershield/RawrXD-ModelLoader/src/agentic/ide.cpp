#include "agentic_ide.h"
#include <iostream>
#include <thread>
#include <chrono>

// SCALAR-ONLY: Integrated autonomous agentic IDE implementation

namespace RawrXD {

AgenticIDE::AgenticIDE() : is_running_(false) {
}

AgenticIDE::~AgenticIDE() {
    Shutdown();
}

bool AgenticIDE::Initialize(const std::string& root_directory) {
    std::cout << "Initializing Scalar Agentic IDE with Chat Workspace..." << std::endl;
    
    root_directory_ = root_directory;

    // Initialize components (scalar)
    server_ = std::make_unique<ScalarServer>(8080);
    file_browser_ = std::make_unique<FileBrowser>();
    chat_interface_ = std::make_unique<ChatInterface>();
    chat_workspace_ = std::make_unique<ChatWorkspace>();
    agentic_engine_ = std::make_unique<AgenticEngine>();
    inference_engine_ = std::make_unique<InferenceEngine>();

    // Set up file browser
    file_browser_->SetRootPath(root_directory_);
    file_browser_->SetOnFileOpen([this](const std::string& path) {
        OnFileOpen(path);
    });

    // Set up chat callbacks
    chat_interface_->SetOnUserMessage([this](const std::string& message) {
        OnUserMessage(message);
    });

    // Set up agent callbacks
    agentic_engine_->SetWorkingDirectory(root_directory_);
    agentic_engine_->SetOnTaskComplete([this](const AgentTask& task, const std::string& result) {
        OnTaskComplete(task, result);
    });

    // Initialize server routes
    SetupServerRoutes();

    // Start server
    if (!server_->Start()) {
        std::cerr << "Failed to start server" << std::endl;
        return false;
    }

    // Initialize inference engine
    std::string model_path = root_directory_ + "/models/default-model.gguf";
    if (!inference_engine_->Initialize(model_path)) {
        std::cout << "Warning: Failed to initialize inference engine (continuing anyway)" << std::endl;
    }

    chat_interface_->AddSystemMessage("Scalar Agentic IDE initialized successfully!");
    chat_interface_->AddSystemMessage("Root directory: " + root_directory_);
    chat_interface_->AddSystemMessage("Server running on http://localhost:8080");

    is_running_ = true;
    std::cout << "IDE initialized successfully!" << std::endl;

    return true;
}

void AgenticIDE::Run() {
    std::cout << "Starting IDE event loop (scalar mode)..." << std::endl;

    while (is_running_) {
        ProcessEvents();
        UpdateUI();

        // Scalar: small delay to prevent busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "IDE event loop stopped" << std::endl;
}

void AgenticIDE::Shutdown() {
    if (!is_running_) return;

    std::cout << "Shutting down IDE..." << std::endl;

    is_running_ = false;

    if (server_) {
        server_->Stop();
    }

    chat_interface_->AddSystemMessage("IDE shutdown complete");
}

void AgenticIDE::ProcessEvents() {
    // Process server events (scalar)
    if (server_) {
        server_->ProcessEvents();
    }

    // Process agent task queue (scalar)
    if (agentic_engine_) {
        agentic_engine_->ProcessQueue();
    }
}

void AgenticIDE::UpdateUI() {
    // Scalar UI update logic
    // This would be called by Qt/GUI framework in real implementation
}

void AgenticIDE::SetupServerRoutes() {
    // Chat endpoint
    server_->POST("/api/chat", [this](const HttpRequest& request) {
        return HandleChatRequest(request);
    });

    // File operations endpoint
    server_->GET("/api/files", [this](const HttpRequest& request) {
        return HandleFileRequest(request);
    });

    server_->POST("/api/files", [this](const HttpRequest& request) {
        return HandleFileRequest(request);
    });

    // Agent operations endpoint
    server_->POST("/api/agent", [this](const HttpRequest& request) {
        return HandleAgentRequest(request);
    });

    // Status endpoint
    server_->GET("/api/status", [this](const HttpRequest& request) {
        return HandleStatusRequest(request);
    });

    std::cout << "Server routes configured" << std::endl;
}

HttpResponse AgenticIDE::HandleChatRequest(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    // Parse message from request body (simplified)
    std::string message = request.body;

    // Send to chat interface
    chat_interface_->SendUserMessage(message);

    // Generate AI response (scalar)
    std::string ai_response = "AI response placeholder for: " + message;

    if (inference_engine_) {
        ai_response = inference_engine_->GenerateToken(message, 50);
    }

    chat_interface_->AddAssistantMessage(ai_response);

    // Build JSON response
    response.body = "{\"response\":\"" + ai_response + "\",\"status\":\"ok\"}";

    return response;
}

HttpResponse AgenticIDE::HandleFileRequest(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    if (request.method == "GET") {
        // List files
        auto root = file_browser_->GetRoot();
        response.body = "{\"files\":[],\"status\":\"ok\"}";  // Simplified
    } else if (request.method == "POST") {
        // File operation (create, edit, delete)
        response.body = "{\"status\":\"ok\"}";
    }

    return response;
}

HttpResponse AgenticIDE::HandleAgentRequest(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    // Parse agent command from request
    std::string command = request.body;

    ExecuteAgenticCommand(command);

    response.body = "{\"status\":\"task_queued\"}";
    return response;
}

HttpResponse AgenticIDE::HandleStatusRequest(const HttpRequest& request) {
    HttpResponse response;
    response.status_code = 200;
    response.content_type = "application/json";

    std::ostringstream status;
    status << "{";
    status << "\"server_running\":" << (server_->IsRunning() ? "true" : "false") << ",";
    status << "\"root_directory\":\"" << root_directory_ << "\",";
    status << "\"message_count\":" << chat_interface_->GetMessages().size() << ",";
    status << "\"status\":\"ok\"";
    status << "}";

    response.body = status.str();
    return response;
}

void AgenticIDE::ExecuteAgenticCommand(const std::string& command) {
    std::cout << "Executing agentic command: " << command << std::endl;

    // Parse command and create agent task (scalar)
    AgentTask task;
    task.id = "task_" + std::to_string(std::time(nullptr));
    task.type = AgentTaskType::CODE_GENERATION;  // Determine from command
    task.description = command;
    task.status = "pending";

    // Queue task for processing
    agentic_engine_->QueueTask(task);

    chat_interface_->AddSystemMessage("Agent task queued: " + command);
}

void AgenticIDE::OpenFile(const std::string& path) {
    file_browser_->OpenFile(path);
    chat_interface_->AddSystemMessage("Opened file: " + path);
}

void AgenticIDE::SaveFile(const std::string& path, const std::string& content) {
    if (agentic_engine_->CreateFile(path, content) == "File created successfully: " + path) {
        chat_interface_->AddSystemMessage("Saved file: " + path);
    } else {
        chat_interface_->AddSystemMessage("Failed to save file: " + path);
    }
}

void AgenticIDE::SearchProject(const std::string& query) {
    std::string results = agentic_engine_->SearchCode(query, root_directory_);
    chat_interface_->AddAssistantMessage("Search results:\n" + results);
}

void AgenticIDE::OnUserMessage(const std::string& message) {
    std::cout << "User message: " << message << std::endl;

    // Check if it's an agent command (scalar pattern matching)
    if (message.find("/agent") == 0) {
        std::string command = message.substr(7);  // Remove "/agent "
        ExecuteAgenticCommand(command);
    } else if (message.find("/search") == 0) {
        std::string query = message.substr(8);
        SearchProject(query);
    } else {
        // Regular chat - generate AI response
        std::string response = "AI placeholder response";

        if (inference_engine_) {
            response = inference_engine_->GenerateToken(message, 50);
        }

        chat_interface_->AddAssistantMessage(response);
    }
}

void AgenticIDE::OnFileOpen(const std::string& path) {
    std::cout << "File opened: " << path << std::endl;
    chat_interface_->AddSystemMessage("Opened: " + path);
}

void AgenticIDE::OnTaskComplete(const AgentTask& task, const std::string& result) {
    std::cout << "Task completed: " << task.description << std::endl;
    std::cout << "Result: " << result << std::endl;

    chat_interface_->AddAssistantMessage("Task completed: " + task.description + "\n" + result);
}

} // namespace RawrXD
