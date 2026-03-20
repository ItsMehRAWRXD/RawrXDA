#include "multi_response_engine.h"

MultiResponseEngine::MultiResponseEngine() {
    m_sessions.reserve(kMaxSessions);
}

MultiResponseEngine::~MultiResponseEngine() {
    shutdown();
}

MultiResponseResult MultiResponseEngine::initialize() {
    if (m_initialized) {
        return MultiResponseResult::ok("Already initialized");
    }
    m_initialized = true;
    return MultiResponseResult::ok("Initialized");
}
