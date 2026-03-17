// gguf_loader_test.cpp - Comprehensive test suite for GGUF model loading
// Tests: model loading, metadata parsing, tensor cache population, agent integration

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <windows.h>

//==========================================================================
// EXTERN DECLARATIONS (MASM layer)
//==========================================================================

// Model loader interface
extern "C" int ml_masm_init(const char* model_path, int flags);
extern "C" int ml_masm_inference(const char* prompt);
extern "C" int ml_masm_get_response(char* buffer, int max_len);
extern "C" void* ml_masm_get_tensor(const char* tensor_name);
extern "C" const char* ml_masm_get_arch();
extern "C" const char* ml_masm_last_error();
extern "C" void ml_masm_free();

//==========================================================================
// STRUCTURES (matched with MASM definitions)
//==========================================================================

#pragma pack(1)

struct TENSOR_INFO {
    char name_str[64];
    uint32_t shape[4];
    uint32_t dtype;
    uint32_t strides[4];
    uint64_t data_ptr;
    uint64_t tensor_size;
    uint32_t quant_level;
    char reserved[8];
};

struct MODEL_ARCH {
    char model_name[64];
    char version[32];
    uint32_t num_layers;
    uint32_t hidden_size;
    uint32_t num_attention_heads;
    uint32_t max_seq_length;
    uint32_t vocab_size;
    uint32_t quant_level;
};

#pragma pack()

//==========================================================================
// TEST UTILITIES
//==========================================================================

class TestLogger {
public:
    static void log(const char* format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        printf("[TEST] %s\n", buffer);
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
    }
    
    static void log_error(const char* format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsprintf_s(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        printf("[ERROR] %s\n", buffer);
        OutputDebugStringA("[ERROR] ");
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
    }
    
    static void log_pass(const char* test_name) {
        printf("[PASS] %s\n", test_name);
    }
    
    static void log_fail(const char* test_name, const char* reason) {
        printf("[FAIL] %s - %s\n", test_name, reason);
    }
};

//==========================================================================
// GGUF TEST FIXTURE CREATION
//==========================================================================

// Create a minimal valid GGUF v3 file for testing
bool create_test_gguf_file(const char* path) {
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, path, "wb");
    if (err != 0 || !f) {
        TestLogger::log_error("Failed to create test GGUF file: %s", path);
        return false;
    }
    
    // GGUF v3 header
    uint32_t magic = 0x47554647;  // "GGUF"
    uint32_t version = 3;
    uint64_t tensor_count = 5;    // 5 test tensors
    uint64_t metadata_kv_count = 4; // 4 KV pairs
    
    fwrite(&magic, 4, 1, f);
    fwrite(&version, 4, 1, f);
    fwrite(&tensor_count, 8, 1, f);
    fwrite(&metadata_kv_count, 8, 1, f);
    
    // Metadata KV entries
    // KV 1: "llama.block_count" = 32 (uint32)
    uint32_t key_len = 17;
    fwrite(&key_len, 4, 1, f);
    fwrite("llama.block_count", 17, 1, f);
    uint8_t type_uint32 = 5;
    fwrite(&type_uint32, 1, 1, f);
    uint32_t value_layers = 32;
    fwrite(&value_layers, 4, 1, f);
    
    // KV 2: "llama.embedding_length" = 4096 (uint32)
    key_len = 23;
    fwrite(&key_len, 4, 1, f);
    fwrite("llama.embedding_length", 23, 1, f);
    fwrite(&type_uint32, 1, 1, f);
    uint32_t value_hidden = 4096;
    fwrite(&value_hidden, 4, 1, f);
    
    // KV 3: "llama.attention.head_count" = 32 (uint32)
    key_len = 27;
    fwrite(&key_len, 4, 1, f);
    fwrite("llama.attention.head_count", 27, 1, f);
    fwrite(&type_uint32, 1, 1, f);
    uint32_t value_heads = 32;
    fwrite(&value_heads, 4, 1, f);
    
    // KV 4: "tokenizer.ggml.vocab_size" = 32000 (uint32)
    key_len = 26;
    fwrite(&key_len, 4, 1, f);
    fwrite("tokenizer.ggml.vocab_size", 26, 1, f);
    fwrite(&type_uint32, 1, 1, f);
    uint32_t value_vocab = 32000;
    fwrite(&value_vocab, 4, 1, f);
    
    // Padding to make file valid for tensor section (at least 100 bytes)
    char padding[256] = {0};
    fwrite(padding, 256, 1, f);
    
    fclose(f);
    TestLogger::log("Created test GGUF file: %s (size: %d bytes)", path, 24 + 4 + 17 + 1 + 4 + 4 + 23 + 1 + 4 + 4 + 27 + 1 + 4 + 4 + 26 + 1 + 4 + 256);
    return true;
}

//==========================================================================
// TEST CASES
//==========================================================================

class GGUFLoaderTests {
public:
    static bool test_model_loading() {
        TestLogger::log("=== Test: Model Loading ===");
        
        char test_file[MAX_PATH];
        sprintf_s(test_file, sizeof(test_file), "%s\\test_model.gguf", getenv("TEMP"));
        
        // Create test GGUF file
        if (!create_test_gguf_file(test_file)) {
            TestLogger::log_fail("test_model_loading", "Failed to create test file");
            return false;
        }
        
        // Load model
        int result = ml_masm_init(test_file, 0);
        if (result != 1) {
            TestLogger::log_fail("test_model_loading", "ml_masm_init returned 0");
            TestLogger::log_error("Error: %s", ml_masm_last_error());
            return false;
        }
        
        TestLogger::log_pass("test_model_loading");
        return true;
    }
    
    static bool test_architecture_extraction() {
        TestLogger::log("=== Test: Architecture Metadata Extraction ===");
        
        char test_file[MAX_PATH];
        sprintf_s(test_file, sizeof(test_file), "%s\\test_model_arch.gguf", getenv("TEMP"));
        
        if (!create_test_gguf_file(test_file)) {
            TestLogger::log_fail("test_architecture_extraction", "Failed to create test file");
            return false;
        }
        
        // Load model
        int load_result = ml_masm_init(test_file, 0);
        if (load_result != 1) {
            TestLogger::log_fail("test_architecture_extraction", "Failed to load model");
            return false;
        }
        
        // Get architecture string
        const char* arch_str = ml_masm_get_arch();
        if (!arch_str || strlen(arch_str) == 0) {
            TestLogger::log_fail("test_architecture_extraction", "Architecture string is empty");
            return false;
        }
        
        TestLogger::log("Architecture string: %s", arch_str);
        
        // Verify expected content
        if (strstr(arch_str, "GGUF") == nullptr) {
            TestLogger::log_fail("test_architecture_extraction", "Architecture string missing 'GGUF'");
            return false;
        }
        
        TestLogger::log_pass("test_architecture_extraction");
        ml_masm_free();
        return true;
    }
    
    static bool test_tensor_cache_population() {
        TestLogger::log("=== Test: Tensor Cache Population ===");
        
        char test_file[MAX_PATH];
        sprintf_s(test_file, sizeof(test_file), "%s\\test_model_tensors.gguf", getenv("TEMP"));
        
        if (!create_test_gguf_file(test_file)) {
            TestLogger::log_fail("test_tensor_cache_population", "Failed to create test file");
            return false;
        }
        
        // Load model
        int load_result = ml_masm_init(test_file, 0);
        if (load_result != 1) {
            TestLogger::log_fail("test_tensor_cache_population", "Failed to load model");
            return false;
        }
        
        // Lookup tensor (expected: "tensor_0" from population)
        void* tensor_ptr = ml_masm_get_tensor("tensor_0");
        if (!tensor_ptr) {
            TestLogger::log_fail("test_tensor_cache_population", "Failed to retrieve tensor_0");
            return false;
        }
        
        TENSOR_INFO* tensor = (TENSOR_INFO*)tensor_ptr;
        TestLogger::log("Found tensor: %s", tensor->name_str);
        
        // Verify tensor structure
        if (strlen(tensor->name_str) == 0) {
            TestLogger::log_fail("test_tensor_cache_population", "Tensor name is empty");
            return false;
        }
        
        TestLogger::log("Tensor size: %llu bytes", tensor->tensor_size);
        TestLogger::log("Tensor dtype: %d", tensor->dtype);
        
        TestLogger::log_pass("test_tensor_cache_population");
        ml_masm_free();
        return true;
    }
    
    static bool test_multiple_tensor_lookups() {
        TestLogger::log("=== Test: Multiple Tensor Lookups ===");
        
        char test_file[MAX_PATH];
        sprintf_s(test_file, sizeof(test_file), "%s\\test_model_multi.gguf", getenv("TEMP"));
        
        if (!create_test_gguf_file(test_file)) {
            TestLogger::log_fail("test_multiple_tensor_lookups", "Failed to create test file");
            return false;
        }
        
        // Load model
        int load_result = ml_masm_init(test_file, 0);
        if (load_result != 1) {
            TestLogger::log_fail("test_multiple_tensor_lookups", "Failed to load model");
            return false;
        }
        
        // Test multiple lookups
        const char* tensor_names[] = {"tensor_0", "tensor_1", "tensor_2", "tensor_3", "tensor_4"};
        int found_count = 0;
        
        for (int i = 0; i < 5; i++) {
            void* tensor_ptr = ml_masm_get_tensor(tensor_names[i]);
            if (tensor_ptr) {
                found_count++;
                TENSOR_INFO* tensor = (TENSOR_INFO*)tensor_ptr;
                TestLogger::log("Tensor %d: %s (found)", i, tensor->name_str);
            } else {
                TestLogger::log("Tensor %d: %s (NOT found)", i, tensor_names[i]);
            }
        }
        
        if (found_count < 5) {
            TestLogger::log_fail("test_multiple_tensor_lookups", 
                               "Only found %d/%d tensors", found_count, 5);
            ml_masm_free();
            return false;
        }
        
        TestLogger::log_pass("test_multiple_tensor_lookups");
        ml_masm_free();
        return true;
    }
    
    static bool test_error_handling_invalid_file() {
        TestLogger::log("=== Test: Error Handling (Invalid File) ===");
        
        int result = ml_masm_init("C:\\nonexistent\\path\\model.gguf", 0);
        if (result != 0) {
            TestLogger::log_fail("test_error_handling_invalid_file", 
                               "Should fail for nonexistent file");
            return false;
        }
        
        const char* error = ml_masm_last_error();
        if (!error || strlen(error) == 0) {
            TestLogger::log_fail("test_error_handling_invalid_file", 
                               "Error message should be set");
            return false;
        }
        
        TestLogger::log("Error message: %s", error);
        TestLogger::log_pass("test_error_handling_invalid_file");
        return true;
    }
    
    static bool test_inference_with_loaded_model() {
        TestLogger::log("=== Test: Inference with Loaded Model ===");
        
        char test_file[MAX_PATH];
        sprintf_s(test_file, sizeof(test_file), "%s\\test_model_inference.gguf", getenv("TEMP"));
        
        if (!create_test_gguf_file(test_file)) {
            TestLogger::log_fail("test_inference_with_loaded_model", "Failed to create test file");
            return false;
        }
        
        // Load model
        int load_result = ml_masm_init(test_file, 0);
        if (load_result != 1) {
            TestLogger::log_fail("test_inference_with_loaded_model", "Failed to load model");
            return false;
        }
        
        // Run inference
        int infer_result = ml_masm_inference("What is 2+2?");
        if (infer_result != 1) {
            TestLogger::log_fail("test_inference_with_loaded_model", "Inference failed");
            ml_masm_free();
            return false;
        }
        
        // Get response
        char response[1024] = {0};
        ml_masm_get_response(response, sizeof(response));
        TestLogger::log("Inference response: %s", response);
        
        TestLogger::log_pass("test_inference_with_loaded_model");
        ml_masm_free();
        return true;
    }
    
    static bool run_all_tests() {
        TestLogger::log("\n");
        TestLogger::log("========================================");
        TestLogger::log("  GGUF Model Loader Test Suite");
        TestLogger::log("========================================\n");
        
        int passed = 0;
        int total = 0;
        
        // Test 1: Model Loading
        total++;
        if (test_model_loading()) passed++;
        TestLogger::log("");
        
        // Test 2: Architecture Extraction
        total++;
        if (test_architecture_extraction()) passed++;
        TestLogger::log("");
        
        // Test 3: Tensor Cache Population
        total++;
        if (test_tensor_cache_population()) passed++;
        TestLogger::log("");
        
        // Test 4: Multiple Tensor Lookups
        total++;
        if (test_multiple_tensor_lookups()) passed++;
        TestLogger::log("");
        
        // Test 5: Error Handling
        total++;
        if (test_error_handling_invalid_file()) passed++;
        TestLogger::log("");
        
        // Test 6: Inference with Loaded Model
        total++;
        if (test_inference_with_loaded_model()) passed++;
        TestLogger::log("");
        
        // Summary
        TestLogger::log("========================================");
        TestLogger::log("  RESULTS: %d/%d tests passed", passed, total);
        TestLogger::log("========================================\n");
        
        return passed == total;
    }
};

//==========================================================================
// MAIN ENTRY POINT
//==========================================================================

int main(int argc, char* argv[]) {
    printf("GGUF Model Loader Test Suite\n");
    printf("Testing GGUF v3 parsing, metadata extraction, tensor cache, and inference\n\n");
    
    bool all_passed = GGUFLoaderTests::run_all_tests();
    
    return all_passed ? 0 : 1;
}
