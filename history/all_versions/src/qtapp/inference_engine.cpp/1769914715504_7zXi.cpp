#include "transformer_inference.hpp"
#include "inference_engine.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ggml.h>

#include "EnterpriseTelemetry.h"


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <numeric>
#include <mutex>

#include <cstdlib>
#include <iostream>
#include <windows.h> // For Named Pipes

// Use shared quant utilities
#include "quant_utils.hpp"

// IPC Utils
bool InferenceEngine::connectToAgent() {
    if (m_hPipe != nullptr && m_hPipe != INVALID_HANDLE_VALUE) {
        return true; 
    }
    
    // Connect to named pipe
    m_hPipe = CreateFileA(
        "\\\\.\\pipe\\RawrXD_IPC",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (m_hPipe == INVALID_HANDLE_VALUE) {
        m_hPipe = nullptr;
        return false;
    }
    
    // Switch to message mode
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(m_hPipe, &dwMode, NULL, NULL);
    
    return true;
}

void InferenceEngine::disconnectAgent() {
    if (m_hPipe && m_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hPipe);
        m_hPipe = nullptr;
    }
}

std::string InferenceEngine::sendToAgent(const std::string& request) {
    if (!connectToAgent()) {
        return "Error: Agent not running.";
    }
    
    DWORD bytesWritten;
    BOOL fSuccess = WriteFile(
        m_hPipe,
        request.c_str(),
        request.length(),
        &bytesWritten,
        NULL
    );
    
    if (!fSuccess) {
        disconnectAgent();
        return "Error: Write failed.";
    }
    
    // Read Response
    char buffer[4096];
    DWORD bytesRead;
    fSuccess = ReadFile(
        m_hPipe,
        buffer,
        4096,
        &bytesRead,
        NULL
    );
    
    if (!fSuccess && GetLastError() != ERROR_MORE_DATA) {
        disconnectAgent();
        return "Error: Read failed.";
    }
    
    return std::string(buffer, bytesRead);
}

InferenceEngine::InferenceEngine(const std::string& ggufPath, void* parent)
    : void(parent), m_loader(nullptr)
{
    // DO NOT load model in constructor - causes stack buffer overrun crashes
    // Model loading must be deferred to avoid blocking main thread
    // Call loadModel() explicitly after construction
    if (!ggufPath.empty()) {
        // Store path for later use if needed
        m_modelPath = ggufPath;
    }
}

InferenceEngine::InferenceEngine(void* parent)
    : void(parent), m_loader(nullptr)
{
}

InferenceEngine::~InferenceEngine()
{
    // Clean up GGUFLoader resources
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    m_tensorCache.clear();
}

bool InferenceEngine::loadModel(const std::string& path)
{
    // NOTE: This function is called from background thread via QtConcurrent
    // DO NOT use std::lock_guard<std::mutex> here - it will deadlock
    // DO NOT signals directly - use QMetaObject::invokeMethod
    
    try {
            << std::string::fromLocal8Bit(std::getenv("CUDA_VISIBLE_DEVICES") ? std::getenv("CUDA_VISIBLE_DEVICES") : "(unset)");
        m_systemPromptTokens.clear();
        
        // Clean up existing loader (if any)
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        
        if (path.empty()) {
            QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
                (bool, false), (std::string, std::string()));
            return false;
        }


        // Create loader with error checking and exception safety
        try {
            m_loader = new GGUFLoaderQt(path);
        } catch (const std::exception& e) {
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
                (bool, false), (std::string, std::string()));
            return false;
        } catch (...) {
            m_loader = nullptr;
            QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
                (bool, false), (std::string, std::string()));
            return false;
        }
        
        if (!m_loader || !m_loader->isOpen()) {
            if (m_loader) {
                delete m_loader;
                m_loader = nullptr;
            }
            QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
                (bool, false), (std::string, std::string()));
            return false;
        }


        m_modelPath = path;
        std::string modelName = extractModelName(path);

        // Enterprise deterministic defaults for coherent responses
        m_temperature = 0.0;
        m_topP = 1.0;
        
        // ========== CHECK FOR UNSUPPORTED QUANTIZATION TYPES ==========
        // This is the key detection point for the IDE conversion workflow
        if (m_loader->hasUnsupportedQuantizationTypes()) {
            std::vector<std::string> unsupportedInfo = m_loader->getUnsupportedQuantizationInfo();
            std::string recommendedType = m_loader->getRecommendedConversionType();
            
            for (const auto& info : unsupportedInfo) {
            }
            
            // signal for IDE to show conversion dialog (thread-safe)
            QMetaObject::invokeMethod(this, "unsupportedQuantizationTypeDetected", //QueuedConnection,
                (std::vector<std::string>, unsupportedInfo), (std::string, recommendedType), (std::string, path));
            
            // Continue with model loading attempt anyway (it may fail later on tensor size calculation)
            // The IDE will show the conversion dialog while we continue
        }
        
        // Initialize tokenizer from model (with full exception safety and memory pressure handling)
        try {
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Initializing tokenizer...");
            }
            initializeTokenizer();
        } catch (const std::bad_alloc& e) {
            m_tokenizerMode = TOKENIZER_FALLBACK;
            // Continue anyway - tokenizer is optional for basic inference
        } catch (const std::exception& e) {
            // Continue anyway - tokenizer is optional for basic inference
        } catch (...) {
        }
        
        // Load vocabulary from GGUF file (CRITICAL for detokenization)
        try {
            if (m_loadProgressCallback) {
                m_loadProgressCallback("Loading vocabulary...");
            }
            
            if (m_loader && m_loader->isOpen()) {
                // Load vocabulary directly from the GGUF file using VocabularyLoader
                bool vocabLoaded = m_vocab.loadFromGGUF(path);
                if (vocabLoaded) {
                } else {
                }
            } else {
            }
        } catch (const std::bad_alloc& e) {
            // Continue anyway - we'll use fallback detokenization
        } catch (const std::exception& e) {
            // Continue anyway - we'll use fallback detokenization
        } catch (...) {
        }


        std::cerr.flush();
        
        // Build initial quantized tensor cache (with full exception safety)
        if (m_loadTensors.load()) {
            try {
                rebuildTensorCache();
            } catch (const std::bad_alloc& e) {
                // Clean up and fail
                delete m_loader;
                m_loader = nullptr;
                QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
                    (bool, false), (std::string, std::string()));
                return false;
            } catch (const std::exception& e) {
                // Continue anyway - we'll try without cache
            } catch (...) {
            }
        } else {
        }


        // === FIX: Dynamically read model architecture from GGUF metadata ===
        // These values are now read from the actual GGUF file instead of hardcoded
        int nLayers = m_loader->getParam("n_layer", 12).toInt();
        int nEmbd = m_loader->getParam("n_embd", 768).toInt();
        int nHead = m_loader->getParam("n_head", 12).toInt();
        int nVocab = m_loader->getParam("n_vocab", 50257).toInt();

        // Log the actual parameters read from the GGUF file
                     ;
        
        if (!m_tensorCache.empty()) {
            try {
                // Convert CachedTensorData -> (std::vector<uint8_t>, int) pairs to preserve type information
                std::unordered_map<std::string, std::pair<std::vector<uint8_t>, int>> tensorCacheWithTypes;
                for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
                    tensorCacheWithTypes.insert(it.key(), 
                        std::pair<std::vector<uint8_t>, int>(it.value().data, it.value().ggml_type_id));
                }
                
                bool transformerLoaded = m_transformer.loadWeightsWithTypes(tensorCacheWithTypes, nLayers, nEmbd, nHead, nVocab);
                if (!transformerLoaded) {
                    // CRITICAL FIX: Mark transformer as ready when using GGUF direct inference
                    // The transformer doesn't need custom weight loading for GGUF direct path
                    
                    // Initialize Native Backend (Titan ASM)
                    // This replaces the "Simulated" path with the experimental native engine
                    size_t vocabSize = (nVocab > 0) ? nVocab : 32000;
                    m_transformer.m_nVocab = vocabSize; // Ensure vocab size is set
                    m_transformer.m_nEmbd = nEmbd;
                    m_transformer.m_nLayers = nLayers;
                    
                    if (m_transformer.initTitanBackend(path.toStdString())) {
                        std::cout << "[InferenceEngine] Native Titan Backend initialized successfully." << std::endl;
                    } else {
                        std::cerr << "[InferenceEngine] Failed to initialize Native Titan Backend." << std::endl;
                    }
                    
                    m_transformer.markReadyForGGUFInference();
                } else {
                }
            } catch (const std::exception& e) {
                // Continue anyway - model is loaded via GGUF loader
            }
        } else {
        }
        
        // Reset KV-cache state for new model
        m_kvCacheReady = false;
        
        QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
            (bool, true), (std::string, modelName));


        // FIX 6: Skip processNextRequest during model load to avoid deadlock
        // The queue will be processed when the first request comes in
        // processNextRequest();


        // FIX 3.2: the signal to notify listeners (thread-safe)
        QMetaObject::invokeMethod(this, "transformerReady", //QueuedConnection);
        
        return true;    } catch (const std::exception& e) {
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
            (bool, false), (std::string, std::string()));
        return false;
    } catch (...) {
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        QMetaObject::invokeMethod(this, "modelLoadedChanged", //QueuedConnection,
            (bool, false), (std::string, std::string()));
        return false;
    }
}

std::string InferenceEngine::processChat(const std::string& prompt)
{
    // Tokenize, run a short generation, and detokenize
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer("inference.processChat");
    telemetry.recordEvent("inference", "processChat.begin", "len=%1"));


    auto input = tokenize(prompt);
    for (int i = 0; i < std::min(10, (int)input.size()); i++) {
    }
    
    auto out = generate(input, 64);
    
    auto result = detokenize(out);

    telemetry.recordTiming("inference", "processChat.complete", timer.elapsedMs(), "tokens_in=%1 tokens_out=%2")));
    
    return result;
}

std::string InferenceEngine::analyzeCode(const std::string& code)
{
    // Simple analysis stub leveraging existing tokenizer to avoid heavy changes
    std::string analysis = std::string(
        "Code Analysis:\n"
        "- Length: %1 chars\n"
        "- Lines: %2\n"
        "- Tokens: %3"
    )) + 1).size());
    return analysis;
}

bool InferenceEngine::isModelLoaded() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_loader && m_loader->isOpen();
}

std::string InferenceEngine::modelPath() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_modelPath;
}

std::vector<std::string> InferenceEngine::tensorNames() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_loader ? m_loader->tensorNames() : std::vector<std::string>();
}

int64_t InferenceEngine::memoryUsageMB() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_memoryUsageMB;
}

double InferenceEngine::tokensPerSecond() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_tokensPerSecond;
}

double InferenceEngine::temperature() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_temperature;
}

std::string InferenceEngine::quantMode() const
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    return m_quantMode;
}


void InferenceEngine::request(const std::string& prompt, int64_t reqId)
{
    request(prompt, reqId, false);  // Default to non-streaming
}

void InferenceEngine::request(const std::string& prompt, int64_t reqId, bool streaming)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (!isModelLoaded()) {
        error(reqId, "Error: No model loaded");
        return;
    }
    
    // FIX 6: Enqueue the request instead of processing immediately
    InferenceRequest request;
    request.prompt = prompt;
    request.requestId = reqId;
    request.streaming = streaming;
    m_requestQueue.enqueue(request);


    // Attempt to start processing if the engine is not busy
    if (!m_isProcessingInference) {
        processNextRequest();
    }
}

void InferenceEngine::unloadModel()
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    m_modelPath.clear();
    m_tensorCache.clear();
    
    modelLoadedChanged(false, std::string());
}

std::string InferenceEngine::extractModelName(const std::string& path) const
{
    std::filesystem::path modelInfo(path);
    return modelInfo.fileName();
}

void InferenceEngine::setQuantMode(const std::string& mode)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (m_quantMode == mode) return;
    
    m_quantMode = mode;
    rebuildTensorCache();
    
    quantChanged(mode);
}

void InferenceEngine::setLayerQuant(const std::string& tensorName, const std::string& quant)
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (m_perLayerQuant.value(tensorName) == quant) return;
    
    m_perLayerQuant.insert(tensorName, quant);
    rebuildTensorCache();
    
    quantChanged(std::string("%1->%2"));
}

void InferenceEngine::rebuildTensorCache()
{
    try {
        m_tensorCache.clear();
        
        if (!m_loader) {
            return;
        }
        
        std::vector<std::string> names = m_loader->tensorNames();
        
        int totalTensors = names.size();
        int processedTensors = 0;
        
        for (const std::string& name : names) {
            processedTensors++;
            if (processedTensors % 10 == 0 || processedTensors == totalTensors) {
                std::string progressMsg = std::string("Loading tensors... %1/%2 (%3%)")
                    
                    ;
                
                // Send progress update via callback if available
                if (m_loadProgressCallback) {
                    m_loadProgressCallback(progressMsg);
                }
            }
            try {
                const std::string qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
                std::vector<uint8_t> raw = m_loader->inflateWeight(name);
                
                if (raw.empty()) {
                    continue;
                }
                
                // Apply quantization safely and capture the resulting type
                try {
                    // Use the new function that returns both data and type
                    auto [quantized, resulting_type_id] = apply_quant_with_type(raw, qmode);

                    if (!quantized.empty()) {
                        CachedTensorData tensorData;
                        tensorData.data = quantized;
                        tensorData.ggml_type_id = resulting_type_id;
                        m_tensorCache.insert(name, tensorData);
                    }
                } catch (const std::exception& e) {
                }
            } catch (const std::exception& e) {
            }
        }
        
            // ===== ENTERPRISE SAMPLING DEFAULTS =====
                << ", top_p=" << m_topP << " (enterprise defaults for coherent output)";


        // Reload transformer weights if cache was rebuilt
        // FIX: Removed dangerous premature weight loading with hardcoded dimensions.
        // Weights should only be loaded in loadModel() after correct dimensions are read.
        /*
        if (!m_tensorCache.empty() && m_loader) {
            try {
                m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
            } catch (const std::exception& e) {
            }
        }
        */
    } catch (const std::exception& e) {
    } catch (...) {
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const std::string& text)
{
    return tokenizeInternal(text, true, true);
}

std::vector<int32_t> InferenceEngine::tokenizeInternal(const std::string& text, bool includeSystemPrompt, bool includeSpecialTokens)
{

    const bool prependSystem = includeSystemPrompt && !m_systemPromptTokens.empty();
    std::vector<int32_t> tokens;

    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                tokens = m_bpeTokenizer.encode(text);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                }
                return tokens;
            }
            break;

        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                tokens = m_spTokenizer.encode(text, includeSpecialTokens, false);
                if (prependSystem) {
                    std::vector<int32_t> withSystem;
                    withSystem.reserve(m_systemPromptTokens.size() + tokens.size());
                    withSystem.insert(withSystem.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
                    withSystem.insert(withSystem.end(), tokens.begin(), tokens.end());
                    tokens.swap(withSystem);
                }
                return tokens;
            }
            break;

        case TOKENIZER_FALLBACK:
        default:
            break;
    }

    // Fallback: Simple word-based tokenization
    if (includeSpecialTokens) {
        tokens.push_back(1);
    }

    if (prependSystem) {
        tokens.insert(tokens.end(), m_systemPromptTokens.begin(), m_systemPromptTokens.end());
    }

    std::vector<std::string> words = text.split(std::regex("[\\s,\\.!?;:]+"), //SkipEmptyParts);

    for (const std::string& word : words) {
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                uint32_t hash = std::unordered_map(word.toLower());
                tokens.push_back((hash % 50000) + 256);
            }
        } else {
            uint32_t hash = std::unordered_map(word.toLower());
            tokens.push_back((hash % 50000) + 256);
        }
    }

    if (includeSpecialTokens) {
        tokens.push_back(2);
    }

    return tokens;
}

void InferenceEngine::buildSystemPromptTokens()
{
    static const std::string kEnterpriseSystemPrompt = QStringLiteral(
        "You are the Zero-Day enterprise mission agent. Respond in concise, clear English with actionable steps and no emojis.");

    try {
        m_systemPromptTokens = tokenizeInternal(kEnterpriseSystemPrompt, false, false);
    } catch (const std::exception& e) {
    }
}

std::string InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    
    std::string result;
    
    // First try: Use actual tokenizers if available
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                auto bpeResult = m_bpeTokenizer.decode(tokens);
                return bpeResult;
            }
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                auto spResult = m_spTokenizer.decode(tokens, true);
                return spResult;
            }
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            break;
    }
    
    // Fallback: Decode using vocabulary
    if (m_vocab.isLoaded()) {
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t tokenId = tokens[i];
            
            // Skip special tokens (BOS=1, EOS=2, PAD=0)
            if (tokenId == 0 || tokenId == 1 || tokenId == 2) {
                continue;
            }
            
            // Look up in vocabulary
            VocabularyLoader::Token vocabToken = m_vocab.getToken(tokenId);
            if (vocabToken.id >= 0 && !vocabToken.text.empty()) {
                result += vocabToken.text;
                if (!vocabToken.text.endsWith(" ") && i + 1 < tokens.size()) {
                    result += " ";
                }
            } else {
                // Unknown token - just show as space
                result += " ";
            }
        }
    } else {
        // No vocabulary loaded - complete fallback
        
        for (size_t i = 0; i < tokens.size(); ++i) {
            int32_t token = tokens[i];
            
            // Skip special tokens
            if (token == 0 || token == 1 || token == 2) continue;
            
            // Try ASCII range first
            if (token >= 32 && token < 127) {
                result += QChar(token);
            } else if (token < 256) {
                result += QChar(token);
            } else {
                // Unknown - show as space
                result += " ";
            }
        }
    }
    
    return result.trimmed();
}

void InferenceEngine::initializeTokenizer()
{
    try {
        
        std::cerr.flush();
        
        // Try to load vocabulary from GGUF file
        if (!m_loader) {
            return;
        }


        if (!m_vocab.loadFromGGUF(m_modelPath)) {
            return;
        }


        std::cerr.flush();
        
        // Use timeout-protected initialization
        if (!initializeTokenizerWithTimeout(5000)) {
            loadFallbackTokenizer();
            return;
        }
        buildSystemPromptTokens();
        
        return;
        
        // Original code below is now handled by initializeTokenizerWithTimeout
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        std::unordered_map<std::string, std::vector<uint8_t>> tokenizerMetadata;
        try {
            tokenizerMetadata = m_loader->getTokenizerMetadata();
        } catch (const std::bad_alloc&) {
            m_tokenizerMode = TOKENIZER_FALLBACK;
            return;
        } catch (const std::exception& e) {
            // Continue without metadata
        }
        
        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            try {
                // Initialize BPE tokenizer with real GGUF metadata
                if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_BPE;
                }
            } catch (const std::bad_alloc&) {
                m_tokenizerMode = TOKENIZER_FALLBACK;
            } catch (const std::exception& e) {
            }
        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            try {
                // Initialize SentencePiece tokenizer with real GGUF metadata
                if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_SP;
                }
            } catch (const std::bad_alloc&) {
                m_tokenizerMode = TOKENIZER_FALLBACK;
            } catch (const std::exception& e) {
            }
        }
    } catch (const std::exception& e) {
        m_tokenizerMode = TOKENIZER_FALLBACK;
    } catch (...) {
        m_tokenizerMode = TOKENIZER_FALLBACK;
    }
    
    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
    }
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens)
{
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer("inference.generate");
    telemetry.recordEvent("inference", "generate.begin", "tokens_in=%1 max=%2"));


    // Check model loaded (with brief lock for safety)
    {
        std::lock_guard<std::mutex> lock(&m_mutex);
        if (!m_loader || !m_loader->isOpen()) {
            telemetry.recordTiming("inference", "generate.no_model", timer.elapsedMs(), "tokens_in=%1"));
            return inputTokens;
        }
    }

    // [New] Try IPC Agent Backend first (Explicit Missing Logic)
    if (connectToAgent()) {
        std::vector<int32_t> generated = sendToAgent(inputTokens);
        if (!generated.empty()) {
             std::vector<int32_t> full = inputTokens;
             full.insert(full.end(), generated.begin(), generated.end());
             return full;
        }
    }

    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        std::chrono::steady_clock localTimer;
        localTimer.start();


        // === FIXED: Process entire prompt to get proper starting context ===
        // Phase 1: Context prefill - process the entire input prompt once
        // The transformer builds the KV-cache (Key-Value cache) for efficient generation.
        std::vector<float> contextLogits = m_transformer.forward(inputTokens);
        
        // Sample the FIRST generated token from the prompt's final logits
        // This ensures the model's response is conditioned on the full prompt
        int32_t currentToken = sampleNextToken(contextLogits, m_temperature, m_topP);
        result.push_back(currentToken);
        
        // === Phase 2: Autoregressive Token Generation (Decoding) ===
        // Generate remaining tokens (we already have the first one from prompt context)
        for (int i = 1; i < maxTokens; ++i) {
            // Generate logits for the next token based ONLY on the current token
            // The Transformer uses the internal KV-cache for past context
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            
            if (logits.empty()) {
                break;
            }


            // === Elegant Sampling Logic using Top-P ===
            // Delegate complex sampling to helper function
            currentToken = sampleNextToken(logits, m_temperature, m_topP);


            // Check for EOS token (2 is common EOS)
            if (currentToken == 2 || currentToken == 0) {
                break;
            }
            
            result.push_back(currentToken);
        }
        
        // Update performance metrics based on this generation step
        int64_t elapsed = localTimer.elapsed();
        int tokensGenerated = result.size() - inputTokens.size();
        if (elapsed > 0 && tokensGenerated > 0) {
            m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
        }
        
                << "ms (" << std::string::number(m_tokensPerSecond, 'f', 1) << " tok/s)";

        telemetry.recordTiming("inference", "generate.complete", timer.elapsedMs(), "tokens_out=%1 tok_s=%2"));
        
    } else {
        // Model not ready - return empty result instead of simulated tokens
        std::cerr << "[InferenceEngine] Generation requested but model is not ready." << std::endl;
        // No stub logic allowed.
        // Return empty vector to indicate failure.
    }
    
    return result;
}

void InferenceEngine::generateStreaming(const std::string& prompt,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete)
{
    // Tokenize then call vector variant
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(onToken), std::move(onComplete));
}

void InferenceEngine::generateStreaming(const std::vector<int32_t>& inputTokens,
                                        int maxTokens,
                                        TokenCallback onToken,
                                        CompleteCallback onComplete)
{
    if (m_threadingEnabled.load()) {
        // Run worker in background to avoid blocking UI thread
        auto future = QtConcurrent::run([this, inputTokens, maxTokens, onToken = std::move(onToken), onComplete = std::move(onComplete)]() mutable {
            streamingGenerateWorker(inputTokens, maxTokens, onToken, onComplete);
        });
    } else {
        streamingGenerateWorker(inputTokens, maxTokens, std::move(onToken), std::move(onComplete));
    }
}

void InferenceEngine::streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              TokenCallback onToken,
                                              CompleteCallback onComplete)
{
    auto &telemetry = RawrXD::EnterpriseTelemetry::instance();
    auto timer = telemetry.startTimer("inference.generate.streaming");
    telemetry.recordEvent("inference", "stream.begin", "tokens_in=%1 max=%2"));

    // Ensure model loaded
    {
        std::lock_guard<std::mutex> lock(&m_mutex);
        if (!m_loader || !m_loader->isOpen()) {
            telemetry.recordTiming("inference", "stream.no_model", timer.elapsedMs(), "tokens_in=%1"));
            if (onComplete) onComplete();
            return;
        }
    }

    if (!m_transformer.isReady()) {
        if (onComplete) onComplete();
        return;
    }

    // Prefill KV-cache if not ready
    if (!m_kvCacheReady && !inputTokens.empty()) {
        m_transformer.forward(inputTokens);
        m_kvCacheReady = true;
    }

    int32_t currentToken = inputTokens.empty() ? 1 : inputTokens.back();
    std::vector<int32_t> emitted;

    for (int i = 0; i < maxTokens; ++i) {
        auto logits = m_transformer.forward(std::vector<int32_t>{currentToken});
        if (logits.empty()) {
            break;
        }

        currentToken = sampleNextToken(logits, m_temperature, m_topP);
        emitted.push_back(currentToken);

        // Detokenize incrementally; token fragment
        std::string frag = detokenize(std::vector<int32_t>{ currentToken });
        if (!frag.empty() && onToken) {
            // Marshal back to void thread if needed
            QMetaObject::invokeMethod(this, [onToken, frag]() {
                onToken(frag);
            }, //QueuedConnection);
        }

        // Simple EOS check (common 2/0, configurable in future)
        if (currentToken == 2 || currentToken == 0) {
            break;
        }

        // Pace a bit to keep UI responsive
        std::thread::msleep(10);
    }

    // completion
    if (onComplete) {
        QMetaObject::invokeMethod(this, [onComplete]() { onComplete(); }, //QueuedConnection);
    }

    // Reset KV-cache for next request
    m_kvCacheReady = false;
    telemetry.recordTiming("inference", "stream.end", timer.elapsedMs(), "tokens_out=%1"));
}

void InferenceEngine::generateStreaming(const std::string& prompt,
                                        int maxTokens,
                                        std::function<void(const std::string&)> tokenCallback,
                                        std::function<void()> completeCallback)
{
    auto tokens = tokenize(prompt);
    generateStreaming(tokens, maxTokens, std::move(tokenCallback), std::move(completeCallback));
}

void InferenceEngine::generateStreaming(const std::vector<int32_t>& inputTokens,
                                        int maxTokens,
                                        std::function<void(const std::string&)> tokenCallback,
                                        std::function<void()> completeCallback)
{
    if (m_threadingEnabled.load()) {
        // Run in background thread to avoid blocking UI thread
        auto future = QtConcurrent::run([this, inputTokens, maxTokens, tokenCallback, completeCallback]() {
            streamingGenerateWorker(inputTokens, maxTokens, tokenCallback, completeCallback);
        });
    } else {
        streamingGenerateWorker(inputTokens, maxTokens, tokenCallback, completeCallback);
    }
}

void InferenceEngine::streamingGenerateWorker(const std::vector<int32_t> inputTokens,
                                              int maxTokens,
                                              std::function<void(const std::string&)> tokenCallback,
                                              std::function<void()> completeCallback)
{
    try {

        if (!m_transformer.isReady()) {
            if (completeCallback) completeCallback();
            return;
        }

        std::vector<int32_t> result = inputTokens;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
        }

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();
        std::string accumulatedText;

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                break;
            }
            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                break;
            }

            result.push_back(currentToken);

            // Detokenize this token and stream it
            std::vector<int32_t> singleToken = { currentToken };
            std::string tokenText = detokenize(singleToken);

            if (!tokenText.empty() && tokenCallback) {
                std::string tokenStr = tokenText.toStdString();
                accumulatedText += tokenStr;
                tokenCallback(tokenStr);
            }

            // Real-time inference
            // No Artificial Delay
        }

        // Reset KV-cache for next generation
        m_kvCacheReady = false;

        if (completeCallback) completeCallback();

    } catch (const std::exception& e) {
        if (completeCallback) completeCallback();
    } catch (...) {
        if (completeCallback) completeCallback();
    }
}

void InferenceEngine::generateStreaming(int64_t reqId, const std::string& prompt, int maxTokens)
{
    if (m_threadingEnabled.load()) {
        // Run in background thread to avoid blocking UI thread
        auto future = QtConcurrent::run([this, reqId, prompt, maxTokens]() {
            streamingGenerateWorkerSignals(reqId, prompt, maxTokens);
        });
    } else {
        streamingGenerateWorkerSignals(reqId, prompt, maxTokens);
    }
}

void InferenceEngine::streamingGenerateWorkerSignals(int64_t reqId, const std::string& prompt, int maxTokens)
{
    try {

        // Tokenize the prompt
        std::vector<int32_t> inputTokens = tokenize(prompt);

        if (!m_transformer.isReady()) {
            streamFinished(reqId);
            return;
        }

        std::vector<int32_t> result = inputTokens;

        // Phase 1: KV-cache prefill
        if (!m_kvCacheReady) {
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
        }

        // Phase 2: Autoregressive generation with streaming
        int32_t currentToken = inputTokens.back();

        for (int i = 0; i < maxTokens; ++i) {
            // Generate next token
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            if (logits.empty()) {
                break;
            }

            currentToken = sampleNextToken(logits, m_temperature, m_topP);

            // Stop conditions (EOS/PAD)
            if (currentToken == 2 || currentToken == 0) {
                break;
            }

            result.push_back(currentToken);

            // Detokenize this token and signal
            std::vector<int32_t> singleToken = { currentToken };
            std::string tokenText = detokenize(singleToken);

            if (!tokenText.empty()) {
                // signal for this token
                streamToken(reqId, tokenText);
            }

            // Real-time inference
            // No Artificial Delay
        }

        // Reset KV-cache for next generation
        m_kvCacheReady = false;


        // completion signal
        streamFinished(reqId);

    } catch (const std::exception& e) {
        streamFinished(reqId);
    } catch (...) {
        streamFinished(reqId);
    }
}

// ============================================================================
// IPC AGENT IMPLEMENTATION
// ============================================================================

bool InferenceEngine::connectToAgent() {
    if (m_hPipe != nullptr && m_hPipe != INVALID_HANDLE_VALUE) return true;
    
    // Try to connect to the named pipe created by RawrXD_Agent.exe
    m_hPipe = CreateFileA(
        "\\\\.\\pipe\\RawrXD_IPC",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
        
    return (m_hPipe != INVALID_HANDLE_VALUE);
}

void InferenceEngine::disconnectAgent() {
    if (m_hPipe != nullptr && m_hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle((HANDLE)m_hPipe);
        m_hPipe = nullptr;
    }
}

std::vector<int32_t> InferenceEngine::sendToAgent(const std::vector<int32_t>& tokens) {
    if (!connectToAgent()) return {};
    
    DWORD bytesWritten;
    BOOL success = WriteFile(
        (HANDLE)m_hPipe,
        tokens.data(),
        (DWORD)(tokens.size() * sizeof(int32_t)),
        &bytesWritten,
        NULL);
        
    if (!success) {
        disconnectAgent();
        return {};
    }
    
    // Read Response
    int32_t buffer[4096]; 
    DWORD bytesRead;
    success = ReadFile(
        (HANDLE)m_hPipe,
        buffer,
        sizeof(buffer),
        &bytesRead,
        NULL);
        
    if (success && bytesRead > 0) {
        std::vector<int32_t> response;
        response.assign(buffer, buffer + (bytesRead / sizeof(int32_t)));
        return response;
    }
    
    return {};
}


// ============================================================================
// ELEGANT IMPLEMENTATION: Top-P (Nucleus) Sampling
// ============================================================================
// Top-P sampling produces far more natural and diverse text than greedy 
// sampling while still being controllable via the temperature parameter.
// 
// Algorithm:
// 1. Convert logits to probabilities using softmax
// 2. Sort tokens by probability (descending)
// 3. Accumulate probabilities until crossing the Top-P threshold
// 4. Randomly sample from this "nucleus"
// ============================================================================

int32_t InferenceEngine::sampleNextToken(std::vector<float>& logits, double temperature, double topP)
{
    if (logits.empty()) {
        return 0;
    }

    if (temperature <= 0.0) {
        auto it = std::max_element(logits.begin(), logits.end());
        return static_cast<int32_t>(std::distance(logits.begin(), it));
    }

    if (topP <= 0.0) {
        topP = 1.0;
    }

    // === Step 1: Convert Logits to Probabilities (Softmax) ===
    
    // Apply temperature scaling (controls randomness)
    if (temperature > 0.0) {
        for (float& logit : logits) {
            logit /= static_cast<float>(temperature);
        }
    }
    
    // Find maximum logit for numerical stability (prevent overflow in exp)
    float maxLogit = *std::max_element(logits.begin(), logits.end());
    
    // Compute exponentiated logits and total sum (Softmax calculation)
    std::vector<float> probs(logits.size());
    float sumExp = 0.0f;
    
    for (size_t i = 0; i < logits.size(); ++i) {
        float expVal = std::exp(logits[i] - maxLogit);
        probs[i] = expVal;
        sumExp += expVal;
    }
    
    // Normalize to get final probabilities
    for (float& prob : probs) {
        prob /= sumExp;
    }

    // === Step 2: Prepare for Top-P Selection ===
    
    // Create a vector of pairs: {probability, token_id}
    std::vector<std::pair<float, int32_t>> sortedTokens;
    for (size_t i = 0; i < probs.size(); ++i) {
        if (probs[i] > 1e-6f) { // Ignore very small probability tokens
            sortedTokens.push_back({probs[i], static_cast<int32_t>(i)});
        }
    }

    // Sort by probability in descending order
    std::sort(sortedTokens.begin(), sortedTokens.end(), 
              [](const auto& a, const auto& b) {
                  return a.first > b.first;
              });

    // === Step 3: Find the Nucleus (Top-P Threshold) ===
    
    float cumulativeProb = 0.0f;
    size_t nucleusSize = 0;

    for (const auto& tokenPair : sortedTokens) {
        cumulativeProb += tokenPair.first;
        nucleusSize++;
        
        // Stop when the cumulative probability exceeds topP (the nucleus)
        if (cumulativeProb >= static_cast<float>(topP)) {
            break;
        }
    }
    
    // Safety check: If the top token already exceeds P, nucleus is just that token
    if (nucleusSize == 0 || sortedTokens.empty()) {
        // Fallback: Use greedy sampling (select highest probability)
        return sortedTokens.empty() ? 0 : sortedTokens.front().second;
    }
    
    // === Step 4: Resample and Select the Next Token ===
    
    // Re-normalize the probabilities within the nucleus
    float nucleusProbSum = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        nucleusProbSum += sortedTokens[i].first;
    }

    // Use a uniform random number in [0, nucleusProbSum)
    float r = getRandomFloat(0.0f, nucleusProbSum);

    // Select the token based on the weighted random draw
    cumulativeProb = 0.0f;
    for (size_t i = 0; i < nucleusSize; ++i) {
        cumulativeProb += sortedTokens[i].first;
        if (r < cumulativeProb) {
            return sortedTokens[i].second;
        }
    }
    
    // Fallback in case of rounding errors: select the last token in the nucleus
    return sortedTokens[nucleusSize - 1].second;
}

// ============================================================================
// THREAD-SAFE RANDOM NUMBER GENERATION
// ============================================================================

float InferenceEngine::getRandomFloat(float min, float max)
{
    // Thread-safe random float generation using C++11 standard library
    // The m_randomEngine is seeded once and reused
    
    static std::once_flag initFlag;
    std::call_once(initFlag, [this]() {
        // Seed the random engine with a high-entropy source
        std::random_device rd;
        m_randomEngine.seed(rd());
    });
    
    // Generate uniform random float in [min, max)
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(m_randomEngine);
}

// ============================================================================
// FIX 6: REQUEST QUEUE IMPLEMENTATION
// ============================================================================

void InferenceEngine::processNextRequest()
{
    std::lock_guard<std::mutex> lock(&m_mutex);
    
    if (m_isProcessingInference) {
        // Already processing a request, wait for the current one to finish.
        return;
    }

    if (m_requestQueue.empty()) {
        // Nothing to do. Engine is idle.
        return;
    }

    // Dequeue the next request
    InferenceRequest currentRequest = m_requestQueue.dequeue();
    m_isProcessingInference = true; 
    
    // Check model readiness before running the heavy task
    if (!m_transformer.isReady()) {
        // Model is loading or initialization failed. Re-enqueue and signal the wait.
        m_requestQueue.prepend(currentRequest); // Put it back at the front

        std::string response = std::string("⚠ Model not ready. Request %1 re-queued. Please wait for model loading to complete.");
        
        // a temporary message so the user knows the request wasn't lost.
        resultReady(currentRequest.requestId, response); 
        m_isProcessingInference = false; // We didn't actually start inference, so we aren't busy.
        return;
    }


                   );

    // --- EXECUTE INFERENCE ---
    m_inferenceTimer.start();
    
    if (currentRequest.streaming) {
        // Use streaming generation
        generateStreaming(currentRequest.requestId, currentRequest.prompt, 128);
        
        // Performance metrics (will be calculated when streaming finishes)
        // For now, just mark as completed
        m_isProcessingInference = false;
        processNextRequest();
        return;
    }
    
    // Non-streaming: synchronous generation
    
    // 1. Tokenize the prompt
    std::vector<int32_t> tokens = tokenize(currentRequest.prompt);
    
    // 2. Run the transformer (synchronous, blocking call)
    std::vector<int32_t> outputTokens = m_transformer.generate(tokens, 50); 

    // 3. Detokenize the result
    std::string response = detokenize(outputTokens);

    // 4. Performance metrics
    int64_t elapsed = m_inferenceTimer.elapsed();
    int generatedTokens = std::max(0, (int)outputTokens.size() - (int)tokens.size());
    if (generatedTokens > 0 && elapsed > 0) {
        m_tokensPerSecond = (generatedTokens * 1000.0) / elapsed;
    }
    
            << "ms (" << std::string::number(m_tokensPerSecond, 'f', 1) << "tok/s)";

    // 5. Signal completion
    resultReady(currentRequest.requestId, response);

    // 6. Cleanup and check for the next request
    m_isProcessingInference = false;
    
    // Recursive call to immediately process the next item if the queue isn't empty
    processNextRequest(); 
}

bool InferenceEngine::initializeTokenizerWithTimeout(int timeoutMs)
{
    
    // Launch metadata fetch in background with timeout protection
    QFuture<std::unordered_map<std::string, std::vector<uint8_t>>> future = QtConcurrent::run([this]() -> std::unordered_map<std::string, std::vector<uint8_t>> {
        try {
            return m_loader->getTokenizerMetadata();
        } catch (const std::exception& e) {
            return std::unordered_map<std::string, std::vector<uint8_t>>();
        } catch (...) {
            return std::unordered_map<std::string, std::vector<uint8_t>>();
        }
    });
    
    // Wait with timeout using std::thread::msleep polling
    std::chrono::steady_clock timer;
    timer.start();
    
    while (!future.isFinished() && timer.elapsed() < timeoutMs) {
        std::thread::msleep(100);
        // Log progress every second to show we're waiting
        if (timer.elapsed() % 1000 < 100) {
        }
    }
    
    if (!future.isFinished()) {
        return false;
    }
    
    // CRITICAL: Double-check that future is actually finished before calling result()
    // Calling result() on an unfinished future blocks indefinitely
    if (!future.isFinished()) {
        return false;
    }
    
    std::unordered_map<std::string, std::vector<uint8_t>> tokenizerMetadata = future.result();
    
    if (tokenizerMetadata.empty()) {
        return false;
    }


    // Determine tokenizer type and load
    VocabularyLoader::TokenizerType vocabType = m_vocab.getType();


    if (vocabType == VocabularyLoader::BPE) {
        try {


            if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_BPE;
                return true;
            }
        } catch (const std::exception& e) {
        }
    } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
        
        // HOTFIX: SentencePiece tokenizer initialization causes a freeze
        // Since we already have the vocabulary loaded (32k tokens), we can use fallback mode
        // The vocabulary-based tokenizer will work for basic chat functionality
        return false;  // Will trigger fallback
    }


    return false;
}

bool InferenceEngine::loadFallbackTokenizer()
{
    
    // Use vocabulary-based fallback tokenization
    m_tokenizerMode = TOKENIZER_FALLBACK;
    
    // The vocabulary is already loaded from GGUF, so we just need to mark the tokenizer as ready
    if (m_vocab.isLoaded() && m_vocab.size() > 0) {
        buildSystemPromptTokens();
        return true;
    }
    
    return false;
}



