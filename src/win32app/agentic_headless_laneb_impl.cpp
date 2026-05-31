// Lane B (RawrEngine headless): resolves symbols referenced by agentic_bridge_headless.cpp
// without linking the full subagent_core.cpp / Win32IDE Phase16-17 translation units.
#include "subagent_core.h"
// Completes BulkFixOrchestrator for SubAgentManager's unique_ptr member destructor.
#include "Win32IDE_Phase16_AgenticController.h"
#include "Win32IDE_Phase17_AgenticProfiler.h"
#include "agent/autonomous_subagent.hpp"

#include <atomic>
#include <sstream>
#include <windows.h>

// -----------------------------------------------------------------------------
// SubAgentManager — functional headless implementation
// -----------------------------------------------------------------------------

struct SubAgentRecord
{
    std::string id;
    std::string parentId;
    std::string description;
    std::string prompt;
    std::string result;
    std::chrono::steady_clock::time_point spawnTime;
    bool completed = false;
    bool cancelled = false;
};

// LAZY SINGLETON PATTERN: Avoid SIOF - non-trivial constructors
inline std::mutex& GetSubAgentMutex()
{
    static std::mutex* inst = new std::mutex();
    return *inst;
}
#define g_subAgentMutex GetSubAgentMutex()

inline std::unordered_map<std::string, SubAgentRecord>& GetSubAgents()
{
    static std::unordered_map<std::string, SubAgentRecord>* inst =
        new std::unordered_map<std::string, SubAgentRecord>();
    return *inst;
}
#define g_subAgents GetSubAgents()

inline std::atomic<uint64_t>& GetSubAgentCounter()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_subAgentCounter GetSubAgentCounter()

SubAgentManager::SubAgentManager(AgenticEngine* engine) : m_engine(engine) {}

SubAgentManager::~SubAgentManager()
{
    cancelAll();
}

std::string SubAgentManager::spawnSubAgent(const std::string& parentId, const std::string& description,
                                           const std::string& prompt)
{
    std::lock_guard<std::mutex> lock(g_subAgentMutex);
    uint64_t idNum = g_subAgentCounter.fetch_add(1, std::memory_order_relaxed);
    std::string id = "sa-" + std::to_string(idNum);
    SubAgentRecord rec;
    rec.id = id;
    rec.parentId = parentId;
    rec.description = description;
    rec.prompt = prompt;
    rec.spawnTime = std::chrono::steady_clock::now();
    g_subAgents[id] = std::move(rec);
    return id;
}

void SubAgentManager::cancelAll()
{
    std::lock_guard<std::mutex> lock(g_subAgentMutex);
    for (auto& [id, rec] : g_subAgents)
    {
        rec.cancelled = true;
    }
}

bool SubAgentManager::waitForSubAgent(const std::string& agentId, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::lock_guard<std::mutex> lock(g_subAgentMutex);
        auto it = g_subAgents.find(agentId);
        if (it != g_subAgents.end() && it->second.completed)
        {
            return true;
        }
        if (it != g_subAgents.end() && it->second.cancelled)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::string SubAgentManager::getSubAgentResult(const std::string& agentId) const
{
    std::lock_guard<std::mutex> lock(g_subAgentMutex);
    auto it = g_subAgents.find(agentId);
    if (it != g_subAgents.end())
    {
        return it->second.result;
    }
    return {};
}

std::string SubAgentManager::executeChain(const std::string& parentId, const std::vector<std::string>& promptTemplates,
                                          const std::string& initialInput)
{
    std::string currentInput = initialInput;
    for (size_t i = 0; i < promptTemplates.size(); ++i)
    {
        std::string prompt = promptTemplates[i];
        // Replace {input} placeholder
        size_t pos = prompt.find("{input}");
        if (pos != std::string::npos)
        {
            prompt.replace(pos, 7, currentInput);
        }
        std::string agentId = spawnSubAgent(parentId, "chain-step-" + std::to_string(i), prompt);
        // Simulate execution
        {
            std::lock_guard<std::mutex> lock(g_subAgentMutex);
            auto it = g_subAgents.find(agentId);
            if (it != g_subAgents.end())
            {
                it->second.result = "[lane-b stub] chain step " + std::to_string(i) +
                                    " not executed against a model (no engine hook): " + prompt.substr(0, 100);
                it->second.completed = true;
                currentInput = it->second.result;
            }
        }
    }
    return currentInput;
}

std::string SubAgentManager::executeSwarm(const std::string& parentId, const std::vector<std::string>& prompts,
                                          const SwarmConfig& config)
{
    std::vector<std::string> results;
    results.reserve(prompts.size());
    for (size_t i = 0; i < prompts.size(); ++i)
    {
        std::string agentId = spawnSubAgent(parentId, "swarm-" + std::to_string(i), prompts[i]);
        // Simulate execution
        {
            std::lock_guard<std::mutex> lock(g_subAgentMutex);
            auto it = g_subAgents.find(agentId);
            if (it != g_subAgents.end())
            {
                it->second.result = "[lane-b stub] swarm slot " + std::to_string(i) + "/" +
                                    std::to_string(prompts.size()) +
                                    " not executed (no engine hook): " + prompts[i].substr(0, 100);
                it->second.completed = true;
                results.push_back(it->second.result);
            }
        }
    }
    // Aggregate results
    std::ostringstream oss;
    oss << "Swarm results (" << results.size() << " agents):\n";
    for (size_t i = 0; i < results.size(); ++i)
    {
        oss << "  [" << i << "] " << results[i] << "\n";
    }
    return oss.str();
}

std::string SubAgentManager::getStatusSummary() const
{
    std::lock_guard<std::mutex> lock(g_subAgentMutex);
    size_t total = g_subAgents.size();
    size_t completed = 0;
    size_t cancelled = 0;
    for (const auto& [id, rec] : g_subAgents)
    {
        if (rec.completed)
            completed++;
        if (rec.cancelled)
            cancelled++;
    }
    std::ostringstream oss;
    oss << "SubAgentManager(active=" << (total - completed - cancelled) << ", completed=" << completed
        << ", cancelled=" << cancelled << ", total=" << total << ")";
    return oss.str();
}

bool SubAgentManager::dispatchToolCall(const std::string& parentId, const std::string& modelOutput,
                                       std::string& toolResult)
{
    (void)parentId;
    toolResult.clear();
    if (modelOutput.empty())
    {
        return false;
    }
    // Lane-B build does not link AgentToolRegistry; never pretend JSON-looking text was executed.
    toolResult =
        "[headless lane-b] tool dispatch not available (no ToolRegistry in this link). Model output was not run as a "
        "tool.\n";
    return false;
}

// -----------------------------------------------------------------------------
// Phase 17 profiler exports (C linkage) + C++ helpers used by headless bridge
// -----------------------------------------------------------------------------

// LAZY SINGLETON PATTERN: Avoid SIOF - std::atomic with non-trivial constructor
inline std::atomic<uint64_t>& GetAgenticEpochCount()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticEpochCount GetAgenticEpochCount()

inline std::atomic<uint64_t>& GetAgenticEpochStartTick()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticEpochStartTick GetAgenticEpochStartTick()

inline std::atomic<uint64_t>& GetAgenticTokenSamples()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticTokenSamples GetAgenticTokenSamples()

inline std::atomic<uint64_t>& GetAgenticToolStarts()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticToolStarts GetAgenticToolStarts()

inline std::atomic<uint64_t>& GetAgenticToolEnds()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticToolEnds GetAgenticToolEnds()

inline std::atomic<uint64_t>& GetAgenticToolOk()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticToolOk GetAgenticToolOk()

inline std::atomic<uint64_t>& GetAgenticToolFail()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticToolFail GetAgenticToolFail()

inline std::atomic<uint64_t>& GetAgenticDurAccum()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_agenticDurAccum GetAgenticDurAccum()

inline std::atomic<uint64_t>& GetLastToolHash()
{
    static std::atomic<uint64_t>* inst = new std::atomic<uint64_t>(0);
    return *inst;
}
#define g_lastToolHash GetLastToolHash()

extern "C" uint64_t RawrXD_Agentic_SampleProfileToken(uint32_t slot)
{
    (void)slot;
    g_agenticTokenSamples.fetch_add(1, std::memory_order_relaxed);
    return GetTickCount64();
}

extern "C" void AgenticProfilerBeginEpoch(void)
{
    g_agenticEpochCount.fetch_add(1, std::memory_order_relaxed);
    g_agenticEpochStartTick.store(GetTickCount64(), std::memory_order_relaxed);
}

extern "C" uint64_t AgenticProfilerGetElapsed(void)
{
    const uint64_t start = g_agenticEpochStartTick.load(std::memory_order_relaxed);
    if (start == 0)
    {
        return 0;
    }
    const uint64_t now = GetTickCount64();
    return (now >= start) ? (now - start) : 0;
}

bool AgenticNotifyToolStart(const char* toolName)
{
    g_agenticToolStarts.fetch_add(1, std::memory_order_relaxed);
    uint64_t h = 1469598103934665603ULL;
    if (toolName)
    {
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(toolName); *p; ++p)
        {
            h ^= static_cast<uint64_t>(*p);
            h *= 1099511628211ULL;
        }
    }
    g_lastToolHash.store(h, std::memory_order_relaxed);
    return true;
}

void AgenticNotifyToolEnd(bool success, uint32_t latencyMs)
{
    g_agenticToolEnds.fetch_add(1, std::memory_order_relaxed);
    if (success)
    {
        g_agenticToolOk.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        g_agenticToolFail.fetch_add(1, std::memory_order_relaxed);
    }
    g_agenticDurAccum.fetch_add(static_cast<uint64_t>(latencyMs), std::memory_order_relaxed);
}

std::string AgenticProfilerTopSummary(uint32_t maxTools)
{
    std::ostringstream oss;
    const uint64_t starts = g_agenticToolStarts.load(std::memory_order_relaxed);
    const uint64_t ends = g_agenticToolEnds.load(std::memory_order_relaxed);
    const uint64_t ok = g_agenticToolOk.load(std::memory_order_relaxed);
    const uint64_t fail = g_agenticToolFail.load(std::memory_order_relaxed);
    const uint64_t totalDur = g_agenticDurAccum.load(std::memory_order_relaxed);
    const uint64_t avgDur = ends ? (totalDur / ends) : 0;
    oss << "headless requested=" << maxTools << " elapsed_ticks=" << AgenticProfilerGetElapsed()
        << " epochs=" << g_agenticEpochCount.load(std::memory_order_relaxed) << " starts=" << starts << " ends=" << ends
        << " ok=" << ok << " fail=" << fail << " avg_latency_ms=" << avgDur
        << " token_samples=" << g_agenticTokenSamples.load(std::memory_order_relaxed)
        << " last_tool_hash=" << g_lastToolHash.load(std::memory_order_relaxed);
    return oss.str();
}

uint32_t Phase17Profiler::GetEpochCount()
{
    return static_cast<uint32_t>(g_agenticEpochCount.load(std::memory_order_relaxed));
}
