#pragma once

#include <string>

namespace RawrXD::Core::VectorIndexPersistence {

// Returns the canonical semantic index cache file path under the workspace root.
std::string ResolveSemanticIndexCachePath(const std::string& workspaceRoot);

// Attempts to load semantic index cache from disk. detailOut receives status context.
bool TryLoadSemanticIndexCache(const std::string& workspaceRoot, std::string* detailOut = nullptr);

// Attempts to save semantic index cache to disk. detailOut receives status context.
bool TrySaveSemanticIndexCache(const std::string& workspaceRoot, std::string* detailOut = nullptr);

} // namespace RawrXD::Core::VectorIndexPersistence
