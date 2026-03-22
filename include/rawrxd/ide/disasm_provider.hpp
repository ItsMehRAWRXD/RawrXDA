#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rawrxd::ide
{

/// E11 — Disassembly provider abstraction (Win32 / tooling). One backend implements this;
/// UI and Monaco lanes depend only on this surface.

struct DisasmSymbol
{
    std::string name;
    std::string kind;  // function | object | unknown
    std::uint64_t address = 0;
};

struct DisasmXref
{
    std::uint64_t fromAddr = 0;
    std::uint64_t toAddr = 0;
    std::string kind;  // call | data | jump
};

class IDisasmProvider
{
  public:
    virtual ~IDisasmProvider() = default;

    virtual std::string backendId() const = 0;

    /// Load an executable or object; returns false if format unsupported.
    virtual bool loadArtifact(const std::string& pathOrUri, std::string& errorOut) = 0;

    virtual std::vector<DisasmSymbol> listSymbols(std::uint32_t maxCount = 4096) = 0;
    virtual std::vector<DisasmXref> xrefsForAddress(std::uint64_t addr, std::uint32_t maxCount = 256) = 0;
};

}  // namespace rawrxd::ide
