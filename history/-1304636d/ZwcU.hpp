#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QObject>
#include <memory>

/**
 * @class AgentCoordinator
 * @brief Coordinates multiple AI agents and manages task DAG execution
 * 
 * Provides:
 * - Agent pool management (Research, Coder, Reviewer, Optimizer, Deployer)
 * - Task DAG (directed acyclic graph) execution with dependency resolution
 * - Inter-agent context sharing
 * - Resource conflict resolution
 * - Real-time progress tracking
 */
class AgentCoordinator : public QObject {
    Q_OBJECT

public:
    explicit AgentCoordinator(QObject* parent = nullptr);
    ~AgentCoordinator() = default;

    // TODO: Implement in Phase 2
    // Placeholder for now
};
