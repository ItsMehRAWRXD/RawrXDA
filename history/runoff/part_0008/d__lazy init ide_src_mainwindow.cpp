#include "../include/mainwindow.h"

#include "agentic_controller.hpp"
#include "production_config_manager.h"
#include "registry_core/include/logger.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QScreen>
#include <QVariant>

#include <chrono>

using RawrXD::Registry::Logger;

namespace RawrXD::IDE {

namespace {

struct SnapshotEnvelope {
    QString id;
    QString absolutePath;
    QByteArray geometry;
    QByteArray state;
};

QString defaultSnapshotRoot() {
    const QString envRoot = qEnvironmentVariable("RAWRXD_REGISTRY_SNAPSHOT_ROOT");
    if (!envRoot.isEmpty()) {
        return envRoot;
    }

    const QString lazyRoot = qEnvironmentVariable("LAZY_INIT_IDE_ROOT");
    if (!lazyRoot.isEmpty()) {
        return QDir(lazyRoot).filePath("state/registry_snapshots");
    }

    const QString baseDir = QCoreApplication::applicationDirPath();
    const QString fallbackBase = baseDir.isEmpty() ? QDir::currentPath() : baseDir;
    return QDir(fallbackBase).filePath("state/registry_snapshots");
}

AgenticResult decodeField(const QJsonObject& object, const QString& key, QByteArray& outField) {
    outField.clear();
    const auto rawValue = object.value(key);
    if (!rawValue.isString()) {
        return AgenticResult::Ok();
    }

    const QString encoded = rawValue.toString();
    if (encoded.isEmpty()) {
        return AgenticResult::Ok();
    }

    const QByteArray decoded = QByteArray::fromBase64(encoded.toUtf8());
    if (decoded.isEmpty()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot field '") + key.toStdString() + "' failed to decode");
    }

    outField = decoded;
    return AgenticResult::Ok();
}

AgenticResult loadSnapshotEnvelope(const QString& requestHint, SnapshotEnvelope& envelope) {
    auto& config = RawrXD::ProductionConfigManager::instance();
    config.loadConfig();

    QString root = config.value("registry_snapshot_root", defaultSnapshotRoot()).toString();
    if (root.isEmpty()) {
        root = defaultSnapshotRoot();
    }

    QDir snapshotDir(root);
    if (!snapshotDir.exists()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot directory missing: ") + root.toStdString());
    }

    QString effectiveHint = requestHint.trimmed();
    if (effectiveHint.isEmpty()) {
        const QVariant configuredHint = config.value("registry_snapshot_preference", QStringLiteral("latest"));
        if (configuredHint.canConvert<QString>()) {
            effectiveHint = configuredHint.toString().trimmed();
        }
    }
    if (effectiveHint.isEmpty()) {
        effectiveHint = QStringLiteral("latest");
    }

    QFileInfo snapshotInfo;
    if (effectiveHint.compare(QStringLiteral("latest"), Qt::CaseInsensitive) == 0) {
        const QFileInfoList files = snapshotDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
        if (files.isEmpty()) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("No registry snapshots found under ") + root.toStdString());
        }
        snapshotInfo = files.front();
    } else {
        QString candidate = effectiveHint;
        if (!candidate.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            candidate.append(QStringLiteral(".json"));
        }
        snapshotInfo = QFileInfo(snapshotDir.filePath(candidate));
        if (!snapshotInfo.exists()) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Snapshot not found: ") + snapshotInfo.absoluteFilePath().toStdString());
        }
    }

    QFile snapshotFile(snapshotInfo.absoluteFilePath());
    if (!snapshotFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Unable to open snapshot: ") + snapshotFile.errorString().toStdString());
    }

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(snapshotFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot parse failure: ") + parseError.errorString().toStdString());
    }

    envelope.id = document.object().value(QStringLiteral("id")).toString(snapshotInfo.baseName());
    envelope.absolutePath = snapshotInfo.absoluteFilePath();

    auto geometryPayload = decodeField(document.object(), QStringLiteral("window_geometry"), envelope.geometry);
    if (!geometryPayload.success) {
        return geometryPayload;
    }

    auto statePayload = decodeField(document.object(), QStringLiteral("window_state"), envelope.state);
    if (!statePayload.success) {
        return statePayload;
    }

    return AgenticResult::Ok();
}

} // namespace (anonymous)

RawrXD::IDE::MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setObjectName("RawrXDMainWindow");
    setMinimumSize(1024, 640);
}

AgenticResult RawrXD::IDE::MainWindow::initialize() {
    auto placementResult = centerOnPrimaryDisplay();
    if (!placementResult.success) {
        return placementResult;
    }

    wireSignals();
    restoreLayout();

    Logger::instance().info("MainWindow initialized");
    return AgenticResult::Ok("MainWindow initialized");
}

AgenticResult RawrXD::IDE::MainWindow::centerOnPrimaryDisplay() {
    auto screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   "No primary screen detected for RawrXD main window placement.");
    }

    const QRect availableGeometry = screen->availableGeometry();
    const QSize windowSize = size();
    const int x = availableGeometry.x() + (availableGeometry.width() - windowSize.width()) / 2;
    const int y = availableGeometry.y() + (availableGeometry.height() - windowSize.height()) / 2;
    setGeometry(x, y, windowSize.width(), windowSize.height());

    return AgenticResult::Ok();
}

void RawrXD::IDE::MainWindow::wireSignals() {
    if (!m_agenticController) {
        m_agenticController = std::make_unique<AgenticController>(this);
    }

    auto* controller = m_agenticController.get();

    QObject::connect(controller, &AgenticController::controllerReady, this, [] {
        Logger::instance().info("Agentic controller signaled readiness");
    });

    QObject::connect(controller, &AgenticController::controllerError, this, [](const QString& message) {
        Logger::instance().error("Agentic controller error: {}", message.toStdString());
    });

    QObject::connect(controller, &AgenticController::telemetryHeartbeat, this, [](const QString& payload) {
        Logger::instance().debug("Agentic heartbeat {}", payload.toStdString());
    });

    QObject::connect(controller, &AgenticController::layoutHydrationRequested, this, [this](const QString& snapshotHint) {
        QString hydratedId;
        auto hydrationResult = hydrateLayout(snapshotHint, hydratedId);
        if (!hydrationResult.success) {
            emit layoutHydrationFailed(snapshotHint, QString::fromStdString(hydrationResult.message));
            Logger::instance().warning("Agentic-requested hydration failed: {}", hydrationResult.message);
            return;
        }

        Logger::instance().info("Agentic-requested hydration applied snapshot {}", hydratedId.toStdString());
    });

    QObject::connect(this, &MainWindow::layoutRestored, controller, &AgenticController::handleLayoutRestored);

    const auto bootstrapResult = controller->bootstrap();
    if (!bootstrapResult.success) {
        Logger::instance().error("Agentic controller bootstrap failed: {}", bootstrapResult.message);
    }
}

void RawrXD::IDE::MainWindow::restoreLayout() {
    const auto start = std::chrono::steady_clock::now();
    QString hydratedId;
    auto hydrationResult = hydrateLayout({}, hydratedId);
    if (!hydrationResult.success) {
        Logger::instance().warning("Layout restore skipped: {}", hydrationResult.message);
        emit layoutHydrationFailed(QString(), QString::fromStdString(hydrationResult.message));
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    Logger::instance().info("Layout restored from snapshot {} in {} ms",
                             hydratedId.toStdString(),
                             elapsed.count());
}

AgenticResult RawrXD::IDE::MainWindow::hydrateLayout(const QString& snapshotHint, QString& outSnapshotId) {
    SnapshotEnvelope envelope{};
    auto envelopeResult = loadSnapshotEnvelope(snapshotHint, envelope);
    if (!envelopeResult.success) {
        return envelopeResult;
    }

    if (!m_lastHydratedSnapshot.isEmpty() && m_lastHydratedSnapshot == envelope.id) {
        outSnapshotId = envelope.id;
        return AgenticResult::Ok(envelope.id.toStdString());
    }

    if (!envelope.geometry.isEmpty()) {
        if (!restoreGeometry(envelope.geometry)) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Failed to restore geometry from ") + envelope.absolutePath.toStdString());
        }
    }

    if (!envelope.state.isEmpty()) {
        if (!restoreState(envelope.state)) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Failed to restore dock state from ") + envelope.absolutePath.toStdString());
        }
    }

    m_lastHydratedSnapshot = envelope.id;
    outSnapshotId = envelope.id;
    emit layoutRestored(envelope.id);
    return AgenticResult::Ok(envelope.id.toStdString());
}

} // namespace RawrXD::IDE