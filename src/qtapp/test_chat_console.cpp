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
    bool initialize(const std::string& model_path) {


        try {
            engine = std::make_unique<InferenceEngine>(model_path);
            
        } catch (const std::exception& e) {
            
            return false;
        }


        // Load model (this will block)
        bool loaded = engine->loadModel(model_path);
        if (!loaded) {
            
            return false;
        }


        return true;
    }
    
    /**
     * @brief Run the interactive chat loop
     */
    void runInteractiveChat() {


        std::string user_input;
        
        while (true) {
            
            std::getline(std::cin, user_input);
            
            // Handle special commands
            if (user_input == "quit" || user_input == "exit") {
                
                break;
            }
            
            if (user_input == "clear") {
                session->clearHistory();
                
                continue;
            }
            
            if (user_input == "stats") {
                printStats();
                continue;
            }
            
            if (user_input.empty()) {
                continue;
            }
            
            // Process the message
            processUserMessage(user_input);
        }
    }
    
    /**
     * @brief Run a quick test with predefined exchanges
     */
    void runQuickTest() {


        std::vector<std::string> test_messages = {
            "Hello!",
            "What is 2+2?",
            "Tell me a fun fact about programming"
        };
        
        for (const auto& msg : test_messages) {
            
            processUserMessage(msg);
            
        }
    }
    
private:
    /**
     * @brief Process a user message and generate AI response
     * @param user_message The text the user typed
     */
    void processUserMessage(const std::string& user_message) {
        if (!engine) {
            
            return;
        }
        
        if (is_waiting_for_response) {
            
            return;
        }
        
        // Add user message to history
        session->addMessage("user", user_message);
        
        // Build prompt with conversation context
        std::string full_prompt = session->buildPromptWithHistory();


        std::cout.flush();
        
        is_waiting_for_response = true;
        
        try {
            // === METHOD 1: Try synchronous generation with detokenization ===


            // Tokenize the prompt
            auto prompt_tokens = engine->tokenize(std::string::fromStdString(full_prompt));


            if (prompt_tokens.empty()) {
                
                is_waiting_for_response = false;
                return;
            }
            
            // Generate tokens
            auto generated_tokens = engine->generate(prompt_tokens, MAX_TOKENS);


            // Extract only the newly generated tokens (skip input tokens)
            if (generated_tokens.size() > prompt_tokens.size()) {
                std::vector<int32_t> new_tokens(
                    generated_tokens.begin() + prompt_tokens.size(),
                    generated_tokens.end()
                );
                
                // Detokenize to text
                std::string response_text = engine->detokenize(new_tokens);
                std::string response = response_text.toStdString();
                
                // Clean up response
                response = cleanupResponse(response);
                
                if (!response.empty()) {
                    
                    // Add to session history
                    session->addMessage("assistant", response);
                } else {
                    
                }
            } else {
                
            }
            
        } catch (const std::exception& e) {
            
        } catch (...) {
            
        }


        is_waiting_for_response = false;
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
        }
        
        // Trim trailing whitespace
        size_t end = result.find_last_not_of(" \n\r\t");
        if (end != std::string::npos) {
            result = result.substr(0, end + 1);
        }
        
        // Limit to single response (stop at double newline)
        size_t response_end = result.find("\n\n");
        if (response_end != std::string::npos) {
            result = result.substr(0, response_end);
        }
        
        return result;
    }
    
    /**
     * @brief Print chat statistics
     */
    void printStats() {


        if (engine) {


        }


        auto recent = session->getRecentMessages(4);
        for (const auto& msg : recent) {
            
            if (msg.content.size() > 60) 
            
        }
        
    }
};

/**
 * @brief Main entry point
 */
int main(int argc, char *argv[])
{
    // Initialize Qt (needed for signals/slots and threading)
    QCoreApplication app(argc, argv);


    try {
        auto chat = std::make_unique<ConsoleChat>();
        
        // Determine which model to load
        // Try 1: Command line argument (e.g., ./test_chat_console /path/to/model.gguf)
        // Try 2: Environment variable CHAT_MODEL
        // Try 3: Default ollama model name
        
        std::string model_path;
        
        if (argc > 1) {
            model_path = std::string::fromLocal8Bit(argv[1]);
            
        } else if (const char* env_model = std::getenv("CHAT_MODEL")) {
            model_path = std::string::fromLocal8Bit(env_model);
            
        } else {
            // Default to a common model (you can customize this)
            model_path = "gemma3";  // or could be a local path


        }
        
        // Initialize chat engine
        if (!chat->initialize(model_path)) {
            
            return 1;
        }
        
        // Choose test mode


        std::string mode_input;
        std::getline(std::cin, mode_input);
        
        if (mode_input == "2") {
            chat->runQuickTest();
        } else {
            chat->runInteractiveChat();
        }


        return 0;
        
    } catch (const std::exception& e) {
        
        return 1;
    } catch (...) {
        
        return 1;
    }
}

