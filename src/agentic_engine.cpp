#include "agentic_engine.h"
#include <iostream>
#include <thread>
#include <fstream>
#include "action_executor.h"

AgenticEngine::AgenticEngine() 
    : m_modelLoaded(false), m_inferenceEngine(nullptr)
{
    // Lazy init router but explicit
    m_router = std::make_shared<RawrXD::UniversalModelRouter>();
    m_currentModelPath = "local-default"; // Default
    
    // Explicit Logic: Set up Router for Titan
    RawrXD::ModelConfig cfg;
    cfg.backend = RawrXD::ModelBackend::LOCAL_TITAN;
    cfg.model_id = "titan-embedded";
    m_router->registerModel("titan-embedded", cfg);
    m_currentModelPath = "titan-embedded";
}

AgenticEngine::~AgenticEngine() = default;

json AgenticEngine::planTask(const std::string& goal) {
     // Explicit Logic: Ask LLM to generate a plan
     std::string schema = R"([{"type": "file_edit", "target": "path/to/file", "content": "code_snippet_or_diff"}, {"type": "command", "cmd": "shell_command"}])";
     std::string prompt = "You are an autonomous coding agent. Your goal is: " + goal + "\n"
                          "Return a JSON array of actions to achieve this goal. NO conversational text.\n"
                          "Schema: " + schema + "\n"
                          "Response:";
                          
     std::string response = processQuery(prompt);
     
     try {
         // Attempt to parse AI response as JSON
         // Robust parsing: Find the first/last bracket to handle potential preamble
         size_t start = response.find('[');
         size_t end = response.rfind(']');
         if (start != std::string::npos && end != std::string::npos && end > start) {
             std::string jsonStr = response.substr(start, end - start + 1);
             return json::parse(jsonStr);
         }
     } catch (...) {}
     
     // Fallback: Return a single generic action if parsing fails - but log the raw failure
     std::cerr << "Plan Parse Failed. Raw Response: " << response << std::endl;
     return json::array({
         {{"type", "error"}, {"description", "Could not parse AI plan into JSON"}, {"raw_response", response}}
     });
}

void AgenticEngine::initialize() {
    // Initialization logic
    m_userPreferences["temperature"] = "0.7";
}

void AgenticEngine::shutdown() {
    // Shutdown logic
}

std::string AgenticEngine::processQuery(const std::string& query) {
    if (m_router && !m_currentModelPath.empty()) {
        return m_router->routeQuery(m_currentModelPath, buildPrompt(query), m_genConfig.temperature);
    }
    return "AgenticEngine: Model not ready.";
}

void AgenticEngine::processQueryAsync(const std::string& query, std::function<void(std::string)> callback) {
    std::thread([this, query, callback]() {
        std::string response = this->processQuery(query);
        if (callback) callback(response);
    }).detach();
}

void AgenticEngine::updateConfig(const GenerationConfig& config) {
    m_genConfig = config;
}

void AgenticEngine::clearHistory() {
    m_history.clear();
}

void AgenticEngine::appendSystemPrompt(const std::string& prompt) {
    m_systemPrompt += prompt + "\n";
}

void AgenticEngine::loadContext(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) return;
    
    m_history.clear();
    std::string line;
    std::string query, response;
    bool parsingQuery = true;
    
    while (std::getline(f, line)) {
        if (line == "---") {
            if (!query.empty()) {
                m_history.push_back({query, response});
                query.clear();
                response.clear();
            }
            parsingQuery = true;
            continue;
        }
        
        if (line.rfind("U: ", 0) == 0) {
            parsingQuery = true;
            query += line.substr(3) + "\n";
        } else if (line.rfind("A: ", 0) == 0) {
            parsingQuery = false;
            response += line.substr(3) + "\n";
        } else {
            if (parsingQuery) query += line + "\n";
            else response += line + "\n";
        }
    }
    if (!query.empty()) {
        m_history.push_back({query, response});
    }
}

void AgenticEngine::saveContext(const std::string& filepath) {
    std::ofstream f(filepath);
    if (!f.is_open()) return;
    
    for (const auto& pair : m_history) {
        f << "U: " << pair.first << "\n";
        f << "A: " << pair.second << "\n";
        f << "---\n";
    }
}

std::vector<std::string> AgenticEngine::getAvailableModels() {
    if (m_router) {
        return m_router->getAvailableModels();
    }
    // Fallback if router not initialized
    return {};
}

std::string AgenticEngine::getCurrentModel() {
    return m_currentModelPath;
}

void AgenticEngine::setModel(const std::string& modelPath) {
    if (modelPath.empty()) return;
    
    // Validate with router if possible
    if (m_router) {
        if (!m_router->isModelAvailable(modelPath)) {
             // Attempt auto-registration for local paths
             if (std::filesystem::exists(modelPath)) {
                 RawrXD::ModelConfig cfg;
                 cfg.backend = RawrXD::ModelBackend::LOCAL_GGUF;
                 cfg.model_id = modelPath;
                 m_router->registerModel(modelPath, cfg);
             }
        }
    }

    m_currentModelPath = modelPath;
    m_modelLoaded = true; // Router handles actual loading lazily
    
    if (onModelLoadingFinished) onModelLoadingFinished(true, "");
    if (onModelReady) onModelReady(true);
}

void AgenticEngine::setModelName(const std::string& modelName) {
    m_currentModelPath = modelName;
    m_modelLoaded = true;
    if (onModelLoadingFinished) onModelLoadingFinished(true, "");
    if (onModelReady) onModelReady(true);
}

void AgenticEngine::processMessage(const std::string& message, const std::string& editorContext) {
    std::string prompt = message;
    if (!editorContext.empty()) {
        prompt += "\nContext:\n" + editorContext;
    }
    std::string response = processQuery(prompt);
    if (onResponseReady) onResponseReady(response);
}

std::string AgenticEngine::buildPrompt(const std::string& query) {
    std::string fullPrompt = m_systemPrompt;
    for (const auto& pair : m_history) {
        fullPrompt += "User: " + pair.first + "\nAssistant: " + pair.second + "\n";
    }
    fullPrompt += "User: " + query + "\nAssistant:";
    return fullPrompt;
}

void AgenticEngine::logInteraction(const std::string& query, const std::string& response) {
    m_history.push_back({query, response});
}
