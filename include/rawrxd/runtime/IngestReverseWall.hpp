#pragma once

#include "MmapRegion.hpp"
#include "RuntimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace RawrXD::Runtime
{

/// **cache + v = ingest reverse wall**: no view is published until digest + size checks pass.
class IngestReverseWall
{
  public:
    static IngestReverseWall& instance();

    /// Expected hex64 (16 hex chars) or empty to skip digest check (dev only — logs warning).
    [[nodiscard]] std::expected<MmapRegion, std::string> ingestFile(const std::wstring& path,
                                                                    const std::string& expectedHex64);

    [[nodiscard]] static std::string fnv1a64HexOfFile(const std::wstring& path);

  private:
    IngestReverseWall() = default;
};

}  // namespace RawrXD::Runtime
