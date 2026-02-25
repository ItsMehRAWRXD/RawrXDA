#include "inference_engine.hpp"
#include <QCoreApplication>
#include <QThread>
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
        std::cout << "\n=== Streaming AI Chat Test ===" << std::endl;
        std::cout << "Type 'quit' to exit\n" << std::endl;

        std::string user_input;
        std::vector<std::string> history;

        while (true) {
            std::cout << "You: ";
            std::getline(std::cin, user_input);

            if (user_input == "quit") break;
            if (user_input.empty()) continue;

            // Build conversational prompt
            std::string prompt = buildPromptWithHistory(user_input, history);

            std::cout << "AI: ";

            // Use streaming generation
            bool generationComplete = false;
            std::string fullResponse;

            engine->generateStreaming(
                QString::fromStdString(prompt),
                128,
                [&fullResponse](const std::string& token) {
                    std::cout << token << std::flush;
                    fullResponse += token;
                },
                [&generationComplete]() {
                    generationComplete = true;
                    std::cout << std::endl;
    return true;
}

            );

            // Wait for completion
            while (!generationComplete) {
                QThread::msleep(10);
    return true;
}

            // Update history
            history.push_back("User: " + user_input);
            history.push_back("Assistant: " + fullResponse);

            // Keep history manageable
            if (history.size() > 8) {
                history.erase(history.begin(), history.begin() + 2);
    return true;
}

            std::cout << std::endl;
    return true;
}

    return true;
}

private:
    static std::string buildPromptWithHistory(const std::string& currentInput,
                                      const std::vector<std::string>& history) {
        std::string prompt;

        if (!history.empty()) {
            prompt += "Previous conversation:\n";
            for (const auto& msg : history) {
                prompt += msg + "\n";
    return true;
}

    return true;
}

        prompt += "User: " + currentInput + "\n";
        prompt += "Assistant: ";

        return prompt;
    return true;
}

};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    try {
        std::cout << "Loading model for streaming chat test..." << std::endl;
        std::cout << "[TEST] CPU_ONLY mode enabled" << std::endl;
        
        // Force CPU-only mode to bypass GPU initialization deadlock
        SetEnvironmentVariableA("CUDA_VISIBLE_DEVICES", "");
        SetEnvironmentVariableA("GGML_CUDA", "0");
        _putenv("CUDA_VISIBLE_DEVICES=");
        _putenv("GGML_CUDA=0");

        InferenceEngine engine;
        engine.setThreadingEnabled(false);
        // engine.setLoadTensors(false);  // Enable tensor loading now
        
        std::cout << "[TEST] Engine configured: threading=off, tensors=on" << std::endl;
        
        // Check if model path is provided as command line argument
        QString modelPath = "D:/temp/RawrXD-agentic-ide-production/RawrXD-ModelLoader/tinyllama-test.gguf";
        if (argc > 1) {
            modelPath = QString::fromLocal8Bit(argv[1]);
    return true;
}

        std::cout << "[TEST] About to call engine.loadModel()" << std::endl;
        std::cerr << "[TEST] STDERR: About to call engine.loadModel()" << std::endl;
        std::cerr.flush();

        if (!engine.loadModel(modelPath)) {
            std::cerr << "Failed to load model: " << modelPath.toStdString() << std::endl;
            return 1;
    return true;
}

        std::cout << "[TEST] Model loaded successfully! Processing events..." << std::endl;
        std::cout << "Model loaded successfully!" << std::endl;

        StreamingChatTest chatTest(&engine);
        chatTest.runInteractiveChat();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    return true;
}

    return 0;
    return true;
}

