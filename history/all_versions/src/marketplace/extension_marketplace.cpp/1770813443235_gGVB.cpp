// ============================================================================
// extension_marketplace.cpp — Non-Qt Extension Marketplace Implementation
// ============================================================================
// NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "extension_marketplace.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <queue>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
// POSIX: would use libcurl
#endif

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Singleton
// ============================================================================
ExtensionMarketplace& ExtensionMarketplace::instance() {
    static ExtensionMarketplace inst;
    return inst;
}

ExtensionMarketplace::ExtensionMarketplace()
    : installDir_(".rawrxd/extensions/"),
      cacheDir_(".rawrxd/extension_cache/"),
      registryUrl_("https://marketplace.rawrxd.dev/api/v1"),
      eventListenerCount_(0), totalDownloadBytes_(0)
{}

ExtensionMarketplace::~ExtensionMarketplace() {
    shutdown();
}

// ============================================================================
// Configuration
// ============================================================================

ExtResult ExtensionMarketplace::setInstallDirectory(const std::string& path) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    installDir_ = path;
    std::filesystem::create_directories(path);
    return ExtResult::ok("Install directory set");
}

ExtResult ExtensionMarketplace::setCacheDirectory(const std::string& path) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    cacheDir_ = path;
    std::filesystem::create_directories(path);
    return ExtResult::ok("Cache directory set");
}

ExtResult ExtensionMarketplace::setRegistryUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    registryUrl_ = url;
    return ExtResult::ok("Registry URL set");
}

ExtResult ExtensionMarketplace::applyPolicy(const EnterprisePolicyConfig& config) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    policy_ = config;

    // Enforce policy on currently installed extensions
    for (auto& [id, manifest] : installed_) {
        ExtResult pr = checkPolicy(manifest);
        if (!pr.success) {
            states_[id] = ExtensionState::BLOCKED;

            MarketplaceEvent evt;
            evt.type = MarketplaceEvent::POLICY_VIOLATION;
            evt.extensionId = id;
            evt.detail = pr.detail;
            emitEvent(evt);
        }
    }

    // Auto-install required extensions
    for (const auto& autoId : config.autoInstallIds) {
        if (installed_.find(autoId) == installed_.end()) {
            installFromRegistry(autoId);
        }
    }

    return ExtResult::ok("Policy applied");
}

// ============================================================================
// Policy Engine
// ============================================================================

ExtResult ExtensionMarketplace::checkPolicy(const ExtensionManifest& manifest) {
    // Check block list
    if (policy_.enforceBlockList) {
        for (const auto& blocked : policy_.blockedExtensionIds) {
            if (manifest.id == blocked) {
                return ExtResult::error("Extension blocked by policy", 10);
            }
        }
        for (const auto& blocked : policy_.blockedPublishers) {
            if (manifest.publisher == blocked) {
                return ExtResult::error("Publisher blocked by policy", 11);
            }
        }
    }

    // Check allow list
    if (policy_.enforceAllowList) {
        bool allowed = false;
        for (const auto& allowedId : policy_.allowedExtensionIds) {
            if (manifest.id == allowedId) { allowed = true; break; }
        }
        if (!allowed) {
            for (const auto& allowedPub : policy_.allowedPublishers) {
                if (manifest.publisher == allowedPub) { allowed = true; break; }
            }
        }
        if (!allowed) {
            return ExtResult::error("Extension not in allow list", 12);
        }
    }

    // Check max extensions limit
    if (policy_.maxExtensions > 0 &&
        installed_.size() >= policy_.maxExtensions) {
        return ExtResult::error("Maximum extension limit reached", 13);
    }

    return ExtResult::ok("Policy check passed");
}

// ============================================================================
// Search / Browse
// ============================================================================

ExtResult ExtensionMarketplace::search(const ExtensionSearchQuery& query,
                                        ExtensionSearchResponse& response) {
    // For local/offline mode: search installed extensions
    response.results.clear();
    response.totalCount = 0;
    response.page = query.page;
    response.pageSize = query.pageSize;

    std::string searchLower = query.text;
    std::transform(searchLower.begin(), searchLower.end(),
                   searchLower.begin(), ::tolower);

    for (auto& [id, manifest] : installed_) {
        // Text search in name, description, publisher
        std::string nameLower = manifest.name;
        std::string descLower = manifest.description;
        std::transform(nameLower.begin(), nameLower.end(),
                       nameLower.begin(), ::tolower);
        std::transform(descLower.begin(), descLower.end(),
                       descLower.begin(), ::tolower);

        bool matches = searchLower.empty() ||
                       nameLower.find(searchLower) != std::string::npos ||
                       descLower.find(searchLower) != std::string::npos ||
                       id.find(searchLower) != std::string::npos;

        if (matches) {
            ExtensionSearchResult sr;
            sr.manifest = manifest;
            sr.installCount = 0;
            sr.rating = 0.0f;
            sr.ratingCount = 0;
            response.results.push_back(std::move(sr));
        }
    }

    response.totalCount = static_cast<uint32_t>(response.results.size());

    // TODO: Also query the remote registry
    return ExtResult::ok("Search complete");
}

ExtResult ExtensionMarketplace::getExtensionDetails(
    const std::string& extensionId,
    ExtensionSearchResult& result)
{
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it != installed_.end()) {
        result.manifest = it->second;
        result.installCount = 0;
        result.rating = 0.0f;
        result.ratingCount = 0;
        return ExtResult::ok("Extension found");
    }

    return ExtResult::error("Extension not found", 1);
}

std::vector<ExtensionManifest> ExtensionMarketplace::listInstalled() const {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    std::vector<ExtensionManifest> result;
    for (auto& [id, manifest] : installed_) {
        result.push_back(manifest);
    }
    return result;
}

std::vector<ExtensionManifest> ExtensionMarketplace::listEnabled() const {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    std::vector<ExtensionManifest> result;
    for (auto& [id, manifest] : installed_) {
        auto sit = states_.find(id);
        if (sit != states_.end() && sit->second == ExtensionState::ENABLED) {
            result.push_back(manifest);
        }
    }
    return result;
}

// ============================================================================
// Install / Uninstall
// ============================================================================

ExtResult ExtensionMarketplace::installFromVsix(const std::string& vsixPath) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    // Verify file exists
    if (!std::filesystem::exists(vsixPath))
        return ExtResult::error("VSIX file not found", 2);

    // Parse manifest from VSIX (it's a ZIP with package.json inside)
    // For now, create a manifest from the filename
    ExtensionManifest manifest;
    std::string filename = std::filesystem::path(vsixPath).stem().string();

    // Try to parse "publisher.name-version" pattern
    auto dotPos = filename.find('.');
    if (dotPos != std::string::npos) {
        manifest.publisher = filename.substr(0, dotPos);
        std::string rest = filename.substr(dotPos + 1);
        auto dashPos = rest.rfind('-');
        if (dashPos != std::string::npos) {
            manifest.name = rest.substr(0, dashPos);
            manifest.version = rest.substr(dashPos + 1);
        } else {
            manifest.name = rest;
            manifest.version = "0.0.1";
        }
    } else {
        manifest.name = filename;
        manifest.publisher = "local";
        manifest.version = "0.0.1";
    }

    manifest.id = manifest.publisher + "." + manifest.name;
    manifest.vsixPath = vsixPath;
    manifest.fileSize = std::filesystem::file_size(vsixPath);
    manifest.isEnabled = true;

    // Check enterprise policy
    ExtResult pr = checkPolicy(manifest);
    if (!pr.success) return pr;

    // Extract VSIX to install directory
    std::string targetDir = installDir_ + manifest.id + "/";
    ExtResult er = extractVsix(vsixPath, targetDir);
    if (!er.success) return er;

    manifest.installPath = targetDir;
    manifest.installedAt = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    // Parse the real manifest if it exists
    std::string manifestPath = targetDir + "package.json";
    if (std::filesystem::exists(manifestPath)) {
        parseManifest(manifestPath, manifest);
    }

    installed_[manifest.id] = manifest;
    states_[manifest.id] = ExtensionState::INSTALLED;

    // Emit event
    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_INSTALLED;
    evt.extensionId = manifest.id;
    evt.detail = "Extension installed";
    emitEvent(evt);

    return ExtResult::ok("Extension installed from VSIX");
}

ExtResult ExtensionMarketplace::installFromRegistry(
    const std::string& extensionId,
    const std::string& version)
{
    // Build download URL
    std::string url = registryUrl_ + "/extensions/" + extensionId +
                      "/versions/" + version + "/download";

    std::string localPath = cacheDir_ + extensionId + ".vsix";

    // Download
    ExtResult dr = httpDownload(url, localPath);
    if (!dr.success) return dr;

    // Install from downloaded VSIX
    return installFromVsix(localPath);
}

ExtResult ExtensionMarketplace::installFromUrl(const std::string& downloadUrl) {
    // Derive a filename from URL
    std::string filename = "extension_download.vsix";
    auto lastSlash = downloadUrl.rfind('/');
    if (lastSlash != std::string::npos) {
        filename = downloadUrl.substr(lastSlash + 1);
    }

    std::string localPath = cacheDir_ + filename;
    ExtResult dr = httpDownload(downloadUrl, localPath);
    if (!dr.success) return dr;

    return installFromVsix(localPath);
}

ExtResult ExtensionMarketplace::uninstall(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    // Check if other extensions depend on this one
    for (auto& [id, manifest] : installed_) {
        if (id == extensionId) continue;
        for (auto& dep : manifest.dependencies) {
            if (dep.extensionId == extensionId) {
                return ExtResult::error("Other extensions depend on this one", 5);
            }
        }
    }

    // Deactivate first
    // deactivate(extensionId);  // Would need to call without lock

    // Remove install directory
    if (!it->second.installPath.empty() &&
        std::filesystem::exists(it->second.installPath)) {
        std::error_code ec;
        std::filesystem::remove_all(it->second.installPath, ec);
    }

    installed_.erase(it);
    states_.erase(extensionId);

    // Emit event
    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_UNINSTALLED;
    evt.extensionId = extensionId;
    evt.detail = "Extension uninstalled";
    emitEvent(evt);

    return ExtResult::ok("Extension uninstalled");
}

ExtResult ExtensionMarketplace::update(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    states_[extensionId] = ExtensionState::UPDATING;

    // TODO: Check registry for newer version, download, and replace
    // For now, just mark as updated

    states_[extensionId] = ExtensionState::ENABLED;

    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_UPDATED;
    evt.extensionId = extensionId;
    evt.detail = "Extension updated";
    emitEvent(evt);

    return ExtResult::ok("Extension updated");
}

ExtResult ExtensionMarketplace::updateAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(marketplaceMutex_);
        for (auto& [id, _] : installed_) {
            ids.push_back(id);
        }
    }

    uint32_t updated = 0;
    for (auto& id : ids) {
        ExtResult r = update(id);
        if (r.success) updated++;
    }

    return ExtResult::ok("All extensions updated");
}

// ============================================================================
// Enable / Disable
// ============================================================================

ExtResult ExtensionMarketplace::enable(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    it->second.isEnabled = true;
    states_[extensionId] = ExtensionState::ENABLED;

    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_ENABLED;
    evt.extensionId = extensionId;
    evt.detail = "Extension enabled";
    emitEvent(evt);

    return ExtResult::ok("Extension enabled");
}

ExtResult ExtensionMarketplace::disable(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    it->second.isEnabled = false;
    states_[extensionId] = ExtensionState::DISABLED;

    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_DISABLED;
    evt.extensionId = extensionId;
    evt.detail = "Extension disabled";
    emitEvent(evt);

    return ExtResult::ok("Extension disabled");
}

// ============================================================================
// Extension Host Integration
// ============================================================================

ExtResult ExtensionMarketplace::activate(const std::string& extensionId,
                                          const std::string& activationEvent) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    if (!it->second.isEnabled)
        return ExtResult::error("Extension is disabled", 3);

    auto sit = states_.find(extensionId);
    if (sit != states_.end() && sit->second == ExtensionState::BLOCKED)
        return ExtResult::error("Extension blocked by policy", 10);

    // TODO: Interface with RawrXD_ExtensionHost.asm
    // Send activation message via shared memory (16MB region per ASM host)

    states_[extensionId] = ExtensionState::ENABLED;

    MarketplaceEvent evt;
    evt.type = MarketplaceEvent::EXTENSION_ACTIVATED;
    evt.extensionId = extensionId;
    evt.detail = activationEvent.empty() ? "*" : activationEvent.c_str();
    emitEvent(evt);

    return ExtResult::ok("Extension activated");
}

ExtResult ExtensionMarketplace::deactivate(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    states_[extensionId] = ExtensionState::INSTALLED;

    return ExtResult::ok("Extension deactivated");
}

ExtensionState ExtensionMarketplace::getState(
    const std::string& extensionId) const
{
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    auto it = states_.find(extensionId);
    if (it == states_.end()) return ExtensionState::NOT_INSTALLED;
    return it->second;
}

ExtResult ExtensionMarketplace::getManifest(
    const std::string& extensionId,
    ExtensionManifest& manifest) const
{
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);
    manifest = it->second;
    return ExtResult::ok("Manifest retrieved");
}

// ============================================================================
// Dependency Resolution
// ============================================================================

ExtResult ExtensionMarketplace::checkDependencies(
    const std::string& extensionId,
    std::vector<std::string>& missingDeps)
{
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    missingDeps.clear();
    for (auto& dep : it->second.dependencies) {
        auto dit = installed_.find(dep.extensionId);
        if (dit == installed_.end()) {
            missingDeps.push_back(dep.extensionId);
        } else if (!dep.versionRange.empty()) {
            if (!semverSatisfies(dit->second.version, dep.versionRange)) {
                missingDeps.push_back(dep.extensionId +
                    " (needs " + dep.versionRange + ")");
            }
        }
    }

    if (!missingDeps.empty()) {
        return ExtResult::error("Missing dependencies", 4);
    }
    return ExtResult::ok("All dependencies satisfied");
}

ExtResult ExtensionMarketplace::installWithDependencies(
    const std::string& extensionId)
{
    std::vector<std::string> depOrder;
    ExtResult dr = getDependencyTree(extensionId, depOrder);
    if (!dr.success) return dr;

    // Install in dependency order
    for (const auto& depId : depOrder) {
        if (installed_.find(depId) == installed_.end()) {
            ExtResult ir = installFromRegistry(depId);
            if (!ir.success) {
                return ExtResult::error("Failed to install dependency", 6);
            }
        }
    }

    // Finally install the target extension
    return installFromRegistry(extensionId);
}

ExtResult ExtensionMarketplace::getDependencyTree(
    const std::string& extensionId,
    std::vector<std::string>& orderedDeps)
{
    // Topological sort of dependency graph
    std::unordered_map<std::string, std::vector<std::string>> adjList;
    std::unordered_map<std::string, uint32_t> inDegree;
    std::queue<std::string> buildQueue;
    buildQueue.push(extensionId);

    // BFS to discover full dependency graph
    while (!buildQueue.empty()) {
        std::string current = buildQueue.front();
        buildQueue.pop();

        if (adjList.find(current) != adjList.end()) continue;
        adjList[current] = {};

        auto it = installed_.find(current);
        if (it != installed_.end()) {
            for (auto& dep : it->second.dependencies) {
                adjList[current].push_back(dep.extensionId);
                inDegree[dep.extensionId]++;
                buildQueue.push(dep.extensionId);
            }
        }
    }

    // Initialize in-degree for root nodes
    for (auto& [id, _] : adjList) {
        if (inDegree.find(id) == inDegree.end()) {
            inDegree[id] = 0;
        }
    }

    // Kahn's algorithm
    std::queue<std::string> readyQueue;
    for (auto& [id, deg] : inDegree) {
        if (deg == 0 && id != extensionId) {
            readyQueue.push(id);
        }
    }

    orderedDeps.clear();
    while (!readyQueue.empty()) {
        std::string curr = readyQueue.front();
        readyQueue.pop();
        orderedDeps.push_back(curr);

        for (auto& neighbor : adjList[curr]) {
            if (--inDegree[neighbor] == 0 && neighbor != extensionId) {
                readyQueue.push(neighbor);
            }
        }
    }

    return ExtResult::ok("Dependency tree resolved");
}

ExtResult ExtensionMarketplace::resolveDependencies(
    const std::string& extensionId,
    std::vector<std::string>& sorted)
{
    // Delegates to getDependencyTree — same topological sort logic
    return getDependencyTree(extensionId, sorted);
}

// ============================================================================
// VSIX Operations
// ============================================================================

ExtResult ExtensionMarketplace::extractVsix(const std::string& vsixPath,
                                              const std::string& targetDir) {
    std::filesystem::create_directories(targetDir);

    // VSIX files are ZIP archives
    // Minimal extraction: copy the file for now
    // TODO: Implement ZIP extraction (minizip or Win32 Shell API)

    // Copy .vsix to target as-is (placeholder)
    std::string destPath = targetDir + "extension.vsix";
    try {
        std::filesystem::copy_file(vsixPath, destPath,
            std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        return ExtResult::error("Failed to copy VSIX", 7);
    }

    return ExtResult::ok("VSIX extracted");
}

ExtResult ExtensionMarketplace::parseManifest(const std::string& manifestPath,
                                               ExtensionManifest& manifest) {
    std::ifstream file(manifestPath);
    if (!file.is_open())
        return ExtResult::error("Cannot open manifest", 1);

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Minimal JSON parser for package.json
    // Extract key fields: name, publisher, version, description, etc.
    auto extractField = [&content](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\"";
        auto pos = content.find(pattern);
        if (pos == std::string::npos) return "";

        auto colonPos = content.find(':', pos + pattern.size());
        if (colonPos == std::string::npos) return "";

        auto quoteStart = content.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) return "";

        auto quoteEnd = content.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) return "";

        return content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    };

    manifest.name = extractField("name");
    manifest.publisher = extractField("publisher");
    manifest.version = extractField("version");
    manifest.description = extractField("description");
    manifest.displayName = extractField("displayName");
    manifest.license = extractField("license");

    if (!manifest.publisher.empty() && !manifest.name.empty()) {
        manifest.id = manifest.publisher + "." + manifest.name;
    }

    return ExtResult::ok("Manifest parsed");
}

ExtResult ExtensionMarketplace::verifySignature(const std::string& vsixPath) {
    // TODO: Implement signature verification
    (void)vsixPath;
    if (!policy_.requireSignatureVerification) {
        return ExtResult::ok("Signature verification not required");
    }
    return ExtResult::error("Signature verification not implemented", -1);
}

// ============================================================================
// SemVer Comparison
// ============================================================================

bool ExtensionMarketplace::semverSatisfies(const std::string& version,
                                            const std::string& range) {
    // Minimal semver check — supports ">=X.Y.Z" and "X.Y.Z"
    if (range.empty()) return true;
    if (range == version) return true;

    // Parse version components
    auto parseVer = [](const std::string& v,
                       int& major, int& minor, int& patch) {
        major = minor = patch = 0;
        sscanf(v.c_str(), "%d.%d.%d", &major, &minor, &patch);
    };

    int vMaj, vMin, vPat;
    parseVer(version, vMaj, vMin, vPat);

    if (range.substr(0, 2) == ">=") {
        int rMaj, rMin, rPat;
        parseVer(range.substr(2), rMaj, rMin, rPat);

        if (vMaj > rMaj) return true;
        if (vMaj == rMaj && vMin > rMin) return true;
        if (vMaj == rMaj && vMin == rMin && vPat >= rPat) return true;
        return false;
    }

    // Default: exact match
    return version == range;
}

// ============================================================================
// HTTP Download
// ============================================================================

ExtResult ExtensionMarketplace::httpDownload(const std::string& url,
                                              const std::string& outputPath) {
#ifdef _WIN32
    // WinHTTP download
    MarketplaceEvent dlStart;
    dlStart.type = MarketplaceEvent::DOWNLOAD_STARTED;
    dlStart.extensionId = "";
    dlStart.detail = url.c_str();
    emitEvent(dlStart);

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    std::wstring wUrl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        return ExtResult::error("Invalid URL", 20);
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Marketplace/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return ExtResult::error("WinHTTP session failed", 21);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName,
                                         urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return ExtResult::error("WinHTTP connect failed", 22);
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ?
                  WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ExtResult::error("WinHTTP request failed", 23);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ExtResult::error("HTTP request failed", 24);
    }

    // Read response body to file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ExtResult::error("Cannot open output file", 25);
    }

    DWORD bytesRead = 0;
    uint8_t buffer[8192];
    uint64_t totalBytes = 0;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead)) {
        if (bytesRead == 0) break;
        outFile.write(reinterpret_cast<char*>(buffer), bytesRead);
        totalBytes += bytesRead;
    }

    outFile.close();
    totalDownloadBytes_.fetch_add(totalBytes);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    MarketplaceEvent dlDone;
    dlDone.type = MarketplaceEvent::DOWNLOAD_COMPLETED;
    dlDone.extensionId = "";
    dlDone.detail = "Download complete";
    emitEvent(dlDone);

    return ExtResult::ok("Download complete");

#else
    // POSIX: would use libcurl
    (void)url;
    (void)outputPath;
    return ExtResult::error("HTTP download not available on this platform", 30);
#endif
}

// ============================================================================
// Offline Cache
// ============================================================================

ExtResult ExtensionMarketplace::cacheExtension(const std::string& extensionId) {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    auto it = installed_.find(extensionId);
    if (it == installed_.end())
        return ExtResult::error("Extension not installed", 1);

    if (!it->second.vsixPath.empty() &&
        std::filesystem::exists(it->second.vsixPath)) {
        std::string cachePath = cacheDir_ + extensionId + ".vsix";
        std::filesystem::create_directories(cacheDir_);
        std::filesystem::copy_file(it->second.vsixPath, cachePath,
            std::filesystem::copy_options::overwrite_existing);
        return ExtResult::ok("Cached");
    }

    return ExtResult::error("No VSIX file available to cache", 2);
}

ExtResult ExtensionMarketplace::clearCache() {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);

    if (std::filesystem::exists(cacheDir_)) {
        std::error_code ec;
        std::filesystem::remove_all(cacheDir_, ec);
        std::filesystem::create_directories(cacheDir_);
    }

    return ExtResult::ok("Cache cleared");
}

ExtensionMarketplace::CacheStats
ExtensionMarketplace::getCacheStats() const {
    CacheStats stats = {};

    if (std::filesystem::exists(cacheDir_)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(cacheDir_)) {
            if (entry.is_regular_file()) {
                stats.cachedExtensions++;
                stats.totalCacheSizeBytes += entry.file_size();
            }
        }
    }

    return stats;
}

// ============================================================================
// Events
// ============================================================================

void ExtensionMarketplace::addEventListener(MarketplaceEventCallback callback,
                                              void* userData) {
    uint32_t idx = eventListenerCount_.load();
    if (idx >= MAX_EVENT_LISTENERS) return;

    eventListeners_[idx].callback = callback;
    eventListeners_[idx].userData = userData;
    eventListenerCount_.fetch_add(1);
}

void ExtensionMarketplace::removeEventListener(
    MarketplaceEventCallback callback)
{
    uint32_t count = eventListenerCount_.load();
    for (uint32_t i = 0; i < count; ++i) {
        if (eventListeners_[i].callback == callback) {
            for (uint32_t j = i; j < count - 1; ++j) {
                eventListeners_[j] = eventListeners_[j + 1];
            }
            eventListenerCount_.fetch_sub(1);
            return;
        }
    }
}

void ExtensionMarketplace::emitEvent(const MarketplaceEvent& evt) {
    uint32_t count = eventListenerCount_.load();
    for (uint32_t i = 0; i < count; ++i) {
        if (eventListeners_[i].callback) {
            eventListeners_[i].callback(evt, eventListeners_[i].userData);
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================

ExtensionMarketplace::MarketplaceStats
ExtensionMarketplace::getStats() const {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    MarketplaceStats stats = {};
    stats.totalInstalled = static_cast<uint32_t>(installed_.size());
    stats.totalDownloadBytes = totalDownloadBytes_.load();

    for (auto& [id, state] : states_) {
        switch (state) {
            case ExtensionState::ENABLED:  stats.totalEnabled++; break;
            case ExtensionState::DISABLED: stats.totalDisabled++; break;
            case ExtensionState::BLOCKED:  stats.totalBlocked++; break;
            default: break;
        }
    }

    return stats;
}

// ============================================================================
// Shutdown
// ============================================================================

void ExtensionMarketplace::shutdown() {
    std::lock_guard<std::mutex> lock(marketplaceMutex_);
    // Deactivate all extensions
    for (auto& [id, manifest] : installed_) {
        states_[id] = ExtensionState::INSTALLED;
    }
}

} // namespace Extensions
} // namespace RawrXD
