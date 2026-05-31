#include "core/vector_index_persistence.h"

#include "context/semantic_index.h"

#include <filesystem>

namespace RawrXD::Core::VectorIndexPersistence {
namespace fs = std::filesystem;

namespace {

fs::path NormalizeWorkspaceRoot(const std::string& workspaceRoot)
{
    if (workspaceRoot.empty())
    {
        return fs::current_path();
    }
    return fs::path(workspaceRoot);
}

} // namespace

std::string ResolveSemanticIndexCachePath(const std::string& workspaceRoot)
{
    const fs::path root = NormalizeWorkspaceRoot(workspaceRoot);
    const fs::path cacheFile = root / ".rawrxd" / "cache" / "index" / "semantic_index.v1.json";
    return cacheFile.string();
}

bool TryLoadSemanticIndexCache(const std::string& workspaceRoot, std::string* detailOut)
{
    const fs::path cachePath = ResolveSemanticIndexCachePath(workspaceRoot);

    if (!fs::exists(cachePath))
    {
        if (detailOut)
        {
            *detailOut = "semantic cache not found: " + cachePath.string();
        }
        return false;
    }

    const bool loaded = RawrXD::SemanticIndex::SemanticIndexEngine::Instance().LoadIndex(cachePath.string());
    if (detailOut)
    {
        *detailOut = loaded ? ("semantic cache loaded: " + cachePath.string())
                            : ("semantic cache load failed: " + cachePath.string());
    }
    return loaded;
}

bool TrySaveSemanticIndexCache(const std::string& workspaceRoot, std::string* detailOut)
{
    const fs::path cachePath = ResolveSemanticIndexCachePath(workspaceRoot);

    std::error_code ec;
    fs::create_directories(cachePath.parent_path(), ec);
    if (ec)
    {
        if (detailOut)
        {
            *detailOut = "failed to create cache directory: " + ec.message();
        }
        return false;
    }

    const bool saved = RawrXD::SemanticIndex::SemanticIndexEngine::Instance().SaveIndex(cachePath.string());
    if (detailOut)
    {
        *detailOut = saved ? ("semantic cache saved: " + cachePath.string())
                           : ("semantic cache save failed: " + cachePath.string());
    }
    return saved;
}

} // namespace RawrXD::Core::VectorIndexPersistence
