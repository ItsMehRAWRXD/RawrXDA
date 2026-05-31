/**
 * @file SovereignASM.h
 * @brief C++ Bridge Interface for RawrXD Sovereign ASM Kernels
 * @version 1.0.0
 * 
 * This header provides the C++ interface to the sovereign ASM kernels.
 * All functions are extern "C" to avoid name mangling for ASM linkage.
 */

#pragma once

#include <cstdint>
#include <cstddef>

// Platform detection
#ifdef _WIN32
    #define SOVEREIGN_API extern "C" __declspec(dllexport)
#else
    #define SOVEREIGN_API extern "C" __attribute__((visibility("default")))
#endif

namespace RawrXD::Sovereign {

//=============================================================================
// Type Definitions
//=============================================================================

using TokenId = int32_t;
using VocabSize = int32_t;
using SeqLength = int32_t;
using HeadDim = int32_t;

/**
 * @brief Quantization types for matrix multiplication
 */
enum class QuantType : int32_t {
    FP32 = 0,       // Full precision
    Q4_0 = 1,       // 4-bit quantization (block size 32)
    Q5_0 = 2,       // 5-bit quantization
    Q8_0 = 3,       // 8-bit quantization (block size 32)
    Q4_K = 4,       // K-quant 4-bit
    Q5_K = 5,       // K-quant 5-bit
    Q6_K = 6        // K-quant 6-bit
};

/**
 * @brief Activation function types
 */
enum class ActivationType : int32_t {
    SILU = 0,       // SiLU/Swish
    GELU = 1,       // Gaussian Error Linear Unit
    RELU = 2,       // Rectified Linear Unit
    TANH = 3,       // Hyperbolic tangent
    SIGMOID = 4     // Logistic sigmoid
};

/**
 * @brief Sampling strategy types
 */
enum class SampleType : int32_t {
    GREEDY = 0,     // Argmax
    TOP_K = 1,      // Top-k sampling
    TOP_P = 2,      // Nucleus sampling
    TEMPERATURE = 3 // Temperature scaling
};

/**
 * @brief Context structure for inference
 * Mirrors the ASM context layout
 */
struct alignas(64) InferenceContext {
    // Position tracking
    int32_t position;           // Current sequence position
    int32_t max_seq_len;      // Maximum sequence length
    
    // Model dimensions
    int32_t embedding_dim;    // Model dimension (e.g., 4096)
    int32_t num_layers;       // Number of transformer layers
    int32_t num_heads;        // Number of attention heads
    int32_t head_dim;         // Dimension per head
    int32_t vocab_size;       // Vocabulary size
    
    // KV cache
    void* kv_cache;           // Pointer to KV cache buffer
    int32_t kv_cache_pos;     // Current position in KV cache
    
    // Sampling parameters
    float temperature;        // Sampling temperature
    int32_t top_k;            // Top-k value
    float top_p;              // Top-p value
    SampleType sample_type;   // Sampling strategy
    
    // Status
    int32_t error_code;       // Last error
    int32_t flags;            // Status flags
};

//=============================================================================
// Matrix Multiplication Kernels
//=============================================================================

namespace MatMul {

/**
 * @brief Standard FP32 matrix multiplication: C = A @ B
 * 
 * @param A Left matrix [M x K]
 * @param B Right matrix [K x N]
 * @param C Output matrix [M x N]
 * @param M Rows in A
 * @param N Columns in B
 * @param K Inner dimension
 * @param lda Leading dimension of A
 * @param ldb Leading dimension of B
 * @param ldc Leading dimension of C
 * @return int32_t 0 on success, error code on failure
 */
SOVEREIGN_API int32_t SovereignMatMul_FP32(
    const float* A,
    const float* B,
    float* C,
    int32_t M,
    int32_t N,
    int32_t K,
    int32_t lda = 0,
    int32_t ldb = 0,
    int32_t ldc = 0
);

/**
 * @brief Q4_0 quantized matrix multiplication
 * 
 * @param A Left matrix [M x K] (FP32)
 * @param B Right matrix [K x N] (Q4_0 quantized)
 * @param C Output matrix [M x N] (FP32)
 * @param scales Quantization scales
 * @param M Rows in A
 * @param N Columns in B
 * @param K Inner dimension
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignMatMul_Q4_0(
    const float* A,
    const void* B,          // Q4_0 packed data
    float* C,
    const float* scales,
    int32_t M,
    int32_t N,
    int32_t K
);

/**
 * @brief Q8_0 quantized matrix multiplication
 */
SOVEREIGN_API int32_t SovereignMatMul_Q8_0(
    const float* A,
    const void* B,          // Q8_0 packed data
    float* C,
    const float* scales,
    int32_t M,
    int32_t N,
    int32_t K
);

/**
 * @brief Batched matrix multiplication
 */
SOVEREIGN_API int32_t SovereignMatMul_Batch(
    const float* const* A,
    const float* const* B,
    float** C,
    int32_t batch_size,
    int32_t M,
    int32_t N,
    int32_t K
);

/**
 * @brief Matrix multiplication with transposed B: C = A @ B^T
 */
SOVEREIGN_API int32_t SovereignMatMul_TransposeB(
    const float* A,
    const float* B,
    float* C,
    int32_t M,
    int32_t N,
    int32_t K
);

} // namespace MatMul

//=============================================================================
// Attention Kernels
//=============================================================================

namespace Attention {

/**
 * @brief Numerically stable softmax with temperature
 * 
 * @param input Input buffer [N]
 * @param output Output buffer [N]
 * @param N Length
 * @param temperature Temperature parameter (1.0 = standard softmax)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignAttention_Softmax(
    const float* input,
    float* output,
    int32_t N,
    float temperature = 1.0f
);

/**
 * @brief Standard attention forward pass: output = softmax(Q @ K^T / sqrt(d)) @ V
 * 
 * @param Q Query matrix [M x D]
 * @param K Key matrix [N x D]
 * @param V Value matrix [N x D]
 * @param output Output matrix [M x D]
 * @param M Query sequence length
 * @param N Key/Value sequence length
 * @param D Head dimension
 * @param scale Attention scale (typically 1/sqrt(D))
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignAttention_Forward(
    const float* Q,
    const float* K,
    const float* V,
    float* output,
    int32_t M,
    int32_t N,
    int32_t D,
    float scale
);

/**
 * @brief Flash Attention (memory-efficient)
 * 
 * Implements tiling to reduce memory bandwidth for long sequences.
 */
SOVEREIGN_API int32_t SovereignAttention_Flash(
    const float* Q,
    const float* K,
    const float* V,
    float* output,
    int32_t M,
    int32_t N,
    int32_t D,
    float scale
);

/**
 * @brief Apply scaling and optional mask to attention scores
 * 
 * @param scores Attention scores buffer [N]
 * @param mask Optional mask buffer [N] (NULL for no masking)
 * @param N Length
 * @param scale Scale factor
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignAttention_ScaleMask(
    float* scores,
    const float* mask,
    int32_t N,
    float scale
);

/**
 * @brief Apply causal (autoregressive) mask to attention scores
 * 
 * Sets scores[i,j] = -inf for j > i (prevents attending to future tokens)
 * 
 * @param scores Score matrix [M x N]
 * @param M Query length
 * @param N Key length
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignAttention_CausalMask(
    float* scores,
    int32_t M,
    int32_t N
);

/**
 * @brief Update KV cache with new token
 * 
 * @param kv_cache KV cache buffer
 * @param new_K New key values [head_dim]
 * @param new_V New value values [head_dim]
 * @param position Position to update
 * @param head_dim Head dimension
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignAttention_KVCacheUpdate(
    void* kv_cache,
    const float* new_K,
    const float* new_V,
    int32_t position,
    int32_t head_dim
);

} // namespace Attention

//=============================================================================
// Tokenizer Kernels
//=============================================================================

namespace Tokenizer {

/**
 * @brief Tokenizer initialization structure
 */
struct Vocabulary {
    const char* tokens;         // Concatenated token strings
    const int32_t* token_lens;  // Length of each token
    const uint32_t* token_ids;  // Token IDs
    int32_t vocab_size;         // Number of tokens
    int32_t max_token_len;      // Maximum token length
};

struct MergeRule {
    uint32_t first;     // First token ID
    uint32_t second;    // Second token ID
    uint32_t result;    // Merged token ID
    int32_t priority;   // Merge priority
};

/**
 * @brief Initialize tokenizer with vocabulary
 * 
 * @param vocab Vocabulary data
 * @param merges Merge rules
 * @param num_merges Number of merge rules
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignTokenizer_Init(
    const Vocabulary* vocab,
    const MergeRule* merges,
    int32_t num_merges
);

/**
 * @brief Encode text to token IDs
 * 
 * @param text Input text
 * @param text_len Text length
 * @param output Output token buffer
 * @param output_size Output buffer size
 * @param num_tokens Number of tokens produced (output)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignTokenizer_Encode(
    const char* text,
    int32_t text_len,
    TokenId* output,
    int32_t output_size,
    int32_t* num_tokens
);

/**
 * @brief Decode token IDs to text
 * 
 * @param tokens Token IDs
 * @param num_tokens Number of tokens
 * @param output Output text buffer
 * @param output_size Output buffer size
 * @param bytes_written Bytes written (output)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignTokenizer_Decode(
    const TokenId* tokens,
    int32_t num_tokens,
    char* output,
    int32_t output_size,
    int32_t* bytes_written
);

/**
 * @brief Core BPE merge loop
 * 
 * @param tokens Token buffer (modified in-place)
 * @param num_tokens Number of tokens (input/output)
 * @return int32_t Final token count
 */
SOVEREIGN_API int32_t SovereignTokenizer_MergeLoop(
    TokenId* tokens,
    int32_t num_tokens
);

/**
 * @brief Load vocabulary from GGUF file
 * 
 * @param filepath GGUF file path
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignTokenizer_LoadVocab(
    const char* filepath
);

} // namespace Tokenizer

//=============================================================================
// Forward Pass Kernels
//=============================================================================

namespace Forward {

/**
 * @brief Single token inference step (REPLACES STUB!)
 * 
 * This is the main entry point for token generation.
 * Replaces the previous pass-through implementation.
 * 
 * @param ctx Inference context
 * @param input_token Input token ID
 * @param output_token Output token ID (result)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SpecEngine_Infer_Single(
    InferenceContext* ctx,
    TokenId input_token,
    TokenId* output_token
);

/**
 * @brief Complete sequence inference
 * 
 * @param ctx Inference context
 * @param input_tokens Input token array
 * @param num_input_tokens Number of input tokens
 * @param output_tokens Output token buffer
 * @param max_output_tokens Maximum output tokens
 * @param num_generated Number of tokens generated (output)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t StandaloneEngine_Infer(
    InferenceContext* ctx,
    const TokenId* input_tokens,
    int32_t num_input_tokens,
    TokenId* output_tokens,
    int32_t max_output_tokens,
    int32_t* num_generated
);

/**
 * @brief Token embedding lookup
 * 
 * @param output Output embedding [embedding_dim]
 * @param token_id Token ID
 * @param position Position in sequence
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_Embedding(
    float* output,
    TokenId token_id,
    int32_t position
);

/**
 * @brief Single transformer block
 * 
 * @param buffer Input/output buffer [embedding_dim]
 * @param layer_index Layer index
 * @param position Sequence position
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_TransformerBlock(
    float* buffer,
    int32_t layer_index,
    int32_t position
);

/**
 * @brief Multi-head self attention
 * 
 * @param buffer Input/output buffer
 * @param layer_index Layer index
 * @param position Sequence position
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_SelfAttention(
    float* buffer,
    int32_t layer_index,
    int32_t position
);

/**
 * @brief Feed-forward network (SwiGLU)
 * 
 * @param buffer Input/output buffer
 * @param layer_index Layer index
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_FFN(
    float* buffer,
    int32_t layer_index
);

/**
 * @brief Language model head projection
 * 
 * @param embedding Input embedding [embedding_dim]
 * @param logits Output logits [vocab_size]
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_LMHead(
    const float* embedding,
    float* logits
);

/**
 * @brief Sample from logits
 * 
 * @param logits Logit buffer [vocab_size]
 * @param vocab_size Vocabulary size
 * @param output_token Output token (result)
 * @param sample_type Sampling strategy
 * @param temperature Temperature (for TEMPERATURE sampling)
 * @param top_k Top-k value (for TOP_K sampling)
 * @param top_p Top-p value (for TOP_P sampling)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t SovereignForward_Sample(
    const float* logits,
    int32_t vocab_size,
    TokenId* output_token,
    SampleType sample_type = SampleType::GREEDY,
    float temperature = 1.0f,
    int32_t top_k = 50,
    float top_p = 0.9f
);

} // namespace Forward

//=============================================================================
// Helper Kernels
//=============================================================================

namespace Kernels {

/**
 * @brief RMS Normalization
 * 
 * @param buffer Input/output buffer [N]
 * @param N Length (default: embedding_dim)
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_RMSNorm(
    float* buffer,
    int32_t N = 0
);

/**
 * @brief Final RMS Normalization
 */
SOVEREIGN_API int32_t Kernel_RMSNorm_Final(
    float* buffer,
    int32_t N = 0
);

/**
 * @brief Layer Normalization
 * 
 * @param buffer Input/output buffer
 * @param gamma Scale parameters
 * @param beta Shift parameters
 * @param N Length
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_LayerNorm(
    float* buffer,
    const float* gamma,
    const float* beta,
    int32_t N
);

/**
 * @brief Residual connection: buffer += residual
 * 
 * @param buffer Input/output buffer
 * @param residual Residual buffer
 * @param N Length
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_ResidualAdd(
    float* buffer,
    const float* residual,
    int32_t N = 0
);

/**
 * @brief Linear projection: output = input @ weights
 * 
 * @param output Output buffer
 * @param input Input buffer
 * @param weights Weight matrix
 * @param input_dim Input dimension
 * @param output_dim Output dimension
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_LinearProject(
    float* output,
    const float* input,
    const float* weights,
    int32_t input_dim,
    int32_t output_dim
);

/**
 * @brief SiLU (Swish) activation
 * 
 * @param buffer Input/output buffer
 * @param N Length
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_SiLU(
    float* buffer,
    int32_t N = 0
);

/**
 * @brief GELU activation
 */
SOVEREIGN_API int32_t Kernel_GELU(
    float* buffer,
    int32_t N = 0
);

/**
 * @brief ReLU activation
 */
SOVEREIGN_API int32_t Kernel_ReLU(
    float* buffer,
    int32_t N = 0
);

/**
 * @brief Element-wise multiplication
 * 
 * @param buffer Input/output buffer
 * @param multiplier Multiplier buffer
 * @param N Length
 * @return int32_t 0 on success
 */
SOVEREIGN_API int32_t Kernel_ElementMul(
    float* buffer,
    const float* multiplier,
    int32_t N = 0
);

/**
 * @brief Element-wise addition
 */
SOVEREIGN_API int32_t Kernel_ElementAdd(
    float* buffer,
    const float* addend,
    int32_t N = 0
);

} // namespace Kernels

//=============================================================================
// Utility Functions
//=============================================================================

namespace Utils {

/**
 * @brief Get kernel version
 * @return Version string
 */
SOVEREIGN_API const char* Sovereign_GetVersion();

/**
 * @brief Check if AVX2 is supported
 * @return true if AVX2 available
 */
SOVEREIGN_API bool Sovereign_HasAVX2();

/**
 * @brief Check if AVX512 is supported
 * @return true if AVX512 available
 */
SOVEREIGN_API bool Sovereign_HasAVX512();

/**
 * @brief Get last error message
 * @return Error string
 */
SOVEREIGN_API const char* Sovereign_GetLastError();

/**
 * @brief Initialize kernel library
 * @return 0 on success
 */
SOVEREIGN_API int32_t Sovereign_Init();

/**
 * @brief Cleanup kernel library
 */
SOVEREIGN_API void Sovereign_Cleanup();

} // namespace Utils

} // namespace RawrXD::Sovereign

// C-compatible aliases for direct ASM linkage
extern "C" {
    // MatMul
    int32_t SovereignMatMul_FP32(const float* A, const float* B, float* C,
                                  int32_t M, int32_t N, int32_t K);
    int32_t SovereignMatMul_Q4_0(const float* A, const void* B, float* C,
                                  const float* scales, int32_t M, int32_t N, int32_t K);
    int32_t SovereignMatMul_Q8_0(const float* A, const void* B, float* C,
                                  const float* scales, int32_t M, int32_t N, int32_t K);
    
    // Attention
    int32_t SovereignAttention_Softmax(const float* input, float* output,
                                        int32_t N, float temperature);
    int32_t SovereignAttention_Forward(const float* Q, const float* K, const float* V,
                                        float* output, int32_t M, int32_t N, int32_t D,
                                        float scale);
    
    // Tokenizer
    int32_t SovereignTokenizer_Encode(const char* text, int32_t text_len,
                                       int32_t* output, int32_t output_size);
    int32_t SovereignTokenizer_Decode(const int32_t* tokens, int32_t num_tokens,
                                       char* output, int32_t output_size);
    
    // Forward Pass
    int32_t SpecEngine_Infer_Single(void* ctx, int32_t input_token, int32_t* output_token);
    int32_t StandaloneEngine_Infer(void* ctx, const int32_t* input_tokens,
                                    int32_t num_input_tokens, int32_t* output_tokens,
                                    int32_t max_output_tokens);
}
