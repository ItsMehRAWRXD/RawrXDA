#include <iostream>
#include <cstring>
#include <cassert>
#include "../asm/ai_agent_masm_bridge.hpp"

// Forward declarations for the functions we implemented
extern "C" {
MasmOperationResult masm_server_inject_request_hook(void* request_buffer, size_t buffer_size,
                                                    void (*transform)(void*, void*));
MasmOperationResult masm_server_stream_chunk_process(const void* input_chunk, size_t chunk_size,
                                                     void* output_buffer, size_t output_size,
                                                     size_t* bytes_processed);
MasmOperationResult masm_agent_correction_apply_bytecode(const void* correction_bytecode, size_t code_size,
                                                        void* target_response, size_t response_size);
MasmOperationResult masm_ai_completion_stream_transform(const void* raw_completion, size_t completion_size,
                                                       void* transformed_output, size_t output_size,
                                                       uint32_t transformation_flags);
}

// Test callback for hook injection
void test_transform_callback(void* buffer, void* context) {
    std::cout << "Transform callback called with context: " << (uintptr_t)context << std::endl;
}

int main() {
    std::cout << "Testing MASM stub implementations..." << std::endl;

    // Test 1: Server request hook injection
    {
        char request[] = "GET /api/test HTTP/1.1\r\nUser-Agent: TestAgent\r\nContent-Type: application/json\r\n\r\n";
        size_t buffer_size = strlen(request) + 1;

        auto result = masm_server_inject_request_hook(request, buffer_size, test_transform_callback);
        std::cout << "Hook injection result: " << (result.success ? "SUCCESS" : "FAILED")
                  << " - " << result.detail << std::endl;
        assert(result.success);
    }

    // Test 2: Stream chunk processing
    {
        const char* json_chunk = "{\"test\": \"data\", \"array\": [1, 2, 3]}";
        char output_buffer[1024] = {0};
        size_t bytes_processed = 0;

        auto result = masm_server_stream_chunk_process(json_chunk, strlen(json_chunk),
                                                       output_buffer, sizeof(output_buffer),
                                                       &bytes_processed);
        std::cout << "Stream processing result: " << (result.success ? "SUCCESS" : "FAILED")
                  << " - " << result.detail << " - Processed: " << bytes_processed << " bytes" << std::endl;
        assert(result.success);
        assert(bytes_processed > 0);
    }

    // Test 3: Bytecode correction
    {
        // Simple replace operation: opcode 0x01, offset 0, length 4, data "NEW!"
        uint8_t bytecode[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 'N', 'E', 'W', '!'};
        char response[] = "OLD DATA HERE";

        auto result = masm_agent_correction_apply_bytecode(bytecode, sizeof(bytecode),
                                                          response, strlen(response));
        std::cout << "Bytecode correction result: " << (result.success ? "SUCCESS" : "FAILED")
                  << " - " << result.detail << std::endl;
        assert(result.success);
        assert(strncmp(response, "NEW!", 4) == 0);
    }

    // Test 4: Completion stream transformation
    {
        const char* completion = "This is a test completion with   multiple   spaces.";
        char output[1024] = {0};

        auto result = masm_ai_completion_stream_transform(completion, strlen(completion),
                                                         output, sizeof(output),
                                                         0x04); // Normalize whitespace
        std::cout << "Stream transform result: " << (result.success ? "SUCCESS" : "FAILED")
                  << " - " << result.detail << std::endl;
        assert(result.success);
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}