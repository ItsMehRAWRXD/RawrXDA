#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QTimer>
#include <QString>

#include <expected>
#include <memory>
#include <string>

class AgenticAgentCoordinator;

namespace RawrXD::IDE {

class AgenticController : public QObject {
    Q_OBJECT

public:
    explicit AgenticController(QObject* parent = nullptr);
    ~AgenticController() override = default;

    std::expected<std::monostate, std::string> bootstrap();

signals:
    void controllerReady();
    void controllerError(const QString& message);
    void layoutHydrationRequested(const QString& snapshotHint);
    void telemetryHeartbeat(const QString& payload);

public slots:
    void handleLayoutRestored(const QString& snapshotId);
    void handleWindowActivated();

private:
    std::expected<std::monostate, std::string> ensureCoordinator();
    QString resolveSnapshotPreference() const;
    void publishHeartbeat();

    std::unique_ptr<AgenticAgentCoordinator> m_coordinator;
    QElapsedTimer m_bootTimer;
    QTimer m_heartbeatTimer;
};

} // namespace RawrXD::IDE
