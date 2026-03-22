#pragma once

#include "model_anatomy.hpp"

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace RawrXD
{

enum class DiffKind : int
{
    None = 0,
    OnlyInA = 1,
    OnlyInB = 2,
    SizeMismatch = 3,
    TypeMismatch = 4,
    ShapeMismatch = 5
};

struct DiffEntry
{
    DiffKind kind{DiffKind::None};
    std::string tensorName;
    uint64_t valueA{0};
    uint64_t valueB{0};
    int layerId{-1};
};

std::vector<DiffEntry> DiffAnatomies(const ModelAnatomy& a, const ModelAnatomy& b, std::ostream* streamOut);

std::string ExportDiffToJson(const std::vector<DiffEntry>& diff, const std::string& nameA, const std::string& nameB,
                             bool pretty);

}  // namespace RawrXD
