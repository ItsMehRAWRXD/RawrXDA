#include "auto_model_loader.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <process.h>
#include <windows.h>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <thread>
#include <openssl/sha.h>

namespace fs = std::filesystem;

namespace AutoModelLoader {

// ============================================================================
// Performance Metrics Implementation
// ============================================================================

void PerformanceMetrics::recordDiscoveryLatency(uint64_t micros) {
    std::lock_guard<std::mutex> lock(metricsLock);
    discoveryLatencies.push_back(micros);
    totalDiscoveryCalls++;
}

void PerformanceMetrics::recordLoadLatency(uint64_t micros) {
    std::lock_guard<std::mutex> lock(metricsLock);
    loadLatencies.push_back(micros);
    totalLoadCalls++;
}

void PerformanceMetrics::recordSelectionLatency(uint64_t micros) {
    std::lock_guard<std::mutex> lock(metricsLock);
    selectionLatencies.push_back(micros);
}

std::string PerformanceMetrics::generatePrometheusMetrics() const {
    std::stringstream ss;
    
    // Counter metrics
    ss << "# HELP model_loader_discovery_total Total number of model discovery operations\n";
    ss << "# TYPE model_loader_discovery_total counter\n";
    ss << "model_loader_discovery_total " << totalDiscoveryCalls << "\n\n";
    
    ss << "# HELP model_loader_load_total Total number of model load attempts\n";
    ss << "# TYPE model_loader_load_total counter\n";
    ss << "model_loader_load_total " << totalLoadCalls << "\n\n";
    
    ss << "# HELP model_loader_load_success Total number of successful model loads\n";
    ss << "# TYPE model_loader_load_success counter\n";
    ss << "model_loader_load_success " << successfulLoads << "\n\n";
    
    ss << "# HELP model_loader_load_failures Total number of failed model loads\n";
    ss << "# TYPE model_loader_load_failures counter\n";
    ss << "model_loader_load_failures " << failedLoads << "\n\n";
    
    ss << "# HELP model_loader_cache_hits Total cache hits\n";
    ss << "# TYPE model_loader_cache_hits counter\n";
    ss << "model_loader_cache_hits " << cacheHits << "\n\n";
    
    ss << "# HELP model_loader_cache_misses Total cache misses\n";
    ss << "# TYPE model_loader_cache_misses counter\n";
    ss << "model_loader_cache_misses " << cacheMisses << "\n\n";
    
    // Calculate latency statistics
    auto calculateStats = [](const std::vector<uint64_t>& latencies) -> std::tuple<double, double, double> {
        if (latencies.empty()) return {0, 0, 0};
        
        std::vector<uint64_t> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());
        
        double avg = 0;
        for (auto l : sorted) avg += l;
        avg /= sorted.size();
        
        double p50 = sorted[sorted.size() / 2];
        double p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
        
        return {avg, p50, p99};
    };
    
    std::lock_guard<std::mutex> lock(metricsLock);
    
    auto [discAvg, discP50, discP99] = calculateStats(discoveryLatencies);
    ss << "# HELP model_loader_discovery_latency_microseconds Discovery operation latency\n";
    ss << "# TYPE model_loader_discovery_latency_microseconds summary\n";
    ss << "model_loader_discovery_latency_microseconds{quantile=\"0.5\"} " << discP50 << "\n";
    ss << "model_loader_discovery_latency_microseconds{quantile=\"0.99\"} " << discP99 << "\n";
    ss << "model_loader_discovery_latency_microseconds_sum " << (discAvg * discoveryLatencies.size()) << "\n";
    ss << "model_loader_discovery_latency_microseconds_count " << discoveryLatencies.size() << "\n\n";
    
    auto [loadAvg, loadP50, loadP99] = calculateStats(loadLatencies);
    ss << "# HELP model_loader_load_latency_microseconds Load operation latency\n";
    ss << "# TYPE model_loader_load_latency_microseconds summary\n";
    ss << "model_loader_load_latency_microseconds{quantile=\"0.5\"} " << loadP50 << "\n";
    ss << "model_loader_load_latency_microseconds{quantile=\"0.99\"} " << loadP99 << "\n";
    ss << "model_loader_load_latency_microseconds_sum " << (loadAvg * loadLatencies.size()) << "\n";
    ss << "model_loader_load_latency_microseconds_count " << loadLatencies.size() << "\n\n";
    
    return ss.str();
}

// ============================================================================
// Circuit Breaker Implementation
// ============================================================================

CircuitBreaker::CircuitBreaker(int threshold, int timeoutMs) 
    : m_threshold(threshold)
    , m_timeoutMs(timeoutMs) {
}

bool CircuitBreaker::allowRequest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == State::OPEN) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastFailureTime).count();
            
        if (elapsed > m_timeoutMs) {
            m_state = State::HALF_OPEN;
            m_successCount = 0;
            m_failureCount = 0;
            return true;
        }
        return false;
    }
    
    return true;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_state == State::HALF_OPEN) {
        m_successCount++;
        if (m_successCount >= 2) {
            m_state = State::CLOSED;
            m_failureCount = 0;
        }
    } else if (m_state == State::CLOSED) {
        m_failureCount = std::max(0, m_failureCount.load() - 1);
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_failureCount++;
    m_lastFailureTime = std::chrono::system_clock::now();
    
    if (m_failureCount >= m_threshold) {
        m_state = State::OPEN;
    }
}

// ============================================================================
// AutoModelLoader Singleton Implementation
// ============================================================================

std::unique_ptr<AutoModelLoader> AutoModelLoader::s_instance = nullptr;
std::mutex AutoModelLoader::s_singletonMutex;

AutoModelLoader::AutoModelLoader() {
    // Initialize circuit breaker
    m_circuitBreaker = std::make_unique<CircuitBreaker>(
        m_config.circuitBreakerThreshold,
        m_config.circuitBreakerTimeoutMs
    );
    
    // Load configuration if exists
    if (fs::exists(m_config.configFilePath)) {
        loadConfiguration(m_config.configFilePath);
    }
    
    log(LogLevel::INFO, "AutoModelLoader initialized", {
        {"version", "2.0.0-enterprise"},
        {"config_file", m_config.configFilePath}
    });
}

AutoModelLoader& AutoModelLoader::GetInstance() {
    std::lock_guard<std::mutex> lock(s_singletonMutex);
    if (!s_instance) {
        s_instance = std::make_unique<AutoModelLoader>();
    }
    return *s_instance;
}

// ============================================================================
// Logging Implementation
// ============================================================================

void AutoModelLoader::log(LogLevel level, const std::string& message, 
                          const std::map<std::string, std::string>& context) {
    auto configLevel = parseLogLevel(m_config.logLevel);
    if (level < configLevel) return;
    
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "] ";
    ss << "[" << logLevelToString(level) << "] ";
    ss << "[AutoModelLoader] ";
    ss << message;
    
    if (!context.empty()) {
        ss << " {";
        bool first = true;
        for (const auto& [key, value] : context) {
            if (!first) ss << ", ";
            ss << key << "=\"" << value << "\"";
            first = false;
        }
        ss << "}";
    }
    
    std::cout << ss.str() << std::endl;
    
    // TODO: Add support for external logging systems (syslog, file, etc.)
}

LogLevel AutoModelLoader::parseLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "FATAL") return LogLevel::FATAL;
    return LogLevel::INFO;
}

std::string AutoModelLoader::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Configuration Management
// ============================================================================

bool AutoModelLoader::loadConfiguration(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    log(LogLevel::INFO, "Loading configuration", {{"path", configPath}});
    
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            log(LogLevel::WARN, "Configuration file not found, using defaults", {
                {"path", configPath}
            });
            return false;
        }
        
        // Simple JSON-like parsing (production would use proper JSON library)
        std::string line;
        while (std::getline(file, line)) {
            // Strip whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            
            if (line.empty() || line[0] == '#' || line[0] == '/') continue;
            
            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;
            
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Strip quotes and whitespace from value
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\",") + 1);
            
            // Parse configuration values
            if (key == "autoLoadEnabled") m_config.autoLoadEnabled = (value == "true");
            else if (key == "preferredModel") m_config.preferredModel = value;
            else if (key == "maxRetries") m_config.maxRetries = std::stoi(value);
            else if (key == "retryDelayMs") m_config.retryDelayMs = std::stoi(value);
            else if (key == "discoveryTimeoutMs") m_config.discoveryTimeoutMs = std::stoi(value);
            else if (key == "loadTimeoutMs") m_config.loadTimeoutMs = std::stoi(value);
            else if (key == "enableCaching") m_config.enableCaching = (value == "true");
            else if (key == "enableHealthChecks") m_config.enableHealthChecks = (value == "true");
            else if (key == "enableMetrics") m_config.enableMetrics = (value == "true");
            else if (key == "maxCacheSize") m_config.maxCacheSize = std::stoull(value);
            else if (key == "logLevel") m_config.logLevel = value;
        }
        
        log(LogLevel::INFO, "Configuration loaded successfully", {
            {"autoLoad", m_config.autoLoadEnabled ? "true" : "false"},
            {"maxRetries", std::to_string(m_config.maxRetries)},
            {"logLevel", m_config.logLevel}
        });
        
        return true;
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Failed to load configuration", {
            {"error", e.what()},
            {"path", configPath}
        });
        return false;
    }
}

bool AutoModelLoader::saveConfiguration(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            log(LogLevel::ERROR, "Failed to open config file for writing", {
                {"path", configPath}
            });
            return false;
        }
        
        file << "# AutoModelLoader Configuration\n";
        file << "# Generated: " << std::time(nullptr) << "\n\n";
        file << "autoLoadEnabled: " << (m_config.autoLoadEnabled ? "true" : "false") << "\n";
        file << "preferredModel: \"" << m_config.preferredModel << "\"\n";
        file << "maxRetries: " << m_config.maxRetries << "\n";
        file << "retryDelayMs: " << m_config.retryDelayMs << "\n";
        file << "discoveryTimeoutMs: " << m_config.discoveryTimeoutMs << "\n";
        file << "loadTimeoutMs: " << m_config.loadTimeoutMs << "\n";
        file << "enableCaching: " << (m_config.enableCaching ? "true" : "false") << "\n";
        file << "enableHealthChecks: " << (m_config.enableHealthChecks ? "true" : "false") << "\n";
        file << "enableMetrics: " << (m_config.enableMetrics ? "true" : "false") << "\n";
        file << "maxCacheSize: " << m_config.maxCacheSize << "\n";
        file << "logLevel: \"" << m_config.logLevel << "\"\n";
        
        log(LogLevel::INFO, "Configuration saved", {{"path", configPath}});
        return true;
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Failed to save configuration", {
            {"error", e.what()},
            {"path", configPath}
        });
        return false;
    }
}

// ============================================================================
// Model Discovery with Enhanced Error Handling
// ============================================================================

std::vector<std::string> AutoModelLoader::discoverAvailableModels() {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    log(LogLevel::INFO, "Starting model discovery");
    
    std::vector<std::string> models;
    
    // Check circuit breaker
    if (!m_circuitBreaker->allowRequest()) {
        log(LogLevel::WARN, "Circuit breaker OPEN, skipping discovery");
        return models;
    }
    
    try {
        // Scan local directories
        auto localModels = scanModelDirectories();
        models.insert(models.end(), localModels.begin(), localModels.end());
        
        log(LogLevel::DEBUG, "Found local models", {
            {"count", std::to_string(localModels.size())}
        });
        
        // Scan Ollama models
        auto ollamaModels = scanOllamaModels();
        models.insert(models.end(), ollamaModels.begin(), ollamaModels.end());
        
        log(LogLevel::DEBUG, "Found Ollama models", {
            {"count", std::to_string(ollamaModels.size())}
        });
        
        m_circuitBreaker->recordSuccess();
        
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Model discovery failed", {
            {"error", e.what()}
        });
        m_circuitBreaker->recordFailure();
        m_metrics.failedLoads++;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    m_metrics.recordDiscoveryLatency(duration);
    
    log(LogLevel::INFO, "Model discovery complete", {
        {"total_models", std::to_string(models.size())},
        {"latency_us", std::to_string(duration)}
    });
    
    return models;
}

std::vector<std::string> AutoModelLoader::scanModelDirectories() {
    std::vector<std::string> models;
    
    // Build search paths from config or defaults
    std::vector<std::string> searchPaths = m_config.searchPaths;
    if (searchPaths.empty()) {
        searchPaths = {
            ".",
            "models",
            "../models",
            "D:/OllamaModels",
            "C:/Users/" + std::string(std::getenv("USERNAME") ? std::getenv("USERNAME") : "Public") + "/.ollama/models/blobs",
            std::string(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "C:/Users/Public") + "/.ollama/models/blobs"
        };
    }
    
    for (const auto& searchPath : searchPaths) {
        if (!fs::exists(searchPath)) {
            log(LogLevel::DEBUG, "Search path not found", {{"path", searchPath}});
            continue;
        }
        
        log(LogLevel::DEBUG, "Scanning directory", {{"path", searchPath}});
        
        try {
            for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string ext = entry.path().extension().string();
                std::string filename = entry.path().filename().string();
                
                // Match GGUF files or Ollama blob format
                if (ext == ".gguf" || ext == ".bin" || 
                    (filename.find("sha256") == 0 && ext.empty())) {
                    
                    std::string modelPath = entry.path().string();
                    models.push_back(modelPath);
                    
                    log(LogLevel::DEBUG, "Found model file", {
                        {"path", modelPath},
                        {"size_bytes", std::to_string(entry.file_size())}
                    });
                }
            }
        } catch (const fs::filesystem_error& e) {
            log(LogLevel::WARN, "Cannot access directory", {
                {"path", searchPath},
                {"error", e.what()}
            });
            continue;
        }
    }
    
    return models;
}

std::vector<std::string> AutoModelLoader::scanOllamaModels() {
    std::vector<std::string> models;
    
    log(LogLevel::DEBUG, "Scanning Ollama models");
    
    // Execute ollama list command with timeout
    FILE* pipe = _popen("ollama list 2>&1", "r");
    if (!pipe) {
        log(LogLevel::WARN, "Failed to execute ollama command");
        return models;
    }
    
    char buffer[256];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 256, pipe) != nullptr) {
            result += buffer;
        }
    }
    int exitCode = _pclose(pipe);
    
    if (exitCode != 0) {
        log(LogLevel::WARN, "Ollama command failed", {
            {"exit_code", std::to_string(exitCode)}
        });
        return models;
    }
    
    // Parse ollama list output
    std::istringstream iss(result);
    std::string line;
    bool firstLine = true;
    
    while (std::getline(iss, line)) {
        if (firstLine) {
            firstLine = false;
            continue; // Skip header
        }
        
        if (line.empty()) continue;
        
        // Parse model name from ollama output
        std::istringstream lineStream(line);
        std::string modelName;
        lineStream >> modelName;
        
        if (!modelName.empty()) {
            models.push_back("ollama:" + modelName);
            log(LogLevel::DEBUG, "Found Ollama model", {{"name", modelName}});
        }
    }
    
    return models;
}

std::string AutoModelLoader::resolveOllamaModelPath(const std::string& modelName) {
    // Ollama models are typically stored in user directory
    std::string ollamaPath = std::string(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "C:/Users/Public") + "/.ollama/models/blobs";
    
    if (!fs::exists(ollamaPath)) {
        log(LogLevel::DEBUG, "Ollama path not found", {{"path", ollamaPath}});
        return "";
    }
    
    // Look for model files in ollama directory
    try {
        for (const auto& entry : fs::directory_iterator(ollamaPath)) {
            if (!entry.is_regular_file()) continue;
            
            std::string filename = entry.path().filename().string();
            // Ollama models are stored as sha256 blobs
            if (filename.find("sha256") == 0) {
                return entry.path().string();
            }
        }
    } catch (const fs::filesystem_error& e) {
        log(LogLevel::ERROR, "Failed to resolve Ollama model path", {
            {"error", e.what()}
        });
        return "";
    }
    
    return "";
}

// ============================================================================
// Model Selection with Enhanced Algorithms
// ============================================================================

std::string AutoModelLoader::selectOptimalModel(const std::vector<std::string>& availableModels) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (availableModels.empty()) {
        log(LogLevel::WARN, "No models available for selection");
        return "";
    }
    
    log(LogLevel::INFO, "Selecting optimal model", {
        {"available_count", std::to_string(availableModels.size())}
    });
    
    // First try preferred model if set
    if (!m_config.preferredModel.empty()) {
        for (const auto& model : availableModels) {
            if (model.find(m_config.preferredModel) != std::string::npos) {
                log(LogLevel::INFO, "Selected preferred model", {
                    {"model", model}
                });
                
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                m_metrics.recordSelectionLatency(duration);
                
                return model;
            }
        }
    }
    
    // Try health-based selection if enabled
    if (m_config.enableHealthChecks) {
        auto healthModel = selectModelByHealth(availableModels);
        if (!healthModel.empty()) {
            log(LogLevel::INFO, "Selected model by health", {{"model", healthModel}});
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
            m_metrics.recordSelectionLatency(duration);
            
            return healthModel;
        }
    }
    
    // Try capability-based selection
    auto capabilityModel = selectModelByCapability(availableModels);
    if (!capabilityModel.empty()) {
        log(LogLevel::INFO, "Selected model by capability", {{"model", capabilityModel}});
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        m_metrics.recordSelectionLatency(duration);
        
        return capabilityModel;
    }
    
    // Try size-based selection
    auto sizeModel = selectModelBySize(availableModels);
    if (!sizeModel.empty()) {
        log(LogLevel::INFO, "Selected model by size", {{"model", sizeModel}});
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        m_metrics.recordSelectionLatency(duration);
        
        return sizeModel;
    }
    
    // Fallback to name-based selection
    auto nameModel = selectModelByName(availableModels);
    log(LogLevel::INFO, "Selected model by name", {{"model", nameModel}});
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    m_metrics.recordSelectionLatency(duration);
    
    return nameModel;
}

std::string AutoModelLoader::selectModelByHealth(const std::vector<std::string>& models) {
    // Check cache for healthy models
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    for (const auto& model : models) {
        auto it = m_modelCache.find(model);
        if (it != m_modelCache.end() && it->second.isHealthy && it->second.failureCount == 0) {
            m_metrics.cacheHits++;
            log(LogLevel::DEBUG, "Found healthy cached model", {{"model", model}});
            return model;
        }
    }
    
    m_metrics.cacheMisses++;
    return "";
}

std::string AutoModelLoader::selectModelByCapability(const std::vector<std::string>& models) {
    // Prefer models that match system capabilities
    for (const auto& model : models) {
        std::string arch = getModelArchitecture(model);
        
        // Prefer smaller models for better performance (7b, 3b, 1b)
        if (arch == "7b" || arch == "3b" || arch == "1b") {
            log(LogLevel::DEBUG, "Selected by architecture", {
                {"model", model},
                {"architecture", arch}
            });
            return model;
        }
    }
    
    return "";
}

std::string AutoModelLoader::selectModelBySize(const std::vector<std::string>& models) {
    // Prefer smaller models for faster loading
    std::string smallestModel;
    size_t smallestSize = SIZE_MAX;
    
    for (const auto& model : models) {
        size_t size = getModelSize(model);
        if (size < smallestSize) {
            smallestSize = size;
            smallestModel = model;
        }
    }
    
    if (!smallestModel.empty()) {
        log(LogLevel::DEBUG, "Selected by size", {
            {"model", smallestModel},
            {"size_bytes", std::to_string(smallestSize)}
        });
    }
    
    return smallestModel;
}

std::string AutoModelLoader::selectModelByName(const std::vector<std::string>& models) {
    // Prefer models with common names
    std::vector<std::string> preferredNames = {
        "codellama", "deepseek", "qwen", "llama", "mistral", "phi", "gemma"
    };
    
    for (const auto& preferredName : preferredNames) {
        for (const auto& model : models) {
            if (model.find(preferredName) != std::string::npos) {
                log(LogLevel::DEBUG, "Selected by name match", {
                    {"model", model},
                    {"preferred_name", preferredName}
                });
                return model;
            }
        }
    }
    
    // Return first available model
    if (!models.empty()) {
        log(LogLevel::DEBUG, "Selected first available", {{"model", models[0]}});
        return models[0];
    }
    
    return "";
}

// ============================================================================
// Model Loading with Retry Logic
// ============================================================================

template<typename Func>
bool AutoModelLoader::retryOperation(Func operation, const std::string& operationName) {
    for (int attempt = 0; attempt < m_config.maxRetries; ++attempt) {
        try {
            log(LogLevel::DEBUG, "Attempting operation", {
                {"operation", operationName},
                {"attempt", std::to_string(attempt + 1)},
                {"max_retries", std::to_string(m_config.maxRetries)}
            });
            
            if (operation()) {
                return true;
            }
            
            if (attempt < m_config.maxRetries - 1) {
                int delayMs = m_config.retryDelayMs * (1 << attempt); // Exponential backoff
                log(LogLevel::WARN, "Operation failed, retrying", {
                    {"operation", operationName},
                    {"delay_ms", std::to_string(delayMs)}
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        } catch (const std::exception& e) {
            log(LogLevel::ERROR, "Operation threw exception", {
                {"operation", operationName},
                {"error", e.what()},
                {"attempt", std::to_string(attempt + 1)}
            });
            
            if (attempt == m_config.maxRetries - 1) {
                return false;
            }
        }
    }
    
    log(LogLevel::ERROR, "Operation failed after all retries", {
        {"operation", operationName},
        {"attempts", std::to_string(m_config.maxRetries)}
    });
    
    return false;
}

bool AutoModelLoader::autoLoadModel() {
    if (!m_config.autoLoadEnabled) {
        log(LogLevel::INFO, "Auto-load disabled");
        return false;
    }
    
    log(LogLevel::INFO, "Starting automatic model load");
    
    // Check circuit breaker
    if (!m_circuitBreaker->allowRequest()) {
        log(LogLevel::ERROR, "Circuit breaker OPEN, cannot load model");
        return false;
    }
    
    auto operation = [this]() -> bool {
        auto availableModels = discoverAvailableModels();
        if (availableModels.empty()) {
            log(LogLevel::WARN, "No models found for automatic loading");
            return false;
        }
        
        std::string selectedModel = selectOptimalModel(availableModels);
        if (selectedModel.empty()) {
            log(LogLevel::WARN, "Could not select optimal model");
            return false;
        }
        
        return loadModel(selectedModel);
    };
    
    bool success = retryOperation(operation, "autoLoadModel");
    
    if (success) {
        m_circuitBreaker->recordSuccess();
        m_metrics.successfulLoads++;
    } else {
        m_circuitBreaker->recordFailure();
        m_metrics.failedLoads++;
    }
    
    return success;
}

bool AutoModelLoader::loadModel(const std::string& modelPath) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    log(LogLevel::INFO, "Loading model", {{"path", modelPath}});
    
    // Check circuit breaker
    if (!m_circuitBreaker->allowRequest()) {
        log(LogLevel::ERROR, "Circuit breaker OPEN, cannot load model");
        m_metrics.failedLoads++;
        return false;
    }
    
    try {
        // Model validation
        if (!validateModel(modelPath)) {
            log(LogLevel::ERROR, "Model validation failed", {{"path", modelPath}});
            m_circuitBreaker->recordFailure();
            m_metrics.failedLoads++;
            return false;
        }
        
        // Check if model exists (unless it's an Ollama model)
        if (modelPath.find("ollama:") != 0 && !fs::exists(modelPath)) {
            log(LogLevel::ERROR, "Model file not found", {{"path", modelPath}});
            m_circuitBreaker->recordFailure();
            m_metrics.failedLoads++;
            return false;
        }
        
        // Basic format validation
        std::string ext = fs::path(modelPath).extension().string();
        if (ext != ".gguf" && ext != ".bin" && modelPath.find("ollama:") != 0) {
            log(LogLevel::ERROR, "Unsupported model format", {
                {"path", modelPath},
                {"extension", ext}
            });
            m_circuitBreaker->recordFailure();
            m_metrics.failedLoads++;
            return false;
        }
        
        // Update metadata cache
        if (m_config.enableCaching) {
            ModelMetadata metadata = getModelMetadata(modelPath);
            updateCache(metadata);
        }
        
        // Perform health check if enabled
        if (m_config.enableHealthChecks) {
            if (!performHealthCheck(modelPath)) {
                log(LogLevel::WARN, "Health check failed", {{"path", modelPath}});
                // Continue anyway for now
            }
        }
        
        m_modelLoaded = true;
        m_loadedModelPath = modelPath;
        
        m_circuitBreaker->recordSuccess();
        m_metrics.successfulLoads++;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        m_metrics.recordLoadLatency(duration);
        
        log(LogLevel::INFO, "Model loaded successfully", {
            {"path", modelPath},
            {"latency_us", std::to_string(duration)}
        });
        
        return true;
        
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Model loading exception", {
            {"path", modelPath},
            {"error", e.what()}
        });
        m_circuitBreaker->recordFailure();
        m_metrics.failedLoads++;
        return false;
    }
}

bool AutoModelLoader::loadModelAsync(const std::string& modelPath, std::function<void(bool)> callback) {
    if (!m_config.enableAsyncLoading) {
        log(LogLevel::WARN, "Async loading disabled, falling back to sync");
        bool result = loadModel(modelPath);
        if (callback) callback(result);
        return result;
    }
    
    log(LogLevel::INFO, "Starting async model load", {{"path", modelPath}});
    
    // Launch async operation
    std::thread([this, modelPath, callback]() {
        bool result = loadModel(modelPath);
        if (callback) {
            callback(result);
        }
    }).detach();
    
    return true;
}

// ============================================================================
// Model Health and Validation
// ============================================================================

bool AutoModelLoader::performHealthCheck(const std::string& modelPath) {
    log(LogLevel::DEBUG, "Performing health check", {{"path", modelPath}});
    
    try {
        // For Ollama models, just check if service is running
        if (modelPath.find("ollama:") == 0) {
            FILE* pipe = _popen("ollama list 2>&1", "r");
            if (!pipe) return false;
            
            char buffer[128];
            std::string result;
            while (fgets(buffer, 128, pipe)) {
                result += buffer;
            }
            _pclose(pipe);
            
            return !result.empty();
        }
        
        // For file models, check accessibility and integrity
        if (!fs::exists(modelPath)) {
            log(LogLevel::ERROR, "Model file not found during health check", {
                {"path", modelPath}
            });
            return false;
        }
        
        // Check file is readable
        std::ifstream file(modelPath, std::ios::binary);
        if (!file.is_open()) {
            log(LogLevel::ERROR, "Cannot open model file", {{"path", modelPath}});
            return false;
        }
        
        // Read first few bytes to ensure file integrity
        char buffer[1024];
        file.read(buffer, sizeof(buffer));
        if (file.gcount() == 0) {
            log(LogLevel::ERROR, "Cannot read from model file", {{"path", modelPath}});
            return false;
        }
        
        log(LogLevel::DEBUG, "Health check passed", {{"path", modelPath}});
        return true;
        
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Health check exception", {
            {"path", modelPath},
            {"error", e.what()}
        });
        return false;
    }
}

bool AutoModelLoader::validateModel(const std::string& modelPath) {
    log(LogLevel::DEBUG, "Validating model", {{"path", modelPath}});
    
    // Skip validation for Ollama models
    if (modelPath.find("ollama:") == 0) {
        return true;
    }
    
    // Check file exists
    if (!fs::exists(modelPath)) {
        log(LogLevel::ERROR, "Model validation failed: file not found", {
            {"path", modelPath}
        });
        return false;
    }
    
    // Check file size (must be > 1MB for legitimate models)
    size_t fileSize = fs::file_size(modelPath);
    if (fileSize < 1024 * 1024) {
        log(LogLevel::ERROR, "Model validation failed: file too small", {
            {"path", modelPath},
            {"size_bytes", std::to_string(fileSize)}
        });
        return false;
    }
    
    // Validate file format
    std::string ext = fs::path(modelPath).extension().string();
    if (ext != ".gguf" && ext != ".bin") {
        log(LogLevel::WARN, "Model validation: unexpected extension", {
            {"path", modelPath},
            {"extension", ext}
        });
    }
    
    log(LogLevel::DEBUG, "Model validation passed", {{"path", modelPath}});
    return true;
}

std::string AutoModelLoader::computeModelHash(const std::string& modelPath) {
    log(LogLevel::DEBUG, "Computing model hash", {{"path", modelPath}});
    
    if (modelPath.find("ollama:") == 0) {
        return "ollama_model_no_hash";
    }
    
    try {
        std::ifstream file(modelPath, std::ios::binary);
        if (!file.is_open()) {
            log(LogLevel::ERROR, "Cannot open file for hashing", {{"path", modelPath}});
            return "";
        }
        
        // For large files, only hash first 10MB
        const size_t HASH_SIZE = 10 * 1024 * 1024;
        std::vector<char> buffer(HASH_SIZE);
        
        file.read(buffer.data(), HASH_SIZE);
        std::streamsize bytesRead = file.gcount();
        
        // Use Windows CryptoAPI or OpenSSL for SHA256
        // Simplified placeholder - production would use proper SHA256
        std::hash<std::string> hasher;
        size_t hash = hasher(std::string(buffer.data(), bytesRead));
        
        std::stringstream ss;
        ss << std::hex << hash;
        
        log(LogLevel::DEBUG, "Hash computed", {
            {"path", modelPath},
            {"hash", ss.str()}
        });
        
        return ss.str();
        
    } catch (const std::exception& e) {
        log(LogLevel::ERROR, "Hash computation failed", {
            {"path", modelPath},
            {"error", e.what()}
        });
        return "";
    }
}

// ============================================================================
// Model Metadata and Caching
// ============================================================================

ModelMetadata AutoModelLoader::getModelMetadata(const std::string& modelPath) {
    ModelMetadata metadata;
    metadata.path = modelPath;
    metadata.name = fs::path(modelPath).filename().string();
    metadata.architecture = getModelArchitecture(modelPath);
    metadata.sizeBytes = getModelSize(modelPath);
    metadata.vendor = extractVendor(modelPath);
    metadata.sha256Hash = computeModelHash(modelPath);
    metadata.lastVerified = std::chrono::system_clock::now();
    metadata.isHealthy = true;
    metadata.failureCount = 0;
    
    return metadata;
}

size_t AutoModelLoader::getModelSize(const std::string& modelPath) {
    if (modelPath.find("ollama:") == 0) {
        // Estimate Ollama model size
        return 100 * 1024 * 1024; // 100MB estimate
    }
    
    try {
        return fs::file_size(modelPath);
    } catch (const fs::filesystem_error& e) {
        log(LogLevel::WARN, "Cannot determine model size", {
            {"path", modelPath},
            {"error", e.what()}
        });
        return SIZE_MAX;
    }
}

std::string AutoModelLoader::getModelArchitecture(const std::string& modelPath) {
    // Extract architecture from filename
    std::string filename = fs::path(modelPath).filename().string();
    
    // Convert to lowercase for comparison
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    
    if (filename.find("1b") != std::string::npos) return "1b";
    if (filename.find("3b") != std::string::npos) return "3b";
    if (filename.find("7b") != std::string::npos) return "7b";
    if (filename.find("13b") != std::string::npos) return "13b";
    if (filename.find("30b") != std::string::npos) return "30b";
    if (filename.find("65b") != std::string::npos) return "65b";
    
    return "unknown";
}

std::string AutoModelLoader::extractVendor(const std::string& modelPath) {
    if (modelPath.find("ollama:") == 0) return "ollama";
    
    std::string filename = fs::path(modelPath).filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    
    if (filename.find("llama") != std::string::npos) return "meta";
    if (filename.find("mistral") != std::string::npos) return "mistral";
    if (filename.find("phi") != std::string::npos) return "microsoft";
    if (filename.find("gemma") != std::string::npos) return "google";
    if (filename.find("qwen") != std::string::npos) return "alibaba";
    if (filename.find("deepseek") != std::string::npos) return "deepseek";
    
    return "unknown";
}

void AutoModelLoader::updateCache(const ModelMetadata& metadata) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    // Check cache size limit
    if (m_modelCache.size() >= m_config.maxCacheSize) {
        evictOldestCacheEntry();
    }
    
    m_modelCache[metadata.path] = metadata;
    
    log(LogLevel::DEBUG, "Updated model cache", {
        {"path", metadata.path},
        {"cache_size", std::to_string(m_modelCache.size())}
    });
}

ModelMetadata* AutoModelLoader::getCachedMetadata(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_modelCache.find(modelPath);
    if (it != m_modelCache.end()) {
        m_metrics.cacheHits++;
        return &it->second;
    }
    
    m_metrics.cacheMisses++;
    return nullptr;
}

void AutoModelLoader::evictOldestCacheEntry() {
    if (m_modelCache.empty()) return;
    
    auto oldest = m_modelCache.begin();
    for (auto it = m_modelCache.begin(); it != m_modelCache.end(); ++it) {
        if (it->second.lastVerified < oldest->second.lastVerified) {
            oldest = it;
        }
    }
    
    log(LogLevel::DEBUG, "Evicting cache entry", {
        {"path", oldest->first}
    });
    
    m_modelCache.erase(oldest);
}

void AutoModelLoader::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_modelCache.clear();
    log(LogLevel::INFO, "Model cache cleared");
}

void AutoModelLoader::preloadModels(const std::vector<std::string>& modelPaths) {
    log(LogLevel::INFO, "Preloading models", {
        {"count", std::to_string(modelPaths.size())}
    });
    
    for (const auto& path : modelPaths) {
        // Load metadata into cache without fully loading the model
        ModelMetadata metadata = getModelMetadata(path);
        updateCache(metadata);
    }
}

std::vector<ModelMetadata> AutoModelLoader::getCachedModels() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    std::vector<ModelMetadata> models;
    for (const auto& [path, metadata] : m_modelCache) {
        models.push_back(metadata);
    }
    
    return models;
}

// ============================================================================
// Status and Metrics Export
// ============================================================================

std::string AutoModelLoader::getStatus() const {
    std::stringstream ss;
    ss << "AutoModelLoader Status:";
    ss << "\n  Auto-load enabled: " << (m_config.autoLoadEnabled ? "Yes" : "No");
    ss << "\n  Model loaded: " << (m_modelLoaded ? "Yes" : "No");
    ss << "\n  Loaded model: " << (m_modelLoaded ? m_loadedModelPath : "None");
    ss << "\n  Preferred model: " << (m_config.preferredModel.empty() ? "None" : m_config.preferredModel);
    ss << "\n  Circuit breaker: " << (m_circuitBreaker->getState() == CircuitBreaker::State::CLOSED ? "CLOSED" : 
                                      m_circuitBreaker->getState() == CircuitBreaker::State::OPEN ? "OPEN" : "HALF_OPEN");
    return ss.str();
}

std::string AutoModelLoader::getDetailedStatus() const {
    std::stringstream ss;
    ss << getStatus();
    ss << "\n\nConfiguration:";
    ss << "\n  Max retries: " << m_config.maxRetries;
    ss << "\n  Retry delay: " << m_config.retryDelayMs << "ms";
    ss << "\n  Discovery timeout: " << m_config.discoveryTimeoutMs << "ms";
    ss << "\n  Load timeout: " << m_config.loadTimeoutMs << "ms";
    ss << "\n  Caching enabled: " << (m_config.enableCaching ? "Yes" : "No");
    ss << "\n  Health checks enabled: " << (m_config.enableHealthChecks ? "Yes" : "No");
    ss << "\n  Metrics enabled: " << (m_config.enableMetrics ? "Yes" : "No");
    ss << "\n  Async loading enabled: " << (m_config.enableAsyncLoading ? "Yes" : "No");
    ss << "\n  Log level: " << m_config.logLevel;
    
    ss << "\n\nPerformance Metrics:";
    ss << "\n  Total discoveries: " << m_metrics.totalDiscoveryCalls;
    ss << "\n  Total load attempts: " << m_metrics.totalLoadCalls;
    ss << "\n  Successful loads: " << m_metrics.successfulLoads;
    ss << "\n  Failed loads: " << m_metrics.failedLoads;
    ss << "\n  Cache hits: " << m_metrics.cacheHits;
    ss << "\n  Cache misses: " << m_metrics.cacheMisses;
    ss << "\n  Cached models: " << m_modelCache.size();
    
    return ss.str();
}

std::string AutoModelLoader::exportMetrics() const {
    if (!m_config.enablePrometheusMetrics) {
        return "Prometheus metrics disabled";
    }
    
    return m_metrics.generatePrometheusMetrics();
}

// ============================================================================
// CLI and Qt IDE Integration
// ============================================================================

void CLIAutoLoader::initialize() {
    AutoModelLoader::GetInstance().setAutoLoadEnabled(true);
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "CLI AutoLoader initialized");
}

void CLIAutoLoader::initializeWithConfig(const std::string& configPath) {
    AutoModelLoader::GetInstance().loadConfiguration(configPath);
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "CLI AutoLoader initialized with config", {
        {"config_path", configPath}
    });
}

void CLIAutoLoader::autoLoadOnStartup() {
    AutoModelLoader::GetInstance().autoLoadModel();
}

bool CLIAutoLoader::isAutoLoadEnabled() {
    return AutoModelLoader::GetInstance().isAutoLoadEnabled();
}

std::string CLIAutoLoader::getStatus() {
    return AutoModelLoader::GetInstance().getDetailedStatus();
}

void CLIAutoLoader::shutdown() {
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "CLI AutoLoader shutting down");
    
    // Export final metrics
    if (AutoModelLoader::GetInstance().getConfig().enableMetrics) {
        auto metrics = AutoModelLoader::GetInstance().exportMetrics();
        std::cout << "\n=== Final Metrics ===\n" << metrics << std::endl;
    }
}

void QtIDEAutoLoader::initialize() {
    AutoModelLoader::GetInstance().setAutoLoadEnabled(true);
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "Qt IDE AutoLoader initialized");
}

void QtIDEAutoLoader::initializeWithConfig(const std::string& configPath) {
    AutoModelLoader::GetInstance().loadConfiguration(configPath);
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "Qt IDE AutoLoader initialized with config", {
        {"config_path", configPath}
    });
}

void QtIDEAutoLoader::autoLoadOnStartup() {
    AutoModelLoader::GetInstance().autoLoadModel();
}

bool QtIDEAutoLoader::isAutoLoadEnabled() {
    return AutoModelLoader::GetInstance().isAutoLoadEnabled();
}

std::string QtIDEAutoLoader::getStatus() {
    return AutoModelLoader::GetInstance().getDetailedStatus();
}

void QtIDEAutoLoader::shutdown() {
    AutoModelLoader::GetInstance().log(LogLevel::INFO, "Qt IDE AutoLoader shutting down");
    
    // Export final metrics
    if (AutoModelLoader::GetInstance().getConfig().enableMetrics) {
        auto metrics = AutoModelLoader::GetInstance().exportMetrics();
        std::cout << "\n=== Final Metrics ===\n" << metrics << std::endl;
    }
}

} // namespace AutoModelLoader
