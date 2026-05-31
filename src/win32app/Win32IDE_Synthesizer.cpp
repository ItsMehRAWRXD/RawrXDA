#include "Win32IDE.h"
#include "IDELogger.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Agentic {

namespace {
std::string ToLower(const std::string& input)
{
    std::string out = input;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool ContainsAny(const std::string& s, const std::vector<std::string>& needles)
{
    for (const auto& n : needles)
    {
        if (!n.empty() && s.find(n) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

size_t CountReplacementLikeNoise(const std::string& s)
{
    // UTF-8 replacement character bytes: EF BF BD
    size_t count = 0;
    for (size_t i = 0; i + 2 < s.size(); ++i)
    {
        const unsigned char b0 = static_cast<unsigned char>(s[i]);
        const unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
        if (b0 == 0xEFu && b1 == 0xBFu && b2 == 0xBDu)
        {
            ++count;
            i += 2;
        }
    }
    return count;
}

std::string NormalizeWhitespace(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    bool lastSpace = false;
    for (char ch : in)
    {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 && ch != '\n' && ch != '\r' && ch != '\t')
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
    return out;
}
}  // namespace

class Synthesizer {
public:
    static Synthesizer& GetInstance()
    {
        static Synthesizer instance;
        return instance;
    }

    std::string MergeProposals(const std::vector<std::string>& proposals)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (proposals.empty())
        {
            LOG_WARNING("[Synthesizer] Merge requested with empty proposal set");
            return {};
        }

        int bestScore = -1;
        size_t bestIndex = 0;

        for (size_t i = 0; i < proposals.size(); ++i)
        {
            const int score = ScoreProposal(proposals[i]);
            if (score > bestScore)
            {
                bestScore = score;
                bestIndex = i;
            }
        }

        std::string merged = NormalizeWhitespace(proposals[bestIndex]);
        if (!ValidateOutput(merged))
        {
            LOG_WARNING("[Synthesizer] Best proposal failed validation; falling back to first valid candidate");
            for (const auto& p : proposals)
            {
                std::string candidate = NormalizeWhitespace(p);
                if (ValidateOutput(candidate))
                {
                    merged = std::move(candidate);
                    break;
                }
            }
        }

        LOG_INFO("[Synthesizer] Merged " + std::to_string(proposals.size()) + " proposals; selected index=" +
                 std::to_string(bestIndex) + " score=" + std::to_string(bestScore));
        return merged;
    }

    bool ValidateOutput(const std::string& output)
    {
        if (output.empty())
        {
            return false;
        }

        const std::string lower = ToLower(output);

        if (ContainsAny(lower, {"[non-text backend payload", "no model selected", "failed to load", "exception"}))
        {
            return false;
        }

        const size_t replacementCount = CountReplacementLikeNoise(output);
        if (!output.empty() && (replacementCount * 100) / output.size() > 2)
        {
            return false;
        }

        size_t printable = 0;
        for (unsigned char c : output)
        {
            if ((c >= 0x20u && c <= 0x7Eu) || c == '\n' || c == '\r' || c == '\t')
            {
                ++printable;
            }
        }

        return printable >= 24;
    }

private:
    int ScoreProposal(const std::string& proposal) const
    {
        if (proposal.empty())
        {
            return 0;
        }

        const std::string lower = ToLower(proposal);
        int score = 0;

        if (proposal.size() >= 96)
            score += 20;
        if (proposal.size() >= 256)
            score += 15;

        if (ContainsAny(lower, {"plan", "step", "analysis", "result"}))
            score += 20;
        if (ContainsAny(lower, {"error", "exception", "failed"}))
            score -= 25;

        const size_t replacementNoise = CountReplacementLikeNoise(proposal);
        score -= static_cast<int>(replacementNoise * 50);

        return score;
    }

    Synthesizer() = default;

    std::mutex m_mutex;
};

}  // namespace RawrXD::Agentic

extern "C" const char* Synthesizer_Merge(const char** proposals, int count)
{
    static std::string result;
    std::vector<std::string> proposalList;

    if (proposals && count > 0)
    {
        proposalList.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const char* p = proposals[i];
            proposalList.emplace_back(p ? p : "");
        }
    }

    result = RawrXD::Agentic::Synthesizer::GetInstance().MergeProposals(proposalList);
    return result.c_str();
}

extern "C" bool Synthesizer_Validate(const char* output)
{
    return RawrXD::Agentic::Synthesizer::GetInstance().ValidateOutput(output ? std::string(output) : std::string());
}
