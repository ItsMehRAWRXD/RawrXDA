// ExtensionHost_Discovery.cpp
// Phase 2 Day 7: Extension Discovery, Marketplace Client, and Manifest Validation

#include "ExtensionHost.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <urlmon.h>
#include <shlobj.h>
#include <wininet.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

namespace RawrXD::Extensions {

// ============================================================================
// Manifest Validation
// ============================================================================
struct ManifestValidationResult {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

static ManifestValidationResult ValidateManifest(const nlohmann::json& manifest) {
    ManifestValidationResult result;

    // Required fields
    if (!manifest.contains("name") || !manifest["name"].is_string()) {
        result.errors.push_back("Missing required field: name");
    }
    if (!manifest.contains("version") || !manifest["version"].is_string()) {
        result.errors.push_back("Missing required field: version");
    }

    // Version format check (semver-like)
    if (manifest.contains("version") && manifest["version"].is_string()) {
        std::string ver = manifest["version"].get<std::string>();
        int dots = 0;
        for (char c : ver) {
            if (c == '.') ++dots;
            else if (!std::isdigit(c)) {
                result.warnings.push_back("Version contains non-numeric characters: " + ver);
                break;
            }
        }
        if (dots < 1 || dots > 3) {
            result.warnings.push_back("Version should follow semver (x.y.z): " + ver);
        }
    }

    // Activation events validation
    if (manifest.contains("activationEvents") && manifest["activationEvents"].is_array()) {
        for (const auto& evt : manifest["activationEvents"]) {
            if (!evt.is_string()) {
                result.warnings.push_back("activationEvents contains non-string value");
                break;
            }
        }
    }

    // Engines field (compatibility)
    if (manifest.contains("engines") && manifest["engines"].is_object()) {
        if (manifest["engines"].contains("rawrxd")) {
            std::string reqVer = manifest["engines"]["rawrxd"].get<std::string>();
            // TODO: Compare against current RawrXD version
        }
    }

    result.valid = result.errors.empty();
    return result;
}

// ============================================================================
// Dependency Resolution
// ============================================================================
struct ExtensionDependency {
    std::string id;
    std::string versionRange;
    bool optional = false;
};

static std::vector<ExtensionDependency> ParseDependencies(const nlohmann::json& manifest) {
    std::vector<ExtensionDependency> deps;
    if (!manifest.contains("dependencies") || !manifest["dependencies"].is_object()) {
        return deps;
    }

    for (auto it = manifest["dependencies"].begin(); it != manifest["dependencies"].end(); ++it) {
        ExtensionDependency dep;
        dep.id = it.key();
        if (it.value().is_string()) {
            dep.versionRange = it.value().get<std::string>();
        } else {
            dep.versionRange = "*";
        }
        deps.push_back(dep);
    }
    return deps;
}

// ============================================================================
// Extension Marketplace Client
// ============================================================================
class MarketplaceClient {
public:
    static constexpr const char* DEFAULT_ENDPOINT = "https://marketplace.visualstudio.com/_apis/public/gallery";

    struct SearchQuery {
        std::string text;
        std::string category;
        int pageSize = 50;
        int pageNumber = 1;
    };

    struct ExtensionEntry {
        std::string id;
        std::string name;
        std::string publisher;
        std::string version;
        std::string description;
        std::string iconUrl;
        int installCount = 0;
        double rating = 0.0;
    };

    bool DownloadExtension(const std::string& publisher, const std::string& name,
                           const std::string& version, const std::string& destPath);
    std::vector<ExtensionEntry> Search(const SearchQuery& query);
    bool CheckForUpdate(const std::string& extensionId, const std::string& currentVersion,
                        std::string& outLatestVersion);

private:
    std::string m_endpoint = DEFAULT_ENDPOINT;

    bool HttpGet(const std::string& url, std::string& outResponse);
    bool DownloadFile(const std::string& url, const std::string& destPath);
};

bool MarketplaceClient::HttpGet(const std::string& url, std::string& outResponse) {
    HINTERNET hInternet = InternetOpenA("RawrXD-ExtensionHost/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), nullptr, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    outResponse.clear();
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outResponse.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return !outResponse.empty();
}

bool MarketplaceClient::DownloadFile(const std::string& url, const std::string& destPath) {
    std::wstring wUrl(url.begin(), url.end());
    std::wstring wDest(destPath.begin(), destPath.end());
    HRESULT hr = URLDownloadToFileW(nullptr, wUrl.c_str(), wDest.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
}

bool MarketplaceClient::DownloadExtension(const std::string& publisher, const std::string& name,
                                          const std::string& version, const std::string& destPath) {
    std::string url = m_endpoint + "/publishers/" + publisher +
                     "/extensions/" + name + "/" + version + "/assetbyname/Microsoft.VisualStudio.Services.VSIXPackage";
    return DownloadFile(url, destPath);
}

std::vector<MarketplaceClient::ExtensionEntry> MarketplaceClient::Search(const SearchQuery& query) {
    std::vector<ExtensionEntry> results;

    // Build a VS Marketplace query payload.
    // Avoid json operator[] ambiguity on MSVC by using explicit array construction.
    nlohmann::json payload = nlohmann::json::object();
    payload["filters"] = nlohmann::json::array();
    nlohmann::json filter = nlohmann::json::object();
    filter["criteria"] = nlohmann::json::array();
    filter["criteria"].push_back(nlohmann::json::object({
        {"filterType", 8}, // SearchText
        {"value", query.text}
    }));
    filter["pageSize"] = query.pageSize;
    filter["pageNumber"] = query.pageNumber;
    payload["filters"].push_back(filter);
    payload["flags"] = (0x2 | 0x200); // IncludeVersions | IncludeFiles

    std::string response;
    std::string url = m_endpoint + "/extensionquery";
    // For now, return empty — full HTTP POST implementation would require WinHTTP
    (void)url;
    (void)payload;
    return results;
}

bool MarketplaceClient::CheckForUpdate(const std::string& extensionId,
                                        const std::string& currentVersion,
                                        std::string& outLatestVersion) {
    size_t dotPos = extensionId.find('.');
    if (dotPos == std::string::npos) return false;

    std::string publisher = extensionId.substr(0, dotPos);
    std::string name = extensionId.substr(dotPos + 1);

    std::string url = m_endpoint + "/publishers/" + publisher +
                     "/extensions/" + name;
    std::string response;
    if (!HttpGet(url, response)) return false;

    try {
        nlohmann::json j = nlohmann::json::parse(response);
        if (j.contains("versions") && j["versions"].is_array() && !j["versions"].empty()) {
            outLatestVersion = j["versions"][static_cast<size_t>(0)]["version"].get<std::string>();
            return outLatestVersion != currentVersion;
        }
    } catch (...) {
        return false;
    }
    return false;
}

// ============================================================================
// ExtensionHost Discovery Methods
// ============================================================================

bool ExtensionHost::InstallExtensionFromMarketplace(const std::string& publisher,
                                                      const std::string& name,
                                                      const std::string& version) {
    if (!m_initialized.load()) return false;

    // Determine extensions directory
    char pathBuffer[MAX_PATH];
    if (!SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, pathBuffer)) {
        return false;
    }
    std::string extDir = std::string(pathBuffer) + "\\RawrXD\\extensions";
    std::filesystem::create_directories(extDir);

    std::string destPath = extDir + "\\" + publisher + "." + name + "-" + version + ".vsix";

    MarketplaceClient client;
    if (!client.DownloadExtension(publisher, name, version, destPath)) {
        return false;
    }

    // Extract VSIX (ZIP format)
    std::string extractDir = extDir + "\\" + publisher + "." + name + "-" + version;
    std::filesystem::create_directories(extractDir);

    // In production, would use a ZIP extraction library
    // For now, mark as needing manual extraction
    return true;
}

bool ExtensionHost::ValidateExtensionManifest(const std::string& manifestPath,
                                               std::vector<std::string>& outErrors,
                                               std::vector<std::string>& outWarnings) {
    std::ifstream f(manifestPath);
    if (!f) {
        outErrors.push_back("Cannot open manifest: " + manifestPath);
        return false;
    }

    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    nlohmann::json manifest = nlohmann::json::parse(text, nullptr, false);
    if (manifest.is_discarded()) {
        outErrors.push_back("JSON parse error: invalid JSON");
        return false;
    }

    auto result = ValidateManifest(manifest);
    outErrors = result.errors;
    outWarnings = result.warnings;
    return result.valid;
}

std::vector<std::string> ExtensionHost::ResolveDependencies(const std::string& extensionId) {
    std::vector<std::string> missing;
    if (!m_initialized.load()) return missing;

    std::shared_ptr<ExtensionInfo> info;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        auto it = m_extensions.find(extensionId);
        if (it == m_extensions.end()) return missing;
        info = it->second;
    }

    std::filesystem::path manifestPath = std::filesystem::path(info->path) / "package.json";
    if (!std::filesystem::exists(manifestPath)) {
        manifestPath = std::filesystem::path(info->path) / "extension.json";
    }

    std::ifstream f(manifestPath);
    if (!f) return missing;

    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    nlohmann::json manifest = nlohmann::json::parse(text, nullptr, false);
    if (manifest.is_discarded()) return missing;

    auto deps = ParseDependencies(manifest);
    for (const auto& dep : deps) {
        if (dep.optional) continue;

        bool found = false;
        {
            std::lock_guard<std::mutex> lock(m_extensionsMutex);
            for (const auto& [id, _] : m_extensions) {
                if (id == dep.id || id.find(dep.id) != std::string::npos) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            missing.push_back(dep.id + "@" + dep.versionRange);
        }
    }

    return missing;
}

bool ExtensionHost::CheckForExtensionUpdates(std::vector<std::pair<std::string, std::string>>& outUpdates) {
    if (!m_initialized.load()) return false;

    MarketplaceClient client;
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(m_extensionsMutex);
        for (const auto& [id, info] : m_extensions) {
            if (info && !info->isBuiltin) {
                ids.push_back(id);
            }
        }
    }

    for (const auto& id : ids) {
        std::string currentVersion;
        {
            std::lock_guard<std::mutex> lock(m_extensionsMutex);
            auto it = m_extensions.find(id);
            if (it != m_extensions.end() && it->second) {
                currentVersion = it->second->version;
            }
        }

        std::string latestVersion;
        if (client.CheckForUpdate(id, currentVersion, latestVersion)) {
            outUpdates.emplace_back(id, latestVersion);
        }
    }

    return !outUpdates.empty();
}

} // namespace RawrXD::Extensions
