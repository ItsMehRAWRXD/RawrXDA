# Native Inference Engine

A high-performance, native inference engine implementation that replaces ggml with custom C++20, MASM x64, and DirectX 12 components.

## Features

- **Native GGUF Loader**: Parses GGUF files without external dependencies using std::ifstream
- **Quantization Kernels**: MASM x64 implementations for Q4_0 and Q8_0 quantization
- **Matrix Multiplication**: AVX/AVX2 optimized with DirectX 12 GPU acceleration
- **BPE Tokenizer**: SSE4.2 accelerated tokenization with C++20 merge tables
- **Speculative Decoding**: Native std::thread implementation for parallel verification
- **KV Cache**: Sliding window with SVD compression for memory efficiency

## Requirements

- Windows 10/11 (64-bit)
- Visual Studio 2022 with C++20 support
- DirectX 12 compatible GPU
- CMake 3.20+

## Building

1. Clone or navigate to the project directory
2. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

3. Configure with CMake:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

4. Build the project:
   ```bash
   cmake --build . --config Release
   ```

## Usage

### Basic Example

```cpp
#include "native_inference_engine.h"

int main() {
    NativeInferenceEngine engine;

    // Initialize with GGUF model
    if (!engine.Initialize("path/to/model.gguf")) {
        return 1;
    }

    // Generate text
    std::string prompt = "The future of AI is";
    std::string result = engine.Generate(prompt, 50, 0.8f);

    std::cout << result << std::endl;
    return 0;
}
```

### Tokenization

```cpp
// Tokenize text
std::vector<uint32_t> tokens = engine.Tokenize("Hello world!");

// Detokenize back to text
std::string text = engine.Detokenize(tokens);
```

## Architecture

### Core Components

1. **NativeGGUFLoader**: Handles GGUF file parsing and tensor loading
2. **NativeBPETokenizer**: BPE tokenization with SIMD acceleration
3. **NativeSpeculativeDecoder**: Parallel token verification
4. **NativeKVCache**: Memory-efficient key-value caching
5. **NativeInferenceEngine**: Main interface coordinating all components

### Performance Optimizations

- **MASM x64 Assembly**: Hand-optimized kernels for quantization and matrix ops
- **AVX/AVX2 Instructions**: Vectorized operations for maximum throughput
- **DirectX 12 Compute**: GPU acceleration for large matrix multiplications
- **SSE4.2 String Processing**: Fast tokenization with SIMD string search
- **Native Threading**: Parallel speculative decoding verification

## Supported Quantization

- **Q4_0**: 4-bit quantization with block-wise scaling
- **Q8_0**: 8-bit quantization with absolute-max scaling

## API Reference

### NativeInferenceEngine

```cpp
class NativeInferenceEngine {
public:
    bool Initialize(const std::string& model_path);
    std::string Generate(const std::string& prompt, size_t max_tokens, float temperature = 1.0f);
    std::vector<uint32_t> Tokenize(const std::string& text) const;
    std::string Detokenize(const std::vector<uint32_t>& tokens) const;

    size_t GetVocabSize() const;
    size_t GetContextLength() const;
    size_t GetEmbeddingDim() const;
};
```

## Testing

Build and run the test executable:

```bash
./Release/native_inference_test path/to/model.gguf
```

This will validate:
- Model loading and initialization
- Tokenization/detokenization
- Basic text generation

## Performance Notes

- First inference may be slower due to initialization
- GPU acceleration activates for matrices > 1024x1024
- KV cache compression reduces memory usage by ~30%
- Speculative decoding can improve throughput by 2-3x

## Troubleshooting

### Build Issues

- Ensure Visual Studio 2022 is installed with C++ workload
- Install DirectX 12 development headers
- Use 64-bit build configuration

### Runtime Issues

- Verify GGUF model compatibility
- Check DirectX 12 GPU support
- Ensure sufficient RAM for model loading

## License

This implementation is provided as-is for research and development purposes.