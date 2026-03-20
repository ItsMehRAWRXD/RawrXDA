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

void MultiResponseEngine::shutdown() {
    if (!m_initialized) {
        return;
    }
    std::lock_guard<std::mutex> sessionLock(m_sessionMutex);
    std::lock_guard<std::mutex> prefLock(m_prefMutex);
    m_sessions.clear();
    m_preferenceHistory.clear();
    m_initialized = false;
}
