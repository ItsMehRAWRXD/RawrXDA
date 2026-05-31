#pragma once
#include <string>

/**
 * @file AutonomousNarrationLayer.h
 * @brief Converts binary evolution events into human-readable tactical summaries.
 */

class AutonomousNarrationLayer {
public:
    static std::string Narrate(const char* type, const char* nodeId, const char* payloadJson);
};
