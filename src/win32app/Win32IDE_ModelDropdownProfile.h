#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace rawrxd {

struct ModelCapabilityProfile
{
    bool thinking = true;
    bool deepResearch = true;
    bool maxMode = true;
    bool swarm = true;
    bool browser = true;
    bool vision = false;
    bool video = false;
    std::string thinkingEffort = "Medium";
};

inline std::string ToLowerCopy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

inline std::string BaseModelDisplayName(const std::string& modelRef)
{
    if (modelRef.empty())
        return modelRef;

    std::error_code ec;
    std::filesystem::path p(modelRef);
    std::string leaf = p.filename().string();
    if (leaf.empty())
        leaf = modelRef;

    // Prefer model filename without extension for readability.
    if (p.has_extension())
        leaf = p.stem().string();

    return leaf;
}

inline ModelCapabilityProfile InferModelCapabilityProfile(const std::string& modelRef)
{
    ModelCapabilityProfile profile;
    const std::string lower = ToLowerCopy(modelRef);

    if (lower.find("gpt-5") != std::string::npos || lower.find("claude-opus") != std::string::npos ||
        lower.find("sonnet-4") != std::string::npos)
    {
        profile.thinkingEffort = "Xhigh";
        profile.vision = true;
        profile.browser = true;
    }
    else if (lower.find("4o") != std::string::npos || lower.find("gemini") != std::string::npos ||
             lower.find("phi3") != std::string::npos)
    {
        profile.thinkingEffort = "High";
        profile.vision = true;
    }
    else if (lower.find("mini") != std::string::npos || lower.find("haiku") != std::string::npos)
    {
        profile.thinkingEffort = "Low";
        profile.deepResearch = false;
        profile.video = false;
    }

    if (lower.find("video") != std::string::npos)
        profile.video = true;

    return profile;
}

inline std::string BuildCapabilityInlineLabel(const ModelCapabilityProfile& profile)
{
    std::vector<std::string> tags;
    if (profile.thinking)
        tags.push_back("Thinking");
    if (profile.deepResearch)
        tags.push_back("Deep Research");
    if (profile.maxMode)
        tags.push_back("Max Mode");
    if (profile.swarm)
        tags.push_back("Swarm");
    if (profile.browser)
        tags.push_back("Browser");
    if (profile.vision)
        tags.push_back("Vision");
    if (profile.video)
        tags.push_back("Video");

    std::string out;
    for (size_t i = 0; i < tags.size(); ++i)
    {
        if (i > 0)
            out += " / ";
        out += tags[i];
    }
    return out;
}

inline std::string BuildModelDropdownLabel(const std::string& modelRef)
{
    const std::string displayName = BaseModelDisplayName(modelRef);
    const ModelCapabilityProfile profile = InferModelCapabilityProfile(modelRef);
    return displayName + " - A Carrot > - " + BuildCapabilityInlineLabel(profile);
}

inline std::string BuildThinkingEffortSummary(const std::string& modelRef)
{
    const ModelCapabilityProfile profile = InferModelCapabilityProfile(modelRef);
    return std::string("Thinking Effort: ") + profile.thinkingEffort;
}

}  // namespace rawrxd
