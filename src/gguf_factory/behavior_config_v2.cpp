#include "behavior_config_v2.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace RawrXD
{
namespace
{

static inline std::string toLower_(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static inline bool parseBool_(const std::unordered_map<std::string, std::string>& kv, const std::string& key, bool def)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return def;
    const std::string v = toLower_(it->second);
    return (v == "1" || v == "true" || v == "yes" || v == "on");
}

static inline int parseInt_(const std::unordered_map<std::string, std::string>& kv, const std::string& key, int def)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return def;
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return def;
    }
}

static inline float parseFloat_(const std::unordered_map<std::string, std::string>& kv, const std::string& key,
                                float def)
{
    const auto it = kv.find(key);
    if (it == kv.end())
        return def;
    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        return def;
    }
}

static inline std::string parseString_(const std::unordered_map<std::string, std::string>& kv, const std::string& key,
                                       const std::string& def)
{
    const auto it = kv.find(key);
    return (it == kv.end()) ? def : it->second;
}

static inline std::vector<std::string> parseCsv_(const std::unordered_map<std::string, std::string>& kv,
                                                 const std::string& key)
{
    std::vector<std::string> out;
    const auto it = kv.find(key);
    if (it == kv.end())
        return out;
    std::istringstream iss(it->second);
    std::string item;
    while (std::getline(iss, item, ','))
    {
        // trim
        while (!item.empty() && std::isspace((unsigned char)item.front()))
            item.erase(item.begin());
        while (!item.empty() && std::isspace((unsigned char)item.back()))
            item.pop_back();
        if (!item.empty())
            out.push_back(item);
    }
    return out;
}

static inline std::vector<float> parseCsvFloats_(const std::unordered_map<std::string, std::string>& kv,
                                                 const std::string& key)
{
    std::vector<float> out;
    const auto it = kv.find(key);
    if (it == kv.end())
        return out;
    std::istringstream iss(it->second);
    std::string item;
    while (std::getline(iss, item, ','))
    {
        // trim
        while (!item.empty() && std::isspace((unsigned char)item.front()))
            item.erase(item.begin());
        while (!item.empty() && std::isspace((unsigned char)item.back()))
            item.pop_back();
        if (item.empty())
            continue;
        try
        {
            out.push_back(std::stof(item));
        }
        catch (...)
        {
        }
    }
    return out;
}

static inline std::string joinCsv_(const std::vector<std::string>& v)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            out += ",";
        out += v[i];
    }
    return out;
}

static inline std::string joinCsvFloats_(const std::vector<float>& v)
{
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            oss << ",";
        oss << v[i];
    }
    return oss.str();
}

}  // namespace

std::unordered_map<std::string, std::string> BehaviorConfigV2::to_kv_pairs() const
{
    auto kv = BehaviorConfig::to_kv_pairs();

    kv["sovereign.meta_cognitive.confidence_calibration"] = std::to_string(self_model.confidence_calibration);
    kv["sovereign.meta_cognitive.uncertainty_per_domain"] = joinCsvFloats_(self_model.uncertainty_per_domain);
    kv["sovereign.meta_cognitive.meta_reasoning_depth"] = std::to_string(self_model.meta_reasoning_depth);
    kv["sovereign.meta_cognitive.uncertainty_quantification"] = self_model.uncertainty_quantification ? "1" : "0";
    kv["sovereign.meta_cognitive.mistake_prediction"] = self_model.mistake_prediction ? "1" : "0";
    kv["sovereign.meta_cognitive.failure_mode_taxonomy"] = self_model.failure_mode_taxonomy;

    kv["sovereign.confidence.propagate"] = propagate_confidence ? "1" : "0";
    kv["sovereign.confidence.minimum_threshold"] = std::to_string(minimum_confidence_threshold);
    kv["sovereign.confidence.refuse_low"] = refuse_low_confidence ? "1" : "0";
    kv["sovereign.confidence.explain_uncertainty"] = explain_uncertainty ? "1" : "0";

    kv["sovereign.adversarial.enabled"] = adversarial.enabled ? "1" : "0";
    kv["sovereign.adversarial.attack_vectors"] = std::to_string(adversarial.attack_vectors);
    kv["sovereign.adversarial.attack_types"] = joinCsv_(adversarial.attack_types);
    kv["sovereign.adversarial.auto_patch"] = adversarial.auto_patch ? "1" : "0";
    kv["sovereign.adversarial.escalate_to_human"] = adversarial.escalate_to_human ? "1" : "0";
    kv["sovereign.adversarial.attack_budget"] = std::to_string(adversarial.attack_budget);
    kv["sovereign.adversarial.persist_attacks"] = adversarial.persist_attacks ? "1" : "0";

    kv["sovereign.temporal.enabled"] = temporal.enabled ? "1" : "0";
    kv["sovereign.temporal.max_branches"] = std::to_string(temporal.max_branches);
    kv["sovereign.temporal.causal_tracking"] = temporal.causal_tracking ? "1" : "0";
    kv["sovereign.temporal.predict_future_states"] = temporal.predict_future_states ? "1" : "0";
    kv["sovereign.temporal.learn_from_abandoned_paths"] = temporal.learn_from_abandoned_paths ? "1" : "0";
    kv["sovereign.temporal.divergence_strategy"] = temporal.divergence_strategy;

    kv["sovereign.speculative.enabled"] = speculative.enabled ? "1" : "0";
    kv["sovereign.speculative.depth"] = std::to_string(speculative.speculation_depth);
    kv["sovereign.speculative.execute_hypotheses"] = speculative.execute_hypotheses ? "1" : "0";
    kv["sovereign.speculative.confidence_threshold"] = std::to_string(speculative.confidence_threshold);
    kv["sovereign.speculative.domains"] = joinCsv_(speculative.speculation_domains);
    kv["sovereign.speculative.cache_results"] = speculative.cache_speculative_results ? "1" : "0";
    kv["sovereign.speculative.discard_low_utility"] = speculative.discard_low_utility ? "1" : "0";

    kv["sovereign.intention.deep_intent_modeling"] = intention.deep_intent_modeling ? "1" : "0";
    kv["sovereign.intention.infer_implicit_requirements"] = intention.infer_implicit_requirements ? "1" : "0";
    kv["sovereign.intention.detect_intent_drift"] = intention.detect_intent_drift ? "1" : "0";
    kv["sovereign.intention.maintain_goal_stack"] = intention.maintain_goal_stack ? "1" : "0";
    kv["sovereign.intention.max_inference_depth"] = std::to_string(intention.max_inference_depth);
    kv["sovereign.intention.propose_higher_goals"] = intention.propose_higher_goals ? "1" : "0";

    kv["sovereign.synthesis.enabled"] = synthesis.enabled ? "1" : "0";
    kv["sovereign.synthesis.learn_project_patterns"] = synthesis.learn_project_patterns ? "1" : "0";
    kv["sovereign.synthesis.synthesize_idioms"] = synthesis.synthesize_idioms ? "1" : "0";
    kv["sovereign.synthesis.cross_language_transfer"] = synthesis.cross_language_transfer ? "1" : "0";
    kv["sovereign.synthesis.evolve_patterns"] = synthesis.evolve_patterns ? "1" : "0";
    kv["sovereign.synthesis.pattern_abstraction_level"] = std::to_string(synthesis.pattern_abstraction_level);

    kv["sovereign.cognitive_offload.maintain_context_graph"] = cognitive_offload.maintain_context_graph ? "1" : "0";
    kv["sovereign.cognitive_offload.auto_summarize"] = cognitive_offload.auto_summarize ? "1" : "0";
    kv["sovereign.cognitive_offload.semantic_chunking"] = cognitive_offload.semantic_chunking ? "1" : "0";
    kv["sovereign.cognitive_offload.attention_management"] = cognitive_offload.attention_management ? "1" : "0";
    kv["sovereign.cognitive_offload.max_items"] = std::to_string(cognitive_offload.max_working_memory_items);
    kv["sovereign.cognitive_offload.forgetting_enabled"] = cognitive_offload.forgetting_enabled ? "1" : "0";
    kv["sovereign.cognitive_offload.forgetting_threshold"] = std::to_string(cognitive_offload.forgetting_threshold);

    kv["sovereign.dialogue.enabled"] = dialogue.enabled ? "1" : "0";
    kv["sovereign.dialogue.agent_personas"] = joinCsv_(dialogue.agent_personas);
    kv["sovereign.dialogue.visible_dialogue"] = dialogue.visible_dialogue ? "1" : "0";
    kv["sovereign.dialogue.max_turns_per_agent"] = std::to_string(dialogue.max_turns_per_agent);
    kv["sovereign.dialogue.require_consensus"] = dialogue.require_consensus ? "1" : "0";
    kv["sovereign.dialogue.minority_report"] = dialogue.minority_report ? "1" : "0";
    kv["sovereign.dialogue.resolution_strategy"] = dialogue.resolution_strategy;

    kv["sovereign.constraints.enabled"] = constraints.enabled ? "1" : "0";
    kv["sovereign.constraints.infer_implicit"] = constraints.infer_implicit_constraints ? "1" : "0";
    kv["sovereign.constraints.detect_violations"] = constraints.detect_constraint_violations ? "1" : "0";
    kv["sovereign.constraints.suggest_repairs"] = constraints.suggest_constraint_repairs ? "1" : "0";
    kv["sovereign.constraints.temporal_constraints"] = constraints.temporal_constraints ? "1" : "0";
    kv["sovereign.constraints.types"] = joinCsv_(constraints.constraint_types);

    kv["sovereign.semantic_diff.enabled"] = semantic_diff.enabled ? "1" : "0";
    kv["sovereign.semantic_diff.detect_intent_changes"] = semantic_diff.detect_intent_changes ? "1" : "0";
    kv["sovereign.semantic_diff.group_related_changes"] = semantic_diff.group_related_changes ? "1" : "0";
    kv["sovereign.semantic_diff.detect_refactoring_patterns"] = semantic_diff.detect_refactoring_patterns ? "1" : "0";
    kv["sovereign.semantic_diff.predict_merge_conflicts"] = semantic_diff.predict_merge_conflicts ? "1" : "0";
    kv["sovereign.semantic_diff.suggest_merge_strategies"] = semantic_diff.suggest_merge_strategies ? "1" : "0";
    kv["sovereign.semantic_diff.similarity_threshold"] = std::to_string(semantic_diff.semantic_similarity_threshold);

    kv["sovereign.predictive.enabled"] = predictive.enabled ? "1" : "0";
    kv["sovereign.predictive.predict_technical_debt"] = predictive.predict_technical_debt ? "1" : "0";
    kv["sovereign.predictive.predict_performance_decay"] = predictive.predict_performance_decay ? "1" : "0";
    kv["sovereign.predictive.predict_security_drift"] = predictive.predict_security_drift ? "1" : "0";
    kv["sovereign.predictive.horizon_days"] = std::to_string(predictive.prediction_horizon);
    kv["sovereign.predictive.confidence_intervals"] = predictive.confidence_intervals ? "1" : "0";
    kv["sovereign.predictive.maintenance_strategy"] = predictive.maintenance_strategy;

    kv["sovereign.absence.enabled"] = absence.enabled ? "1" : "0";
    kv["sovereign.absence.detect_missing_tests"] = absence.detect_missing_tests ? "1" : "0";
    kv["sovereign.absence.detect_missing_error_handling"] = absence.detect_missing_error_handling ? "1" : "0";
    kv["sovereign.absence.detect_missing_documentation"] = absence.detect_missing_documentation ? "1" : "0";
    kv["sovereign.absence.detect_missing_security_checks"] = absence.detect_missing_security_checks ? "1" : "0";
    kv["sovereign.absence.detect_missing_edge_cases"] = absence.detect_missing_edge_cases ? "1" : "0";
    kv["sovereign.absence.confidence_threshold"] = std::to_string(absence.absence_confidence_threshold);

    kv["sovereign.causal.enabled"] = causal.enabled ? "1" : "0";
    kv["sovereign.causal.max_depth"] = std::to_string(causal.max_causal_depth);
    kv["sovereign.causal.infer_hidden_causes"] = causal.infer_hidden_causes ? "1" : "0";
    kv["sovereign.causal.detect_confounders"] = causal.detect_confounding_factors ? "1" : "0";
    kv["sovereign.causal.propose_interventions"] = causal.propose_interventions ? "1" : "0";
    kv["sovereign.causal.track_certainty"] = causal.track_causal_certainty ? "1" : "0";

    kv["sovereign.versioning.enabled"] = versioning.enabled ? "1" : "0";
    kv["sovereign.versioning.track_intent_versioning"] = versioning.track_intent_versioning ? "1" : "0";
    kv["sovereign.versioning.detect_breaking_changes"] = versioning.detect_breaking_changes ? "1" : "0";
    kv["sovereign.versioning.suggest_shims"] = versioning.suggest_compatibility_shims ? "1" : "0";
    kv["sovereign.versioning.maintain_graph"] = versioning.maintain_version_graph ? "1" : "0";
    kv["sovereign.versioning.strategy"] = versioning.versioning_strategy;

    kv["sovereign.environment.enabled"] = environment.enabled ? "1" : "0";
    kv["sovereign.environment.infer_dependencies"] = environment.infer_dependencies ? "1" : "0";
    kv["sovereign.environment.detect_version_conflicts"] = environment.detect_version_conflicts ? "1" : "0";
    kv["sovereign.environment.generate_dockerfile"] = environment.generate_dockerfile ? "1" : "0";
    kv["sovereign.environment.generate_ci_pipeline"] = environment.generate_ci_pipeline ? "1" : "0";
    kv["sovereign.environment.suggest_strategies"] = environment.suggest_environment_strategies ? "1" : "0";
    kv["sovereign.environment.test_reproducibility"] = environment.test_environment_reproducibility ? "1" : "0";

    return kv;
}

BehaviorConfigV2 BehaviorConfigV2::from_kv_pairs(const std::unordered_map<std::string, std::string>& kv)
{
    BehaviorConfigV2 cfg;

    cfg.self_model.confidence_calibration = parseFloat_(kv, "sovereign.meta_cognitive.confidence_calibration", 0.0f);
    cfg.self_model.uncertainty_per_domain = parseCsvFloats_(kv, "sovereign.meta_cognitive.uncertainty_per_domain");
    cfg.self_model.meta_reasoning_depth = parseInt_(kv, "sovereign.meta_cognitive.meta_reasoning_depth", 0);
    cfg.self_model.uncertainty_quantification =
        parseBool_(kv, "sovereign.meta_cognitive.uncertainty_quantification", true);
    cfg.self_model.mistake_prediction = parseBool_(kv, "sovereign.meta_cognitive.mistake_prediction", false);
    cfg.self_model.failure_mode_taxonomy = parseString_(kv, "sovereign.meta_cognitive.failure_mode_taxonomy", "");

    cfg.propagate_confidence = parseBool_(kv, "sovereign.confidence.propagate", true);
    cfg.minimum_confidence_threshold = parseFloat_(kv, "sovereign.confidence.minimum_threshold", 0.7f);
    cfg.refuse_low_confidence = parseBool_(kv, "sovereign.confidence.refuse_low", true);
    cfg.explain_uncertainty = parseBool_(kv, "sovereign.confidence.explain_uncertainty", true);

    cfg.adversarial.enabled = parseBool_(kv, "sovereign.adversarial.enabled", false);
    cfg.adversarial.attack_vectors = parseInt_(kv, "sovereign.adversarial.attack_vectors", 5);
    cfg.adversarial.attack_types = parseCsv_(kv, "sovereign.adversarial.attack_types");
    cfg.adversarial.auto_patch = parseBool_(kv, "sovereign.adversarial.auto_patch", true);
    cfg.adversarial.escalate_to_human = parseBool_(kv, "sovereign.adversarial.escalate_to_human", true);
    cfg.adversarial.attack_budget = parseFloat_(kv, "sovereign.adversarial.attack_budget", 0.3f);
    cfg.adversarial.persist_attacks = parseBool_(kv, "sovereign.adversarial.persist_attacks", false);

    cfg.temporal.enabled = parseBool_(kv, "sovereign.temporal.enabled", false);
    cfg.temporal.max_branches = parseInt_(kv, "sovereign.temporal.max_branches", 10);
    cfg.temporal.causal_tracking = parseBool_(kv, "sovereign.temporal.causal_tracking", true);
    cfg.temporal.predict_future_states = parseBool_(kv, "sovereign.temporal.predict_future_states", true);
    cfg.temporal.learn_from_abandoned_paths = parseBool_(kv, "sovereign.temporal.learn_from_abandoned_paths", true);
    cfg.temporal.divergence_strategy = parseString_(kv, "sovereign.temporal.divergence_strategy", "semantic");

    cfg.speculative.enabled = parseBool_(kv, "sovereign.speculative.enabled", false);
    cfg.speculative.speculation_depth = parseInt_(kv, "sovereign.speculative.depth", 3);
    cfg.speculative.execute_hypotheses = parseBool_(kv, "sovereign.speculative.execute_hypotheses", false);
    cfg.speculative.confidence_threshold = parseFloat_(kv, "sovereign.speculative.confidence_threshold", 0.85f);
    cfg.speculative.speculation_domains = parseCsv_(kv, "sovereign.speculative.domains");
    cfg.speculative.cache_speculative_results = parseBool_(kv, "sovereign.speculative.cache_results", true);
    cfg.speculative.discard_low_utility = parseBool_(kv, "sovereign.speculative.discard_low_utility", true);

    cfg.intention.deep_intent_modeling = parseBool_(kv, "sovereign.intention.deep_intent_modeling", false);
    cfg.intention.infer_implicit_requirements = parseBool_(kv, "sovereign.intention.infer_implicit_requirements", true);
    cfg.intention.detect_intent_drift = parseBool_(kv, "sovereign.intention.detect_intent_drift", true);
    cfg.intention.maintain_goal_stack = parseBool_(kv, "sovereign.intention.maintain_goal_stack", true);
    cfg.intention.max_inference_depth = parseInt_(kv, "sovereign.intention.max_inference_depth", 5);
    cfg.intention.propose_higher_goals = parseBool_(kv, "sovereign.intention.propose_higher_goals", false);

    cfg.synthesis.enabled = parseBool_(kv, "sovereign.synthesis.enabled", false);
    cfg.synthesis.learn_project_patterns = parseBool_(kv, "sovereign.synthesis.learn_project_patterns", true);
    cfg.synthesis.synthesize_idioms = parseBool_(kv, "sovereign.synthesis.synthesize_idioms", true);
    cfg.synthesis.cross_language_transfer = parseBool_(kv, "sovereign.synthesis.cross_language_transfer", false);
    cfg.synthesis.evolve_patterns = parseBool_(kv, "sovereign.synthesis.evolve_patterns", true);
    cfg.synthesis.pattern_abstraction_level = parseInt_(kv, "sovereign.synthesis.pattern_abstraction_level", 2);

    cfg.cognitive_offload.maintain_context_graph =
        parseBool_(kv, "sovereign.cognitive_offload.maintain_context_graph", true);
    cfg.cognitive_offload.auto_summarize = parseBool_(kv, "sovereign.cognitive_offload.auto_summarize", true);
    cfg.cognitive_offload.semantic_chunking = parseBool_(kv, "sovereign.cognitive_offload.semantic_chunking", true);
    cfg.cognitive_offload.attention_management =
        parseBool_(kv, "sovereign.cognitive_offload.attention_management", true);
    cfg.cognitive_offload.max_working_memory_items = parseInt_(kv, "sovereign.cognitive_offload.max_items", 100);
    cfg.cognitive_offload.forgetting_enabled = parseBool_(kv, "sovereign.cognitive_offload.forgetting_enabled", true);
    cfg.cognitive_offload.forgetting_threshold =
        parseFloat_(kv, "sovereign.cognitive_offload.forgetting_threshold", 0.1f);

    cfg.dialogue.enabled = parseBool_(kv, "sovereign.dialogue.enabled", false);
    cfg.dialogue.agent_personas = parseCsv_(kv, "sovereign.dialogue.agent_personas");
    cfg.dialogue.visible_dialogue = parseBool_(kv, "sovereign.dialogue.visible_dialogue", false);
    cfg.dialogue.max_turns_per_agent = parseInt_(kv, "sovereign.dialogue.max_turns_per_agent", 3);
    cfg.dialogue.require_consensus = parseBool_(kv, "sovereign.dialogue.require_consensus", true);
    cfg.dialogue.minority_report = parseBool_(kv, "sovereign.dialogue.minority_report", true);
    cfg.dialogue.resolution_strategy = parseString_(kv, "sovereign.dialogue.resolution_strategy", "vote");

    cfg.constraints.enabled = parseBool_(kv, "sovereign.constraints.enabled", true);
    cfg.constraints.infer_implicit_constraints = parseBool_(kv, "sovereign.constraints.infer_implicit", true);
    cfg.constraints.detect_constraint_violations = parseBool_(kv, "sovereign.constraints.detect_violations", true);
    cfg.constraints.suggest_constraint_repairs = parseBool_(kv, "sovereign.constraints.suggest_repairs", true);
    cfg.constraints.temporal_constraints = parseBool_(kv, "sovereign.constraints.temporal_constraints", false);
    cfg.constraints.constraint_types = parseCsv_(kv, "sovereign.constraints.types");

    cfg.semantic_diff.enabled = parseBool_(kv, "sovereign.semantic_diff.enabled", true);
    cfg.semantic_diff.detect_intent_changes = parseBool_(kv, "sovereign.semantic_diff.detect_intent_changes", true);
    cfg.semantic_diff.group_related_changes = parseBool_(kv, "sovereign.semantic_diff.group_related_changes", true);
    cfg.semantic_diff.detect_refactoring_patterns =
        parseBool_(kv, "sovereign.semantic_diff.detect_refactoring_patterns", true);
    cfg.semantic_diff.predict_merge_conflicts = parseBool_(kv, "sovereign.semantic_diff.predict_merge_conflicts", true);
    cfg.semantic_diff.suggest_merge_strategies =
        parseBool_(kv, "sovereign.semantic_diff.suggest_merge_strategies", true);
    cfg.semantic_diff.semantic_similarity_threshold =
        parseFloat_(kv, "sovereign.semantic_diff.similarity_threshold", 0.7f);

    cfg.predictive.enabled = parseBool_(kv, "sovereign.predictive.enabled", false);
    cfg.predictive.predict_technical_debt = parseBool_(kv, "sovereign.predictive.predict_technical_debt", true);
    cfg.predictive.predict_performance_decay = parseBool_(kv, "sovereign.predictive.predict_performance_decay", true);
    cfg.predictive.predict_security_drift = parseBool_(kv, "sovereign.predictive.predict_security_drift", true);
    cfg.predictive.prediction_horizon = parseInt_(kv, "sovereign.predictive.horizon_days", 90);
    cfg.predictive.confidence_intervals = parseBool_(kv, "sovereign.predictive.confidence_intervals", true);
    cfg.predictive.maintenance_strategy = parseString_(kv, "sovereign.predictive.maintenance_strategy", "proactive");

    cfg.absence.enabled = parseBool_(kv, "sovereign.absence.enabled", true);
    cfg.absence.detect_missing_tests = parseBool_(kv, "sovereign.absence.detect_missing_tests", true);
    cfg.absence.detect_missing_error_handling = parseBool_(kv, "sovereign.absence.detect_missing_error_handling", true);
    cfg.absence.detect_missing_documentation = parseBool_(kv, "sovereign.absence.detect_missing_documentation", true);
    cfg.absence.detect_missing_security_checks =
        parseBool_(kv, "sovereign.absence.detect_missing_security_checks", true);
    cfg.absence.detect_missing_edge_cases = parseBool_(kv, "sovereign.absence.detect_missing_edge_cases", true);
    cfg.absence.absence_confidence_threshold = parseFloat_(kv, "sovereign.absence.confidence_threshold", 0.6f);

    cfg.causal.enabled = parseBool_(kv, "sovereign.causal.enabled", false);
    cfg.causal.max_causal_depth = parseInt_(kv, "sovereign.causal.max_depth", 10);
    cfg.causal.infer_hidden_causes = parseBool_(kv, "sovereign.causal.infer_hidden_causes", true);
    cfg.causal.detect_confounding_factors = parseBool_(kv, "sovereign.causal.detect_confounders", true);
    cfg.causal.propose_interventions = parseBool_(kv, "sovereign.causal.propose_interventions", true);
    cfg.causal.track_causal_certainty = parseBool_(kv, "sovereign.causal.track_certainty", true);

    cfg.versioning.enabled = parseBool_(kv, "sovereign.versioning.enabled", false);
    cfg.versioning.track_intent_versioning = parseBool_(kv, "sovereign.versioning.track_intent_versioning", true);
    cfg.versioning.detect_breaking_changes = parseBool_(kv, "sovereign.versioning.detect_breaking_changes", true);
    cfg.versioning.suggest_compatibility_shims = parseBool_(kv, "sovereign.versioning.suggest_shims", true);
    cfg.versioning.maintain_version_graph = parseBool_(kv, "sovereign.versioning.maintain_graph", true);
    cfg.versioning.versioning_strategy = parseString_(kv, "sovereign.versioning.strategy", "semantic");

    cfg.environment.enabled = parseBool_(kv, "sovereign.environment.enabled", false);
    cfg.environment.infer_dependencies = parseBool_(kv, "sovereign.environment.infer_dependencies", true);
    cfg.environment.detect_version_conflicts = parseBool_(kv, "sovereign.environment.detect_version_conflicts", true);
    cfg.environment.generate_dockerfile = parseBool_(kv, "sovereign.environment.generate_dockerfile", true);
    cfg.environment.generate_ci_pipeline = parseBool_(kv, "sovereign.environment.generate_ci_pipeline", true);
    cfg.environment.suggest_environment_strategies = parseBool_(kv, "sovereign.environment.suggest_strategies", true);
    cfg.environment.test_environment_reproducibility =
        parseBool_(kv, "sovereign.environment.test_reproducibility", true);

    return cfg;
}

bool BehaviorConfigV2::validate(std::string* error) const
{
    if (!(minimum_confidence_threshold >= 0.0f && minimum_confidence_threshold <= 1.0f))
    {
        if (error)
            *error = "minimum_confidence_threshold out of range [0,1]";
        return false;
    }
    if (adversarial.attack_vectors < 0)
    {
        if (error)
            *error = "adversarial.attack_vectors must be >= 0";
        return false;
    }
    if (!(adversarial.attack_budget >= 0.0f && adversarial.attack_budget <= 1.0f))
    {
        if (error)
            *error = "adversarial.attack_budget out of range [0,1]";
        return false;
    }
    if (temporal.max_branches < 0)
    {
        if (error)
            *error = "temporal.max_branches must be >= 0";
        return false;
    }
    if (speculative.speculation_depth < 0)
    {
        if (error)
            *error = "speculative.speculation_depth must be >= 0";
        return false;
    }
    if (!(speculative.confidence_threshold >= 0.0f && speculative.confidence_threshold <= 1.0f))
    {
        if (error)
            *error = "speculative.confidence_threshold out of range [0,1]";
        return false;
    }
    if (intention.max_inference_depth < 0)
    {
        if (error)
            *error = "intention.max_inference_depth must be >= 0";
        return false;
    }
    if (synthesis.pattern_abstraction_level < 0)
    {
        if (error)
            *error = "synthesis.pattern_abstraction_level must be >= 0";
        return false;
    }
    if (cognitive_offload.max_working_memory_items < 0)
    {
        if (error)
            *error = "cognitive_offload.max_working_memory_items must be >= 0";
        return false;
    }
    if (!(cognitive_offload.forgetting_threshold >= 0.0f && cognitive_offload.forgetting_threshold <= 1.0f))
    {
        if (error)
            *error = "cognitive_offload.forgetting_threshold out of range [0,1]";
        return false;
    }
    if (!(semantic_diff.semantic_similarity_threshold >= 0.0f && semantic_diff.semantic_similarity_threshold <= 1.0f))
    {
        if (error)
            *error = "semantic_diff.semantic_similarity_threshold out of range [0,1]";
        return false;
    }
    if (predictive.prediction_horizon < 0)
    {
        if (error)
            *error = "predictive.prediction_horizon must be >= 0";
        return false;
    }
    if (!(absence.absence_confidence_threshold >= 0.0f && absence.absence_confidence_threshold <= 1.0f))
    {
        if (error)
            *error = "absence.absence_confidence_threshold out of range [0,1]";
        return false;
    }

    if (error)
        *error = "";
    return true;
}

}  // namespace RawrXD
