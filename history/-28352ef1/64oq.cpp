#include "agentic_controller.hpp"

#include "agentic_agent_coordinator.h"
#include "production_config_manager.h"
#include "win32app/IDELogger.h"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <unordered_map>

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
    LOG_INFO(std::string("AgenticController bootstrap completed in " + std::to_string(m_bootTimer.elapsed()) + " ms").c_str());
    return AgenticResult::Ok("Agentic system bootstrapped");
}

AgenticResult AgenticController::ensureCoordinator() {
    if (m_coordinator) {
        return AgenticResult::Ok();
    }

    try {
        m_coordinator = std::make_unique<AgenticAgentCoordinator>();
        const std::array<AgenticAgentCoordinator::AgentRole, 3> seedRoles = {
            AgenticAgentCoordinator::AgentRole::Analyzer,
            AgenticAgentCoordinator::AgentRole::Planner,
            AgenticAgentCoordinator::AgentRole::Executor
        };

        for (const auto role : seedRoles) {
            const std::string agentId = m_coordinator->createAgent(role);
            LOG_DEBUG(std::string("Primed agent " + agentId + " for role " + std::to_string(static_cast<int>(role))).c_str());
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

    const std::string configured = config.value("registry_snapshot_preference", nlohmann::json("latest")).get<std::string>();
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
        nlohmann::json statuses = m_coordinator->getAllAgentStatuses();
        payload += ",\"coordination\":{";
        bool first = true;
        for (auto it = statuses.begin(); it != statuses.end(); ++it) {
            if (!first) payload += ",";
            payload += "\"" + it.key() + "\":" + it.value().dump();
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
    LOG_INFO(std::string("Agentic controller acknowledged snapshot " + snapshotId).c_str());

    if (m_coordinator) {
        m_coordinator->synchronizeState();
    }
}

void AgenticController::handleWindowActivated() {
    if (!m_coordinator) {
        return;
    }

    const nlohmann::json metrics = m_coordinator->getAllAgentStatuses();
    auto findOr = [&](const std::string& key, const std::string& def) -> std::string {
        if (metrics.contains(key)) {
            const auto& v = metrics[key];
            if (v.is_string()) return v.get<std::string>();
            return v.dump();
        }
        return def;
    };
    LOG_DEBUG(std::string("Window activation routed to agentic coordinator: agents=" + findOr("total_agents", "0") + " tasks=" + findOr("total_tasks_assigned", "0")).c_str());
}

} // namespace RawrXD::IDE

