// Simple IDE instantiation test
#include <windows.h>
#include "logging/logger.h"

int main() {
    Logger logger("SimpleTest", "logs/");
    logger.info("Test started");
    logger.info("Simple IDE Test");
    logger.info("===============");
    logger.info("About to load library");

    // Just test if we can reach main
    logger.info("Success: Program executed");

    return 0;
}
