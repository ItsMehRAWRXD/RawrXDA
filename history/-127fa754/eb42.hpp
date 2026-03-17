#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTimer>
#include <QString>

#include <memory>
#include <string>
#include <utility>

class AgenticAgentCoordinator;

namespace RawrXD::IDE {

enum class AgenticError {
    None,
    BootstrapFailed,
    CoordinatorNotReady,
    InvalidState,
    CommunicationError
};

struct AgenticResult {
    bool success;
    AgenticError error;
    std::string message;

    static AgenticResult Ok(std::string msg = {}) {
        return {true, AgenticError::None, std::move(msg)};
    }

    static AgenticResult Fail(AgenticError err, std::string msg) {
        return {false, err, std::move(msg)};
    }
};

class AgenticController : public QObject {
    Q_OBJECT

public:
    explicit AgenticController(QObject* parent = nullptr);
    ~AgenticController() override = default;

    AgenticResult bootstrap();

signals:
    void controllerReady();
    void controllerError(const QString& message);
    void layoutHydrationRequested(const QString& snapshotHint);
    void telemetryHeartbeat(const QString& payload);

public slots:
    void handleLayoutRestored(const QString& snapshotId);
    void handleWindowActivated();

private:
    AgenticResult ensureCoordinator();
    QString resolveSnapshotPreference() const;
    void publishHeartbeat();

    std::unique_ptr<AgenticAgentCoordinator> m_coordinator;
    QElapsedTimer m_bootTimer;
    QTimer m_heartbeatTimer;
};

} // namespace RawrXD::IDE
