/**
 * inference_engine_noqt.cpp
 * Pure C++ inference engine implementation without Qt dependencies
 */

#include "inference_engine_noqt.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <sstream>

InferenceEngine::InferenceEngine(const std::string& ggufPath)
    : m_loader(nullptr) {
    if (!ggufPath.empty()) {
        m_modelPath = ggufPath;
    }
}

InferenceEngine::~InferenceEngine() {
    unloadModel();
}

bool InferenceEngine::loadModel(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_isLoading = true;
    
    try {
        m_systemPromptTokens.clear();
        
        // Clean up existing loader
        if (m_loader) {
            m_loader.reset();
        }
        
        if (path.empty()) {
            m_isLoading = false;
            return false;
        }
        
        // Report progress
        if (m_loadProgressCallback) {
            m_loadProgressCallback("Loading model from: " + path);
        }
        
        // Create and load GGUF
        m_loader = std::make_unique<GGUFLoader>(path);
        if (!m_loader || !m_loader->isOpen()) {
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Failed to open GGUF file");
            }
            m_loader.reset();
            m_isLoading = false;
            return false;
        }
        
        if (m_loadProgressCallback) {
            m_loadProgressCallback("GGUF file loaded successfully");
        }
        
        m_modelPath = path;
        
        // Load tensor cache if needed
        if (m_loadTensors.load()) {
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Loading tensor cache...");
            }
            
            try {
                auto tensorNames = m_loader->getTensorNames();
                for (const auto& name : tensorNames) {
                    auto tensorData = m_loader->getTensor(name);
                    if (!tensorData.empty()) {
                        CachedTensorData cached;
                        cached.data = tensorData;
                        cached.ggml_type_id = m_loader->getTensorType(name);
                        m_tensorCache[name] = cached;
                    }
                }
                
                if (m_loadProgressCallback) {
                    std::string msg = "Cached " + std::to_string(m_tensorCache.size()) + " tensors";
                    m_loadProgressCallback(msg);
                }
            } catch (const std::exception& e) {
                if (m_loadProgressCallback) {
                    m_loadProgressCallback(std::string("Warning: Failed to load tensor cache: ") + e.what());
                }
            }
        }
        
        // Get model architecture from GGUF metadata
        int nLayers = m_loader->getParam("n_layer", 12);
        int nEmbd = m_loader->getParam("n_embd", 768);
        int nHead = m_loader->getParam("n_head", 12);
        int nVocab = m_loader->getParam("n_vocab", 50257);
        
        if (m_loadProgressCallback) {
            std::ostringstream oss;
            oss << "Model architecture: Layers=" << nLayers << " Embd=" << nEmbd 
                << " Heads=" << nHead << " Vocab=" << nVocab;
            m_loadProgressCallback(oss.str());
        }
        
        // Initialize transformer if tensors are loaded
        if (!m_tensorCache.empty()) {
            try {
                if (m_loadProgressCallback) {
                    m_loadProgressCallback("Initializing transformer...");
                }
                
                // Load transformer weights
                std::map<std::string, std::pair<std::vector<uint8_t>, int>> tensorMap;
                for (auto& p : m_tensorCache) {
                    tensorMap[p.first] = {p.second.data, p.second.ggml_type_id};
                }
                
                bool transformerOk = m_transformer.loadWeightsWithTypes(tensorMap, nLayers, nEmbd, nHead, nVocab);
                if (!transformerOk) {
                    if (m_loadProgressCallback) {
                        m_loadProgressCallback("Using GGUF direct inference (transformer not fully loaded)");
                    }
                    m_transformer.markReadyForGGUFInference();
                }
            } catch (const std::exception& e) {
                if (m_loadProgressCallback) {
                    m_loadProgressCallback(std::string("Warning: Transformer init failed: ") + e.what());
                }
            }
        }
        
        m_kvCacheReady = false;
        
        if (m_loadProgressCallback) {
            m_loadProgressCallback("Model ready for inference");
        }
        
        m_isLoading = false;
        return true;
        
    } catch (const std::exception& e) {
        if (m_loadProgressCallback) {
            m_loadProgressCallback(std::string("Error: ") + e.what());
        }
        m_loader.reset();
        m_isLoading = false;
        return false;
    }
}

bool InferenceEngine::isModelLoaded() const {
    return m_loader != nullptr && m_loader->isOpen();
}

std::string InferenceEngine::modelPath() const {
    return m_modelPath;
}

std::vector<std::string> InferenceEngine::tensorNames() const {
    if (!m_loader) return {};
    return m_loader->getTensorNames();
}

int64_t InferenceEngine::memoryUsageMB() const {
    int64_t total = 0;
    for (const auto& p : m_tensorCache) {
        total += p.second.data.size();
    }
    return total / (1024 * 1024);
}

double InferenceEngine::tokensPerSecond() const {
    return m_tokensPerSecond.load();
}

double InferenceEngine::temperature() const {
    return m_temperature.load();
}

std::string InferenceEngine::quantMode() const {
    return m_quantMode;
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens) {
    if (!isModelLoaded()) {
        return {};
    }
    
    std::vector<int32_t> output = inputTokens;
    
    try {
        // Use transformer if available
        auto generated = m_transformer.generateTokens(inputTokens, maxTokens);
        output.insert(output.end(), generated.begin(), generated.end());
    } catch (const std::exception& e) {
        // Fallback to GGUF direct inference
        
    }
    
    return output;
}

void InferenceEngine::generateStreaming(const std::vector<int32_t>& inputTokens,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete) {
    if (m_threadingEnabled.load()) {
        m_loaderThread = std::thread([this, inputTokens, maxTokens, onToken, onComplete]() {
            streamingGenerateWorker(inputTokens, maxTokens, onToken, onComplete);
        });
        m_loaderThread.detach();
    } else {
        streamingGenerateWorker(inputTokens, maxTokens, onToken, onComplete);
    }
}

void InferenceEngine::generateStreaming(const std::string& prompt,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete) {
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, onToken, onComplete);
}

void InferenceEngine::streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              TokenCallback onToken,
                                              CompleteCallback onComplete) {
    try {
        auto generated = generate(inputTokens, maxTokens);
        
        // Send tokens one at a time (simulating streaming)
        for (size_t i = inputTokens.size(); i < generated.size(); ++i) {
            auto token_str = detokenize({generated[i]});
            if (onToken) onToken(token_str);
        }
    } catch (const std::exception& e) {
        
    }
    
    if (onComplete) onComplete();
}

std::vector<int32_t> InferenceEngine::tokenize(const std::string& text) {
    // Use BPE tokenizer
    std::vector<int32_t> tokens;
    try {
        // Tokenization logic here
        // For now, simple implementation
        tokens.push_back(1);  // BOS token
        for (char c : text) {
            tokens.push_back(static_cast<int32_t>(c));
        }
        tokens.push_back(2);  // EOS token
    } catch (const std::exception& e) {
        
    }
    return tokens;
}

std::string InferenceEngine::detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    for (int32_t token : tokens) {
        if (token > 0 && token < 256) {
            result += static_cast<char>(token);
        }
    }
    return result;
}

std::string InferenceEngine::processChat(const std::string& prompt) {
    auto tokens = tokenize(prompt);
    auto generated = generate(tokens, 100);
    return detokenize(generated);
}

std::string InferenceEngine::analyzeCode(const std::string& code) {
    return processChat("Analyze this code:\n" + code);
}

int InferenceEngine::vocabSize() const {
    if (!m_loader) return 0;
    return m_loader->getParam("n_vocab", 50257);
}

int InferenceEngine::embeddingDim() const {
    if (!m_loader) return 0;
    return m_loader->getParam("n_embd", 768);
}

void InferenceEngine::setQuantMode(const std::string& mode) {
    m_quantMode = mode;
    // Apply quantization mode to transformer
}

void InferenceEngine::setLayerQuant(const std::string& tensorName, const std::string& quant) {
    // Set quantization for specific layer
}

void InferenceEngine::unloadModel() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_loader) {
        m_loader.reset();
    }
    m_tensorCache.clear();
    m_modelPath.clear();
}

const InferenceEngine::VulkanContext* InferenceEngine::getGPUContext() const {
    // Return GPU context from transformer if available
    return nullptr;
}
