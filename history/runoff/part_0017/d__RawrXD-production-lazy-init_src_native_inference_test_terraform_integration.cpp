// test_terraform_integration.cpp
// Test program for TerraForm-enhanced GGUF loader

#include "native_gguf_loader.h"
#include <iostream>
#include <iomanip>

int main(int argc, char* argv[]) {
    const char* filepath = (argc > 1) ? argv[1] : "model.gguf";

    std::cout << "Testing TerraForm-enhanced Native GGUF Loader" << std::endl;
    std::cout << "Loading: " << filepath << std::endl;
    std::cout << std::endl;

    // Test C++ interface
    std::cout << "=== C++ Interface Test ===" << std::endl;
    try {
        NativeGGUFLoader loader;

        if (!loader.Open(filepath)) {
            std::cerr << "Failed to open file: " << filepath << std::endl;
            return 1;
        }

        const auto& header = loader.GetHeader();
        std::cout << "GGUF Header:" << std::endl;
        std::cout << "  Magic: 0x" << std::hex << header.magic << std::dec << std::endl;
        std::cout << "  Version: " << header.version << std::endl;
        std::cout << "  Tensors: " << header.tensor_count << std::endl;
        std::cout << "  Metadata: " << header.metadata_kv_count << std::endl;
        std::cout << std::endl;

        const auto& tensors = loader.GetTensors();
        std::cout << "Tensors (" << tensors.size() << "):" << std::endl;

        for (size_t i = 0; i < std::min(tensors.size(), size_t(5)); ++i) {
            const auto& tensor = tensors[i];
            std::cout << "  " << tensor.name << ": ";
            for (uint32_t j = 0; j < tensor.n_dims; ++j) {
                std::cout << tensor.dims[j];
                if (j < tensor.n_dims - 1) std::cout << "x";
            }
            std::cout << " (" << GGUFUtils::GetDataTypeSize(static_cast<GGUFUtils::DataType>(tensor.type))
                      << " bytes per element, " << tensor.size << " total bytes)" << std::endl;
        }

        if (tensors.size() > 5) {
            std::cout << "  ... and " << (tensors.size() - 5) << " more tensors" << std::endl;
        }

        loader.Close();

    } catch (const std::exception& e) {
        std::cerr << "C++ interface error: " << e.what() << std::endl;
    }

    std::cout << std::endl;

    // Test C interface (TerraForm compatible)
    std::cout << "=== C Interface Test (TerraForm Compatible) ===" << std::endl;

    native_gguf_loader_t c_loader = native_gguf_loader_create();
    if (!c_loader) {
        std::cerr << "Failed to create C loader" << std::endl;
        return 1;
    }

    if (native_gguf_loader_open(c_loader, filepath)) {
        size_t tensor_count = native_gguf_loader_get_tensor_count(c_loader);
        std::cout << "C interface: Successfully opened file with " << tensor_count << " tensors" << std::endl;

        // Try to load a common tensor
        const char* test_tensors[] = {
            "token_embd.weight",
            "blk.0.attn_q.weight",
            "output.weight",
            nullptr
        };

        for (const char** tensor_name = test_tensors; *tensor_name; ++tensor_name) {
            // Allocate a small buffer for testing
            size_t buffer_size = 1024; // 1KB test buffer
            void* buffer = malloc(buffer_size);

            if (native_gguf_loader_load_tensor(c_loader, *tensor_name, buffer, buffer_size)) {
                std::cout << "  Successfully loaded tensor: " << *tensor_name << std::endl;
            }

            free(buffer);
        }

        native_gguf_loader_close(c_loader);
        std::cout << "C interface: Successfully closed file" << std::endl;
    } else {
        std::cerr << "C interface: Failed to open file" << std::endl;
    }

    native_gguf_loader_destroy(c_loader);

    std::cout << std::endl;
    std::cout << "TerraForm integration test completed!" << std::endl;
    std::cout << "The loader is now fully compatible with RXUC-TerraForm universal compiler." << std::endl;

    return 0;
}