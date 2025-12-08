#include "inference_engine.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <numeric>
#include <mutex>

// Use shared quant utilities
#include "quant_utils.hpp"

InferenceEngine::InferenceEngine(const QString& ggufPath, QObject* parent)
    : QObject(parent), m_loader(nullptr)
{
    if (!ggufPath.isEmpty()) {
        loadModel(ggufPath);
    }
}

InferenceEngine::InferenceEngine(QObject* parent)
    : QObject(parent), m_loader(nullptr)
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

bool InferenceEngine::loadModel(const QString& path)
{
    try {
        QMutexLocker lock(&m_mutex);
        
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        
        if (path.isEmpty()) {
            qWarning() << "[InferenceEngine] Model path is empty";
            emit modelLoadedChanged(false, QString());
            return false;
        }
        
        qInfo() << "[InferenceEngine] Attempting to load model from:" << path;
        
        // Create loader with error checking
        m_loader = new GGUFLoaderQt(path);
        
        if (!m_loader->isOpen()) {
            qWarning() << "[InferenceEngine] GGUFLoader failed to open file:" << path;
            delete m_loader;
            m_loader = nullptr;
            emit modelLoadedChanged(false, QString());
            return false;
        }
        
        m_modelPath = path;
        QString modelName = extractModelName(path);
        
        // Initialize tokenizer from model
        try {
            initializeTokenizer();
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to initialize tokenizer:" << e.what();
            // Continue anyway - tokenizer is optional for basic inference
        }
        
        // Build initial quantized tensor cache
        try {
            rebuildTensorCache();
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to build tensor cache:" << e.what();
            // Continue anyway - we'll try without cache
        }
        
        // === FIX: Dynamically read model architecture from GGUF metadata ===
        // These values are now read from the actual GGUF file instead of hardcoded
        int nLayers = m_loader->getParam("n_layer", 12).toInt();
        int nEmbd = m_loader->getParam("n_embd", 768).toInt();
        int nHead = m_loader->getParam("n_head", 12).toInt();
        int nVocab = m_loader->getParam("n_vocab", 50257).toInt();

        // Log the actual parameters read from the GGUF file
        qInfo() << QString("[InferenceEngine] Detected model architecture: Layers=%1, Embedding=%2, Heads=%3, Vocab=%4")
                     .arg(nLayers).arg(nEmbd).arg(nHead).arg(nVocab);
        
        if (!m_tensorCache.isEmpty()) {
            try {
                // Convert CachedTensorData -> QByteArray for legacy loadWeights signature
                QHash<QString, QByteArray> byteArrayCache;
                for (auto it = m_tensorCache.constBegin(); it != m_tensorCache.constEnd(); ++it) {
                    byteArrayCache.insert(it.key(), it.value().data);
                }
                
                bool transformerLoaded = m_transformer.loadWeights(byteArrayCache, nLayers, nEmbd, nHead, nVocab);
                if (!transformerLoaded) {
                    qWarning() << "[InferenceEngine] Transformer weight loading failed, inference will be limited";
                } else {
                    qInfo() << "[InferenceEngine] Transformer initialized successfully with real model parameters";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Exception loading transformer weights:" << e.what();
                // Continue anyway - model may work in limited capacity
            }
        } else {
            qWarning() << "[InferenceEngine] Tensor cache is empty, transformer initialization skipped";
        }
        
        // Reset KV-cache state for new model
        m_kvCacheReady = false;
        
        qInfo() << "[InferenceEngine] Model loaded successfully:" << modelName;
        emit modelLoadedChanged(true, modelName);
        
        // FIX 6: Immediately check the queue after model load.
        processNextRequest(); 
        
        // FIX 3.2: Emit the signal to notify listeners
        emit transformerReady();
        
        return true;    } catch (const std::exception& e) {
        qCritical() << "[InferenceEngine] CRITICAL: Exception during model loading:" << e.what();
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        emit modelLoadedChanged(false, QString());
        return false;
    } catch (...) {
        qCritical() << "[InferenceEngine] CRITICAL: Unknown exception during model loading";
        if (m_loader) {
            delete m_loader;
            m_loader = nullptr;
        }
        emit modelLoadedChanged(false, QString());
        return false;
    }
}

QString InferenceEngine::processChat(const QString& prompt)
{
    // Tokenize, run a short generation, and detokenize
    auto input = tokenize(prompt);
    auto out = generate(input, 64);
    return detokenize(out);
}

QString InferenceEngine::analyzeCode(const QString& code)
{
    // Simple analysis stub leveraging existing tokenizer to avoid heavy changes
    QString analysis = QString(
        "Code Analysis:\n"
        "- Length: %1 chars\n"
        "- Lines: %2\n"
        "- Tokens: %3"
    ).arg(code.size()).arg(code.count('\n') + 1).arg(tokenize(code).size());
    return analysis;
}

bool InferenceEngine::isModelLoaded() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader && m_loader->isOpen();
}

QString InferenceEngine::modelPath() const
{
    QMutexLocker lock(&m_mutex);
    return m_modelPath;
}

QStringList InferenceEngine::tensorNames() const
{
    QMutexLocker lock(&m_mutex);
    return m_loader ? m_loader->tensorNames() : QStringList();
}

qint64 InferenceEngine::memoryUsageMB() const
{
    QMutexLocker lock(&m_mutex);
    return m_memoryUsageMB;
}

double InferenceEngine::tokensPerSecond() const
{
    QMutexLocker lock(&m_mutex);
    return m_tokensPerSecond;
}

double InferenceEngine::temperature() const
{
    QMutexLocker lock(&m_mutex);
    return m_temperature;
}

QString InferenceEngine::quantMode() const
{
    QMutexLocker lock(&m_mutex);
    return m_quantMode;
}

void InferenceEngine::request(const QString& prompt, qint64 reqId)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        qWarning() << "No model loaded for inference request" << reqId;
        emit error(reqId, "Error: No model loaded");
        return;
    }
    
    // FIX 6: Enqueue the request instead of processing immediately
    InferenceRequest request;
    request.prompt = prompt;
    request.requestId = reqId;
    m_requestQueue.enqueue(request);

    qInfo() << QString("Request %1 enqueued. Queue size: %2").arg(reqId).arg(m_requestQueue.size());

    // Attempt to start processing if the engine is not busy
    if (!m_isProcessingInference) {
        processNextRequest();
    }
}

void InferenceEngine::unloadModel()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    m_modelPath.clear();
    m_tensorCache.clear();
    
    emit modelLoadedChanged(false, QString());
}

QString InferenceEngine::extractModelName(const QString& path) const
{
    QFileInfo modelInfo(path);
    return modelInfo.fileName();
}

void InferenceEngine::setQuantMode(const QString& mode)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_quantMode == mode) return;
    
    m_quantMode = mode;
    rebuildTensorCache();
    
    emit quantChanged(mode);
}

void InferenceEngine::setLayerQuant(const QString& tensorName, const QString& quant)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_perLayerQuant.value(tensorName) == quant) return;
    
    m_perLayerQuant.insert(tensorName, quant);
    rebuildTensorCache();
    
    emit quantChanged(QString("%1->%2").arg(tensorName, quant));
}

void InferenceEngine::rebuildTensorCache()
{
    try {
        m_tensorCache.clear();
        
        if (!m_loader) {
            qWarning() << "[InferenceEngine] No GGUF loader available for tensor cache rebuild";
            return;
        }
        
        QStringList names = m_loader->tensorNames();
        qInfo() << "[InferenceEngine] Rebuilding tensor cache with" << names.size() << "tensors";
        
        for (const QString& name : names) {
            try {
                const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
                QByteArray raw = m_loader->inflateWeight(name);
                
                if (raw.isEmpty()) {
                    qDebug() << "[InferenceEngine] Empty tensor data for:" << name;
                    continue;
                }
                
                // Apply quantization safely and capture the resulting type
                try {
                    // Use the new function that returns both data and type
                    auto [quantized, resulting_type_id] = apply_quant_with_type(raw, qmode);

                    if (!quantized.isEmpty()) {
                        CachedTensorData tensorData;
                        tensorData.data = quantized;
                        tensorData.ggml_type_id = resulting_type_id;
                        m_tensorCache.insert(name, tensorData);
                    }
                } catch (const std::exception& e) {
                    qWarning() << "[InferenceEngine] Failed to quantize tensor" << name << ":" << e.what();
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Error processing tensor" << name << ":" << e.what();
            }
        }
        
        qInfo() << "[InferenceEngine] Tensor cache built with" << m_tensorCache.size() << "tensors";
        
        // Reload transformer weights if cache was rebuilt
        // FIX: Removed dangerous premature weight loading with hardcoded dimensions.
        // Weights should only be loaded in loadModel() after correct dimensions are read.
        /*
        if (!m_tensorCache.isEmpty() && m_loader) {
            try {
                m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to load weights to transformer:" << e.what();
            }
        }
        */
    } catch (const std::exception& e) {
        qCritical() << "[InferenceEngine] Critical exception in rebuildTensorCache:" << e.what();
    } catch (...) {
        qCritical() << "[InferenceEngine] Unknown exception in rebuildTensorCache";
    }
}

std::vector<int32_t> InferenceEngine::tokenize(const QString& text)
{
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                return m_bpeTokenizer.encode(text);
            }
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                return m_spTokenizer.encode(text, true, false);  // Add BOS, no EOS
            }
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            break;
    }
    
    // Fallback: Simple word-based tokenization
    // TODO: This is a placeholder - production should always use proper tokenizer
    std::vector<int32_t> tokens;
    
    // Add BOS token
    tokens.push_back(1);
    
    // Split on whitespace and punctuation
    QStringList words = text.split(QRegularExpression("[\\s,\\.!?;:]+"), Qt::SkipEmptyParts);
    
    for (const QString& word : words) {
        // Use vocabulary if available
        if (m_vocab.isLoaded()) {
            int32_t tokenId = m_vocab.getTokenId(word.toLower());
            if (tokenId >= 0) {
                tokens.push_back(tokenId);
            } else {
                // Hash unknown words
                uint32_t hash = qHash(word.toLower());
                tokens.push_back((hash % 50000) + 256);
            }
        } else {
            // Pure fallback: hash-based
            uint32_t hash = qHash(word.toLower());
            tokens.push_back((hash % 50000) + 256);
        }
    }
    
    // Add EOS token
    tokens.push_back(2);
    
    return tokens;
}

QString InferenceEngine::detokenize(const std::vector<int32_t>& tokens)
{
    // Use appropriate tokenizer based on mode
    switch (m_tokenizerMode) {
        case TOKENIZER_BPE:
            if (m_bpeTokenizer.isReady()) {
                return m_bpeTokenizer.decode(tokens);
            }
            break;
            
        case TOKENIZER_SP:
            if (m_spTokenizer.isReady()) {
                return m_spTokenizer.decode(tokens, true);  // Skip special tokens
            }
            break;
            
        case TOKENIZER_FALLBACK:
        default:
            break;
    }
    
    // Fallback: Use vocabulary or generate placeholders
    QString result;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        int32_t token = tokens[i];
        
        // Skip special tokens
        if (token == 1 || token == 2) continue;  // BOS/EOS
        
        // Use vocabulary if available
        if (m_vocab.isLoaded()) {
            VocabularyLoader::Token vocabToken = m_vocab.getToken(token);
            if (vocabToken.id >= 0) {
                result += vocabToken.text + " ";
                continue;
            }
        }
        
        // Pure fallback: placeholder
        if (token >= 256 && token < 50256) {
            result += QString("tok_%1 ").arg(token);
        } else if (token < 256) {
            result += QChar(token);
        }
    }
    
    return result.trimmed();
}

void InferenceEngine::initializeTokenizer()
{
    try {
        // Try to load vocabulary from GGUF file
        if (!m_loader) {
            qWarning() << "[InferenceEngine] No GGUF loader available, skipping tokenizer init";
            return;
        }
        
        if (!m_vocab.loadFromGGUF(m_modelPath)) {
            qWarning() << "[InferenceEngine] Failed to load vocabulary from GGUF";
            return;
        }
        
        qInfo() << "[InferenceEngine] Vocabulary loaded:" << m_vocab.size() << "tokens";
        
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        QHash<QString, QByteArray> tokenizerMetadata;
        try {
            tokenizerMetadata = m_loader->getTokenizerMetadata();
        } catch (const std::exception& e) {
            qWarning() << "[InferenceEngine] Failed to load tokenizer metadata:" << e.what();
            // Continue without metadata
        }
        
        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            try {
                // Initialize BPE tokenizer with real GGUF metadata
                if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_BPE;
                    qInfo() << "[InferenceEngine] Using BPE tokenizer (GPT-2 compatible)";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to initialize BPE tokenizer:" << e.what();
            }
        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            try {
                // Initialize SentencePiece tokenizer with real GGUF metadata
                if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                    m_tokenizerMode = TOKENIZER_SP;
                    qInfo() << "[InferenceEngine] Using SentencePiece tokenizer (LLaMA/Mistral compatible)";
                }
            } catch (const std::exception& e) {
                qWarning() << "[InferenceEngine] Failed to initialize SentencePiece tokenizer:" << e.what();
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[InferenceEngine] Critical exception in tokenizer initialization:" << e.what();
        m_tokenizerMode = TOKENIZER_FALLBACK;
    } catch (...) {
        qWarning() << "[InferenceEngine] Unknown exception in tokenizer initialization";
        m_tokenizerMode = TOKENIZER_FALLBACK;
    }
    
    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
        qInfo() << "[InferenceEngine] Using fallback word-based tokenizer (limited functionality)";
    }
}

std::vector<int32_t> InferenceEngine::generate(const std::vector<int32_t>& inputTokens, int maxTokens)
{
    QMutexLocker lock(&m_mutex);
    
    if (!isModelLoaded()) {
        qWarning() << "Cannot generate - no model loaded";
        return inputTokens;
    }
    
    std::vector<int32_t> result = inputTokens;
    
    // Check if transformer is ready
    if (m_transformer.isReady()) {
        QElapsedTimer localTimer;
        localTimer.start();
        
        // === ELEGANT FIX: Two-Phase Inference with KV-Cache ===
        // Phase 1: Context prefill - process the entire input prompt once
        // The transformer builds the KV-cache (Key-Value cache) for efficient generation.
        // Note: If transformer has decodeContext method, use it; otherwise use forward
        if (!m_kvCacheReady) {
            // Process full context to populate KV-cache
            // This is called only once per inference request
            m_transformer.forward(inputTokens);
            m_kvCacheReady = true;
            qInfo() << "KV-cache prefilled with" << inputTokens.size() << "context tokens";
        }
        
        // The last token ID becomes the starting point for autoregressive generation
        int32_t currentToken = inputTokens.back();
        
        // === Phase 2: Autoregressive Token Generation (Decoding) ===
        for (int i = 0; i < maxTokens; ++i) {
            // Generate logits for the next token based ONLY on the current token
            // The Transformer uses the internal KV-cache for past context context
            std::vector<float> logits = m_transformer.forward(std::vector<int32_t>{currentToken});
            
            if (logits.empty()) {
                qWarning() << "Transformer forward pass returned no logits";
                break;
            }
            
            // === Elegant Sampling Logic using Top-P ===
            // Delegate complex sampling to helper function
            currentToken = sampleNextToken(logits, m_temperature, m_topP);
            
            // Check for EOS token (2 is common EOS)
            if (currentToken == 2 || currentToken == 0) {
                qInfo() << "Generation stopped by EOS token";
                break;
            }
            
            result.push_back(currentToken);
        }
        
        // Update performance metrics based on this generation step
        qint64 elapsed = localTimer.elapsed();
        int tokensGenerated = result.size() - inputTokens.size();
        if (elapsed > 0 && tokensGenerated > 0) {
            m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
        }
        
        qInfo() << "Generation complete:" << tokensGenerated << "tokens in" << elapsed 
                << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << " tok/s, Top-P=" 
                << QString::number(m_topP, 'f', 2) << ")";
        
        // Reset KV-cache for next inference
        m_kvCacheReady = false;
        
    } else {
        // Fallback: Simple echo with placeholder
        qWarning() << "Transformer not ready, using placeholder generation";
        
        // Just add a few placeholder tokens
        for (int i = 0; i < std::min(maxTokens, 10); ++i) {
            result.push_back(1000 + i);  // Placeholder tokens
        }
    }
    
    return result;
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
    QMutexLocker lock(&m_mutex);
    
    if (m_isProcessingInference) {
        // Already processing a request, wait for the current one to finish.
        return;
    }

    if (m_requestQueue.isEmpty()) {
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

        QString response = QString("⚠ Model not ready. Request %1 re-queued. Please wait for model loading to complete.").arg(currentRequest.requestId);
        
        // Emit a temporary message so the user knows the request wasn't lost.
        emit resultReady(currentRequest.requestId, response); 
        m_isProcessingInference = false; // We didn't actually start inference, so we aren't busy.
        return;
    }

    qInfo() << QString("Starting inference for request %1. %2 remaining in queue.")
                   .arg(currentRequest.requestId)
                   .arg(m_requestQueue.size());

    // --- EXECUTE INFERENCE ---
    m_inferenceTimer.start();
    
    // 1. Tokenize the prompt
    std::vector<int32_t> tokens = tokenize(currentRequest.prompt);
    
    // 2. Run the transformer (synchronous, blocking call)
    std::vector<int32_t> outputTokens = m_transformer.generate(tokens, 50); 

    // 3. Detokenize the result
    QString response = detokenize(outputTokens);

    // 4. Performance metrics
    qint64 elapsed = m_inferenceTimer.elapsed();
    int generatedTokens = std::max(0, (int)outputTokens.size() - (int)tokens.size());
    if (generatedTokens > 0 && elapsed > 0) {
        m_tokensPerSecond = (generatedTokens * 1000.0) / elapsed;
    }
    
    qInfo() << "Inference completed:" << outputTokens.size() << "tokens in" << elapsed 
            << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << "tok/s)";

    // 5. Signal completion
    emit resultReady(currentRequest.requestId, response);

    // 6. Cleanup and check for the next request
    m_isProcessingInference = false;
    
    // Recursive call to immediately process the next item if the queue isn't empty
    processNextRequest(); 
}


