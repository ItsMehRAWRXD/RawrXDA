/**
 * @file model_name_utils.cpp
 * @brief Implementation of automatic model name normalizer and resolver.
 */

#include "model_name_utils.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {

std::string ModelNameUtils::deriveFromPath(const std::string& path) {
    if (path.empty()) return "";
    try {
        fs::path p(path);
        std::string stem = p.stem().string();
        if (stem.empty()) return "";
        return stem;
    } catch (...) {
        // Fallback: strip extension manually
        size_t slash = path.find_last_of("/\\");
        std::string base = (slash != std::string::npos) ? path.substr(slash + 1) : path;
        size_t dot = base.rfind('.');
        if (dot != std::string::npos && (base.size() - dot) <= 6)
            base = base.substr(0, dot);
        return base;
    }
}

bool ModelNameUtils::charAllowed(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) ||
           c == '-' || c == '_' || c == ':' || c == '.';
}

std::string ModelNameUtils::toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r;
}

std::string ModelNameUtils::normalize(const std::string& name) {
    // Trim
    size_t start = 0, end = name.size();
    while (start < end && (name[start] == ' ' || name[start] == '\t')) ++start;
    while (end > start && (name[end - 1] == ' ' || name[end - 1] == '\t')) --end;
    if (start >= end) return "";

    std::string trimmed = name.substr(start, end - start);
    for (char c : trimmed) {
        if (!charAllowed(c)) return "";  // Reject if any disallowed char
    }
    return trimmed;
}

bool ModelNameUtils::isValid(const std::string& name) {
    return !normalize(name).empty();
}

std::string ModelNameUtils::resolveToApiName(const std::string& input,
                                             const std::vector<std::string>& knownModels) {
    std::string n = normalize(input);
    if (n.empty()) return input;  // Pass through original if normalization fails

    // Exact match
    for (const auto& m : knownModels) {
        if (m == n) return m;
    }

    // Case-insensitive match (Ollama may return lowercase in /api/tags)
    std::string nLower = toLower(n);
    for (const auto& m : knownModels) {
        if (toLower(m) == nLower) return m;
    }

    return n;
}

std::vector<std::string> ModelNameUtils::getBigDaddyGVariants() {
    return {
        "BigDaddyG-Q4_K_M",
        "BigDaddyG-F32-FROM-Q4",
        "BigDaddyG-F32",
        "BigDaddyG-Q4",
        "BigDaddyG",
        "bigdaddyg-q4_k_m",
        "bigdaddyg-f32-from-q4",
        "bigdaddyg-f32",
        "bigdaddyg-q4",
        "bigdaddyg"
    };
}

void ModelNameUtils::addDerivedNamesFromDirs(const std::vector<std::string>& dirPaths,
                                             std::vector<std::string>& outNames) {
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    for (const auto& dir : dirPaths) {
        std::string pattern = dir;
        if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/')
            pattern += "\\";
        pattern += "*";

        HANDLE h = FindFirstFileA(pattern.c_str(), &findData);
        if (h == INVALID_HANDLE_VALUE) continue;

        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                std::string name(findData.cFileName);
                if (name == "blobs" || name == "manifests" || name == "registry") continue;
                if (isValid(name)) {
                    auto it = std::find(outNames.begin(), outNames.end(), name);
                    if (it == outNames.end())
                        outNames.push_back(name);
                }
            }
        } while (FindNextFileA(h, &findData));
        FindClose(h);
    }
#else
    (void)dirPaths;
    (void)outNames;
#endif
}

} // namespace RawrXD
