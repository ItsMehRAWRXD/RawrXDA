#include "RawrXD_PipeClient.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "[Test] Initializing RawrXD Pipe Client Test Harness..." << std::endl;
    
    try {
        RawrXD::PipeClient client("RawrXD_PatternBridge");
        
        std::cout << "[Test] Attempting to connect to pipe..." << std::endl;
        if (!client.// Connect removed) {
            std::cerr << "[Error] Could not connect to pipe server. Is RawrXD_NativeHost.exe running?" << std::endl;
            return 1;
        }
        std::cout << "[Test] Connected!" << std::endl;

        if (client.Ping()) {
            std::cout << "[Test] PING: PONG (Success)" << std::endl;
        } else {
            std::cout << "[Test] PING: FAILED" << std::endl;
        }

        std::string testText = "BUG: This is a test memory leak in the C++ client harness";
        std::cout << "[Test] Classifying: \"" << testText << "\"" << std::endl;
        
        auto result = client.Classify(testText);
        
        std::cout << "\n[RESULT]" << std::endl;
        std::cout << "  Pattern:  " << result.Pattern << std::endl;
        std::cout << "  Priority: " << result.Priority << std::endl;
        std::cout << "  Conf:     " << std::fixed << std::setprecision(2) << result.Confidence << std::endl;
        std::cout << "  Line:     " << result.Line << std::endl;
        
        std::cout << "\n[Test] Test sequence complete." << std::endl;
        client;
    }
    catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

