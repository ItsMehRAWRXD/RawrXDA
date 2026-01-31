#include "agentic_controller.hpp"

#include "agentic_agent_coordinator.h"
#include "production_config_manager.h"

#include <array>

using RawrXD::Registry::Logger;

namespace RawrXD::IDE {

namespace {
constexpr int kHeartbeatIntervalMs = 15000;
}

AgenticController::AgenticController()
     {
    // Object::  // Signal connection removed\n}

AgenticController::~AgenticController() = default;

AgenticResult AgenticController::bootstrap() {
    m_bootTimer.restart();

    auto coordinationResult = ensureCoordinator();
    if (!coordinationResult.success) {
        controllerError(std::string::fromStdString(coordinationResult.message));
        return coordinationResult;
    }

    const std::string snapshotHint = resolveSnapshotPreference();
    layoutHydrationRequested(snapshotHint);
    controllerReady();

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

    const std::any configured = config.value("registry_snapshot_preference", std::stringLiteral("latest"));
    if (configured.canConvert<std::string>()) {
        const std::string hint = configured.toString().trimmed();
        if (!hint.empty()) {
            return hint;
        }
    }

    const std::string envHint = qEnvironmentVariable("RAWRXD_REGISTRY_SNAPSHOT");
    if (!envHint.empty()) {
        return envHint;
    }

    return std::stringLiteral("latest");
}

void AgenticController::publishHeartbeat() {
    void* payload;
    payload.insert(std::stringLiteral("timestamp"), // DateTime::currentDateTimeUtc().toString(ISODateWithMs));
    payload.insert(std::stringLiteral("uptime_ms"), static_cast<int64_t>(m_bootTimer.isValid() ? m_bootTimer.elapsed() : 0));

    if (m_coordinator) {
        payload.insert(std::stringLiteral("coordination"), m_coordinator->getCoordinationMetrics());
    }

    const std::string serialized = std::string::fromUtf8(void*(payload).toJson(void*::Compact));
    telemetryHeartbeat(serialized);
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
    Logger::instance().debug("Window activation routed to agentic coordinator: agents={} tasks={}",
                             metrics.value(std::stringLiteral("total_agents")),
                             metrics.value(std::stringLiteral("total_tasks_assigned")));
}

} // namespace RawrXD::IDE







