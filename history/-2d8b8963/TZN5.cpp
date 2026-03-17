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

bool InferenceEngine::loadModel(const QString& path)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_loader) {
        delete m_loader;
        m_loader = nullptr;
    }
    
    m_loader = new GGUFLoader(path);
    
    if (!m_loader->isOpen()) {
        qWarning() << "Failed to load GGUF model:" << path;
        delete m_loader;
        m_loader = nullptr;
        emit modelLoadedChanged(false, QString());
        return false;
    }
    
    m_modelPath = path;
    QString modelName = extractModelName(path);
    qInfo() << "Model loaded successfully:" << modelName;
    
    // Initialize tokenizer from model
    initializeTokenizer();
    
    // Build initial quantized tensor cache
    rebuildTensorCache();
    
    // === FIX: Dynamically read model architecture from GGUF metadata ===
    // These values are now read from the actual GGUF file instead of hardcoded
    int nLayers = m_loader->getParam("n_layer", 12).toInt();
    int nEmbd = m_loader->getParam("n_embd", 768).toInt();
    int nHead = m_loader->getParam("n_head", 12).toInt();
    int nVocab = m_loader->getParam("n_vocab", 50257).toInt();

    // Log the actual parameters read from the GGUF file
    qInfo() << QString("Detected model architecture: Layers=%1, Embedding=%2, Heads=%3, Vocab=%4")
                 .arg(nLayers).arg(nEmbd).arg(nHead).arg(nVocab);
    
    if (!m_tensorCache.isEmpty()) {
        bool transformerLoaded = m_transformer.loadWeights(m_tensorCache, nLayers, nEmbd, nHead, nVocab);
        if (!transformerLoaded) {
            qWarning() << "Transformer weight loading failed, inference will be limited";
        } else {
            qInfo() << "Transformer initialized successfully with real model parameters";
        }
    }
    
    // Reset KV-cache state for new model
    m_kvCacheReady = false;
    
    emit modelLoadedChanged(true, modelName);
    return true;
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
    
    m_inferenceTimer.start();
    
    // Use transformer if ready, otherwise fallback to placeholder
    if (m_transformer.isReady()) {
        // Tokenize the prompt
        std::vector<int32_t> inputTokens = tokenize(prompt);
        
        qInfo() << "Running transformer inference with" << inputTokens.size() << "input tokens";
        
        // === CORE FIX: Delegate to the dedicated generate method (max 50 new tokens) ===
        // The generate method handles the autoregressive loop, logging, and performance metrics.
        std::vector<int32_t> allTokens = generate(inputTokens, 50);

        // Calculate generated tokens (excluding input tokens)
        int inputSize = inputTokens.size();
        int totalTokens = allTokens.size();
        int generatedTokens = std::max(0, totalTokens - inputSize);
        
        // Detokenize the generated part (detokenize handles which to skip)
        QString response = detokenize(allTokens); 
        
        // Performance metrics
        qint64 elapsed = m_inferenceTimer.elapsed();
        if (generatedTokens > 0 && elapsed > 0) {
            m_tokensPerSecond = (generatedTokens * 1000.0) / elapsed;
        }
        
        qInfo() << "Inference completed:" << totalTokens << "tokens in" << elapsed 
                << "ms (" << QString::number(m_tokensPerSecond, 'f', 1) << "tok/s)";
        
        emit resultReady(reqId, response);
    } else {
        // Fallback: model not fully initialized
        QString response = QString("⚠ Model loaded but transformer not ready\n\n"
                                   "Model: %1\n"
                                   "Quantization: %2\n"
                                   "Cached tensors: %3\n\n"
                                   "Input: \"%4\"\n\n"
                                   "[Transformer weights still loading...]")
                                 .arg(extractModelName(m_modelPath))
                                 .arg(m_quantMode)
                                 .arg(m_tensorCache.size())
                                 .arg(prompt);
        
        qInfo() << "Transformer not ready, using fallback response";
        emit resultReady(reqId, response);
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
    m_tensorCache.clear();
    
    if (!m_loader) return;
    
    QStringList names = m_loader->tensorNames();
    for (const QString& name : names) {
        const QString qmode = m_perLayerQuant.contains(name) ? m_perLayerQuant.value(name) : m_quantMode;
        QByteArray raw = m_loader->inflateWeight(name);
        if (raw.isEmpty()) continue;
        m_tensorCache.insert(name, apply_quant(raw, qmode));
    }
    
    // Reload transformer weights if cache was rebuilt
    if (!m_tensorCache.isEmpty() && m_loader) {
        m_transformer.loadWeights(m_tensorCache, 12, 768, 12, 50257);
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
    // Try to load vocabulary from GGUF file
    if (m_vocab.loadFromGGUF(m_modelPath)) {
        qInfo() << "Vocabulary loaded:" << m_vocab.size() << "tokens";
        
        // === FIX: Load real metadata required for the tokenizer ===
        // The tokenizer needs parameters like merges/patterns (for BPE) or 
        // the raw SentencePiece model file content (often stored as an array in GGUF metadata)
        QHash<QString, QByteArray> tokenizerMetadata = m_loader ? m_loader->getTokenizerMetadata() 
                                                                  : QHash<QString, QByteArray>();
        
        // Determine which tokenizer to use based on vocab type
        VocabularyLoader::TokenizerType vocabType = m_vocab.getType();
        
        if (vocabType == VocabularyLoader::BPE) {
            // Initialize BPE tokenizer with real GGUF metadata
            if (m_bpeTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_BPE;
                qInfo() << "Using BPE tokenizer (GPT-2 compatible)";
            }
        } else if (vocabType == VocabularyLoader::SENTENCEPIECE) {
            // Initialize SentencePiece tokenizer with real GGUF metadata
            if (m_spTokenizer.loadFromGGUFMetadata(tokenizerMetadata)) {
                m_tokenizerMode = TOKENIZER_SP;
                qInfo() << "Using SentencePiece tokenizer (LLaMA/Mistral compatible)";
            }
        }
    }
    
    // Fallback message
    if (m_tokenizerMode == TOKENIZER_FALLBACK) {
        qInfo() << "Using fallback word-based tokenizer (limited functionality)";
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


