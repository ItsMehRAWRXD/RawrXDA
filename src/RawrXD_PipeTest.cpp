// RawrXD_PipeTest.cpp - Named pipe client test harness
#include "RawrXD_PipeClient.h"
#include "logging/logger.h"
#include <string>

int main() {
    Logger logger("PipeTest");

    RawrXD::PipeClient client("RawrXD_PatternBridge");

    if (!client.Connect()) {
        logger.error("Failed to connect to RawrXD_PatternBridge pipe");
        return 1;
    }

    if (client.Ping()) {
        logger.info("Ping succeeded");
    } else {
        logger.warn("Ping failed");
    }

    std::string testText = "Test pattern classification via named pipe";
    auto result = client.Classify(testText);
    logger.info("Classification result received");

    return 0;
}

