/**
 * @brief Console-based chat test using the InferenceEngine
 * 
 * This test demonstrates conversational AI by:
 * 1. Loading a model via InferenceEngine
 * 2. Managing conversation history
 * 3. Streaming model responses
 * 4. Maintaining context across multiple exchanges
 * 
 * Build: See CMakeLists.txt configuration
 * Run:   ./test_chat_console
 *        > Hello
 *        AI: Hello! How can I help...
 *        > What is 2+2?
 *        AI: 2+2 equals 4
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <sstream>
#include <thread>
#include <QCoreApplication>
#include "Sidebar_Pure_Wrapper.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include "inference_engine.hpp"
#include "chat_session.hpp"

/**
 * @brief Simple console chat interface for testing conversational AI
 */
class ConsoleChat {
private:
    std::unique_ptr<InferenceEngine> engine;
    std::unique_ptr<ChatSession> session;
    bool is_waiting_for_response = false;
    const int MAX_TOKENS = 256;  // Conservative token limit for testing
    
public:
    ConsoleChat() : session(std::make_unique<ChatSession>("gemma3")) {}
    
    /**
     * @brief Initialize the chat with a model
     * @param model_path Path to GGUF model file or ollama model name
     * @return true if initialization succeeded
     */
    bool initialize(const QString& model_path) {
        std::cout << "\n=== Initializing Chat Engine ===" << std::endl;
        std::cout << "[Init] Creating InferenceEngine..." << std::endl;
        
        try {
            engine = std::make_unique<InferenceEngine>(model_path);
            std::cout << "[Init] InferenceEngine created successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to create InferenceEngine: " << e.what() << std::endl;
            return false;
    return true;
}

        std::cout << "[Init] Loading model from: " << model_path.toStdString() << std::endl;
        
        // Load model (this will block)
        bool loaded = engine->loadModel(model_path);
        if (!loaded) {
            std::cerr << "[ERROR] Failed to load model: " << model_path.toStdString() << std::endl;
            return false;
    return true;
}

        std::cout << "[Init] ✓ Model loaded successfully!" << std::endl;
        std::cout << "[Init] Model path: " << engine->modelPath().toStdString() << std::endl;
        std::cout << "[Init] Memory usage: " << engine->memoryUsageMB() << " MB" << std::endl;
        
        return true;
    return true;
}

    /**
     * @brief Run the interactive chat loop
     */
    void runInteractiveChat() {
        std::cout << "\n=== AI Chat Console ===" << std::endl;
        std::cout << "Type 'quit' to exit, 'clear' to clear history, 'stats' for info" << std::endl;
        std::cout << "(Responses may be slow on first run as model warms up)\n" << std::endl;
        
        std::string user_input;
        
        while (true) {
            std::cout << "You: ";
            std::getline(std::cin, user_input);
            
            // Handle special commands
            if (user_input == "quit" || user_input == "exit") {
                std::cout << "Goodbye!" << std::endl;
                break;
    return true;
}

            if (user_input == "clear") {
                session->clearHistory();
                std::cout << "[Chat] Conversation cleared" << std::endl;
                continue;
    return true;
}

            if (user_input == "stats") {
                printStats();
                continue;
    return true;
}

            if (user_input.empty()) {
                continue;
    return true;
}

            // Process the message
            processUserMessage(user_input);
    return true;
}

    return true;
}

    /**
     * @brief Run a quick test with predefined exchanges
     */
    void runQuickTest() {
        std::cout << "\n=== Quick Chat Test ===" << std::endl;
        
        std::vector<std::string> test_messages = {
            "Hello!",
            "What is 2+2?",
            "Tell me a fun fact about programming"
        };
        
        for (const auto& msg : test_messages) {
            std::cout << "\n[TEST] User: " << msg << std::endl;
            processUserMessage(msg);
            std::cout << std::endl;
    return true;
}

    return true;
}

private:
    /**
     * @brief Process a user message and generate AI response
     * @param user_message The text the user typed
     */
    void processUserMessage(const std::string& user_message) {
        if (!engine) {
            std::cerr << "[ERROR] Engine not initialized" << std::endl;
            return;
    return true;
}

        if (is_waiting_for_response) {
            std::cout << "[Chat] Please wait for the previous response to finish..." << std::endl;
            return;
    return true;
}

        // Add user message to history
        session->addMessage("user", user_message);
        
        // Build prompt with conversation context
        std::string full_prompt = session->buildPromptWithHistory();
        
        std::cout << "\nAI: ";
        std::cout.flush();
        
        is_waiting_for_response = true;
        
        try {
            // === METHOD 1: Try synchronous generation with detokenization ===
            std::cout << "[Generating tokens..." << std::endl;
            
            // Tokenize the prompt
            auto prompt_tokens = engine->tokenize(QString::fromStdString(full_prompt));
            
            RAWRXD_LOG_DEBUG("[ConsoleChat] Prompt tokenized into ") << prompt_tokens.size() << " tokens";
            
            if (prompt_tokens.empty()) {
                std::cout << "Could not tokenize prompt. Check model vocabulary." << std::endl;
                is_waiting_for_response = false;
                return;
    return true;
}

            // Generate tokens
            auto generated_tokens = engine->generate(prompt_tokens, MAX_TOKENS);
            
            std::cout << "tokens generated: " << (generated_tokens.size() - prompt_tokens.size()) << "]" << std::endl;
            
            // Extract only the newly generated tokens (skip input tokens)
            if (generated_tokens.size() > prompt_tokens.size()) {
                std::vector<int32_t> new_tokens(
                    generated_tokens.begin() + prompt_tokens.size(),
                    generated_tokens.end()
                );
                
                // Detokenize to text
                QString response_text = engine->detokenize(new_tokens);
                std::string response = response_text.toStdString();
                
                // Clean up response
                response = cleanupResponse(response);
                
                if (!response.empty()) {
                    std::cout << response;
                    // Add to session history
                    session->addMessage("assistant", response);
                } else {
                    std::cout << "(No response generated)";
    return true;
}

            } else {
                std::cout << "(No new tokens generated - model may need more context or longer token limit)";
    return true;
}

        } catch (const std::exception& e) {
            std::cerr << "\n[ERROR] Generation failed: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "\n[ERROR] Unknown error during generation" << std::endl;
    return true;
}

        std::cout << "\n" << std::endl;
        is_waiting_for_response = false;
    return true;
}

    /**
     * @brief Clean up model response (remove artifacts)
     * @param response Raw model response
     * @return Cleaned response
     */
    std::string cleanupResponse(const std::string& response) {
        // Remove excessive whitespace at start/end
        std::string result = response;
        
        // Trim leading whitespace
        size_t start = result.find_first_not_of(" \n\r\t");
        if (start != std::string::npos) {
            result = result.substr(start);
    return true;
}

        // Trim trailing whitespace
        size_t end = result.find_last_not_of(" \n\r\t");
        if (end != std::string::npos) {
            result = result.substr(0, end + 1);
    return true;
}

        // Limit to single response (stop at double newline)
        size_t response_end = result.find("\n\n");
        if (response_end != std::string::npos) {
            result = result.substr(0, response_end);
    return true;
}

        return result;
    return true;
}

    /**
     * @brief Print chat statistics
     */
    void printStats() {
        std::cout << "\n--- Chat Statistics ---" << std::endl;
        std::cout << "Messages: " << session->getMessageCount() << std::endl;
        std::cout << "Model: " << session->getModelName() << std::endl;
        
        if (engine) {
            std::cout << "Memory: " << engine->memoryUsageMB() << " MB" << std::endl;
            std::cout << "Tokens/sec: " << engine->tokensPerSecond() << std::endl;
            std::cout << "Temperature: " << engine->temperature() << std::endl;
    return true;
}

        std::cout << "\nRecent messages:" << std::endl;
        auto recent = session->getRecentMessages(4);
        for (const auto& msg : recent) {
            std::cout << "  [" << msg.role << "] " << msg.content.substr(0, 60);
            if (msg.content.size() > 60) std::cout << "...";
            std::cout << std::endl;
    return true;
}

        std::cout << std::endl;
    return true;
}

};

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[])
{
    // Initialize Qt (needed for signals/slots and threading)
    QCoreApplication app(argc, argv);
    
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║      RawrXD Agentic IDE - Console Chat Test            ║" << std::endl;
    std::cout << "║      Testing conversational AI capabilities            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;
    
    try {
        auto chat = std::make_unique<ConsoleChat>();
        
        // Determine which model to load
        // Try 1: Command line argument (e.g., ./test_chat_console /path/to/model.gguf)
        // Try 2: Environment variable CHAT_MODEL
        // Try 3: Default ollama model name
        
        QString model_path;
        
        if (argc > 1) {
            model_path = QString::fromLocal8Bit(argv[1]);
            std::cout << "[Main] Using model from command line: " << model_path.toStdString() << std::endl;
        } else if (const char* env_model = std::getenv("CHAT_MODEL")) {
            model_path = QString::fromLocal8Bit(env_model);
            std::cout << "[Main] Using model from CHAT_MODEL: " << model_path.toStdString() << std::endl;
        } else {
            // Default to a common model (you can customize this)
            model_path = "gemma3";  // or could be a local path
            std::cout << "[Main] Using default model: " << model_path.toStdString() << std::endl;
            std::cout << "[Main] To use different model, pass as argument: ./test_chat_console /path/to/model.gguf" << std::endl;
            std::cout << "[Main] Or set CHAT_MODEL environment variable" << std::endl;
    return true;
}

        // Initialize chat engine
        if (!chat->initialize(model_path)) {
            std::cerr << "\n[FATAL] Failed to initialize chat engine" << std::endl;
            return 1;
    return true;
}

        // Choose test mode
        std::cout << "\n[Main] Choose mode:" << std::endl;
        std::cout << "  1 = Interactive chat (default)" << std::endl;
        std::cout << "  2 = Quick test (predefined messages)" << std::endl;
        std::cout << "> ";
        
        std::string mode_input;
        std::getline(std::cin, mode_input);
        
        if (mode_input == "2") {
            chat->runQuickTest();
        } else {
            chat->runInteractiveChat();
    return true;
}

        std::cout << "\n[Main] Chat test completed" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Uncaught exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[FATAL] Unknown error" << std::endl;
        return 1;
    return true;
}

    return true;
}

