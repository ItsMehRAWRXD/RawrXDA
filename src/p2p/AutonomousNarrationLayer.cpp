#include "AutonomousNarrationLayer.h"
#include <sstream>
#include <iostream>

std::string AutonomousNarrationLayer::Narrate(const char* type, const char* nodeId, const char* payloadJson) {
    std::string t(type);
    std::string n(nodeId);
    std::string p(payloadJson);
    
    std::stringstream result;
    if (t == "KernelPromoted") {
        result << "Node " << n << " promoted a new kernel after multi-seed validation. Replaced previous version with " << p << ". Optimization verified.";
    } else if (t == "SealEvent") {
        result << "LocalNode hardened kernel execution at " << p << ". Memory protected with Execute-Only (XOM) flags.";
    } else if (t == "PulseCycleComplete") {
        result << "Global Synchronization Pulse completed. Swarm state: " << p << ". Consensus maintained.";
    } else if (t == "ShadowPassStarted") {
        result << "Deployment Phase A active: Comparing Sovereign Kernel against stable baseline for " << p << ".";
    } else if (t == "ShadowPassComplete") {
        result << "Shadow Validation finished. Result: " << p << ". Ready for primary promotion.";
    } else {
        result << "[" << t << "] Trace detected on " << n << " involving " << p;
    }
    
    return result.str();
}
