// ============================================================================
// native_agent.cpp — Native Agent Implementation
// ============================================================================
#include "native_agent.hpp"
#include <sstream>
#include <fstream>

namespace RawrXD {

NativeAgent::NativeAgent(CPUInferenceEngine* engine)
    : m_engine(engine)
    , m_deepThink(false)
    , m_deepResearch(false)
    , m_noRefusal(false)
    , m_autoCorrect(false)
    , m_maxMode(false)
    , m_languageContext("cpp")
{
}

NativeAgent::~NativeAgent() = default;

} // namespace RawrXD
