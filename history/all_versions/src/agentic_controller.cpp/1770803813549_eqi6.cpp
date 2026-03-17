#include "agentic_controller.hpp"

#include "agentic_agent_coordinator.h"
#include "production_config_manager.h"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <unordered_map>

using RawrXD::Registry::Logger;

namespace RawrXD::IDE {

namespace {
constexpr int kHeartbeatIntervalMs = 15000;
}

AgenticController::AgenticController()
{
    // No signal connections needed — callbacks used instead
}

AgenticController::~AgenticController() = default;

AgenticResult AgenticController::bootstrap() {
    m_bootTimer.restart();

    auto coordinationResult = ensureCoordinator();
    if (!coordinationResult.success) {
        if (onControllerError) {
            onControllerError(coordinationResult.message);
        }
        return coordinationResult;
    }

    const std::string snapshotHint = resolveSnapshotPreference();
    if (onLayoutHydrationRequested) {
        onLayoutHydrationRequested(snapshotHint);
    }
    if (onControllerReady) {
        onControllerReady();
    }

    m_heartbeatTimer.start(kHeartbeatIntervalMs);
    Logger::instance().info("AgenticController bootstrap completed in {} ms", m_bootTimer.elapsed());
    return AgenticResult::Ok("Agentic system bootstrapped");
}

AgenticResult AgenticController::ensureCoordinator() {
    if (m_coordinator) {
        return AgenticResult::Ok();
    }

    try {
        m_coordinator = std::make_unique<AgenticAgentCoordinator>(this);
        const std::array<AgenticAgentCoordinator::AgentRole, 3> seedRoles = {
            AgenticAgentCoordinator::AgentRole::Analyzer,
            AgenticAgentCoordinator::AgentRole::Planner,
            AgenticAgentCoordinator::AgentRole::Executor
        };

        for (const auto role : seedRoles) {
            const std::string agentId = m_coordinator->createAgent(role);
            Logger::instance().debug("Primed agent {} for role {}", agentId, static_cast<int>(role));
        }

        return AgenticResult::Ok();
    } catch (const std::exception& ex) {
        return AgenticResult::Fail(AgenticError::BootstrapFailed,
                                   std::string("Failed to initialize AgenticAgentCoordinator: ") + ex.what());
    } catch (...) {
        return AgenticResult::Fail(AgenticError::BootstrapFailed,
                                   "Failed to initialize AgenticAgentCoordinator: unknown error");
    }
}

std::string AgenticController::resolveSnapshotPreference() const {
    auto& config = RawrXD::ProductionConfigManager::instance();
    config.loadConfig();

    const std::string configured = config.valueAsString("registry_snapshot_preference", "latest");
    if (!configured.empty()) {
        return configured;
    }

    const char* envHint = std::getenv("RAWRXD_REGISTRY_SNAPSHOT");
    if (envHint && envHint[0] != '\0') {
        return std::string(envHint);
    }

    return std::string("latest");
}

void AgenticController::publishHeartbeat() {
    // Build a simple JSON payload manually
    std::string payload = "{";
    // payload += "\"timestamp\":\"...\",";  // TODO: add ISO8601 timestamp if needed
    payload += "\"uptime_ms\":";
    payload += std::to_string(m_bootTimer.isValid() ? m_bootTimer.elapsed() : 0);

    if (m_coordinator) {
        auto metrics = m_coordinator->getCoordinationMetrics();
        payload += ",\"coordination\":{";
        bool first = true;
        for (const auto& [key, value] : metrics) {
            if (!first) payload += ",";
            payload += "\"" + key + "\":" + value;
            first = false;
        }
        payload += "}";
    }

    payload += "}";

    if (onTelemetryHeartbeat) {
        onTelemetryHeartbeat(payload);
    }
}

void AgenticController::tick() {
    if (m_heartbeatTimer.shouldFire()) {
        publishHeartbeat();
    }
}

void AgenticController::handleLayoutRestored(const std::string& snapshotId) {
    Logger::instance().info("Agentic controller acknowledged snapshot {}", snapshotId);

    if (m_coordinator) {
        m_coordinator->saveCheckpoint();
    }
}

void AgenticController::handleWindowActivated() {
    if (!m_coordinator) {
        return;
    }

    const auto metrics = m_coordinator->getCoordinationMetrics();
    auto findOr = [&](const std::string& key, const std::string& def) -> std::string {
        auto it = metrics.find(key);
        return (it != metrics.end()) ? it->second : def;
    };
    Logger::instance().debug("Window activation routed to agentic coordinator: agents={} tasks={}",
                             findOr("total_agents", "0"),
                             findOr("total_tasks_assigned", "0"));
}

} // namespace RawrXD::IDE

