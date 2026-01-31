#include "RawrXD_PipeClient.h"
#include <iostream>
#include <iomanip>

int main() {


    try {
        RawrXD::PipeClient client("RawrXD_PatternBridge");


        if (!client.// Connect removed) {
            
            return 1;
        }


        if (client.Ping()) {
            
        } else {
            
        }

        std::string testText = "BUG: This is a test memory leak in the C++ client harness";


        auto result = client.Classify(testText);


        client;
    }
    catch (const std::exception& e) {
        
        return 1;
    }

    return 0;
}

