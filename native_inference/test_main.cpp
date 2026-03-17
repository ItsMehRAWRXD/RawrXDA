#include "native_inference_engine.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <gguf_model_path>" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];

    // Initialize engine
    NativeInferenceEngine engine;
    if (!engine.Initialize(model_path)) {
        std::cerr << "Failed to initialize inference engine" << std::endl;
        return 1;
    }

    std::cout << "Native Inference Engine initialized successfully!" << std::endl;
    std::cout << "Vocab size: " << engine.GetVocabSize() << std::endl;
    std::cout << "Context length: " << engine.GetContextLength() << std::endl;
    std::cout << "Embedding dimension: " << engine.GetEmbeddingDim() << std::endl;

    // Test tokenization
    std::string test_text = "Hello, world!";
    auto tokens = engine.Tokenize(test_text);
    std::cout << "Tokenized '" << test_text << "' to " << tokens.size() << " tokens" << std::endl;

    // Test detokenization
    std::string decoded = engine.Detokenize(tokens);
    std::cout << "Detokenized back to: '" << decoded << "'" << std::endl;

    // Test generation (simplified)
    std::string prompt = "The quick brown fox";
    std::cout << "Generating from prompt: '" << prompt << "'" << std::endl;

    std::string generated = engine.Generate(prompt, 10, 0.8f);
    std::cout << "Generated text: '" << generated << "'" << std::endl;

    return 0;
}