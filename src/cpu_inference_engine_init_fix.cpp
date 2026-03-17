// cpu_inference_engine_init_fix.cpp
// Patch to add to LoadModel to ensure metadata is properly initialized

// This code should be inserted in CPUInferenceEngine::LoadModel after loader.Open() succeeds

// =======================================================================================
// CRITICAL: Initialize model dimensions from GGUF metadata
// Without this, m_numLayers/m_embeddingDim/m_vocabSize stay at 0, causing vector overflow
// =======================================================================================

// Get GGUF header and validate
const RawrXD::GGUFHeader header = loader.GetHeader();
if (header.tensor_count == 0) {
    fprintf(stderr, "[LoadModel] ERROR: GGUF has 0 tensors\n");
    return false;
}

// Extract metadata - try multiple naming conventions
auto metadata = loader.GetMetadata();  // or loader.GetAllMetadata()

// Helper to find metadata key (tries multiple aliases)
auto findMetaInt = [&metadata](std::initializer_list<const char*> keys, int defaultVal) -> int {
    for (const char* key : keys) {
        if (metadata.count(key)) {
            try {
                return static_cast<int>(metadata.at(key).GetInt());  // or GetI32() depending on API
            } catch (...) {}
        }
    }
    return defaultVal;
};

// Initialize dimensions from metadata (try all common GGUF key names)
m_numLayers = findMetaInt({
    "llama.block_count",
    "block_count",
    "n_layer",
    "num_hidden_layers",
    "transformer.n_layers"
}, layer_limit);  // Use CLI override if metadata missing

m_embeddingDim = findMetaInt({
    "llama.embedding_length",
    "embedding_length",
    "n_embd",
    "hidden_size",
    "d_model"
}, 4096);  // Common default

m_vocabSize = findMetaInt({
    "tokenizer.ggml.tokens.length",
    "vocab_size",
    "n_vocab"
}, 32000);  // Common default

m_numHeads = findMetaInt({
    "llama.attention.head_count",
    "n_head",
    "num_attention_heads"
}, 32);  // Common default

// ==== CRITICAL VALIDATION: Detect uninitialized/corrupted values ====
fprintf(stderr, "[LoadModel] Metadata parsed: layers=%d embed=%d vocab=%d heads=%d\n",
        m_numLayers, m_embeddingDim, m_vocabSize, m_numHeads);

// Sanity check for 0xCDCDCDCD pattern (uninitialized memory)
if (m_numLayers < 0 || m_numLayers > 512) {
    fprintf(stderr, "[LoadModel] ERROR: Invalid layers=%d (0x%08X), forcing to %d\n",
            m_numLayers, static_cast<unsigned int>(m_numLayers), layer_limit);
    m_numLayers = layer_limit;
}

if (m_embeddingDim < 0 || m_embeddingDim > 32768) {
    fprintf(stderr, "[LoadModel] ERROR: Invalid embed=%d (0x%08X), forcing to 4096\n",
            m_embeddingDim, static_cast<unsigned int>(m_embeddingDim));
    m_embeddingDim = 4096;
}

if (m_vocabSize <  0 || m_vocabSize > 200000) {
    fprintf(stderr, "[LoadModel] ERROR: Invalid vocab=%d (0x%08X), forcing to 32000\n",
            m_vocabSize, static_cast<unsigned int>(m_vocabSize));
    m_vocabSize = 32000;
}

// Final validation before marking model as loaded
if (m_numLayers <= 0 || m_embeddingDim <= 0 || m_vocabSize <= 0) {
    fprintf(stderr, "[LoadModel] FATAL: Model dimensions invalid after sanitization\n");
    return false;
}

fprintf(stderr, "[LoadModel] Final dimensions: layers=%d embed=%d vocab=%d\n",
        m_numLayers, m_embeddingDim, m_vocabSize);

// Mark model as loaded ONLY after successful initialization
m_modelLoaded = true;
