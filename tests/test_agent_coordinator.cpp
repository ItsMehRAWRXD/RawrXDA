/**
 * AgentCoordinator unit tests — C++20 only (Qt-free).
 *
 * The original QtTest-based tests required AgentCoordinator from
 * src/orchestration/agent_coordinator.hpp (QObject, QString, QJsonObject, etc.).
 * That component is not present in the current tree.
 *
 * This stub compiles without Qt and reports that the full test suite
 * should be run when AgentCoordinator is available (C++20 + nlohmann::json or similar).
 */

#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "AgentCoordinator test (C++20): component not in tree.\n";
    std::cout << "  To run coordinator tests, build with orchestration/AgentCoordinator (C++20, no Qt).\n";
    return 0;
}
