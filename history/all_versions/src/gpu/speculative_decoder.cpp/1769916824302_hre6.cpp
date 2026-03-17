#include "speculative_decoder.h"
#include "../gpu_masm/gpu_masm_bridge.h"
#include "../ggml_masm/ggml_masm_bridge.h"
#include "cpu_inference_engine.h"
#include "ai_model_caller.h"

SpeculativeDecoder::SpeculativeDecoder()
    : m_gpuAccelerated(false)
    , m_draftModelLoaded(false)
    , m_targetModelLoaded(false)
{
    m_draftEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    m_targetEngine = std::make_unique<RawrXD::CPUInferenceEngine>();

    // Check if GPU backend is available for speculative decoding
    if (IsBackendInitialized()) {
        m_gpuAccelerated = true;
    }
}

SpeculativeDecoder::~SpeculativeDecoder() = default;

void SpeculativeDecoder::setDraftModel(const std::string &modelPath)
{
    m_draftModelPath = modelPath;
    if (m_draftEngine->LoadModel(modelPath)) {
        m_draftModelLoaded = true;
    }
}

void SpeculativeDecoder::setTargetModel(const std::string &modelPath)
{
    m_targetModelPath = modelPath;
    if (m_targetEngine->LoadModel(modelPath)) {
        m_targetModelLoaded = true;
    }
}

std::vector<int> SpeculativeDecoder::generateTokens(const std::string &prompt, int maxTokens)
{
    // Generate draft tokens using the draft model
    std::vector<int> draftTokens = generateDraftTokens(prompt, maxTokens);
    
    // Verify draft tokens with the target model
    std::vector<int> verifiedTokens = verifyTokens(prompt, draftTokens);
    
    // Notify stats
    if (!draftTokens.empty()) {
        float rate = (float)verifiedTokens.size() / (float)draftTokens.size();
        acceptanceRateChanged(rate);
    }

    return verifiedTokens;
}

std::vector<int> SpeculativeDecoder::generateDraftTokens(const std::string &prompt, int maxTokens)
{
    if (!m_draftModelLoaded) return {};
    
    // Use member engine
    std::vector<int32_t> context = m_draftEngine->Tokenize(prompt);
    std::vector<int32_t> output = m_draftEngine->Generate(context, maxTokens);

    std::vector<int> result;
    for (auto t : output) result.push_back((int)t);
    return result;
}

std::vector<int> SpeculativeDecoder::verifyTokens(const std::string &prompt, const std::vector<int> &draftTokens)
{
    if (!m_targetModelLoaded) return draftTokens; // Fallback

    std::vector<int> verifiedTokens;
    
    // Use member engine
    std::vector<int32_t> contextIds = m_targetEngine->Tokenize(prompt);
    
    for (int token : draftTokens) {
        // Run forward pass on current context
        std::vector<int32_t> nextTokens = m_targetEngine->Generate(contextIds, 1);
        if (nextTokens.empty()) break;
        
        int bestId = nextTokens[0];
        if (bestId == token) {
            verifiedTokens.push_back(token);
            contextIds.push_back(token);
        } else {
            // Rejection! We accept the target model's true token and stop speculative batch
            verifiedTokens.push_back(bestId); 
            break;
        }
    }
    
    return verifiedTokens;
}