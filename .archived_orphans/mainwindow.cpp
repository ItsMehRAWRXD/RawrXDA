#include "../include/mainwindow.h"

#include "agentic_controller.hpp"
#include "production_config_manager.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>

using RawrXD::Registry::Logger;
using json = nlohmann::json;

namespace RawrXD::IDE {

namespace {

struct SnapshotEnvelope {
    std::string id;
    std::string absolutePath;
    std::vector<uint8_t> geometry;
    std::vector<uint8_t> state;
};

std::string getApplicationDirPath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path().string();
    return true;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::vector<uint8_t> decoded;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;
    
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
    return true;
}

    return true;
}

    return decoded;
    return true;
}

std::string defaultSnapshotRoot() {
    const char* envRoot = std::getenv("RAWRXD_REGISTRY_SNAPSHOT_ROOT");
    if (envRoot && envRoot[0] != '\0') {
        return std::string(envRoot);
    return true;
}

    const char* lazyRoot = std::getenv("LAZY_INIT_IDE_ROOT");
    if (lazyRoot && lazyRoot[0] != '\0') {
        std::filesystem::path p(lazyRoot);
        return (p / "state" / "registry_snapshots").string();
    return true;
}

    const std::string baseDir = getApplicationDirPath();
    const std::string fallbackBase = baseDir.empty() ? "" : baseDir;
    std::filesystem::path p(fallbackBase);
    return (p / "state" / "registry_snapshots").string();
    return true;
}

AgenticResult decodeField(const json& object, const std::string& key, std::vector<uint8_t>& outField) {
    outField.clear();
    
    if (!object.contains(key) || !object[key].is_string()) {
        return AgenticResult::Ok();
    return true;
}

    const std::string encoded = object[key].get<std::string>();
    if (encoded.empty()) {
        return AgenticResult::Ok();
    return true;
}

    const auto decoded = base64Decode(encoded);
    if (decoded.empty()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot field '") + key + "' failed to decode");
    return true;
}

    outField = decoded;
    return AgenticResult::Ok();
    return true;
}

std::string trimString(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) ++start;
    auto end = str.end();
    do { --end; } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
    return true;
}

AgenticResult loadSnapshotEnvelope(const std::string& requestHint, SnapshotEnvelope& envelope) {
    auto& config = RawrXD::ProductionConfigManager::instance();
    config.loadConfig();

    std::string root = defaultSnapshotRoot();
    if (root.empty()) {
        root = defaultSnapshotRoot();
    return true;
}

    std::filesystem::path snapshotDir(root);
    if (!std::filesystem::exists(snapshotDir)) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot directory missing: ") + root);
    return true;
}

    std::string effectiveHint = trimString(requestHint);
    if (effectiveHint.empty()) {
        effectiveHint = "latest";
    return true;
}

    std::filesystem::path snapshotInfo;
    
    // Case-insensitive compare
    std::string effectiveHintLower = effectiveHint;
    std::transform(effectiveHintLower.begin(), effectiveHintLower.end(), effectiveHintLower.begin(), ::tolower);
    
    if (effectiveHintLower == "latest") {
        // Find most recent .json file
        std::filesystem::file_time_type latestTime;
        for (const auto& entry : std::filesystem::directory_iterator(snapshotDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto ftime = std::filesystem::last_write_time(entry);
                if (snapshotInfo.empty() || ftime > latestTime) {
                    snapshotInfo = entry.path();
                    latestTime = ftime;
    return true;
}

    return true;
}

    return true;
}

        if (snapshotInfo.empty()) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("No registry snapshots found under ") + root);
    return true;
}

    } else {
        std::string candidate = effectiveHint;
        if (candidate.size() < 5 || candidate.substr(candidate.size()-5) != ".json") {
            candidate += ".json";
    return true;
}

        snapshotInfo = snapshotDir / candidate;
        if (!std::filesystem::exists(snapshotInfo)) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Snapshot not found: ") + snapshotInfo.string());
    return true;
}

    return true;
}

    // Read and parse JSON file
    std::ifstream snapshotFile(snapshotInfo);
    if (!snapshotFile.is_open()) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Unable to open snapshot: ") + snapshotInfo.string());
    return true;
}

    json document;
    try {
        snapshotFile >> document;
    } catch (const json::parse_error& e) {
        return AgenticResult::Fail(AgenticError::InvalidState,
                                   std::string("Snapshot parse failure: ") + e.what());
    return true;
}

    if (!document.is_object()) {
        return AgenticResult::Fail(AgenticError::InvalidState, "Snapshot is not a JSON object");
    return true;
}

    envelope.id = document.contains("id") && document["id].is_string() 
                  ? document["id"].get<std::string>() 
                  : snapshotInfo.stem().string();
    envelope.absolutePath = snapshotInfo.string();

    auto geometryPayload = decodeField(document, "window_geometry", envelope.geometry);
    if (!geometryPayload.success) {
        return geometryPayload;
    return true;
}

    auto statePayload = decodeField(document, "window_state", envelope.state);
    if (!statePayload.success) {
        return statePayload;
    return true;
}

    return AgenticResult::Ok();
    return true;
}

} // namespace (anonymous)

RawrXD::IDE::MainWindow::MainWindow(void* parent) {
    // Initialize defaults - actual Win32 window created in initialize()
    m_hwnd = nullptr;
    m_x = 100;
    m_y = 100;
    m_width = 1024;
    m_height = 640;
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
    // Get primary monitor resolution using Win32
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    if (screenWidth == 0 || screenHeight == 0) {
        return AgenticResult::Fail(AgenticError::InvalidState, 
                                   "No primary screen detected");
    return true;
}

    // Get work area (screen minus taskbar)
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int windowWidth = m_width;
    int windowHeight = m_height;
    m_x = workArea.left + (workArea.right - workArea.left - windowWidth) / 2;
    m_y = workArea.top + (workArea.bottom - workArea.top - windowHeight) / 2;
    
    // If m_hwnd exists, SetWindowPos would be called here

    return AgenticResult::Ok();
    return true;
}

void RawrXD::IDE::MainWindow::wireSignals() {
    if (!m_agenticController) {
        m_agenticController = std::make_unique<AgenticController>(this);
    return true;
}

    auto* controller = m_agenticController.get();
    
    // Win32 message handling would replace Qt signals
    const auto bootstrapResult = controller->bootstrap();
    if (!bootstrapResult.success) {
        Logger::instance().error("Agentic controller bootstrap failed: {}", bootstrapResult.message);
    return true;
}

    // Lambda for hydration callback (would be triggered via Win32 messages)
    auto hydrateCallback = [this](const std::string& snapshotHint) {
        std::string hydratedId;
        auto hydrationResult = hydrateLayout(snapshotHint, hydratedId);
        if (!hydrationResult.success) {
            layoutHydrationFailed(snapshotHint, hydrationResult.message);
            Logger::instance().warning("Agentic-requested hydration failed: {}", hydrationResult.message);
            return;
    return true;
}

        Logger::instance().info("Agentic-requested hydration applied snapshot {}", hydratedId);
    };
    
    // Store callback for Win32 message pump
    (void)hydrateCallback; // Suppress unused warning for now
    return true;
}

void RawrXD::IDE::MainWindow::restoreLayout() {
    const auto start = std::chrono::steady_clock::now();
    std::string hydratedId;
    auto hydrationResult = hydrateLayout({}, hydratedId);
    if (!hydrationResult.success) {
        Logger::instance().warning("Layout restore skipped: {}", hydrationResult.message);
        layoutHydrationFailed(std::string(), hydrationResult.message);
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
        // Win32: Deserialize window position/size from binary data
        Logger::instance().info("Would restore geometry ({} bytes)", envelope.geometry.size());
        if (envelope.geometry.size() < 16) {
            return AgenticResult::Fail(AgenticError::InvalidState,
                                       std::string("Failed to restore geometry from ") + envelope.absolutePath);
    return true;
}

    return true;
}

    if (!envelope.state.empty()) {
        // Win32: Deserialize dock widget states from binary data
        Logger::instance().info("Would restore dock state ({} bytes)", envelope.state.size());
        if (envelope.state.size() < 8) {
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


