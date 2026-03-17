#include "github_model_integration.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace GitHubModelIntegration {

// ==================== HTTP Utilities ====================

static std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

static std::wstring stringToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

static std::string executeCommand(const std::string& command) {
    std::string result;
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    _pclose(pipe);
    return result;
}

static std::string jsonStringValue(const json& obj, const std::string& key, const std::string& fallback) {
    if (!obj.contains(key)) return fallback;
    const json& value = obj[key];
    if (value.is_string() || value.is_number() || value.is_boolean() || value.is_null()) {
        return value.get<std::string>();
    }
    return fallback;
}

static int jsonIntValue(const json& obj, const std::string& key, int fallback) {
    if (!obj.contains(key)) return fallback;
    const json& value = obj[key];
    if (value.is_number()) return value.get<int>();
    return fallback;
}

static bool jsonBoolValue(const json& obj, const std::string& key, bool fallback) {
    if (!obj.contains(key)) return fallback;
    const json& value = obj[key];
    if (value.is_boolean()) return value.get<bool>();
    return fallback;
}

// ==================== GitHubAPIClient Implementation ====================

GitHubAPIClient::GitHubAPIClient() {}

GitHubAPIClient::~GitHubAPIClient() {}

bool GitHubAPIClient::authenticate(const std::string& token) {
    if (token.empty()) return false;
    
    token_ = token;
    
    // Verify token by fetching user info
    try {
        std::string response = makeAuthRequest("GET", "/user");
        if (!response.empty()) {
            json userJson = json::parse(response);
            std::string login = jsonStringValue(userJson, "login", "");
            if (!login.empty()) {
                authenticated_ = true;
                std::cout << "[GitHub] Authenticated as: " << login << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Authentication failed: " << e.what() << std::endl;
    }
    
    authenticated_ = false;
    return false;
}

std::string GitHubAPIClient::getAuthenticatedUser() {
    if (!authenticated_) return "";
    
    try {
        std::string response = makeAuthRequest("GET", "/user");
        json userJson = json::parse(response);
        return jsonStringValue(userJson, "login", "");
    } catch (...) {
        return "";
    }
}

std::vector<GitHubRepo> GitHubAPIClient::listUserRepos(const std::string& username) {
    std::vector<GitHubRepo> repos;
    try {
        std::string endpoint = "/users/" + username + "/repos?per_page=100";
        std::string response = makeAuthRequest("GET", endpoint);
        json reposJson = json::parse(response);
        
        if (reposJson.is_array()) {
            for (auto it = reposJson.array_begin(); it != reposJson.array_end(); ++it) {
                const json& repoJson = *it;
                GitHubRepo repo;
                if (repoJson.contains("owner") && repoJson["owner"].is_object()) {
                    repo.owner = jsonStringValue(repoJson["owner"], "login", "");
                }
                repo.name = jsonStringValue(repoJson, "name", "");
                repo.fullName = jsonStringValue(repoJson, "full_name", "");
                repo.cloneUrl = jsonStringValue(repoJson, "clone_url", "");
                repo.description = jsonStringValue(repoJson, "description", "");
                repo.isPrivate = jsonBoolValue(repoJson, "private", false);
                repo.stars = jsonIntValue(repoJson, "stargazers_count", 0);
                repo.forks = jsonIntValue(repoJson, "forks_count", 0);
                repo.defaultBranch = jsonStringValue(repoJson, "default_branch", "main");
                repo.createdAt = jsonStringValue(repoJson, "created_at", "");
                repo.updatedAt = jsonStringValue(repoJson, "updated_at", "");
                repos.push_back(repo);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to list repos: " << e.what() << std::endl;
    }
    return repos;
}

std::vector<GitHubRepo> GitHubAPIClient::listOrgRepos(const std::string& orgName) {
    std::vector<GitHubRepo> repos;
    try {
        std::string endpoint = "/orgs/" + orgName + "/repos?per_page=100";
        std::string response = makeAuthRequest("GET", endpoint);
        json reposJson = json::parse(response);
        
        if (reposJson.is_array()) {
            for (auto it = reposJson.array_begin(); it != reposJson.array_end(); ++it) {
                const json& repoJson = *it;
                GitHubRepo repo;
                if (repoJson.contains("owner") && repoJson["owner"].is_object()) {
                    repo.owner = jsonStringValue(repoJson["owner"], "login", "");
                }
                repo.name = jsonStringValue(repoJson, "name", "");
                repo.fullName = jsonStringValue(repoJson, "full_name", "");
                repo.cloneUrl = jsonStringValue(repoJson, "clone_url", "");
                repo.description = jsonStringValue(repoJson, "description", "");
                repo.isPrivate = jsonBoolValue(repoJson, "private", false);
                repo.stars = jsonIntValue(repoJson, "stargazers_count", 0);
                repo.forks = jsonIntValue(repoJson, "forks_count", 0);
                repo.defaultBranch = jsonStringValue(repoJson, "default_branch", "main");
                repo.createdAt = jsonStringValue(repoJson, "created_at", "");
                repo.updatedAt = jsonStringValue(repoJson, "updated_at", "");
                repos.push_back(repo);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to list org repos: " << e.what() << std::endl;
    }
    return repos;
}

std::vector<GitHubRepo> GitHubAPIClient::searchRepos(const std::string& query, const std::string& topic) {
    std::vector<GitHubRepo> repos;
    try {
        std::string searchQuery = query;
        if (!topic.empty()) {
            searchQuery += "+topic:" + topic;
        }
        
        std::string endpoint = "/search/repositories?q=" + searchQuery + "&per_page=100";
        std::string response = makeAuthRequest("GET", endpoint);
        json searchJson = json::parse(response);
        
        if (searchJson.contains("items") && searchJson["items"].is_array()) {
            const json& items = searchJson["items"];
            for (auto it = items.array_begin(); it != items.array_end(); ++it) {
                const json& repoJson = *it;
                GitHubRepo repo;
                if (repoJson.contains("owner") && repoJson["owner"].is_object()) {
                    repo.owner = jsonStringValue(repoJson["owner"], "login", "");
                }
                repo.name = jsonStringValue(repoJson, "name", "");
                repo.fullName = jsonStringValue(repoJson, "full_name", "");
                repo.cloneUrl = jsonStringValue(repoJson, "clone_url", "");
                repo.description = jsonStringValue(repoJson, "description", "");
                repo.isPrivate = jsonBoolValue(repoJson, "private", false);
                repo.stars = jsonIntValue(repoJson, "stargazers_count", 0);
                repo.forks = jsonIntValue(repoJson, "forks_count", 0);
                repo.defaultBranch = jsonStringValue(repoJson, "default_branch", "main");
                repo.createdAt = jsonStringValue(repoJson, "created_at", "");
                repo.updatedAt = jsonStringValue(repoJson, "updated_at", "");
                repos.push_back(repo);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to search repos: " << e.what() << std::endl;
    }
    return repos;
}

GitHubRepo GitHubAPIClient::getRepo(const std::string& owner, const std::string& repo) {
    GitHubRepo result;
    try {
        std::string endpoint = "/repos/" + owner + "/" + repo;
        std::string response = makeAuthRequest("GET", endpoint);
        json repoJson = json::parse(response);
        
        if (repoJson.contains("owner") && repoJson["owner"].is_object()) {
            result.owner = jsonStringValue(repoJson["owner"], "login", "");
        }
        result.name = jsonStringValue(repoJson, "name", "");
        result.fullName = jsonStringValue(repoJson, "full_name", "");
        result.cloneUrl = jsonStringValue(repoJson, "clone_url", "");
        result.description = jsonStringValue(repoJson, "description", "");
        result.isPrivate = jsonBoolValue(repoJson, "private", false);
        result.stars = jsonIntValue(repoJson, "stargazers_count", 0);
        result.forks = jsonIntValue(repoJson, "forks_count", 0);
        result.defaultBranch = jsonStringValue(repoJson, "default_branch", "main");
        result.createdAt = jsonStringValue(repoJson, "created_at", "");
        result.updatedAt = jsonStringValue(repoJson, "updated_at", "");
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to get repo: " << e.what() << std::endl;
    }
    return result;
}

bool GitHubAPIClient::createRepo(const std::string& name, const std::string& description, bool isPrivate) {
    if (!authenticated_) return false;
    
    try {
        json body;
        body["name"] = name;
        body["description"] = description;
        body["private"] = isPrivate;
        body["auto_init"] = true;
        
        std::string response = makeAuthRequest("POST", "/user/repos", body.dump());
        json repoJson = json::parse(response);
        
        if (repoJson.contains("id")) {
            std::string fullName;
            if (repoJson.contains("full_name") && repoJson["full_name"].is_string()) {
                fullName = repoJson["full_name"].get<std::string>();
            }
            std::cout << "[GitHub] Created repository: " << fullName << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to create repo: " << e.what() << std::endl;
    }
    return false;
}

std::string GitHubAPIClient::getFileContent(const std::string& owner, const std::string& repo,
                                           const std::string& path, const std::string& branch) {
    try {
        std::string endpoint = "/repos/" + owner + "/" + repo + "/contents/" + path + "?ref=" + branch;
        std::string response = makeAuthRequest("GET", endpoint);
        json contentJson = json::parse(response);
        
        if (contentJson.contains("content")) {
            // Content is base64 encoded
            std::string content = contentJson["content"].get<std::string>();
            // TODO: Decode base64
            return content;
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub] Failed to get file content: " << e.what() << std::endl;
    }
    return "";
}

GitHubModelMetadata GitHubAPIClient::fetchModelMetadata(const std::string& repoUrl) {
    GitHubModelMetadata metadata;
    
    // Parse repo URL to extract owner/repo
    size_t lastSlash = repoUrl.find_last_of('/');
    size_t secondLastSlash = repoUrl.find_last_of('/', lastSlash - 1);
    
    if (lastSlash != std::string::npos && secondLastSlash != std::string::npos) {
        std::string owner = repoUrl.substr(secondLastSlash + 1, lastSlash - secondLastSlash - 1);
        std::string repo = repoUrl.substr(lastSlash + 1);
        
        // Remove .git suffix if present
        if (repo.ends_with(".git")) {
            repo = repo.substr(0, repo.length() - 4);
        }
        
        try {
            // Fetch model.json if exists
            std::string modelJson = getFileContent(owner, repo, "model.json");
            if (!modelJson.empty()) {
                json meta = json::parse(modelJson);
                if (meta.contains("name") && meta["name"].is_string()) {
                    metadata.modelName = meta["name"].get<std::string>();
                } else {
                    metadata.modelName = repo;
                }
                if (meta.contains("version") && meta["version"].is_string()) {
                    metadata.version = meta["version"].get<std::string>();
                } else {
                    metadata.version = "1.0.0";
                }
                if (meta.contains("description") && meta["description"].is_string()) {
                    metadata.description = meta["description"].get<std::string>();
                }
                if (meta.contains("format") && meta["format"].is_string()) {
                    metadata.format = meta["format"].get<std::string>();
                } else {
                    metadata.format = "gguf";
                }
                if (meta.contains("architecture") && meta["architecture"].is_string()) {
                    metadata.architecture = meta["architecture"].get<std::string>();
                } else {
                    metadata.architecture = "unknown";
                }
                
                if (meta.contains("tags")) {
                    const auto& tags = meta["tags"];
                    if (tags.is_array()) {
                        for (size_t i = 0; i < tags.size(); ++i) {
                            const auto& tag = tags[i];
                            if (tag.is_string()) {
                                metadata.tags.push_back(tag.get<std::string>());
                            }
                        }
                    } else if (tags.is_object()) {
                        for (auto it = tags.begin(); it != tags.end(); ++it) {
                            metadata.tags.push_back(it->first);
                        }
                    }
                }
            }
            
            // Fetch README.md
            std::string readme = getFileContent(owner, repo, "README.md");
            if (metadata.description.empty() && !readme.empty()) {
                // Extract first paragraph as description
                size_t firstNewline = readme.find('\n');
                if (firstNewline != std::string::npos) {
                    metadata.description = readme.substr(0, firstNewline);
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[GitHub] Failed to fetch model metadata: " << e.what() << std::endl;
        }
    }
    
    metadata.repoUrl = repoUrl;
    return metadata;
}

std::string GitHubAPIClient::makeRequest(const std::string& method, const std::string& endpoint,
                                        const std::string& body, const std::map<std::string, std::string>& headers) {
    std::wstring host = L"api.github.com";
    std::wstring wEndpoint = stringToWstring(endpoint);
    std::wstring wMethod = stringToWstring(method);
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Add headers
    std::wstring allHeaders = L"Accept: application/vnd.github.v3+json\r\n";
    allHeaders += L"User-Agent: RawrXD/1.0\r\n";
    
    for (const auto& [key, value] : headers) {
        allHeaders += stringToWstring(key) + L": " + stringToWstring(value) + L"\r\n";
    }
    
    if (!token_.empty()) {
        allHeaders += L"Authorization: token " + stringToWstring(token_) + L"\r\n";
    }
    
    WinHttpAddRequestHeaders(hRequest, allHeaders.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    // Send request
    BOOL result = WinHttpSendRequest(hRequest,
                                     WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
                                     body.length(), body.length(), 0);
    
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Receive response
    result = WinHttpReceiveResponse(hRequest, nullptr);
    if (!result) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Read data
    std::string response;
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;
        
        std::vector<char> buffer(bytesAvailable + 1);
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            buffer[bytesRead] = '\0';
            response.append(buffer.data(), bytesRead);
        }
    } while (bytesAvailable > 0);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return response;
}

std::string GitHubAPIClient::makeAuthRequest(const std::string& method, const std::string& endpoint,
                                            const std::string& body) {
    return makeRequest(method, endpoint, body, {});
}

bool GitHubAPIClient::downloadFile(const std::string& url, const std::string& outputPath) {
    // Parse URL
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;
    
    size_t hostStart = schemeEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos) return false;
    
    std::string host = url.substr(hostStart, pathStart - hostStart);
    std::string path = url.substr(pathStart);
    
    std::wstring wHost = stringToWstring(host);
    std::wstring wPath = stringToWstring(path);
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Open output file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Download data
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;
        
        std::vector<char> buffer(bytesAvailable);
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            outFile.write(buffer.data(), bytesRead);
        }
    } while (bytesAvailable > 0);
    
    outFile.close();
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return true;
}

// ==================== GitHubModelManager Implementation ====================

GitHubModelManager::GitHubModelManager()
    : apiClient_(std::make_unique<GitHubAPIClient>()),
      cacheDirectory_("./github_models_cache") {
    
    fs::create_directories(cacheDirectory_);
}

GitHubModelManager::~GitHubModelManager() {}

GitHubModelManager& GitHubModelManager::getInstance() {
    static GitHubModelManager instance;
    return instance;
}

void GitHubModelManager::setToken(const std::string& token) {
    apiClient_->authenticate(token);
}

void GitHubModelManager::setOrganization(const std::string& org) {
    organization_ = org;
}

void GitHubModelManager::setCacheDirectory(const std::string& dir) {
    cacheDirectory_ = dir;
    fs::create_directories(cacheDirectory_);
}

std::vector<GitHubModelMetadata> GitHubModelManager::discoverModels(const std::string& topic) {
    std::vector<GitHubModelMetadata> models;
    
    // Search for model repositories
    auto repos = apiClient_->searchRepos("gguf", topic.empty() ? "gguf-model" : topic);
    
    for (const auto& repo : repos) {
        GitHubModelMetadata metadata = apiClient_->fetchModelMetadata(repo.cloneUrl);
        if (!metadata.modelName.empty()) {
            models.push_back(metadata);
            modelCache_[metadata.modelName] = metadata;
        }
    }
    
    return models;
}

std::vector<GitHubModelMetadata> GitHubModelManager::listOrganizationModels() {
    std::vector<GitHubModelMetadata> models;
    
    if (organization_.empty()) return models;
    
    auto repos = apiClient_->listOrgRepos(organization_);
    
    for (const auto& repo : repos) {
        GitHubModelMetadata metadata = apiClient_->fetchModelMetadata(repo.cloneUrl);
        if (!metadata.modelName.empty()) {
            models.push_back(metadata);
            modelCache_[metadata.modelName] = metadata;
        }
    }
    
    return models;
}

bool GitHubModelManager::cloneRepo(const std::string& repoUrl, const std::string& localPath) {
    std::string command = "git clone " + repoUrl + " \"" + localPath + "\"";
    std::string output = executeCommand(command);
    
    return fs::exists(localPath) && fs::is_directory(localPath);
}

bool GitHubModelManager::downloadModel(const std::string& repoUrl, const std::string& localPath) {
    // Clone repository
    std::string tempDir = cacheDirectory_ + "/temp_" + std::to_string(std::time(nullptr));
    
    if (!cloneRepo(repoUrl, tempDir)) {
        std::cerr << "[GitHub] Failed to clone repository: " << repoUrl << std::endl;
        return false;
    }
    
    // Find GGUF files
    bool foundModel = false;
    for (const auto& entry : fs::recursive_directory_iterator(tempDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
            fs::copy_file(entry.path(), localPath, fs::copy_options::overwrite_existing);
            foundModel = true;
            std::cout << "[GitHub] Downloaded model: " << entry.path().filename() << std::endl;
            break;
        }
    }
    
    // Cleanup
    fs::remove_all(tempDir);
    
    return foundModel;
}

bool GitHubModelManager::publishModel(const std::string& modelPath, const std::string& repoName,
                                     const GitHubModelMetadata& metadata) {
    if (!apiClient_->isAuthenticated()) {
        std::cerr << "[GitHub] Not authenticated. Cannot publish model." << std::endl;
        return false;
    }
    
    // Create repository
    if (!apiClient_->createRepo(repoName, metadata.description, false)) {
        std::cerr << "[GitHub] Failed to create repository." << std::endl;
        return false;
    }
    
    // Clone the new repository
    std::string user = apiClient_->getAuthenticatedUser();
    std::string repoUrl = "https://github.com/" + user + "/" + repoName + ".git";
    std::string tempDir = cacheDirectory_ + "/publish_" + repoName;
    
    if (!cloneRepo(repoUrl, tempDir)) {
        std::cerr << "[GitHub] Failed to clone new repository." << std::endl;
        return false;
    }
    
    // Copy model file
    fs::path modelFile = fs::path(modelPath);
    fs::copy_file(modelPath, tempDir + "/" + modelFile.filename().string(),
                 fs::copy_options::overwrite_existing);
    
    // Create model.json
    json metaJson;
    metaJson["name"] = metadata.modelName;
    metaJson["version"] = metadata.version;
    metaJson["description"] = metadata.description;
    metaJson["format"] = metadata.format;
    metaJson["architecture"] = metadata.architecture;
    metaJson["tags"] = metadata.tags;
    
    std::ofstream metaFile(tempDir + "/model.json");
    metaFile << metaJson.dump(2);
    metaFile.close();
    
    // Create README.md
    std::ofstream readme(tempDir + "/README.md");
    readme << "# " << metadata.modelName << "\n\n";
    readme << metadata.description << "\n\n";
    readme << "## Model Information\n\n";
    readme << "- **Format**: " << metadata.format << "\n";
    readme << "- **Architecture**: " << metadata.architecture << "\n";
    readme << "- **Version**: " << metadata.version << "\n";
    readme.close();
    
    // Commit and push
    executeCommand("cd \"" + tempDir + "\" && git add .");
    executeCommand("cd \"" + tempDir + "\" && git commit -m \"Add model files\"");
    executeCommand("cd \"" + tempDir + "\" && git push origin main");
    
    // Cleanup
    fs::remove_all(tempDir);
    
    std::cout << "[GitHub] Successfully published model to: " << repoUrl << std::endl;
    return true;
}

std::vector<std::string> GitHubModelManager::listCachedModels() const {
    std::vector<std::string> models;
    
    for (const auto& [name, metadata] : modelCache_) {
        models.push_back(name);
    }
    
    return models;
}

void GitHubModelManager::clearCache() {
    modelCache_.clear();
}

// ==================== AutoLoaderGitHubBridge Implementation ====================

void AutoLoaderGitHubBridge::initialize(const std::string& token, const std::string& org) {
    auto& manager = GitHubModelManager::getInstance();
    manager.setToken(token);
    if (!org.empty()) {
        manager.setOrganization(org);
    }
}

bool AutoLoaderGitHubBridge::registerGitHubModel(const std::string& repoUrl) {
    auto& manager = GitHubModelManager::getInstance();
    
    // Extract model name from repo URL
    size_t lastSlash = repoUrl.find_last_of('/');
    std::string modelName = repoUrl.substr(lastSlash + 1);
    if (modelName.ends_with(".git")) {
        modelName = modelName.substr(0, modelName.length() - 4);
    }
    
    // Download model
    std::string localPath = "./github_models/" + modelName + ".gguf";
    fs::create_directories("./github_models");
    
    return manager.downloadModel(repoUrl, localPath);
}

std::vector<std::string> AutoLoaderGitHubBridge::listAvailableGitHubModels() {
    auto& manager = GitHubModelManager::getInstance();
    auto models = manager.listOrganizationModels();
    
    std::vector<std::string> modelNames;
    for (const auto& model : models) {
        modelNames.push_back(model.modelName);
    }
    
    return modelNames;
}

bool AutoLoaderGitHubBridge::syncWithGitHub() {
    auto& manager = GitHubModelManager::getInstance();
    auto models = manager.listOrganizationModels();
    
    std::cout << "[GitHub] Found " << models.size() << " models in organization." << std::endl;
    
    // Download any new models
    for (const auto& model : models) {
        std::string localPath = "./github_models/" + model.modelName + ".gguf";
        if (!fs::exists(localPath)) {
            std::cout << "[GitHub] Downloading new model: " << model.modelName << std::endl;
            manager.downloadModel(model.repoUrl, localPath);
        }
    }
    
    return true;
}

bool AutoLoaderGitHubBridge::publishCustomModel(const std::string& modelName, const std::string& repoName) {
    auto& manager = GitHubModelManager::getInstance();
    
    std::string modelPath = "./custom_models/" + modelName + ".gguf";
    if (!fs::exists(modelPath)) {
        std::cerr << "[GitHub] Model not found: " << modelPath << std::endl;
        return false;
    }
    
    GitHubModelMetadata metadata;
    metadata.modelName = modelName;
    metadata.version = "1.0.0";
    metadata.description = "Custom model built with RawrXD";
    metadata.format = "gguf";
    metadata.architecture = "transformer";
    metadata.tags = {"custom", "gguf", "rawrxd"};
    
    return manager.publishModel(modelPath, repoName, metadata);
}

GitHubModelMetadata AutoLoaderGitHubBridge::getModelInfo(const std::string& modelName) {
    auto& manager = GitHubModelManager::getInstance();
    auto models = manager.listCachedModels();
    
    // Search in cache
    for (const auto& name : models) {
        if (name == modelName) {
            // Return cached metadata
            GitHubModelMetadata metadata;
            metadata.modelName = modelName;
            return metadata;
        }
    }
    
    return GitHubModelMetadata();
}

} // namespace GitHubModelIntegration
