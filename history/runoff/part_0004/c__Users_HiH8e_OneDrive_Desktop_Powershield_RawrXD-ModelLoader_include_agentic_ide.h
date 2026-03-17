#pragma once
#include <memory>
#include "scalar_server.h"
#include "file_browser.h"
#include "chat_interface.h"
#include "chat_workspace.h"
#include "agentic_engine.h"
#include "inference_engine.h"

// SCALAR-ONLY: Integrated autonomous agentic IDE with comprehensive chat workspace

namespace RawrXD {

class AgenticIDE {
public:
    AgenticIDE();
    ~AgenticIDE();

    // Initialization
    bool Initialize(const std::string& root_directory);
    void Run();  // Main event loop (scalar)
    void Shutdown();

    // Component access
    ScalarServer* GetServer() { return server_.get(); }
    FileBrowser* GetFileBrowser() { return file_browser_.get(); }
    ChatInterface* GetChatInterface() { return chat_interface_.get(); }
    ChatWorkspace* GetChatWorkspace() { return chat_workspace_.get(); }
    AgenticEngine* GetAgenticEngine() { return agentic_engine_.get(); }
    InferenceEngine* GetInferenceEngine() { return inference_engine_.get(); }

    // IDE Commands
    void ExecuteAgenticCommand(const std::string& command);
    void OpenFile(const std::string& path);
    void SaveFile(const std::string& path, const std::string& content);
    void SearchProject(const std::string& query);

private:
    std::unique_ptr<ScalarServer> server_;
    std::unique_ptr<FileBrowser> file_browser_;
    std::unique_ptr<ChatInterface> chat_interface_;
    std::unique_ptr<ChatWorkspace> chat_workspace_;
    std::unique_ptr<AgenticEngine> agentic_engine_;
    std::unique_ptr<InferenceEngine> inference_engine_;

    bool is_running_;
    std::string root_directory_;

    // Event loop processing (scalar)
    void ProcessEvents();
    void UpdateUI();

    // Callbacks
    void OnUserMessage(const std::string& message);
    void OnFileOpen(const std::string& path);
    void OnTaskComplete(const AgentTask& task, const std::string& result);

    // Server route handlers
    void SetupServerRoutes();
    HttpResponse HandleChatRequest(const HttpRequest& request);
    HttpResponse HandleFileRequest(const HttpRequest& request);
    HttpResponse HandleAgentRequest(const HttpRequest& request);
    HttpResponse HandleStatusRequest(const HttpRequest& request);
};

} // namespace RawrXD
