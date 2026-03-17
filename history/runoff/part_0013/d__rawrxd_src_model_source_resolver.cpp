/**
 * @file model_source_resolver.cpp
 * @brief Unified Model Source Resolution — HuggingFace, Ollama blobs, HTTP, local files
 *
 * Implements ModelSourceResolver using WinHTTP for network operations.
 * All downloads go to a local cache directory. Ollama blobs are searched in
 * standard Ollama directories and validated for GGUF magic bytes.
 *
 * This module does NOT modify any existing GGUF loading logic. It only resolves
 * model sources to local filesystem paths that StreamingGGUFLoader can open.
 */

#include "model_source_resolver.h"
#include "model_loader/GGUFConstants.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace fs = std::filesystem;

namespace RawrXD {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ModelSourceResolver::ModelSourceResolver()
    : m_cacheDir(GetDefaultCacheDir())
{
    // Ensure cache directory exists
    try {
        fs::create_directories(m_cacheDir);
    } catch (const std::exception& e) {
        std::cerr << "[ModelSourceResolver] Warning: Could not create cache dir: " 
                  << m_cacheDir << " (" << e.what() << ")" << std::endl;
    }
}

ModelSourceResolver::~ModelSourceResolver() {
    CancelDownload();
}

// ============================================================================
// Source Detection
// ============================================================================

GGUFConstants::ModelSourceType ModelSourceResolver::DetectSourceType(const std::string& input) const {
    if (input.empty()) {
        return GGUFConstants::ModelSourceType::UNKNOWN;
    }

    // Check for explicit protocol prefixes
    if (input.substr(0, 5) == "hf://" || input.substr(0, 14) == "huggingface://") {
        return GGUFConstants::ModelSourceType::HUGGINGFACE_REPO;
    }
    if (input.substr(0, 7) == "http://" || input.substr(0, 8) == "https://") {
        // Check if it's a HuggingFace URL
        if (input.find("huggingface.co") != std::string::npos) {
            return GGUFConstants::ModelSourceType::HUGGINGFACE_REPO;
        }
        return GGUFConstants::ModelSourceType::HTTP_URL;
    }

    // Check if it's a local file path
    // Windows paths: C:\..., D:\..., \\server\..., or relative paths ending in .gguf
    if ((input.length() > 2 && input[1] == ':') ||        // Drive letter path
        (input.substr(0, 2) == "\\\\") ||                   // UNC path
        (input[0] == '/' || input[0] == '\\')) {           // Absolute path
        return GGUFConstants::ModelSourceType::LOCAL_FILE;
    }

    // Check file extension — if it ends with .gguf, treat as local path
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.length() > 5 && lower.substr(lower.length() - 5) == ".gguf") {
        return GGUFConstants::ModelSourceType::LOCAL_FILE;
    }

    // Check for HuggingFace pattern: "owner/repo" (exactly one slash, no colons)
    size_t slashCount = std::count(input.begin(), input.end(), '/');
    size_t colonCount = std::count(input.begin(), input.end(), ':');
    
    if (slashCount == 1 && colonCount == 0 && input[0] != '/' && input[0] != '\\') {
        // Likely a HuggingFace repo ID like "TheBloke/Llama-2-7B-GGUF"
        return GGUFConstants::ModelSourceType::HUGGINGFACE_REPO;
    }

    // Check for Ollama pattern: "model:tag" or plain model name
    if (colonCount == 1 || (slashCount == 0 && colonCount == 0)) {
        // Could be "llama3.2:3b" or "llama3.2" (Ollama model names)
        return GGUFConstants::ModelSourceType::OLLAMA_BLOB;
    }

    // Last resort: if the file exists locally, it's local
    if (fs::exists(input)) {
        return GGUFConstants::ModelSourceType::LOCAL_FILE;
    }

    return GGUFConstants::ModelSourceType::UNKNOWN;
}

// ============================================================================
// Unified Resolution
// ============================================================================

ResolvedModelPath ModelSourceResolver::Resolve(const std::string& input,
                                                DownloadProgressCallback progress) {
    ResolvedModelPath result;
    result.original_input = input;
    result.source_type = DetectSourceType(input);

    std::cout << "[ModelSourceResolver] Resolving: " << input << std::endl;
    std::cout << "[ModelSourceResolver] Detected source: " << SourceTypeToString(result.source_type) << std::endl;

    switch (result.source_type) {
        case GGUFConstants::ModelSourceType::LOCAL_FILE: {
            // Direct local file — validate it exists and has GGUF magic
            std::string path = input;
            
            if (!fs::exists(path)) {
                result.error_message = "File not found: " + path;
                std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                return result;
            }

            if (!ValidateGGUFMagic(path)) {
                result.error_message = "File is not a valid GGUF file (bad magic bytes): " + path;
                std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                return result;
            }

            result.success = true;
            result.local_path = path;
            std::cout << "[ModelSourceResolver] ✅ Local file validated: " << path << std::endl;
            break;
        }

        case GGUFConstants::ModelSourceType::HUGGINGFACE_REPO: {
            // Parse repo ID from various formats
            std::string repo_id = input;
            std::string specific_file;
            
            // Strip protocol prefixes
            if (repo_id.substr(0, 5) == "hf://") {
                repo_id = repo_id.substr(5);
            } else if (repo_id.substr(0, 14) == "huggingface://") {
                repo_id = repo_id.substr(14);
            } else if (repo_id.find("huggingface.co/") != std::string::npos) {
                // Parse URL: https://huggingface.co/owner/repo/resolve/main/file.gguf
                size_t hfPos = repo_id.find("huggingface.co/");
                repo_id = repo_id.substr(hfPos + 15);
                // Check if URL includes a specific file path
                if (repo_id.find("/resolve/") != std::string::npos) {
                    size_t resolvePos = repo_id.find("/resolve/");
                    std::string afterResolve = repo_id.substr(resolvePos + 9);
                    repo_id = repo_id.substr(0, resolvePos);
                    // Skip "main/" or branch name
                    size_t branchSlash = afterResolve.find('/');
                    if (branchSlash != std::string::npos) {
                        specific_file = afterResolve.substr(branchSlash + 1);
                    }
                }
            }

            // Check if "repo_id/filename.gguf" was provided
            if (specific_file.empty()) {
                size_t slashCount = std::count(repo_id.begin(), repo_id.end(), '/');
                if (slashCount >= 2) {
                    // "owner/repo/file.gguf" format
                    size_t secondSlash = repo_id.find('/', repo_id.find('/') + 1);
                    specific_file = repo_id.substr(secondSlash + 1);
                    repo_id = repo_id.substr(0, secondSlash);
                }
            }
            
            result.hf_repo_id = repo_id;

            // Check local cache first
            std::string cache_subdir = m_cacheDir + "/" + SanitizeFilename(repo_id);
            if (!specific_file.empty()) {
                std::string cached_path = cache_subdir + "/" + specific_file;
                if (fs::exists(cached_path) && ValidateGGUFMagic(cached_path)) {
                    std::cout << "[ModelSourceResolver] ✅ Found in cache: " << cached_path << std::endl;
                    result.success = true;
                    result.local_path = cached_path;
                    result.hf_filename = specific_file;
                    return result;
                }
            }

            // If no specific file specified, get available GGUF files
            if (specific_file.empty()) {
                auto files = GetHuggingFaceGGUFFiles(repo_id);
                if (files.empty()) {
                    result.error_message = "No GGUF files found in HuggingFace repo: " + repo_id;
                    std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                    return result;
                }

                // Pick the best GGUF file: prefer Q4_K_M, then Q4_K_S, then Q5_K_M, then smallest
                specific_file = files[0].filename;  // default to first
                for (const auto& f : files) {
                    std::string q = f.quantization;
                    std::transform(q.begin(), q.end(), q.begin(), ::toupper);
                    if (q.find("Q4_K_M") != std::string::npos) {
                        specific_file = f.filename;
                        break;
                    }
                    if (q.find("Q4_K_S") != std::string::npos || 
                        q.find("Q5_K_M") != std::string::npos) {
                        specific_file = f.filename;
                    }
                }
                
                std::cout << "[ModelSourceResolver] Selected GGUF: " << specific_file << std::endl;
            }

            result.hf_filename = specific_file;

            // Download the file
            std::string local_path = DownloadFromHuggingFace(repo_id, specific_file, progress);
            if (local_path.empty()) {
                result.error_message = "Failed to download from HuggingFace: " + repo_id + "/" + specific_file;
                std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                return result;
            }

            result.success = true;
            result.local_path = local_path;
            std::cout << "[ModelSourceResolver] ✅ Downloaded from HF: " << local_path << std::endl;
            break;
        }

        case GGUFConstants::ModelSourceType::OLLAMA_BLOB: {
            std::string model_name = input;
            result.ollama_model_name = model_name;

            OllamaBlobInfo blob = ResolveOllamaBlob(model_name);
            if (!blob.is_valid_gguf) {
                result.error_message = "No valid GGUF blob found for Ollama model: " + model_name;
                std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                return result;
            }

            result.success = true;
            result.local_path = blob.blob_path;
            std::cout << "[ModelSourceResolver] ✅ Ollama blob resolved: " << blob.blob_path 
                      << " (" << (blob.size_bytes / (1024*1024)) << " MB)" << std::endl;
            break;
        }

        case GGUFConstants::ModelSourceType::HTTP_URL: {
            // Check cache first
            std::string filename = ExtractFilenameFromURL(input);
            std::string cached_path = m_cacheDir + "/" + filename;
            
            if (fs::exists(cached_path) && ValidateGGUFMagic(cached_path)) {
                std::cout << "[ModelSourceResolver] ✅ Found in cache: " << cached_path << std::endl;
                result.success = true;
                result.local_path = cached_path;
                return result;
            }

            // Download
            std::string local_path = DownloadFromURL(input, progress);
            if (local_path.empty()) {
                result.error_message = "Failed to download from URL: " + input;
                std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
                return result;
            }

            result.success = true;
            result.local_path = local_path;
            std::cout << "[ModelSourceResolver] ✅ Downloaded from URL: " << local_path << std::endl;
            break;
        }

        default: {
            // Try as local file first, then Ollama, then return error
            if (fs::exists(input) && ValidateGGUFMagic(input)) {
                result.success = true;
                result.local_path = input;
                result.source_type = GGUFConstants::ModelSourceType::LOCAL_FILE;
                break;
            }

            // Try Ollama
            OllamaBlobInfo blob = ResolveOllamaBlob(input);
            if (blob.is_valid_gguf) {
                result.success = true;
                result.local_path = blob.blob_path;
                result.source_type = GGUFConstants::ModelSourceType::OLLAMA_BLOB;
                result.ollama_model_name = input;
                break;
            }

            result.error_message = "Cannot determine model source type for: " + input;
            std::cerr << "[ModelSourceResolver] " << result.error_message << std::endl;
            break;
        }
    }

    return result;
}

// ============================================================================
// HuggingFace Operations
// ============================================================================

std::vector<HFModelInfo> ModelSourceResolver::SearchHuggingFace(const std::string& query, int limit) {
    std::vector<HFModelInfo> results;
    
    std::string url = "https://huggingface.co/api/models?search=" + query 
                    + "&filter=gguf&limit=" + std::to_string(limit) 
                    + "&sort=downloads&direction=-1";
    
    std::cout << "[ModelSourceResolver] Searching HF: " << query << std::endl;
    
    std::string response = WinHTTPGet(url, m_hfToken);
    if (response.empty()) {
        std::cerr << "[ModelSourceResolver] No response from HuggingFace API" << std::endl;
        return results;
    }

    // Parse JSON array of model objects
    // Expected: [{"id": "owner/model", "downloads": 12345}, ...]
    auto objects = SplitJSONObjects(response);
    for (const auto& obj : objects) {
        if (obj.empty()) continue;
        
        HFModelInfo info;
        info.repo_id = ExtractJSONString(obj, "id");
        info.model_name = ExtractJSONString(obj, "modelId");
        if (info.model_name.empty()) info.model_name = info.repo_id;
        info.downloads = ExtractJSONNumber(obj, "downloads");
        
        if (!info.repo_id.empty()) {
            results.push_back(info);
        }
    }
    
    std::cout << "[ModelSourceResolver] Found " << results.size() << " HF models" << std::endl;
    return results;
}

HFModelInfo ModelSourceResolver::GetHuggingFaceModelInfo(const std::string& repo_id) {
    HFModelInfo info;
    info.repo_id = repo_id;
    
    std::string url = "https://huggingface.co/api/models/" + repo_id;
    std::string response = WinHTTPGet(url, m_hfToken);
    
    if (response.empty()) {
        std::cerr << "[ModelSourceResolver] Failed to fetch HF model info: " << repo_id << std::endl;
        return info;
    }
    
    info.model_name = ExtractJSONString(response, "modelId");
    if (info.model_name.empty()) info.model_name = repo_id;
    info.downloads = ExtractJSONNumber(response, "downloads");
    
    // Parse siblings array for file list
    size_t siblingsPos = response.find("\"siblings\"");
    if (siblingsPos != std::string::npos) {
        size_t arrayStart = response.find('[', siblingsPos);
        size_t arrayEnd = std::string::npos;
        
        // Find matching close bracket (handle nested arrays)
        if (arrayStart != std::string::npos) {
            int depth = 1;
            for (size_t i = arrayStart + 1; i < response.size() && depth > 0; ++i) {
                if (response[i] == '[') depth++;
                else if (response[i] == ']') { depth--; if (depth == 0) arrayEnd = i; }
            }
        }
        
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string siblings = response.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            auto fileObjs = SplitJSONObjects(siblings);
            
            for (const auto& fileObj : fileObjs) {
                std::string filename = ExtractJSONString(fileObj, "rfilename");
                if (filename.empty()) continue;
                
                // Only include .gguf files
                std::string lower = filename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.length() > 5 && lower.substr(lower.length() - 5) == ".gguf") {
                    HFModelFileInfo fileInfo;
                    fileInfo.filename = filename;
                    fileInfo.size_bytes = ExtractJSONNumber(fileObj, "size");
                    
                    // Extract quantization from filename
                    // Patterns: Q4_K_M, Q5_K_S, Q2_K, Q8_0, f16, etc.
                    std::string upper = filename;
                    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                    
                    const char* quantPatterns[] = {
                        "Q2_K", "Q3_K_S", "Q3_K_M", "Q3_K_L",
                        "Q4_0", "Q4_1", "Q4_K_S", "Q4_K_M",
                        "Q5_0", "Q5_1", "Q5_K_S", "Q5_K_M",
                        "Q6_K", "Q8_0", "F16", "F32",
                        "IQ2_XXS", "IQ2_XS", "IQ3_XXS", "IQ3_S", "IQ4_NL", "IQ4_XS"
                    };
                    
                    for (const auto& pattern : quantPatterns) {
                        if (upper.find(pattern) != std::string::npos) {
                            fileInfo.quantization = pattern;
                            break;
                        }
                    }
                    
                    info.gguf_files.push_back(fileInfo);
                }
            }
        }
    }
    
    std::cout << "[ModelSourceResolver] HF model " << repo_id << ": " 
              << info.gguf_files.size() << " GGUF files" << std::endl;
    return info;
}

std::vector<HFModelFileInfo> ModelSourceResolver::GetHuggingFaceGGUFFiles(const std::string& repo_id) {
    HFModelInfo info = GetHuggingFaceModelInfo(repo_id);
    return info.gguf_files;
}

std::string ModelSourceResolver::DownloadFromHuggingFace(const std::string& repo_id,
                                                          const std::string& filename,
                                                          DownloadProgressCallback progress) {
    std::string url = "https://huggingface.co/" + repo_id + "/resolve/main/" + filename;
    
    // Create cache subdirectory for this repo
    std::string cache_subdir = m_cacheDir + "/" + SanitizeFilename(repo_id);
    try {
        fs::create_directories(cache_subdir);
    } catch (...) {
        std::cerr << "[ModelSourceResolver] Failed to create cache dir: " << cache_subdir << std::endl;
        return "";
    }
    
    std::string output_path = cache_subdir + "/" + filename;
    
    // Check if already downloaded and valid
    if (fs::exists(output_path) && ValidateGGUFMagic(output_path)) {
        std::cout << "[ModelSourceResolver] Using cached file: " << output_path << std::endl;
        if (progress) {
            ModelDownloadProgress p;
            p.source_url = url;
            p.local_path = output_path;
            p.filename = filename;
            p.progress_percent = 100.0f;
            p.is_completed = true;
            p.total_bytes = fs::file_size(output_path);
            p.downloaded_bytes = p.total_bytes;
            progress(p);
        }
        return output_path;
    }
    
    std::cout << "[ModelSourceResolver] Downloading from HF: " << url << std::endl;
    
    if (WinHTTPDownload(url, output_path, progress, m_hfToken)) {
        if (ValidateGGUFMagic(output_path)) {
            return output_path;
        } else {
            std::cerr << "[ModelSourceResolver] Downloaded file is not valid GGUF" << std::endl;
            fs::remove(output_path);
        }
    }
    
    return "";
}

// ============================================================================
// Ollama Blob Operations
// ============================================================================

std::vector<OllamaBlobInfo> ModelSourceResolver::FindOllamaBlobs() {
    std::vector<OllamaBlobInfo> results;
    
    // Load manifest map: blob filename (sha256-xxx) -> friendly name (e.g. library/llama3.2:latest)
    auto manifestMap = LoadOllamaManifestMap();
    
    auto blobPaths = GetOllamaBlobPaths();
    
    for (const auto& blobDir : blobPaths) {
        if (!fs::exists(blobDir) || !fs::is_directory(blobDir)) {
            continue;
        }
        
        std::cout << "[ModelSourceResolver] Scanning Ollama blobs: " << blobDir << std::endl;
        
        try {
            for (const auto& entry : fs::directory_iterator(blobDir)) {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                
                // Ollama blobs are named sha256-<hash> without .gguf extension
                if (filename.substr(0, 7) != "sha256-") continue;
                
                uint64_t size = entry.file_size();
                
                // Only consider files > 50MB (likely model weights)
                if (size < 50 * 1024 * 1024) continue;
                
                std::string path = entry.path().string();
                
                OllamaBlobInfo blob;
                blob.blob_path = path;
                blob.size_bytes = size;
                blob.is_valid_gguf = ValidateGGUFMagic(path);
                // Map blob digest to friendly model name (e.g. library/llama3.2:latest)
                auto it = manifestMap.find(filename);
                blob.model_name = (it != manifestMap.end()) ? it->second : (filename.substr(0, 24) + "...");
                
                if (blob.is_valid_gguf) {
                    std::cout << "[ModelSourceResolver]   Found GGUF blob: " << blob.model_name 
                              << " (" << (size / (1024*1024)) << " MB)" << std::endl;
                    results.push_back(blob);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[ModelSourceResolver] Error scanning " << blobDir << ": " << e.what() << std::endl;
        }
    }
    
    // Also search standard GGUF file locations
    auto searchPaths = GetOllamaSearchPaths();
    for (const auto& searchPath : searchPaths) {
        if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
            continue;
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                std::string lower = filename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                
                if (lower.length() > 5 && lower.substr(lower.length() - 5) == ".gguf") {
                    std::string path = entry.path().string();
                    
                    OllamaBlobInfo blob;
                    blob.blob_path = path;
                    blob.size_bytes = entry.file_size();
                    blob.model_name = filename;
                    blob.is_valid_gguf = ValidateGGUFMagic(path);
                    
                    if (blob.is_valid_gguf) {
                        results.push_back(blob);
                    }
                }
            }
        } catch (...) {}
    }
    
    return results;
}

OllamaBlobInfo ModelSourceResolver::ResolveOllamaBlob(const std::string& model_name) {
    OllamaBlobInfo result;
    result.model_name = model_name;
    
    std::cout << "[ModelSourceResolver] Resolving Ollama model: " << model_name << std::endl;
    
    // 1. Enhanced resolution — check if it's a SHA or friendly name from manifest
    std::string targetBlobDigest = "";
    
    // Detect SHA patterns (sha256:hex or just hex)
    if (model_name.substr(0, 7) == "sha256-") {
        targetBlobDigest = model_name;
    } else if (model_name.find("sha256:") == 0) {
        targetBlobDigest = "sha256-" + model_name.substr(7);
    } else if ((model_name.length() == 64 || model_name.length() == 12) && 
               std::all_of(model_name.begin(), model_name.end(), [](unsigned char c) { return std::isxdigit(c); })) {
        targetBlobDigest = "sha256-" + model_name;
    }

    // Load manifest map to resolve friendly names (like "bigdaddyg") to blobs
    auto manifestMap = LoadOllamaManifestMap();
    if (targetBlobDigest.empty()) {
        for (const auto& pair : manifestMap) {
            // Check for exact match, match with :latest, or partial name match
            if (pair.second == model_name || 
                pair.second == model_name + ":latest" ||
                pair.second == "library/" + model_name + ":latest" ||
                pair.second.find("/" + model_name + ":") != std::string::npos) {
                targetBlobDigest = pair.first;
                std::cout << "[ModelSourceResolver] Resolved friendly name '" << model_name << "' to blob " << targetBlobDigest << std::endl;
                break;
            }
        }
    }

    // Extract base name (strip :tag) for named GGUF search
    std::string baseName = model_name;
    size_t colonPos = baseName.find(':');
    if (colonPos != std::string::npos) {
        baseName = baseName.substr(0, colonPos);
    }
    
    // 2. Search in standard GGUF file locations first (named .gguf files)
    auto searchPaths = GetOllamaSearchPaths();
    for (const auto& searchPath : searchPaths) {
        if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
            continue;
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                std::string lower = filename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                
                std::string baseNameLower = baseName;
                std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);
                
                if (lower.find(baseNameLower) != std::string::npos && 
                    lower.length() > 5 && lower.substr(lower.length() - 5) == ".gguf") {
                    std::string path = entry.path().string();
                    if (ValidateGGUFMagic(path)) {
                        result.blob_path = path;
                        result.size_bytes = entry.file_size();
                        result.is_valid_gguf = true;
                        std::cout << "[ModelSourceResolver] ✅ Found named GGUF: " << path << std::endl;
                        return result;
                    }
                }
            }
        } catch (...) {}
    }
    
    // 3. Search for the specific blob (if resolved) or collect all for fallback
    auto blobPaths = GetOllamaBlobPaths();
    struct BlobCandidate { std::string path; uint64_t size; };
    std::vector<BlobCandidate> candidates;
    
    for (const auto& blobDir : blobPaths) {
        if (!fs::exists(blobDir) || !fs::is_directory(blobDir)) {
            continue;
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(blobDir)) {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                if (filename.substr(0, 7) != "sha256-") continue;
                
                uint64_t size = entry.file_size();
                std::string path = entry.path().string();
                
                // If we have a target digest, check for prefix match
                if (!targetBlobDigest.empty() && filename.find(targetBlobDigest) == 0) {
                    if (ValidateGGUFMagic(path)) {
                        result.blob_path = path;
                        result.size_bytes = size;
                        result.is_valid_gguf = true;
                        std::cout << "[ModelSourceResolver] ✅ Found targeted blob matching '" << targetBlobDigest << "': " << path << std::endl;
                        return result;
                    }
                }
                
                // Otherwise only collect candidates > 50MB
                if (size > 50 * 1024 * 1024 && ValidateGGUFMagic(path)) {
                    candidates.push_back({path, size});
                }
            }
        } catch (...) {}
    }
    
    // 4. Fallback: take the largest valid blob if no specific match was found
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), 
                  [](const BlobCandidate& a, const BlobCandidate& b) { return a.size > b.size; });
        
        result.blob_path = candidates[0].path;
        result.size_bytes = candidates[0].size;
        result.is_valid_gguf = true;
        std::cout << "[ModelSourceResolver] ⚠️ Model '" << model_name << "' not precisely located; using largest blob: " 
                  << result.blob_path << " (" << (result.size_bytes / (1024*1024)) << " MB)" << std::endl;
    } else {
        std::cerr << "[ModelSourceResolver] ✗ No valid GGUF blobs found in Ollama storage." << std::endl;
    }
    
    return result;
}

// ============================================================================
// HTTP Download Operations
// ============================================================================

std::string ModelSourceResolver::DownloadFromURL(const std::string& url,
                                                  DownloadProgressCallback progress) {
    std::string filename = ExtractFilenameFromURL(url);
    if (filename.empty()) {
        filename = "model_download.gguf";
    }
    
    std::string output_path = m_cacheDir + "/" + filename;
    
    // Check cache
    if (fs::exists(output_path) && ValidateGGUFMagic(output_path)) {
        std::cout << "[ModelSourceResolver] Using cached: " << output_path << std::endl;
        if (progress) {
            ModelDownloadProgress p;
            p.source_url = url;
            p.local_path = output_path;
            p.filename = filename;
            p.progress_percent = 100.0f;
            p.is_completed = true;
            p.total_bytes = fs::file_size(output_path);
            p.downloaded_bytes = p.total_bytes;
            progress(p);
        }
        return output_path;
    }
    
    std::cout << "[ModelSourceResolver] Downloading: " << url << std::endl;
    
    if (WinHTTPDownload(url, output_path, progress)) {
        if (ValidateGGUFMagic(output_path)) {
            return output_path;
        }
        std::cerr << "[ModelSourceResolver] Downloaded file is not valid GGUF" << std::endl;
        fs::remove(output_path);
    }
    
    return "";
}

// ============================================================================
// Configuration
// ============================================================================

void ModelSourceResolver::SetCacheDirectory(const std::string& path) {
    m_cacheDir = path;
    try {
        fs::create_directories(m_cacheDir);
    } catch (...) {}
}

std::string ModelSourceResolver::GetCacheDirectory() const {
    return m_cacheDir;
}

void ModelSourceResolver::SetHuggingFaceToken(const std::string& token) {
    m_hfToken = token;
}

void ModelSourceResolver::CancelDownload() {
    m_cancelRequested = true;
}

bool ModelSourceResolver::IsDownloading() const {
    return m_downloading.load();
}

// ============================================================================
// Validation
// ============================================================================

bool ModelSourceResolver::ValidateGGUFMagic(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    return (file.good() && magic == GGUFConstants::GGUF_MAGIC);
}

std::string ModelSourceResolver::SourceTypeToString(GGUFConstants::ModelSourceType type) {
    switch (type) {
        case GGUFConstants::ModelSourceType::LOCAL_FILE:       return "Local File";
        case GGUFConstants::ModelSourceType::HUGGINGFACE_REPO: return "HuggingFace Hub";
        case GGUFConstants::ModelSourceType::OLLAMA_BLOB:      return "Ollama Blob";
        case GGUFConstants::ModelSourceType::HTTP_URL:         return "HTTP URL";
        default: return "Unknown";
    }
}

// ============================================================================
// WinHTTP Helpers
// ============================================================================

#ifdef _WIN32

// Helper: convert std::string to std::wstring
static std::wstring ToWideString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

// Helper: Parse URL into components
struct URLComponents {
    std::wstring host;
    std::wstring path;
    int port = 443;
    bool isHTTPS = true;
};

static URLComponents ParseURL(const std::string& url) {
    URLComponents comp;
    
    std::wstring wurl = ToWideString(url);
    
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    
    wchar_t hostBuf[256] = {};
    wchar_t pathBuf[2048] = {};
    
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 2048;
    
    if (WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &uc)) {
        comp.host = hostBuf;
        comp.path = pathBuf;
        comp.port = uc.nPort;
        comp.isHTTPS = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    }
    
    return comp;
}

std::string ModelSourceResolver::WinHTTPGet(const std::string& url, const std::string& auth_token) {
    std::string result;
    
    auto urlComp = ParseURL(url);
    if (urlComp.host.empty()) {
        std::cerr << "[ModelSourceResolver] Failed to parse URL: " << url << std::endl;
        return result;
    }
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-AgenticIDE/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;
    
    // Set timeouts
    WinHttpSetTimeouts(hSession, 10000, 10000, 30000, 30000);
    
    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.host.c_str(), 
                                        (INTERNET_PORT)urlComp.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return result;
    }
    
    DWORD flags = urlComp.isHTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.path.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
    
    // Add auth header if provided
    if (!auth_token.empty()) {
        std::wstring authHeader = L"Authorization: Bearer " + ToWideString(auth_token);
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), (DWORD)-1, 
                                WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
    
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
    
    // Check status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, 
                       WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200) {
        std::cerr << "[ModelSourceResolver] HTTP " << statusCode << " from: " << url << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }
    
    // Read response body
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    do {
        bytesAvailable = 0;
        WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
        
        if (bytesAvailable > 0) {
            std::vector<char> buffer(bytesAvailable + 1, 0);
            WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead);
            result.append(buffer.data(), bytesRead);
        }
    } while (bytesAvailable > 0);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return result;
}

bool ModelSourceResolver::WinHTTPDownload(const std::string& url, const std::string& output_path,
                                           DownloadProgressCallback progress,
                                           const std::string& auth_token) {
    std::lock_guard<std::mutex> lock(m_downloadMutex);
    m_downloading = true;
    m_cancelRequested = false;
    
    auto urlComp = ParseURL(url);
    if (urlComp.host.empty()) {
        std::cerr << "[ModelSourceResolver] Failed to parse download URL: " << url << std::endl;
        m_downloading = false;
        return false;
    }
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-AgenticIDE/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_downloading = false;
        return false;
    }
    
    // Longer timeouts for large file downloads
    WinHttpSetTimeouts(hSession, 10000, 10000, 300000, 300000);
    
    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.host.c_str(),
                                        (INTERNET_PORT)urlComp.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    DWORD flags = urlComp.isHTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.path.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    // Add auth header if provided
    if (!auth_token.empty()) {
        std::wstring authHeader = L"Authorization: Bearer " + ToWideString(auth_token);
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), (DWORD)-1,
                                WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        DWORD err = GetLastError();
        std::cerr << "[ModelSourceResolver] WinHTTP send failed, error: " << err << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    // Get content length
    DWORD contentLengthSize = sizeof(DWORD);
    uint64_t totalBytes = 0;
    {
        wchar_t clBuf[64] = {};
        DWORD clBufSize = sizeof(clBuf);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                               WINHTTP_HEADER_NAME_BY_INDEX, clBuf, &clBufSize,
                               WINHTTP_NO_HEADER_INDEX)) {
            totalBytes = _wtoi64(clBuf);
        }
    }
    
    // Check for redirect or error status
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode != 200) {
        std::cerr << "[ModelSourceResolver] Download HTTP " << statusCode << " from: " << url << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    // Open output file
    std::ofstream outFile(output_path, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "[ModelSourceResolver] Failed to open output: " << output_path << std::endl;
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_downloading = false;
        return false;
    }
    
    // Download loop with progress
    uint64_t downloadedBytes = 0;
    const DWORD BUFFER_SIZE = 256 * 1024;  // 256KB chunks
    std::vector<char> buffer(BUFFER_SIZE);
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    bool success = true;
    
    auto lastProgressTime = std::chrono::steady_clock::now();
    
    do {
        if (m_cancelRequested.load()) {
            std::cout << "[ModelSourceResolver] Download cancelled" << std::endl;
            success = false;
            break;
        }
        
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) break;
        if (bytesAvailable == 0) break;
        
        DWORD toRead = std::min(bytesAvailable, BUFFER_SIZE);
        if (!WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead)) {
            success = false;
            break;
        }
        
        if (bytesRead == 0) break;
        
        outFile.write(buffer.data(), bytesRead);
        downloadedBytes += bytesRead;
        
        // Report progress at most every 250ms
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgressTime).count();
        
        if (progress && elapsed > 250) {
            ModelDownloadProgress p;
            p.source_url = url;
            p.local_path = output_path;
            p.total_bytes = totalBytes;
            p.downloaded_bytes = downloadedBytes;
            p.progress_percent = (totalBytes > 0) ? (100.0f * downloadedBytes / totalBytes) : 0.0f;
            p.is_completed = false;
            progress(p);
            lastProgressTime = now;
        }
        
    } while (bytesAvailable > 0);
    
    outFile.close();
    
    // Final progress callback
    if (progress && success) {
        ModelDownloadProgress p;
        p.source_url = url;
        p.local_path = output_path;
        p.total_bytes = downloadedBytes;
        p.downloaded_bytes = downloadedBytes;
        p.progress_percent = 100.0f;
        p.is_completed = true;
        progress(p);
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    m_downloading = false;
    
    if (!success) {
        // Clean up partial download
        try { fs::remove(output_path); } catch (...) {}
    }
    
    return success;
}

#else
// Non-Windows stubs
std::string ModelSourceResolver::WinHTTPGet(const std::string& url, const std::string& auth_token) {
    std::cerr << "[ModelSourceResolver] WinHTTP not available on this platform" << std::endl;
    return "";
}

bool ModelSourceResolver::WinHTTPDownload(const std::string& url, const std::string& output_path,
                                           DownloadProgressCallback progress,
                                           const std::string& auth_token) {
    std::cerr << "[ModelSourceResolver] WinHTTP not available on this platform" << std::endl;
    return false;
}
#endif

// ============================================================================
// Path Helpers
// ============================================================================

std::string ModelSourceResolver::GetDefaultCacheDir() const {
    std::string home = GetUserHomeDir();
    return home + "/.cache/rawrxd/models";
}

std::string ModelSourceResolver::GetUserHomeDir() const {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return path;
    }
    // Fallback to USERPROFILE env var
    const char* userProfile = getenv("USERPROFILE");
    if (userProfile) return userProfile;
    return "C:/Users/Default";
#else
    const char* home = getenv("HOME");
    if (home) return home;
    return "/tmp";
#endif
}

std::string ModelSourceResolver::SanitizeFilename(const std::string& name) const {
    std::string result = name;
    for (char& c : result) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || 
            c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return result;
}

std::string ModelSourceResolver::ExtractFilenameFromURL(const std::string& url) const {
    size_t lastSlash = url.find_last_of('/');
    if (lastSlash == std::string::npos) return "model.gguf";
    
    std::string filename = url.substr(lastSlash + 1);
    
    // Remove query parameters
    size_t queryPos = filename.find('?');
    if (queryPos != std::string::npos) {
        filename = filename.substr(0, queryPos);
    }
    
    if (filename.empty()) return "model.gguf";
    return filename;
}

std::vector<std::string> ModelSourceResolver::GetOllamaSearchPaths() const {
    std::string home = GetUserHomeDir();
    std::string username;
    
#ifdef _WIN32
    const char* user = getenv("USERNAME");
    if (user) username = user;
#else
    const char* user = getenv("USER");
    if (user) username = user;
#endif
    
    return {
        "D:/OllamaModels",
        home + "/.ollama/models",
        "C:/Users/" + username + "/.ollama/models",
        "C:/Ollama/models",
        home + "/.cache/ollama",
        "D:/OllamaModels/blobs",
    };
}

std::vector<std::string> ModelSourceResolver::GetOllamaBlobPaths() const {
    std::string home = GetUserHomeDir();
    
#ifdef _WIN32
    const char* user = getenv("USERNAME");
    std::string username = user ? user : "";
    return {
        home + "/.ollama/models/blobs",
        "D:/OllamaModels/blobs",
        "C:/Users/" + username + "/.ollama/models/blobs",
    };
#else
    return {
        home + "/.ollama/models/blobs",
        home + "/.ollama/blobs/blobs",
    };
#endif
}

std::vector<std::string> ModelSourceResolver::GetOllamaManifestBasePaths() const {
    std::string home = GetUserHomeDir();
#ifdef _WIN32
    const char* user = getenv("USERNAME");
    std::string username = user ? user : "";
    return {
        home + "/.ollama/models",
        "D:/OllamaModels",
        "C:/Users/" + username + "/.ollama/models",
    };
#else
    return {
        home + "/.ollama/models",
        home + "/.ollama",
    };
#endif
}

std::map<std::string, std::string> ModelSourceResolver::LoadOllamaManifestMap() const {
    std::map<std::string, std::string> blobToName;
    auto basePaths = GetOllamaManifestBasePaths();
    const std::string manifestsSubdir = "manifests/registry.ollama.ai";
    
    for (const auto& base : basePaths) {
        fs::path manifestsDir = fs::path(base) / manifestsSubdir;
        if (!fs::exists(manifestsDir) || !fs::is_directory(manifestsDir)) continue;
        
        try {
            for (const auto& entry : fs::recursive_directory_iterator(manifestsDir)) {
                if (!entry.is_regular_file()) continue;
                
                std::string manifestPath = entry.path().string();
                std::string relPath = fs::relative(entry.path(), manifestsDir).generic_string();
                std::replace(relPath.begin(), relPath.end(), '\\', '/');
                
                // Path format: namespace/model/tag or namespace/model/tag/file
                std::vector<std::string> parts;
                std::istringstream ss(relPath);
                std::string part;
                while (std::getline(ss, part, '/')) parts.push_back(part);
                
                if (parts.size() < 2) continue;
                
                std::string displayName = parts[0] + "/" + parts[1];
                if (parts.size() >= 3) displayName += ":" + parts[2];
                else displayName += ":latest";
                
                std::ifstream f(manifestPath);
                if (!f) continue;
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                f.close();
                
                // Extract all "digest":"sha256:hex" values
                size_t pos = 0;
                const std::string needle = "\"digest\":\"sha256:";
                while ((pos = content.find(needle, pos)) != std::string::npos) {
                    size_t start = pos + needle.length();
                    size_t end = content.find('"', start);
                    if (end != std::string::npos && end > start) {
                        std::string hex = content.substr(start, end - start);
                        std::string blobName = "sha256-" + hex;
                        blobToName[blobName] = displayName;
                    }
                    pos = start;
                }
            }
        } catch (const std::exception&) {}
    }
    return blobToName;
}

// ============================================================================
// JSON Parsing Helpers (minimal, no external dependency)
// ============================================================================

std::string ModelSourceResolver::ExtractJSONString(const std::string& json, const std::string& key) const {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) return "";
    
    // Find the colon after the key
    size_t colonPos = json.find(':', keyPos + pattern.length());
    if (colonPos == std::string::npos) return "";
    
    // Find opening quote of value
    size_t openQuote = json.find('"', colonPos + 1);
    if (openQuote == std::string::npos) return "";
    
    // Find closing quote (handling escaped quotes)
    size_t closeQuote = openQuote + 1;
    while (closeQuote < json.size()) {
        if (json[closeQuote] == '"' && json[closeQuote - 1] != '\\') break;
        closeQuote++;
    }
    
    if (closeQuote >= json.size()) return "";
    return json.substr(openQuote + 1, closeQuote - openQuote - 1);
}

uint64_t ModelSourceResolver::ExtractJSONNumber(const std::string& json, const std::string& key) const {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(':', keyPos + pattern.length());
    if (colonPos == std::string::npos) return 0;
    
    // Skip whitespace after colon
    size_t numStart = colonPos + 1;
    while (numStart < json.size() && (json[numStart] == ' ' || json[numStart] == '\t' || json[numStart] == '\n')) {
        numStart++;
    }
    
    // Read digits
    std::string numStr;
    while (numStart < json.size() && (isdigit(json[numStart]) || json[numStart] == '.')) {
        numStr += json[numStart++];
    }
    
    if (numStr.empty()) return 0;
    
    try {
        return std::stoull(numStr);
    } catch (...) {
        return 0;
    }
}

std::vector<std::string> ModelSourceResolver::SplitJSONObjects(const std::string& json) const {
    std::vector<std::string> objects;
    int depth = 0;
    std::string current;
    
    for (char c : json) {
        if (c == '{') {
            depth++;
            current += c;
        } else if (c == '}') {
            current += c;
            depth--;
            if (depth == 0) {
                objects.push_back(current);
                current.clear();
            }
        } else if (depth > 0) {
            current += c;
        }
    }
    
    return objects;
}

} // namespace RawrXD
