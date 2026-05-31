// ExtensionInstaller.cpp — Implementation of ExtensionInstaller
//
// Uses WinINet for HTTP downloads, std::filesystem for extraction,
// and integrates with ExtensionLoader for activation.

#include "ExtensionInstaller.hpp"
#include "ExtensionUIState.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>

namespace RawrXD {

// ============================================================================
// Helpers
// ============================================================================

static std::string generateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else if (i == 14) {
            uuid += '4';
        } else if (i == 19) {
            uuid += hex[dis(gen) & 0x3 | 0x8];
        } else {
            uuid += hex[dis(gen)];
        }
    }
    return uuid;
}

static std::string urlToFilename(const std::string& url) {
    size_t lastSlash = url.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return url.substr(lastSlash + 1);
    }
    return "download.zip";
}

static bool createDirectoryRecursive(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (...) {
        return false;
    }
}

static std::string sanitizeId(const std::string& id) {
    std::string result;
    result.reserve(id.size());
    for (char c : id) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.') {
            result += c;
        } else {
            result += '_';
        }
    }
    return result;
}

// ============================================================================
// ExtensionManifest parsing
// ============================================================================

ExtensionManifest ExtensionManifest::fromJson(const std::string& jsonStr) {
    ExtensionManifest manifest;
    try {
        auto j = nlohmann::json::parse(jsonStr);

        // Identity
        if (j.contains("id")) manifest.id = j["id"].get<std::string>();
        if (j.contains("name")) manifest.name = j["name"].get<std::string>();
        if (j.contains("version")) manifest.version = j["version"].get<std::string>();
        if (j.contains("description")) manifest.description = j["description"].get<std::string>();
        if (j.contains("author")) manifest.author = j["author"].get<std::string>();
        if (j.contains("license")) manifest.license = j["license"].get<std::string>();
        if (j.contains("homepage")) manifest.homepage = j["homepage"].get<std::string>();
        if (j.contains("repository")) manifest.repository = j["repository"].get<std::string>();

        // Capabilities
        if (j.contains("capabilities") && j["capabilities"].is_array()) {
            for (const auto& cap : j["capabilities"]) {
                std::string capStr = cap.get<std::string>();
                if (capStr == "tool-provider") manifest.capabilities = manifest.capabilities | ExtensionCapability::ToolProvider;
                else if (capStr == "theme-provider") manifest.capabilities = manifest.capabilities | ExtensionCapability::ThemeProvider;
                else if (capStr == "language-support") manifest.capabilities = manifest.capabilities | ExtensionCapability::LanguageSupport;
                else if (capStr == "native-module") manifest.capabilities = manifest.capabilities | ExtensionCapability::NativeModule;
                else if (capStr == "script-module") manifest.capabilities = manifest.capabilities | ExtensionCapability::ScriptModule;
            }
        }

        // Requirements
        if (j.contains("requirements") && j["requirements"].is_array()) {
            for (const auto& req : j["requirements"]) {
                ExtensionRequirement r;
                if (req.contains("name")) r.name = req["name"].get<std::string>();
                if (req.contains("minVersion")) r.minVersion = req["minVersion"].get<std::string>();
                if (req.contains("maxVersion")) r.maxVersion = req["maxVersion"].get<std::string>();
                if (req.contains("optional")) r.optional = req["optional"].get<bool>();
                manifest.requirements.push_back(r);
            }
        }

        // Entry points
        if (j.contains("entryPoints") && j["entryPoints"].is_array()) {
            for (const auto& ep : j["entryPoints"]) {
                ExtensionEntryPoint e;
                if (ep.contains("type")) e.type = ep["type"].get<std::string>();
                if (ep.contains("path")) e.path = ep["path"].get<std::string>();
                if (ep.contains("exportName")) e.exportName = ep["exportName"].get<std::string>();
                if (ep.contains("args") && ep["args"].is_array()) {
                    for (const auto& arg : ep["args"]) {
                        e.args.push_back(arg.get<std::string>());
                    }
                }
                manifest.entryPoints.push_back(e);
            }
        }

        // Tools
        if (j.contains("tools") && j["tools"].is_array()) {
            for (const auto& tool : j["tools"]) {
                ExtensionToolRegistration t;
                if (tool.contains("name")) t.name = tool["name"].get<std::string>();
                if (tool.contains("description")) t.description = tool["description"].get<std::string>();
                if (tool.contains("schemaPath")) t.schemaPath = tool["schemaPath"].get<std::string>();
                if (tool.contains("handlerPath")) t.handlerPath = tool["handlerPath"].get<std::string>();
                manifest.tools.push_back(t);
            }
        }

        // Native
        if (j.contains("native")) {
            auto& native = j["native"];
            if (native.contains("dllPath")) manifest.nativeDllPath = native["dllPath"].get<std::string>();
            if (native.contains("entryPoint")) manifest.nativeEntryPoint = native["entryPoint"].get<std::string>();
        }

        // Metadata
        if (j.contains("keywords") && j["keywords"].is_array()) {
            for (const auto& kw : j["keywords"]) {
                manifest.keywords.push_back(kw.get<std::string>());
            }
        }
        if (j.contains("categories") && j["categories"].is_array()) {
            for (const auto& cat : j["categories"]) {
                manifest.categories.push_back(cat.get<std::string>());
            }
        }
        if (j.contains("icon")) manifest.iconPath = j["icon"].get<std::string>();
        if (j.contains("previewImage")) manifest.previewImagePath = j["previewImage"].get<std::string>();

    } catch (const std::exception& e) {
        // Return partially filled manifest; caller should call isValid()
        (void)e;
    }
    return manifest;
}

ExtensionManifest ExtensionManifest::fromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return ExtensionManifest{};
    std::stringstream buffer;
    buffer << f.rdbuf();
    return fromJson(buffer.str());
}

std::string ExtensionManifest::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["version"] = version;
    j["description"] = description;
    j["author"] = author;
    j["license"] = license;
    j["homepage"] = homepage;
    j["repository"] = repository;

    std::vector<std::string> caps;
    if (hasCapability(capabilities, ExtensionCapability::ToolProvider)) caps.push_back("tool-provider");
    if (hasCapability(capabilities, ExtensionCapability::ThemeProvider)) caps.push_back("theme-provider");
    if (hasCapability(capabilities, ExtensionCapability::LanguageSupport)) caps.push_back("language-support");
    if (hasCapability(capabilities, ExtensionCapability::NativeModule)) caps.push_back("native-module");
    if (hasCapability(capabilities, ExtensionCapability::ScriptModule)) caps.push_back("script-module");
    j["capabilities"] = caps;

    j["requirements"] = nlohmann::json::array();
    for (const auto& r : requirements) {
        nlohmann::json req;
        req["name"] = r.name;
        req["minVersion"] = r.minVersion;
        req["maxVersion"] = r.maxVersion;
        req["optional"] = r.optional;
        j["requirements"].push_back(req);
    }

    j["entryPoints"] = nlohmann::json::array();
    for (const auto& ep : entryPoints) {
        nlohmann::json e;
        e["type"] = ep.type;
        e["path"] = ep.path;
        e["exportName"] = ep.exportName;
        e["args"] = ep.args;
        j["entryPoints"].push_back(e);
    }

    j["tools"] = nlohmann::json::array();
    for (const auto& t : tools) {
        nlohmann::json tool;
        tool["name"] = t.name;
        tool["description"] = t.description;
        tool["schemaPath"] = t.schemaPath;
        tool["handlerPath"] = t.handlerPath;
        j["tools"].push_back(tool);
    }

    if (nativeDllPath) j["native"]["dllPath"] = *nativeDllPath;
    if (nativeEntryPoint) j["native"]["entryPoint"] = *nativeEntryPoint;

    j["keywords"] = keywords;
    j["categories"] = categories;
    if (iconPath) j["icon"] = *iconPath;
    if (previewImagePath) j["previewImage"] = *previewImagePath;

    return j.dump(2);
}

bool ExtensionManifest::isValid(std::string* outError) const {
    if (id.empty()) { if (outError) *outError = "Missing 'id'"; return false; }
    if (name.empty()) { if (outError) *outError = "Missing 'name'"; return false; }
    if (version.empty()) { if (outError) *outError = "Missing 'version'"; return false; }
    if (capabilities == ExtensionCapability::None) { if (outError) *outError = "No capabilities declared"; return false; }
    return true;
}

bool ExtensionManifest::isCompatibleWithHost(const std::string& hostVersion) const {
    for (const auto& req : requirements) {
        if (req.name == "rawrxd" || req.name == "win32ide") {
            if (!req.minVersion.empty()) {
                // Simple semver comparison (major.minor.patch)
                if (hostVersion < req.minVersion) return false;
            }
            if (!req.maxVersion.empty()) {
                if (hostVersion > req.maxVersion) return false;
            }
        }
    }
    return true;
}

// ============================================================================
// ExtensionInstaller
// ============================================================================

ExtensionInstaller::ExtensionInstaller()
    : m_extensionsRoot(GetExtensionsRoot()),
      m_registryPath(m_extensionsRoot + "registry.json"),
      m_tempDir(m_extensionsRoot + "temp\\")
{
    createDirectoryRecursive(m_extensionsRoot);
    createDirectoryRecursive(m_tempDir);
    m_loader = std::make_unique<ExtensionLoader>();
    loadRegistry();
}

ExtensionInstaller::~ExtensionInstaller() {
    saveRegistry();
}

std::string ExtensionInstaller::installFromUrl(const std::string& url,
                                                const std::string& expectedId,
                                                ExtensionProgressCallback onProgress,
                                                ExtensionLifecycleCallback onStateChange) {
    std::string taskId = generateUuid();

    ExtensionDownloadProgress progress;
    progress.taskId = taskId;
    progress.state = ExtensionDownloadState::Pending;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_downloads[taskId] = progress;
    }

    // Bridge to ExtensionUIState if no explicit callback provided
    ExtensionProgressCallback uiProgress = onProgress;
    ExtensionLifecycleCallback uiState = onStateChange;

    if (!uiProgress) {
        uiProgress = [this, taskId](const ExtensionDownloadProgress& p) {
            (void)taskId;
            // Mirror to global UI state
            auto& state = ExtensionUIState::Instance();
            float pct = (p.bytesTotal > 0)
                ? static_cast<float>(p.bytesDone) / static_cast<float>(p.bytesTotal)
                : 0.0f;
            state.updateProgress(p.taskId, pct, p.bytesDone, p.bytesTotal, p.speedBps);
        };
    }

    if (!uiState) {
        uiState = [](const std::string& extId, ExtensionLifecycleState st) {
            auto& state = ExtensionUIState::Instance();
            switch (st) {
                case ExtensionLifecycleState::Installed:
                    state.updateStatus(extId, ExtensionUIStatus::Installed);
                    break;
                case ExtensionLifecycleState::Active:
                    state.updateStatus(extId, ExtensionUIStatus::Active);
                    break;
                case ExtensionLifecycleState::Inactive:
                    state.updateStatus(extId, ExtensionUIStatus::Installed);
                    break;
                case ExtensionLifecycleState::DownloadFailed:
                case ExtensionLifecycleState::VerifyFailed:
                case ExtensionLifecycleState::InstallFailed:
                    state.updateStatus(extId, ExtensionUIStatus::Failed);
                    break;
                default:
                    break;
            }
        };
    }

    // Start background download
    std::thread worker(&ExtensionInstaller::downloadWorker, this,
                       taskId, url, expectedId, uiProgress, uiState);
    worker.detach();

    return taskId;
}

void ExtensionInstaller::downloadWorker(const std::string& taskId,
                                          const std::string& url,
                                          const std::string& expectedId,
                                          ExtensionProgressCallback onProgress,
                                          ExtensionLifecycleCallback onStateChange) {
    auto reportProgress = [&](ExtensionDownloadState state, uint64_t done, uint64_t total,
                               double speed, const std::string& error = {}) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_downloads.find(taskId);
        if (it != m_downloads.end()) {
            it->second.state = state;
            it->second.bytesDone = done;
            it->second.bytesTotal = total;
            it->second.speedBps = speed;
            if (!error.empty()) it->second.errorMessage = error;
        }
        if (onProgress) {
            onProgress(it->second);
        }
    };

    auto reportState = [&](const std::string& extId, ExtensionLifecycleState state) {
        if (onStateChange) {
            onStateChange(extId, state);
        }
    };

    // --- Download ---
    reportProgress(ExtensionDownloadState::Downloading, 0, 0, 0);

    std::string filename = urlToFilename(url);
    std::string tempPath = m_tempDir + taskId + "_" + filename;

    HINTERNET hInternet = InternetOpenA("RawrXD-ExtensionInstaller/1.0",
                                         INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        reportProgress(ExtensionDownloadState::Failed, 0, 0, 0, "Failed to initialize WinINet");
        return;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), nullptr, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        reportProgress(ExtensionDownloadState::Failed, 0, 0, 0, "Failed to open URL");
        return;
    }

    std::ofstream outFile(tempPath, std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        reportProgress(ExtensionDownloadState::Failed, 0, 0, 0, "Failed to create temp file");
        return;
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    uint64_t totalBytes = 0;
    uint64_t doneBytes = 0;
    auto startTime = std::chrono::steady_clock::now();

    // Try to get content length
    DWORD contentLength = 0;
    DWORD lengthSize = sizeof(contentLength);
    HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                   &contentLength, &lengthSize, nullptr);
    if (contentLength > 0) totalBytes = contentLength;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        doneBytes += bytesRead;

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - startTime).count();
        double speed = elapsed > 0 ? doneBytes / elapsed : 0;

        reportProgress(ExtensionDownloadState::Downloading, doneBytes, totalBytes, speed);
    }

    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (doneBytes == 0) {
        reportProgress(ExtensionDownloadState::Failed, 0, 0, 0, "Downloaded 0 bytes");
        return;
    }

    // --- Verify ---
    reportProgress(ExtensionDownloadState::Verifying, doneBytes, totalBytes, 0);

    if (m_verifySignatures) {
        std::string verifyError;
        if (!verifySignature(tempPath, &verifyError)) {
            reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                           "Signature verification failed: " + verifyError);
            return;
        }
    }

    // --- Extract + Install ---
    reportProgress(ExtensionDownloadState::Installing, doneBytes, totalBytes, 0);

    std::string extractDir = m_tempDir + taskId + "_extract\\";
    createDirectoryRecursive(extractDir);

    std::string extractError;
    if (!extractArchive(tempPath, extractDir, &extractError)) {
        reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                       "Extraction failed: " + extractError);
        return;
    }

    // Find manifest
    std::string manifestPath = extractDir + "manifest.json";
    if (!std::filesystem::exists(manifestPath)) {
        // Try nested: some archives have a single top-level dir
        for (const auto& entry : std::filesystem::directory_iterator(extractDir)) {
            if (entry.is_directory()) {
                std::string nestedManifest = entry.path().string() + "\\manifest.json";
                if (std::filesystem::exists(nestedManifest)) {
                    manifestPath = nestedManifest;
                    extractDir = entry.path().string() + "\\";
                    break;
                }
            }
        }
    }

    if (!std::filesystem::exists(manifestPath)) {
        reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                       "No manifest.json found in extension package");
        return;
    }

    ExtensionManifest manifest = ExtensionManifest::fromFile(manifestPath);
    std::string validationError;
    if (!manifest.isValid(&validationError)) {
        reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                       "Invalid manifest: " + validationError);
        return;
    }

    if (!expectedId.empty() && manifest.id != expectedId) {
        reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                       "Extension ID mismatch: expected " + expectedId + ", got " + manifest.id);
        return;
    }

    // Check compatibility
    if (!manifest.isCompatibleWithHost("14.7.3")) {
        reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                       "Extension incompatible with this RawrXD version");
        return;
    }

    // Install to final location
    std::string installDir = m_extensionsRoot + sanitizeId(manifest.id) + "\\";
    createDirectoryRecursive(installDir);

    // Remove old version if exists
    try {
        std::filesystem::remove_all(installDir);
    } catch (...) {}

    // Move extracted files to install dir
    try {
        std::filesystem::rename(extractDir, installDir);
    } catch (...) {
        // Fallback: copy
        try {
            std::filesystem::copy(extractDir, installDir,
                                  std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove_all(extractDir);
        } catch (const std::exception& e) {
            reportProgress(ExtensionDownloadState::Failed, doneBytes, totalBytes, 0,
                           "Failed to install files: " + std::string(e.what()));
            return;
        }
    }

    // Clean up temp files
    try {
        std::filesystem::remove(tempPath);
        std::filesystem::remove_all(m_tempDir + taskId + "_extract\\");
    } catch (...) {}

    // Register
    ExtensionRecord record;
    record.manifest = manifest;
    record.state = ExtensionLifecycleState::Installed;
    record.installPath = installDir;
    record.downloadUrl = url;
    record.installedSize = doneBytes;
    record.installedAt = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_installed[manifest.id] = record;
    }

    reportState(manifest.id, ExtensionLifecycleState::Installed);
    reportProgress(ExtensionDownloadState::Completed, doneBytes, totalBytes, 0);

    // Auto-activate if it has native module
    if (hasCapability(manifest.capabilities, ExtensionCapability::NativeModule)) {
        activate(manifest.id);
    }
}

bool ExtensionInstaller::installFromLocal(const std::string& path,
                                          std::string* outError) {
    std::string manifestPath = path + "\\manifest.json";
    if (!std::filesystem::exists(manifestPath)) {
        if (outError) *outError = "No manifest.json found at " + path;
        return false;
    }

    ExtensionManifest manifest = ExtensionManifest::fromFile(manifestPath);
    std::string validationError;
    if (!manifest.isValid(&validationError)) {
        if (outError) *outError = "Invalid manifest: " + validationError;
        return false;
    }

    std::string installDir = m_extensionsRoot + sanitizeId(manifest.id) + "\\";
    createDirectoryRecursive(installDir);

    try {
        std::filesystem::remove_all(installDir);
        std::filesystem::copy(path, installDir,
                              std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        if (outError) *outError = "Failed to copy files: " + std::string(e.what());
        return false;
    }

    ExtensionRecord record;
    record.manifest = manifest;
    record.state = ExtensionLifecycleState::Installed;
    record.installPath = installDir;
    record.installedAt = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_installed[manifest.id] = record;
    }

    saveRegistry();
    return true;
}

bool ExtensionInstaller::cancelInstall(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_downloads.find(taskId);
    if (it == m_downloads.end()) return false;
    it->second.state = ExtensionDownloadState::Cancelled;
    return true;
}

bool ExtensionInstaller::activate(const std::string& extId, std::string* outError) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it == m_installed.end()) {
        if (outError) *outError = "Extension not installed";
        return false;
    }

    ExtensionRecord& record = it->second;
    if (record.state == ExtensionLifecycleState::Active) {
        return true;
    }

    record.state = ExtensionLifecycleState::Activating;

    // If native module, delegate to ExtensionLoader
    if (hasCapability(record.manifest.capabilities, ExtensionCapability::NativeModule)) {
        // ExtensionLoader scans the directory and loads native_manifest.json
        m_loader->Scan();
        m_loader->LoadNativeModules();
    }

    record.state = ExtensionLifecycleState::Active;
    record.lastActivatedAt = std::chrono::system_clock::now();
    record.activationCount++;

    saveRegistry();
    return true;
}

bool ExtensionInstaller::deactivate(const std::string& extId, std::string* outError) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it == m_installed.end()) {
        if (outError) *outError = "Extension not installed";
        return false;
    }

    ExtensionRecord& record = it->second;
    if (record.state != ExtensionLifecycleState::Active) {
        return true;
    }

    record.state = ExtensionLifecycleState::Deactivating;

    // Unload from ExtensionLoader
    m_loader->UnloadExtension(extId);

    record.state = ExtensionLifecycleState::Inactive;
    saveRegistry();
    return true;
}

bool ExtensionInstaller::uninstall(const std::string& extId, std::string* outError) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it == m_installed.end()) {
        if (outError) *outError = "Extension not installed";
        return false;
    }

    ExtensionRecord& record = it->second;
    record.state = ExtensionLifecycleState::Uninstalling;

    // Deactivate first
    if (record.state == ExtensionLifecycleState::Active) {
        m_loader->UnloadExtension(extId);
    }

    // Delete files
    try {
        std::filesystem::remove_all(record.installPath);
    } catch (const std::exception& e) {
        if (outError) *outError = "Failed to remove files: " + std::string(e.what());
        record.state = ExtensionLifecycleState::InstallFailed;
        return false;
    }

    m_installed.erase(it);
    saveRegistry();
    return true;
}

std::string ExtensionInstaller::update(const std::string& extId,
                                         ExtensionProgressCallback onProgress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it == m_installed.end()) {
        return {};
    }

    const ExtensionRecord& record = it->second;
    if (record.downloadUrl.empty()) {
        return {};
    }

    // Deactivate old version
    if (record.state == ExtensionLifecycleState::Active) {
        m_loader->UnloadExtension(extId);
    }

    // Re-install from same URL
    return installFromUrl(record.downloadUrl, extId, onProgress);
}

std::vector<ExtensionRecord> ExtensionInstaller::listInstalled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExtensionRecord> result;
    result.reserve(m_installed.size());
    for (const auto& kv : m_installed) {
        result.push_back(kv.second);
    }
    return result;
}

std::optional<ExtensionRecord> ExtensionInstaller::getRecord(const std::string& extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it != m_installed.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<ExtensionDownloadProgress> ExtensionInstaller::getProgress(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_downloads.find(taskId);
    if (it != m_downloads.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ExtensionInstaller::isInstalled(const std::string& extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_installed.count(extId) > 0;
}

bool ExtensionInstaller::isActive(const std::string& extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it == m_installed.end()) return false;
    return it->second.state == ExtensionLifecycleState::Active;
}

bool ExtensionInstaller::saveRegistry(std::string* outError) {
    nlohmann::json j = nlohmann::json::array();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& kv : m_installed) {
            const ExtensionRecord& r = kv.second;
            nlohmann::json entry;
            entry["manifest"] = nlohmann::json::parse(r.manifest.toJson());
            entry["state"] = static_cast<int>(r.state);
            entry["installPath"] = r.installPath;
            entry["downloadUrl"] = r.downloadUrl;
            entry["installedSize"] = r.installedSize;
            entry["activationCount"] = r.activationCount;
            entry["lastError"] = r.lastError;
            j.push_back(entry);
        }
    }

    try {
        std::ofstream f(m_registryPath);
        if (!f.is_open()) {
            if (outError) *outError = "Failed to open registry file";
            return false;
        }
        f << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        if (outError) *outError = std::string("Failed to save registry: ") + e.what();
        return false;
    }
}

bool ExtensionInstaller::loadRegistry(std::string* outError) {
    if (!std::filesystem::exists(m_registryPath)) {
        return true; // No registry yet — that's fine
    }

    try {
        std::ifstream f(m_registryPath);
        if (!f.is_open()) {
            if (outError) *outError = "Failed to open registry file";
            return false;
        }

        nlohmann::json j;
        try {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            j = nlohmann::json::parse(content);
        } catch (...) {
            if (outError) *outError = "Failed to parse registry JSON";
            return false;
        }

        if (!j.is_array()) {
            if (outError) *outError = "Invalid registry format";
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_installed.clear();

        for (const auto& entry : j) {
            ExtensionRecord record;
            if (entry.contains("manifest")) {
                record.manifest = ExtensionManifest::fromJson(entry["manifest"].dump());
            }
            if (entry.contains("state")) record.state = static_cast<ExtensionLifecycleState>(entry["state"].get<int>());
            if (entry.contains("installPath")) record.installPath = entry["installPath"].get<std::string>();
            if (entry.contains("downloadUrl")) record.downloadUrl = entry["downloadUrl"].get<std::string>();
            if (entry.contains("installedSize")) record.installedSize = entry["installedSize"].get<uint64_t>();
            if (entry.contains("activationCount")) record.activationCount = entry["activationCount"].get<int>();
            if (entry.contains("lastError")) record.lastError = entry["lastError"].get<std::string>();

            if (!record.manifest.id.empty()) {
                m_installed[record.manifest.id] = record;
            }
        }

        return true;
    } catch (const std::exception& e) {
        if (outError) *outError = std::string("Failed to load registry: ") + e.what();
        return false;
    }
}

// ============================================================================
// Private helpers
// ============================================================================

bool ExtensionInstaller::verifySignature(const std::string& path, std::string* outError) {
    // Delegate to existing plugin signature verifier
    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (!verifier.isInitialized()) {
        verifier.initialize(
            RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy());
    }

    std::wstring wPath;
    try {
        wPath = std::filesystem::path(path).wstring();
    } catch (...) {
        wchar_t wbuf[MAX_PATH * 4] = {};
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wbuf, MAX_PATH * 4);
        if (wlen > 0) wPath.assign(wbuf);
    }

    if (wPath.empty()) {
        if (outError) *outError = "Failed to convert path to wide";
        return false;
    }

    auto result = verifier.verify(wPath.c_str());
    if (!verifier.shouldAllowInstall(result)) {
        if (outError) *outError = "Signature verification failed";
        return false;
    }
    return true;
}

bool ExtensionInstaller::extractArchive(const std::string& archivePath,
                                        const std::string& destDir,
                                        std::string* outError) {
    // For .zip files, use Windows Shell API
    // For simplicity, assume .zip for now
    if (archivePath.size() > 4 &&
        archivePath.substr(archivePath.size() - 4) == ".zip") {
        // Use PowerShell Expand-Archive as a portable fallback
        std::string cmd = "powershell -Command \"Expand-Archive -Path '\"" + archivePath + "\'' -DestinationPath '\"" + destDir + "\'' -Force\"";
        int result = std::system(cmd.c_str());
        if (result != 0) {
            if (outError) *outError = "Failed to extract archive (code " + std::to_string(result) + ")";
            return false;
        }
        return true;
    }

    // For directories, just copy
    if (std::filesystem::is_directory(archivePath)) {
        try {
            std::filesystem::copy(archivePath, destDir,
                                  std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
            return true;
        } catch (const std::exception& e) {
            if (outError) *outError = std::string("Copy failed: ") + e.what();
            return false;
        }
    }

    if (outError) *outError = "Unsupported archive format";
    return false;
}

bool ExtensionInstaller::validateManifest(const ExtensionManifest& manifest, std::string* outError) {
    return manifest.isValid(outError);
}

std::string ExtensionInstaller::generateTaskId() {
    return generateUuid();
}

void ExtensionInstaller::setState(const std::string& extId, ExtensionLifecycleState state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it != m_installed.end()) {
        it->second.state = state;
    }
}

void ExtensionInstaller::setError(const std::string& extId, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_installed.find(extId);
    if (it != m_installed.end()) {
        it->second.lastError = error;
    }
}

// ============================================================================
// Settings
// ============================================================================

void ExtensionInstaller::setTrustedSources(const std::vector<ExtensionSource>& sources) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trustedSources = sources;
}

void ExtensionInstaller::setAutoUpdate(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoUpdate = enabled;
}

void ExtensionInstaller::setVerifySignatures(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_verifySignatures = enabled;
}

// ============================================================================
// Tool registry integration (stub — wire to AgentToolHandlers)
// ============================================================================

void ExtensionInstaller::registerToolsWithAgenticSystem(class AgentToolHandlers* handlers) {
    (void)handlers;
    // TODO: Iterate active extensions, register their tools with the agentic system
    // This requires AgentToolHandlers to expose a dynamic registration API
}

// ============================================================================
// Search / discovery (stub — requires marketplace API)
// ============================================================================

std::vector<ExtensionManifest> ExtensionInstaller::search(const std::string& query, int maxResults) {
    (void)query;
    (void)maxResults;
    // TODO: Query marketplace API
    return {};
}

std::vector<ExtensionManifest> ExtensionInstaller::checkForUpdates() {
    // TODO: Query marketplace for newer versions of installed extensions
    return {};
}

} // namespace RawrXD
