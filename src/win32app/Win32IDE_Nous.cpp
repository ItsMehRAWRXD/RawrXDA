#include "Win32IDE.h"
#include "IDELogger.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD::Autonomy {

struct PlanStep {
    std::string description;
    uint32_t priority;
    bool is_critical;
};

namespace {
std::string NormalizeGoalText(const std::string& input)
{
    std::string out;
    out.reserve(input.size());

    bool lastSpace = false;
    for (char ch : input)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20)
        {
            continue;
        }

        const bool isSpace = (ch == ' ' || ch == '\t');
        if (isSpace)
        {
            if (lastSpace)
            {
                continue;
            }
            out.push_back(' ');
            lastSpace = true;
        }
        else
        {
            out.push_back(ch);
            lastSpace = false;
        }
    }

    while (!out.empty() && out.front() == ' ')
    {
        out.erase(out.begin());
    }
    while (!out.empty() && out.back() == ' ')
    {
        out.pop_back();
    }

    return out;
}

bool ContainsWord(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
    {
        return false;
    }
    return haystack.find(needle) != std::string::npos;
}
}  // namespace

class Nous {
public:
    static Nous& GetInstance()
    {
        static Nous instance;
        return instance;
    }

    bool IngestGoal(const std::string& goal_id, const std::string& description)
    {
        const std::string normalizedGoalId = NormalizeGoalText(goal_id);
        const std::string normalizedDescription = NormalizeGoalText(description);

        if (normalizedGoalId.empty() || normalizedDescription.empty())
        {
            LOG_WARNING("[Nous] Rejecting empty goal input");
            return false;
        }

        GoalRecord record;
        record.goalId = normalizedGoalId;
        record.description = normalizedDescription;
        record.createdAt = std::chrono::steady_clock::now();
        record.revision++;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto& slot = m_goals[normalizedGoalId];
            if (!slot.goalId.empty())
            {
                record.revision = slot.revision + 1;
            }
            m_goals[normalizedGoalId] = record;
        }

        LOG_INFO("[Nous] Ingested goal " + normalizedGoalId + " (rev=" + std::to_string(record.revision) + ")");
        return true;
    }

    std::vector<PlanStep> DecomposePlan(const std::string& goal_id)
    {
        const std::string normalizedGoalId = NormalizeGoalText(goal_id);
        if (normalizedGoalId.empty())
        {
            return {};
        }

        GoalRecord record;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_goals.find(normalizedGoalId);
            if (it == m_goals.end())
            {
                return {};
            }
            record = it->second;
        }

        const std::string lower = ToLower(record.description);
        std::vector<PlanStep> steps;
        steps.reserve(8);

        // Step 1: always define objective and boundaries first.
        steps.push_back({"Capture objective constraints and acceptance criteria", 1u, true});

        // Goal-adaptive decomposition keeps planning deterministic and cheap.
        if (ContainsWord(lower, "build") || ContainsWord(lower, "compile") || ContainsWord(lower, "link"))
        {
            steps.push_back({"Resolve unresolved symbols and dependency graph", 2u, true});
            steps.push_back({"Run targeted build lane and collect diagnostics", 3u, true});
        }
        if (ContainsWord(lower, "smoke") || ContainsWord(lower, "test") || ContainsWord(lower, "validate"))
        {
            steps.push_back({"Execute strict smoke scenarios and capture failure causes", 4u, true});
        }
        if (ContainsWord(lower, "model") || ContainsWord(lower, "inference") || ContainsWord(lower, "agentic"))
        {
            steps.push_back({"Probe inference pipeline for token and output integrity", 5u, true});
        }

        steps.push_back({"Persist checkpoint and telemetry for replay", 6u, false});
        steps.push_back({"Emit final report with pass/fail gates", 7u, true});

        return steps;
    }

private:
    struct GoalRecord {
        std::string goalId;
        std::string description;
        std::chrono::steady_clock::time_point createdAt{};
        uint32_t revision = 0;
    };

    static std::string ToLower(const std::string& input)
    {
        std::string out = input;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    Nous() = default;

    std::mutex m_mutex;
    std::unordered_map<std::string, GoalRecord> m_goals;
};

}  // namespace RawrXD::Autonomy

extern "C" bool Nous_IngestGoal(const char* goal_id, const char* description)
{
    if (!goal_id || !description)
    {
        return false;
    }
    return RawrXD::Autonomy::Nous::GetInstance().IngestGoal(goal_id, description);
}

extern "C" void* Nous_DecomposePlan(const char* goal_id)
{
    if (!goal_id)
    {
        return nullptr;
    }

    auto* steps = new std::vector<RawrXD::Autonomy::PlanStep>(
        RawrXD::Autonomy::Nous::GetInstance().DecomposePlan(goal_id));

    if (steps->empty())
    {
        delete steps;
        return nullptr;
    }

    return static_cast<void*>(steps);
}

extern "C" void Nous_FreeDecomposedPlan(void* opaque_steps)
{
    auto* steps = static_cast<std::vector<RawrXD::Autonomy::PlanStep>*>(opaque_steps);
    delete steps;
}
