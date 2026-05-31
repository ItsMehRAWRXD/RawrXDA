#include "cpu_inference_engine.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Testing RawrXD inference with phi3mini model..." << std::endl;

    // Create inference engine
    RawrXD::CPUInferenceEngine engine;

    // Load the model
    std::string modelPath = "d:/rawrxd/build-ninja/bin/models/phi3mini.gguf";
    std::cout << "Loading model: " << modelPath << std::endl;

    if (!engine.LoadModel(modelPath)) {
        std::cerr << "Failed to load model!" << std::endl;
        return 1;
    }

    std::cout << "Model loaded successfully!" << std::endl;

    // Test tokenization
    std::string testText = "Hello, how are you?";
    std::cout << "Tokenizing: \"" << testText << "\"" << std::endl;

    std::vector<int32_t> tokens = engine.Tokenize(testText);
    std::cout << "Tokens: " << tokens.size() << " tokens" << std::endl;

    // Test detokenization
    std::string decoded = engine.Detokenize(tokens);
    std::cout << "Detokenized: \"" << decoded << "\"" << std::endl;

    // Test generation
    std::cout << "Generating response..." << std::endl;
    std::vector<int32_t> response = engine.Generate(tokens, 20);
    std::string responseText = engine.Detokenize(response);
    std::cout << "Generated: \"" << responseText << "\"" << std::endl;

    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}