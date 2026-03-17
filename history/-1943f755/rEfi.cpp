#include "agentic_controller.hpp"

#include "agentic_agent_coordinator.h"
#include "production_config_manager.h"
#include "registry_core/include/logger.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>
#include <QVariant>

#include <array>

using RawrXD::Registry::Logger;

namespace RawrXD::IDE {

namespace {
constexpr int kHeartbeatIntervalMs = 15000;
}

AgenticController::AgenticController(QObject* parent)
    : QObject(parent) {
    QObject::connect(&m_heartbeatTimer, &QTimer::timeout, this, &AgenticController::publishHeartbeat);
}

std::expected<std::monostate, std::string> AgenticController::bootstrap() {
    m_bootTimer.restart();

    auto coordinationResult = ensureCoordinator();
    if (!coordinationResult) {
        emit controllerError(QString::fromStdString(coordinationResult.error()));
        return coordinationResult;
    }

    const QString snapshotHint = resolveSnapshotPreference();
    emit layoutHydrationRequested(snapshotHint);
    emit controllerReady();

    m_heartbeatTimer.start(kHeartbeatIntervalMs);
    Logger::instance().info("AgenticController bootstrap completed in {} ms", m_bootTimer.elapsed());
    return std::monostate{};
}

std::expected<std::monostate, std::string> AgenticController::ensureCoordinator() {
    if (m_coordinator) {
        return std::monostate{};
    }

    try {
        m_coordinator = std::make_unique<AgenticAgentCoordinator>(this);
        const std::array<AgenticAgentCoordinator::AgentRole, 3> seedRoles = {
            AgenticAgentCoordinator::AgentRole::Analyzer,
            AgenticAgentCoordinator::AgentRole::Planner,
            AgenticAgentCoordinator::AgentRole::Executor
        };

        for (const auto role : seedRoles) {
            const QString agentId = m_coordinator->createAgent(role);
            Logger::instance().debug("Primed agent {} for role {}", agentId.toStdString(), static_cast<int>(role));
        }

        return std::monostate{};
    } catch (const std::exception& ex) {
        return std::unexpected(std::string("Failed to initialize AgenticAgentCoordinator: ") + ex.what());
    } catch (...) {
        return std::unexpected("Failed to initialize AgenticAgentCoordinator: unknown error");
    }
}

QString AgenticController::resolveSnapshotPreference() const {
    auto& config = RawrXD::ProductionConfigManager::instance();
    config.loadConfig();

    const QVariant configured = config.value("registry_snapshot_preference", QStringLiteral("latest"));
    if (configured.canConvert<QString>()) {
        const QString hint = configured.toString().trimmed();
        if (!hint.isEmpty()) {
            return hint;
        }
    }

    const QString envHint = qEnvironmentVariable("RAWRXD_REGISTRY_SNAPSHOT");
    if (!envHint.isEmpty()) {
        return envHint;
    }

    return QStringLiteral("latest");
}

void AgenticController::publishHeartbeat() {
    QJsonObject payload;
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    payload.insert(QStringLiteral("uptime_ms"), static_cast<qint64>(m_bootTimer.isValid() ? m_bootTimer.elapsed() : 0));

    if (m_coordinator) {
        payload.insert(QStringLiteral("coordination"), m_coordinator->getCoordinationMetrics());
    }

    const QString serialized = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    emit telemetryHeartbeat(serialized);
}

void AgenticController::handleLayoutRestored(const QString& snapshotId) {
    Logger::instance().info("Agentic controller acknowledged snapshot {}", snapshotId.toStdString());

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
                             metrics.value(QStringLiteral("total_agents")).toInt(),
                             metrics.value(QStringLiteral("total_tasks_assigned")).toInt());
}

} // namespace RawrXD::IDE
