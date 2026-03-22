// Win32IDE_SwarmModelSelector.h — Swarm / local model path discovery (non-UI)
#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Win32App {

/// Scans `RAWRXD_SWARM_MODEL_DIR` (UTF-8 path), or else `%USERPROFILE%\.ollama\models`,
/// for `*.gguf` files (non-recursive). Returns full UTF-8 paths, capped at `maxEntries`.
std::vector<std::string> enumerateSwarmModelCandidates(unsigned maxEntries = 512);

} // namespace Win32App
} // namespace RawrXD
