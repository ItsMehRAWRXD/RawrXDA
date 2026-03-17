// ============================================================================
// reasoning_cot_bridge.cpp — Bridge: ReasoningProfile → ChainOfThoughtEngine
// ============================================================================
//
// Connects the tunable ReasoningProfile/PipelineOrchestrator system to the
// existing ChainOfThoughtEngine (Phase 32A). When the orchestrator needs to
// invoke the CoT engine, it goes through this bridge to translate profile
// settings into CoT steps and presets.
//
// This file also provides the convenience API for one-shot "tunable CoT"
// execution: set a reasoning depth, and the bridge automatically configures
// the ChainOfThoughtEngine for you.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../include/reasoning_profile.h"
#include "../include/reasoning_pipeline_orchestrator.h"
#include "../include/chain_of_thought_engine.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// CoTRoleId lookup helper
// ============================================================================
namespace {

CoTRoleId roleNameToId(const std::string& name) {
    if (name == "thinker")         return CoTRoleId::Thinker;
    if (name == "critic")          return CoTRoleId::Critic;
    if (name == "auditor")         return CoTRoleId::Auditor;
    if (name == "researcher")      return CoTRoleId::Researcher;
    if (name == "debater_for")     return CoTRoleId::DebaterFor;
    if (name == "debater_against") return CoTRoleId::DebaterAgainst;
    if (name == "verifier")        return CoTRoleId::Verifier;
    if (name == "refiner")         return CoTRoleId::Refiner;
    if (name == "synthesizer")     return CoTRoleId::Synthesizer;
    if (name == "brainstorm")      return CoTRoleId::Brainstorm;
    if (name == "summarizer")      return CoTRoleId::Summarizer;
    return CoTRoleId::Thinker; // fallback
}

} // anonymous namespace

// ============================================================================
// ReasoningCoTBridge — the integration interface
// ============================================================================

class ReasoningCoTBridge {
public:
    static ReasoningCoTBridge& instance() {
        static ReasoningCoTBridge bridge;
        return bridge;
    }

    // Configure the ChainOfThoughtEngine based on the current ReasoningProfile.
    // Call this before executeChain() to apply profile settings.
    void configureCoTFromProfile() {
        auto& profMgr = ReasoningProfileManager::instance();
        ReasoningProfile profile = profMgr.getProfile();
        auto& cot = ChainOfThoughtEngine::instance();

        // Clear existing steps
        cot.clearSteps();

        // Build the agent chain from the profile at the configured depth
        auto& orchestrator = ReasoningPipelineOrchestrator::instance();
        InputComplexity complexity = InputComplexity::Moderate; // default assumption

        int effectiveDepth = profile.reasoning.reasoningDepth;
        std::vector<std::string> chain;

        // Build chain using the same logic as the orchestrator
        const auto& r = profile.reasoning;
        if (r.enableBrainstorm)  chain.push_back("brainstorm");
        if (r.enableResearcher)  chain.push_back("researcher");
        if (r.enableThinker)     chain.push_back("thinker");
        if (r.enableDebaters) {
            chain.push_back("debater_for");
            chain.push_back("debater_against");
        }
        if (r.enableCritic)      chain.push_back("critic");
        if (r.enableAuditor)     chain.push_back("auditor");
        if (r.enableVerifier)    chain.push_back("verifier");
        if (r.enableRefiner)     chain.push_back("refiner");
        if (r.enableSummarizer)  chain.push_back("summarizer");
        if (r.enableSynthesizer) chain.push_back("synthesizer");

        if (chain.empty()) {
            chain.push_back("thinker");
            chain.push_back("synthesizer");
        }

        // Truncate to effective depth
        if ((int)chain.size() > effectiveDepth && effectiveDepth > 0) {
            std::string last = chain.back();
            chain.resize(effectiveDepth);
            if (!chain.empty() && chain.back() != last) {
                chain.back() = last;
            }
        }

        // Set max steps on the CoT engine
        cot.setMaxSteps((int)chain.size());

        // Add each role as a CoT step
        for (const auto& roleName : chain) {
            CoTStep step;
            step.role = roleNameToId(roleName);
            step.skip = false;
            cot.addStep(step);
        }
    }

    // One-shot: configure + execute with current profile settings.
    // Returns a PipelineResult wrapping the CoT chain result.
    PipelineResult executeWithProfile(const std::string& userQuery,
                                       const std::string& context = "") {
        configureCoTFromProfile();

        auto& cot = ChainOfThoughtEngine::instance();
        CoTChainResult cotResult = cot.executeChain(userQuery);

        // Map CoT result to PipelineResult
        PipelineResult result;
        result.success = cotResult.success;
        result.finalAnswer = cotResult.finalOutput;
        result.totalLatencyMs = cotResult.totalLatencyMs;

        for (const auto& sr : cotResult.stepResults) {
            PipelineStepResult step;
            step.stepIndex = sr.stepIndex;
            step.agentRole = sr.roleName;
            step.content = sr.output;
            step.latencyMs = sr.latencyMs;
            step.tokenCount = sr.tokenCount;
            step.success = sr.success;
            step.skipped = sr.skipped;
            step.error = sr.error;
            result.steps.push_back(step);
        }

        if (!cotResult.success && !cotResult.error.empty()) {
            result.error = cotResult.error;
        }

        // Apply visibility formatting
        auto profile = ReasoningProfileManager::instance().getProfile();
        result.visible.finalAnswer = result.finalAnswer;
        result.visible.totalSteps = (int)result.steps.size();

        switch (profile.reasoning.visibility) {
        case ReasoningVisibility::FinalOnly:
            break;
        case ReasoningVisibility::ProgressBar:
            result.visible.showProgress = true;
            break;
        case ReasoningVisibility::StepSummary:
            result.visible.showProgress = true;
            for (const auto& s : result.steps) {
                if (s.success) {
                    std::string summary = s.agentRole + ": ";
                    if (s.content.size() > 80)
                        summary += s.content.substr(0, 80) + "...";
                    else
                        summary += s.content;
                    result.visible.stepSummaries.push_back(summary);
                }
            }
            break;
        case ReasoningVisibility::FullCoT:
            result.visible.showProgress = true;
            for (const auto& s : result.steps) {
                result.visible.fullCoTSteps.push_back(
                    "[" + s.agentRole + "]\n" + s.content);
            }
            break;
        default:
            break;
        }

        return result;
    }

    // Configure the profile from a preset name, then configure CoT.
    void applyPresetAndConfigure(const char* presetName) {
        ReasoningProfileManager::instance().applyPreset(presetName);
        configureCoTFromProfile();
    }

    // Adjust depth and reconfigure
    void setDepthAndConfigure(int depth) {
        ReasoningProfileManager::instance().setReasoningDepth(depth);
        configureCoTFromProfile();
    }

private:
    ReasoningCoTBridge() = default;
    ~ReasoningCoTBridge() = default;
    ReasoningCoTBridge(const ReasoningCoTBridge&) = delete;
    ReasoningCoTBridge& operator=(const ReasoningCoTBridge&) = delete;
};

// ============================================================================
// C-compatible API (for use from non-C++ contexts, server endpoints, etc.)
// ============================================================================

extern "C" {

// Set reasoning depth (0–8)
void rawrxd_set_reasoning_depth(int depth) {
    ReasoningProfileManager::instance().setReasoningDepth(depth);
}

// Get current reasoning depth
int rawrxd_get_reasoning_depth() {
    return ReasoningProfileManager::instance().getReasoningDepth();
}

// Apply a named preset: "fast", "normal", "deep", "critical", "swarm",
// "adaptive", "dev", "max"
void rawrxd_apply_reasoning_preset(const char* name) {
    ReasoningProfileManager::instance().applyPreset(name);
}

// Enable/disable adaptive mode
void rawrxd_set_adaptive_enabled(int enabled) {
    ReasoningProfileManager::instance().setAdaptiveEnabled(enabled != 0);
}

// Enable/disable thermal monitoring
void rawrxd_set_thermal_enabled(int enabled) {
    ReasoningProfileManager::instance().setThermalEnabled(enabled != 0);
}

// Enable/disable swarm reasoning
void rawrxd_set_swarm_enabled(int enabled) {
    ReasoningProfileManager::instance().setSwarmEnabled(enabled != 0);
}

// Enable/disable self-tuning
void rawrxd_set_self_tune_enabled(int enabled) {
    ReasoningProfileManager::instance().setSelfTuneEnabled(enabled != 0);
}

// Set CoT visibility: 0=FinalOnly, 1=ProgressBar, 2=StepSummary, 3=FullCoT
void rawrxd_set_cot_visibility(int visibility) {
    if (visibility < 0 || visibility >= (int)ReasoningVisibility::Count) return;
    ReasoningProfileManager::instance().setVisibility((ReasoningVisibility)visibility);
}

// Set reasoning mode: 0=Fast, 1=Normal, 2=Deep, 3=Critical, 4=Swarm, 5=Adaptive, 6=DevDebug
void rawrxd_set_reasoning_mode(int mode) {
    if (mode < 0 || mode >= (int)ReasoningMode::Count) return;
    ReasoningProfileManager::instance().setMode((ReasoningMode)mode);
}

// Set swarm agent count (2–16)
void rawrxd_set_swarm_agent_count(int count) {
    ReasoningProfileManager::instance().setSwarmAgentCount(count);
}

// Set latency target in ms
void rawrxd_set_latency_target(double ms) {
    ReasoningProfileManager::instance().setLatencyTarget(ms);
}

// Save current profile to file
int rawrxd_save_reasoning_profile(const char* path) {
    return ReasoningProfileManager::instance().saveProfileToFile(path) ? 1 : 0;
}

// Load profile from file
int rawrxd_load_reasoning_profile(const char* path) {
    return ReasoningProfileManager::instance().loadProfileFromFile(path) ? 1 : 0;
}

// Start thermal monitor thread
void rawrxd_start_thermal_monitor() {
    ReasoningPipelineOrchestrator::instance().startThermalMonitor();
}

// Stop thermal monitor thread
void rawrxd_stop_thermal_monitor() {
    ReasoningPipelineOrchestrator::instance().stopThermalMonitor();
}

} // extern "C"
