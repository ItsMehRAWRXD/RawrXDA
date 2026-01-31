#include "inference_engine.hpp"


#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <windows.h>

class StreamingChatTest {
private:
    InferenceEngine* engine;

public:
    explicit StreamingChatTest(InferenceEngine* eng) : engine(eng) {}

    void runInteractiveChat() {


        std::string user_input;
        std::vector<std::string> history;

        while (true) {
            
            std::getline(std::cin, user_input);

            if (user_input == "quit") break;
            if (user_input.empty()) continue;

            // Build conversational prompt
            std::string prompt = buildPromptWithHistory(user_input, history);


            // Use streaming generation
            bool generationComplete = false;
            std::string fullResponse;

            engine->generateStreaming(
                std::string::fromStdString(prompt),
                128,
                [&fullResponse](const std::string& token) {
                    
                    fullResponse += token;
                },
                [&generationComplete]() {
                    generationComplete = true;
                    
                }
            );

            // Wait for completion
            while (!generationComplete) {
                std::thread::msleep(10);
            }

            // Update history
            history.push_back("User: " + user_input);
            history.push_back("Assistant: " + fullResponse);

            // Keep history manageable
            if (history.size() > 8) {
                history.erase(history.begin(), history.begin() + 2);
            }


        }
    }

private:
    static std::string buildPromptWithHistory(const std::string& currentInput,
                                      const std::vector<std::string>& history) {
        std::string prompt;

        if (!history.empty()) {
            prompt += "Previous conversation:\n";
            for (const auto& msg : history) {
                prompt += msg + "\n";
            }
        }

        prompt += "User: " + currentInput + "\n";
        prompt += "Assistant: ";

        return prompt;
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    try {


        // Force CPU-only mode to bypass GPU initialization deadlock
        SetEnvironmentVariableA("CUDA_VISIBLE_DEVICES", "");
        SetEnvironmentVariableA("GGML_CUDA", "0");
        _putenv("CUDA_VISIBLE_DEVICES=");
        _putenv("GGML_CUDA=0");

        InferenceEngine engine;
        engine.setThreadingEnabled(false);
        // engine.setLoadTensors(false);  // Enable tensor loading now


        // Check if model path is provided as command line argument
        std::string modelPath = "D:/temp/RawrXD-agentic-ide-production/RawrXD-ModelLoader/tinyllama-test.gguf";
        if (argc > 1) {
            modelPath = std::string::fromLocal8Bit(argv[1]);
        }


        std::cerr.flush();

        if (!engine.loadModel(modelPath)) {
            
            return 1;
        }


        StreamingChatTest chatTest(&engine);
        chatTest.runInteractiveChat();

    } catch (const std::exception& e) {
        
        return 1;
    }

    return 0;
}

