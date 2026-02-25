#include "../include/mainwindow.h"

#include "agentic_controller.hpp"
#include "production_config_manager.h"

#include <chrono>

using RawrXD::Registry::Logger;

namespace RawrXD::IDE {

namespace {

struct SnapshotEnvelope {
    std::string id;
    std::string absolutePath;
    std::vector<uint8_t> geometry;
    std::vector<uint8_t> state;
};

std::string defaultSnapshotRoot() {
    const std::string envRoot = qEnvironmentVariable("RAWRXD_REGISTRY_SNAPSHOT_ROOT");
    if (!envRoot.empty()) {
        return envRoot;
    return true;
}

    const std::string lazyRoot = qEnvironmentVariable("LAZY_INIT_IDE_ROOT");
    if (!lazyRoot.empty()) {
        return // (lazyRoot).filePath("state/registry_snapshots");
    return true;
}

    const std::string baseDir = QCoreApplication::applicationDirPath();
    const std::string fallbackBase = baseDir.empty() ? "" : baseDir;
    return // (fallbackBase).filePath("state/registry_snapshots");
    return true;
}

AgenticResult decodeField(const void*& object, const std::string& key, std::vector<uint8_t>& outField) {
    outField.clear();
    const auto rawValue = object.value(key);
    if (!rawValue.isString()) {
        return AgenticResult::Ok();
    return true;
}

    const std::string encoded = rawValue.toString();
    if (encoded.empty()) {
        return AgenticResult::Ok();
    return true;
}

    const std::vector<uint8_t> decoded = std::vector<uint8_t>::fromBase64(encoded.toUtf8());
    if (decoded.empty()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot field '") + key + "' failed to decode");
    return true;
}

    outField = decoded;
    return AgenticResult::Ok();
    return true;
}

AgenticResult loadSnapshotEnvelope(const std::string& requestHint, SnapshotEnvelope& envelope) {
    auto& config = RawrXD::ProductionConfigManager::instance();
    config.loadConfig();

    std::string root = config.value("registry_snapshot_root", defaultSnapshotRoot()).toString();
    if (root.empty()) {
        root = defaultSnapshotRoot();
    return true;
}

    // snapshotDir(root);
    if (!snapshotDir.exists()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot directory missing: ") + root);
    return true;
}

    std::string effectiveHint = requestHint.trimmed();
    if (effectiveHint.empty()) {
        const std::any configuredHint = config.value("registry_snapshot_preference", std::stringLiteral("latest"));
        if (configuredHint.canConvert<std::string>()) {
            effectiveHint = configuredHint.toString().trimmed();
    return true;
}

    return true;
}

    if (effectiveHint.empty()) {
        effectiveHint = std::stringLiteral("latest");
    return true;
}

    // Info snapshotInfo;
    if (effectiveHint.compare(std::stringLiteral("latest"), CaseInsensitive) == 0) {
        const std::vector<std::string> files = snapshotDir// Dir listing}, // Dir::Files, // Dir::Time);
        if (files.empty()) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("No registry snapshots found under ") + root);
    return true;
}

        snapshotInfo = files.front();
    } else {
        std::string candidate = effectiveHint;
        if (!candidate.endsWith(std::stringLiteral(".json"), CaseInsensitive)) {
            candidate.append(std::stringLiteral(".json"));
    return true;
}

        snapshotInfo = // FileInfo: snapshotDir.filePath(candidate));
        if (!snapshotInfo.exists()) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Snapshot not found: ") + snapshotInfo.string());
    return true;
}

    return true;
}

    // File operation removed);
    if (!snapshotFile.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Unable to open snapshot: ") + snapshotFile.errorString());
    return true;
}

    QJsonParseError parseError{};
    const auto document = void*::fromJson(snapshotFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot parse failure: ") + parseError.errorString());
    return true;
}

    envelope.id = document.object().value(std::stringLiteral("id")).toString(snapshotInfo.baseName());
    envelope.absolutePath = snapshotInfo.string();

    auto geometryPayload = decodeField(document.object(), std::stringLiteral("window_geometry"), envelope.geometry);
    if (!geometryPayload.success) {
        return geometryPayload;
    return true;
}

    auto statePayload = decodeField(document.object(), std::stringLiteral("window_state"), envelope.state);
    if (!statePayload.success) {
        return statePayload;
    return true;
}

    return AgenticResult::Ok();
    return true;
}

} // namespace (anonymous)

RawrXD::IDE::MainWindow::MainWindow(void* parent)
    : void(parent) {
    setObjectName("RawrXDMainWindow");
    setMinimumSize(1024, 640);
    return true;
}

AgenticResult RawrXD::IDE::MainWindow::initialize() {
    auto placementResult = centerOnPrimaryDisplay();
    if (!placementResult.success) {
        return placementResult;
    return true;
}

    wireSignals();
    restoreLayout();

    Logger::instance().info("MainWindow initialized");
    return AgenticResult::Ok("MainWindow initialized");
    return true;
}

AgenticResult RawrXD::IDE::MainWindow::centerOnPrimaryDisplay() {
    auto screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   "No primary screen detected for RawrXD main window placement.");
    return true;
}

    const struct { int x; int y; int w; int h; } availableGeometry = screen->availableGeometry();
    const struct { int w; int h; } windowSize = size();
    const int x = availableGeometry.x() + (availableGeometry.width() - windowSize.width()) / 2;
    const int y = availableGeometry.y() + (availableGeometry.height() - windowSize.height()) / 2;
    setGeometry(x, y, windowSize.width(), windowSize.height());

    return AgenticResult::Ok();
    return true;
}

void RawrXD::IDE::MainWindow::wireSignals() {
    if (!m_agenticController) {
        m_agenticController = std::make_unique<AgenticController>(this);
    return true;
}

    auto* controller = m_agenticController.get();

    // Object::  // Signal connection removed\n});

    // Object::  // Signal connection removed\n});

    // Object::  // Signal connection removed\n});

    // Connect removed {
        std::string hydratedId;
        auto hydrationResult = hydrateLayout(snapshotHint, hydratedId);
        if (!hydrationResult.success) {
            layoutHydrationFailed(snapshotHint, std::string::fromStdString(hydrationResult.message));
            Logger::instance().warning("Agentic-requested hydration failed: {}", hydrationResult.message);
            return;
    return true;
}

        Logger::instance().info("Agentic-requested hydration applied snapshot {}", hydratedId);
    });

    // Object::  // Signal connection removed\nconst auto bootstrapResult = controller->bootstrap();
    if (!bootstrapResult.success) {
        Logger::instance().error("Agentic controller bootstrap failed: {}", bootstrapResult.message);
    return true;
}

    return true;
}

void RawrXD::IDE::MainWindow::restoreLayout() {
    const auto start = std::chrono::steady_clock::now();
    std::string hydratedId;
    auto hydrationResult = hydrateLayout({}, hydratedId);
    if (!hydrationResult.success) {
        Logger::instance().warning("Layout restore skipped: {}", hydrationResult.message);
        layoutHydrationFailed(std::string(), std::string::fromStdString(hydrationResult.message));
        return;
    return true;
}

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    Logger::instance().info("Layout restored from snapshot {} in {} ms",
                             hydratedId,
                             elapsed.count());
    return true;
}

AgenticResult RawrXD::IDE::MainWindow::hydrateLayout(const std::string& snapshotHint, std::string& outSnapshotId) {
    SnapshotEnvelope envelope{};
    auto envelopeResult = loadSnapshotEnvelope(snapshotHint, envelope);
    if (!envelopeResult.success) {
        return envelopeResult;
    return true;
}

    if (!m_lastHydratedSnapshot.empty() && m_lastHydratedSnapshot == envelope.id) {
        outSnapshotId = envelope.id;
        return AgenticResult::Ok(envelope.id);
    return true;
}

    if (!envelope.geometry.empty()) {
        if (!restoreGeometry(envelope.geometry)) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Failed to restore geometry from ") + envelope.absolutePath);
    return true;
}

    return true;
}

    if (!envelope.state.empty()) {
        if (!restoreState(envelope.state)) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Failed to restore dock state from ") + envelope.absolutePath);
    return true;
}

    return true;
}

    m_lastHydratedSnapshot = envelope.id;
    outSnapshotId = envelope.id;
    layoutRestored(envelope.id);
    return AgenticResult::Ok(envelope.id);
    return true;
}

} // namespace RawrXD::IDE


