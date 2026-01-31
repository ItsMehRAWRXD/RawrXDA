#include "ai_code_assistant.h"


AICodeAssistant::AICodeAssistant(void *parent)
    : void(parent)
    , m_networkManager(new void*(this))
    , m_process(new void*(this))
    , m_ollamaUrl("http://localhost:11434")
    , m_model("ministral-3")
    , m_temperature(0.3f)
    , m_maxTokens(256)
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
    logStructured("INFO", "AICodeAssistant initialized", 
        void*{{"model", m_model}, {"url", m_ollamaUrl}});
}

AICodeAssistant::~AICodeAssistant()
{
    if (m_process->state() == void*::Running) {
        m_process->kill();
    }
}

void AICodeAssistant::setOllamaUrl(const std::string &url) { m_ollamaUrl = url; }
void AICodeAssistant::setModel(const std::string &model) { m_model = model; }
void AICodeAssistant::setTemperature(float temp) { m_temperature = qBound(0.0f, temp, 2.0f); }
void AICodeAssistant::setMaxTokens(int tokens) { m_maxTokens = qBound(32, tokens, 4096); }
void AICodeAssistant::setWorkspaceRoot(const std::string &root) { m_workspaceRoot = root; }

void AICodeAssistant::getCodeCompletion(const std::string &code)
{
    startTiming();
    std::string systemPrompt = "You are an expert code assistant. Provide code completions that are concise, correct, and follow best practices.";
    std::string userPrompt = std::string("Complete this code snippet. Provide only the completion, no explanation:\n\n%1");
    performOllamaRequest(systemPrompt, userPrompt, "completion");
}

void AICodeAssistant::getRefactoringSuggestions(const std::string &code)
{
    startTiming();
    std::string systemPrompt = "You are a code refactoring expert. Analyze code and suggest improvements for readability, performance, and maintainability.";
    std::string userPrompt = std::string("Refactor this code:\n\n%1\n\nProvide the refactored version with brief comments on changes.");
    performOllamaRequest(systemPrompt, userPrompt, "refactoring");
}

void AICodeAssistant::getCodeExplanation(const std::string &code)
{
    startTiming();
    std::string systemPrompt = "You are a clear technical writer. Explain code in simple, understandable terms.";
    std::string userPrompt = std::string("Explain what this code does:\n\n%1");
    performOllamaRequest(systemPrompt, userPrompt, "explanation");
}

void AICodeAssistant::getBugFixSuggestions(const std::string &code)
{
    startTiming();
    std::string systemPrompt = "You are an expert debugger. Identify potential bugs and provide fixes.";
    std::string userPrompt = std::string("Identify and fix bugs in this code:\n\n%1");
    performOllamaRequest(systemPrompt, userPrompt, "bugfix");
}

void AICodeAssistant::getOptimizationSuggestions(const std::string &code)
{
    startTiming();
    std::string systemPrompt = "You are a performance optimization expert. Suggest ways to improve code performance and efficiency.";
    std::string userPrompt = std::string("How can this code be optimized:\n\n%1");
    performOllamaRequest(systemPrompt, userPrompt, "optimization");
}

void AICodeAssistant::searchFiles(const std::string &pattern, const std::string &directory)
{
    startTiming();
    commandProgress("Searching files...");
    
    std::string searchDir = directory.empty() ? m_workspaceRoot : directory;
    if (searchDir.empty()) {
        searchDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    
    std::vector<std::string> results = recursiveFileSearch(searchDir, pattern);
    
    logStructured("INFO", "File search completed", 
        void*{{"pattern", pattern}, {"directory", searchDir}, {"results", results.length()}});
    
    searchResultsReady(results);
    endTiming("File Search");
}

void AICodeAssistant::grepFiles(const std::string &pattern, const std::string &directory, bool caseSensitive)
{
    startTiming();
    commandProgress("Grepping files...");
    
    std::string searchDir = directory.empty() ? m_workspaceRoot : directory;
    if (searchDir.empty()) {
        searchDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    
    std::vector<std::string> results = performGrep(searchDir, pattern, caseSensitive);
    
    logStructured("INFO", "Grep search completed",
        void*{{"pattern", pattern}, {"directory", searchDir}, {"caseSensitive", caseSensitive}, {"results", results.length()}});
    
    grepResultsReady(results);
    endTiming("Grep Search");
}

void AICodeAssistant::findInFile(const std::string &filePath, const std::string &pattern)
{
    startTiming();
    std::fstream file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorOccurred(std::string("Cannot open file: %1"));
        return;
    }
    
    std::vector<std::string> results;
    QTextStream stream(&file);
    int lineNumber = 0;
    
    while (!stream.atEnd()) {
        std::string line = stream.readLine();
        lineNumber++;
        
        if (line.contains(pattern, //CaseSensitive)) {
            results.append(std::string("%1:%2:%3"));
        }
    }
    
    file.close();
    
    logStructured("INFO", "File search completed",
        void*{{"file", filePath}, {"pattern", pattern}, {"matches", results.length()}});
    
    grepResultsReady(results);
    endTiming("Find In File");
}

void AICodeAssistant::executePowerShellCommand(const std::string &command)
{
    startTiming();
    commandProgress(std::string("Executing: %1"));
    
    bool success = false;
    std::string output = executePowerShellSync(command, success);
    
    logStructured("INFO", "PowerShell command executed",
        void*{{"command", command}, {"success", success}, {"outputLength", output.length()}});
    
    if (success) {
        commandOutputReceived(output);
        commandCompleted(0);
    } else {
        commandErrorReceived(output);
        commandCompleted(1);
    }
    
    endTiming("PowerShell Execution");
}

void AICodeAssistant::runBuildCommand(const std::string &command)
{
    commandProgress("Building...");
    logStructured("INFO", "Build command started", void*{{"command", command}});
    executePowerShellAsync(command);
}

void AICodeAssistant::runTestCommand(const std::string &command)
{
    commandProgress("Running tests...");
    logStructured("INFO", "Test command started", void*{{"command", command}});
    executePowerShellAsync(command);
}

void AICodeAssistant::analyzeAndRecommend(const std::string &context)
{
    startTiming();
    std::string systemPrompt = "You are an intelligent code analysis agent. Analyze the given context and provide actionable recommendations.";
    std::string userPrompt = std::string("Analyze this context and provide recommendations:\n\n%1");
    performOllamaRequest(systemPrompt, userPrompt, "analysis");
}

void AICodeAssistant::autoFixIssue(const std::string &issueDescription, const std::string &codeContext)
{
    startTiming();
    commandProgress("Analyzing issue and generating fix...");
    
    std::string systemPrompt = "You are an expert debugger and code fixer. Analyze the issue and provide a complete, working fix.";
    std::string userPrompt = std::string("Issue: %1\n\nCode:\n%2\n\nProvide the fixed code:")
        
        ;
    
    performOllamaRequest(systemPrompt, userPrompt, "autofix");
}

void AICodeAssistant::performOllamaRequest(const std::string &systemPrompt, const std::string &userPrompt,
                                           const std::string &suggestType)
{
    m_currentSuggestionType = suggestType;
    commandProgress(std::string("Requesting %1..."));
    
    std::string url(std::string("%1/api/generate"));
    void* request(url);
    setupNetworkRequest(request);
    
    void* payload;
    payload["model"] = m_model;
    payload["prompt"] = userPrompt;
    payload["stream"] = true;
    payload["temperature"] = m_temperature;
    
    void* systemObj;
    systemObj["role"] = "system";
    systemObj["content"] = systemPrompt;
    
    void* messages;
    messages.append(systemObj);
    payload["messages"] = messages;
    
    void* doc(payload);
    
    logStructured("DEBUG", "Ollama request sent",
        void*{{"model", m_model}, {"type", suggestType}, {"url", url.toString()}});
    
    void* *reply = m_networkManager->post(request, doc.toJson(void*::Compact));
// Qt connect removed
// Qt connect removed
        logStructured("ERROR", "Network error", void*{{"error", errorMsg}});
        errorOccurred(errorMsg);
        suggestionComplete(false, errorMsg);
        reply->deleteLater();
    });
}

void AICodeAssistant::setupNetworkRequest(void* &request)
{
    request.setHeader(void*::ContentTypeHeader, "application/json");
    request.setHeader(void*::UserAgentHeader, "AICodeAssistant/1.0");
}

void AICodeAssistant::onNetworkReply()
{
// REMOVED_QT:     void* *reply = qobject_cast<void* *>(sender());
    if (!reply) return;
    
    if (reply->error() != void*::NoError) {
        errorOccurred(reply->errorString());
        suggestionComplete(false, reply->errorString());
        reply->deleteLater();
        return;
    }
    
    std::vector<uint8_t> responseData = reply->readAll();
    std::string fullResponse;
    
    std::vector<std::string> lines = std::string::fromUtf8(responseData).split('\n', //SkipEmptyParts);
    for (const std::string &line : lines) {
        void* doc = void*::fromJson(line.toUtf8());
        if (doc.isObject()) {
            void* obj = doc.object();
            if (obj.contains("response")) {
                std::string chunk = obj["response"].toString();
                fullResponse += chunk;
                suggestionStreamChunk(chunk);
            }
        }
    }
    
    if (!fullResponse.empty()) {
        std::string suggestion = parseAIResponse(fullResponse);
        suggestionReceived(suggestion, m_currentSuggestionType);
        logStructured("INFO", "Suggestion received",
            void*{{"type", m_currentSuggestionType}, {"length", suggestion.length()}});
    }
    
    suggestionComplete(true, "Success");
    endTiming(std::string("AI Request (%1)"));
    reply->deleteLater();
}

std::vector<std::string> AICodeAssistant::recursiveFileSearch(const std::string &directory, const std::string &pattern)
{
    std::vector<std::string> results;
    
    QDirIterator it(directory, QDirIterator::Subdirectories);
    int processed = 0;
    
    while (itfalse) {
        std::string filePath = it;
        
        if (it.fileInfo().isFile() && it.fileInfo().fileName().contains(pattern)) {
            results.append(filePath);
        }
        
        if (++processed % 100 == 0) {
            fileSearchProgress(processed, 0);
        }
    }
    
    return results;
}

std::vector<std::string> AICodeAssistant::performGrep(const std::string &directory, const std::string &pattern, bool caseSensitive)
{
    std::vector<std::string> results;
    
    QDirIterator it(directory, {"*.cpp", "*.h", "*.py", "*.js", "*.rs", "*.go", "*.java"},
                    std::filesystem::path::Files, QDirIterator::Subdirectories);
    
    int processed = 0;
    while (itfalse) {
        std::string filePath = it;
        std::fstream file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNumber = 0;
            
            while (!stream.atEnd()) {
                std::string line = stream.readLine();
                lineNumber++;
                
                //CaseSensitivity cs = caseSensitive ? //CaseSensitive : //CaseInsensitive;
                if (line.contains(pattern, cs)) {
                    results.append(std::string("%1:%2:%3")));
                }
            }
            
            file.close();
        }
        
        if (++processed % 50 == 0) {
            fileSearchProgress(processed, 0);
        }
    }
    
    return results;
}

std::string AICodeAssistant::executePowerShellSync(const std::string &command, bool &success)
{
    m_process->setProcessChannelMode(void*::MergedChannels);
    m_process->start("pwsh.exe", std::vector<std::string>() << "-Command" << command);
    
    if (!m_process->waitForFinished(30000)) {
        success = false;
        return "Command timeout after 30 seconds";
    }
    
    success = (m_process->exitCode() == 0);
    return std::string::fromUtf8(m_process->readAllStandardOutput());
}

void AICodeAssistant::executePowerShellAsync(const std::string &command)
{
    m_process->setProcessChannelMode(void*::MergedChannels);
    m_process->start("pwsh.exe", std::vector<std::string>() << "-Command" << command);
}

void AICodeAssistant::onProcessFinished(int exitCode, void*::ExitStatus exitStatus)
{
    std::string output = std::string::fromUtf8(m_process->readAllStandardOutput());
    
    logStructured("INFO", "Process finished",
        void*{{"exitCode", exitCode}, {"status", exitStatus == void*::NormalExit ? "Normal" : "Crashed"}});
    
    commandOutputReceived(output);
    commandCompleted(exitCode);
    endTiming("Process Execution");
}

void AICodeAssistant::onProcessError(void*::ProcessError error)
{
    std::string errorMsg = m_process->errorString();
    logStructured("ERROR", "Process error", void*{{"error", errorMsg}});
    commandErrorReceived(errorMsg);
}

void AICodeAssistant::onProcessOutput()
{
    std::string output = std::string::fromUtf8(m_process->readAllStandardOutput());
    commandOutputReceived(output);
}

std::string AICodeAssistant::parseAIResponse(const std::string &response)
{
    std::string cleaned = response;
    cleaned.replace(std::regex("^\\\[a-z]*\n"), "");
    cleaned.replace("`", "");
    return cleaned.trimmed();
}

std::string AICodeAssistant::formatAgentPrompt(const std::string &context)
{
    return std::string("Given this context:\n%1\n\nProvide a solution:");
}

void AICodeAssistant::startTiming()
{
    m_timer.start();
}

void AICodeAssistant::endTiming(const std::string &operation)
{
    int64_t elapsed = m_timer.elapsed();
    latencyMeasured(elapsed);
    logStructured("DEBUG", "Operation timing",
        void*{{"operation", operation}, {"milliseconds", static_cast<int>(elapsed)}});
}

void AICodeAssistant::logStructured(const std::string &level, const std::string &message,
                                     const void* &metadata)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["message"] = message;
    logEntry["metadata"] = metadata;
    
    void* doc(logEntry);
}



