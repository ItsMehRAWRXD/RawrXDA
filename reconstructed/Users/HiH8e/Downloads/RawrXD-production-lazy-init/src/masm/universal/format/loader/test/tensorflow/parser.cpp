#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <windows.h>

// Parser Context Structure (matching MASM)
struct ParserContext {
    uint32_t error_code;
    const char* error_message;
    void (*progress_cb)(uint64_t current, uint64_t total, void* data);
    void* progress_data;
    uint32_t total_steps;
    uint32_t current_step;
};

// Extern MASM functions
extern "C" {
    void* ParseTensorFlowSavedModel(const wchar_t* path, ParserContext* ctx, size_t* out_size);
    void* ParseONNXFile(const wchar_t* path, ParserContext* ctx, size_t* out_size);
}

// Progress callback implementation
void on_progress(uint64_t current, uint64_t total, void* data) {
    float percent = (float)current / total * 100.0f;
    std::cout << "\rProgress: " << percent << "% (" << current << "/" << total << ")" << std::flush;
}

int main() {
    std::cout << "=== TensorFlow/ONNX Parser Test Harness ===" << std::endl;

    ParserContext ctx = {0};
    ctx.progress_cb = on_progress;
    
    size_t gguf_size = 0;
    
    // Test 1: TensorFlow SavedModel
    std::cout << "Testing TensorFlow SavedModel parsing..." << std::endl;
    const wchar_t* tf_path = L"test_data/resnet50_saved_model";
    void* tf_gguf = ParseTensorFlowSavedModel(tf_path, &ctx, &gguf_size);
    
    if (tf_gguf) {
        std::cout << "\nSuccess! GGUF size: " << gguf_size << " bytes" << std::endl;
        free(tf_gguf);
    } else {
        std::cerr << "\nFailed: " << (ctx.error_message ? ctx.error_message : "Unknown error") << std::endl;
    }

    // Test 2: ONNX File
    std::cout << "\nTesting ONNX parsing..." << std::endl;
    const wchar_t* onnx_path = L"test_data/mnist.onnx";
    void* onnx_gguf = ParseONNXFile(onnx_path, &ctx, &gguf_size);
    
    if (onnx_gguf) {
        std::cout << "\nSuccess! GGUF size: " << gguf_size << " bytes" << std::endl;
        free(onnx_gguf);
    } else {
        std::cerr << "\nFailed: " << (ctx.error_message ? ctx.error_message : "Unknown error") << std::endl;
    }

    return 0;
}
