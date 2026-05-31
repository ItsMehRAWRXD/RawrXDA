#pragma once

#include "behavior_config.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD
{

struct BehaviorConfigV2 final : BehaviorConfig
{
    // META-COGNITIVE LAYER
    struct SelfModelState
    {
        float confidence_calibration = 0.0f;
        std::vector<float> uncertainty_per_domain;
        int meta_reasoning_depth = 0;
        bool uncertainty_quantification = true;
        bool mistake_prediction = false;
        std::string failure_mode_taxonomy;
    } self_model;

    bool propagate_confidence = true;
    float minimum_confidence_threshold = 0.7f;
    bool refuse_low_confidence = true;
    bool explain_uncertainty = true;

    struct AdversarialConfig
    {
        bool enabled = false;
        int attack_vectors = 5;
        std::vector<std::string> attack_types;
        bool auto_patch = true;
        bool escalate_to_human = true;
        float attack_budget = 0.3f;
        bool persist_attacks = false;
    } adversarial;

    struct TemporalConfig
    {
        bool enabled = false;
        int max_branches = 10;
        bool causal_tracking = true;
        bool predict_future_states = true;
        bool learn_from_abandoned_paths = true;
        std::string divergence_strategy = "semantic";
    } temporal;

    struct SpeculativeConfig
    {
        bool enabled = false;
        int speculation_depth = 3;
        bool execute_hypotheses = false;
        float confidence_threshold = 0.85f;
        std::vector<std::string> speculation_domains;
        bool cache_speculative_results = true;
        bool discard_low_utility = true;
    } speculative;

    struct IntentionConfig
    {
        bool deep_intent_modeling = false;
        bool infer_implicit_requirements = true;
        bool detect_intent_drift = true;
        bool maintain_goal_stack = true;
        int max_inference_depth = 5;
        bool propose_higher_goals = false;
    } intention;

    struct SynthesisConfig
    {
        bool enabled = false;
        bool learn_project_patterns = true;
        bool synthesize_idioms = true;
        bool cross_language_transfer = false;
        bool evolve_patterns = true;
        int pattern_abstraction_level = 2;
    } synthesis;

    struct CognitiveOffloadConfig
    {
        bool maintain_context_graph = true;
        bool auto_summarize = true;
        bool semantic_chunking = true;
        bool attention_management = true;
        int max_working_memory_items = 100;
        bool forgetting_enabled = true;
        float forgetting_threshold = 0.1f;
    } cognitive_offload;

    struct DialogueConfig
    {
        bool enabled = false;
        std::vector<std::string> agent_personas;
        bool visible_dialogue = false;
        int max_turns_per_agent = 3;
        bool require_consensus = true;
        bool minority_report = true;
        std::string resolution_strategy = "vote";
    } dialogue;

    struct ConstraintConfig
    {
        bool enabled = true;
        bool infer_implicit_constraints = true;
        bool detect_constraint_violations = true;
        bool suggest_constraint_repairs = true;
        bool temporal_constraints = false;
        std::vector<std::string> constraint_types;
    } constraints;

    struct SemanticDiffConfig
    {
        bool enabled = true;
        bool detect_intent_changes = true;
        bool group_related_changes = true;
        bool detect_refactoring_patterns = true;
        bool predict_merge_conflicts = true;
        bool suggest_merge_strategies = true;
        float semantic_similarity_threshold = 0.7f;
    } semantic_diff;

    struct PredictiveConfig
    {
        bool enabled = false;
        bool predict_technical_debt = true;
        bool predict_performance_decay = true;
        bool predict_security_drift = true;
        int prediction_horizon = 90;
        bool confidence_intervals = true;
        std::string maintenance_strategy = "proactive";
    } predictive;

    struct AbsenceConfig
    {
        bool enabled = true;
        bool detect_missing_tests = true;
        bool detect_missing_error_handling = true;
        bool detect_missing_documentation = true;
        bool detect_missing_security_checks = true;
        bool detect_missing_edge_cases = true;
        float absence_confidence_threshold = 0.6f;
    } absence;

    struct CausalConfig
    {
        bool enabled = false;
        int max_causal_depth = 10;
        bool infer_hidden_causes = true;
        bool detect_confounding_factors = true;
        bool propose_interventions = true;
        bool track_causal_certainty = true;
    } causal;

    struct VersioningConfig
    {
        bool enabled = false;
        bool track_intent_versioning = true;
        bool detect_breaking_changes = true;
        bool suggest_compatibility_shims = true;
        bool maintain_version_graph = true;
        std::string versioning_strategy = "semantic";
    } versioning;

    struct EnvironmentConfig
    {
        bool enabled = false;
        bool infer_dependencies = true;
        bool detect_version_conflicts = true;
        bool generate_dockerfile = true;
        bool generate_ci_pipeline = true;
        bool suggest_environment_strategies = true;
        bool test_environment_reproducibility = true;
    } environment;

    // Serialization
    std::unordered_map<std::string, std::string> to_kv_pairs() const override;
    static BehaviorConfigV2 from_kv_pairs(const std::unordered_map<std::string, std::string>& kv);
    bool validate(std::string* error = nullptr) const override;
};

}  // namespace RawrXD
