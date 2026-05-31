#pragma once

#include "audit_subsystem.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace rawrxd::audit::integration {

using json = nlohmann::json;

inline Actor make_actor(
    std::string_view user_id,
    std::string_view session_id,
    std::string_view ip,
    std::string_view user_agent,
    std::string_view process_id,
    std::string_view thread_id,
    std::string_view salt) {
    Actor actor;
    actor.user_id_hash = hash_string(std::string(user_id) + std::string(salt));
    actor.session_id_hash = hash_string(std::string(session_id) + std::string(salt));
    actor.ip_address_hash = hash_string(std::string(ip) + std::string(salt));
    actor.user_agent_hash = hash_string(user_agent);
    actor.process_id = std::string(process_id);
    actor.thread_id = std::string(thread_id);
    return actor;
}

inline json make_not_implemented_response(
    std::string_view endpoint,
    std::string_view reason,
    std::string_view warning,
    bool critical,
    std::string_view event_id,
    const json& extra = json::object()) {
    json r = {
        {"success", false},
        {"implemented", false},
        {"mode", "stub"},
        {"reason", std::string(reason)},
        {"endpoint", std::string(endpoint)},
        {"state_changed", false},
        {"warning", std::string(warning)},
        {"severity", critical ? "CRITICAL" : "WARNING"},
        {"timestamp", format_iso8601(std::chrono::system_clock::now())},
        {"event_id", std::string(event_id)},
    };
    for (const auto& [k, v] : extra.items()) {
        r[k] = v;
    }
    return r;
}

inline json handle_swarm_not_implemented(
    std::string_view endpoint,
    std::string_view request_body,
    const Actor& actor) {
    auto& audit = AuditOrchestrator::instance();
    const std::string event_id = generate_uuid_v4();

    AuditEvent ev = AuditEvent::builder()
        .with_id(event_id)
        .with_timestamp(std::chrono::system_clock::now())
        .with_severity(Severity::WARNING)
        .with_category(Category::SWARM_OPERATION)
        .with_actor(actor)
        .with_action(Action{
            "swarm_operation",
            std::string(endpoint),
            "POST",
            "swarm_control",
            {"payload"}})
        .with_resource(Resource{"swarm_cluster", "none", "default", "local"})
        .with_outcome(Outcome{
            false,
            false,
            "endpoint_not_implemented",
            "No swarm state mutation occurred.",
            501})
        .with_context(Context{
            hash_string(request_body),
            "0",
            "null",
            "null",
            0,
            0,
            {}})
        .build();

    audit.log_event(ev);

    return make_not_implemented_response(
        endpoint,
        "endpoint_not_implemented",
        "NO_CLUSTER_STATE_CHANGE",
        false,
        event_id);
}

inline json handle_backend_switch_not_implemented(
    std::string_view request_body,
    std::string_view requested_backend,
    std::string_view active_backend,
    const Actor& actor) {
    auto& audit = AuditOrchestrator::instance();
    const std::string event_id = generate_uuid_v4();

    AuditEvent ev = AuditEvent::builder()
        .with_id(event_id)
        .with_timestamp(std::chrono::system_clock::now())
        .with_severity(Severity::ERROR)
        .with_category(Category::BACKEND_SWITCH)
        .with_actor(actor)
        .with_action(Action{
            "backend_switch",
            "/api/backend/switch",
            "POST",
            "switch_backend",
            {"backend"}})
        .with_resource(Resource{"inference_backend", std::string(active_backend), "inference", "local"})
        .with_outcome(Outcome{
            false,
            false,
            "endpoint_not_implemented",
            "Backend switch not performed. Active backend unchanged.",
            501})
        .with_context(Context{
            hash_string(request_body),
            "0",
            hash_string(active_backend),
            "null",
            0,
            0,
            {}})
        .build();

    audit.log_event(ev);

    json extra = {
        {"requested_backend", std::string(requested_backend)},
        {"actual_backend", std::string(active_backend)},
        {"warning", "BACKEND_UNCHANGED"},
    };
    return make_not_implemented_response(
        "/api/backend/switch",
        "endpoint_not_implemented",
        "BACKEND_UNCHANGED",
        false,
        event_id,
        extra);
}

inline json handle_safety_rollback_not_implemented(
    std::string_view request_body,
    std::string_view target,
    std::string_view current_state_hash,
    const Actor& actor) {
    auto& audit = AuditOrchestrator::instance();
    const std::string event_id = generate_uuid_v4();

    AuditEvent ev = AuditEvent::builder()
        .with_id(event_id)
        .with_timestamp(std::chrono::system_clock::now())
        .with_severity(Severity::CRITICAL)
        .with_category(Category::SAFETY_ROLLBACK)
        .with_actor(actor)
        .with_action(Action{
            "safety_rollback",
            "/api/safety/rollback",
            "POST",
            "rollback",
            {"target_version"}})
        .with_resource(Resource{"system_state", std::string(target), "safety", "global"})
        .with_outcome(Outcome{
            false,
            false,
            "endpoint_not_implemented",
            "Rollback not performed. Manual incident response required.",
            501})
        .with_context(Context{
            hash_string(request_body),
            "0",
            std::string(current_state_hash),
            "null",
            0,
            0,
            {}})
        .build();

    audit.log_event(ev);

    json extra = {
        {"rollback_target", std::string(target)},
        {"incident_response", {
            {"manual_rollback_required", true},
            {"escalation_required", true}
        }}
    };
    return make_not_implemented_response(
        "/api/safety/rollback",
        "endpoint_not_implemented",
        "NO_ROLLBACK_PERFORMED",
        true,
        event_id,
        extra);
}

inline json handle_agent_replay_not_implemented(
    std::string_view request_body,
    std::string_view session_id,
    const Actor& actor) {
    auto& audit = AuditOrchestrator::instance();
    const std::string event_id = generate_uuid_v4();

    AuditEvent ev = AuditEvent::builder()
        .with_id(event_id)
        .with_timestamp(std::chrono::system_clock::now())
        .with_severity(Severity::WARNING)
        .with_category(Category::AGENT_REPLAY)
        .with_actor(actor)
        .with_action(Action{
            "agent_replay",
            "/api/agents/replay",
            "POST",
            "replay",
            {"session_id"}})
        .with_resource(Resource{"agent_session", std::string(session_id), "agents", "local"})
        .with_outcome(Outcome{
            false,
            false,
            "endpoint_not_implemented",
            "Replay not generated.",
            501})
        .with_context(Context{
            hash_string(request_body),
            "0",
            "null",
            "null",
            0,
            0,
            {}})
        .build();

    audit.log_event(ev);

    return make_not_implemented_response(
        "/api/agents/replay",
        "endpoint_not_implemented",
        "NO_REPLAY_GENERATED",
        false,
        event_id);
}

} // namespace rawrxd::audit::integration
