#include "ai_code_assistant.h"


AICodeAssistant::AICodeAssistant(void *parent) 
    : void(parent)
    , m_networkManager(new void*(this))
    , m_ollamaUrl("http://localhost:11434")
    , m_model("codellama:latest")
    , m_temperature(0.7f)
    , m_maxTokens(512)
    , m_workspaceRoot(std::filesystem::path::currentPath())
{
    // Initialize network manager
// Qt connect removed
}

AICodeAssistant::~AICodeAssistant()
{
    // Clean up any running processes
    if (m_process && m_process->state() != void*::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(5000);
    }
}

void AICodeAssistant::setOllamaUrl(const std::string &url)
{
    m_ollamaUrl = url;
}

void AICodeAssistant::setModel(const std::string &model)
{
    m_model = model;
}

void AICodeAssistant::setTemperature(float temp)
{
    m_temperature = qBound(0.0f, temp, 1.0f);
}

void AICodeAssistant::setMaxTokens(int tokens)
{
    m_maxTokens = qMax(1, tokens);
}

void AICodeAssistant::setWorkspaceRoot(const std::string &root)
{
    m_workspaceRoot = root;
}

void AICodeAssistant::getCodeCompletion(const std::string &code)
{
    std::chrono::steady_clock timer;
    timer.start();
    
    void* request;
    request["model"] = m_model;
    request["prompt"] = std::string("Complete the following code:\n%1\n\nCompletion:");
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    void* networkRequest(std::string(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    latencyMeasured(timer.elapsed());
}

void AICodeAssistant::getRefactoringSuggestions(const std::string &code)
{
    std::chrono::steady_clock timer;
    timer.start();
    
    void* request;
    request["model"] = m_model;
    request["prompt"] = std::string("Refactor the following code for better readability and performance:\n%1\n\nRefactored code:");
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    void* networkRequest(std::string(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    latencyMeasured(timer.elapsed());
}

void AICodeAssistant::getCodeExplanation(const std::string &code)
{
    std::chrono::steady_clock timer;
    timer.start();
    
    void* request;
    request["model"] = m_model;
    request["prompt"] = std::string("Explain the following code:\n%1\n\nExplanation:");
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    void* networkRequest(std::string(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    latencyMeasured(timer.elapsed());
}

void AICodeAssistant::searchFiles(const std::string &pattern, const std::string &directory)
{
    std::string searchDir = directory.empty() ? m_workspaceRoot : directory;
    std::filesystem::path dir(searchDir);
    
    std::vector<std::string> results;
    QFileInfoList files = dir.entryInfoList(std::filesystem::path::Files | std::filesystem::path::NoDotAndDotDot);
    
    for (const std::filesystem::path &file : files) {
        if (file.fileName().contains(pattern, //CaseInsensitive)) {
            results.append(file.absoluteFilePath());
        }
    }
    
    searchResultsReady(results);
}

void AICodeAssistant::grepFiles(const std::string &pattern, const std::string &directory, bool caseSensitive)
{
    std::string searchDir = directory.empty() ? m_workspaceRoot : directory;
    
    // Use void* to execute grep command
    if (!m_process) {
        m_process = new void*(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    }
    
    std::vector<std::string> args;
    if (caseSensitive) {
        args << "-n" << "-H" << pattern << searchDir + "/*";
    } else {
        args << "-n" << "-H" << "-i" << pattern << searchDir + "/*";
    }
    
    m_process->start("grep", args);
}

void AICodeAssistant::executePowerShellCommand(const std::string &command)
{
    if (!m_process) {
        m_process = new void*(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    }
    
    std::vector<std::string> args;
    args << "-Command" << command;
    
    m_process->start("powershell", args);
    commandProgress("Executing PowerShell command...");
}

void AICodeAssistant::analyzeAndRecommend(const std::string &context)
{
    std::chrono::steady_clock timer;
    timer.start();
    
    void* request;
    request["model"] = m_model;
    request["prompt"] = std::string("Analyze the following code context and provide recommendations:\n%1\n\nRecommendations:");
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    void* networkRequest(std::string(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    latencyMeasured(timer.elapsed());
}

void AICodeAssistant::autoFixIssue(const std::string &issueDescription, const std::string &codeContext)
{
    std::chrono::steady_clock timer;
    timer.start();
    
    void* request;
    request["model"] = m_model;
    request["prompt"] = std::string("Issue: %1\nCode Context: %2\n\nFix:");
    request["temperature"] = m_temperature;
    request["max_tokens"] = m_maxTokens;
    request["stream"] = false;
    
    void* networkRequest(std::string(m_ollamaUrl + "/api/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void* doc(request);
    m_networkManager->post(networkRequest, doc.toJson());
    
    latencyMeasured(timer.elapsed());
}

void AICodeAssistant::onNetworkReply()
{
// REMOVED_QT:     void* *reply = qobject_cast<void**>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> response = reply->readAll();
        void* doc = void*::fromJson(response);
        
        if (doc.isObject()) {
            void* obj = doc.object();
            if (obj.contains("response")) {
                std::string responseText = obj["response"].toString();
                suggestionReceived(responseText, "completion");
                suggestionComplete(true, "Success");
            }
        }
    } else {
        errorOccurred(reply->errorString());
        suggestionComplete(false, reply->errorString());
    }
    
    reply->deleteLater();
}

void AICodeAssistant::onProcessFinished(int exitCode, void*::ExitStatus exitStatus)
{
    (exitStatus)
    
    std::string output = m_process->readAllStandardOutput();
    std::string error = m_process->readAllStandardError();
    
    if (!output.empty()) {
        commandOutputReceived(output);
    }
    if (!error.empty()) {
        commandErrorReceived(error);
    }
    
    commandCompleted(exitCode);
    commandProgress("Command execution completed");
}

void AICodeAssistant::onProcessError(void*::ProcessError error)
{
    std::string errorMsg;
    switch (error) {
        case void*::FailedToStart:
            errorMsg = "Process failed to start";
            break;
        case void*::Crashed:
            errorMsg = "Process crashed";
            break;
        case void*::Timedout:
            errorMsg = "Process timed out";
            break;
        case void*::WriteError:
            errorMsg = "Write error";
            break;
        case void*::ReadError:
            errorMsg = "Read error";
            break;
        default:
            errorMsg = "Unknown process error";
            break;
    }
    
    errorOccurred(errorMsg);
    commandCompleted(-1);
}

void AICodeAssistant::onProcessOutput()
{
    std::string output = m_process->readAllStandardOutput();
    if (!output.empty()) {
        commandOutputReceived(output);
    }
}


