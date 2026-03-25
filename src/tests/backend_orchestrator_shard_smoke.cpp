#include "BackendOrchestrator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <cstdio>
#include <vector>

namespace {

std::string makeShardName(const std::string& base, int index, int total)
{
    char buf[256] = {0};
    std::snprintf(buf, sizeof(buf), "%s-%05d-of-%05d.gguf", base.c_str(), index, total);
    return std::string(buf);
}

bool writeDummyFile(const std::filesystem::path& p, size_t bytes)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
        return false;
    for (size_t i = 0; i < bytes; ++i)
        out.put(static_cast<char>(i & 0xFF));
    return out.good();
}

}  // namespace

int main()
{
    namespace fs = std::filesystem;

    const fs::path tempRoot = fs::temp_directory_path() / "rawrxd_shard_smoke";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);
    if (ec)
    {
        std::cerr << "Failed to create temp directory: " << tempRoot.string() << "\n";
        return 1;
    }

    const std::string base = "smoke-model";
    const int totalShards = 3;

    // Stable positive path: a single-file model always bypasses shard-name expansion.
    const fs::path singleModel = tempRoot / "single-model.gguf";
    if (!writeDummyFile(singleModel, 9))
    {
        std::cerr << "Failed to create single model file: " << singleModel.string() << "\n";
        return 2;
    }

    auto& orch = RawrXD::BackendOrchestrator::Instance();
    orch.ClearShards();

    const std::vector<int> devices = {0, 1, 2};

    if (!orch.ShardModel(singleModel.string(), devices))
    {
        std::cerr << "ShardModel unexpectedly failed on single-file model\n";
        return 3;
    }

    const auto shards = orch.GetShards();
    if (shards.size() != devices.size())
    {
        std::cerr << "Unexpected shard count: " << shards.size() << "\n";
        return 4;
    }

    int totalLayers = 0;
    size_t totalBudget = 0;
    for (const auto& s : shards)
    {
        if (s.layer_start >= 0 && s.layer_end >= s.layer_start)
            totalLayers += (s.layer_end - s.layer_start + 1);
        totalBudget += s.vram_bytes;
    }

    // Metadata parse fails for synthetic shards, so fallback path should be 32 layers.
    if (totalLayers != 32)
    {
        std::cerr << "Expected fallback 32 layers, got " << totalLayers << "\n";
        return 5;
    }

    // Single-file dummy model size is 9 bytes.
    if (totalBudget != 9)
    {
        std::cerr << "Expected total VRAM budget 9 bytes, got " << totalBudget << "\n";
        return 6;
    }

    // Missing-shard safety path: only shard 1 exists for a 3-shard naming contract.
    const fs::path firstShard = tempRoot / makeShardName(base, 1, totalShards);
    if (!writeDummyFile(firstShard, 1))
    {
        std::cerr << "Failed to create first shard file for negative test\n";
        return 8;
    }

    orch.ClearShards();
    if (orch.ShardModel(firstShard.string(), devices))
    {
        std::cerr << "ShardModel should fail when a shard is missing\n";
        return 7;
    }

    fs::remove_all(tempRoot, ec);
    std::cout << "BackendOrchestrator shard smoke test passed\n";
    return 0;
}
