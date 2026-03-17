#pragma once
#include <vector>
#include <string>
#include <future>
#include <deque>
#include "CommonTypes.h"
#include "utils/Expected.h"

namespace RawrXD {

enum class SwarmError {
    Success = 0,
    InitializationFailed,
    TaskSubmissionFailed,
    ConsensusFailed,
    Timeout,
    InternalError
};

struct SwarmResult {
    std::string consensus;
    float confidence;
    std::vector<std::string> individualThoughts;
    double executionTimeMs;
};

struct InternalSwarmTask {
    std::string id;
    std::string description;
    std::string context;
    int priority;
    std::promise<SwarmResult> promise;
};

class SwarmOrchestrator {
public:
    SwarmOrchestrator();
    ~SwarmOrchestrator();

    RawrXD::Expected<void, SwarmError> initialize();
    std::future<SwarmResult> submitTask(const std::string& description, const std::string& context);
    
private:
    // Pimpl or minimal members to avoid heavy includes
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace RawrXD
